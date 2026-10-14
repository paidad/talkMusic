# TalkMusic

Qt Widgets / C++17 / CMake 桌面应用：左侧正在播放，中间演示聊天，右侧音乐与好友，底部固定播放栏。暖白背景、云红强调，原创唱片封面和矢量绘制图标。

## 使用

1. 在右侧「音乐」页点击「导入本地音乐」，多选音频文件。重复路径会自动跳过。
2. 双击歌曲播放；底部控制播放、暂停、切歌、进度、音量和播放模式，最右按钮打开播放队列。
3. 左侧爱心收藏当前歌曲，分享按钮将歌曲卡片发送到当前演示会话。卡片的播放按钮使用本机文件。
4. 右侧「好友」页切换会话；输入文字后按 Enter 发送。消息仅在当前运行保留，演示好友不会收到消息。
5. 顶部右侧按钮可折叠两侧面板，拖动分隔线可调整宽度。

支持读取音频中的歌曲、歌手和封面元数据。缺失时显示文件名、未知歌手和原创占位封面。歌词使用同目录、同名的 UTF-8 `.lrc` 文件，支持多时间戳、毫秒及 `offset`，点击歌词可跳转播放位置。具体音频格式支持取决于 Qt Multimedia 后端。

音乐库与收藏保存于 `QStandardPaths::AppDataLocation/library.json`，Windows 通常在用户的 `AppData/Roaming/TalkMusic/TalkMusic` 下。窗口布局和音量由 `QSettings` 保存。启动不自动播放。音乐库只记录文件路径，不复制音频文件。

首版未接入登录、在线曲库、真实好友通信、同步播放和 AI。「一起听」按钮会明确提示功能尚未接入。

## 构建

依赖 Qt 6.5 及以上的 Widgets、Multimedia 模块；测试还需要 Qt Test。本机验证环境为 Qt 6.7.3、MSVC 2022、Ninja。请使用 **x64 Native Tools Command Prompt for VS 2022**，不要混用 MinGW 编译器和 MSVC Qt 库。

```bat
"D:\Software\Qt\Tools\CMake_64\bin\cmake.exe" -S . -B build\talkmusic-release -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_FLAGS_DEBUG=/Z7 /Ob0 /Od /RTC1" -DBUILD_TESTING=OFF -DCMAKE_PREFIX_PATH=D:/Software/Qt/6.7.3/msvc2022_64 -DCMAKE_MAKE_PROGRAM=D:/Software/Qt/Tools/Ninja/ninja.exe
"D:\Software\Qt\Tools\CMake_64\bin\cmake.exe" --build build\talkmusic-release
"D:\Software\Qt\Tools\CMake_64\bin\cmake.exe" --install build\talkmusic-release --prefix build/package
```

安装步骤部署 Qt 运行库和插件，运行 `build/package/bin/talkMusic.exe`，移动程序时应保留整个运行目录。目标 Windows 机器需要相应的 Microsoft Visual C++ 运行时。

## 测试

```bat
"D:\Software\Qt\Tools\CMake_64\bin\cmake.exe" -S . -B build\talkmusic-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS_DEBUG=/Z7 /Ob0 /Od /RTC1" -DCMAKE_PREFIX_PATH=D:/Software/Qt/6.7.3/msvc2022_64 -DCMAKE_MAKE_PROGRAM=D:/Software/Qt/Tools/Ninja/ninja.exe
"D:\Software\Qt\Tools\CMake_64\bin\cmake.exe" --build build\talkmusic-msvc
"D:\Software\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build\talkmusic-msvc --output-on-failure
```

`/Z7` 将调试信息保存在目标文件中，用于避开本机 `/Zi` 的程序数据库管理器错误。CTest 自动设置 Qt 插件及运行库路径，使用 offscreen 平台进行界面测试。Windows 下 offscreen 测试显式加载系统微软雅黑和 Segoe UI 字体；正常运行使用系统字体。

测试覆盖歌词解析、导入去重、收藏和元数据持久化、会话隔离、真实 WAV 播放、暂停和跳转、缺失文件、无效媒体、快速切歌、自动切歌、单曲循环以及界面搜索、分享、会话切换、折叠和设置恢复。测试使用独立临时目录，不修改个人音乐库。

设置 `TALKMUSIC_SCREENSHOT_DIR` 可在界面测试中保存默认和最小窗口截图。高 DPI 检查可在运行测试前设置 `QT_SCALE_FACTOR=1.5`。

## 代码结构

- `MainWindow`：布局、控件以及模块信号连接。
- `MusicLibrary`：歌曲数据、路径去重、异步元数据读取和原子保存。
- `PlaybackController`：播放器生命周期、队列策略、播放控制与歌词定位。切歌重建播放器，避免 Qt 6.7 FFmpeg 后端加载期间换源的断言。
- `DemoChat`：独立的演示会话和消息状态，后续可替换为联网服务。
- `uihelpers`：可缩放的原创图标、占位封面和动态省略文本。

界面目前由 C++ 布局构建，初始模板 `mainwindow.ui` 不参与构建。
