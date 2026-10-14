#pragma once
#include <QObject>
#include <QVector>
#include <QString>
#include <QMediaPlayer>
#include <QTimer>

struct Song {
    QString path;
    QString title;
    QString artist;
    bool favorite = false;
};

class MusicLibrary : public QObject {
    Q_OBJECT
public:
    explicit MusicLibrary(QObject *parent = nullptr, const QString &storagePath = {});
    const QVector<Song> &songs() const { return m_songs; }
    const Song *find(const QString &path) const;
    int importFiles(const QStringList &paths);
    void toggleFavorite(const QString &path);
    void updateMetadata(const QString &path, const QString &title, const QString &artist);
    static QString normalizedPath(const QString &path);
signals:
    void changed();
    void storageError(const QString &message);
private:
    void save();
    void probeNext();
    void finishProbe();
    QVector<Song> m_songs;
    QString m_storagePath;
    QStringList m_pending;
    QString m_probing;
    QMediaPlayer *m_probe;
    QTimer m_probeTimeout;
};
