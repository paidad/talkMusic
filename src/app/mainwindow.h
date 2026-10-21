#pragma once
#include <QMainWindow>
#include "music/musiclibrary.h"
#include "music/playbackcontroller.h"

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
// 应用装配层：负责布局、导航和跨模块信号连接，具体存储与播放交给对应服务。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, const QString &libraryPath = {});
protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    // 四个界面区域分别创建；切换工作区不会重建底部播放器。
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
    // 服务由 QObject 父子关系管理；界面成员指针用于访问已加入布局的控件。
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
    // 专注模式暂存进入前的可见性和分栏尺寸，退出时恢复。
    bool m_focus = false, m_restoreLeft = false, m_restoreRight = false;
    QByteArray m_focusState;
    int m_lastSidebarIndex = 0;
    QSlider *m_progress;
    QSlider *m_volume;
    bool m_favoritesOnly = false;
    int m_leftWidth = 280;
    int m_rightWidth = 290;
};
