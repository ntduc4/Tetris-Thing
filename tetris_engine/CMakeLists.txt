cmake_minimum_required(VERSION 3.20)

project(tetris_engine LANGUAGES CXX)

add_library(tetris_engine
    src/core/Board.cpp
    src/core/Piece.cpp
    src/engine/Engine.cpp
    src/engine/Movement.cpp
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


# Tests cases
enable_testing()

add_executable(tetris_core_board_tests tests/core/board_tests.cpp)
target_link_libraries(tetris_core_board_tests PRIVATE tetris_engine)
target_include_directories(tetris_core_board_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_core_board_tests COMMAND tetris_core_board_tests)


add_executable(tetris_core_piece_tests tests/core/piece_tests.cpp)
target_link_libraries(tetris_core_piece_tests PRIVATE tetris_engine)
target_include_directories(tetris_core_piece_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_core_piece_tests COMMAND tetris_core_piece_tests)


add_executable(tetris_engine_rules_spec_tests tests/engine/rules_spec_tests.cpp)
target_link_libraries(tetris_engine_rules_spec_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_rules_spec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_rules_spec_tests COMMAND tetris_engine_rules_spec_tests)


add_executable(tetris_engine_runtime_spec_tests tests/engine/engine_runtime_spec_tests.cpp)
target_link_libraries(tetris_engine_runtime_spec_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_runtime_spec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_runtime_spec_tests COMMAND tetris_engine_runtime_spec_tests)


add_executable(tetris_engine_movement_spec_tests tests/engine/movement_spec_tests.cpp)
target_link_libraries(tetris_engine_movement_spec_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_movement_spec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_movement_spec_tests COMMAND tetris_engine_movement_spec_tests)


add_executable(tetris_engine_spawn_spec_tests tests/engine/spawn_spec_tests.cpp) 
target_link_libraries(tetris_engine_spawn_spec_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_spawn_spec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_spawn_spec_tests COMMAND tetris_engine_spawn_spec_tests)

add_executable(tetris_engine_hold_spec_tests tests/engine/hold_spec_tests.cpp)
target_link_libraries(tetris_engine_hold_spec_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_hold_spec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_hold_spec_tests COMMAND tetris_engine_hold_spec_tests)

add_executable(tetris_engine_ghost_spec_tests tests/engine/ghost_spec_tests.cpp)
target_link_libraries(tetris_engine_ghost_spec_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_ghost_spec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_ghost_spec_tests COMMAND tetris_engine_ghost_spec_tests)
