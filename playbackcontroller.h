#pragma once
#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVector>
#include <QPixmap>
#include "musiclibrary.h"
#include "lyricsreader.h"

class PlaybackController : public QObject {
    Q_OBJECT
public:
    enum Mode { Sequential, Shuffle, RepeatOne };
    explicit PlaybackController(MusicLibrary *library, QObject *parent = nullptr);
    QString currentPath() const { return m_current; }
    QMediaPlayer *player() const { return m_player; }
    const QVector<LyricLine> &lyrics() const { return m_lyrics; }
    QString plainLyrics() const { return m_plainLyrics; }
    QString lyricSource() const { return m_lyricSource; }
    int currentLyric() const { return m_lyricIndex; }
    Mode mode() const { return m_mode; }
    float volume() const { return m_audio->volume(); }
    void playSong(const QString &path);
    void togglePlayback();
    void next();
    void previous();
    void seek(qint64 position);
    void setVolume(float volume);
    void cycleMode();
    void clearCurrentSong();
signals:
    void playbackStateChanged(QMediaPlayer::PlaybackState state);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void seekableChanged(bool seekable);
    void currentSongChanged();
    void artworkChanged(const QPixmap &artwork);
    void lyricChanged(int index);
    void modeChanged();
    void error(const QString &message);
private:
    void createPlayer();
    void advance(bool automatic, int direction);
    void refreshMetadata();
    MusicLibrary *m_library;
    QMediaPlayer *m_player;
    QAudioOutput *m_audio;
    QString m_current;
    QVector<LyricLine> m_lyrics;
    QString m_plainLyrics;
    QString m_lyricSource;
    int m_lyricIndex = -1;
    Mode m_mode = Sequential;
};
