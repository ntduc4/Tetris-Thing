.DEFAULT_GOAL := build
.PHONY: help configure build test python test-python ai cli run-cli qt run-qt clean

BUILD_DIR := build
PYTHON ?= python3

help:
	@printf '%s\n' 'make build        Build the engine, CLI, and available optional targets' \
		'make test         Build and run all available tests' \
		'make python       Build the tetris_py module' \
		'make test-python  Build and test the Python module' \
		'make ai           Build tetris_py and run ../tetris_ai/main.py' \
		'make run-cli      Build and launch the interactive CLI' \
		'make run-qt       Build and launch the Qt app (requires Qt 6)' \
		'make clean        Remove the build directory'

configure:
	cmake -S . -B $(BUILD_DIR)

build: configure
	cmake --build $(BUILD_DIR)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

python:
	cmake -S . -B $(BUILD_DIR) -DTETRIS_BUILD_PYTHON=ON -DPython3_EXECUTABLE="$(shell $(PYTHON) -c 'import sys; print(sys.executable)')"
	cmake --build $(BUILD_DIR) --target tetris_py

test-python: python
	ctest --test-dir $(BUILD_DIR) -R tetris_python_tests --output-on-failure

ai: python
	PYTHONPATH="$(abspath $(BUILD_DIR))$${PYTHONPATH:+:$${PYTHONPATH}}" $(PYTHON) ../tetris_ai/main.py

cli: configure
	cmake --build $(BUILD_DIR) --target tetris_cli

run-cli: cli
	./$(BUILD_DIR)/tetris_cli

qt:
	cmake -S . -B $(BUILD_DIR) -DTETRIS_BUILD_QT=ON
	cmake --build $(BUILD_DIR) --target tetris_qt

run-qt: qt
	./$(BUILD_DIR)/tetris_qt

clean:
	rm -rf $(BUILD_DIR)
