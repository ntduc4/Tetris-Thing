cmake_minimum_required(VERSION 3.20)

project(tetris_engine LANGUAGES CXX)

add_library(tetris_engine
    src/core/Board.cpp
    src/core/Piece.cpp
)

target_compile_features(tetris_engine PUBLIC cxx_std_20)

target_include_directories(tetris_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(tetris_cli
    apps/cli.cpp
)

target_link_libraries(tetris_cli
    PRIVATE
        tetris_engine
)

enable_testing()

add_executable(tetris_tests
    tests/main.cpp
    tests/board_tests.cpp
    tests/piece_tests.cpp
)

target_link_libraries(tetris_tests
    PRIVATE
        tetris_engine
)

add_test(NAME tetris_tests COMMAND tetris_tests)
