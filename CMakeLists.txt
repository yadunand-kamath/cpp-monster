cmake_minimum_required(VERSION 3.24)

project(cpp_workbook LANGUAGES CXX)

# --- Global settings -------------------------------------------------------
# Individual chapter/project targets may override these per-target; this is
# just the workbook-wide default so a bare `add_executable` picks up C++20
# without every chapter having to repeat the boilerplate.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Warnings ---------------------------------------------------------------
# README.md's toolchain section commits to /W4 /permissive- on MSVC and an
# equivalent strict set on GCC/Clang. Centralized here as an INTERFACE
# target so every chapter/project links it instead of repeating flags.
add_library(workbook_warnings INTERFACE)
if (MSVC)
    target_compile_options(workbook_warnings INTERFACE /W4 /permissive- /Zc:preprocessor)
else()
    target_compile_options(workbook_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# --- Sanitizers --------------------------------------------------------------
# One sanitizer per build, selected by CMAKE_BUILD_TYPE-independent cache
# option, never combined (see README.md's sanitizer-availability matrix).
# ASan is available under MSVC; UBSan/TSan/MSan are Clang/GCC-only and are
# therefore only meaningful under the wsl-clang-* presets.
set(WORKBOOK_SANITIZER "" CACHE STRING "Sanitizer to build with: OFF, address, undefined, thread, memory")
add_library(workbook_sanitizers INTERFACE)
if (WORKBOOK_SANITIZER AND NOT WORKBOOK_SANITIZER STREQUAL "OFF")
    if (MSVC)
        if (NOT WORKBOOK_SANITIZER STREQUAL "address")
            message(FATAL_ERROR "MSVC only supports WORKBOOK_SANITIZER=address (UBSan/TSan/MSan require wsl-clang-* presets)")
        endif()
        target_compile_options(workbook_sanitizers INTERFACE /fsanitize=address)
    else()
        target_compile_options(workbook_sanitizers INTERFACE -fsanitize=${WORKBOOK_SANITIZER} -fno-omit-frame-pointer -g)
        target_link_options(workbook_sanitizers INTERFACE -fsanitize=${WORKBOOK_SANITIZER})
    endif()
endif()

# --- Testing ------------------------------------------------------------------
# GoogleTest via FetchContent, discovered by CTest via gtest_discover_tests(),
# exactly as committed to in README.md. Every chapter/project CMakeLists that
# adds tests should `include(GoogleTest)` and call `gtest_discover_tests()`
# on its test executable; this fetch happens once, here, for the whole tree.
enable_testing()
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
# Keep GoogleTest's own runtime library selection consistent with ours on MSVC.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# --- Subdirectories -----------------------------------------------------------
# Chapters and projects register themselves here as their CMakeLists.txt files
# are added. Each is wrapped in `if (EXISTS ...)` so this root file works
# correctly both today (sandbox only) and as content grows a target at a time
# — nobody has to remember to touch this file when adding chapter N's tests.
add_subdirectory(sandbox)

foreach(chapter_dir
    01-core-semantics 02-lifetime-raii 03-value-categories 04-stl
    05-generic-programming 06-error-handling 07-object-model 08-compilation-abi
    09-systems-programming 10-build-systems 11-concurrency 12-performance
    13-modern-cpp-architecture
)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${chapter_dir}/CMakeLists.txt")
        add_subdirectory(${chapter_dir})
    endif()
endforeach()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/projects/CMakeLists.txt")
    add_subdirectory(projects)
endif()
