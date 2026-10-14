#pragma once
#include <QMainWindow>
#include "musiclibrary.h"
#include "playbackcontroller.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QScrollArea;
class QSlider;
class QSplitter;
class QToolButton;
class QVBoxLayout;
class CoverWidget;
class NoteStore;
class NoteEditor;
class NoteNavigator;
class TodoStore;
class TodoWidget;
class QStackedWidget;
class QTabWidget;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, const QString &libraryPath = {});
protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    QWidget *createPlayingPanel();
    QWidget *createWorkspace();
    QWidget *createLibraryPanel();
    QWidget *createPlaybackBar();
    void refreshLibrary();
    void refreshCurrentSong();
    void refreshLyrics();
    void refreshTodos();
    void newTodoList();
    void selectTodo(const QString &id);
    void showTodoRecycle();
    void importMusic();
    void removeMusic(const QString &path);
    void notify(const QString &message);
    void togglePanel(int index);
    void toggleFocus();
    MusicLibrary *m_library;
    PlaybackController *m_playback;
    NoteStore *m_notes;
    TodoStore *m_todos;
    NoteEditor *m_noteEditor = nullptr;
    NoteNavigator *m_noteNavigator;
    TodoWidget *m_todoWidget;
    QStackedWidget *m_workspace;
    QTabWidget *m_sidebarTabs;
    QListWidget *m_todoList;
    QSplitter *m_splitter;
    CoverWidget *m_cover;
    QLabel *m_songTitle;
    QLabel *m_artist;
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
    QListWidget *m_songList;
    QListWidget *m_lyricList;
    QToolButton *m_focusButton;
    bool m_focus = false, m_restoreLeft = false, m_restoreRight = false;
    QByteArray m_focusState;
    int m_lastSidebarIndex = 0;
    QSlider *m_progress;
    QSlider *m_volume;
    bool m_favoritesOnly = false;
    int m_leftWidth = 280;
    int m_rightWidth = 290;
};
