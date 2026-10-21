include(FetchContent)

# 用函数作用域限制 TagLib 通用选项，避免它的 BUILD_TESTING 污染主项目测试设置。
function(talkmusic_add_taglib)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_BINDINGS OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_TESTING OFF)
    set(WITH_ZLIB OFF)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    # 固定版本与下载摘要，使构建来源可复现、可校验。
    FetchContent_Declare(taglib
        URL https://taglib.org/releases/taglib-2.2.1.tar.gz
        URL_HASH SHA256=7e76b5299dcef427c486bffe455098470c8da91cf3ccb9ea804893df57389b5e
        TIMEOUT 90
    )
    FetchContent_GetProperties(taglib)
    if(NOT taglib_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(taglib)
        add_subdirectory(${taglib_SOURCE_DIR} ${taglib_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
    # TagLib 内部目标主要导出安装态头文件目录，这里补齐源码构建所需路径。
    file(GLOB_RECURSE taglib_headers CONFIGURE_DEPENDS "${taglib_SOURCE_DIR}/taglib/*.h")
    set(taglib_includes "${taglib_BINARY_DIR}")
    foreach(header IN LISTS taglib_headers)
        get_filename_component(directory "${header}" DIRECTORY)
        list(APPEND taglib_includes "${directory}")
    endforeach()
    list(REMOVE_DUPLICATES taglib_includes)
    target_include_directories(tag INTERFACE "$<BUILD_INTERFACE:${taglib_includes}>")
    set(TALKMUSIC_TAGLIB_SOURCE_DIR "${taglib_SOURCE_DIR}" PARENT_SCOPE)
    install(FILES "${taglib_SOURCE_DIR}/COPYING.LGPL" "${taglib_SOURCE_DIR}/COPYING.MPL"
        DESTINATION share/licenses/taglib)
    install(FILES "${taglib_SOURCE_DIR}/3rdparty/utfcpp/LICENSE"
        DESTINATION share/licenses/utfcpp)
endfunction()
talkmusic_add_taglib()
