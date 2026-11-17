# Tetris Engine

Headless Tetris engine with configurable boards, gravity, lock delay, bags, spin detection, score/attack tables, garbage, solo goals, local multiplayer, and turn-based AI placement search.

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

`make python` builds the module and `make test-python` tests it. From the repository root, `make -C tetris_engine ai-setup` installs the Poetry training dependencies, builds the binding for that interpreter and checks ROCm; `make -C tetris_engine ai` opens the marimo notebook. `make run-cli` and `make run-qt` launch the other clients.

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

`Engine` exposes actions, board/queue snapshots, garbage, and rule configuration; `SoloSession` and `Match` expose seeded starts and automatic spawns. Board row 0 is the bottom.

For turn-based AI decisions, `generate_active_piece_placement()` and `generate_hold_piece_placement()` return reachable, replayable `Placement` objects (hold flag, final pose, spin context, and movement path). `apply_placement(action)` validates and locks one, returning `LockResult` with lines, spin, attack and `score_delta`; it does **not** spawn the next piece. The search uses zero ticks: gravity, garbage travel and input timing are outside this MVP.

To examine outcomes without changing the live game, call `snapshot = game.clone()` once, `working = game.clone()` once, then for each candidate `working.restore(snapshot)` and `result = working.apply_placement(candidate)`. Restore copies board/RNG/queue/state into the existing engine without reallocating rule systems; it only accepts a clone of the same rule context. Post-lock `working.board` and `result` can feed a later observation encoder. Do not supply snapshot RNG or future unseen pieces to the policy; spawning on a seeded clone reveals hidden information. Custom randomizers need `clone()` and `restore_from()` to support this API; custom rule strategies need `clone()`.
