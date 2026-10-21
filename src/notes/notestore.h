#pragma once
#include <QObject>
#include <QString>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QDateTime>

// 笔记目录的文件操作入口：限制访问范围，拒绝符号链接与 Windows junction。
class NoteStore : public QObject {
    Q_OBJECT
public:
    explicit NoteStore(QObject *parent = nullptr);
    QString root() const { return m_root; }
    bool available() const;
    void setRoot(const QString &path);
    // 不只比较路径前缀，还会检查相对路径越界和中间目录链接。
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

// 单篇文档的保存权威：维护内存文本、磁盘基线、自动保存及外部修改冲突。
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
    // 普通保存先检查磁盘；冲突时返回 false，由调用方阻止丢失内容的切换。
    bool save();
    bool reload();
    // 用户明确选择覆盖后使用，不走普通保存的冲突阻断流程。
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
    // 最近成功读取或写入的字节摘要，用作外部修改比较基线。
    QByteArray m_hash;
    QDateTime m_diskTime;
    qint64 m_diskSize = -1;
    // 内存改动与外部冲突分别记录；BOM / CRLF 在写回时恢复。
    bool m_dirty = false, m_conflict = false, m_bom = false, m_crlf = false;
    QFileSystemWatcher m_watcher;
    // 输入防抖保存与监听之外的定时检查；它们仍在对象所属线程执行。
    QTimer m_saveTimer, m_poll;
};
