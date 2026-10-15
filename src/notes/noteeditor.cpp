#include "notes/noteeditor.h"
#include "widgets/uihelpers.h"
#include "notes/webnoteeditor.h"
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextDocument>
#include <QTextCursor>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QSettings>
#include <QShortcut>
#include <QTimer>
#include <QScrollBar>
#include <QPainter>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QClipboard>
#include <QApplication>
#include <QScreen>

namespace {
QWidget *markdownHelp(QWidget *parent) {
    auto *popup = new QWidget(parent, Qt::Popup);
    popup->setObjectName("markdownHelpPopup");
    popup->setAccessibleName("Markdown 语法帮助");
    popup->setStyleSheet(R"(
        QWidget#markdownHelpPopup { background: #fcfbf9; border: 1px solid #e8e1e3; border-radius: 10px; }
        QLabel#helpTitle { font-size: 17px; font-weight: 600; }
        QLabel#helpSection { color: #ce3d51; font-weight: 600; padding-top: 8px; }
        QLabel#helpExample { background: #f3f0f2; border-radius: 8px; padding: 10px; font-family: Consolas, 'Microsoft YaHei UI'; }
        QLabel#helpDescription { color: #8a818a; }
        QPushButton { padding: 5px 9px; }
    )");
    auto *layout = new QVBoxLayout(popup); layout->setContentsMargins(16, 16, 16, 16); layout->setSpacing(10);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel("Markdown 语法速查"); title->setObjectName("helpTitle"); header->addWidget(title, 1);
    auto *close = new QPushButton("关闭"); close->setAccessibleName("关闭 Markdown 帮助"); header->addWidget(close);
    QObject::connect(close, &QPushButton::clicked, popup, &QWidget::hide); layout->addLayout(header);
    auto *intro = new QLabel("查看语法，复制示例，再回到笔记中使用。"); intro->setWordWrap(true); intro->setObjectName("helpDescription"); layout->addWidget(intro);
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); layout->addWidget(scroll, 1);
    auto *content = new QWidget; auto *items = new QVBoxLayout(content); items->setContentsMargins(0, 0, 10, 0); items->setSpacing(8);
    const auto section = [items](const QString &name) {
        auto *label = new QLabel(name); label->setObjectName("helpSection"); items->addWidget(label);
    };
    const auto example = [items, popup](const QString &name, const QString &sample, const QString &description = QString()) {
        auto *row = new QHBoxLayout; auto *label = new QLabel(name); row->addWidget(label, 1);
        auto *copy = new QPushButton("复制示例"); copy->setObjectName("copyMarkdownExample");
        copy->setAccessibleName("复制" + name + "示例"); copy->setProperty("markdownExample", sample); row->addWidget(copy);
        QObject::connect(copy, &QPushButton::clicked, popup, [copy, sample] {
            QApplication::clipboard()->setText(sample); copy->setText("已复制");
            QTimer::singleShot(1200, copy, [copy] { copy->setText("复制示例"); });
        });
        items->addLayout(row);
        auto *code = new QLabel(sample); code->setObjectName("helpExample"); code->setTextFormat(Qt::PlainText);
        code->setWordWrap(true); code->setTextInteractionFlags(Qt::TextSelectableByMouse); items->addWidget(code);
        if (!description.isEmpty()) {
            auto *hint = new QLabel(description); hint->setObjectName("helpDescription"); hint->setWordWrap(true); items->addWidget(hint);
        }
    };
    section("标题");
    example("标题层级", "# 一级标题\n## 二级标题\n### 三级标题", "# 后留一个空格；最多可使用六个 #。");
    section("文字格式");
    example("加粗", "**加粗文字**"); example("斜体", "*斜体文字*"); example("行内代码", "`代码或命令`");
    section("列表");
    example("无序列表", "- 第一项\n- 第二项"); example("有序列表", "1. 第一项\n2. 第二项");
    example("任务列表", "- [ ] 未完成\n- [x] 已完成", "方括号内留空格或填 x；与软件的独立待办清单互不同步。");
    section("引用与分隔线");
    example("引用", "> 引用的文字"); example("分隔线", "\n---\n", "分隔线上下留空行，避免被识别为标题。");
    section("链接与图片");
    example("链接", "[链接名称](https://example.com)");
    example("本地图片", "![图片说明](images/photo.png)", "路径相对当前笔记所在文件夹，图片需位于所选笔记目录内；不会自动复制或上传图片。");
    section("代码块");
    example("带语言的代码块", "```cpp\nint main() {\n    return 0;\n}\n```", "上下各用三个反引号闭合；cpp 可换成其他语言，也可省略。");
    section("常见提醒");
    auto *tips = new QLabel("• 段落之间空一行。\n• Markdown 标记使用半角符号。\n• 代码块的起止反引号需要成对出现。\n• Ctrl+S 保存，输入停止后也会自动保存。");
    tips->setWordWrap(true); tips->setObjectName("helpDescription"); items->addWidget(tips); items->addStretch(); scroll->setWidget(content);
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), popup);
    QObject::connect(escape, &QShortcut::activated, popup, &QWidget::hide);
    return popup;
}
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
            qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
            while (block.isValid() && top <= gutter->height()) {
                const qreal height = blockBoundingRect(block).height();
                if (block.isVisible() && top + height >= 0 && block.layout()->lineCount()) {
                    // Match the actual first visual line's baseline, including fallback fonts (e.g. emoji).
                    const auto line = block.layout()->lineAt(0);
                    const auto number = QString::number(block.blockNumber() + 1);
                    painter.drawText(QPointF(gutter->width() - 10 - QFontMetricsF(font()).horizontalAdvance(number),
                                            top + line.y() + line.ascent()), number);
                }
                block = block.next();
                if (block.isValid()) top = blockBoundingGeometry(block).translated(contentOffset()).top();
            }
            return true;
        }
        return QPlainTextEdit::eventFilter(object, event);
    }
private:
    void margins() {
        const int width = 20 + fontMetrics().horizontalAdvance(QString::number(qMax(99, blockCount())));
        setViewportMargins(width, 0, 0, 0);
        // Block geometry is relative to the viewport, whose origin includes the stylesheet padding.
        const auto area = viewport()->geometry();
        gutter->setGeometry(area.left() - width, area.top(), width, area.height()); gutter->update();
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
    m_status = new ElidedLabel(QString());
    m_status->setObjectName("noteSaveStatus"); m_status->setProperty("role", "small");
    m_status->setWordWrap(false); m_status->setFixedWidth(112);
    m_status->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    heading->addWidget(m_title, 1); heading->addWidget(m_status); layout->addLayout(heading);
    auto *toolbar = new QHBoxLayout; toolbar->setSpacing(6);
    const QStringList names{"源码", "预览"};
    for (int i : {0, 1}) {
        m_modes[i] = new QPushButton(names[i]); m_modes[i]->setObjectName("noteMode" + QString::number(i)); m_modes[i]->setCheckable(true); toolbar->addWidget(m_modes[i]);
        connect(m_modes[i], &QPushButton::clicked, this, [this, i] { setMode(i); });
    }
    toolbar->addStretch();
    m_help = new QPushButton("Markdown 帮助"); m_help->setObjectName("markdownHelpButton");
    m_help->setToolTip("查看 Markdown 常用语法与示例"); m_help->setStyleSheet("padding: 7px 8px; font-size: 12px;"); toolbar->addWidget(m_help);
    connect(m_help, &QPushButton::clicked, this, [this] {
        if (!m_helpPopup) m_helpPopup = markdownHelp(this);
        const auto screen = m_help->screen()->availableGeometry();
        m_helpPopup->resize(qMin(440, screen.width() - 24), qMin(600, screen.height() - 24));
        const auto anchor = m_help->mapToGlobal(QPoint(m_help->width(), m_help->height()));
        int x = anchor.x() - m_helpPopup->width(); int y = anchor.y() + 6;
        if (y + m_helpPopup->height() > screen.bottom() - 8) y = m_help->mapToGlobal(QPoint(0, 0)).y() - m_helpPopup->height() - 6;
        x = qBound(screen.left() + 8, x, screen.right() - m_helpPopup->width() - 8);
        y = qBound(screen.top() + 8, y, screen.bottom() - m_helpPopup->height() - 8);
        m_helpPopup->move(x, y); m_helpPopup->show(); m_helpPopup->setFocus();
    });
    auto *save = new QPushButton("保存"); save->setObjectName("saveNote"); toolbar->addWidget(save);
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
    // Keep source mode (0); migrate the former split mode (2) to editable preview (1).
    m_mode = QSettings().value("notes/mode", 1).toInt() == 0 ? 0 : 1;
    QSettings().setValue("notes/mode", m_mode);
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
        layout->addWidget(page->editor); layout->addWidget(page->preview);
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
    for (int i = 0; i < 2; ++i) { m_modes[i]->setChecked(m_mode == i); m_modes[i]->setEnabled(m_active != nullptr); }
    m_help->setVisible(m_mode == 0);
    if (m_mode != 0 && m_helpPopup) m_helpPopup->hide();
    if (m_active) { m_active->editor->setVisible(m_mode == 0); m_active->preview->setVisible(m_mode == 1); m_active->preview->setReadOnly(m_mode == 0); }
}
void NoteEditor::setMode(int mode) {
    if (mode < 0 || mode > 1) return;
    if (mode != m_mode && !flushAll()) return;
    if (m_active) render(m_active);
    m_mode = mode; QSettings().setValue("notes/mode", mode); refresh();
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
