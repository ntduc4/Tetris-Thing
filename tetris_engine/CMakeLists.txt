cmake_minimum_required(VERSION 3.20)

project(tetris_engine LANGUAGES CXX)

add_library(tetris_engine
    src/core/Board.cpp
    src/core/Piece.cpp
    src/engine/Engine.cpp
    src/engine/Match.cpp
    src/engine/RNG.cpp
    src/engine/Rules.cpp
    src/engine/SoloSession.cpp
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

option(TETRIS_BUILD_QT "Build the local-play Qt Widgets app when Qt 6 is available" ON)
if(TETRIS_BUILD_QT)
    find_package(Qt6 QUIET COMPONENTS Widgets)
    if(Qt6Widgets_FOUND)
        add_library(tetris_qt_widgets STATIC
            apps/qt/BoardWidget.cpp
            apps/qt/GameConfig.cpp
            apps/qt/MainWindow.cpp
            apps/qt/RulesDialog.cpp
        )
        target_compile_features(tetris_qt_widgets PUBLIC cxx_std_20)
        target_include_directories(tetris_qt_widgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/apps/qt)
        target_link_libraries(tetris_qt_widgets PUBLIC tetris_engine Qt6::Widgets)

        add_executable(tetris_qt apps/qt/main.cpp)
        target_link_libraries(tetris_qt PRIVATE tetris_qt_widgets)
    else()
        message(STATUS "Qt 6 Widgets not found: skipping tetris_qt")
    endif()
endif()


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

add_executable(tetris_engine_hard_drop_distance_spec_tests tests/engine/hard_drop_distance_spec_tests.cpp)
target_link_libraries(tetris_engine_hard_drop_distance_spec_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_hard_drop_distance_spec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_hard_drop_distance_spec_tests COMMAND tetris_engine_hard_drop_distance_spec_tests)

add_executable(tetris_engine_try_move_spec_tests tests/engine/engine_try_move_spec_tests.cpp)
target_link_libraries(tetris_engine_try_move_spec_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_try_move_spec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_try_move_spec_tests COMMAND tetris_engine_try_move_spec_tests)

add_executable(tetris_engine_gameplay_spec_tests tests/engine/gameplay_spec_tests.cpp)
target_link_libraries(tetris_engine_gameplay_spec_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_gameplay_spec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_gameplay_spec_tests COMMAND tetris_engine_gameplay_spec_tests)

if(TARGET tetris_qt_widgets)
    find_package(Qt6 QUIET COMPONENTS Test)
    if(Qt6Test_FOUND)
        add_executable(tetris_qt_app_tests tests/qt_app_tests.cpp)
        target_link_libraries(tetris_qt_app_tests PRIVATE tetris_qt_widgets Qt6::Test)
        add_test(NAME tetris_qt_app_tests COMMAND tetris_qt_app_tests)
        set_tests_properties(tetris_qt_app_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()
