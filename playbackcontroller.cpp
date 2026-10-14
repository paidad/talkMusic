#include "playbackcontroller.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMediaMetaData>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <algorithm>

QVector<LyricLine> parseLyrics(const QString &text) {
    QVector<LyricLine> lines;
    const QRegularExpression stamp(R"(\[(\d+):(\d{1,2})(?:[.:](\d{1,3}))?\])");
    const QRegularExpression offsetPattern(R"(\[offset:([+-]?\d+)\])", QRegularExpression::CaseInsensitiveOption);
    const auto offsetMatch = offsetPattern.match(text);
    const qint64 offset = offsetMatch.hasMatch() ? offsetMatch.captured(1).toLongLong() : 0;
    for (const auto &line : text.split('\n')) {
        auto matches = stamp.globalMatch(line);
        QVector<qint64> times;
        int end = 0;
        while (matches.hasNext()) {
            const auto match = matches.next();
            if (match.captured(2).toInt() >= 60) continue;
            const auto fraction = match.captured(3);
            const auto millis = fraction.isEmpty() ? 0 : fraction.leftJustified(3, '0').toInt();
            times.append(qMax<qint64>(0, (match.captured(1).toLongLong() * 60 + match.captured(2).toInt()) * 1000 + millis + offset));
            end = match.capturedEnd();
        }
        const auto content = line.mid(end).trimmed();
        if (!content.isEmpty()) for (const auto time : times) lines.append({time, content});
    }
    std::stable_sort(lines.begin(), lines.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
    return lines;
}

PlaybackController::PlaybackController(MusicLibrary *library, QObject *parent) : QObject(parent),
    m_library(library), m_player(nullptr), m_audio(new QAudioOutput(this)) {
    createPlayer();
}

void PlaybackController::createPlayer() {
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audio);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &PlaybackController::playbackStateChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &PlaybackController::durationChanged);
    connect(m_player, &QMediaPlayer::seekableChanged, this, &PlaybackController::seekableChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &PlaybackController::positionChanged);
    connect(m_player, &QMediaPlayer::metaDataChanged, this, &PlaybackController::refreshMetadata);
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &message) {
        emit error(tr("无法播放此歌曲：%1。请尝试其他文件。").arg(message));
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            const auto source = m_current;
            QTimer::singleShot(0, this, [this, source] {
                if (m_current == source && m_player->mediaStatus() == QMediaPlayer::EndOfMedia) advance(true, 1);
            });
        }
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        int index = -1;
        for (int i = 0; i < m_lyrics.size() && m_lyrics[i].time <= position; ++i) index = i;
        if (index != m_lyricIndex) { m_lyricIndex = index; emit lyricChanged(index); }
    });
}

void PlaybackController::playSong(const QString &path) {
    if (!m_library->find(path)) { emit error(tr("此歌曲不在本地音乐库中，请先导入。")); return; }
    if (!QFileInfo(path).isFile()) { emit error(tr("音乐文件已移动或删除：%1").arg(QFileInfo(path).fileName())); return; }
    m_current = path;
    m_lyrics.clear(); m_lyricIndex = -1;
    const auto info = QFileInfo(path);
    QFile file(info.dir().filePath(info.completeBaseName() + ".lrc"));
    if (file.open(QIODevice::ReadOnly)) m_lyrics = parseLyrics(QString::fromUtf8(file.readAll()));
    emit artworkChanged({});
    // Qt 6.7's FFmpeg backend can assert if setSource interrupts a pending load.
    // Dispose of that load together with its player, keeping audio settings intact.
    delete m_player;
    createPlayer();
    emit playbackStateChanged(QMediaPlayer::StoppedState);
    emit positionChanged(0); emit durationChanged(0); emit seekableChanged(false);
    m_player->setSource(QUrl::fromLocalFile(path));
    emit currentSongChanged(); emit lyricChanged(-1);
    m_player->play();
}

void PlaybackController::refreshMetadata() {
    if (m_current.isEmpty() || m_player->source() != QUrl::fromLocalFile(m_current)) return;
    const auto meta = m_player->metaData();
    m_library->updateMetadata(m_current, meta.stringValue(QMediaMetaData::Title), meta.stringValue(QMediaMetaData::ContributingArtist));
    auto artwork = meta.value(QMediaMetaData::CoverArtImage).value<QImage>();
    if (artwork.isNull()) artwork = meta.value(QMediaMetaData::ThumbnailImage).value<QImage>();
    if (!artwork.isNull()) emit artworkChanged(QPixmap::fromImage(artwork));
}

void PlaybackController::togglePlayback() {
    if (m_current.isEmpty()) {
        if (m_library->songs().isEmpty()) emit error(tr("请先从右侧导入本地音乐。"));
        else playSong(m_library->songs().first().path);
    } else if (m_player->playbackState() == QMediaPlayer::PlayingState) m_player->pause();
    else if (m_player->mediaStatus() == QMediaPlayer::InvalidMedia || m_player->mediaStatus() == QMediaPlayer::EndOfMedia) playSong(m_current);
    else m_player->play();
}

void PlaybackController::advance(bool automatic, int direction) {
    QStringList queue;
    for (const auto &song : m_library->songs()) if (QFileInfo(song.path).isFile()) queue.append(song.path);
    if (queue.isEmpty()) { emit error(tr("没有可播放的本地文件，请重新导入音乐。")); return; }
    int index = queue.indexOf(m_current);
    if (automatic && m_mode == RepeatOne && index >= 0) { playSong(m_current); return; }
    if (m_mode == Shuffle && queue.size() > 1) {
        const int old = index;
        do { index = QRandomGenerator::global()->bounded(queue.size()); } while (index == old);
    } else {
        if (automatic && index == queue.size() - 1) return; // Sequential mode stops at the end.
        index = index < 0 ? (direction > 0 ? 0 : queue.size() - 1) : (index + direction + queue.size()) % queue.size();
    }
    playSong(queue[index]);
}

void PlaybackController::next() { advance(false, 1); }
void PlaybackController::previous() { advance(false, -1); }
void PlaybackController::seek(qint64 position) { if (m_player->isSeekable()) m_player->setPosition(qBound<qint64>(0, position, m_player->duration())); }
void PlaybackController::setVolume(float volume) { m_audio->setVolume(qBound(0.0f, volume, 1.0f)); }
void PlaybackController::cycleMode() { m_mode = static_cast<Mode>((m_mode + 1) % 3); emit modeChanged(); }
