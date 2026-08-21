cmake_minimum_required(VERSION 3.15)

project(Sigma360
    VERSION 0.1.0
    LANGUAGES C
)

# Require C11 (bump this if you need a newer standard)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Default to Release build if nothing specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Emit compile_commands.json (handy for editors / clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(sigma360
    src/main.c
    src/cli.c
)

target_include_directories(sigma360 PRIVATE src)

target_compile_options(sigma360 PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)