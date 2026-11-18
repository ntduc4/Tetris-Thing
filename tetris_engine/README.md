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

## C++ MisaMino-inspired teacher

`tetris_bot` is a separate CMake library; `Engine` does not own or depend on the bot. Link `tetris_bot` and include `bot/MisaMinoBot.hpp`:

```cpp
tetris::MisaMinoBot bot({.search_depth = 3, .beam_width = 12,
                         .node_budget = 4000});
if (auto placement = bot.choose(game)) {
  auto lock = game.apply_placement(*placement);
  // Record placement, lock.spin and lock.attack_sent for demonstrations.
  if (!game.game_over())
    game.spawn_next_piece(); // As with any direct Engine placement.
}
```

The optional `tetris_py` module also exposes `MisaMinoConfig` and `MisaMinoBot` (linking `tetris_bot`). In the `tetris_ai/train.py` notebook:

```python
actions = env.legal_placements()
teacher_action = env.misamino_choice()  # Or pass a configured tetris_py.MisaMinoBot.
teacher_index = actions.index(teacher_action)
observation, reward, terminated, truncated, info = env.step(teacher_action)
```

`misamino_choice()` matches the C++ bot's result by its entire path and spin context and returns the **same Python object** as the cached legal action required by `step()`. It raises if the game is over or the choice cannot be matched; it does not train a model.

The notebook has Q-learning and behavioural-cloning training buttons that share `tetris_ai/checkpoints/encoding_v3/shared/latest.pt` and its versioned models and evaluation replay. On the first click, the most recently saved legacy Q or BC checkpoint supplies the weights; after that, each button resumes the shared model and trains with its own method. The optimizer resumes only when the checkpoint was saved by the same method; switching methods starts a fresh optimizer. Original checkpoints remain on disk. Cloning trains with the teacher action's index as the cross-entropy label and steps the environment with that action.

This is an **independent, MisaMino-inspired adaptation**, not the original binary or a verified move-for-move port. It follows the [original main bot's](https://github.com/misakamm/MisaMino/blob/3c6ebb03c62dd7e48099d0d1dc33bafcb718f62b/tetris_ai/tetris_ai.cpp) bounded beam-search idea: score post-lock board holes/open holes, height/roughness and transitions; consider hold, visible next pieces, B2B/combo, attack efficiency, spin clears and potential T cavities; deduplicate continuation states. Default limits are **three total placements, beam width 12, 4,000 simulated placements**; effective depth is capped so empty-hold and non-hold branches have enough visible pieces to reach the same horizon. All root actions are evaluated even if they exceed the budget; an incomplete future layer is discarded. Weights are adapted for this engine (including its 10-row training board), not copied from MisaMino's TOJ/22-row settings. Tune/search-evaluate them before calling this a strength-matched baseline.

Every candidate is generated by this engine's reachability search and scored with **this engine's `LockResult`**. A grounded spin and a moving-drop non-spin with the same landing cells remain separate actions. `choose(const Engine&)` never mutates the supplied engine and returns an executable path, including the hold flag. Lookahead consumes only the preview captured at the decision boundary; an empty hold cannot draw an unseen piece. When garbage is pending, search stops at one ply and scores the pre-rise board reconstructed from public landing cells so secret packet holes cannot affect the choice. This is a turn-based search: it does not model real-time key timing or predict unobserved pieces/garbage holes.

The upstream repository at commit `3c6ebb03c62dd7e48099d0d1dc33bafcb718f62b` has a `LICENSE` containing only “Copyleft by Misakamm”; redistribution permissions for its source are unclear. **No upstream files are included in this target.** The original uses different kicks, spin and attack rules, spawn/hidden rows, garbage timing and search weights, so parity has not been established. The notebook's v3 encoder does not distinguish every spin/non-spin path with the same landing cells, limiting how faithfully cloning can imitate the teacher.
