#pragma once
#include <QWidget>
class NoteStore;
class NoteEditor;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QLabel;
class NoteNavigator : public QWidget {
    Q_OBJECT
public:
    explicit NoteNavigator(NoteStore *store, NoteEditor *editor, QWidget *parent = nullptr);
    void chooseFolder();
    void refresh();
private:
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
