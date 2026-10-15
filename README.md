# TalkMusic

Qt Widgets / C++17 / CMake 桌面个人工作空间：左侧音乐播放，中间 Markdown 笔记或待办清单，右侧“音乐 / 笔记 / 待办”导航，底部固定播放器。暖白、浅灰、云红视觉风格。

## 音乐

右侧音乐页导入本地音频，双击播放。保留收藏、搜索、音量、顺序/随机/单曲循环和播放队列；歌曲行的 × 或右键菜单只从音乐库移除，不删除文件。切换工作区不会停止音乐。

歌词优先使用同名 LRC，其次使用音频内嵌歌词；同步歌词可高亮、点击定位，纯文本歌词显示全文。LRC 支持 UTF-8 和带 BOM 的 UTF-16/UTF-32；内嵌歌词由只读 TagLib 读取。左侧插入按钮将当前歌曲信息与本地路径写入当前笔记；未打开笔记时禁用。

## Markdown 笔记

1. 在右侧“笔记 → 文档”选择一个本地文件夹。文档树展示子目录和 `.md` 文件，不跟随符号链接或 Windows junction。
2. 点击笔记在中间打开；新建笔记或子文件夹，右键可重命名和移入系统回收站。删除会先确认，恢复请使用“打开系统回收站”在 Windows 中还原。
3. 默认“编辑”采用类似 Typora 的即时渲染，可直接点击正文编辑，当前段落显示 Markdown 标记。“源码”带行号；“分屏”显示源码与只读排版结果。工作区小于 700px 时分屏不可用，暂时显示即时编辑，空间恢复后继续分屏。
4. “大纲”按标题层级显示当前文档章节；支持 ATX 与 Setext 标题，排除围栏代码和缩进代码，重复标题按源位置分别定位。
5. 输入停止约一秒后自动保存，也可 Ctrl+S 或点击保存。只打开、查看和切换模式不改写文件；实际编辑后可能规范化 Markdown 格式，保留内容语义、UTF-8 BOM 和 CRLF 换行习惯。切换或退出前同步最后一次输入；中文输入尚未完成时请先确认候选文字。

正文使用暖白背景、居中排版和清晰标题层级；代码块提供浅灰圆角背景、语法高亮、行号、语言标识及复制按钮，复制不包含行号。右键提供剪切/复制/粘贴、撤销/重做、标题、加粗、斜体、行内代码、链接、引用、列表及插入代码块/分隔线/本地图片。

仅打开有效 UTF-8 笔记，其他编码会提示转换。即时编辑使用本地打包的 Vditor 3.11.2 和 Qt WebEngine，支持常用 Markdown。图片相对于文档目录解析，仅允许引用笔记文件夹内的本地图片，不复制或上传附件，不自动加载远程图片；HTTP/HTTPS/mailto 链接点击后交给系统浏览器。网页不直接写文件，保存及冲突处理统一由 C++ 完成。首版没有公式、Mermaid、复杂表格工具和跨目录拖拽移动，扩展语法可使用源码模式编辑。

外部修改时，未编辑的笔记会重新加载；本地有修改则暂停自动保存，可选择“加载磁盘版本 / 覆盖磁盘版本 / 另存为”。另存为目标应位于笔记根目录内。保存失败或冲突未解决时，阻止切换文档、工作区和正常退出，以保留编辑内容。目录丢失会保留路径设置与已打开内容，可恢复原目录后保存；启动时目录不可用则可重新选择目录。

每篇打开的笔记保留编辑器、光标、滚动和撤销历史；重新启动恢复最后打开的文件和编辑模式。图片路径不会在重命名或移动目录时自动改写。

## 待办清单

右侧“待办”新建清单，点击清单标题在中间打开。每份清单包含多个任务，支持标题、可选备注、完成/未完成及删除；已完成任务可折叠，任务按创建顺序排列。

右键清单可重命名或移入应用内回收区；“待办回收区”可恢复清单。“撤销删除”恢复当前清单最近删除的任务及原位置，撤销历史在本次运行保留。任务编辑保存失败时保留输入内容；添加和完成操作失败时不会改变原数据。

清单保存为独立、版本为 1 的 JSON 文件，清单与任务使用 UUID。损坏或不支持版本的文件保留原件并提示，不自动覆盖。Markdown 中的复选列表与独立待办互不同步。首版不包含日期、提醒、优先级、分组或联网同步。

## 布局与数据

默认窗口 1280×800，最小 960×640。右侧默认约 290px；分隔线可拖动，两侧可折叠。顶部专注模式收起两侧栏，保留底部播放器，退出时恢复原布局。选择音乐标签只改变右侧导航，不替换正在使用的中间工作区。

笔记直接保存在所选文件夹。音乐库位于 `QStandardPaths::AppDataLocation/library.json`，待办位于同目录的 `todos/<UUID>.todo.json`，回收清单位于 `todos/trash/`。Windows 通常对应用户的 `AppData/Roaming/TalkMusic/TalkMusic`。布局、笔记根目录、最近笔记/清单与音量通过 QSettings 保存；启动不自动播放。

改版移除了好友、聊天演示和一起听功能，不修改既有音乐库。备份时请同时备份笔记文件夹和应用数据目录。

## 构建

依赖 Qt 6.5 及以上的 Widgets、Multimedia、WebEngineWidgets、WebChannel 模块；测试还需要 Qt Test。本机验证环境为 Qt 6.7.3、MSVC 2022、Ninja。请使用 **x64 Native Tools Command Prompt for VS 2022**，不要混用 MinGW 编译器和 MSVC Qt 库。

首次配置会自动下载官方 TagLib 2.2.1 源码并校验 SHA-256，随后静态编译，不需要额外安装 TagLib。离线构建可使用 `-DFETCHCONTENT_SOURCE_DIR_TAGLIB=已解压源码的绝对路径`，官方发布包中已包含 utf8cpp。运行目录附带 TagLib 和 utf8cpp 许可证。

```bat
"D:\Software\Qt\Tools\CMake_64\bin\cmake.exe" -S . -B build\talkmusic-release -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_FLAGS_DEBUG=/Z7 /Ob0 /Od /RTC1" "-DCMAKE_C_FLAGS_DEBUG=/Z7 /Ob0 /Od /RTC1" -DBUILD_TESTING=OFF -DCMAKE_PREFIX_PATH=D:/Software/Qt/6.7.3/msvc2022_64 -DCMAKE_MAKE_PROGRAM=D:/Software/Qt/Tools/Ninja/ninja.exe
"D:\Software\Qt\Tools\CMake_64\bin\cmake.exe" --build build\talkmusic-release
"D:\Software\Qt\Tools\CMake_64\bin\cmake.exe" --install build\talkmusic-release --prefix "%CD%/build/package"
```

安装步骤部署 Qt 运行库和插件，包含 WebEngine 子进程、资源与语言包；运行 `build/package/bin/talkMusic.exe`，移动程序时应保留整个运行目录。目标 Windows 机器需要相应的 Microsoft Visual C++ 运行时。网页编辑资源已内嵌，不需要 Node.js、npm 或联网。

## 测试

```bat
"D:\Software\Qt\Tools\CMake_64\bin\cmake.exe" -S . -B build\talkmusic-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS_DEBUG=/Z7 /Ob0 /Od /RTC1" "-DCMAKE_C_FLAGS_DEBUG=/Z7 /Ob0 /Od /RTC1" -DCMAKE_PREFIX_PATH=D:/Software/Qt/6.7.3/msvc2022_64 -DCMAKE_MAKE_PROGRAM=D:/Software/Qt/Tools/Ninja/ninja.exe
"D:\Software\Qt\Tools\CMake_64\bin\cmake.exe" --build build\talkmusic-msvc
"D:\Software\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build\talkmusic-msvc --output-on-failure
```

`/Z7` 将调试信息保存在目标文件中，用于避开本机 `/Zi` 的程序数据库管理器错误。CTest 自动设置 Qt 插件及运行库路径，使用 offscreen 平台进行界面测试。Windows 下 offscreen 测试显式加载系统微软雅黑和 Segoe UI 字体；正常运行使用系统字体。

测试覆盖音乐播放、歌词、收藏、移除、队列，以及笔记自动保存、外部修改冲突、保存失败、关闭保护、编码与换行保留、目录导航和 Windows junction 隔离、重命名、重复标题定位、本地图片与远程图片隔离。待办测试覆盖稳定 ID、编辑、完成、删除撤销、清单回收恢复、损坏文件保留和保存失败。所有数据测试使用独立临时目录。

即时编辑测试还覆盖查看不改写、最后一次输入同步、旧版本回调隔离、中文组合输入保护、加粗/标题/正文右键操作、代码复制不含行号，以及分屏预览的可读性。代码块编辑时只显示源码，退出后显示不可编辑的高亮预览；通过 WebEngine 原生键盘事件验证回车、连续输入、撤销/重做、退出代码块、分屏及保存重开，覆盖无语言标记和 C++ 代码块，防止重复显示和重复保存。当前完整套件包含 20 项测试。中文组合输入使用自动化事件验证，具体输入法的候选窗口交互可在正常桌面运行时试用。

设置 `TALKMUSIC_SCREENSHOT_DIR` 可保存笔记、右键菜单、专注模式、待办和最小窗口截图；`QT_SCALE_FACTOR=1.5` 用于高 DPI 验证。WebEngine offscreen 环境可设置 `QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu` 使用软件渲染，正常桌面运行无需该参数。

## 代码结构

- `MainWindow`：三栏布局、工作区切换和播放器连接。
- `MusicLibrary`、`PlaybackController`、`lyricsreader`：音乐库、播放及歌词读取。
- `NoteStore`、`NoteDocument`：安全文件操作、原子保存、自动保存与外部冲突检查。
- `NoteEditor`、`WebNoteEditor`、`editor/`：源码行号、即时编辑网页、版本化桥接与持久页面状态。
- `NoteNavigator`、`markdownoutline`：文件树与大纲。
- `TodoStore`、`TodoWidget`：清单持久化、回收、删除撤销与任务编辑。
- `uihelpers`：原始图标、占位封面和动态省略文本。

界面由 C++ 布局构建，初始 `mainwindow.ui` 不参与构建。Git 暂存、提交和推送由用户手动执行。
