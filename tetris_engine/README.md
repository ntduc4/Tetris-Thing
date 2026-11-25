# Tetris Engine

Headless Tetris engine with configurable boards, gravity, lock delay, bags, spin detection, score/attack tables, garbage, solo goals, local multiplayer, and turn-based AI placement search.

## Build and play

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/tetris_qt
```

The Qt 6 Widgets app is built when Qt 6 Widgets and Concurrent are available (`-DTETRIS_BUILD_QT=OFF` to skip it). Each board shows hold, five 4×4 previews, and side B2B/combo indicators; Season 2-like attacks also show surge charge. Select **Zen**, **Sprint**, **Blitz**, **Custom / Marathon**, **Local room (2P)**, or **Turn-based vs bot**. **Rules…** edits solo and room rules separately, including **SRS+** or **SRS-X** rotation; applying rules restarts that game. **Keybinds…** changes both players' keys. Accepted rules and keybinds save automatically and reload on launch.

Default P1: **A/D/S** move, **W/Q/E** rotate, **C** hold, **Space** drop. Default P2: **←/→/↓** move, **↑/J/K** rotate, **L** hold, **Enter** drop. In solo play, **F** toggles draw mode; select a cell, left-drag to paint, right-drag to erase. **Next round** continues a local match.

**Spin setup…** opens a separate template editor, leaving the current game intact. Choose an n×m box (1–10 columns by 1–20 bottom rows), paint only inside it, and click each row's **F/O** label to select **filled** (garbage outside the box) or **overhang** (empty outside). Pick the active piece, optional starting hold and next-piece sequence, then **Save JSON…**; **Open JSON…** edits saved templates and imports older fixed-board setups. On an opt-in AI reset, the engine selects a seeded horizontal position and 50/50 mirror; a mirror also swaps **S/Z** and **J/L** painted cells and scripted active, hold and next pieces (but not the subsequent seeded 7-bag). The usual active spawn column is unchanged. The upper 20 hidden rows remain empty. In `tetris_ai/train.py`, **Preview a Qt spin setup** loads JSON into an independent `PlacementEnv` for inspection; `load_spin_setup(path)` and `PlacementEnv.reset(seed, spin_setup=setup)` provide the same opt-in reset from Python. To mix openings, pass explicitly loaded setups and a probability, e.g. `PlacementEnv(spin_setups=(setup,), spin_chance=0.5, clean_garbage_layers=(1, 2), clean_garbage_chance=0.2, cheese_garbage_layers=(1, 3), cheese_garbage_chance=0.25)`. Python independently decides whether to include clean or cheese garbage, then picks each included type's layer count from its inclusive range; the default chance of 1 preserves prior behavior. Set the minimum to 1 if the chance should be the exact probability of that garbage type appearing. Python also chooses the template per seed; `reset(seed, top_is_spin=True/False)` overrides spin selection. Clean garbage shares a hole column, cheese garbage independently chooses a hole in each row, and any spin template is placed **above** the garbage. Opening rows must fit within 20 visible rows. The engine generates the chosen board and mirrored scripted pieces; ordinary training/evaluation, rewards and saved checkpoints are unchanged until the environment is explicitly configured.

Qt keyboard handling keeps charged left/right DAS and the ARR interval across a hard drop, natural lock, or successful hold: the next piece shifts immediately in the held direction (to the wall when ARR is zero), without waiting for DAS again. Pressing the opposite direction or releasing either of two held directions starts a fresh delay for the active direction. Soft drop remains held across pieces; rotations and hard drop do not auto-repeat. DAS/ARR are still configurable under **Rules…**; the modern-looking playfield and existing TETR.IO-style rule defaults are unchanged.

In **Turn-based vs bot**, choose **MisaMino bot** or **Latest AI** from the opponent selector. P1 uses the usual keys and the opponent places exactly one piece after each P1 hard drop; neither board falls on a timer. MisaMino uses its existing C++ placement search (no Python or checkpoint required) on a selectable 40- or 10-row board. Choose the same board height as the current model checkpoint when comparing opponents; neither option fixes the known TETR.IO spawn-height difference. Matches have 10 columns, zero gravity, attacks, and wins; local room settings do not alter them. The latest AI loads `tetris_ai/checkpoints/encoding_v3/shared/latest.pt` on entry, Restart, and Next round, using the board height encoded in that checkpoint; changing the height starts a fresh match. Install the training Poetry environment (`poetry -C ../tetris_ai install --no-root` from this directory), then run `make run-qt`. AI inference runs on CPU in a separate process, using only the bot board, active/held pieces, five previews, incoming garbage total, and engine-generated legal placements. Missing or incompatible models produce an on-screen error instead of silently substituting another bot. Set `TETRIS_MODEL_PYTHON` to a Python interpreter with PyTorch and NumPy to run without Poetry; `TETRIS_MODEL_CHECKPOINT` overrides the checkpoint path.

Each playfield shows an incoming-garbage meter on its left. The bar fills from the bottom to 20 lines; more than 20 lines turns it red. The status beneath the board shows the exact pending line count, including when the bar is full.

The CLI remains available: `./build/tetris_cli --help`. Use `--ticks 600` for a headless smoke test. Both clients run at 60 ticks per second. Incoming garbage waits through line clears and rises at most eight lines per non-clearing placement (TETR.IO-like Qt defaults use 20 travel ticks). Room garbage holes use a shared round seed, not a column march.

The Season 2 1v1 attack rule uses [TETR.IO's multiplier](https://tetris.wiki/TETR.IO#Combo_System:_%22Multiplier%22): round down, use logarithmic combo attack for zero-base clears from combo x2, add B2B after combo scaling, and add a flat +1 for quads/spins that clear garbage. An All Clear sends an unmultiplied five-line packet in addition to its line-clear attack; surge releases in three packets. Other timing, blocking, and room details remain approximations, not verified TETR.IO parity.

**Guideline** attack now follows the supplied *2009 Tetris Design Guideline*, Appendix B: normal clears send 0/1/2/4, T-spin clears send 2/4/6, and qualifying B2B attacks add one line (a mini single still sends zero). The 2009 table has no combo or all-clear attack bonus. This changes the existing Guideline attack choice; Season 1/2 and custom attacks are unchanged. Qt's saved numeric rule selection retains its original index.

For the full engine-side 2009 profile, use `./build/tetris_cli --ruleset guideline-2009` (optionally `--level 1..15`), or set `EngineConfig.guideline_2009 = True` in Python. The profile requires a 10×40 board (20 visible rows and 20 buffer rows), uses a 7-bag, 1–6 previews, five-point 90° SRS without a 180 input, T-only spins, level-dependent 2009 scores, immediate post-generation descent, Block Out / Lock Out / Top Out, and extended placement lock-down. In Guideline multiplayer matches, incoming garbage counters first; remaining ready lines rise after every lock, and a new random hole is chosen after each eight lines *actually risen*. Untargeted attacks reach all opponents; `Match.set_target` selects one opponent instead. Explicit `receive_garbage(lines, hole)` still accepts a chosen hole for fixtures. Guideline Marathon in `SoloSession` uses fixed ten-line level goals through level 15; Sprint and Blitz retain their separate objectives. TETR.IO defaults and bot-training configurations do not opt into this profile. UI presentation, music, input auto-repeat, and platform-dependent generation delays/animations are not implemented by the headless engine.

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

For Guideline play:

```python
cfg = t.EngineConfig()
cfg.guideline_2009 = True
cfg.level = 1
game = t.Engine(cfg)
game.reset(42)
game.spawn_next_piece()
```

Setting `cfg.attack = t.AttackRule.Guideline` alone selects **only** the 2009
multiplayer attack table, without replacing the other rule subsystems.

`Engine` exposes actions, board/queue snapshots, garbage, and rule configuration; `SoloSession` and `Match` expose seeded starts and automatic spawns. Board row 0 is the bottom.

For turn-based AI decisions, `generate_active_piece_placement()` and `generate_hold_piece_placement()` return reachable, replayable `Placement` objects (hold flag, final pose, spin context, movement path, and engine-classified `spin`: `None`, `Mini`, or `Full`). This classification is for the board and rules at generation time; `LockResult.spin` remains authoritative when the action is actually locked. `apply_placement(action)` validates and locks one, returning `LockResult` with lines, spin, attack and `score_delta`; it does **not** spawn the next piece. The search uses zero ticks: gravity, garbage travel and input timing are outside this MVP.

To examine outcomes without changing the live game, call `snapshot = game.clone()` once, `working = game.clone()` once, then for each candidate `working.restore(snapshot)` and `result = working.apply_placement(candidate)`. Restore copies board/RNG/queue/state into the existing engine without reallocating rule systems; it only accepts a clone of the same rule context. Post-lock `working.board` and `result` can feed a later observation encoder. Do not supply snapshot RNG or future unseen pieces to the policy; spawning on a seeded clone reveals hidden information. Custom randomizers need `clone()` and `restore_from()` to support this API; custom rule strategies need `clone()`.

## C++ MisaMino-inspired teacher

`tetris_bot` is a separate CMake library; `Engine` does not own or depend on the bot. Link `tetris_bot` and include `bot/MisaMinoBot.hpp`:

```cpp
tetris::MisaMinoBot bot({.search_depth = 3, .beam_width = 12,
                         .node_budget = 4000, .search_threads = 4});
if (auto placement = bot.choose(game)) {
  auto lock = game.apply_placement(*placement);
  // Record placement, lock.spin and lock.attack_sent for demonstrations.
  if (!game.game_over())
    game.spawn_next_piece(); // As with any direct Engine placement.
}
```

For an entire teacher-played episode, call `bot.play_episode(game, piece_cap)` on
an engine that has been reset (it spawns the first piece if necessary). It
plays through `apply_placement()` and `spawn_next_piece()`, mutates `game`, and
returns placements and lock results for each turn, plus game-over/truncation
flags. The bot reuses engine placement searches from overlapping states in
the previous turn's beam, then recomputes beam rankings at the new root:
this preserves `choose()`'s moves rather than carrying a pruned beam that
could change the teacher's decisions. `reused_searches` counts cache hits;
the cache is limited to one episode and only the previous turn's beam states.

```cpp
game.reset(42);
auto episode = bot.play_episode(game, 500);
for (const auto &turn : episode.turns) {
  // turn.placement is the replayable path; turn.lock is the engine result.
}
```

The optional `tetris_py` module also exposes `MisaMinoConfig` and `MisaMinoBot` (linking `tetris_bot`). In the `tetris_ai/train.py` notebook:

```python
actions = env.legal_placements()
teacher_action = env.misamino_choice()  # Or pass a configured tetris_py.MisaMinoBot.
teacher_index = actions.index(teacher_action)
observation, reward, terminated, truncated, info = env.step(teacher_action)
```

`misamino_choice()` matches the C++ bot's result by its entire path and spin context and returns the **same Python object** as the cached legal action required by `step()`. `teacher_action.spin` exposes the classification before stepping; the existing v3 model does not encode it yet. It raises if the game is over or the choice cannot be matched; it does not train a model.

Search uses up to twelve CPU workers by default (bounded by available cores and beam parents). The workers independently generate and score beam parents; a deterministic merge preserves the same global beam choices as one worker. In Python, set `config = tetris_py.MisaMinoConfig(); config.search_threads = 1; bot = tetris_py.MisaMinoBot(config)` if running enough independent environments to oversubscribe CPU cores; benchmark the inner and outer worker counts for your machine. The Python `choose()` binding releases the GIL, but a single training loop still runs sequentially unless episodes are scheduled concurrently.

The notebook has Q-learning, Monte Carlo, and behavioural-cloning training buttons that share `tetris_ai/checkpoints/encoding_v3/shared/latest.pt`. On the first click, the most recently saved legacy Q or BC checkpoint supplies the weights; after that, each button resumes the shared model and trains with its own method. The optimizer resumes only when the checkpoint was saved by the same method; switching methods starts a fresh optimizer. Cloning trains with the teacher action's index as the cross-entropy label and steps the environment with that action.

Every 100 **total** episodes, a greedy evaluation on three fixed seeds records score, cleared lines, attack, pieces survived, piece-cap rate, spin and perfect clears, and stack height. The metrics are embedded in `shared/latest.pt` and appended to `shared/evaluations.jsonl`; the final partial batch also updates `latest.pt`. Every 1,000 total episodes, an immutable model checkpoint with those metrics is added under `shared/models/`. Existing checkpoints are not overwritten. Survival is measured in placements rather than seconds because solo training has zero gravity. The separate replay still shows one greedy evaluation game.

This is an **independent, MisaMino-inspired adaptation**, not the original binary or a verified move-for-move port. It follows the [original main bot's](https://github.com/misakamm/MisaMino/blob/3c6ebb03c62dd7e48099d0d1dc33bafcb718f62b/tetris_ai/tetris_ai.cpp) bounded beam-search idea: score post-lock buried versus two-column side-accessible holes (including their depth and consecutive gaps), height/roughness and transitions; consider hold, visible next pieces, B2B/combo, attack efficiency, spin clears and potential T cavities; deduplicate continuation states. Default limits are **three total placements, beam width 12, 4,000 simulated placements**; effective depth is capped so empty-hold and non-hold branches have enough visible pieces to reach the same horizon. All root actions are evaluated even if they exceed the budget; an incomplete future layer is discarded. Weights are adapted for this engine (including its configurable training board height), not copied from MisaMino's TOJ/22-row settings. The upstream search also uses different beam widths, evaluation features and deeper configurations; tune and compare outcomes before calling this a strength-matched baseline.

Every candidate is generated by this engine's reachability search and scored with **this engine's `LockResult`**. A grounded spin and a moving-drop non-spin with the same landing cells remain separate actions. `choose(const Engine&)` never mutates the supplied engine and returns an executable path, including the hold flag. Lookahead consumes only the preview captured at the decision boundary; an empty hold cannot draw an unseen piece. When garbage is pending, search stops at one ply and scores the pre-rise board reconstructed from public landing cells so secret packet holes cannot affect the choice. This is a turn-based search: it does not model real-time key timing or predict unobserved pieces/garbage holes.

The upstream repository at commit `3c6ebb03c62dd7e48099d0d1dc33bafcb718f62b` has a [LICENSE](https://github.com/misakamm/MisaMino/blob/3c6ebb03c62dd7e48099d0d1dc33bafcb718f62b/LICENSE) containing only “Copyleft by Misakamm”; it does not specify reuse or redistribution terms. Copyleft still relies on copyright. **No upstream files or copied source are included in this target.** The original uses different kicks, spin and attack rules, spawn/hidden rows, garbage timing and search weights, so parity has not been established. The notebook's v3 encoder does not distinguish every spin/non-spin path with the same landing cells, limiting how faithfully cloning can imitate the teacher.

The engine's reachability search keeps the same engine-generated movement paths but avoids allocating a shape vector on every collision and copying paths at every intermediate search state. On one seeded opening position in a Release build, `MisaMinoBot::choose` went from approximately **0.16 s to 0.08 s on a 40-row board** (about **0.06 s to 0.03 s on 10 rows**); timings vary with hardware and board state. The bot evaluates the final search layer without materializing/sorting unused candidates. These optimizations do not use hidden future pieces or change the engine's placement rules.

The bot now computes candidate deduplication keys only for contenders, scans board occupancy once per scored position, and reuses a landing for each distinct piece pose during placement generation. Drop/replay validation no longer repeats the same landing search. A small Release benchmark over 12 seeded empty openings on this development machine measured median `choose()` latency of roughly **77 → 22 ms (40 rows)** and **28 → 9 ms (10 rows)**, comparing the prior one-worker search with the new four-worker default. One worker after the C++ optimizations took roughly **62 ms (40 rows)** and **25 ms (10 rows)**. On 32 independent 40-row openings, one serial worker delivered about **16 choices/s** versus **143 choices/s** with four concurrent calls using four search workers each (24 CPU cores available). These are not whole-episode training throughput measurements; thread counts and other workloads affect the results.
