# Android build for New Rally-X.
#
# The entire game core is the same C++ that the desktop build uses -- there is
# no Android-specific gameplay code.  This file only wires SDL2 and the shared
# sources together into the libmain.so that SDLActivity loads.
cmake_minimum_required(VERSION 3.22)
project(newrallyx CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# android/app/src/main/cpp -> repository root
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(SDL_ROOT "${REPO_ROOT}/third_party/SDL2-2.30.9")

if(NOT EXISTS "${SDL_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "SDL2 sources not found at ${SDL_ROOT}. Run tools/fetch_sdl2_android.sh first.")
endif()

add_subdirectory("${SDL_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/SDL2")

file(GLOB_RECURSE GAME_SOURCES "${REPO_ROOT}/src/*.cpp")

# SDLActivity loads "SDL2" then "main", so the game library must be libmain.so.
add_library(main SHARED ${GAME_SOURCES})

target_include_directories(main PRIVATE "${REPO_ROOT}/src")
target_link_libraries(main PRIVATE SDL2 SDL2main)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(main PRIVATE -O0 -g)
else()
    target_compile_options(main PRIVATE -O2)
endif()
target_compile_options(main PRIVATE -Wall -Wextra -Wno-unused-parameter)
