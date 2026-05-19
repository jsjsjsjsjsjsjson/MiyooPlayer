cmake_minimum_required(VERSION 3.13)

project(MiyooPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MIYOO_USE_SDL2 "Build against SDL2 instead of SDL 1.2" OFF)
option(MIYOO_BUILD_KEYTEST "Build SDL key test utility" ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

if(MIYOO_USE_SDL2)
    pkg_check_modules(SDL_BACKEND REQUIRED sdl2)
    add_compile_definitions(MIYOO_USE_SDL2=1)
else()
    pkg_check_modules(SDL_BACKEND REQUIRED sdl)
endif()

set(SDL_BACKEND_EXISTING_INCLUDE_DIRS "")
foreach(dir IN LISTS SDL_BACKEND_INCLUDE_DIRS)
    if(EXISTS "${dir}")
        list(APPEND SDL_BACKEND_EXISTING_INCLUDE_DIRS "${dir}")
    endif()
endforeach()

add_executable(miyoo-player
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

target_include_directories(miyoo-player PRIVATE src)
target_include_directories(miyoo-player PRIVATE ${SDL_BACKEND_EXISTING_INCLUDE_DIRS})
target_compile_options(miyoo-player PRIVATE ${SDL_BACKEND_CFLAGS_OTHER})
target_link_directories(miyoo-player PRIVATE ${SDL_BACKEND_LIBRARY_DIRS})

target_compile_options(miyoo-player PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-deprecated-declarations
    -Wno-psabi
    $<$<CONFIG:Release>:-Ofast>
    $<$<CONFIG:Release>:-fomit-frame-pointer>
    $<$<CONFIG:Release>:-fno-rtti>
)

target_link_libraries(miyoo-player PRIVATE
    PkgConfig::AVFORMAT
    PkgConfig::AVCODEC
    PkgConfig::AVUTIL
    PkgConfig::SWSCALE
    PkgConfig::SWRESAMPLE
    ${SDL_BACKEND_LIBRARIES}
)

install(TARGETS miyoo-player RUNTIME DESTINATION bin)

if(MIYOO_BUILD_KEYTEST)
    add_executable(miyoo-keytest tools/keytest/main.cpp)
    target_include_directories(miyoo-keytest PRIVATE src ${SDL_BACKEND_EXISTING_INCLUDE_DIRS})
    target_compile_options(miyoo-keytest PRIVATE
        ${SDL_BACKEND_CFLAGS_OTHER}
        -Wall
        -Wextra
        -Wpedantic
        -Wno-psabi
        $<$<CONFIG:Release>:-Ofast>
        $<$<CONFIG:Release>:-fomit-frame-pointer>
        $<$<CONFIG:Release>:-fno-rtti>
    )
    target_link_directories(miyoo-keytest PRIVATE ${SDL_BACKEND_LIBRARY_DIRS})
    target_link_libraries(miyoo-keytest PRIVATE ${SDL_BACKEND_LIBRARIES})
    install(TARGETS miyoo-keytest RUNTIME DESTINATION bin)
endif()
