#pragma once
#include <QMainWindow>
#include "musiclibrary.h"
#include "playbackcontroller.h"
#include "demochat.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QScrollArea;
class QSlider;
class QSplitter;
class QToolButton;
class QVBoxLayout;
class CoverWidget;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, const QString &libraryPath = {});
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    QWidget *createPlayingPanel();
    QWidget *createChatPanel();
    QWidget *createLibraryPanel();
    QWidget *createPlaybackBar();
    void refreshLibrary();
    void refreshCurrentSong();
    void refreshLyrics();
    void refreshChat();
    void importMusic();
    void removeMusic(const QString &path);
    void sendMessage();
    void notify(const QString &message);
    void togglePanel(int index);
    MusicLibrary *m_library;
    PlaybackController *m_playback;
    DemoChat *m_chat;
    QSplitter *m_splitter;
    CoverWidget *m_cover;
    QLabel *m_songTitle;
    QLabel *m_artist;
    QLabel *m_chatTitle;
    QLabel *m_bottomTitle;
    QLabel *m_bottomArtist;
    QLabel *m_elapsed;
    QLabel *m_duration;
    QLabel *m_notice;
    QLabel *m_lyricInfo;
    QToolButton *m_favorite;
    QToolButton *m_share;
    QToolButton *m_play;
    QToolButton *m_mode;
    QToolButton *m_leftToggle;
    QToolButton *m_rightToggle;
    QLineEdit *m_search;
    QLineEdit *m_input;
    QListWidget *m_songList;
    QListWidget *m_lyricList;
    QScrollArea *m_messages;
    QSlider *m_progress;
    QSlider *m_volume;
    bool m_favoritesOnly = false;
    int m_leftWidth = 280;
    int m_rightWidth = 260;
};
