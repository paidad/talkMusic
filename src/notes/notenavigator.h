#pragma once
#include <QWidget>
class NoteStore;
class NoteEditor;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QLabel;
// 右侧“文档 / 大纲”：负责树展示与选择，将文件变更委托给 NoteStore。
class NoteNavigator : public QWidget {
    Q_OBJECT
public:
    explicit NoteNavigator(NoteStore *store, NoteEditor *editor, QWidget *parent = nullptr);
    void chooseFolder();
    void refresh();
private:
    // 重建树时保留展开状态；程序主动切文档时优先选中当前文档。
    void refreshSelection(bool preferActive);
    void populate(QTreeWidgetItem *parent, const QString &directory);
    void outline();
    QString selectedPath() const;
    void create(bool directory);
    void renameSelected();
    void deleteSelected();
    NoteStore *m_store;
    NoteEditor *m_editor;
    QTreeWidget *m_files, *m_outline;
    QLineEdit *m_search;
    QLabel *m_rootLabel, *m_empty;
};
