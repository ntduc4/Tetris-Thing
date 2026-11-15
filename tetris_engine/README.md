# Tetris Engine

Headless Tetris engine with configurable boards, gravity, lock delay, bags, spin detection, score/attack tables, garbage, solo goals, and local multiplayer. AI placement generation is intentionally left unimplemented.

## Build and play

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/tetris_qt
```

The Qt 6 Widgets app is built when Qt 6 is available (`-DTETRIS_BUILD_QT=OFF` to skip it). Select **Zen**, **Sprint**, **Blitz**, **Custom / Marathon**, or **Local room (2P)**. **Rules…** edits solo and room rules separately; applying them restarts that game. **Keybinds…** changes both players' keys.

Default P1: **A/D/S** move, **W/Q/E** rotate, **C** hold, **Space** drop. Default P2: **←/→/↓** move, **↑/J/K** rotate, **L** hold, **Enter** drop. In solo play, **F** toggles draw mode; select a cell, left-drag to paint, right-drag to erase. **Next round** continues a local match.

The CLI remains available: `./build/tetris_cli --help`. Use `--ticks 600` for a headless smoke test. Both clients run at 60 ticks per second. TETR.IO-like presets are approximations, not verified seasonal rules or networked rooms.
