# Tetris Engine

Headless Tetris engine with configurable boards, gravity, lock delay, bags, spin detection, score/attack tables, garbage, solo goals, and local multiplayer. AI placement generation is intentionally left unimplemented.

## Build and play

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/tetris_qt
```

The Qt 6 Widgets app is built when Qt 6 is available (`-DTETRIS_BUILD_QT=OFF` to skip it). Each board shows hold, five 4×4 previews, and side B2B/combo indicators; Season 2-like attacks also show surge charge. Select **Zen**, **Sprint**, **Blitz**, **Custom / Marathon**, or **Local room (2P)**. **Rules…** edits solo and room rules separately, including **SRS+** or **SRS-X** rotation; applying rules restarts that game. **Keybinds…** changes both players' keys. Accepted rules and keybinds save automatically and reload on launch.

Default P1: **A/D/S** move, **W/Q/E** rotate, **C** hold, **Space** drop. Default P2: **←/→/↓** move, **↑/J/K** rotate, **L** hold, **Enter** drop. In solo play, **F** toggles draw mode; select a cell, left-drag to paint, right-drag to erase. **Next round** continues a local match.

The CLI remains available: `./build/tetris_cli --help`. Use `--ticks 600` for a headless smoke test. Both clients run at 60 ticks per second. Incoming garbage waits through line clears and rises at most eight lines per non-clearing placement (TETR.IO-like Qt defaults use 20 travel ticks). Room garbage holes use a shared round seed, not a column march. Presets remain approximations, not verified seasonal rules or networked rooms; Season 2-like surge releases in three packets.

## Python

Install `pybind11` for the same Python interpreter used by CMake (`python3 -m pip install pybind11`), then build normally. The optional `tetris_py` module has no Qt dependency (`-DTETRIS_BUILD_PYTHON=OFF` skips it). From this directory:

`make python` builds the module, `make test-python` tests it, and `make ai` builds it and runs `../tetris_ai/main.py` with the right `PYTHONPATH` (or use `make -C tetris_engine ai` from the repository root). `make run-cli` and `make run-qt` launch the other clients.

```sh
PYTHONPATH=build python3 - <<'PY'
import tetris_py as t
game = t.Engine(t.EngineConfig())
game.reset(42)
game.spawn_next_piece()
print(game.active_piece.piece.type, game.preview_queue())
print(game.hard_drop().attack_packets)
game.spawn_next_piece()  # Engine does not spawn automatically after a lock.
PY
```

`Engine` exposes actions, board/queue snapshots, garbage, and rule configuration; `SoloSession` and `Match` expose seeded starts and automatic spawns. Board row 0 is the bottom. AI placement methods remain unimplemented.
