#pragma once
#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVector>
#include <QPixmap>
#include "music/musiclibrary.h"
#include "music/lyricsreader.h"

// 封装媒体后端和歌词定位，向界面发出状态信号，不直接操作主窗口控件。
class PlaybackController : public QObject {
    Q_OBJECT
public:
    // 顺序自然结束时停止；随机避免立即重复；单曲循环仅约束自动切歌。
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
    // automatic 区分自然播完与用户切歌，direction 表示向前或向后。
    void advance(bool automatic, int direction);
    void refreshMetadata();
    MusicLibrary *m_library;
    // 切歌可能重建播放器；音频输出独立保留以维持音量。
    QMediaPlayer *m_player;
    QAudioOutput *m_audio;
    QString m_current;
    QVector<LyricLine> m_lyrics;
    QString m_plainLyrics;
    QString m_lyricSource;
    // -1 表示当前位置尚未进入任何同步歌词。
    int m_lyricIndex = -1;
    Mode m_mode = Sequential;
};
