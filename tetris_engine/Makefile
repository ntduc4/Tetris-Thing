.DEFAULT_GOAL := build
.PHONY: help configure build test python test-python ai-setup ai-check ai cli run-cli qt run-qt clean

BUILD_DIR := build
BUILD_TYPE ?= Release
PYTHON ?= python3

help:
	@printf '%s\n' 'make build        Build the engine, CLI, and available optional targets' \
		'make test         Build and run all available tests' \
		'make python       Build the tetris_py module' \
		'make test-python  Build and test the Python module' \
		'make ai-setup     Install the training environment, build tetris_py, and check ROCm' \
		'make ai-check     Check ROCm, PyTorch, marimo, and the engine binding' \
		'make ai           Open the training marimo notebook (after ai-setup)' \
		'make run-cli      Build and launch the interactive CLI' \
		'make run-qt       Build and launch the Qt app (requires Qt 6)' \
		'make clean        Remove the build directory'

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

python:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DTETRIS_BUILD_PYTHON=ON -DPython3_EXECUTABLE="$(shell $(PYTHON) -c 'import sys; print(sys.executable)')"
	cmake --build $(BUILD_DIR) --target tetris_py

test-python: python
	ctest --test-dir $(BUILD_DIR) -R tetris_python_tests --output-on-failure

ai-setup:
	poetry -C ../tetris_ai install --no-root
	$(MAKE) python PYTHON="$$(poetry -C ../tetris_ai env info --executable)"
	$(MAKE) ai-check

ai-check:
	PYTHONPATH="$(abspath $(BUILD_DIR))$${PYTHONPATH:+:$${PYTHONPATH}}" poetry -C ../tetris_ai run python ../tetris_ai/check_env.py

ai: ai-check
	PYTHONPATH="$(abspath $(BUILD_DIR))$${PYTHONPATH:+:$${PYTHONPATH}}" poetry -C ../tetris_ai run marimo edit ../tetris_ai/train.py

cli: configure
	cmake --build $(BUILD_DIR) --target tetris_cli

run-cli: cli
	./$(BUILD_DIR)/tetris_cli

qt:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DTETRIS_BUILD_QT=ON
	cmake --build $(BUILD_DIR) --target tetris_qt

run-qt: qt
	./$(BUILD_DIR)/tetris_qt

clean:
	rm -rf $(BUILD_DIR)
