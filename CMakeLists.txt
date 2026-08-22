cmake_minimum_required(VERSION 3.15)

project(Sigma360
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(cJSON REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED IMPORTED_TARGET notcurses-core)

add_executable(sigma360
    src/main.c
    src/tui.c
    src/nav.c
    src/utilities.c
)

target_include_directories(sigma360 PRIVATE src)
target_link_libraries(sigma360 PRIVATE PkgConfig::NOTCURSES cjson)

target_compile_options(sigma360 PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CONFIG:Debug>:-fsanitize=address -fno-omit-frame-pointer>
)

target_link_options(sigma360 PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
)
