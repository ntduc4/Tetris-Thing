import marimo

__generated_with = "0.25.2"
app = marimo.App(width="medium")


@app.cell
def _():
    import marimo as mo
    import numpy as np
    from torch import torch, nn
    import tetris_py

    return mo, nn, np, tetris_py, torch


@app.cell
def _(tetris_py, torch):
    from dataclasses import dataclass

    BOARD_WIDTH = 10
    BOARD_HEIGHT = 10
    PREVIEW_COUNT = 5
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
            config.width = BOARD_WIDTH
            config.height = BOARD_HEIGHT
            config.preview_count = PREVIEW_COUNT
            config.gravity = 0
            config.gravity_increase = 0
            config.bag = tetris_py.BagRule.Seven
            config.rotation = tetris_py.RotationRule.SRSPlus
            config.spin = tetris_py.SpinRule.AllMiniPlus
            config.attack = tetris_py.AttackRule.Season2
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
            reward = lock.score_delta # Solo objective: lines per capped episode.
            return self.observe(), reward, terminated, truncated, info

    env = PlacementEnv()
    initial_observation = env.reset(seed=42)
    available_placements = env.legal_placements()
    (device, len(initial_observation["board"]), len(available_placements))
    return BOARD_HEIGHT, available_placements, device, env


@app.cell
def _(available_placements, mo):
    if not available_placements:
        raise RuntimeError("No placements available for the current game state")

    placement_index = mo.ui.slider(
        start=0,
        stop=len(available_placements) - 1,
        step=1,
        value=0,
        show_value=True,
        label=f"Placement index (0–{len(available_placements) - 1})",
    )
    show_hidden = mo.ui.checkbox(label="Show all rows (including hidden rows)")
    mo.hstack([placement_index, show_hidden], justify="start")
    return placement_index, show_hidden


@app.cell
def _(available_placements, env, mo, placement_index, show_hidden):
    placement = available_placements[int(placement_index.value)]
    before = env.observe()["board"]
    landing = {(cell.row, cell.col) for cell in placement.cells()}
    height, width = len(before), len(before[0])
    if len(landing) != 4 or any(
        not (0 <= row < height and 0 <= col < width) or before[row][col] != 0
        for row, col in landing
    ):
        raise RuntimeError("Selected placement overlaps the locked board or is out of bounds")

    # A clone is an engine checkpoint. Applying the placement here never
    # modifies env or spawns an unrevealed next piece.
    preview = env.checkpoint().engine
    active = preview.active_piece
    active_cells = {
        (active.pos.row + offset.row, active.pos.col + offset.col)
        for offset in active.piece.current_shape()
    } if active is not None else set()
    lock = preview.apply_placement(placement)
    if not lock.locked:
        raise RuntimeError("Engine did not lock the selected placement")
    after = tuple(tuple(int(cell) for cell in row) for row in preview.board.render())
    if env.observe()["board"] != before:
        raise RuntimeError("Placement preview modified the live environment")

    symbols = ".IOTSZJLGX"  # Empty, tetrominoes, garbage, unclearable.
    rows = height if show_hidden.value or any(row >= 20 for row, _ in landing) else min(20, height)

    def board_text(board, overlay=frozenset(), falling=frozenset()):
        lines = [f"   +{'-' * width}+"]
        for row in range(rows - 1, -1, -1):
            cells = "".join(
                "@" if (row, col) in overlay
                else "a" if (row, col) in falling
                else symbols[board[row][col]]
                for col in range(width)
            )
            lines.append(f"{row:02d} |{cells}|")
        lines.append(f"   +{'-' * width}+")
        lines.append(f"    {''.join(str(col % 10) for col in range(width))}")
        return "\n".join(lines)

    move_names = ", ".join(move.name for move in placement.moves) or "(hard drop immediately)"
    kick = placement.spin_context.kick_offset
    kick_text = "none" if kick is None else f"({kick.row}, {kick.col})"
    mo.vstack(
        [
            mo.md(
                f"**Placement {int(placement_index.value) + 1}/{len(available_placements)}** "
                f"— {placement.type.name}, hold: {placement.use_hold}, "
                f"rotation: {placement.final_rotation.name}, "
                f"root (row, col): ({placement.final_pos.row}, {placement.final_pos.col}), "
                f"drop distance: {placement.drop_distance}.  \n"
                f"Moves: {move_names}. Last movement: {placement.spin_context.last_movement.name}; "
                f"kick: {kick_text}.  \n"
                f"**Engine result:** lines {lock.lines_cleared}, score +{lock.score_delta}, "
                f"attack {lock.attack_sent}, spin {lock.spin.name}, "
                f"perfect clear: {lock.perfect_clear}."
            ),
            mo.hstack(
                [
                    mo.md(
                        "**Before: active (`a`), chosen landing (`@`)**\n```text\n"
                        + board_text(before, landing, active_cells)
                        + "\n```"
                    ),
                    mo.md("**After lock/line clear**\n```text\n" + board_text(after) + "\n```"),
                ],
                justify="start",
                widths="equal",
            ),
            mo.md(
                "`.` empty, letters locked cells (`G` garbage), `a` active piece, "
                "`@` chosen landing. Row 0 is at the bottom. "
                "The post-lock board includes line clears; no next piece is spawned."
            ),
        ]
    )
    return


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## AI model section

    ### Q function definition and input encoding
    """)
    return


@app.cell
def _(BOARD_HEIGHT, nn, np, tetris_py):
    class PlacementQFunction(nn.Module):
        def __init__(self, feature_dim):
            super().__init__()
            self.layers = nn.Sequential(
                nn.Linear(feature_dim, 256),
                nn.ReLU(),
                nn.Linear(256, 128),
                nn.ReLU(),
                nn.Linear(128, 1)
            )
        def forward(self, x):
            return self.layers(x).squeeze(-1)

    def encode(observation, placement: tetris_py.Placement):
        board = (np.asarray(observation["board"])!=0).astype(np.float32).ravel()
        # print(board)
        action = np.array([
            placement.final_pos.row / BOARD_HEIGHT,
            placement.final_pos.col / BOARD_HEIGHT,
            int(placement.final_rotation) / 4,
            float(placement.use_hold)
        ], dtype = np.float32)
        return np.concatenate((board, action))

    gamma = 0.99
    return PlacementQFunction, encode, gamma


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Choosing an action
    """)
    return


@app.cell
def _(PlacementQFunction, device, encode, env, np, torch):
    # import numpy as np

    observation = env.reset(seed=42)
    actions = env.legal_placements()

    feature_dim = len(encode(observation, actions[0]))
    print(feature_dim)

    q_online = PlacementQFunction(feature_dim).to(device)
    optimizer = torch.optim.Adam(q_online.parameters(), lr=1e-4)

    features = np.stack([encode(observation, a) for a in actions])
    print(features.shape)
    batch = torch.as_tensor(features, dtype=torch.float32, device=device)

    with torch.no_grad():
        scores = q_online(batch)
        chosen_index = int(scores.argmax().item())
        print(scores.shape, chosen_index)

    action = actions[chosen_index]
    return (
        action,
        chosen_index,
        feature_dim,
        observation,
        optimizer,
        q_online,
        scores,
    )


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Training step
    """)
    return


@app.cell
def _(
    action,
    chosen_index,
    device,
    encode,
    env,
    gamma,
    np,
    observation,
    optimizer,
    q_online,
    scores,
    torch,
):
    import torch.nn.functional as F

    # Batch dimension
    current_input = torch.as_tensor(
        encode(observation, action), dtype=torch.float32, device=device
    ).unsqueeze(0)

    next_observation, reward, terminated, truncated, info = env.step(action)
    prediction = q_online(current_input)[0] # [0] for turning [1] -> scalar
                                            # Without no_grad to keep gradient

    with torch.no_grad():
        if terminated or truncated:
            target = torch.tensor(reward, dtype=torch.float32, device=device)
        else:
            next_actions = env.legal_placements()
            next_features = np.stack([encode(next_observation, a) for a in next_actions])
            next_batch = torch.as_tensor(next_features, dtype=torch.float32, device=device)
            target = reward + gamma * q_online(next_batch).max()
            print(scores.shape, chosen_index)

    loss = F.smooth_l1_loss(prediction, target)
    optimizer.zero_grad(set_to_none=True)
    loss.backward()
    optimizer.step()
    return (F,)


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Save and load model
    """)
    return


@app.cell
def _(PlacementQFunction, device, feature_dim, q_online, torch):
    torch.save(
        {
            "model": q_online.state_dict(),
            "feature_dim": feature_dim,
            "encoding_version": 1,
        },
        "q.pt",
    )

    saved = torch.load("q.pt", map_location=device, weights_only=True)
    loaded_model = PlacementQFunction(saved["feature_dim"]).to(device)
    loaded_model.load_state_dict(saved["model"])
    loaded_model.eval()
    return


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Greedy evaluation helper
    """)
    return


@app.cell
def _(device, encode, env, np, torch):
    def evaluate_episode(model, seed=987654321):
        """Record a greedy game without modifying the training environment."""
        eval_env = type(env)(piece_cap=env.piece_cap)  # Independent of training.
        observation = eval_env.reset(seed)
        frames = []
        was_training = model.training
        model.eval()
        try:
            with torch.no_grad():
                while True:
                    actions = eval_env.legal_placements()
                    features = np.stack([encode(observation, a) for a in actions])
                    batch = torch.as_tensor(features, dtype=torch.float32, device=device)
                    action = actions[int(model(batch).argmax().item())]

                    next_observation, reward, terminated, truncated, info = eval_env.step(action)
                    frames.append({
                        "before": observation["board"],
                        "landing": [[cell.row, cell.col] for cell in action.cells()],
                        "after": next_observation["board"],
                        "reward": reward,
                        "score": next_observation["score"],
                        "lines": info["lines_cleared"],
                        "moves": [move.name for move in action.moves],
                        "piece": action.type.name,
                        "use_hold": action.use_hold,
                        "rotation": action.final_rotation.name,
                    })
                    observation = next_observation
                    if terminated or truncated:
                        break
        finally:
            model.train(was_training)
        return {"seed": seed, "frames": frames}

    return (evaluate_episode,)


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Full episode training
    """)
    return


@app.cell
def _(F, device, encode, gamma, nn, np, torch):
    import random

    rng = random.Random(6789)

    def train_an_episode_q_learning(
        seed: int,
        q_online: nn.Module,
        optimizer: torch.optim.Adam,
        training_env,
        epsilon: float = 0.05,
    ):
        observation = training_env.reset(seed)
        while True:
            # Current action
            actions = training_env.legal_placements()
            features = np.stack([encode(observation, a) for a in actions])
            batch = torch.as_tensor(features, dtype=torch.float32, device=device)
            with torch.no_grad():
                scores = q_online(batch)
            chosen_index = rng.randrange(len(actions)) if rng.random() < epsilon else int(scores.argmax().item())
            action = actions[chosen_index]
            current_input = torch.as_tensor(
                encode(observation, action), dtype=torch.float32, device=device
            ).unsqueeze(0)

            # Applying action
            next_observation, reward, terminated, truncated, info = training_env.step(action)
            prediction = q_online(current_input)[0]
            observation = next_observation

            # Q learning target update
            with torch.no_grad():
                if terminated or truncated:
                    target = torch.tensor(reward, dtype=torch.float32, device=device)
                else:
                    next_actions = training_env.legal_placements()
                    next_features = np.stack([encode(next_observation, a) for a in next_actions])
                    next_batch = torch.as_tensor(next_features, dtype=torch.float32, device=device)
                    target = reward + gamma * q_online(next_batch).max()
            loss = F.smooth_l1_loss(prediction, target)
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()

            if terminated or truncated:
                break

    return (train_an_episode_q_learning,)


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Checkpoint helpers
    """)
    return


@app.cell
def _(PlacementQFunction, device, feature_dim, gamma, torch):
    from pathlib import Path as _Path

    checkpoint_dir = _Path("/home/ntd/Projects/tetris/tetris_ai")

    def init_training_model():
        """Resume model and Adam state from latest.pt, or start a new run."""
        model = PlacementQFunction(feature_dim).to(device)
        optimizer = torch.optim.Adam(model.parameters(), lr=1e-4)
        latest = checkpoint_dir / "latest.pt"
        completed = 0
        if latest.exists():
            saved = torch.load(latest, map_location=device, weights_only=True)
            if saved["feature_dim"] != feature_dim or saved.get("encoding_version") != 1:
                raise ValueError("Checkpoint does not match the current feature encoder.")
            model.load_state_dict(saved["model"])
            if "optimizer" in saved:
                optimizer.load_state_dict(saved["optimizer"])
            completed = int(saved.get("episodes_trained", 0))
        return model, optimizer, completed

    def save_training_model(model, optimizer, episodes_trained):
        """Keep each version immutable while updating the latest alias."""
        checkpoint = {
            "model": model.state_dict(),
            "optimizer": optimizer.state_dict(),
            "feature_dim": feature_dim,
            "encoding_version": 1,
            "episodes_trained": episodes_trained,
            "gamma": gamma,
        }
        versioned = checkpoint_dir / "models" / f"episodes_{episodes_trained:06d}.pt"
        versioned.parent.mkdir(parents=True, exist_ok=True)
        with versioned.open("xb") as file:  # Never overwrite a versioned model.
            torch.save(checkpoint, file)
        latest = checkpoint_dir / "latest.pt"
        torch.save(checkpoint, latest)  # Update the alias; old versions remain.
        return latest, versioned

    return checkpoint_dir, init_training_model, save_training_model


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Training loop

    Each click resumes from `latest.pt`, trains N_EPISODES more episodes, and saves a new version.
    """)
    return


@app.cell
def _(mo):
    N_EPISODES = 1000
    train_button = mo.ui.run_button(label=f"Train next {N_EPISODES} episodes")
    train_button
    return N_EPISODES, train_button


@app.cell
def _(
    N_EPISODES,
    checkpoint_dir,
    env,
    init_training_model,
    mo,
    train_an_episode_q_learning,
    train_button,
):
    mo.stop(not train_button.value, mo.md("Click the button above to train."))

    trained_model, trained_optimizer, completed_episodes = init_training_model()
    episodes_trained = completed_episodes + N_EPISODES
    _versioned = checkpoint_dir / "models" / f"episodes_{episodes_trained:06d}.pt"
    if _versioned.exists():
        raise FileExistsError(f"Refusing to overwrite {_versioned}")

    _train_env = type(env)(piece_cap=env.piece_cap)  # Leave the demo board alone.
    trained_model.train()
    for _episode in range(completed_episodes, episodes_trained):
        train_an_episode_q_learning(
            seed=10_000 + _episode,
            q_online=trained_model,
            optimizer=trained_optimizer,
            training_env=_train_env,
            epsilon=0.02,
        )
        if (_episode - completed_episodes + 1) % 20 == 0:
            print(f"Trained {_episode - completed_episodes + 1}/{N_EPISODES} episodes")
    return episodes_trained, trained_model, trained_optimizer


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Save model and evaluation replay
    """)
    return


@app.cell
def _(
    checkpoint_dir,
    episodes_trained,
    evaluate_episode,
    mo,
    save_training_model,
    trained_model,
    trained_optimizer,
):
    from json import dumps as _dumps

    _latest, _versioned = save_training_model(
        trained_model, trained_optimizer, episodes_trained
    )
    replay_data = evaluate_episode(trained_model)
    replay_data["episodes_trained"] = episodes_trained
    replay_path = checkpoint_dir / "replays" / "eval.json"
    replay_path.parent.mkdir(parents=True, exist_ok=True)
    replay_path.write_text(_dumps(replay_data))
    mo.md(
        f"Trained through **{episodes_trained} episodes**. Saved `{_latest.name}`, "
        f"`models/{_versioned.name}`, and a greedy evaluation replay "
        f"with **{len(replay_data['frames'])} placements**."
    )
    return (replay_data,)


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Episode replay

    Scrub through the just-saved evaluation game, one placement at a time.
    """)
    return


@app.cell
def _(mo, replay_data):
    _frames = replay_data["frames"]
    replay_slider = mo.ui.slider(
        start=0,
        stop=max(1, len(_frames) - 1),
        step=1,
        value=0,
        disabled=len(_frames) < 2,
        show_value=True,
        label="Placement index",
    )
    return (replay_slider,)


@app.cell
def _(mo, replay_data, replay_slider):
    _frames = replay_data["frames"]
    if _frames:
        _index = int(replay_slider.value)
        _frame = _frames[_index]
        _landing = {tuple(cell) for cell in _frame["landing"]}
        _symbols = ".IOTSZJLGX"

        def _replay_board_text(board, overlay=frozenset()):
            _height, _width = len(board), len(board[0])
            _lines = [f"   +{'-' * _width}+"]
            for _row in range(_height - 1, -1, -1):
                _cells = "".join(
                    "@" if (_row, _col) in overlay else _symbols[board[_row][_col]]
                    for _col in range(_width)
                )
                _lines.append(f"{_row:02d} |{_cells}|")
            _lines.append(f"   +{'-' * _width}+")
            _lines.append(f"    {''.join(str(col % 10) for col in range(_width))}")
            return "\n".join(_lines)

        _moves = ", ".join(_frame["moves"]) or "(hard drop immediately)"
        _replay_view = mo.vstack(
            [
                mo.md(
                    f"**Seed {replay_data['seed']} — placement {_index + 1}/{len(_frames)}**  \n"
                    f"Piece: {_frame.get('piece', 'unknown')}, "
                    f"hold: {_frame.get('use_hold', 'unknown')}, "
                    f"rotation: {_frame.get('rotation', 'unknown')}.  \n"
                    f"Moves: {_moves}. Reward: {_frame['reward']}; "
                    f"lines: {_frame['lines']}; score: {_frame['score']}."
                ),
                replay_slider,
                mo.hstack(
                    [
                        mo.md(
                            "**Before: chosen landing (`@`)**\n```text\n"
                            + _replay_board_text(_frame["before"], _landing)
                            + "\n```"
                        ),
                        mo.md(
                            "**After placement**\n```text\n"
                            + _replay_board_text(_frame["after"])
                            + "\n```"
                        ),
                    ],
                    justify="start",
                    widths="equal",
                ),
                mo.md("Row 0 is at the bottom; the board after placement includes line clears."),
            ],
            gap=0.25,
        )
    else:
        _replay_view = mo.md("No placements to display.")
    _replay_view
    return


if __name__ == "__main__":
    app.run()
