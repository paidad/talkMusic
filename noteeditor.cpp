#include "noteeditor.h"
#include "uihelpers.h"
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextCursor>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QSplitter>
#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QSettings>
#include <QShortcut>
#include <QDesktopServices>
#include <QTimer>
#include <QImage>
#include <QResizeEvent>
#include <QScrollBar>

namespace {
class MarkdownHighlighter : public QSyntaxHighlighter {
public:
    explicit MarkdownHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}
protected:
    void highlightBlock(const QString &text) override {
        const auto fence = QRegularExpression(R"(^ {0,3}(`{3,}|~{3,}))").match(text);
        const int previous = previousBlockState() > 0 ? previousBlockState() : 0;
        if (fence.hasMatch()) {
            const int state = fence.captured(1).front() == '`' ? 1 : 2;
            setCurrentBlockState(previous == state ? 0 : state); setFormat(0, text.size(), QColor("#a97751")); return;
        }
        setCurrentBlockState(previous);
        if (previous) { setFormat(0, text.size(), QColor("#a97751")); return; }
        if (QRegularExpression(R"(^ {0,3}#{1,6}(?:\s|$))").match(text).hasMatch()) {
            QTextCharFormat format; format.setForeground(QColor("#bf4658")); format.setFontWeight(QFont::DemiBold); setFormat(0, text.size(), format);
        }
        for (const auto &pattern : {QString(R"(\*\*.+?\*\*)"), QString(R"(`[^`]+`)"), QString(R"(\[[^\]]+\]\([^\)]+\))")}) {
            auto matches = QRegularExpression(pattern).globalMatch(text);
            while (matches.hasNext()) { const auto match = matches.next(); setFormat(match.capturedStart(), match.capturedLength(), QColor("#9b637a")); }
        }
    }
};
class LocalMarkdownBrowser : public QTextBrowser {
public:
    explicit LocalMarkdownBrowser(QWidget *parent = nullptr) : QTextBrowser(parent) {
        setOpenLinks(false); setOpenExternalLinks(false);
        connect(this, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
            if (url.scheme().isEmpty() && url.path().isEmpty() && !url.fragment().isEmpty()) scrollToAnchor(url.fragment());
            else if (url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "mailto") QDesktopServices::openUrl(url);
        });
    }
protected:
    QVariant loadResource(int type, const QUrl &name) override {
        const auto resolved = document()->baseUrl().resolved(name);
        if (type == QTextDocument::ImageResource && resolved.isLocalFile()) return QImage(resolved.toLocalFile());
        return {}; // No automatic remote images or remote resources.
    }
};
bool underneath(const QString &path, const QString &parent) { return path == parent || path.startsWith(parent + "/"); }
}

struct NoteEditor::Page {
    QWidget *widget;
    QSplitter *splitter;
    QPlainTextEdit *editor;
    LocalMarkdownBrowser *preview;
    NoteDocument *document;
    QVector<MarkdownHeading> outline;
    QTimer *renderTimer;
};

NoteEditor::NoteEditor(NoteStore *store, QWidget *parent) : QWidget(parent), m_store(store) {
    setObjectName("noteWorkspace"); setMinimumWidth(350);
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(24, 24, 24, 18); layout->setSpacing(12);
    auto *heading = new QHBoxLayout;
    m_title = new ElidedLabel("你的写作空间"); m_title->setProperty("role", "heading"); m_title->setTextFormat(Qt::PlainText);
    m_status = new QLabel; m_status->setObjectName("noteSaveStatus"); m_status->setProperty("role", "small"); m_status->setMaximumWidth(150); m_status->setWordWrap(true);
    heading->addWidget(m_title, 1); heading->addWidget(m_status); layout->addLayout(heading);
    auto *toolbar = new QHBoxLayout; toolbar->setSpacing(6);
    const QStringList names{"源码", "预览", "分屏"};
    for (int i = 0; i < 3; ++i) {
        m_modes[i] = new QPushButton(names[i]); m_modes[i]->setObjectName("noteMode" + QString::number(i)); m_modes[i]->setCheckable(true); toolbar->addWidget(m_modes[i]);
        connect(m_modes[i], &QPushButton::clicked, this, [this, i] { setMode(i); });
    }
    toolbar->addStretch(); auto *save = new QPushButton("保存"); save->setObjectName("saveNote"); toolbar->addWidget(save);
    connect(save, &QPushButton::clicked, this, [this] { flushAll(); });
    auto *shortcut = new QShortcut(QKeySequence::Save, this); connect(shortcut, &QShortcut::activated, this, [this] { flushAll(); });
    layout->addLayout(toolbar);
    m_conflict = new QWidget; m_conflict->setObjectName("noteConflictBar"); auto *choices = new QHBoxLayout(m_conflict); choices->setContentsMargins(0, 0, 0, 0); choices->setSpacing(6);
    auto *reload = new QPushButton("加载磁盘版本"); reload->setObjectName("reloadNote");
    auto *overwrite = new QPushButton("覆盖磁盘版本"); overwrite->setObjectName("overwriteNote");
    auto *saveAs = new QPushButton("另存为"); saveAs->setObjectName("saveNoteAs"); choices->addWidget(reload); choices->addWidget(overwrite); choices->addWidget(saveAs);
    connect(reload, &QPushButton::clicked, this, [this] { if (m_active) m_active->document->reload(); });
    connect(overwrite, &QPushButton::clicked, this, [this] { if (m_active) m_active->document->overwrite(); });
    connect(saveAs, &QPushButton::clicked, this, &NoteEditor::resolveSaveAs); layout->addWidget(m_conflict);
    m_stack = new QStackedWidget; layout->addWidget(m_stack, 1);
    auto *welcome = new QLabel("让音乐陪你记下灵感\n\n在右侧选择笔记文件夹，\n打开一篇文档，或新建你的第一篇笔记。");
    welcome->setObjectName("noteWelcome"); welcome->setAlignment(Qt::AlignCenter); welcome->setWordWrap(true); welcome->setProperty("role", "muted"); m_stack->addWidget(welcome);
    auto *hint = new QLabel("Ctrl+S 保存 · 输入停止后自动保存 · 笔记与待办独立管理"); hint->setWordWrap(true); hint->setProperty("role", "small"); layout->addWidget(hint);
    m_mode = qBound(0, QSettings().value("notes/mode", 0).toInt(), 2);
    connect(store, &NoteStore::renamed, this, [this](const QString &oldPath, const QString &newPath) {
        const auto keys = m_pages.keys();
        for (const auto &key : keys) if (underneath(key, oldPath)) {
            auto *page = m_pages.take(key); const auto destination = newPath + key.mid(oldPath.size());
            page->document->relocate(destination); m_pages.insert(destination, page); render(page);
        }
        refresh(); emit activeChanged(); emit outlineChanged();
    });
    connect(store, &NoteStore::removed, this, [this](const QString &path) {
        for (const auto &key : m_pages.keys()) if (underneath(key, path)) {
            auto *page = m_pages.take(key); if (page == m_active) m_active = nullptr;
            m_stack->removeWidget(page->widget); delete page->widget; delete page;
        }
        if (!m_active) m_stack->setCurrentIndex(0);
        refresh(); emit activeChanged(); emit outlineChanged();
    });
    refresh();
}
QString NoteEditor::activePath() const { return m_active ? m_active->document->path() : QString(); }
NoteEditor::~NoteEditor() { for (auto *page : m_pages) delete page; }
QPlainTextEdit *NoteEditor::activeEditor() const { return m_active ? m_active->editor : nullptr; }
bool NoteEditor::openFile(const QString &path) {
    const auto normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (normalized == activePath()) return true;
    if (!flushAll()) return false;
    if (!m_store->contains(normalized) || !QFileInfo(normalized).isFile() || !normalized.endsWith(".md", Qt::CaseInsensitive)) { emit error("请选择笔记目录中的 Markdown 文件。"); return false; }
    auto *page = m_pages.value(normalized);
    if (!page) {
        auto *widget = new QWidget;
        auto *document = new NoteDocument(m_store, widget);
        if (!document->open(normalized)) { emit error(document->status()); delete widget; return false; }
        page = new Page; page->widget = widget; page->document = document;
        auto *layout = new QVBoxLayout(widget); layout->setContentsMargins(0, 0, 0, 0);
        page->splitter = new QSplitter; layout->addWidget(page->splitter);
        page->editor = new QPlainTextEdit; page->editor->setObjectName("markdownSource"); page->editor->setPlainText(document->text());
        page->editor->setStyleSheet("QPlainTextEdit {font-family: Consolas, 'Microsoft YaHei UI'; font-size: 14px; background: #fcfbf9; border: none; padding: 10px; selection-background-color: #e8aab4;}");
        page->editor->setTabStopDistance(32); new MarkdownHighlighter(page->editor->document());
        page->preview = new LocalMarkdownBrowser; page->preview->setObjectName("markdownPreview");
        page->preview->setFrameShape(QFrame::NoFrame); page->preview->setStyleSheet("QTextBrowser {background: #fcfbf9; border: none; padding: 12px; font-size: 15px;}");
        page->preview->document()->setDefaultStyleSheet("h1 {font-size: 26px; margin-top: 20px;} h2 {font-size: 21px; margin-top: 22px;} h3 {font-size: 18px;} p {line-height: 155%; margin-bottom: 12px;} pre {background-color: #f2f1f3; font-family: Consolas; padding: 12px;} code {font-family: Consolas; color: #946174;} blockquote {color: #8d8186; margin-left: 16px;} a {color: #cf465b;} table {border-collapse: collapse;} td, th {padding: 6px;}");
        page->splitter->addWidget(page->editor); page->splitter->addWidget(page->preview); page->splitter->setChildrenCollapsible(false);
        page->renderTimer = new QTimer(widget); page->renderTimer->setSingleShot(true); page->renderTimer->setInterval(250);
        connect(page->renderTimer, &QTimer::timeout, this, [this, page] { render(page); });
        connect(page->editor, &QPlainTextEdit::textChanged, this, [page] { page->document->setText(page->editor->toPlainText()); page->renderTimer->start(); });
        connect(document, &NoteDocument::reloaded, this, [this, page] {
            const int position = page->editor->textCursor().position(); const int scroll = page->editor->verticalScrollBar()->value();
            page->editor->setPlainText(page->document->text()); auto cursor = page->editor->textCursor(); cursor.setPosition(qMin(position, page->editor->document()->characterCount() - 1)); page->editor->setTextCursor(cursor); page->editor->verticalScrollBar()->setValue(scroll);
            render(page); refresh();
        });
        connect(document, &NoteDocument::statusChanged, this, [this, page] {
            if (page == m_active) refresh();
        });
        m_pages.insert(normalized, page); m_stack->addWidget(widget); render(page);
    }
    m_active = page; m_stack->setCurrentWidget(page->widget); refresh();
    QSettings().setValue("notes/lastFile", activePath()); emit activeChanged(); emit outlineChanged(); return true;
}
bool NoteEditor::flushAll() {
    for (auto *page : m_pages) if (!page->document->save()) {
        m_active = page; m_stack->setCurrentWidget(page->widget); refresh(); emit activeChanged(); emit outlineChanged();
        emit error("笔记尚未保存，请先处理保存失败或外部修改冲突。"); return false;
    }
    return true;
}
void NoteEditor::clearDocuments() {
    m_active = nullptr;
    for (auto *page : m_pages) { m_stack->removeWidget(page->widget); delete page->widget; delete page; }
    m_pages.clear(); m_stack->setCurrentIndex(0); refresh(); emit activeChanged(); emit outlineChanged();
}
void NoteEditor::render(Page *page) {
    const int scroll = page->preview->verticalScrollBar()->value();
    page->outline = markdownHeadings(page->editor->toPlainText());
    page->preview->document()->setBaseUrl(QUrl::fromLocalFile(QFileInfo(page->document->path()).absolutePath() + "/"));
    page->preview->setMarkdown(page->editor->toPlainText());
    for (auto block = page->preview->document()->firstBlock(); block.isValid(); block = block.next()) {
        QTextCursor cursor(block); auto format = block.blockFormat();
        const int level = format.headingLevel();
        const bool code = format.nonBreakableLines() || format.hasProperty(QTextFormat::BlockCodeFence) || format.hasProperty(QTextFormat::BlockCodeLanguage);
        if (code) {
            format.setBackground(QColor("#f1eff2")); format.setLeftMargin(12); format.setRightMargin(12);
            format.setTopMargin(0); format.setBottomMargin(0); format.setLineHeight(145, QTextBlockFormat::ProportionalHeight);
            QTextCharFormat characters; characters.setFontFamilies({"Consolas", "Microsoft YaHei UI"}); characters.setFontPointSize(10.5); cursor.movePosition(QTextCursor::StartOfBlock); cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor); cursor.mergeCharFormat(characters);
        } else {
            format.setTopMargin(level ? 18 : 5); format.setBottomMargin(level ? 12 : 8);
            format.setLineHeight(level ? 125 : 150, QTextBlockFormat::ProportionalHeight);
            if (level) { QTextCharFormat characters; characters.setFontPointSize(level == 1 ? 21 : level == 2 ? 17 : 14); characters.setFontWeight(QFont::DemiBold); cursor.movePosition(QTextCursor::StartOfBlock); cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor); cursor.mergeCharFormat(characters); }
        }
        cursor.clearSelection(); cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }
    page->preview->verticalScrollBar()->setValue(scroll);
    if (page == m_active) emit outlineChanged();
}
void NoteEditor::refresh() {
    m_title->setText(m_active ? QFileInfo(activePath()).completeBaseName() : "你的写作空间"); m_title->setToolTip(activePath());
    m_status->setText(m_active ? m_active->document->status() : QString());
    m_status->setToolTip(m_active ? m_active->document->status() : QString());
    m_conflict->setVisible(m_active && m_active->document->conflict());
    const int effectiveMode = m_mode == 2 && width() < 700 ? 0 : m_mode;
    for (int i = 0; i < 3; ++i) { m_modes[i]->setChecked(effectiveMode == i); m_modes[i]->setEnabled(m_active && (i != 2 || width() >= 700)); }
    if (m_active) { m_active->editor->setVisible(effectiveMode != 1); m_active->preview->setVisible(effectiveMode != 0); }
}
void NoteEditor::setMode(int mode) {
    if (mode < 0 || mode > 2 || (mode == 2 && width() < 700)) return;
    m_mode = mode; QSettings().setValue("notes/mode", mode); refresh();
}
void NoteEditor::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    refresh();
}
QVector<MarkdownHeading> NoteEditor::headings() const { return m_active ? markdownHeadings(m_active->editor->toPlainText()) : QVector<MarkdownHeading>(); }
void NoteEditor::jumpToHeading(int sourceLine) {
    if (!m_active) return;
    render(m_active);
    auto sourceBlock = m_active->editor->document()->findBlockByNumber(sourceLine);
    if (sourceBlock.isValid()) { QTextCursor cursor(sourceBlock); m_active->editor->setTextCursor(cursor); m_active->editor->ensureCursorVisible(); }
    int ordinal = -1;
    for (int i = 0; i < m_active->outline.size(); ++i) if (m_active->outline[i].sourceLine == sourceLine) ordinal = i;
    int index = 0;
    for (auto block = m_active->preview->document()->firstBlock(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() > 0 && index++ == ordinal) { m_active->preview->setTextCursor(QTextCursor(block)); m_active->preview->ensureCursorVisible(); break; }
    }
}
void NoteEditor::insertSong(const QString &title, const QString &artist, const QString &path) {
    if (!m_active) return;
    auto cursor = m_active->editor->textCursor(); if (m_mode == 1) cursor.movePosition(QTextCursor::End);
    cursor.insertText(QString("\n> 正在听：%1%2  \n> 本地路径：%3\n\n").arg(title, artist.isEmpty() ? QString() : " — " + artist, path));
    m_active->editor->setTextCursor(cursor);
}
void NoteEditor::resolveSaveAs() {
    if (!m_active) return;
    const auto chosen = QFileDialog::getSaveFileName(this, "将本地版本另存为", activePath(), "Markdown (*.md)");
    if (chosen.isEmpty()) return;
    const auto path = QDir::cleanPath(QFileInfo(chosen).absoluteFilePath());
    if (m_pages.contains(path) && m_pages[path] != m_active) { emit error("目标笔记已打开，请选择其他文件名。"); return; }
    const auto old = activePath();
    if (m_active->document->saveAs(path)) { m_pages.remove(old); m_pages.insert(path, m_active); render(m_active); refresh(); QSettings().setValue("notes/lastFile", path); emit activeChanged(); }
}
