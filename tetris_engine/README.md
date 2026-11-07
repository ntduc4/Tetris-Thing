# Tetris Engine

This directory contains a headless Tetris engine intended for single-player, multiplayer, and AI move search.

## Current Status

- [x] `core/Board.hpp` and `src/core/Board.cpp`
- [x] `core/Piece.hpp` and `src/core/Piece.cpp`
- [x] Basic tests for current board and piece behavior
- [x] Public engine-facing headers for movement, rotation, RNG, spin, score, attack, and engine orchestration
- [ ] `Engine` runtime logic
    - [x] Basic spawn, hold, movement, kick-based rotation, ghost, and hard drop
    - [x] Basic locking and line clears
    - [ ] Board and active-piece rendering
    - [ ] Complete gravity, speed increase, and lock delay
    - [ ] Spin detection, scoring, and attack calculation using the provided rule systems
    - [ ] Garbage receive, cancellation, and send
    - [ ] Game-over/top-out handling and full reset
    - [ ] BFS reachable placements (including hold and kicks) for AI evaluation
- [ ] Single-player modes (Sprint, Blitz, Marathon)
- [ ] Configurable input behavior (DAS/ARR, soft drop speed)
- [ ] Concrete rotation systems such as `SRS`
- [ ] Concrete randomizers such as `SevenBag`
- [ ] Concrete spin detection
- [ ] Concrete scoring rules
- [ ] Concrete attack tables
- [ ] Multiplayer match orchestration (garbage routing, KOs, rounds)

## What Needs To Be Done

1. Finish movement and input behavior.
2. Implement concrete rotation systems in `include/engine/Rotation.hpp`.
3. Implement concrete randomizers in `include/engine/RNG.hpp`.
4. Implement concrete spin detectors in `include/engine/Spin.hpp`.
5. Implement concrete scoring rules in `include/engine/Score.hpp`.
6. Implement concrete attack rules in `include/engine/Attack.hpp`.
7. Finish `Engine.cpp` and wire the single-player loop:
   - spawn
   - hold
   - move and rotate
   - gravity and lock
   - line clear resolution
   - score and attack calculation
   - garbage receive
   - game-over handling
8. Extend tests to cover engine behavior once implementations exist.
9. Add match-level multiplayer support later in a separate API layer.

## Suggested Build Check

From `tetris_engine/`:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
