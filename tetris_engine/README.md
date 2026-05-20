# Tetris Engine

This directory contains the core board and piece model plus the public engine API for a single-player Tetris runtime.

## Current Status

- [x] `core/Board.hpp` and `src/core/Board.cpp`
- [x] `core/Piece.hpp` and `src/core/Piece.cpp`
- [x] Basic tests for current board and piece behavior
- [x] Public engine-facing headers for movement, rotation, RNG, spin, score, attack, and engine orchestration
- [ ] `Engine` runtime logic
- [ ] Movement helper implementations
- [ ] Concrete rotation systems such as `SRS`
- [ ] Concrete randomizers such as `SevenBag`
- [ ] Concrete spin detection
- [ ] Concrete scoring rules
- [ ] Concrete attack tables
- [ ] Multiplayer match orchestration

## What Needs To Be Done

1. Implement movement helpers declared in `include/engine/Movement.hpp`.
2. Implement `RotationSystem` behavior in `include/core/Rotation.hpp`.
3. Implement concrete randomizers in `include/engine/RNG.hpp`.
4. Implement concrete spin detectors in `include/engine/Spin.hpp`.
5. Implement concrete scoring rules in `include/engine/Score.hpp`.
6. Implement concrete attack rules in `include/engine/Attack.hpp`.
7. Add `Engine.cpp` and wire the single-player loop:
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
