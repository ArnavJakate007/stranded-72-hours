cmake_minimum_required(VERSION 3.16)
project(STRANDED CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB LOGIC_SOURCES CONFIGURE_DEPENDS src/logic/*.cpp)

add_executable(STRANDED ${GAME_SOURCES})
target_include_directories(STRANDED PRIVATE src external/raylib/include)
target_link_directories(STRANDED PRIVATE external/raylib/lib)
target_compile_options(STRANDED PRIVATE -Wall -Wextra)
target_link_libraries(STRANDED PRIVATE raylib opengl32 gdi32 winmm -static -static-libgcc -static-libstdc++)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_link_options(STRANDED PRIVATE -mwindows)   # no console window in the release build
endif()

# copy the data folder next to the executable so it runs from the build folder
add_custom_command(TARGET STRANDED POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:STRANDED>/data)

# logic-only self-test: no window, no raylib
add_executable(selftest tests/selftest.cpp ${LOGIC_SOURCES})
target_include_directories(selftest PRIVATE src)
target_compile_options(selftest PRIVATE -Wall -Wextra)

# automated playtester: plays every ending through the real rules (build/playtest.exe [scenario number])
add_executable(playtest tests/playtest.cpp ${LOGIC_SOURCES})
target_include_directories(playtest PRIVATE src)
target_compile_options(playtest PRIVATE -Wall -Wextra)
