#pragma once
#include <QString>
#include <QVector>

struct LyricLine { qint64 time; QString text; };
struct SongLyrics {
    QVector<LyricLine> timedLines;
    QString plainText;
    QString source;
    bool empty() const { return timedLines.isEmpty() && plainText.isEmpty(); }
};

QVector<LyricLine> parseLyrics(const QString &text);
SongLyrics readSongLyrics(const QString &audioPath);
