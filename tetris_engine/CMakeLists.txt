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

# Bot code is separate from the game runtime. Link this target only for
# consumers that need an engine-aware search teacher.
add_library(tetris_bot src/bot/MisaMinoBot.cpp)
target_compile_features(tetris_bot PUBLIC cxx_std_20)
target_link_libraries(tetris_bot PUBLIC tetris_engine)

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
            apps/qt/PieceQueueWidget.cpp
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

option(TETRIS_BUILD_PYTHON "Build the headless Python module when pybind11 is available" ON)
if(TETRIS_BUILD_PYTHON)
    find_package(Python3 QUIET COMPONENTS Interpreter Development.Module)
    if(Python3_FOUND)
        set(PYBIND11_FINDPYTHON ON)
        find_package(pybind11 QUIET CONFIG)
        if(NOT pybind11_FOUND)
            # pip-installed pybind11 may not be in CMake's standard prefix path.
            execute_process(
                COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
                OUTPUT_VARIABLE _pybind11_cmake_dir
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE _pybind11_result
            )
            if(_pybind11_result EQUAL 0)
                set(pybind11_DIR "${_pybind11_cmake_dir}")
                find_package(pybind11 QUIET CONFIG)
            endif()
        endif()
        if(pybind11_FOUND)
            set_target_properties(tetris_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
            pybind11_add_module(tetris_py apps/python/bindings.cpp)
            target_link_libraries(tetris_py PRIVATE tetris_engine)
        else()
            message(STATUS "pybind11 not found: skipping tetris_py")
        endif()
    else()
        message(STATUS "Python 3 development files not found: skipping tetris_py")
    endif()
endif()


# Tests cases
enable_testing()

add_test(NAME tetris_cli_srs_x_smoke COMMAND tetris_cli --ticks 30 --rotation srs-x)

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

add_executable(tetris_engine_rotation_variants_tests tests/engine/rotation_variants_tests.cpp)
target_link_libraries(tetris_engine_rotation_variants_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_rotation_variants_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_rotation_variants_tests COMMAND tetris_engine_rotation_variants_tests)

add_executable(tetris_engine_placement_search_tests tests/engine/placement_search_tests.cpp)
target_link_libraries(tetris_engine_placement_search_tests PRIVATE tetris_engine)
target_include_directories(tetris_engine_placement_search_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_engine_placement_search_tests COMMAND tetris_engine_placement_search_tests)

add_executable(tetris_bot_tests tests/bot/misamino_bot_tests.cpp)
target_link_libraries(tetris_bot_tests PRIVATE tetris_bot)
target_include_directories(tetris_bot_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME tetris_bot_tests COMMAND tetris_bot_tests)

if(TARGET tetris_qt_widgets)
    find_package(Qt6 QUIET COMPONENTS Test)
    if(Qt6Test_FOUND)
        add_executable(tetris_qt_app_tests tests/qt_app_tests.cpp)
        target_link_libraries(tetris_qt_app_tests PRIVATE tetris_qt_widgets Qt6::Test)
        add_test(NAME tetris_qt_app_tests COMMAND tetris_qt_app_tests)
        set_tests_properties(tetris_qt_app_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()

if(TARGET tetris_py)
    add_test(NAME tetris_python_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/python_bindings_tests.py)
    set_tests_properties(tetris_python_tests PROPERTIES
        ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:tetris_py>")
endif()
