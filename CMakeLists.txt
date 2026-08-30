# Alternative build to the Makefile.
#
# NOTE: the Makefile is the build that has been exercised in this repository;
# CMake was not available in the development environment, so treat this file as
# convenience scaffolding rather than a verified path.
cmake_minimum_required(VERSION 3.16)
project(new_rally_x CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(REMOVE_ITEM GAME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_library(rallyx_core STATIC ${GAME_SOURCES})
target_include_directories(rallyx_core PUBLIC src)

# Prefer the vendored headers staged by tools/fetch_sdl2.sh, else find SDL2.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sdl2/include/SDL2/SDL.h)
    target_include_directories(rallyx_core PUBLIC third_party/sdl2/include/SDL2
                                                third_party/sdl2/include)
    target_link_directories(rallyx_core PUBLIC third_party/sdl2/lib)
    target_link_libraries(rallyx_core PUBLIC SDL2)
else()
    find_package(SDL2 REQUIRED)
    target_include_directories(rallyx_core PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(rallyx_core PUBLIC ${SDL2_LIBRARIES})
endif()

add_executable(newrallyx src/main.cpp)
target_link_libraries(newrallyx PRIVATE rallyx_core)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(rallyx_tests ${TEST_SOURCES})
target_include_directories(rallyx_tests PRIVATE tests)
target_link_libraries(rallyx_tests PRIVATE rallyx_core)

enable_testing()
add_test(NAME gameplay COMMAND rallyx_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
