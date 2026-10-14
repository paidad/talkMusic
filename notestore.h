#pragma once
#include <QObject>
#include <QString>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QDateTime>

class NoteStore : public QObject {
    Q_OBJECT
public:
    explicit NoteStore(QObject *parent = nullptr);
    QString root() const { return m_root; }
    bool available() const;
    void setRoot(const QString &path);
    bool contains(const QString &path) const;
    QString create(const QString &parentPath, const QString &name, bool directory = false);
    QString rename(const QString &path, const QString &name);
    bool trash(const QString &path);
    static bool validName(const QString &name);
signals:
    void changed();
    void error(const QString &message);
    void renamed(const QString &oldPath, const QString &newPath);
    void removed(const QString &path);
private:
    void watch();
    QString m_root;
    QFileSystemWatcher m_watcher;
    QTimer m_timer;
};

class NoteDocument : public QObject {
    Q_OBJECT
public:
    explicit NoteDocument(NoteStore *store, QObject *parent = nullptr);
    bool open(const QString &path);
    QString path() const { return m_path; }
    QString text() const { return m_text; }
    bool dirty() const { return m_dirty; }
    bool conflict() const { return m_conflict; }
    QString status() const { return m_status; }
    void setText(const QString &text);
    bool save();
    bool reload();
    bool overwrite();
    bool saveAs(const QString &path);
    void relocate(const QString &path);
    void checkDisk(bool force = true);
signals:
    void reloaded();
    void statusChanged();
private:
    bool read(QString &text, QByteArray &hash);
    bool write(const QString &path);
    void watch();
    void setStatus(const QString &text);
    void markConflict(const QString &message);
    NoteStore *m_store;
    QString m_path, m_text, m_status;
    QByteArray m_hash;
    QDateTime m_diskTime;
    qint64 m_diskSize = -1;
    bool m_dirty = false, m_conflict = false, m_bom = false, m_crlf = false;
    QFileSystemWatcher m_watcher;
    QTimer m_saveTimer, m_poll;
};
