#pragma once
#include <QString>
#include <QVector>

// 时间统一使用毫秒，与 QMediaPlayer::position() 保持一致。
struct LyricLine { qint64 time; QString text; };
struct SongLyrics {
    QVector<LyricLine> timedLines;
    QString plainText;
    QString source;
    bool empty() const { return timedLines.isEmpty() && plainText.isEmpty(); }
};

// LRC 支持一行多个时间戳和全局 offset；结果按时间稳定排序。
QVector<LyricLine> parseLyrics(const QString &text);
// 优先同目录同名 LRC，无法获得有效内容时再读取内嵌歌词。
SongLyrics readSongLyrics(const QString &audioPath);
