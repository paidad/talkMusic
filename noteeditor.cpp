#include "noteeditor.h"
#include "uihelpers.h"
#include "webnoteeditor.h"
#include <QPlainTextEdit>
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
#include <QTimer>
#include <QResizeEvent>
#include <QScrollBar>
#include <QPainter>
#include <QSignalBlocker>

namespace {
class SourceEditor : public QPlainTextEdit {
public:
    SourceEditor() {
        gutter = new QWidget(this);
        gutter->installEventFilter(this);
        connect(this, &QPlainTextEdit::blockCountChanged, this, [this] { margins(); });
        connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &, int) { gutter->update(); });
        margins();
    }
protected:
    void resizeEvent(QResizeEvent *event) override {
        QPlainTextEdit::resizeEvent(event); margins();
    }
    bool eventFilter(QObject *object, QEvent *event) override {
        if (object == gutter && event->type() == QEvent::Paint) {
            QPainter painter(gutter); painter.fillRect(gutter->rect(), QColor("#f5f2f4"));
            painter.setPen(QColor("#aaa0ac")); painter.setFont(font());
            auto block = firstVisibleBlock();
            int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
            while (block.isValid() && top <= gutter->height()) {
                const int height = qRound(blockBoundingRect(block).height());
                if (block.isVisible() && top + height >= 0)
                    painter.drawText(0, top, gutter->width() - 10, fontMetrics().height(), Qt::AlignRight, QString::number(block.blockNumber() + 1));
                top += height; block = block.next();
            }
            return true;
        }
        return QPlainTextEdit::eventFilter(object, event);
    }
private:
    void margins() {
        const int width = 20 + fontMetrics().horizontalAdvance(QString::number(qMax(99, blockCount())));
        setViewportMargins(width, 0, 0, 0); gutter->setGeometry(0, 0, width, height()); gutter->update();
    }
    QWidget *gutter;
};
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
bool underneath(const QString &path, const QString &parent) { return path == parent || path.startsWith(parent + "/"); }
}

struct NoteEditor::Page {
    QWidget *widget;
    QSplitter *splitter;
    QPlainTextEdit *editor;
    WebNoteEditor *preview;
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
    const QStringList names{"源码", "编辑", "分屏"};
    for (int i : {1, 0, 2}) {
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
    connect(overwrite, &QPushButton::clicked, this, [this] { if (m_active && m_active->preview->synchronize()) m_active->document->overwrite(); });
    connect(saveAs, &QPushButton::clicked, this, &NoteEditor::resolveSaveAs); layout->addWidget(m_conflict);
    m_stack = new QStackedWidget; layout->addWidget(m_stack, 1);
    auto *welcome = new QLabel("让音乐陪你记下灵感\n\n在右侧选择笔记文件夹，\n打开一篇文档，或新建你的第一篇笔记。");
    welcome->setObjectName("noteWelcome"); welcome->setAlignment(Qt::AlignCenter); welcome->setWordWrap(true); welcome->setProperty("role", "muted"); m_stack->addWidget(welcome);
    auto *hint = new QLabel("Ctrl+S 保存 · 输入停止后自动保存 · 笔记与待办独立管理"); hint->setWordWrap(true); hint->setProperty("role", "small"); layout->addWidget(hint);
    m_mode = qBound(0, QSettings().value("notes/mode", 1).toInt(), 2);
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
WebNoteEditor *NoteEditor::activeWebEditor() const { return m_active ? m_active->preview : nullptr; }
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
        page->editor = new SourceEditor; page->editor->setObjectName("markdownSource"); page->editor->setPlainText(document->text());
        page->editor->setStyleSheet("QPlainTextEdit {font-family: Consolas, 'Microsoft YaHei UI'; font-size: 14px; background: #fcfbf9; border: none; padding: 10px; selection-background-color: #e8aab4;}");
        page->editor->setTabStopDistance(32); new MarkdownHighlighter(page->editor->document());
        page->preview = new WebNoteEditor(m_store, normalized, document->text());
        connect(page->preview, &WebNoteEditor::error, this, &NoteEditor::error);
        connect(page->preview, &WebNoteEditor::saveRequest, this, [this] { flushAll(); });
        connect(page->preview, &WebNoteEditor::textChanged, this, [this, page](const QString &text) {
            QSignalBlocker blocked(page->editor);
            const int position = page->editor->textCursor().position();
            const int scroll = page->editor->verticalScrollBar()->value();
            // A single undoable replacement preserves the source editor's history.
            auto cursor = page->editor->textCursor(); cursor.beginEditBlock();
            cursor.select(QTextCursor::Document); cursor.insertText(text); cursor.endEditBlock();
            cursor.setPosition(qMin(position, text.size())); page->editor->setTextCursor(cursor);
            page->editor->verticalScrollBar()->setValue(scroll);
            page->document->setText(text); page->outline = markdownHeadings(text);
            if (page == m_active) emit outlineChanged();
        });
        page->splitter->addWidget(page->editor); page->splitter->addWidget(page->preview); page->splitter->setChildrenCollapsible(false);
        page->renderTimer = new QTimer(widget); page->renderTimer->setSingleShot(true); page->renderTimer->setInterval(250);
        connect(page->renderTimer, &QTimer::timeout, this, [this, page] { render(page); });
        connect(page->editor, &QPlainTextEdit::textChanged, this, [page] { page->document->setText(page->editor->toPlainText()); page->renderTimer->start(); });
        connect(document, &NoteDocument::reloaded, this, [this, page] {
            const int position = page->editor->textCursor().position(); const int scroll = page->editor->verticalScrollBar()->value();
            page->editor->setPlainText(page->document->text()); auto cursor = page->editor->textCursor(); cursor.setPosition(qMin(position, page->editor->document()->characterCount() - 1)); page->editor->setTextCursor(cursor); page->editor->verticalScrollBar()->setValue(scroll);
            page->preview->setDocument(page->document->path(), page->document->text(), true);
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
    if (m_flushing) return false;
    m_flushing = true;
    for (auto *page : m_pages) {
        render(page); // Deliver any native-source edit before requesting a browser snapshot.
        if (page->preview->synchronize() && page->document->save()) continue;
        m_active = page; m_stack->setCurrentWidget(page->widget); refresh(); emit activeChanged(); emit outlineChanged();
        m_flushing = false;
        emit error("笔记尚未保存，请先处理同步失败、保存失败或外部修改冲突。"); return false;
    }
    m_flushing = false;
    return true;
}
void NoteEditor::clearDocuments() {
    m_active = nullptr;
    for (auto *page : m_pages) { m_stack->removeWidget(page->widget); delete page->widget; delete page; }
    m_pages.clear(); m_stack->setCurrentIndex(0); refresh(); emit activeChanged(); emit outlineChanged();
}
void NoteEditor::render(Page *page) {
    page->outline = markdownHeadings(page->editor->toPlainText());
    page->preview->setDocument(page->document->path(), page->editor->toPlainText());
    if (page == m_active) emit outlineChanged();
}
void NoteEditor::refresh() {
    m_title->setText(m_active ? QFileInfo(activePath()).completeBaseName() : "你的写作空间"); m_title->setToolTip(activePath());
    m_status->setText(m_active ? m_active->document->status() : QString());
    m_status->setToolTip(m_active ? m_active->document->status() : QString());
    m_conflict->setVisible(m_active && m_active->document->conflict());
    const int effectiveMode = m_mode == 2 && width() < 700 ? 1 : m_mode;
    for (int i = 0; i < 3; ++i) { m_modes[i]->setChecked(effectiveMode == i); m_modes[i]->setEnabled(m_active && (i != 2 || width() >= 700)); }
    if (m_active) { m_active->editor->setVisible(effectiveMode != 1); m_active->preview->setVisible(effectiveMode != 0); m_active->preview->setReadOnly(effectiveMode == 2 || effectiveMode == 0); }
}
void NoteEditor::setMode(int mode) {
    if (mode < 0 || mode > 2 || (mode == 2 && width() < 700)) return;
    if (mode != m_mode && !flushAll()) return;
    if (m_active) render(m_active);
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
    m_active->preview->jumpToHeading(ordinal);
}
void NoteEditor::insertSong(const QString &title, const QString &artist, const QString &path) {
    if (!m_active) return;
    const auto text = QString("\n> 正在听：%1%2  \n> 本地路径：%3\n\n").arg(title, artist.isEmpty() ? QString() : " — " + artist, path);
    if (!m_active->editor->isVisible()) { m_active->preview->insertMarkdown(text); return; }
    auto cursor = m_active->editor->textCursor(); cursor.insertText(text);
    m_active->editor->setTextCursor(cursor);
}
void NoteEditor::resolveSaveAs() {
    if (!m_active) return;
    if (!m_active->preview->synchronize()) return;
    const auto chosen = QFileDialog::getSaveFileName(this, "将本地版本另存为", activePath(), "Markdown (*.md)");
    if (chosen.isEmpty()) return;
    const auto path = QDir::cleanPath(QFileInfo(chosen).absoluteFilePath());
    if (m_pages.contains(path) && m_pages[path] != m_active) { emit error("目标笔记已打开，请选择其他文件名。"); return; }
    const auto old = activePath();
    if (m_active->document->saveAs(path)) { m_pages.remove(old); m_pages.insert(path, m_active); render(m_active); refresh(); QSettings().setValue("notes/lastFile", path); emit activeChanged(); }
}
