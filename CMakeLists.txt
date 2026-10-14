cmake_minimum_required(VERSION 3.19)
project(talkMusic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia)

qt_standard_project_setup()
include(cmake/TagLib.cmake)

qt_add_executable(talkMusic
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    musiclibrary.cpp musiclibrary.h
    playbackcontroller.cpp playbackcontroller.h
    notestore.cpp notestore.h
    noteeditor.cpp noteeditor.h
    notenavigator.cpp notenavigator.h
    markdownoutline.cpp markdownoutline.h
    todostore.cpp todostore.h
    todowidget.cpp todowidget.h
    uihelpers.cpp uihelpers.h
    lyricsreader.cpp lyricsreader.h
)

target_link_libraries(talkMusic
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Multimedia
        tag
)

if(MSVC)
    target_compile_options(talkMusic PRIVATE /utf-8)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(talkMusicTests
        tests/tst_talkmusic.cpp
        mainwindow.cpp mainwindow.h
        musiclibrary.cpp musiclibrary.h
        playbackcontroller.cpp playbackcontroller.h
        notestore.cpp notestore.h
        noteeditor.cpp noteeditor.h
        notenavigator.cpp notenavigator.h
        markdownoutline.cpp markdownoutline.h
        todostore.cpp todostore.h
        todowidget.cpp todowidget.h
        uihelpers.cpp uihelpers.h
        lyricsreader.cpp lyricsreader.h
    )
    target_include_directories(talkMusicTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(talkMusicTests PRIVATE TAGLIB_FIXTURE_DIR="${TALKMUSIC_TAGLIB_SOURCE_DIR}/tests/data")
    target_link_libraries(talkMusicTests PRIVATE Qt::Widgets Qt::Multimedia Qt::Test tag)
    if(MSVC)
        target_compile_options(talkMusicTests PRIVATE /utf-8)
    endif()
    add_test(NAME talkMusicTests COMMAND talkMusicTests)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    set_tests_properties(talkMusicTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${QT_BIN_DIR}/../plugins")
    set_tests_properties(talkMusicTests PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_BIN_DIR}")
endif()

include(GNUInstallDirs)

install(TARGETS talkMusic
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET talkMusic
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
