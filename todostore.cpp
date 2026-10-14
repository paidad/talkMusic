#include "todostore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QSet>

namespace { QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); } }
TodoStore::TodoStore(QObject *parent, const QString &directory) : QObject(parent),
    m_directory(directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/todos" : directory) {
    for (const auto &file : QDir(m_directory).entryInfoList({"*.todo.json"}, QDir::Files | QDir::NoSymLinks, QDir::Name)) {
        TodoList list;
        if (read(file.absoluteFilePath(), list) && !find(list.id)) m_lists.append(list);
        else QTimer::singleShot(0, this, [this, path = file.fileName()] { emit error("待办文件无法读取或格式版本不支持，原件已保留：" + path); });
    }
}
QString TodoStore::filePath(const QString &id, bool trash) const { return m_directory + (trash ? "/trash/" : "/") + id + ".todo.json"; }
const TodoList *TodoStore::find(const QString &id) const { for (const auto &list : m_lists) if (list.id == id) return &list; return nullptr; }
bool TodoStore::read(const QString &path, TodoList &list) const {
    QFile file(path); if (QFileInfo(path).isSymLink() || !file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError parse; const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) return false;
    const auto object = document.object();
    if (object.value("version").toInt(-1) != 1 || !object.value("id").isString() || !object.value("title").isString() || !object.value("tasks").isArray()) return false;
    list.id = object.value("id").toString(); list.title = object.value("title").toString();
    if (QUuid(list.id).isNull() || QFileInfo(path).fileName() != list.id + ".todo.json" || list.title.trimmed().isEmpty()) return false;
    QSet<QString> ids;
    for (const auto &value : object.value("tasks").toArray()) {
        if (!value.isObject()) return false;
        const auto task = value.toObject();
        if (!task.value("id").isString() || !task.value("title").isString() || !task.value("notes").isString() || !task.value("completed").isBool()) return false;
        TodoTask parsed{task.value("id").toString(), task.value("title").toString(), task.value("notes").toString(), task.value("completed").toBool()};
        if (QUuid(parsed.id).isNull() || ids.contains(parsed.id) || parsed.title.trimmed().isEmpty()) return false;
        ids.insert(parsed.id); list.tasks.append(parsed);
    }
    return true;
}
QVector<TodoList> TodoStore::recycled() const {
    QVector<TodoList> result;
    if (QFileInfo(m_directory + "/trash").isSymLink() || QFileInfo(m_directory + "/trash").isJunction()) return result;
    for (const auto &file : QDir(m_directory + "/trash").entryInfoList({"*.todo.json"}, QDir::Files | QDir::NoSymLinks, QDir::Name)) {
        TodoList list; if (read(file.absoluteFilePath(), list)) result.append(list);
    }
    return result;
}
bool TodoStore::write(const TodoList &list) {
    if (QFileInfo(m_directory).isSymLink() || !QDir().mkpath(m_directory) || QFileInfo(filePath(list.id)).isSymLink()) { emit error("待办保存目录不可用。"); return false; }
    if (QFileInfo::exists(filePath(list.id))) {
        TodoList disk; if (!read(filePath(list.id), disk)) { emit error("待办文件损坏或版本不支持，原文件已保留，修改未应用。"); return false; }
    }
    QJsonArray tasks;
    for (const auto &task : list.tasks) tasks.append(QJsonObject{{"id", task.id}, {"title", task.title}, {"notes", task.notes}, {"completed", task.completed}});
    const auto data = QJsonDocument(QJsonObject{{"version", 1}, {"id", list.id}, {"title", list.title}, {"tasks", tasks}}).toJson();
    QSaveFile file(filePath(list.id));
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) { emit error("待办保存失败，修改未应用：" + file.errorString()); return false; }
    return true;
}
bool TodoStore::replace(const TodoList &list) {
    if (!write(list)) return false;
    for (auto &existing : m_lists) if (existing.id == list.id) { existing = list; emit changed(); return true; }
    return false;
}
QString TodoStore::createList(const QString &title) {
    if (title.trimmed().isEmpty()) return {};
    TodoList list{uuid(), title.trimmed(), {}}; if (!write(list)) return {};
    m_lists.append(list); emit changed(); return list.id;
}
bool TodoStore::renameList(const QString &id, const QString &title) {
    const auto *list = find(id); if (!list || title.trimmed().isEmpty()) return false;
    auto updated = *list; updated.title = title.trimmed(); return replace(updated);
}
bool TodoStore::trashList(const QString &id) {
    if (!find(id)) return false;
    const auto trashDirectory = m_directory + "/trash";
    if (QFileInfo(trashDirectory).isSymLink() || !QDir().mkpath(trashDirectory) || !QFile::rename(filePath(id), filePath(id, true))) {
        emit error("无法移入待办回收区，原清单仍保留。"); return false;
    }
    for (int i = 0; i < m_lists.size(); ++i) if (m_lists[i].id == id) { m_lists.removeAt(i); break; }
    emit changed(); return true;
}
bool TodoStore::restoreList(const QString &id) {
    TodoList list;
    if (QUuid(id).isNull() || find(id) || !read(filePath(id, true), list) || !QFile::rename(filePath(id, true), filePath(id))) {
        emit error("无法恢复清单：文件无效或目标已存在。"); return false;
    }
    m_lists.append(list); emit changed(); return true;
}
bool TodoStore::addTask(const QString &id, const QString &title, const QString &notes) {
    const auto *list = find(id); if (!list || title.trimmed().isEmpty()) return false;
    auto updated = *list; updated.tasks.append({uuid(), title.trimmed(), notes, false}); return replace(updated);
}
bool TodoStore::editTask(const QString &id, const QString &taskId, const QString &title, const QString &notes) {
    const auto *list = find(id); if (!list || title.trimmed().isEmpty()) return false;
    auto updated = *list;
    for (auto &task : updated.tasks) if (task.id == taskId) { task.title = title.trimmed(); task.notes = notes; return replace(updated); }
    return false;
}
bool TodoStore::completeTask(const QString &id, const QString &taskId, bool completed) {
    const auto *list = find(id); if (!list) return false;
    auto updated = *list;
    for (auto &task : updated.tasks) if (task.id == taskId) { task.completed = completed; return replace(updated); }
    return false;
}
bool TodoStore::removeTask(const QString &id, const QString &taskId) {
    const auto *list = find(id); if (!list) return false;
    auto updated = *list;
    for (int i = 0; i < updated.tasks.size(); ++i) if (updated.tasks[i].id == taskId) {
        Deleted deleted{updated.tasks[i], i}; updated.tasks.removeAt(i);
        if (!replace(updated)) return false;
        m_deleted[id].append(deleted); emit changed(); return true;
    }
    return false;
}
bool TodoStore::undoRemoval(const QString &id) {
    const auto *list = find(id); if (!list || !canUndo(id)) return false;
    const auto deleted = m_deleted[id].last(); auto updated = *list;
    updated.tasks.insert(qMin(deleted.index, int(updated.tasks.size())), deleted.task);
    if (!replace(updated)) return false;
    m_deleted[id].removeLast(); emit changed(); return true;
}
