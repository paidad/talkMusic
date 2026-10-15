#include "notes/notenavigator.h"
#include "notes/notestore.h"
#include "notes/noteeditor.h"
#include <QTreeWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QSettings>
#include <QDesktopServices>
#include <QSignalBlocker>
#include <QStyle>

NoteNavigator::NoteNavigator(NoteStore *store, NoteEditor *editor, QWidget *parent) : QWidget(parent), m_store(store), m_editor(editor) {
    setObjectName("noteNavigator"); auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 10, 0, 0); layout->setSpacing(10);
    auto *tabs = new QTabWidget; tabs->setObjectName("noteNavigationTabs");
    auto *documents = new QWidget; auto *documentLayout = new QVBoxLayout(documents); documentLayout->setContentsMargins(0, 12, 0, 0); documentLayout->setSpacing(10);
    auto *folder = new QPushButton("选择笔记文件夹"); folder->setObjectName("chooseNotesFolder"); documentLayout->addWidget(folder); connect(folder, &QPushButton::clicked, this, &NoteNavigator::chooseFolder);
    m_rootLabel = new QLabel; m_rootLabel->setTextFormat(Qt::PlainText); m_rootLabel->setProperty("role", "small"); m_rootLabel->setWordWrap(true); documentLayout->addWidget(m_rootLabel);
    m_search = new QLineEdit; m_search->setObjectName("noteSearch"); m_search->setPlaceholderText("搜索文档名称"); m_search->setClearButtonEnabled(true); documentLayout->addWidget(m_search);
    auto *actions = new QHBoxLayout; actions->setSpacing(6);
    auto *note = new QPushButton("新建笔记"); note->setObjectName("newNote"); auto *directory = new QPushButton("新建文件夹"); directory->setObjectName("newNoteFolder"); actions->addWidget(note); actions->addWidget(directory); documentLayout->addLayout(actions);
    connect(note, &QPushButton::clicked, this, [this] { create(false); }); connect(directory, &QPushButton::clicked, this, [this] { create(true); });
    m_files = new QTreeWidget; m_files->setObjectName("noteFileTree"); m_files->setHeaderHidden(true); m_files->setIndentation(15); m_files->setContextMenuPolicy(Qt::CustomContextMenu); m_files->setDragDropMode(QAbstractItemView::NoDragDrop); documentLayout->addWidget(m_files, 1);
    m_empty = new QLabel; m_empty->setProperty("role", "muted"); m_empty->setWordWrap(true); documentLayout->addWidget(m_empty);
    auto *trash = new QPushButton("打开系统回收站"); documentLayout->addWidget(trash);
    connect(trash, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("shell:RecycleBinFolder")); });
    tabs->addTab(documents, "文档");
    m_outline = new QTreeWidget; m_outline->setObjectName("noteOutline"); m_outline->setHeaderHidden(true); m_outline->setIndentation(15); tabs->addTab(m_outline, "大纲"); layout->addWidget(tabs, 1);
    connect(m_search, &QLineEdit::textChanged, this, &NoteNavigator::refresh);
    connect(m_files, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (item && QFileInfo(item->data(0, Qt::UserRole).toString()).isFile()) {
            if (!m_editor->openFile(item->data(0, Qt::UserRole).toString())) refresh();
        }
    });
    connect(m_files, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        auto *item = m_files->itemAt(position); if (!item) return;
        m_files->setCurrentItem(item); if (!m_files->currentItem()) return;
        QMenu menu; auto *rename = menu.addAction("重命名"); auto *remove = menu.addAction("移入系统回收站");
        auto *selected = menu.exec(m_files->viewport()->mapToGlobal(position));
        if (selected == rename) renameSelected(); else if (selected == remove) deleteSelected();
    });
    connect(m_outline, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) { const auto line = item->data(0, Qt::UserRole); if (line.isValid()) m_editor->jumpToHeading(line.toInt()); });
    connect(store, &NoteStore::changed, this, &NoteNavigator::refresh); connect(editor, &NoteEditor::outlineChanged, this, &NoteNavigator::outline);
    connect(editor, &NoteEditor::activeChanged, this, [this] { QTimer::singleShot(0, this, [this] { refreshSelection(true); outline(); }); });
    refresh(); outline();
}
void NoteNavigator::chooseFolder() {
    const auto path = QFileDialog::getExistingDirectory(this, "选择笔记文件夹", m_store->root());
    if (path.isEmpty() || !m_editor->flushAll()) return;
    m_editor->clearDocuments(); m_store->setRoot(path); QSettings().setValue("notes/root", m_store->root()); QSettings().remove("notes/lastFile");
}
QString NoteNavigator::selectedPath() const { return m_files->currentItem() ? m_files->currentItem()->data(0, Qt::UserRole).toString() : QString(); }
void NoteNavigator::populate(QTreeWidgetItem *parent, const QString &directory) {
    const auto query = m_search->text().trimmed();
    for (const auto &entry : QDir(directory).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::DirsFirst | QDir::Name)) {
        if (entry.isSymLink() || entry.isJunction() || (!entry.isDir() && !entry.fileName().endsWith(".md", Qt::CaseInsensitive))) continue;
        if (!entry.isDir() && !entry.fileName().contains(query, Qt::CaseInsensitive)) continue;
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_files);
        item->setText(0, entry.fileName()); item->setData(0, Qt::UserRole, entry.absoluteFilePath()); item->setToolTip(0, entry.absoluteFilePath());
        item->setIcon(0, style()->standardIcon(entry.isDir() ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
        if (entry.isDir()) { populate(item, entry.absoluteFilePath()); if (!query.isEmpty() && !item->childCount()) { delete item; continue; } }
        if (!query.isEmpty()) item->setExpanded(true);
    }
}
void NoteNavigator::refresh() {
    refreshSelection(false);
}
void NoteNavigator::refreshSelection(bool preferActive) {
    QStringList expanded; QTreeWidgetItemIterator old(m_files);
    while (*old) { if ((*old)->isExpanded()) expanded << (*old)->data(0, Qt::UserRole).toString(); ++old; }
    const auto selected = selectedPath(); const auto active = m_editor->activePath();
    QSignalBlocker blocker(m_files); m_files->clear();
    m_rootLabel->setText(m_store->root().isEmpty() ? "尚未选择笔记目录" : QFileInfo(m_store->root()).fileName()); m_rootLabel->setToolTip(m_store->root());
    if (m_store->available()) populate(nullptr, m_store->root());
    m_empty->setText(!m_store->available() ? (m_store->root().isEmpty() ? "选择一个文件夹，开始管理你的 Markdown 笔记。" : "原笔记目录暂时不可用。设置已保留，请重新选择或恢复目录。") : !m_files->topLevelItemCount() ? "没有符合条件的文档。" : QString());
    QTreeWidgetItemIterator it(m_files);
    QTreeWidgetItem *activeItem = nullptr, *selectedItem = nullptr;
    while (*it) {
        const auto path = (*it)->data(0, Qt::UserRole).toString();
        if (expanded.contains(path)) (*it)->setExpanded(true);
        if (path == active) activeItem = *it; if (path == selected) selectedItem = *it; ++it;
    }
    if (activeItem || selectedItem) m_files->setCurrentItem(preferActive && activeItem ? activeItem : selectedItem ? selectedItem : activeItem);
}
void NoteNavigator::outline() {
    m_outline->clear(); QVector<QTreeWidgetItem *> parents(7, nullptr);
    for (const auto &heading : m_editor->headings()) {
        QTreeWidgetItem *parent = nullptr;
        for (int level = heading.level - 1; level > 0; --level) if (parents[level]) { parent = parents[level]; break; }
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_outline);
        item->setText(0, heading.title.isEmpty() ? "（空标题）" : heading.title); item->setData(0, Qt::UserRole, heading.sourceLine); item->setExpanded(true);
        parents[heading.level] = item; for (int level = heading.level + 1; level < 7; ++level) parents[level] = nullptr;
    }
    if (!m_outline->topLevelItemCount()) { auto *item = new QTreeWidgetItem(m_outline); item->setText(0, m_editor->activePath().isEmpty() ? "打开笔记后显示大纲" : "当前文档没有标题"); item->setFlags(Qt::NoItemFlags); }
}
void NoteNavigator::create(bool directory) {
    if (!m_store->available() || !m_editor->flushAll()) return;
    auto parent = selectedPath(); if (!QFileInfo(parent).isDir()) parent = parent.isEmpty() ? m_store->root() : QFileInfo(parent).absolutePath();
    bool ok; const auto name = QInputDialog::getText(this, directory ? "新建文件夹" : "新建笔记", "名称", QLineEdit::Normal, directory ? "新文件夹" : "未命名.md", &ok);
    if (!ok) return; const auto path = m_store->create(parent, name, directory); if (!path.isEmpty() && !directory) m_editor->openFile(path);
}
void NoteNavigator::renameSelected() {
    const auto path = selectedPath(); if (path.isEmpty() || !m_editor->flushAll()) return;
    bool ok; const auto name = QInputDialog::getText(this, "重命名", "新名称", QLineEdit::Normal, QFileInfo(path).fileName(), &ok);
    if (ok) m_store->rename(path, name);
}
void NoteNavigator::deleteSelected() {
    const auto path = selectedPath(); if (path.isEmpty() || !m_editor->flushAll()) return;
    if (QMessageBox::question(this, "移入系统回收站", "将此文档或文件夹移入系统回收站？可在系统回收站中恢复。", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) m_store->trash(path);
}
