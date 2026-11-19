cmake_minimum_required(VERSION 3.16)
project(gaming_mps_core_tests LANGUAGES CXX)
enable_testing()
add_executable(core core.cpp ../common/input.cpp ../common/screen.cpp ../common/code.cpp
    ../common/menu.cpp ../common/stub.cpp ../games/tetris/tetris.cpp
    ../games/minesweeper/minesweeper.cpp ../games/pong/pong.cpp)
target_include_directories(core PRIVATE ../common ../games/tetris ../games/minesweeper ../games/pong)
target_compile_features(core PRIVATE cxx_std_17)
target_compile_options(core PRIVATE -Wall -Wextra -Werror)
add_test(NAME core COMMAND core)
