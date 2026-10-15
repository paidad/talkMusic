#include "music/musiclibrary.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaMetaData>
#include <QSaveFile>
#include <QStandardPaths>

QString MusicLibrary::normalizedPath(const QString &path) {
    QFileInfo info(path);
    QString result = info.canonicalFilePath();
    if (result.isEmpty()) result = QDir::cleanPath(info.absoluteFilePath());
    return result;
}

MusicLibrary::MusicLibrary(QObject *parent, const QString &storagePath) : QObject(parent),
    m_storagePath(storagePath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/library.json" : storagePath),
    m_probe(new QMediaPlayer(this)) {
    QFile file(m_storagePath);
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error == QJsonParseError::NoError && doc.isArray()) {
                for (const auto &value : doc.array()) {
                    const auto obj = value.toObject();
                    const auto path = obj.value("path").toString();
                    if (path.isEmpty()) continue;
                    const auto normalized = normalizedPath(path);
                    if (!find(normalized)) m_songs.append({normalized, obj.value("title").toString(QFileInfo(path).completeBaseName()),
                        obj.value("artist").toString(), obj.value("favorite").toBool()});
                }
            } else QTimer::singleShot(0, this, [this] { emit storageError(tr("音乐库文件损坏，已使用空音乐库；导入后将重新保存。")); });
        } else QTimer::singleShot(0, this, [this] { emit storageError(tr("无法读取音乐库文件。")); });
    }
    m_probeTimeout.setSingleShot(true);
    m_probeTimeout.setInterval(3000);
    connect(&m_probeTimeout, &QTimer::timeout, this, &MusicLibrary::finishProbe);
    connect(m_probe, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::InvalidMedia) {
            const auto expected = m_probing;
            QTimer::singleShot(0, this, [this, expected] { if (!expected.isEmpty() && expected == m_probing) finishProbe(); });
        }
    });
    connect(m_probe, &QMediaPlayer::errorOccurred, this, [this] {
        const auto expected = m_probing;
        QTimer::singleShot(0, this, [this, expected] { if (!expected.isEmpty() && expected == m_probing) finishProbe(); });
    });
}

const Song *MusicLibrary::find(const QString &path) const {
    for (const auto &song : m_songs) {
#ifdef Q_OS_WIN
        if (song.path.compare(path, Qt::CaseInsensitive) == 0) return &song;
#else
        if (song.path == path) return &song;
#endif
    }
    return nullptr;
}

int MusicLibrary::importFiles(const QStringList &paths) {
    int count = 0;
    for (const auto &file : paths) {
        const auto path = normalizedPath(file);
        if (!QFileInfo(path).isFile() || find(path)) continue;
        m_songs.append({path, QFileInfo(path).completeBaseName(), {}, false});
        m_pending.append(path);
        ++count;
    }
    if (count) { save(); emit changed(); probeNext(); }
    return count;
}

void MusicLibrary::toggleFavorite(const QString &path) {
    for (auto &song : m_songs) if (song.path == path) {
        song.favorite = !song.favorite;
        save(); emit changed(); return;
    }
}

bool MusicLibrary::removeSong(const QString &path) {
    const auto *song = find(path);
    if (!song) return false;
    const auto storedPath = song->path;
    for (int i = 0; i < m_songs.size(); ++i) {
        if (m_songs[i].path != storedPath) continue;
        m_songs.removeAt(i);
        m_pending.removeAll(storedPath);
        // An in-flight metadata probe may finish, but updateMetadata never adds songs.
        save(); emit songRemoved(storedPath); emit changed(); return true;
    }
    return false;
}

void MusicLibrary::updateMetadata(const QString &path, const QString &title, const QString &artist) {
    for (auto &song : m_songs) if (song.path == path) {
        bool updated = false;
        if (!title.trimmed().isEmpty() && title != song.title) { song.title = title; updated = true; }
        if (!artist.trimmed().isEmpty() && artist != song.artist) { song.artist = artist; updated = true; }
        if (updated) { save(); emit changed(); }
        return;
    }
}

void MusicLibrary::probeNext() {
    if (!m_probing.isEmpty() || m_pending.isEmpty()) return;
    m_probing = m_pending.takeFirst();
    m_probeTimeout.start();
    m_probe->setSource(QUrl::fromLocalFile(m_probing));
}

void MusicLibrary::finishProbe() {
    if (m_probing.isEmpty()) return;
    m_probeTimeout.stop();
    const auto path = m_probing;
    const auto meta = m_probe->metaData();
    updateMetadata(path, meta.stringValue(QMediaMetaData::Title), meta.stringValue(QMediaMetaData::ContributingArtist));
    m_probe->setSource({});
    m_probing.clear();
    // Queue the next file after the previous backend's notifications have drained.
    QTimer::singleShot(0, this, &MusicLibrary::probeNext);
}

void MusicLibrary::save() {
    if (!QDir().mkpath(QFileInfo(m_storagePath).absolutePath())) {
        emit storageError(tr("无法创建音乐库保存目录。")); return;
    }
    QJsonArray data;
    for (const auto &song : m_songs) data.append(QJsonObject{{"path", song.path}, {"title", song.title}, {"artist", song.artist}, {"favorite", song.favorite}});
    QSaveFile file(m_storagePath);
    const auto bytes = QJsonDocument(data).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        emit storageError(tr("保存音乐库失败：%1").arg(file.errorString()));
}
