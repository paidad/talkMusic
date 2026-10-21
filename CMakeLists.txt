cmake_minimum_required(VERSION 3.19)
project(talkMusic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia WebEngineWidgets WebChannel)

qt_standard_project_setup()
include(cmake/TagLib.cmake)

# 应用与测试共用业务源码列表，避免新增模块时漏改其中一个目标。
set(TALKMUSIC_SOURCES
    src/app/mainwindow.cpp src/app/mainwindow.h
    src/music/musiclibrary.cpp src/music/musiclibrary.h
    src/music/playbackcontroller.cpp src/music/playbackcontroller.h
    src/music/lyricsreader.cpp src/music/lyricsreader.h
    src/notes/notestore.cpp src/notes/notestore.h
    src/notes/noteeditor.cpp src/notes/noteeditor.h
    src/notes/webnoteeditor.cpp src/notes/webnoteeditor.h
    src/notes/notenavigator.cpp src/notes/notenavigator.h
    src/notes/markdownoutline.cpp src/notes/markdownoutline.h
    src/todos/todostore.cpp src/todos/todostore.h
    src/todos/todowidget.cpp src/todos/todowidget.h
    src/widgets/uihelpers.cpp src/widgets/uihelpers.h
)

qt_add_executable(talkMusic WIN32 MACOSX_BUNDLE src/main.cpp ${TALKMUSIC_SOURCES})
# 统一以 src 为头文件引用根目录，例如 notes/noteeditor.h。
target_include_directories(talkMusic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(talkMusic
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Multimedia
        Qt::WebEngineWidgets
        Qt::WebChannel
        tag
)

if(MSVC)
    # 源码与中文注释统一使用 UTF-8，避免依赖本机默认代码页。
    target_compile_options(talkMusic PRIVATE /utf-8)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(talkMusicTests
        tests/tst_talkmusic.cpp
        ${TALKMUSIC_SOURCES}
    )
    target_include_directories(talkMusicTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(talkMusicTests PRIVATE TAGLIB_FIXTURE_DIR="${TALKMUSIC_TAGLIB_SOURCE_DIR}/tests/data")
    target_link_libraries(talkMusicTests PRIVATE Qt::Widgets Qt::Multimedia Qt::WebEngineWidgets Qt::WebChannel Qt::Test tag)
    if(MSVC)
        target_compile_options(talkMusicTests PRIVATE /utf-8)
    endif()
    add_test(NAME talkMusicTests COMMAND talkMusicTests -o -,txt)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    set_tests_properties(talkMusicTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${QT_BIN_DIR}/../plugins")
    set_tests_properties(talkMusicTests PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_BIN_DIR}")
endif()

# 网页与第三方编辑器资源编入 Qt 资源系统，运行时不依赖 npm 或网络下载。
file(GLOB_RECURSE NOTE_EDITOR_ASSETS CONFIGURE_DEPENDS third_party/vditor/dist/*)
set(NOTE_EDITOR_TARGETS talkMusic)
if(BUILD_TESTING)
    list(APPEND NOTE_EDITOR_TARGETS talkMusicTests)
endif()
foreach(editor_target IN LISTS NOTE_EDITOR_TARGETS)
    qt_add_resources(${editor_target} note_editor_ui PREFIX "/" FILES editor/index.html editor/editor.js editor/style.css)
    qt_add_resources(${editor_target} note_editor_vendor PREFIX "/vditor" BASE third_party/vditor FILES ${NOTE_EDITOR_ASSETS})
endforeach()

include(GNUInstallDirs)

install(TARGETS talkMusic
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 部署整个运行目录，包括 Qt 插件与 WebEngine 进程/资源，不只复制 exe。
qt_generate_deploy_app_script(
    TARGET talkMusic
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
install(FILES third_party/vditor/LICENSE DESTINATION ${CMAKE_INSTALL_BINDIR}/licenses RENAME Vditor-MIT.txt)
install(FILES third_party/vditor/dist/js/highlight.js/LICENSE DESTINATION ${CMAKE_INSTALL_BINDIR}/licenses RENAME highlight-js-BSD.txt)
install(FILES third_party/vditor/Lute-LICENSE third_party/vditor/diff-match-patch-LICENSE DESTINATION ${CMAKE_INSTALL_BINDIR}/licenses)
