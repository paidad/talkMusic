#include <QtTest>
#include <QTemporaryDir>
#include <QDataStream>
#include <QFile>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QSplitter>
#include <QTabWidget>
#include <QSlider>
#include <QSettings>
#include <QLabel>
#include <QFontDatabase>
#include <QCryptographicHash>
#include <wavfile.h>
#include <id3v2tag.h>
#include <unsynchronizedlyricsframe.h>
#include <synchronizedlyricsframe.h>
#include <fileref.h>
#include <tpropertymap.h>
#include <cmath>
#include "musiclibrary.h"
#include "playbackcontroller.h"
#include "notestore.h"
#include "noteeditor.h"
#include "notenavigator.h"
#include "markdownoutline.h"
#include "todostore.h"
#include "todowidget.h"
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QTextBlock>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QCheckBox>
#include <QSignalSpy>
#include <QImage>
#include <QProcess>
#include "mainwindow.h"

static QString writeWave(const QString &directory, const QString &name, int seconds = 12) {
    const auto path = directory + "/" + name + ".wav";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { qWarning() << "Cannot write wave" << path << file.errorString() << QDir(directory).exists(); return {}; }
    QDataStream out(&file); out.setByteOrder(QDataStream::LittleEndian);
    const quint32 size = seconds * 8000 * 2;
    out.writeRawData("RIFF", 4); out << quint32(36 + size); out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
    out.writeRawData("data", 4); out << size;
    for (quint32 i = 0; i < size / 2; ++i) out << qint16(std::sin(i * 6.2831853 * 220 / 8000) * 1000);
    return path;
}

static bool addEmbeddedLyrics(const QString &path, const QString &text, bool synced = false, TagLib::String::Type encoding = TagLib::String::UTF8) {
#ifdef Q_OS_WIN
    const auto filename = QDir::toNativeSeparators(path).toStdWString();
#else
    const auto filename = QFile::encodeName(path).toStdString();
#endif
    TagLib::RIFF::WAV::File file(filename.c_str(), false);
    if (!file.isValid()) return false;
    file.ID3v2Tag()->removeFrames("USLT"); file.ID3v2Tag()->removeFrames("SYLT");
    const auto utf8 = text.toUtf8();
    if (synced) {
        auto *frame = new TagLib::ID3v2::SynchronizedLyricsFrame(encoding);
        frame->setType(TagLib::ID3v2::SynchronizedLyricsFrame::Lyrics);
        frame->setTimestampFormat(TagLib::ID3v2::SynchronizedLyricsFrame::AbsoluteMilliseconds);
        TagLib::ID3v2::SynchronizedLyricsFrame::SynchedTextList entries;
        entries.append({0, TagLib::String(utf8.constData(), TagLib::String::UTF8)});
        entries.append({2000, TagLib::String("second line", TagLib::String::UTF8)});
        frame->setSynchedText(entries); file.ID3v2Tag()->addFrame(frame);
    } else {
        auto *frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(encoding);
        frame->setDescription("lyrics"); frame->setText(TagLib::String(utf8.constData(), TagLib::String::UTF8));
        file.ID3v2Tag()->addFrame(frame);
    }
    return file.save();
}

static QByteArray fileHash(const QString &path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

class TalkMusicTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_temp;
private slots:
    void initTestCase() {
        QVERIFY(m_temp.isValid());
        if (qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
            // The Windows offscreen plugin does not enumerate system fonts.
            QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
            QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");
            QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
            QFontDatabase::addApplicationFont("C:/Windows/Fonts/consola.ttf");
        }
        QCoreApplication::setOrganizationName("TalkMusicTests"); QCoreApplication::setApplicationName("IsolatedTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_temp.path());
    }
    void lyrics() {
        const auto lines = parseLyrics("[ar:Artist]\n[offset:-100]\n[00:02.50][00:04.005]第二句\n[00:00.2]第一句\n[00:61]invalid\nplain text");
        QCOMPARE(lines.size(), 3); QCOMPARE(lines[0].time, 100); QCOMPARE(lines[0].text, QString("第一句"));
        QCOMPARE(lines[1].time, 2400); QCOMPARE(lines[2].time, 3905);
        QVERIFY(parseLyrics("[ti:No timestamps]\njust words").isEmpty());
    }
    void embeddedLyricsAndPrecedence() {
        const auto wave = writeWave(m_temp.path(), "内嵌歌词测试");
        QVERIFY(addEmbeddedLyrics(wave, "第一句\n第二句", false, TagLib::String::UTF16));
        const auto original = fileHash(wave);
        auto result = readSongLyrics(wave);
        QCOMPARE(result.plainText, QString("第一句\n第二句")); QVERIFY(result.timedLines.isEmpty()); QCOMPARE(result.source, QString("内嵌歌词"));
        QCOMPARE(fileHash(wave), original);
        QVERIFY(addEmbeddedLyrics(wave, "[00:00.00]内嵌第一句\n[00:02.00]内嵌第二句"));
        result = readSongLyrics(wave); QCOMPARE(result.timedLines.size(), 2); QVERIFY(result.plainText.isEmpty());
        QFile sidecar(m_temp.path() + "/内嵌歌词测试.lrc"); QVERIFY(sidecar.open(QIODevice::WriteOnly));
        sidecar.write("[00:01.00]External first\n"); sidecar.close();
        result = readSongLyrics(wave); QCOMPARE(result.source, QString("本地 LRC")); QCOMPARE(result.timedLines.first().text, QString("External first"));
        QVERIFY(sidecar.open(QIODevice::WriteOnly | QIODevice::Truncate)); sidecar.write("[ti:metadata only]\n"); sidecar.close();
        result = readSongLyrics(wave); QCOMPARE(result.source, QString("内嵌歌词"));
        QVERIFY(QFile::remove(sidecar.fileName()));
        QVERIFY(addEmbeddedLyrics(wave, "同步歌词", true));
        result = readSongLyrics(wave); QCOMPARE(result.timedLines.size(), 2); QCOMPARE(result.timedLines[1].time, 2000);
        QCOMPARE(result.timedLines.first().text, QString("同步歌词"));
        const auto noLyrics = writeWave(m_temp.path(), "Without lyrics"); QVERIFY(readSongLyrics(noLyrics).empty());
        QFile broken(m_temp.path() + "/bad-tags.mp3"); QVERIFY(broken.open(QIODevice::WriteOnly)); broken.write(QByteArray::fromHex("4944330400007f7f7f7f")); broken.close();
        QVERIFY(readSongLyrics(broken.fileName()).empty());
    }
    void commonEmbeddedFormats() {
        for (const auto &name : {"lame_cbr.mp3", "no-tags.flac", "no-tags.m4a", "test.ogg"}) {
            const auto path = m_temp.path() + "/" + name;
            QVERIFY(QFile::copy(QString(TAGLIB_FIXTURE_DIR) + "/" + name, path));
            {
#ifdef Q_OS_WIN
                const auto filename = QDir::toNativeSeparators(path).toStdWString();
#else
                const auto filename = QFile::encodeName(path).toStdString();
#endif
                TagLib::FileRef file(filename.c_str(), false);
                QVERIFY(!file.isNull());
                auto properties = file.file()->properties();
                properties.replace("LYRICS", TagLib::StringList(TagLib::String("[00:01.00]test embedded\n[00:03.00]second", TagLib::String::UTF8)));
                file.file()->setProperties(properties); QVERIFY(file.save());
            }
            const auto original = fileHash(path);
            const auto result = readSongLyrics(path);
            QCOMPARE(result.timedLines.size(), 2); QCOMPARE(result.timedLines.first().text, QString("test embedded"));
            QCOMPARE(fileHash(path), original);
        }
    }
    void removalAndQueue() {
        const auto a = writeWave(m_temp.path(), "Remove A"); const auto b = writeWave(m_temp.path(), "Remove B");
        const auto storage = m_temp.path() + "/removal.json";
        MusicLibrary library(nullptr, storage); library.importFiles({a, b});
        const auto first = library.songs()[0].path; const auto second = library.songs()[1].path;
        PlaybackController playback(&library); playback.setVolume(0); playback.playSong(first);
        library.toggleFavorite(first);
        QVERIFY(library.removeSong(second)); QCOMPARE(playback.currentPath(), first);
        QVERIFY(QFile::exists(b)); QVERIFY(!library.find(second));
        playback.next(); QCOMPARE(playback.currentPath(), first);
        QVERIFY(library.removeSong(first)); QVERIFY(playback.currentPath().isEmpty());
        QCOMPARE(playback.player()->playbackState(), QMediaPlayer::StoppedState);
        QVERIFY(playback.lyrics().isEmpty()); QVERIFY(playback.plainLyrics().isEmpty());
        QVERIFY(QFile::exists(a)); QVERIFY(!library.removeSong(first));
        QTest::qWait(100); QVERIFY(library.songs().isEmpty());
        MusicLibrary restored(nullptr, storage); QVERIFY(restored.songs().isEmpty());
        QCOMPARE(library.importFiles({a}), 1); QVERIFY(!library.songs().first().favorite);
    }
    void libraryPersistence() {
        const auto wave = writeWave(m_temp.path(), "Library Song");
        const auto storage = m_temp.path() + "/persistence/library.json";
        {
            MusicLibrary library(nullptr, storage);
            QCOMPARE(library.importFiles({wave, m_temp.path() + "/./Library Song.wav", m_temp.path() + "/absent.wav"}), 1);
            QCOMPARE(library.songs().first().title, QString("Library Song"));
            library.toggleFavorite(library.songs().first().path);
            library.updateMetadata(library.songs().first().path, "New title", "An artist");
        }
        MusicLibrary restored(nullptr, storage);
        QCOMPARE(restored.songs().size(), 1); QVERIFY(restored.songs().first().favorite);
        QCOMPARE(restored.songs().first().title, QString("New title")); QCOMPARE(restored.songs().first().artist, QString("An artist"));
        restored.updateMetadata(restored.songs().first().path, {}, {});
        QCOMPARE(restored.songs().first().title, QString("New title"));
    }
    void importedMetadata() {
        const auto wave = writeWave(m_temp.path(), "Tagged Song");
        QFile file(wave); QVERIFY(file.open(QIODevice::ReadWrite));
        QByteArray info;
        QDataStream tags(&info, QIODevice::WriteOnly); tags.setByteOrder(QDataStream::LittleEndian);
        tags.writeRawData("INFO", 4);
        auto tag = [&tags](const char *key, const QByteArray &text) {
            const QByteArray data = text + '\0'; tags.writeRawData(key, 4); tags << quint32(data.size());
            tags.writeRawData(data.constData(), data.size()); if (data.size() % 2) tags << quint8(0);
        };
        tag("INAM", "Evening melody"); tag("IART", "TalkMusic artist");
        QVERIFY(file.seek(file.size()));
        QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("LIST", 4); stream << quint32(info.size()); stream.writeRawData(info.constData(), info.size());
        const auto size = file.size(); QVERIFY(file.seek(4)); stream << quint32(size - 8); file.close();
        MusicLibrary library(nullptr, m_temp.path() + "/metadata.json"); library.importFiles({wave});
        QTRY_COMPARE_WITH_TIMEOUT(library.songs().first().title, QString("Evening melody"), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(library.songs().first().artist, QString("TalkMusic artist"), 10000);
    }
    void playbackAndFailures() {
        const auto a = writeWave(m_temp.path(), "Playback A"); const auto b = writeWave(m_temp.path(), "Playback B");
        QFile lyricsFile(m_temp.path() + "/Playback A.lrc"); QVERIFY(lyricsFile.open(QIODevice::WriteOnly));
        lyricsFile.write("[00:00.00]start\n[00:02.00]next line\n"); lyricsFile.close();
        MusicLibrary library(nullptr, m_temp.path() + "/player.json"); library.importFiles({a, b});
        PlaybackController playback(&library); playback.setVolume(0);
        QSignalSpy errors(&playback, &PlaybackController::error);
        QCOMPARE(playback.player()->playbackState(), QMediaPlayer::StoppedState);
        playback.playSong(library.songs().first().path);
        QTRY_VERIFY_WITH_TIMEOUT(playback.player()->duration() >= 11000, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(playback.player()->playbackState(), QMediaPlayer::PlayingState, 10000);
        QCOMPARE(playback.lyrics().size(), 2);
        playback.togglePlayback(); QCOMPARE(playback.player()->playbackState(), QMediaPlayer::PausedState);
        playback.seek(3000); QTRY_COMPARE(playback.currentLyric(), 1);
        playback.togglePlayback(); QTRY_COMPARE(playback.player()->playbackState(), QMediaPlayer::PlayingState);
        playback.next(); QCOMPARE(playback.currentPath(), library.songs()[1].path);
        QVERIFY(playback.lyrics().isEmpty());
        playback.previous(); QCOMPARE(playback.currentPath(), library.songs()[0].path);
        playback.cycleMode(); QCOMPARE(playback.mode(), PlaybackController::Shuffle);
        playback.next(); QCOMPARE(playback.currentPath(), library.songs()[1].path);
        playback.cycleMode(); QCOMPARE(playback.mode(), PlaybackController::RepeatOne);
        playback.cycleMode(); QCOMPARE(playback.mode(), PlaybackController::Sequential);
        playback.player()->stop();
        QTRY_VERIFY_WITH_TIMEOUT(playback.player()->mediaStatus() != QMediaPlayer::LoadingMedia, 10000);
        playback.player()->setSource({});
        QTRY_VERIFY(!QFile::exists(a) || QFile::remove(a)); playback.playSong(library.songs()[0].path); QVERIFY(!errors.isEmpty());
        const int previousErrors = errors.count();
        QFile bad(m_temp.path() + "/unsupported.mp3"); QVERIFY(bad.open(QIODevice::WriteOnly)); bad.write("invalid media"); bad.close();
        library.importFiles({bad.fileName()}); playback.playSong(MusicLibrary::normalizedPath(bad.fileName()));
        QTRY_VERIFY_WITH_TIMEOUT(errors.count() > previousErrors, 10000);
    }
    void rapidSwitchAndEndOfQueue() {
        const auto a = writeWave(m_temp.path(), "Queue A", 2); const auto b = writeWave(m_temp.path(), "Queue B", 2);
        MusicLibrary library(nullptr, m_temp.path() + "/queue.json"); library.importFiles({a, b});
        PlaybackController playback(&library); playback.setVolume(0);
        for (int i = 0; i < 6; ++i) playback.playSong(library.songs()[i % 2].path);
        QTRY_VERIFY_WITH_TIMEOUT(playback.player()->duration() >= 1900, 10000);
        QCOMPARE(playback.currentPath(), library.songs()[1].path);
        playback.seek(1900);
        QTRY_COMPARE_WITH_TIMEOUT(playback.player()->mediaStatus(), QMediaPlayer::EndOfMedia, 5000);
        QCOMPARE(playback.currentPath(), library.songs()[1].path);
        playback.playSong(library.songs()[0].path);
        QTRY_VERIFY_WITH_TIMEOUT(playback.player()->duration() >= 1900, 10000);
        playback.seek(1900);
        QTRY_COMPARE_WITH_TIMEOUT(playback.currentPath(), library.songs()[1].path, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(playback.player()->duration() >= 1900, 10000);
        playback.cycleMode(); playback.cycleMode();
        QSignalSpy changed(&playback, &PlaybackController::currentSongChanged);
        playback.seek(1900);
        QTRY_VERIFY_WITH_TIMEOUT(!changed.isEmpty(), 5000);
        QCOMPARE(playback.currentPath(), library.songs()[1].path);
    }
    void outlineAndNotePersistence() {
        const auto outline = markdownHeadings("# First\n\n```cpp\n# fake\n```\n\nSame\n----\n\n## Same\n    # indented\n~~~\n## hidden\n~~~\n");
        QCOMPARE(outline.size(), 3); QCOMPARE(outline[1].sourceLine, 6); QCOMPARE(outline[2].sourceLine, 9);
        const auto root = m_temp.path() + "/notes-store"; QVERIFY(QDir().mkpath(root));
        NoteStore store; store.setRoot(root);
        const auto folder = store.create(root, "学习", true); QVERIFY(!folder.isEmpty());
        const auto note = store.create(folder, "第一篇"); QVERIFY(note.endsWith(".md"));
        QVERIFY(!store.contains(m_temp.path() + "/outside.md")); QVERIFY(store.create(root, "../escape").isEmpty());
        QVERIFY(store.create(root, "CON").isEmpty());
        NoteDocument document(&store); QVERIFY(document.open(note));
        document.setText("# 我的笔记\n\n原始 **Markdown**\n");
        QTRY_VERIFY_WITH_TIMEOUT(!document.dirty(), 3000);
        QFile file(note); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), document.text().toUtf8()); file.close();
        const auto renamed = store.rename(note, "改名.md"); QVERIFY(!renamed.isEmpty()); document.relocate(renamed);
        QVERIFY(document.save()); QCOMPARE(document.text(), QString("# 我的笔记\n\n原始 **Markdown**\n"));
        // BOM and CRLF remain stable instead of reserializing rendered Markdown.
        const auto windowsNote = store.create(root, "windows.md"); QFile windowsFile(windowsNote); QVERIFY(windowsFile.open(QIODevice::WriteOnly));
        windowsFile.write(QByteArray("\xef\xbb\xbf") + "# Windows\r\n"); windowsFile.close();
        NoteDocument windowsDocument(&store); QVERIFY(windowsDocument.open(windowsNote)); windowsDocument.setText("# Windows\nnext\n"); QVERIFY(windowsDocument.save());
        QVERIFY(windowsFile.open(QIODevice::ReadOnly)); QCOMPARE(windowsFile.readAll(), QByteArray("\xef\xbb\xbf") + "# Windows\r\nnext\r\n");
    }
    void noteConflictsAndFailures() {
        const auto root = m_temp.path() + "/conflicts"; QVERIFY(QDir().mkpath(root)); NoteStore store; store.setRoot(root);
        const auto path = store.create(root, "conflict.md"); NoteDocument document(&store); QVERIFY(document.open(path));
        document.setText("local draft");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write("disk edit"); file.close();
        document.checkDisk(); QVERIFY(document.conflict()); QVERIFY(!document.save()); QCOMPARE(document.text(), QString("local draft"));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("disk edit")); file.close();
        QVERIFY(document.reload()); QCOMPARE(document.text(), QString("disk edit")); QVERIFY(!document.conflict());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write("clean external"); file.close(); document.checkDisk();
        QCOMPARE(document.text(), QString("clean external"));
        document.setText("keep this"); QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write("other"); file.close(); document.checkDisk();
        QVERIFY(document.overwrite()); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("keep this")); file.close();
        document.setText("save elsewhere"); QVERIFY(QFile::remove(path)); document.checkDisk(); QVERIFY(document.conflict());
        const auto copy = QDir(root).filePath("copy.md"); QVERIFY(document.saveAs(copy)); QCOMPARE(document.path(), copy);
        document.setText("do not lose this"); QVERIFY(QFile::remove(copy)); QVERIFY(QDir().mkdir(copy));
        document.checkDisk(); QVERIFY(!document.overwrite()); QCOMPARE(document.text(), QString("do not lose this")); QVERIFY(document.dirty());
    }
    void todosPersistenceRecycleAndUndo() {
        const auto directory = m_temp.path() + "/todos-model"; TodoStore store(nullptr, directory);
        const auto id = store.createList("本周计划"); QVERIFY(!id.isEmpty());
        QVERIFY(store.addTask(id, "第一项", "备注")); QVERIFY(store.addTask(id, "第二项"));
        const auto first = store.find(id)->tasks[0].id; const auto second = store.find(id)->tasks[1].id;
        QVERIFY(store.completeTask(id, first, true)); QVERIFY(store.editTask(id, second, "修改标题", "新的备注"));
        QVERIFY(store.removeTask(id, first)); QVERIFY(store.canUndo(id)); QVERIFY(store.undoRemoval(id));
        QCOMPARE(store.find(id)->tasks.first().id, first); QVERIFY(store.find(id)->tasks.first().completed);
        QVERIFY(store.renameList(id, "新的计划"));
        { TodoStore restored(nullptr, directory); QCOMPARE(restored.find(id)->tasks[1].notes, QString("新的备注")); QCOMPARE(restored.find(id)->title, QString("新的计划")); }
        QVERIFY(store.trashList(id)); QVERIFY(!store.find(id)); QCOMPARE(store.recycled().size(), 1);
        QVERIFY(store.restoreList(id)); QCOMPARE(store.find(id)->tasks[1].id, second);
        QFile damaged(directory + "/" + id + ".todo.json"); QVERIFY(damaged.open(QIODevice::WriteOnly | QIODevice::Truncate)); damaged.write("{broken"); damaged.close();
        QVERIFY(!store.addTask(id, "must not overwrite")); QVERIFY(damaged.open(QIODevice::ReadOnly)); QCOMPARE(damaged.readAll(), QByteArray("{broken")); damaged.close();
        TodoStore corrupted(nullptr, directory); QVERIFY(corrupted.lists().isEmpty());
        const auto blocked = m_temp.path() + "/blocked-todos"; QFile blocker(blocked); QVERIFY(blocker.open(QIODevice::WriteOnly)); blocker.write("keep"); blocker.close();
        TodoStore unavailable(nullptr, blocked); QVERIFY(unavailable.createList("cannot write").isEmpty()); QVERIFY(unavailable.lists().isEmpty());
    }
    void noteNavigationAndLinks() {
        QSettings().clear(); const auto root = m_temp.path() + "/navigation"; const auto outside = m_temp.path() + "/outside-notes";
        QVERIFY(QDir().mkpath(root)); QVERIFY(QDir().mkpath(outside)); NoteStore store; store.setRoot(root);
        const auto folder = store.create(root, "folder", true); const auto a = store.create(folder, "match.md"); store.create(root, "other.md");
#ifdef Q_OS_WIN
        QProcess process; process.start("cmd.exe", {"/c", "mklink", "/J", QDir::toNativeSeparators(root + "/linked"), QDir::toNativeSeparators(outside)});
        QVERIFY(process.waitForFinished()); QCOMPARE(process.exitCode(), 0); QVERIFY(QFileInfo(root + "/linked").isJunction());
        QVERIFY(!store.contains(root + "/linked/unsafe.md"));
#endif
        NoteEditor editor(&store); NoteNavigator navigator(&store, &editor); navigator.show(); navigator.refresh();
        auto *tree = navigator.findChild<QTreeWidget *>("noteFileTree");
        QTreeWidgetItemIterator it(tree); bool hasLink = false; while (*it) { if ((*it)->text(0) == "linked") hasLink = true; ++it; } QVERIFY(!hasLink);
        navigator.findChild<QLineEdit *>("noteSearch")->setText("match"); QCOMPARE(tree->topLevelItemCount(), 1); QCOMPARE(tree->topLevelItem(0)->childCount(), 1);
        tree->setCurrentItem(tree->topLevelItem(0)->child(0)); QCOMPARE(editor.activePath(), a);
        const auto renamed = store.rename(folder, "renamed"); QCOMPARE(editor.activePath(), renamed + "/match.md"); QVERIFY(editor.flushAll());
        store.setRoot(root + "/missing"); QVERIFY(!store.available()); QCOMPARE(store.root(), root + "/missing");
#ifdef Q_OS_WIN
        // Remove only the junction created by this test; its target stays untouched.
        QVERIFY(QDir().rmdir(root + "/linked")); QVERIFY(QFileInfo(outside).isDir());
#endif
    }
    void noteConflictBlocksWindowClose() {
        QSettings().clear(); const auto root = m_temp.path() + "/close-guard"; QVERIFY(QDir().mkpath(root));
        QFile file(root + "/active.md"); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("original"); file.close();
        QSettings().setValue("notes/root", root); QSettings().setValue("notes/lastFile", file.fileName());
        MainWindow window(nullptr, m_temp.path() + "/close-guard-library.json"); window.show();
        auto *editor = window.findChild<NoteEditor *>(); editor->activeEditor()->insertPlainText("local draft");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write("external version"); file.close();
        QVERIFY(!window.close()); QVERIFY(window.isVisible()); QVERIFY(editor->findChild<QWidget *>("noteConflictBar")->isVisible());
        editor->findChild<QPushButton *>("reloadNote")->click(); QCOMPARE(editor->activeEditor()->toPlainText(), QString("external version"));
        QVERIFY(window.close());
    }
    void noteEditorSwitchingAndOutline() {
        QSettings().clear(); const auto root = m_temp.path() + "/editor"; QVERIFY(QDir().mkpath(root)); NoteStore store; store.setRoot(root);
        const auto a = store.create(root, "A"); const auto b = store.create(root, "B");
        NoteEditor editor(&store); editor.resize(850, 650); editor.show(); QVERIFY(editor.openFile(a));
        editor.activeEditor()->insertPlainText("# Same\n\ntext\n\n# Same\nsecond\n"); QVERIFY(editor.flushAll());
        auto *firstEditor = editor.activeEditor(); firstEditor->moveCursor(QTextCursor::End); firstEditor->insertPlainText("extra");
        QVERIFY(editor.openFile(b)); QVERIFY(editor.openFile(a)); QCOMPARE(editor.activeEditor(), firstEditor); QVERIFY(firstEditor->document()->isUndoAvailable());
        editor.setMode(1); editor.jumpToHeading(4); QCOMPARE(firstEditor->textCursor().blockNumber(), 4);
        auto *preview = firstEditor->parentWidget()->findChild<QTextBrowser *>(); QVERIFY(preview); QCOMPARE(preview->textCursor().block().text(), QString("Same"));
        editor.setMode(2); QCOMPARE(editor.mode(), 2); editor.resize(600, 650); QTest::qWait(50); QVERIFY(!editor.findChild<QPushButton *>("noteMode2")->isEnabled());
        // Relative images are resolved from the document directory; remote images stay unloaded.
        QImage image(12, 12, QImage::Format_RGB32); image.fill(Qt::red); QVERIFY(image.save(root + "/local.png"));
        firstEditor->insertPlainText("\n![local](local.png)\n![remote](https://example.invalid/image.png)\n"); QTest::qWait(300);
        const auto resource = preview->document()->resource(QTextDocument::ImageResource, QUrl("local.png")); QVERIFY(!resource.value<QImage>().isNull());
        QVERIFY(preview->document()->resource(QTextDocument::ImageResource, QUrl("https://example.invalid/image.png")).isNull());
        QVERIFY(editor.flushAll());
    }
    void interfaceAndScreenshots() {
        QSettings().clear(); const auto root = m_temp.path() + "/workspace-notes"; QVERIFY(QDir().mkpath(root + "/学习"));
        const auto note = root + "/学习/Qt 学习笔记.md"; QFile file(note); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QString("# 音乐陪伴下的学习时光\n\n让思绪安静下来，记下今天的新发现。\n\n## 今天的收获\n\n- 保留原始 Markdown\n- 文件夹与大纲各司其职\n- 写作时，音乐继续播放\n\n```cpp\n// 把灵感留在这里\nauto today = QStringLiteral(\"从容一点\");\n```\n\n## 接下来\n\n> 一份笔记，一段旋律，一个清晰的开始。\n").toUtf8()); file.close();
        QSettings().setValue("notes/root", root); QSettings().setValue("notes/lastFile", note);
        const auto song = writeWave(m_temp.path(), "午后练习曲"); const auto storage = m_temp.path() + "/new-ui.json";
        { MusicLibrary library(nullptr, storage); library.importFiles({song}); }
        MainWindow window(nullptr, storage); window.show(); QTest::qWait(100);
        auto *tabs = window.findChild<QTabWidget *>("libraryTabs"); QCOMPARE(tabs->count(), 3); QCOMPARE(tabs->tabText(1), QString("笔记")); QCOMPARE(tabs->tabText(2), QString("待办"));
        QVERIFY(!window.findChild<QListWidget *>("friendList"));
        auto *editor = window.findChild<NoteEditor *>(); QCOMPARE(editor->activePath(), note); editor->setMode(1);
        auto *volume = window.findChild<QSlider *>("volumeSlider"); volume->setValue(0);
        auto *play = window.findChild<QToolButton *>("playButton"); play->click(); QTRY_COMPARE_WITH_TIMEOUT(play->toolTip(), QString("暂停"), 10000);
        window.findChild<QToolButton *>("favoriteButton")->click(); window.findChild<QToolButton *>("insertSongButton")->click(); QVERIFY(editor->activeEditor()->toPlainText().contains("午后练习曲"));
        tabs->setCurrentIndex(0); QCOMPARE(window.findChild<QStackedWidget *>("workspaceStack")->currentWidget(), editor);
        tabs->setCurrentIndex(1); QCOMPARE(play->toolTip(), QString("暂停"));
        const auto output = qEnvironmentVariable("TALKMUSIC_SCREENSHOT_DIR");
        if (!output.isEmpty()) { QDir().mkpath(output); QTest::qWait(300); QVERIFY(window.grab().save(output + "/talkmusic-notes.png")); }
        auto *store = window.findChild<TodoStore *>(); const auto id = store->createList("本周的小目标"); QVERIFY(store->addTask(id, "完成 Qt 笔记", "整理模型与视图的理解")); QVERIFY(store->addTask(id, "听一张喜欢的专辑"));
        tabs->setCurrentIndex(2); auto *titles = window.findChild<QListWidget *>("todoListTitles"); titles->setCurrentRow(0); auto *todo = window.findChild<TodoWidget *>(); QCOMPARE(todo->activeId(), id);
        auto *add = window.findChild<QLineEdit *>("newTaskTitle"); add->setText("休息十分钟"); window.findChild<QPushButton *>("addTask")->click(); QCOMPARE(store->find(id)->tasks.size(), 3);
        auto *check = todo->findChild<QCheckBox *>("taskCheck"); QVERIFY(check); check->click(); QTRY_VERIFY(store->find(id)->tasks.first().completed);
        auto *remove = todo->findChild<QPushButton *>("removeTask"); QVERIFY(remove); remove->click(); QTRY_VERIFY(store->canUndo(id));
        window.findChild<QPushButton *>("undoTaskRemoval")->click(); QCOMPARE(store->find(id)->tasks.size(), 3);
        if (!output.isEmpty()) { QTest::qWait(100); QVERIFY(window.grab().save(output + "/talkmusic-todos.png")); }
        tabs->setCurrentIndex(0); QCOMPARE(window.findChild<QStackedWidget *>("workspaceStack")->currentWidget(), todo); QCOMPARE(play->toolTip(), QString("暂停"));
        auto *splitter = window.findChild<QSplitter *>("mainSplitter"); const auto sizes = splitter->sizes();
        window.findChild<QToolButton *>("focusMode")->click(); QVERIFY(splitter->widget(0)->isHidden()); QVERIFY(splitter->widget(2)->isHidden());
        window.findChild<QToolButton *>("focusMode")->click(); QVERIFY(!splitter->widget(0)->isHidden()); QVERIFY(!splitter->widget(2)->isHidden()); QCOMPARE(splitter->sizes(), sizes);
        window.resize(960, 640); QTest::qWait(100); QVERIFY(splitter->widget(1)->width() >= 350);
        QVERIFY(window.findChild<QWidget *>("cover")->geometry().bottom() < window.findChild<QLabel *>("playingSongTitle")->geometry().top());
        if (!output.isEmpty()) QVERIFY(window.grab().save(output + "/talkmusic-workspace-960.png"));
        volume->setValue(27); window.close(); QVERIFY(!window.isVisible());
        MainWindow restored(nullptr, storage); QCOMPARE(restored.findChild<QSlider *>("volumeSlider")->value(), 27); QCOMPARE(restored.findChild<NoteEditor *>()->activePath(), note); QCOMPARE(restored.findChild<TodoWidget *>()->activeId(), id);
        QCOMPARE(restored.findChild<QToolButton *>("playButton")->toolTip(), QString("播放"));
    }

};
QTEST_MAIN(TalkMusicTest)
#include "tst_talkmusic.moc"
