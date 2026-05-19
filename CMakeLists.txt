cmake_minimum_required(VERSION 3.13)

project(MiyooPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MIYOO_NATIVE_TEST "Build a native desktop test binary using the host SDL/FFmpeg libraries" OFF)
set(MIYOO_USE_SDL2_DEFAULT OFF)
if(MIYOO_NATIVE_TEST)
    set(MIYOO_USE_SDL2_DEFAULT ON)
endif()
option(MIYOO_USE_SDL2 "Build against SDL2 instead of SDL 1.2" ${MIYOO_USE_SDL2_DEFAULT})
option(MIYOO_BUILD_KEYTEST "Build SDL key test utility" ON)

if(MIYOO_NATIVE_TEST AND CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "MIYOO_NATIVE_TEST is a host/native build; configure it without cmake/miyoo-toolchain.cmake")
endif()

find_package(PkgConfig REQUIRED)

pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

if(MIYOO_USE_SDL2)
    pkg_check_modules(SDL_BACKEND REQUIRED sdl2)
else()
    pkg_check_modules(SDL_BACKEND REQUIRED sdl)
endif()

set(SDL_BACKEND_EXISTING_INCLUDE_DIRS "")
foreach(dir IN LISTS SDL_BACKEND_INCLUDE_DIRS)
    if(EXISTS "${dir}")
        list(APPEND SDL_BACKEND_EXISTING_INCLUDE_DIRS "${dir}")
    endif()
endforeach()

set(MIYOO_PLAYER_TARGET miyoo-player)
if(MIYOO_NATIVE_TEST)
    set(MIYOO_PLAYER_TARGET miyoo-player-native)
endif()

set(MIYOO_PLAYER_SOURCES
    src/audio_queue.cpp
    src/file_browser.cpp
    src/ffmpeg_compat.cpp
    src/input.cpp
    src/main.cpp
    src/media_tools.cpp
    src/player.cpp
    src/playlist_manager.cpp
    src/settings.cpp
    src/sdl_platform.cpp
    src/ui.cpp
    src/video_renderer.cpp
)

function(miyoo_apply_common_options target_name)
    target_include_directories(${target_name} PRIVATE src ${SDL_BACKEND_EXISTING_INCLUDE_DIRS})
    target_compile_options(${target_name} PRIVATE
        ${SDL_BACKEND_CFLAGS_OTHER}
        -Wall
        -Wextra
        -Wpedantic
        -Wno-deprecated-declarations
        -Wno-psabi
        $<$<CONFIG:Release>:-mcpu=arm926ej-s>
        $<$<CONFIG:Release>:-marm>
        $<$<CONFIG:Release>:-Ofast>
        $<$<CONFIG:Release>:-fomit-frame-pointer>
        $<$<CONFIG:Release>:-fno-rtti>
    )
    if(MIYOO_USE_SDL2)
        target_compile_definitions(${target_name} PRIVATE MIYOO_USE_SDL2=1)
    endif()
    if(MIYOO_NATIVE_TEST)
        target_compile_definitions(${target_name} PRIVATE MIYOO_NATIVE_TEST=1)
    endif()
    target_link_directories(${target_name} PRIVATE ${SDL_BACKEND_LIBRARY_DIRS})
    if(SDL_BACKEND_LDFLAGS_OTHER)
        target_link_options(${target_name} PRIVATE ${SDL_BACKEND_LDFLAGS_OTHER})
    endif()
endfunction()

add_executable(${MIYOO_PLAYER_TARGET} ${MIYOO_PLAYER_SOURCES})
miyoo_apply_common_options(${MIYOO_PLAYER_TARGET})
target_link_libraries(${MIYOO_PLAYER_TARGET} PRIVATE
    PkgConfig::AVFORMAT
    PkgConfig::AVCODEC
    PkgConfig::AVUTIL
    PkgConfig::SWSCALE
    PkgConfig::SWRESAMPLE
    ${SDL_BACKEND_LIBRARIES}
)

if(NOT MIYOO_NATIVE_TEST)
    install(TARGETS ${MIYOO_PLAYER_TARGET} RUNTIME DESTINATION bin)
endif()

if(MIYOO_BUILD_KEYTEST)
    set(MIYOO_KEYTEST_TARGET miyoo-keytest)
    if(MIYOO_NATIVE_TEST)
        set(MIYOO_KEYTEST_TARGET miyoo-keytest-native)
    endif()

    add_executable(${MIYOO_KEYTEST_TARGET} tools/keytest/main.cpp)
    miyoo_apply_common_options(${MIYOO_KEYTEST_TARGET})
    target_link_libraries(${MIYOO_KEYTEST_TARGET} PRIVATE ${SDL_BACKEND_LIBRARIES})
    if(NOT MIYOO_NATIVE_TEST)
        install(TARGETS ${MIYOO_KEYTEST_TARGET} RUNTIME DESTINATION bin)
    endif()
endif()
