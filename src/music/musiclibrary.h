#pragma once
#include <QObject>
#include <QVector>
#include <QString>
#include <QMediaPlayer>
#include <QTimer>

// 音乐库保存文件引用和展示信息，不复制音频；path 是规范化后的本地路径。
struct Song {
    QString path;
    QString title;
    QString artist;
    bool favorite = false;
};

// 管理导入、收藏、移除和持久化；独立探测器串行读取元数据，不占用播放中的播放器。
class MusicLibrary : public QObject {
    Q_OBJECT
public:
    explicit MusicLibrary(QObject *parent = nullptr, const QString &storagePath = {});
    const QVector<Song> &songs() const { return m_songs; }
    // 返回容器内部元素的观察指针，修改音乐库后不可继续持有旧指针。
    const Song *find(const QString &path) const;
    int importFiles(const QStringList &paths);
    void toggleFavorite(const QString &path);
    // 仅移除库记录，不删除磁盘上的音频文件。
    bool removeSong(const QString &path);
    void updateMetadata(const QString &path, const QString &title, const QString &artist);
    static QString normalizedPath(const QString &path);
signals:
    void changed();
    void songRemoved(const QString &path);
    void storageError(const QString &message);
private:
    void save();
    void probeNext();
    void finishProbe();
    QVector<Song> m_songs;
    QString m_storagePath;
    // 等待探测的路径与当前任务分开保存；超时后允许队列继续前进。
    QStringList m_pending;
    QString m_probing;
    QMediaPlayer *m_probe;
    QTimer m_probeTimeout;
};
