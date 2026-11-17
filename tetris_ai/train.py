import marimo

__generated_with = "0.25.2"
app = marimo.App(width="medium")


@app.cell
def _():
    import torch
    import tetris_py

    return tetris_py, torch


@app.cell
def _(tetris_py, torch):
    from dataclasses import dataclass

    device = torch.device("cuda")  # ROCm uses PyTorch's cuda device API.
    if torch.version.hip is None or not torch.cuda.is_available():
        raise RuntimeError("ROCm GPU unavailable; run `make -C tetris_engine ai-setup`")

    @dataclass(frozen=True)
    class EnvCheckpoint:
        """Internal episode checkpoint; never use its hidden RNG as a model input."""

        engine: tetris_py.Engine
        seed: int
        pieces_placed: int
        done: bool

    class PlacementEnv:
        """Solo, zero-gravity environment; one engine placement per step.

        Select one tetris_py.Placement from legal_placements(), not a keypress.
        Board rows start at the bottom (including hidden rows). Checkpoints
        are for branching/rollouts, not for leaking unrevealed future pieces.
        """

        def __init__(self, piece_cap=500):
            if not isinstance(piece_cap, int) or piece_cap < 1:
                raise ValueError("piece_cap must be a positive integer")
            config = tetris_py.EngineConfig()
            config.width = 10
            config.height = 40
            config.preview_count = 5
            config.gravity = 0
            config.gravity_increase = 0
            config.bag = tetris_py.BagRule.Seven
            config.rotation = tetris_py.RotationRule.SRSPlus
            config.spin = tetris_py.SpinRule.AllMiniPlus
            config.attack = tetris_py.AttackRule.Off
            self._engine = tetris_py.Engine(config)
            self.piece_cap = piece_cap
            self._seed = None
            self.pieces_placed = 0
            self.done = True
            self._actions = None

        def reset(self, seed):
            if not isinstance(seed, int) or not 0 <= seed < 2**64:
                raise ValueError("seed must be an unsigned 64-bit integer")
            self._seed = seed
            self.pieces_placed = 0
            self.done = False
            self._actions = None
            self._engine.reset(seed)
            if not self._engine.spawn_next_piece():
                raise RuntimeError("Could not spawn the first piece")
            return self.observe()

        def checkpoint(self):
            if self._seed is None:
                raise RuntimeError("Call reset(seed) before taking a checkpoint")
            return EnvCheckpoint(self._engine.clone(), self._seed, self.pieces_placed, self.done)

        def restore(self, checkpoint):
            if not isinstance(checkpoint, EnvCheckpoint):
                raise TypeError("Expected an EnvCheckpoint from this environment")
            self._engine.restore(checkpoint.engine)
            self._seed = checkpoint.seed
            self.pieces_placed = checkpoint.pieces_placed
            self.done = checkpoint.done
            self._actions = None  # Reacquire placement objects after restore.
            return self.observe()

        def observe(self):
            if self._seed is None:
                raise RuntimeError("Call reset(seed) before observing")
            active = None if self.done else self._engine.active_piece
            hold = self._engine.hold_piece if active is not None else None
            return {
                "board": tuple(
                    tuple(int(cell) for cell in row) for row in self._engine.board.render()
                ),
                "active_piece": int(active.piece.type) if active is not None else None,
                "rotation": int(active.piece.rotation) if active is not None else None,
                "position": (active.pos.row, active.pos.col) if active is not None else None,
                "hold_piece": int(hold) if hold is not None else None,
                "can_hold": self._engine.can_hold if active is not None else False,
                "preview": tuple(int(piece) for piece in self._engine.preview_queue())
                if active is not None else (),
                "combo": self._engine.combo,
                "back_to_back": self._engine.back_to_back,
                "back_to_back_count": self._engine.back_to_back_count,
                "score": self._engine.score,
                "lines_cleared": self._engine.lines_cleared,
                "pieces_placed": self.pieces_placed,
            }

        def legal_placements(self):
            if self._seed is None:
                raise RuntimeError("Call reset(seed) before requesting placements")
            if self.done:
                return ()
            if self._actions is None:
                active = self._engine.generate_active_piece_placement()
                held = self._engine.generate_hold_piece_placement() if self._engine.can_hold else []
                self._actions = tuple(active + held)
            return self._actions

        def step(self, action):
            if self.done:
                raise RuntimeError("Episode ended; call reset(seed)")
            if not isinstance(action, tetris_py.Placement):
                raise TypeError("Select a Placement from legal_placements()")
            actions = self.legal_placements()
            if not any(action is candidate for candidate in actions):
                raise ValueError("Select a placement from this state's legal_placements()")
            lock = self._engine.apply_placement(action)
            if not lock.locked:
                raise RuntimeError("Engine did not lock the selected placement")
            if not self._engine.game_over:
                spawned = self._engine.spawn_next_piece()
                if not spawned and not self._engine.game_over:
                    raise RuntimeError("Engine failed to respawn after the placement")

            self.pieces_placed += 1
            self._actions = None
            terminated = self._engine.game_over
            truncated = not terminated and self.pieces_placed >= self.piece_cap
            self.done = terminated or truncated
            info = {
                "lines_cleared": lock.lines_cleared,
                "score_delta": lock.score_delta,
                "spin": int(lock.spin),
                "perfect_clear": lock.perfect_clear,
                "attack_sent": lock.attack_sent,
                "pieces_placed": self.pieces_placed,
                "top_out": terminated,
            }
            reward = float(lock.lines_cleared)  # Solo objective: lines per capped episode.
            return self.observe(), reward, terminated, truncated, info

    env = PlacementEnv()
    initial_observation = env.reset(seed=42)
    available_placements = env.legal_placements()
    (device, len(initial_observation["board"]), len(available_placements))
    return EnvCheckpoint, PlacementEnv, available_placements, device, env, initial_observation


if __name__ == "__main__":
    app.run()
