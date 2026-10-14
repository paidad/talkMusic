#include "mainwindow.h"
#include "uihelpers.h"
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
const QColor red("#df4657");
QLabel *label(const QString &text, const QString &role = {}, QWidget *parent = nullptr) {
    auto *widget = new QLabel(text, parent); widget->setTextFormat(Qt::PlainText); widget->setProperty("role", role); return widget;
}
QToolButton *iconButton(const QString &icon, const QString &tooltip, QWidget *parent = nullptr) {
    auto *button = new QToolButton(parent); button->setIcon(musicIcon(icon)); button->setIconSize({20, 20});
    button->setToolTip(tooltip); button->setAccessibleName(tooltip); button->setCursor(Qt::PointingHandCursor); button->setFixedSize(34, 34); return button;
}
QString clockText(qint64 millis) {
    const auto seconds = millis / 1000;
    return QString("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
}

MainWindow::MainWindow(QWidget *parent, const QString &libraryPath) : QMainWindow(parent), m_library(new MusicLibrary(this, libraryPath)),
    m_playback(new PlaybackController(m_library, this)), m_chat(new DemoChat(this)) {
    setWindowTitle("TalkMusic · 把喜欢的歌，听给你");
    setWindowIcon(musicIcon("music", red)); resize(1280, 800); setMinimumSize(960, 640);
    setStyleSheet(R"(
        QWidget { font-family: 'Microsoft YaHei UI', 'Microsoft YaHei', sans-serif; font-size: 13px; color: #34343e; }
        QMainWindow, QWidget#root { background: #fcfbf9; }
        QFrame#header, QFrame#playbackBar { background: #ffffff; }
        QFrame#header { border-bottom: 1px solid #eeebea; }
        QFrame#playbackBar { border-top: 1px solid #eeebea; }
        QWidget#playingPanel { background: #faf6f4; }
        QWidget#libraryPanel { background: #f7f7f8; }
        QWidget#chatPanel, QScrollArea, QScrollArea > QWidget > QWidget { background: #fcfbf9; }
        QLabel { background: transparent; }
        QLabel[role='brand'] { font-size: 21px; font-weight: 700; letter-spacing: 1px; }
        QLabel[role='heading'] { font-size: 19px; font-weight: 600; }
        QLabel[role='songTitle'] { font-size: 20px; font-weight: 600; }
        QLabel[role='muted'] { color: #92919b; font-size: 12px; }
        QLabel[role='small'] { color: #92919b; font-size: 11px; }
        QLabel[role='eyebrow'] { color: #ab9290; font-size: 11px; letter-spacing: 2px; }
        QLabel[role='badge'] { color: #cb5966; background: #fbeaed; border-radius: 8px; padding: 4px 9px; font-size: 11px; }
        QLabel#notice { color: #b44854; background: #fff0ee; padding: 9px 20px; }
        QToolButton { border: none; border-radius: 8px; background: transparent; }
        QToolButton:hover { background: #f0e8e7; }
        QToolButton:disabled { background: transparent; }
        QToolButton#playButton { background: #df4657; border-radius: 22px; }
        QToolButton#playButton:hover { background: #cd3548; }
        QToolButton#playButton:pressed { background: #b92e41; }
        QPushButton { border: 1px solid #e8e4e4; border-radius: 8px; background: #ffffff; padding: 7px 13px; }
        QPushButton:hover { background: #f9eeee; border-color: #ecc4ca; }
        QPushButton[accent='true'] { background: #df4657; color: white; border: none; }
        QPushButton[accent='true']:hover { background: #cd3548; }
        QPushButton:checked { color: #d94759; background: #fbeaed; border-color: #fbeaed; }
        QLineEdit { border: 1px solid #e9e5e4; border-radius: 9px; padding: 9px 12px; background: white; selection-background-color: #df4657; }
        QLineEdit:focus { border-color: #e29aa5; }
        QListWidget { background: transparent; border: none; outline: none; }
        QListWidget::item { border-radius: 8px; padding: 5px; }
        QListWidget::item:hover { background: #f0ebec; }
        QListWidget::item:selected { background: #fbeaed; color: #ce3d51; }
        QListWidget#lyrics::item { padding: 10px 0; color: #9d9195; }
        QListWidget#lyrics::item:selected { color: #d94759; background: transparent; font-weight: 600; }
        QListWidget#lyrics::item:hover { background: transparent; }
        QTabWidget::pane { border: none; }
        QTabBar::tab { padding: 10px 18px; color: #94929b; border-bottom: 2px solid transparent; }
        QTabBar::tab:selected { color: #d94759; border-bottom: 2px solid #df4657; font-weight: 600; }
        QSlider::groove:horizontal { height: 4px; background: #eeeaeb; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #df4657; border-radius: 2px; }
        QSlider::handle:horizontal { background: #df4657; border: 2px solid white; width: 10px; height: 10px; margin: -5px 0; border-radius: 7px; }
        QScrollBar:vertical { width: 5px; background: transparent; margin: 0; }
        QScrollBar::handle:vertical { background: #ddd6d8; border-radius: 2px; min-height: 24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
        QSplitter::handle { background: #eee9e8; }
        QSplitter::handle:hover { background: #e9b4bd; }
        QFrame[message='incoming'] { background: white; border: 1px solid #eee8e6; border-radius: 12px; }
        QFrame[message='outgoing'] { background: #fbeaed; border: 1px solid #f6dce1; border-radius: 12px; }
        QFrame#songCard { background: white; border: 1px solid #eee5e6; border-radius: 10px; }
        QMenu { background: white; border: 1px solid #e8e4e4; padding: 6px; }
        QMenu::item { padding: 7px 18px; }
        QMenu::item:selected { background: #fbeaed; color: #ce3d51; }
    )");
    auto *root = new QWidget; root->setObjectName("root"); setCentralWidget(root);
    auto *layout = new QVBoxLayout(root); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto *header = new QFrame; header->setObjectName("header"); header->setFixedHeight(64);
    auto *top = new QHBoxLayout(header); top->setContentsMargins(22, 0, 22, 0); top->setSpacing(12);
    auto *logo = label({}); logo->setPixmap(musicIcon("music", red).pixmap(26, 26)); logo->setFixedSize(26, 30);
    top->addWidget(logo); top->addWidget(label("TalkMusic", "brand")); top->addSpacing(12);
    top->addWidget(label("把喜欢的歌，听给你", "muted")); top->addStretch();
    auto *badge = label("本地音乐 · 演示社交", "badge"); badge->setFixedHeight(28); top->addWidget(badge);
    m_leftToggle = iconButton("left", "折叠 / 展开正在播放"); m_leftToggle->setObjectName("toggleLeft");
    m_rightToggle = iconButton("right", "折叠 / 展开音乐与好友"); m_rightToggle->setObjectName("toggleRight");
    top->addWidget(m_leftToggle); top->addWidget(m_rightToggle); layout->addWidget(header);
    auto *noticeBar = new QWidget; noticeBar->setObjectName("noticeBar");
    auto *noticeLayout = new QHBoxLayout(noticeBar); noticeLayout->setContentsMargins(0, 0, 8, 0);
    m_notice = label({}); m_notice->setObjectName("notice"); m_notice->setWordWrap(true); noticeLayout->addWidget(m_notice, 1);
    auto *dismiss = new QPushButton("关闭提示"); noticeLayout->addWidget(dismiss);
    connect(dismiss, &QPushButton::clicked, noticeBar, &QWidget::hide);
    noticeBar->hide(); layout->addWidget(noticeBar);
    m_splitter = new QSplitter(Qt::Horizontal); m_splitter->setObjectName("mainSplitter"); m_splitter->setHandleWidth(1);
    m_splitter->addWidget(createPlayingPanel()); m_splitter->addWidget(createChatPanel()); m_splitter->addWidget(createLibraryPanel());
    m_splitter->setChildrenCollapsible(false); m_splitter->setStretchFactor(0, 0); m_splitter->setStretchFactor(1, 1); m_splitter->setStretchFactor(2, 0);
    layout->addWidget(m_splitter, 1); layout->addWidget(createPlaybackBar());
    connect(m_leftToggle, &QToolButton::clicked, this, [this] { togglePanel(0); });
    connect(m_rightToggle, &QToolButton::clicked, this, [this] { togglePanel(2); });
    connect(m_library, &MusicLibrary::changed, this, [this] { refreshLibrary(); refreshCurrentSong(); refreshChat(); });
    connect(m_library, &MusicLibrary::storageError, this, &MainWindow::notify);
    connect(m_playback, &PlaybackController::error, this, &MainWindow::notify);
    connect(m_playback, &PlaybackController::currentSongChanged, this, [this] { refreshCurrentSong(); refreshLyrics(); refreshLibrary(); });
    connect(m_playback, &PlaybackController::artworkChanged, m_cover, &CoverWidget::setArtwork);
    connect(m_playback, &PlaybackController::lyricChanged, this, [this](int index) {
        m_lyricList->setCurrentRow(index);
        if (index >= 0) m_lyricList->scrollToItem(m_lyricList->item(index), QAbstractItemView::PositionAtCenter);
    });
    connect(m_chat, &DemoChat::changed, this, &MainWindow::refreshChat);
    connect(m_playback, &PlaybackController::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        const bool playing = state == QMediaPlayer::PlayingState;
        m_play->setIcon(musicIcon(playing ? "pause" : "play", Qt::white));
        m_play->setToolTip(playing ? "暂停" : "播放"); m_play->setAccessibleName(m_play->toolTip());
    });
    connect(m_playback, &PlaybackController::durationChanged, this, [this](qint64 duration) { m_duration->setText(clockText(duration)); });
    connect(m_playback, &PlaybackController::seekableChanged, m_progress, &QSlider::setEnabled);
    connect(m_playback, &PlaybackController::positionChanged, this, [this](qint64 position) {
        m_elapsed->setText(clockText(position));
        if (!m_progress->isSliderDown()) m_progress->setValue(m_playback->player()->duration() > 0 ? int(double(position) / m_playback->player()->duration() * 10000) : 0);
    });
    QSettings settings;
    m_volume->setValue(qBound(0, settings.value("volume", 65).toInt(), 100));
    if (settings.contains("geometry")) restoreGeometry(settings.value("geometry").toByteArray());
    m_splitter->setSizes({280, 738, 260});
    if (settings.contains("splitter")) m_splitter->restoreState(settings.value("splitter").toByteArray());
    if (settings.value("leftHidden", false).toBool()) togglePanel(0);
    if (settings.value("rightHidden", false).toBool()) togglePanel(2);
    refreshLibrary(); refreshCurrentSong(); refreshLyrics(); refreshChat();
}

QWidget *MainWindow::createPlayingPanel() {
    auto *panel = new QWidget; panel->setObjectName("playingPanel"); panel->setMinimumWidth(220);
    auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(24, 25, 24, 18); layout->setSpacing(12);
    layout->addWidget(label("NOW PLAYING", "eyebrow"));
    m_cover = new CoverWidget; m_cover->setObjectName("cover"); m_cover->setFixedHeight(232); layout->addWidget(m_cover);
    m_songTitle = label("让音乐开始", "songTitle"); m_songTitle->setWordWrap(true); m_songTitle->setMaximumHeight(65); layout->addWidget(m_songTitle);
    m_artist = label("选择一首歌，开启今天的心情", "muted"); m_artist->setWordWrap(true); layout->addWidget(m_artist);
    auto *actions = new QHBoxLayout;
    m_favorite = iconButton("heart", "收藏当前歌曲"); m_favorite->setObjectName("favoriteButton");
    m_share = iconButton("share", "分享到当前演示会话"); m_share->setObjectName("shareButton");
    actions->addWidget(m_favorite); actions->addWidget(m_share); actions->addStretch(); actions->addWidget(label("本地音源", "small")); layout->addLayout(actions);
    connect(m_favorite, &QToolButton::clicked, this, [this] { m_library->toggleFavorite(m_playback->currentPath()); });
    connect(m_share, &QToolButton::clicked, this, [this] { m_chat->shareSong(m_playback->currentPath()); });
    m_lyricList = new QListWidget; m_lyricList->setObjectName("lyrics"); m_lyricList->setFocusPolicy(Qt::NoFocus);
    m_lyricList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); layout->addWidget(m_lyricList, 1);
    connect(m_lyricList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const int index = m_lyricList->row(item);
        if (index < m_playback->lyrics().size()) m_playback->seek(m_playback->lyrics()[index].time);
    });
    m_lyricInfo = label({}, "small"); m_lyricInfo->setObjectName("lyricSource"); m_lyricInfo->setWordWrap(true);
    m_lyricInfo->setAlignment(Qt::AlignCenter); layout->addWidget(m_lyricInfo);
    layout->addWidget(label("听见此刻，也听见彼此。", "small"), 0, Qt::AlignHCenter);
    return panel;
}

QWidget *MainWindow::createChatPanel() {
    auto *panel = new QWidget; panel->setObjectName("chatPanel"); panel->setMinimumWidth(350);
    auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(24, 24, 24, 20); layout->setSpacing(14);
    auto *heading = new QHBoxLayout;
    auto *titles = new QVBoxLayout; titles->setSpacing(5);
    m_chatTitle = label({}, "heading"); titles->addWidget(m_chatTitle); titles->addWidget(label("分享旋律，让聊天多一点共鸣", "muted"));
    heading->addLayout(titles, 1);
    auto *listen = new QPushButton("一起听"); listen->setIcon(musicIcon("music", red)); listen->setObjectName("listenTogether"); heading->addWidget(listen);
    connect(listen, &QPushButton::clicked, this, [this] { notify("一起听尚未接入服务端。当前音乐仅在本机播放，右侧好友和消息均为演示。"); });
    layout->addLayout(heading);
    auto *banner = label("演示会话 · 消息仅在本次运行保留，好友不会收到", "badge"); banner->setWordWrap(true); layout->addWidget(banner);
    m_messages = new QScrollArea; m_messages->setObjectName("chatMessages"); m_messages->setFrameShape(QFrame::NoFrame); m_messages->setWidgetResizable(true);
    m_messages->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); layout->addWidget(m_messages, 1);
    auto *composer = new QHBoxLayout; composer->setSpacing(10);
    m_input = new QLineEdit; m_input->setObjectName("messageInput"); m_input->setPlaceholderText("聊聊这首歌，或分享今天的心情…"); m_input->setMaxLength(2000); m_input->setMinimumWidth(0);
    auto *send = new QPushButton("发送"); send->setObjectName("sendMessage"); send->setProperty("accent", true); send->setFixedWidth(64);
    composer->addWidget(m_input, 1); composer->addWidget(send); layout->addLayout(composer);
    layout->addWidget(label("Enter 发送  ·  左侧分享按钮可发送歌曲卡片", "small"));
    connect(send, &QPushButton::clicked, this, &MainWindow::sendMessage);
    connect(m_input, &QLineEdit::returnPressed, this, &MainWindow::sendMessage);
    return panel;
}

QWidget *MainWindow::createLibraryPanel() {
    auto *panel = new QWidget; panel->setObjectName("libraryPanel"); panel->setMinimumWidth(230);
    auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(14, 18, 14, 16);
    auto *tabs = new QTabWidget; tabs->setObjectName("libraryTabs");
    auto *music = new QWidget; auto *musicLayout = new QVBoxLayout(music); musicLayout->setContentsMargins(0, 16, 0, 0); musicLayout->setSpacing(12);
    m_search = new QLineEdit; m_search->setObjectName("musicSearch"); m_search->setPlaceholderText("搜索本地歌曲 / 歌手"); m_search->setClearButtonEnabled(true); musicLayout->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::refreshLibrary);
    auto *import = new QPushButton("导入本地音乐"); import->setObjectName("importMusic"); import->setIcon(musicIcon("import", red)); musicLayout->addWidget(import);
    connect(import, &QPushButton::clicked, this, &MainWindow::importMusic);
    auto *filters = new QHBoxLayout; filters->setSpacing(6);
    auto *all = new QPushButton("本地音乐"); auto *favorites = new QPushButton("收藏"); favorites->setObjectName("favoritesFilter");
    all->setCheckable(true); all->setChecked(true); favorites->setCheckable(true);
    filters->addWidget(all); filters->addWidget(favorites); musicLayout->addLayout(filters);
    connect(all, &QPushButton::clicked, this, [this, all, favorites] { m_favoritesOnly = false; all->setChecked(true); favorites->setChecked(false); refreshLibrary(); });
    connect(favorites, &QPushButton::clicked, this, [this, all, favorites] { m_favoritesOnly = true; all->setChecked(false); favorites->setChecked(true); refreshLibrary(); });
    m_songList = new QListWidget; m_songList->setObjectName("songList"); m_songList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); musicLayout->addWidget(m_songList, 1);
    connect(m_songList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) { m_playback->playSong(item->data(Qt::UserRole).toString()); });
    m_songList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_songList, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        const auto *item = m_songList->itemAt(position);
        if (!item || item->data(Qt::UserRole).toString().isEmpty()) return;
        const auto path = item->data(Qt::UserRole).toString();
        QMenu menu;
        auto *remove = menu.addAction(musicIcon("remove"), "从音乐库移除（保留文件）");
        if (menu.exec(m_songList->viewport()->mapToGlobal(position)) == remove) removeMusic(path);
    });
    musicLayout->addWidget(label("双击播放 · 歌曲卡片共享本地文件", "small"));
    tabs->addTab(music, "音乐");
    auto *friends = new QWidget; auto *friendsLayout = new QVBoxLayout(friends); friendsLayout->setContentsMargins(0, 18, 0, 0); friendsLayout->setSpacing(12);
    friendsLayout->addWidget(label("演示好友", "eyebrow"));
    auto *friendList = new QListWidget; friendList->setObjectName("friendList");
    for (const auto &name : m_chat->conversations()) {
        auto *item = new QListWidgetItem(friendList); item->setData(Qt::UserRole, name); item->setSizeHint({180, 72});
        auto *row = new QWidget; auto *rowLayout = new QHBoxLayout(row); rowLayout->setContentsMargins(4, 6, 4, 6);
        auto *avatar = label(name.startsWith("一起听") ? "♫" : name.left(1)); avatar->setFixedSize(38, 38); avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet("background: #ecdfdf; color: #a45e69; border-radius: 19px; font-size: 17px;"); rowLayout->addWidget(avatar);
        auto *text = new QVBoxLayout; text->setSpacing(4); text->addWidget(label(name)); text->addWidget(label(name.startsWith("一起听") ? "音乐客厅 · 演示房间" : "演示在线 · 尚未连接", "small")); rowLayout->addLayout(text, 1);
        friendList->setItemWidget(item, row);
    }
    friendList->setCurrentRow(0);
    connect(friendList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) { if (item) m_chat->selectConversation(item->data(Qt::UserRole).toString()); });
    friendsLayout->addWidget(friendList, 1);
    auto *explanation = label("这是本地交互演示。好友状态为示例，歌曲只在你的电脑上播放。", "muted"); explanation->setWordWrap(true); friendsLayout->addWidget(explanation);
    tabs->addTab(friends, "好友"); layout->addWidget(tabs); return panel;
}

QWidget *MainWindow::createPlaybackBar() {
    auto *bar = new QFrame; bar->setObjectName("playbackBar"); bar->setFixedHeight(80);
    auto *layout = new QHBoxLayout(bar); layout->setContentsMargins(24, 8, 20, 8); layout->setSpacing(12);
    auto *summary = new QWidget; summary->setFixedWidth(170); auto *summaryLayout = new QVBoxLayout(summary); summaryLayout->setContentsMargins(0, 0, 0, 0); summaryLayout->setSpacing(4);
    m_bottomTitle = label("尚未播放"); m_bottomTitle->setObjectName("bottomTitle"); m_bottomTitle->setStyleSheet("font-weight: 600;");
    m_bottomArtist = label("导入你喜欢的音乐", "small"); summaryLayout->addWidget(m_bottomTitle); summaryLayout->addWidget(m_bottomArtist); layout->addWidget(summary);
    auto *previous = iconButton("previous", "上一首"); previous->setObjectName("previousButton");
    m_play = iconButton("play", "播放"); m_play->setObjectName("playButton"); m_play->setFixedSize(44, 44); m_play->setIcon(musicIcon("play", Qt::white));
    auto *next = iconButton("next", "下一首"); next->setObjectName("nextButton");
    layout->addWidget(previous); layout->addWidget(m_play); layout->addWidget(next);
    connect(previous, &QToolButton::clicked, m_playback, &PlaybackController::previous);
    connect(m_play, &QToolButton::clicked, m_playback, &PlaybackController::togglePlayback);
    connect(next, &QToolButton::clicked, m_playback, &PlaybackController::next);
    m_elapsed = label("00:00", "small"); m_elapsed->setFixedWidth(38); layout->addWidget(m_elapsed);
    m_progress = new QSlider(Qt::Horizontal); m_progress->setObjectName("playbackProgress"); m_progress->setRange(0, 10000); m_progress->setMinimumWidth(100); m_progress->setEnabled(false); layout->addWidget(m_progress, 1);
    auto seek = [this] { m_playback->seek(qint64(double(m_progress->value()) / 10000 * m_playback->player()->duration())); };
    connect(m_progress, &QSlider::sliderReleased, this, seek);
    connect(m_progress, &QSlider::actionTriggered, this, [this](int action) {
        if (action != QAbstractSlider::SliderMove) m_playback->seek(qint64(double(m_progress->sliderPosition()) / 10000 * m_playback->player()->duration()));
    });
    m_duration = label("00:00", "small"); m_duration->setFixedWidth(38); layout->addWidget(m_duration);
    m_mode = iconButton("queue", "顺序播放"); m_mode->setObjectName("playbackMode"); layout->addWidget(m_mode);
    connect(m_mode, &QToolButton::clicked, m_playback, &PlaybackController::cycleMode);
    connect(m_playback, &PlaybackController::modeChanged, this, [this] {
        const auto mode = m_playback->mode();
        m_mode->setIcon(musicIcon(mode == PlaybackController::Shuffle ? "shuffle" : mode == PlaybackController::RepeatOne ? "repeat" : "queue", mode == PlaybackController::Sequential ? QColor("#777982") : red));
        m_mode->setToolTip(mode == PlaybackController::Shuffle ? "随机播放" : mode == PlaybackController::RepeatOne ? "单曲循环" : "顺序播放");
        m_mode->setAccessibleName(m_mode->toolTip());
    });
    auto *volumeIcon = label({}); volumeIcon->setPixmap(musicIcon("volume").pixmap(20, 20)); layout->addWidget(volumeIcon);
    m_volume = new QSlider(Qt::Horizontal); m_volume->setObjectName("volumeSlider"); m_volume->setRange(0, 100); m_volume->setFixedWidth(70); m_volume->setToolTip("音量"); layout->addWidget(m_volume);
    connect(m_volume, &QSlider::valueChanged, this, [this](int volume) { m_playback->setVolume(volume / 100.0f); m_volume->setToolTip(QString("音量 %1%").arg(volume)); });
    auto *queue = iconButton("queue", "播放队列"); queue->setObjectName("queueButton"); layout->addWidget(queue);
    connect(queue, &QToolButton::clicked, this, [this, queue] {
        QMenu menu;
        menu.addSection("播放队列 · 本地音乐");
        for (const auto &song : m_library->songs()) {
            auto *action = menu.addAction(song.path == m_playback->currentPath() ? "♪  " + song.title : song.title);
            const auto path = song.path; connect(action, &QAction::triggered, this, [this, path] { m_playback->playSong(path); });
        }
        if (m_library->songs().isEmpty()) menu.addAction("请先导入音乐")->setEnabled(false);
        menu.exec(queue->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
    });
    return bar;
}

void MainWindow::refreshLibrary() {
    m_songList->clear();
    const auto query = m_search->text().trimmed();
    for (const auto &song : m_library->songs()) {
        if (m_favoritesOnly && !song.favorite) continue;
        if (!query.isEmpty() && !song.title.contains(query, Qt::CaseInsensitive) && !song.artist.contains(query, Qt::CaseInsensitive)) continue;
        auto *item = new QListWidgetItem(m_songList); item->setData(Qt::UserRole, song.path); item->setSizeHint({180, 65}); item->setToolTip(song.path);
        auto *row = new QWidget; auto *rowLayout = new QHBoxLayout(row); rowLayout->setContentsMargins(3, 4, 3, 4); rowLayout->setSpacing(7);
        auto *icon = label({}); icon->setPixmap(musicIcon("music", song.path == m_playback->currentPath() ? red : QColor("#b4a3a8")).pixmap(23, 23)); rowLayout->addWidget(icon);
        auto *text = new QVBoxLayout; text->setSpacing(4);
        auto *title = new ElidedLabel(song.title);
        text->addWidget(title); text->addWidget(label(!QFileInfo::exists(song.path) ? "文件缺失" : song.artist.isEmpty() ? "未知歌手 · 本地音乐" : song.artist, "small")); rowLayout->addLayout(text, 1);
        if (song.favorite) { auto *heart = label({}); heart->setPixmap(musicIcon("heartFilled", red).pixmap(14, 14)); rowLayout->addWidget(heart); }
        for (auto *child : row->findChildren<QLabel *>()) child->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *remove = iconButton("remove", "从音乐库移除（保留文件）"); remove->setObjectName("removeSongButton"); remove->setFixedSize(24, 26); remove->setIconSize({15, 15}); rowLayout->addWidget(remove);
        const auto path = song.path;
        connect(remove, &QToolButton::clicked, this, [this, path] { QTimer::singleShot(0, this, [this, path] { removeMusic(path); }); });
        m_songList->setItemWidget(item, row);
        if (song.path == m_playback->currentPath()) m_songList->setCurrentItem(item);
    }
    if (!m_songList->count()) {
        auto *item = new QListWidgetItem(m_library->songs().isEmpty() ? "还没有音乐\n\n点击上方按钮导入你的歌曲" : "没有符合条件的歌曲", m_songList);
        item->setFlags(Qt::NoItemFlags); item->setTextAlignment(Qt::AlignCenter); item->setSizeHint({180, 140});
    }
}

void MainWindow::refreshCurrentSong() {
    const auto *song = m_library->find(m_playback->currentPath());
    m_favorite->setEnabled(song); m_share->setEnabled(song);
    if (!song) {
        m_songTitle->setText("让音乐开始"); m_songTitle->setToolTip({});
        m_artist->setText("选择一首歌，开启今天的心情");
        m_bottomTitle->setText("尚未播放"); m_bottomTitle->setToolTip({}); m_bottomArtist->setText("导入你喜欢的音乐");
        m_favorite->setIcon(musicIcon("heart")); return;
    }
    m_songTitle->setText(song->title); m_songTitle->setToolTip(song->title);
    m_artist->setText(song->artist.isEmpty() ? "未知歌手 · 本地音乐" : song->artist);
    m_bottomTitle->setText(QFontMetrics(m_bottomTitle->font()).elidedText(song->title, Qt::ElideRight, 168)); m_bottomTitle->setToolTip(song->title);
    m_bottomArtist->setText(QFontMetrics(m_bottomArtist->font()).elidedText(m_artist->text(), Qt::ElideRight, 168));
    m_favorite->setIcon(musicIcon(song->favorite ? "heartFilled" : "heart", song->favorite ? red : QColor("#777982")));
    m_favorite->setToolTip(song->favorite ? "取消收藏" : "收藏当前歌曲");
}

void MainWindow::refreshLyrics() {
    m_lyricList->clear();
    const bool synced = !m_playback->lyrics().isEmpty();
    m_lyricList->setWordWrap(true);
    m_lyricList->setSelectionMode(synced ? QAbstractItemView::SingleSelection : QAbstractItemView::NoSelection);
    m_lyricInfo->setText(m_playback->lyricSource().isEmpty() ? QString() : m_playback->lyricSource() + (synced ? " · 同步歌词" : " · 纯文本，无时间戳"));
    for (const auto &line : m_playback->lyrics()) {
        auto *item = new QListWidgetItem(line.text, m_lyricList); item->setTextAlignment(Qt::AlignCenter); item->setToolTip(line.text);
    }
    if (!synced && !m_playback->plainLyrics().isEmpty()) {
        for (const auto &line : m_playback->plainLyrics().split('\n')) {
            auto *item = new QListWidgetItem(line, m_lyricList); item->setTextAlignment(Qt::AlignCenter);
            item->setToolTip(line); item->setFlags(Qt::ItemIsEnabled);
        }
    } else if (!synced) {
        auto *item = new QListWidgetItem("暂无歌词", m_lyricList); item->setTextAlignment(Qt::AlignCenter); item->setFlags(Qt::NoItemFlags);
    }
}

void MainWindow::refreshChat() {
    const bool nearBottom = m_messages->verticalScrollBar()->maximum() - m_messages->verticalScrollBar()->value() < 40;
    const int previousScroll = m_messages->verticalScrollBar()->value();
    m_chatTitle->setText(m_chat->activeConversation());
    auto *container = new QWidget; auto *layout = new QVBoxLayout(container); layout->setContentsMargins(0, 10, 0, 10); layout->setSpacing(20);
    auto *day = label("今天 · 一段从音乐开始的对话", "small"); day->setAlignment(Qt::AlignCenter); layout->addWidget(day);
    for (const auto &message : m_chat->messages()) {
        auto *row = new QWidget; auto *rowLayout = new QHBoxLayout(row); rowLayout->setContentsMargins(0, 0, 0, 0); rowLayout->setSpacing(10);
        auto *avatar = label(message.sender.left(1)); avatar->setFixedSize(32, 32); avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(message.outgoing ? "background: #df4657; color: white; border-radius: 16px;" : "background: #e6dfd8; color: #897064; border-radius: 16px;");
        auto *column = new QWidget; auto *columnLayout = new QVBoxLayout(column); columnLayout->setContentsMargins(0, 0, 0, 0); columnLayout->setSpacing(6);
        auto *sender = label(message.sender + "  " + message.time.toString("HH:mm"), "small"); sender->setAlignment(message.outgoing ? Qt::AlignRight : Qt::AlignLeft); columnLayout->addWidget(sender);
        if (message.songPath.isEmpty()) {
            auto *bubble = new QFrame; bubble->setProperty("message", message.outgoing ? "outgoing" : "incoming");
            auto *bubbleLayout = new QVBoxLayout(bubble); bubbleLayout->setContentsMargins(14, 11, 14, 11);
            auto *text = label(message.text); text->setTextFormat(Qt::PlainText); text->setWordWrap(true); text->setTextInteractionFlags(Qt::TextSelectableByMouse); text->setMinimumWidth(0); bubbleLayout->addWidget(text); columnLayout->addWidget(bubble);
        } else {
            const auto *song = m_library->find(message.songPath);
            auto *card = new QFrame; card->setObjectName("songCard"); auto *cardLayout = new QHBoxLayout(card); cardLayout->setContentsMargins(12, 12, 12, 12); cardLayout->setSpacing(10);
            auto *cover = new CoverWidget; cover->setFixedSize(54, 54); cover->setMinimumSize(54, 54); cardLayout->addWidget(cover);
            auto *info = new QVBoxLayout; info->setSpacing(4); auto *title = label(song ? song->title : QFileInfo(message.songPath).completeBaseName()); title->setWordWrap(true); info->addWidget(title);
            info->addWidget(label(song && !song->artist.isEmpty() ? song->artist : "本地歌曲", "small")); info->addWidget(label(song ? "歌曲分享 · 本机播放" : "已从音乐库移除", "small")); cardLayout->addLayout(info, 1);
            auto *play = iconButton("play", "播放分享的本地歌曲"); play->setObjectName("songCardPlay"); play->setIcon(musicIcon("play", red)); cardLayout->addWidget(play);
            const auto path = message.songPath; connect(play, &QToolButton::clicked, this, [this, path] { m_playback->playSong(path); }); columnLayout->addWidget(card);
            play->setEnabled(song != nullptr);
        }
        column->setMaximumWidth(420);
        if (message.outgoing) { rowLayout->addStretch(1); rowLayout->addWidget(column, 4); rowLayout->addWidget(avatar, 0, Qt::AlignTop); }
        else { rowLayout->addWidget(avatar, 0, Qt::AlignTop); rowLayout->addWidget(column, 4); rowLayout->addStretch(1); }
        layout->addWidget(row);
    }
    layout->addStretch();
    auto *old = m_messages->takeWidget(); delete old; m_messages->setWidget(container);
    QTimer::singleShot(0, this, [this, nearBottom, previousScroll] { m_messages->verticalScrollBar()->setValue(nearBottom ? m_messages->verticalScrollBar()->maximum() : previousScroll); });
}

void MainWindow::importMusic() {
    const auto files = QFileDialog::getOpenFileNames(this, "导入本地音乐", {}, "音频文件 (*.mp3 *.wav *.flac *.m4a *.aac *.ogg *.opus *.wma);;所有文件 (*)");
    if (files.isEmpty()) return;
    const int added = m_library->importFiles(files);
    notify(added ? QString("已导入 %1 首歌曲。双击右侧歌曲开始播放。").arg(added) : "没有新增歌曲：文件已存在或无法读取。");
}
void MainWindow::sendMessage() {
    if (m_input->text().trimmed().isEmpty()) return;
    m_chat->sendText(m_input->text()); m_input->clear(); m_input->setFocus();
    QTimer::singleShot(0, this, [this] { m_messages->verticalScrollBar()->setValue(m_messages->verticalScrollBar()->maximum()); });
}
void MainWindow::removeMusic(const QString &path) {
    if (m_library->removeSong(path)) notify("已从音乐库移除，磁盘上的歌曲文件仍然保留。");
}
void MainWindow::notify(const QString &message) {
    m_notice->setText(message); m_notice->parentWidget()->show();
    // A persistent, non-modal notice keeps playback errors readable.
    m_notice->setToolTip("提示会在下次操作反馈时更新");
}
void MainWindow::togglePanel(int index) {
    auto *panel = m_splitter->widget(index);
    auto sizes = m_splitter->sizes();
    int &remembered = index == 0 ? m_leftWidth : m_rightWidth;
    if (panel->isHidden()) {
        panel->show(); sizes[index] = remembered; sizes[1] = qMax(350, m_splitter->width() - sizes[0] - sizes[2]); m_splitter->setSizes(sizes);
    } else { remembered = qMax(panel->minimumWidth(), sizes[index]); panel->hide(); }
}
void MainWindow::closeEvent(QCloseEvent *event) {
    QSettings settings; settings.setValue("geometry", saveGeometry()); settings.setValue("splitter", m_splitter->saveState());
    settings.setValue("leftHidden", m_splitter->widget(0)->isHidden()); settings.setValue("rightHidden", m_splitter->widget(2)->isHidden()); settings.setValue("volume", m_volume->value());
    QMainWindow::closeEvent(event);
}
