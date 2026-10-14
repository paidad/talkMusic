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
#include <cmath>
#include "musiclibrary.h"
#include "playbackcontroller.h"
#include "demochat.h"
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
    void conversations() {
        DemoChat chat;
        chat.sendText("   "); QCOMPARE(chat.messages().size(), 1);
        chat.sendText(" hello "); QCOMPARE(chat.messages().last().text, QString("hello"));
        chat.shareSong("local.wav"); QCOMPARE(chat.messages().last().songPath, QString("local.wav"));
        chat.selectConversation("林间"); QCOMPARE(chat.messages().size(), 1);
        chat.selectConversation("unknown"); QCOMPARE(chat.activeConversation(), QString("林间"));
        chat.selectConversation("一起听 · 音乐客厅"); QCOMPARE(chat.messages().size(), 3);
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
    void interfaceAndScreenshots() {
        const auto song = writeWave(m_temp.path(), "午后 · 练习曲");
        const auto second = writeWave(m_temp.path(), "雨落在窗边");
        QFile lyricsFile(m_temp.path() + "/午后 · 练习曲.lrc"); QVERIFY(lyricsFile.open(QIODevice::WriteOnly));
        lyricsFile.write(QString("[00:00.00]把喜欢的歌，听给你\n[00:02.00]让旋律停在这个午后\n[00:04.00]也让心情慢慢靠近\n[00:06.00]听见此刻，也听见彼此\n").toUtf8()); lyricsFile.close();
        const auto storage = m_temp.path() + "/ui.json";
        { MusicLibrary library(nullptr, storage); library.importFiles({song, second}); }
        MainWindow window(nullptr, storage); window.show();
        QTest::qWait(100);
        auto *songs = window.findChild<QListWidget *>("songList"); QVERIFY(songs); QCOMPARE(songs->count(), 2);
        auto *search = window.findChild<QLineEdit *>("musicSearch"); search->setText("午后"); QCOMPARE(songs->count(), 1); search->clear();
        auto *volume = window.findChild<QSlider *>("volumeSlider"); volume->setValue(0);
        auto *play = window.findChild<QToolButton *>("playButton"); play->click();
        QTRY_COMPARE_WITH_TIMEOUT(play->toolTip(), QString("暂停"), 10000);
        window.findChild<QToolButton *>("favoriteButton")->click();
        window.findChild<QPushButton *>("favoritesFilter")->click(); QCOMPARE(songs->count(), 1);
        window.findChild<QToolButton *>("shareButton")->click();
        QVERIFY(window.findChild<QToolButton *>("songCardPlay"));
        auto *input = window.findChild<QLineEdit *>("messageInput"); input->setText("这首歌很适合一个安静的下午。");
        window.findChild<QPushButton *>("sendMessage")->click(); QVERIFY(input->text().isEmpty());
        bool messageFound = false;
        for (auto *item : window.findChildren<QLabel *>()) if (item->text() == "这首歌很适合一个安静的下午。") messageFound = true;
        QVERIFY(messageFound);
        auto *tabs = window.findChild<QTabWidget *>("libraryTabs"); tabs->setCurrentIndex(1);
        auto *friends = window.findChild<QListWidget *>("friendList"); friends->setCurrentRow(1);
        QVERIFY(!window.findChild<QToolButton *>("songCardPlay"));
        friends->setCurrentRow(0); QVERIFY(window.findChild<QToolButton *>("songCardPlay"));
        tabs->setCurrentIndex(0);
        QCOMPARE(play->toolTip(), QString("暂停")); // Switching pages does not interrupt music.
        QTest::qWait(200);
        const auto output = qEnvironmentVariable("TALKMUSIC_SCREENSHOT_DIR");
        if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(window.grab().save(output + "/talkmusic-1280.png")); }
        window.resize(960, 640); QTest::qWait(100);
        auto *splitter = window.findChild<QSplitter *>("mainSplitter"); QVERIFY(splitter->widget(1)->width() >= 350);
        QVERIFY(window.findChild<QFrame *>("playbackBar")->height() == 80);
        if (!output.isEmpty()) QVERIFY(window.grab().save(output + "/talkmusic-960.png"));
        window.findChild<QToolButton *>("toggleLeft")->click(); QVERIFY(splitter->widget(0)->isHidden());
        window.findChild<QToolButton *>("toggleRight")->click(); QVERIFY(splitter->widget(2)->isHidden());
        window.findChild<QToolButton *>("toggleLeft")->click(); QVERIFY(!splitter->widget(0)->isHidden());
        window.findChild<QPushButton *>("listenTogether")->click(); QVERIFY(window.findChild<QWidget *>("noticeBar")->isVisible());
        volume->setValue(27); window.close();
        MainWindow restored(nullptr, storage);
        QCOMPARE(restored.findChild<QSlider *>("volumeSlider")->value(), 27);
        QVERIFY(restored.findChild<QSplitter *>("mainSplitter")->widget(2)->isHidden());
        QCOMPARE(restored.findChild<QToolButton *>("playButton")->toolTip(), QString("播放"));
    }
};
QTEST_MAIN(TalkMusicTest)
#include "tst_talkmusic.moc"
