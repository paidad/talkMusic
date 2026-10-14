#pragma once
#include <QObject>
#include <QVector>
#include <QMap>

struct TodoTask { QString id, title, notes; bool completed = false; };
struct TodoList { QString id, title; QVector<TodoTask> tasks; };
class TodoStore : public QObject {
    Q_OBJECT
public:
    explicit TodoStore(QObject *parent = nullptr, const QString &directory = {});
    QVector<TodoList> lists() const { return m_lists; }
    QVector<TodoList> recycled() const;
    const TodoList *find(const QString &id) const;
    QString createList(const QString &title);
    bool renameList(const QString &id, const QString &title);
    bool trashList(const QString &id);
    bool restoreList(const QString &id);
    bool addTask(const QString &listId, const QString &title, const QString &notes = {});
    bool editTask(const QString &listId, const QString &taskId, const QString &title, const QString &notes);
    bool completeTask(const QString &listId, const QString &taskId, bool completed);
    bool removeTask(const QString &listId, const QString &taskId);
    bool undoRemoval(const QString &listId);
    bool canUndo(const QString &listId) const { return !m_deleted.value(listId).isEmpty(); }
signals:
    void changed();
    void error(const QString &message);
private:
    struct Deleted { TodoTask task; int index; };
    QString filePath(const QString &id, bool trash = false) const;
    bool read(const QString &path, TodoList &list) const;
    bool write(const TodoList &list);
    bool replace(const TodoList &list);
    QString m_directory;
    QVector<TodoList> m_lists;
    QMap<QString, QVector<Deleted>> m_deleted;
};
