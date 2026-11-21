cmake_minimum_required(VERSION 3.20)
# Bump VERSION before tagging a release (the tag build checks they match).
project(quickchat VERSION 0.4.1 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT WIN32)
    message(FATAL_ERROR "Targets Windows. On Linux use -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake")
endif()

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    message(FATAL_ERROR "Build with llvm-mingw (clang): the mod relies on its MSVC-style SEH and GNU extensions together.")
else()
    add_compile_options(-Wall -Wno-missing-field-initializers -fms-extensions)
    add_compile_options("$<$<CONFIG:Release>:-O2;-ffunction-sections;-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-s>")
    add_link_options(-static)
endif()

# The FH2 Mod Platform (git submodule): d3dcompiler_47.dll and DWrite.dll
# (the same file), shipped next to azimuth.exe, fh2mod.h and fh2mod_hud.h.
add_subdirectory(platform)
configure_file(cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/fh2_version.h)

# The mod: fh2-mods\QuickChat.dll
add_library(quickchat SHARED src/quickchat.cpp src/version.rc)
target_include_directories(quickchat PRIVATE src include ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(quickchat PRIVATE QC_VERSION="${PROJECT_VERSION}")
target_link_libraries(quickchat PRIVATE fh2mod_api)
set_target_properties(quickchat PROPERTIES OUTPUT_NAME QuickChat PREFIX "")

# The phrase rules, with no game: run quickchat_test.exe.
option(QC_TESTS "Build the offline test" OFF)
if(QC_TESTS)
    add_executable(quickchat_test tests/quickchat_test.cpp)
    target_include_directories(quickchat_test PRIVATE src include)
    target_link_libraries(quickchat_test PRIVATE fh2mod_api)
endif()
