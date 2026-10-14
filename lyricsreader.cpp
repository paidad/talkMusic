#include "lyricsreader.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QStringDecoder>
#include <algorithm>
#include <fileref.h>
#include <tfilestream.h>
#include <tpropertymap.h>
#include <mpegfile.h>
#include <wavfile.h>
#include <aifffile.h>
#include <flacfile.h>
#include <id3v2tag.h>
#include <synchronizedlyricsframe.h>
#include <unsynchronizedlyricsframe.h>

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

namespace {
QString cleanText(QString text) {
    text.replace("\r\n", "\n"); text.replace('\r', '\n'); text.remove(QChar::ByteOrderMark); text.remove(QChar::Null);
    return text.trimmed();
}
QString fromTag(const TagLib::String &text) {
    const auto utf8 = text.to8Bit(true);
    return cleanText(QString::fromUtf8(utf8.data(), qsizetype(utf8.size())));
}
SongLyrics fromText(const QString &text, const QString &source) {
    auto cleaned = cleanText(text);
    auto lines = parseLyrics(cleaned);
    if (lines.isEmpty()) {
        cleaned.remove(QRegularExpression(R"(^\[(?:ar|ti|al|by|offset|re|ve|length):[^\r\n]*\]\s*$)", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption));
        cleaned = cleaned.trimmed();
    }
    return {lines, lines.isEmpty() ? cleaned : QString(), source};
}
TagLib::ID3v2::Tag *id3Tag(TagLib::File *file) {
    if (auto *mpeg = dynamic_cast<TagLib::MPEG::File *>(file)) return mpeg->ID3v2Tag(false);
    if (auto *wave = dynamic_cast<TagLib::RIFF::WAV::File *>(file)) return wave->ID3v2Tag();
    if (auto *aiff = dynamic_cast<TagLib::RIFF::AIFF::File *>(file)) return aiff->tag();
    if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(file)) return flac->ID3v2Tag(false);
    return nullptr;
}
SongLyrics readEmbedded(const QString &path) {
#ifdef Q_OS_WIN
    const auto filename = QDir::toNativeSeparators(path).toStdWString();
#else
    const auto filename = QFile::encodeName(path).toStdString();
#endif
    TagLib::FileStream stream(filename.c_str(), true);
    if (!stream.isOpen()) return {};
    TagLib::FileRef ref(&stream, false, TagLib::AudioProperties::Fast);
    if (ref.isNull() || !ref.file()->isValid()) return {};
    SongLyrics unsynced;
    if (auto *tag = id3Tag(ref.file())) {
        for (auto *frame : tag->frameList("SYLT")) {
            auto *synced = dynamic_cast<TagLib::ID3v2::SynchronizedLyricsFrame *>(frame);
            if (!synced || (synced->type() != TagLib::ID3v2::SynchronizedLyricsFrame::Lyrics && synced->type() != TagLib::ID3v2::SynchronizedLyricsFrame::TextTranscription)) continue;
            SongLyrics result; result.source = QStringLiteral("内嵌歌词");
            QStringList plain;
            for (const auto &entry : synced->synchedText()) {
                const auto text = fromTag(entry.text);
                if (text.isEmpty()) continue;
                plain.append(text);
                if (synced->timestampFormat() == TagLib::ID3v2::SynchronizedLyricsFrame::AbsoluteMilliseconds) result.timedLines.append({entry.time, text});
            }
            if (!result.timedLines.isEmpty()) {
                std::stable_sort(result.timedLines.begin(), result.timedLines.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
                return result;
            }
            if (unsynced.empty() && !plain.isEmpty()) unsynced = {{}, plain.join('\n'), result.source};
        }
        for (auto *frame : tag->frameList("USLT")) {
            auto *lyrics = dynamic_cast<TagLib::ID3v2::UnsynchronizedLyricsFrame *>(frame);
            if (!lyrics) continue;
            auto result = fromText(fromTag(lyrics->text()), QStringLiteral("内嵌歌词"));
            if (!result.timedLines.isEmpty()) return result;
            if (unsynced.empty() && !result.empty()) unsynced = result;
        }
    }
    const auto properties = ref.file()->properties();
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        const auto key = fromTag(it->first).toUpper().section(':', 0, 0);
        if (key != "SYNCEDLYRICS" && key != "LYRICS" && key != "UNSYNCEDLYRICS" && key != "UNSYNCHRONIZEDLYRICS" && key != "LYRIC") continue;
        for (const auto &value : it->second) {
            auto result = fromText(fromTag(value), QStringLiteral("内嵌歌词"));
            if (!result.timedLines.isEmpty()) return result;
            if (unsynced.empty() && !result.empty()) unsynced = result;
        }
    }
    return unsynced;
}
}

SongLyrics readSongLyrics(const QString &audioPath) {
    const auto info = QFileInfo(audioPath);
    QFile file(info.dir().filePath(info.completeBaseName() + ".lrc"));
    if (file.open(QIODevice::ReadOnly)) {
        const auto bytes = file.readAll();
        const auto encoding = QStringConverter::encodingForData(bytes);
        QStringDecoder decoder(encoding.value_or(QStringConverter::Utf8));
        auto result = fromText(decoder(bytes), QStringLiteral("本地 LRC"));
        if (!decoder.hasError() && !result.empty()) return result;
    }
    return readEmbedded(audioPath);
}
