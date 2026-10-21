#include "notes/webnoteeditor.h"
#include "notes/notestore.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineUrlRequestJob>
#include <QWebChannel>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QBuffer>
#include <QDesktopServices>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QPointer>

namespace {
// 自定义 scheme 必须在 QApplication 之前注册，测试程序也使用相同入口。
const bool registered = [] {
    QWebEngineUrlScheme scheme("talknote");
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(443);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                    QWebEngineUrlScheme::LocalAccessAllowed | QWebEngineUrlScheme::CorsEnabled);
    QWebEngineUrlScheme::registerScheme(scheme);
    return true;
}();
QString json(const QVariantMap &value) {
    // 参数经 JSON 编码再传给脚本，避免文档中的引号、换行破坏 JavaScript 语法。
    return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(value)).toJson(QJsonDocument::Compact));
}
class OfflineRequests : public QWebEngineUrlRequestInterceptor {
public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;
    void interceptRequest(QWebEngineUrlRequestInfo &info) override {
        // 离线资源允许列表；禁用普通网络与 file 请求，图片只能走受控本地入口。
        const auto scheme = info.requestUrl().scheme();
        info.block(scheme != "qrc" && scheme != "talknote" && scheme != "data" && scheme != "blob");
    }
};
class Images : public QWebEngineUrlSchemeHandler {
public:
    Images(NoteStore *store, QObject *parent) : QWebEngineUrlSchemeHandler(parent), store(store) {}
    mutable QString directory;
    NoteStore *store;
    void requestStarted(QWebEngineUrlRequestJob *job) override {
        if (job->requestMethod() != "GET" || job->requestUrl().host() != "images") {
            job->fail(QWebEngineUrlRequestJob::RequestDenied); return;
        }
        const auto relative = job->requestUrl().path().mid(1);
        const auto file = QDir::cleanPath(QDir(directory).absoluteFilePath(relative));
        if (!store->contains(file)) { job->fail(QWebEngineUrlRequestJob::RequestDenied); return; }
        // 解码后重新编码为图片，不把任意 HTML、脚本或原始文件字节直接交给页面。
        QImage image(file);
        if (image.isNull()) { job->fail(QWebEngineUrlRequestJob::UrlNotFound); return; }
        auto *buffer = new QBuffer(job); buffer->open(QIODevice::ReadWrite);
        image.save(buffer, "PNG"); buffer->seek(0); job->reply("image/png", buffer);
    }
};
class NotePage : public QWebEnginePage {
public:
    using QWebEnginePage::QWebEnginePage;
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool mainFrame) override {
        if (type == NavigationTypeLinkClicked) {
            if (url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "mailto") QDesktopServices::openUrl(url);
            return false;
        }
        return !mainFrame || url.scheme() == "qrc";
    }
    void javaScriptAlert(const QUrl &, const QString &) override {}
    bool javaScriptConfirm(const QUrl &, const QString &) override { return false; }
    bool javaScriptPrompt(const QUrl &, const QString &, const QString &, QString *) override { return false; }
};
}

// 只注册窄桥接对象，不把 QWidget 的属性与生命周期接口暴露给网页。
class NoteBridge : public QObject {
    Q_OBJECT
public:
    NoteBridge(WebNoteEditor *editor, QObject *parent) : QObject(parent), editor(editor) {}
    Q_INVOKABLE QVariantMap configuration() const { return editor->configuration(); }
    Q_INVOKABLE void loaded(int generation) { editor->loaded(generation); }
    Q_INVOKABLE void changed(int generation, int revision, const QString &text) { editor->changed(generation, revision, text); }
    Q_INVOKABLE void saveRequested() { editor->saveRequested(); }
    Q_INVOKABLE void chooseImage() { editor->chooseImage(); }
    Q_INVOKABLE void clipboardWrite(const QString &text) { editor->clipboardWrite(text); }
    Q_INVOKABLE QString clipboardRead() const { return editor->clipboardRead(); }
    Q_INVOKABLE void openLink(const QString &url) { editor->openLink(url); }
private:
    WebNoteEditor *editor;
};

WebNoteEditor::WebNoteEditor(NoteStore *store, const QString &path, const QString &text, QWidget *parent)
    : QWidget(parent), m_store(store), m_path(path), m_text(text) {
    setObjectName("markdownLiveEditor");
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0);
    m_view = new QWebEngineView(this); layout->addWidget(m_view);
    // 等待正确代次的加载确认后才接受输入，防止初始化覆盖用户刚输入的内容。
    m_view->setEnabled(false); // The document must be loaded and acknowledged before accepting input.
    auto *profile = new QWebEngineProfile(this); // Off the record, no browser cache or persistent drafts.
    profile->setUrlRequestInterceptor(new OfflineRequests(profile));
    auto *images = new Images(store, profile); images->directory = QFileInfo(path).absolutePath();
    profile->installUrlSchemeHandler("talknote", images);
    auto *page = new NotePage(profile, m_view); m_view->setPage(page);
    page->setBackgroundColor(QColor("#fcfbf9"));
    // Chromium treats the custom image scheme as a non-file origin. The request
    // interceptor is the authoritative allowlist and blocks every HTTP(S) request.
    page->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    // Required for the local image scheme; the interceptor still rejects all file: requests.
    page->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    page->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    page->settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    auto *channel = new QWebChannel(page); channel->registerObject("host", new NoteBridge(this, channel)); page->setWebChannel(channel);
    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (!ok) { m_failed = true; emit error("笔记编辑器加载失败，请保留当前内容并切换源码模式。"); }
    });
    connect(page, &QWebEnginePage::renderProcessTerminated, this, [this] {
        m_ready = false; m_failed = true; emit error("笔记编辑器进程已停止。最后同步的内容仍保留，保存与切换已暂停。");
    });
    m_view->setContextMenuPolicy(Qt::NoContextMenu);
    m_view->load(QUrl("qrc:/editor/index.html"));
}
QVariantMap WebNoteEditor::configuration() const {
    return {{"generation", m_generation}, {"revision", m_revision}, {"text", m_text}, {"readOnly", m_readOnly}};
}
void WebNoteEditor::load(bool resetUndo) {
    auto config = configuration(); config.insert("resetUndo", resetUndo);
    m_view->page()->runJavaScript("window.talkMusic && talkMusic.load(" + json(config) + ")");
}
void WebNoteEditor::loaded(int generation) {
    if (generation != m_generation) { load(true); return; }
    m_ready = true; m_failed = false; m_view->setEnabled(true); emit becameReady();
}
void WebNoteEditor::changed(int generation, int revision, const QString &text) {
    // 同一文档重载后旧消息可能晚到；已处理版本和只读页面的修改都不接受。
    if (generation != m_generation || revision <= m_revision || m_readOnly) return;
    m_revision = revision;
    if (m_text != text) { m_text = text; emit textChanged(text); }
}
void WebNoteEditor::setDocument(const QString &path, const QString &text, bool resetUndo) {
    // 未变化时保留 DOM 和撤销状态；实际重载才推进代次并重置内容版本。
    if (m_path == path && m_text == text && !resetUndo) return;
    m_path = path; m_text = text; ++m_generation; m_revision = 0;
    auto *images = static_cast<const Images *>(m_view->page()->profile()->urlSchemeHandler("talknote"));
    images->directory = QFileInfo(path).absolutePath();
    if (m_ready) load(resetUndo);
}
void WebNoteEditor::setReadOnly(bool readOnly) {
    if (readOnly == m_readOnly) return;
    // 切到源码等场景会收紧桥接为只读，必须先接收网页最后一次输入。
    if (readOnly && m_ready && !synchronize()) return;
    m_readOnly = readOnly;
    if (m_ready) m_view->page()->runJavaScript(QString("talkMusic.readOnly(%1)").arg(readOnly ? "true" : "false"));
}
bool WebNoteEditor::synchronize() {
    // 有界快照屏障：等待期间排除新用户输入，但其他事件仍可能运行，需防重入。
    if (m_failed || m_syncing) return false;
    if (!m_ready) return true; // No editable DOM exists yet; C++ still holds the original text.
    m_syncing = true;
    QEventLoop loop; QTimer timer; timer.setSingleShot(true);
    bool received = false, composing = false;
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    // 超时返回后局部变量将销毁；迟到回调先检查等待对象，再访问引用捕获。
    QPointer<QEventLoop> guard(&loop);
    m_view->page()->runJavaScript("talkMusic.snapshot()", [this, guard, &received, &composing](const QVariant &value) {
        if (!guard) return; // A timed-out renderer can respond later.
        const auto map = value.toMap();
        received = map.value("generation").toInt() == m_generation;
        composing = map.value("composing").toBool();
        if (received && !composing && map.value("modified").toBool())
            changed(m_generation, map.value("revision").toInt(), map.value("text").toString());
        guard->quit();
    });
    timer.start(2000); loop.exec(QEventLoop::ExcludeUserInputEvents); m_syncing = false;
    if (!received || composing) emit error(composing ? "请先完成中文输入，再保存或切换笔记。" : "编辑器内容尚未同步，请稍后再试。");
    return received && !composing;
}
void WebNoteEditor::insertMarkdown(const QString &text) {
    if (!m_ready || m_readOnly) { emit error("编辑器正在加载，请稍后插入内容。"); return; }
    m_view->page()->runJavaScript("talkMusic.insert(" + json({{"text", text}}) + ")");
}
void WebNoteEditor::jumpToHeading(int ordinal) {
    if (m_ready) m_view->page()->runJavaScript(QString("talkMusic.heading(%1)").arg(ordinal));
}
void WebNoteEditor::saveRequested() { emit saveRequest(); }
void WebNoteEditor::chooseImage() {
    // 只插入相对引用，不复制文件；访问范围由 NoteStore 与图片 scheme 再次校验。
    if (!synchronize()) return;
    const auto file = QFileDialog::getOpenFileName(this, "插入本地图片引用", QFileInfo(m_path).absolutePath(), "图片 (*.png *.jpg *.jpeg *.gif *.bmp *.webp *.svg)");
    if (file.isEmpty()) return;
    if (!m_store->contains(file)) { emit error("请选择笔记文件夹内的图片。图片仅引用，不复制或上传。"); return; }
    auto relative = QDir(QFileInfo(m_path).absolutePath()).relativeFilePath(file);
    insertMarkdown("![" + QFileInfo(file).completeBaseName().replace("]", "\\]") + "](<" + relative.replace(">", "%3E") + ">)");
}
void WebNoteEditor::clipboardWrite(const QString &text) { QApplication::clipboard()->setText(text); }
QString WebNoteEditor::clipboardRead() const { return QApplication::clipboard()->text(); }
void WebNoteEditor::openLink(const QString &text) {
    // 外部链接交给系统程序，仅接受明确支持的协议，不执行任意自定义 scheme。
    const QUrl url(text.trimmed());
    if (url.scheme() == "https" || url.scheme() == "http" || url.scheme() == "mailto") QDesktopServices::openUrl(url);
}

#include "webnoteeditor.moc"
