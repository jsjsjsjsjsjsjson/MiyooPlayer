set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(MIYOO_ROOT /opt/miyoo CACHE PATH "MiyooCFW toolchain root")
set(CMAKE_SYSROOT ${MIYOO_ROOT}/arm-miyoo-linux-uclibcgnueabi/sysroot CACHE PATH "MiyooCFW sysroot")

set(CMAKE_C_COMPILER ${MIYOO_ROOT}/bin/arm-miyoo-linux-uclibcgnueabi-gcc CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER ${MIYOO_ROOT}/bin/arm-miyoo-linux-uclibcgnueabi-g++ CACHE FILEPATH "")

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
set(ENV{PKG_CONFIG_LIBDIR}
    "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig:${CMAKE_SYSROOT}/lib/pkgconfig")

set(CMAKE_C_FLAGS_INIT "-march=armv5te -mtune=arm926ej-s -msoft-float")
set(CMAKE_CXX_FLAGS_INIT "-march=armv5te -mtune=arm926ej-s -msoft-float -fno-exceptions")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--as-needed")
