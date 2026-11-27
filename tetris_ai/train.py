import marimo

__generated_with = "0.25.2"
app = marimo.App(width="medium")


@app.cell
def _():
    import marimo as mo
    import numpy as np
    from torch import torch, nn
    import tetris_py
    from spin_setup import load_spin_setup

    return load_spin_setup, mo, nn, np, tetris_py, torch


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## Training environment
    """)
    return


@app.cell
def _(tetris_py, torch):
    from dataclasses import dataclass

    BOARD_WIDTH = 10
    BOARD_HEIGHT = 40
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

        def __init__(self, piece_cap=500, *, spin_setups=(), spin_chance=0.0,
                     clean_garbage_layers=(0, 0), cheese_garbage_layers=(0, 0),
                     clean_garbage_chance=1.0, cheese_garbage_chance=1.0):
            if not isinstance(piece_cap, int) or piece_cap < 1:
                raise ValueError("piece_cap must be a positive integer")
            import math
            from spin_setup import SpinSetup

            def layer_range(value, name):
                if type(value) is int:
                    value = (value, value)
                if (not isinstance(value, tuple) or len(value) != 2 or
                        any(type(count) is not int for count in value) or
                        not 0 <= value[0] <= value[1] <= 20):
                    raise ValueError(f"{name} must be 0..20 or a (min, max) range")
                return value

            def probability(value, name):
                if (isinstance(value, bool) or
                        not isinstance(value, (int, float)) or
                        not math.isfinite(value) or not 0 <= value <= 1):
                    raise ValueError(f"{name} must be between 0 and 1")
                return value

            if isinstance(spin_setups, SpinSetup):
                spin_setups = (spin_setups,)
            self.spin_setups = tuple(spin_setups)
            if any(not isinstance(setup, SpinSetup) for setup in self.spin_setups):
                raise TypeError("spin_setups must contain loaded SpinSetup templates")
            spin_chance = probability(spin_chance, "spin_chance")
            if spin_chance and not self.spin_setups:
                raise ValueError("spin_chance requires at least one spin setup")
            self.spin_chance = spin_chance
            self.clean_garbage_layers = layer_range(
                clean_garbage_layers, "clean_garbage_layers")
            self.cheese_garbage_layers = layer_range(
                cheese_garbage_layers, "cheese_garbage_layers")
            self.clean_garbage_chance = probability(
                clean_garbage_chance, "clean_garbage_chance")
            self.cheese_garbage_chance = probability(
                cheese_garbage_chance, "cheese_garbage_chance")
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
            self._misamino_bot = tetris_py.MisaMinoBot()
            self.piece_cap = piece_cap
            self._seed = None
            self.pieces_placed = 0
            self.done = True
            self._actions = None

        def reset(self, seed, spin_setup=None, *, top_is_spin=None):
            if not isinstance(seed, int) or not 0 <= seed < 2**64:
                raise ValueError("seed must be an unsigned 64-bit integer")
            self._seed = None
            self.pieces_placed = 0
            self.done = True
            self._actions = None
            import random
            from spin_setup import SpinSetup

            if spin_setup is not None and not isinstance(spin_setup, SpinSetup):
                raise TypeError("Use load_spin_setup(path) for a Qt spin setup")
            if top_is_spin is not None and type(top_is_spin) is not bool:
                raise TypeError("top_is_spin must be True, False or None")
            if spin_setup is not None and top_is_spin is False:
                raise ValueError("Cannot request a spin setup with top_is_spin=False")
            rng = random.Random(seed ^ 0xA0761D6478BD642F)
            use_spin = (top_is_spin if top_is_spin is not None else
                        spin_setup is not None or
                        (bool(self.spin_setups) and rng.random() < self.spin_chance))
            if use_spin and spin_setup is None:
                if not self.spin_setups:
                    raise ValueError("top_is_spin=True requires spin_setups or spin_setup")
                spin_setup = rng.choice(self.spin_setups)

            def choose_layers(layer_range, chance):
                # A 100% gate consumes no extra RNG, preserving existing
                # seeded layer counts when the new chance option is omitted.
                if chance == 0 or (chance < 1 and rng.random() >= chance):
                    return 0
                return rng.randint(*layer_range)

            clean = choose_layers(
                self.clean_garbage_layers, self.clean_garbage_chance)
            cheese = choose_layers(
                self.cheese_garbage_layers, self.cheese_garbage_chance)
            height = spin_setup.height if use_spin else 0
            if clean + cheese + height > 20:
                raise ValueError("Opening template and garbage exceed 20 visible rows")
            if not use_spin and not clean and not cheese:
                self._engine.reset(seed)
                if not self._engine.spawn_next_piece():
                    raise RuntimeError("Could not spawn the first piece")
            elif use_spin:
                spin_setup.apply(self._engine, seed, tetris_py,
                                 clean_garbage_layers=clean,
                                 cheese_garbage_layers=cheese)
            else:
                self._engine.reset(seed)
                self._engine.generate_opening([], [], clean, cheese, seed)
                if not self._engine.spawn_next_piece():
                    raise RuntimeError("Could not spawn the first piece")
            self._seed = seed
            self.done = False
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
                # Pending total is observable; packet holes/RNG are not model inputs.
                "pending_garbage_lines": self._engine.pending_garbage_lines,
                "combo": self._engine.combo,
                "back_to_back": self._engine.back_to_back,
                "back_to_back_count": self._engine.back_to_back_count,
                "score": self._engine.score,
                "lines_cleared": self._engine.lines_cleared,
                "pieces_placed": self.pieces_placed,
            }

        def legal_placements(self) -> list[tetris_py.Placement]:
            if self._seed is None:
                raise RuntimeError("Call reset(seed) before requesting placements")
            if self.done:
                return ()
            if self._actions is None:
                active = self._engine.generate_active_piece_placement()
                held = self._engine.generate_hold_piece_placement() if self._engine.can_hold else []
                self._actions = tuple(active + held)
            return self._actions

        def misamino_choice(self, bot=None):
            """Return this state's cached legal Placement chosen by the C++ teacher.

            An optional MisaMinoBot can set a different search budget. The bot
            returns a separate Placement object, so match the full replayable
            action (including spin context) before passing it to step().
            """
            if self._seed is None:
                raise RuntimeError("Call reset(seed) before requesting a teacher choice")
            if self.done:
                raise RuntimeError("Episode ended; call reset(seed)")
            bot = self._misamino_bot if bot is None else bot
            if not isinstance(bot, tetris_py.MisaMinoBot):
                raise TypeError("Expected a tetris_py.MisaMinoBot")
            proposed = bot.choose(self._engine)
            if proposed is None:
                raise RuntimeError("Teacher found no legal placement")

            def action_key(placement):
                kick = placement.spin_context.kick_offset
                return (
                    placement.use_hold, int(placement.type),
                    placement.final_pos.row, placement.final_pos.col,
                    int(placement.final_rotation), placement.drop_distance,
                    tuple(int(move) for move in placement.moves),
                    int(placement.spin_context.last_movement),
                    None if kick is None else (kick.row, kick.col),
                )

            key = action_key(proposed)
            matches = [action for action in self.legal_placements()
                       if action_key(action) == key]
            if len(matches) != 1:
                raise RuntimeError("Teacher choice did not match exactly one legal placement")
            return matches[0]

        def step(self, action):
            if self.done:
                raise RuntimeError("Episode ended; call reset(seed)")
            if not isinstance(action, tetris_py.Placement):
                raise TypeError("Select a Placement from legal_placements()")
            actions = self.legal_placements()
            if not any(action is candidate for candidate in actions):
                raise ValueError("Select a placement from this state's legal_placements()")
            pre_b2b_count = self._engine.back_to_back_count
            height_before = self._engine.board.max_height()
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
            b2b_count = self._engine.back_to_back_count
            height_after = self._engine.board.max_height()
            info = {
                "lines_cleared": lock.lines_cleared,
                "score_delta": lock.score_delta,
                "height_delta": height_after - height_before,
                "spin": int(lock.spin),
                "perfect_clear": lock.perfect_clear,
                "attack_sent": lock.attack_sent,
                "pieces_placed": self.pieces_placed,
                "top_out": terminated,
                "b2b_delta": b2b_count - pre_b2b_count
            }
            reward = 2.0 * lock.attack_sent + info["b2b_delta"] + lock.lines_cleared - (0.1 * (info["height_delta"])) + 0.05
            if terminated:          # Top-out, not the 500-piece time limit
                reward -= 50.0
            return self.observe(), reward, terminated, truncated, info



    return BOARD_WIDTH, PREVIEW_COUNT, PlacementEnv, device


@app.cell
def _(PlacementEnv, device, load_spin_setup, mo):
    from pathlib import Path

    spin_dir = Path("../tetris_engine/spin_setups")  # From tetris_ai/
    setups = tuple(load_spin_setup(p) for p in sorted(spin_dir.glob("*.json")))

    env = PlacementEnv(
        spin_setups=setups, spin_chance=0.25,
        cheese_garbage_layers=(2, 4), cheese_garbage_chance=0.05, clean_garbage_chance=0.25, clean_garbage_layers=(4,4))
    initial_observation = env.reset(seed=42)
    get_viewer_checkpoint, set_viewer_checkpoint = mo.state(env.checkpoint())
    (device, len(initial_observation["board"]), len(env.legal_placements()))
    return env, get_viewer_checkpoint, set_viewer_checkpoint


@app.cell(hide_code=True)
def _(mo):
    mo.md("""
    ## Preview a Qt spin setup (no training)

    Use **Spin setup…** in the Qt app to draw a small bottom-anchored template
    with filled/overhang rows and save JSON. Paste its path below to load an
    independent `PlacementEnv` opening. Each reset chooses a seeded horizontal
    offset and mirror; S/Z and J/L scripted pieces mirror with the drawing,
    then the seeded 7-bag resumes. `PlacementEnv(spin_setups=(...), spin_chance=...,
    clean_garbage_layers=(min, max), clean_garbage_chance=...,
    cheese_garbage_layers=(min, max), cheese_garbage_chance=...)` chooses
    the opening per seed; `reset(seed, top_is_spin=True/False)` can override it.
    Each garbage type has an independent chance gate, then a seeded layer count.
    Use a minimum of 1 for an exact chance of appearing; a minimum of 0 can
    still produce no garbage after its chance gate passes. Garbage sits below
    any selected spin template. Normal training and evaluation remain unchanged
    until explicitly configured.
    """)
    return


@app.cell
def _(mo):
    spin_setup_path = mo.ui.text(label="Spin setup JSON path")
    spin_setup_seed = mo.ui.number(start=0, stop=2**31 - 1, value=42,
                                    label="Opening / bag seed")
    load_spin_button = mo.ui.run_button(label="Load spin setup")
    mo.hstack([spin_setup_path, spin_setup_seed, load_spin_button])
    return load_spin_button, spin_setup_path, spin_setup_seed


@app.cell
def _(
    PlacementEnv,
    load_spin_button,
    load_spin_setup,
    mo,
    spin_setup_path,
    spin_setup_seed,
    tetris_py,
):
    if not load_spin_button.value:
        _setup_view = mo.md("Choose a saved JSON setup and click **Load spin setup**.")
    else:
        _setup = load_spin_setup(spin_setup_path.value)
        _spin_env = PlacementEnv()
        _full_board, _offset, _mirrored = _setup.render(int(spin_setup_seed.value))
        _observation = _spin_env.reset(
            int(spin_setup_seed.value), spin_setup=_setup)
        _actions = _spin_env.legal_placements()
        _board = "\n".join(
            f"{row:02d} " + "".join(".IOTSZJLGX"[cell] for cell in _observation["board"][row])
            for row in range(19, -1, -1)
        )
        _name = lambda p: "Empty" if p is None else "IOTSZJL"[p]
        _setup_view = mo.md(
            f"**Template x:** {_offset}; **mirrored:** {_mirrored}. "
            f"**Active:** {_name(_observation['active_piece'])}; "
            f"**hold:** {_name(_observation['hold_piece'])}; "
            f"**next:** {''.join(_name(p) for p in _observation['preview'])}. "
            f"**Reachable placements:** {len(_actions)}, "
            f"including {sum(a.spin != tetris_py.SpinType.None_ for a in _actions)} "
            "spin-classified actions. These are candidate placements, not "
            "a claim that a spin line clear occurred.\n\n"
            f"```text\n{_board}\n```"
        )
    _setup_view
    return


@app.cell
def _(get_viewer_checkpoint, mo):
    # Read the opening before the lower demo cells run. A clicked demo step
    # publishes a new snapshot, refreshing this viewer without a dependency
    # on the lower training cell.
    viewer_checkpoint = get_viewer_checkpoint()
    viewer_engine = viewer_checkpoint.engine
    available_placements = ()
    placement_index = None
    show_hidden = None
    if viewer_checkpoint.done:
        _viewer_controls = mo.md(
            "The current episode has ended; no placements are available.")
    else:
        active = viewer_engine.generate_active_piece_placement()
        held = (viewer_engine.generate_hold_piece_placement()
                if viewer_engine.can_hold else [])
        available_placements = tuple(active + held)
        if not available_placements:
            _viewer_controls = mo.md(
                "No placements are available for the current game state.")
        else:
            placement_index = mo.ui.slider(
                start=0,
                stop=len(available_placements) - 1,
                step=1,
                value=0,
                show_value=True,
                label=f"Placement index (0–{len(available_placements) - 1})",
            )
            show_hidden = mo.ui.checkbox(label="Show all rows (including hidden rows)")
            _viewer_controls = mo.vstack([
                mo.md(f"Current state: {viewer_checkpoint.pieces_placed} piece(s) "
                      "placed. Letters on the board are locked cells."),
                mo.hstack([placement_index, show_hidden], justify="start"),
            ])
    _viewer_controls
    return available_placements, placement_index, show_hidden, viewer_engine


@app.cell
def _(available_placements, mo, placement_index, show_hidden, viewer_engine):
    def render_action():
        if not available_placements:
            return mo.md("No placements to preview in this state.")

        placement = available_placements[int(placement_index.value)]
        before = tuple(tuple(int(cell) for cell in row)
                       for row in viewer_engine.board.render())
        landing = {(cell.row, cell.col) for cell in placement.cells()}
        height, width = len(before), len(before[0])
        if len(landing) != 4 or any(
            not (0 <= row < height and 0 <= col < width) or before[row][col] != 0
            for row, col in landing
        ):
            raise RuntimeError("Selected placement overlaps the locked board or is out of bounds")

        # Both the action list and board come from the same checkpoint.
        # Applying the placement to a clone never modifies the viewer or live env.
        preview = viewer_engine.clone()
        active = preview.active_piece
        active_cells = {
            (active.pos.row + offset.row, active.pos.col + offset.col)
            for offset in active.piece.current_shape()
        } if active is not None else set()
        lock = preview.apply_placement(placement)
        if not lock.locked:
            raise RuntimeError("Engine did not lock the selected placement")
        after = tuple(tuple(int(cell) for cell in row) for row in preview.board.render())
        if tuple(tuple(int(cell) for cell in row)
                 for row in viewer_engine.board.render()) != before:
            raise RuntimeError("Placement preview modified its source checkpoint")

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
        return mo.vstack(
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

    render_action()
    return


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    # AI model section

    ## Q function definition and input encoding
    """)
    return


@app.cell
def _(BOARD_WIDTH, PREVIEW_COUNT, nn, np, tetris_py, torch):
    SCALAR_FEATURES = 85  # Current features after the three board planes

    class PlacementQFunction(nn.Module):
        def __init__(self, feature_dim):
            super().__init__()
            board_features = feature_dim - SCALAR_FEATURES
            board_height, remainder = divmod(board_features, 3 * BOARD_WIDTH)
            if remainder or board_height < 4:
                raise ValueError("Unexpected board feature size")

            self.height = board_height
            self.board_features = board_features
            self.spatial = nn.Sequential(
                nn.Conv2d(3, 32, kernel_size=3, padding=1),
                nn.ReLU(),
                nn.Conv2d(32, 32, kernel_size=3, padding=1),
                nn.ReLU(),
                nn.Flatten(),  # Retains exact row and column positions
                nn.Linear(32 * board_height * BOARD_WIDTH, 128),
                nn.ReLU(),
            )
            self.head = nn.Sequential(
                nn.Linear(128 + SCALAR_FEATURES, 128),
                nn.ReLU(),
                nn.Linear(128, 1),
            )

        def forward(self, x):
            board = x[:, :self.board_features].reshape(
                -1, 3, self.height, BOARD_WIDTH
            )
            other = x[:, self.board_features:]
            return self.head(torch.cat((self.spatial(board), other), dim=1)).squeeze(-1)

    def encode(observation, placement: tetris_py.Placement) -> np.ndarray:
        # Keep this feature order paired with ENCODING_VERSION in checkpoints.
        board = np.asarray(observation["board"])
        height, width = board.shape

        # The board is public; distinguish placed garbage from ordinary cells.
        occupied = (board != int(tetris_py.Cell.Empty)).astype(np.float32).ravel()
        garbage = (board == int(tetris_py.Cell.Garbage)).astype(np.float32).ravel()
        landing = np.zeros((height, width), dtype=np.float32)
        for cell in placement.cells():
            landing[cell.row, cell.col] = 1.0

        def piece_one_hot(piece):
            # Seven piece types, plus an eighth slot for an empty hold/active piece.
            values = np.zeros(8, dtype=np.float32)
            values[7 if piece is None else int(piece)] = 1.0
            return values

        active_rotation = np.zeros(4, dtype=np.float32)
        if observation["rotation"] is not None:
            active_rotation[observation["rotation"]] = 1.0
        active_position = observation["position"]
        active = np.array([
            active_position[0] / height if active_position is not None else 0.0,
            active_position[1] / width if active_position is not None else 0.0,
            float(observation["can_hold"]),
        ], dtype=np.float32)

        # Pad the fixed number of visible preview slots after a terminal state.
        preview = observation["preview"]
        if len(preview) > PREVIEW_COUNT:
            raise ValueError("More preview pieces than the configured input size")
        previews = [
            piece_one_hot(preview[i] if i < len(preview) else None)
            for i in range(PREVIEW_COUNT)
        ]

        candidate_piece = np.zeros(7, dtype=np.float32)
        candidate_piece[int(placement.type)] = 1.0
        candidate_rotation = np.zeros(4, dtype=np.float32)
        candidate_rotation[int(placement.final_rotation)] = 1.0
        action = np.array([
            placement.final_pos.row / height,
            placement.final_pos.col / width,
            float(placement.use_hold),
            placement.drop_distance / height,
        ], dtype=np.float32)

        state_feature = np.array([
            float(observation["back_to_back"]),
            np.log1p(observation["back_to_back_count"]) / np.log1p(20.0),
            np.log1p(observation["combo"]) / np.log1p(20.0),
        ], dtype=np.float32)
        spin_one_hot = np.zeros(3, dtype=np.float32)
        spin_one_hot[int(placement.spin)] = 1.0


        return np.concatenate((
            occupied, garbage, landing.ravel(),
            piece_one_hot(observation["active_piece"]), active_rotation, active,
            piece_one_hot(observation["hold_piece"]), *previews,
            candidate_piece, candidate_rotation, action,
            np.array([observation["pending_garbage_lines"] / height], dtype=np.float32),
            state_feature,
            spin_one_hot
        ))

    gamma = 0.99
    ENCODING_VERSION = 5
    return ENCODING_VERSION, PlacementQFunction, encode, gamma


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## Choosing an action
    """)
    return


@app.cell
def _(
    PlacementQFunction,
    device,
    encode,
    env,
    np,
    set_viewer_checkpoint,
    torch,
):
    # import numpy as np

    observation = env.reset(seed=42)
    set_viewer_checkpoint(env.checkpoint())
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
    ## Training step (optional)

    The action viewer above starts at the untouched opening. Click below to
    apply the selected demo action and view the resulting board and next actions.
    """)
    return


@app.cell
def _(mo):
    run_demo_step = mo.ui.run_button(label="Apply demo action and training step")
    run_demo_step
    return (run_demo_step,)


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
    run_demo_step,
    scores,
    set_viewer_checkpoint,
    torch,
):
    import torch.nn.functional as F

    # Marimo schedules cells by dependencies, not display order. The button
    # keeps the demo from locking a piece just by opening the notebook.
    if run_demo_step.value and env.pieces_placed == 0:
        current_input = torch.as_tensor(
            encode(observation, action), dtype=torch.float32, device=device
        ).unsqueeze(0)
        next_observation, reward, terminated, truncated, info = env.step(action)
        set_viewer_checkpoint(env.checkpoint())
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
    ## Save and load model
    """)
    return


@app.cell
def _(
    ENCODING_VERSION,
    PlacementQFunction,
    device,
    feature_dim,
    q_online,
    torch,
):
    from pathlib import Path as _Path

    _demo_path = (
        _Path("/home/ntd/Projects/tetris/tetris_ai")
        / "checkpoints" / f"encoding_v{ENCODING_VERSION}" / "q_demo.pt"
    )
    _demo_path.parent.mkdir(parents=True, exist_ok=True)
    torch.save(
        {
            "model": q_online.state_dict(),
            "feature_dim": feature_dim,
            "encoding_version": ENCODING_VERSION,
        },
        _demo_path,
    )

    saved = torch.load(_demo_path, map_location=device, weights_only=True)
    loaded_model = PlacementQFunction(saved["feature_dim"]).to(device)
    loaded_model.load_state_dict(saved["model"])
    loaded_model.eval()
    return


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## Greedy evaluation helper
    """)
    return


@app.cell
def _(device, encode, env, np, torch):
    EVAL_SEEDS = (987654321, 987654322, 987654323)

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

    def evaluate_benchmark(model):
        """Greedy fixed-seed solo games; survival is in pieces, not seconds."""
        results = []
        was_training = model.training
        model.eval()
        try:
            with torch.no_grad():
                for seed in EVAL_SEEDS:
                    eval_env = type(env)(piece_cap=env.piece_cap)
                    observation = eval_env.reset(seed)
                    game = {
                        "seed": seed,
                        "score": 0,
                        "lines_cleared": 0,
                        "attack_sent": 0,
                        "pieces_placed": 0,
                        "spin_clears": 0,
                        "perfect_clears": 0,
                        "max_stack_height": 0,
                        "top_out": False,
                        "reached_piece_cap": False,
                    }
                    while True:
                        actions = eval_env.legal_placements()
                        features = np.stack([encode(observation, action) for action in actions])
                        batch = torch.as_tensor(features, dtype=torch.float32, device=device)
                        action = actions[int(model(batch).argmax().item())]
                        observation, _, terminated, truncated, info = eval_env.step(action)
                        game["score"] = observation["score"]
                        game["lines_cleared"] += info["lines_cleared"]
                        game["attack_sent"] += info["attack_sent"]
                        game["pieces_placed"] = info["pieces_placed"]
                        game["spin_clears"] += int(
                            info["lines_cleared"] > 0 and info["spin"] != 0
                        )
                        game["perfect_clears"] += int(info["perfect_clear"])
                        game["max_stack_height"] = max(
                            game["max_stack_height"],
                            next(
                                (row + 1 for row in range(len(observation["board"]) - 1, -1, -1)
                                 if any(observation["board"][row])),
                                0,
                            ),
                        )
                        if terminated or truncated:
                            game["top_out"] = bool(terminated)
                            game["reached_piece_cap"] = bool(truncated)
                            results.append(game)
                            break
        finally:
            model.train(was_training)

        pieces = sum(game["pieces_placed"] for game in results)
        return {
            "seeds": list(EVAL_SEEDS),
            "piece_cap": env.piece_cap,
            "games": results,
            "mean_score": sum(game["score"] for game in results) / len(results),
            "mean_lines_cleared": sum(game["lines_cleared"] for game in results) / len(results),
            "mean_attack_sent": sum(game["attack_sent"] for game in results) / len(results),
            "mean_pieces_placed": pieces / len(results),
            "piece_cap_rate": sum(game["reached_piece_cap"] for game in results) / len(results),
            "attack_per_piece": sum(game["attack_sent"] for game in results) / max(1, pieces),
            "lines_per_piece": sum(game["lines_cleared"] for game in results) / max(1, pieces),
        }

    return evaluate_benchmark, evaluate_episode


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## Pick an action
    """)
    return


@app.cell
def _(PlacementEnv, device, encode, nn, np, tetris_py, torch):
    import random

    rng = random.Random(6789)


    def get_action_random(training_env: PlacementEnv, observation, model: nn.Module, epsilon: float) -> tetris_py.Placement:
        actions = training_env.legal_placements()
        features = np.stack([encode(observation, a) for a in actions])
        batch = torch.as_tensor(features, dtype=torch.float32, device=device)
        with torch.no_grad():
            scores = model(batch)
        chosen_index = rng.randrange(len(actions)) if rng.random() < epsilon else int(scores.argmax().item())
        action = actions[chosen_index]
        return action


    def get_action_expert(training_env: PlacementEnv, observation, model: nn.Module, epsilon: float) -> tetris_py.Placement:
        actions = training_env.legal_placements()
        if rng.random() < epsilon:
            teacher_action = training_env.misamino_choice()
            teacher_index = actions.index(teacher_action)
            chosen_index = teacher_index  
        else: 
            features = np.stack([encode(observation, a) for a in actions])
            batch = torch.as_tensor(features, dtype=torch.float32, device=device)
            with torch.no_grad():
                scores = model(batch)
            chosen_index = int(scores.argmax().item())
        action = actions[chosen_index]
        return action

    return get_action_expert, get_action_random


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## Full episode training (Q-learning)
    """)
    return


@app.cell
def _(
    F,
    PlacementEnv,
    device,
    encode,
    get_action_expert,
    get_action_random,
    nn,
    np,
    torch,
):
    from collections import deque
    from typing import List, Tuple

    def train_an_episode_q_learning(
        seed: int,
        q_online: nn.Module,
        optimizer: torch.optim.Adam,
        training_env: PlacementEnv,
        gamma: float,
        n: int = 14,
        epsilon: float = 0.05,
        explore_random: bool = False,
    ):
        observation = training_env.reset(seed)
        pending: deque[Tuple[np.ndarray, float]] = deque() # queue of (features, reward)

        # Q learning n step TD target update
        def update_oldest(bootstrap_observation = None):
            # Calc target
            steps = len(pending)
            reward_sum = sum((gamma**i) * reward for i, (_, reward) in enumerate(pending))
            target = torch.tensor(reward_sum, dtype=torch.float32, device=device)
            if bootstrap_observation is not None:
                with torch.no_grad():
                    next_actions = training_env.legal_placements()
                    next_features = np.stack([encode(bootstrap_observation, a) for a in next_actions])
                    next_batch = torch.as_tensor(next_features, dtype=torch.float32, device=device)
                    target += (gamma**steps) * q_online(next_batch).max()

            # Update
            current_features, _ = pending[0]
            current_batch = torch.as_tensor(current_features, dtype=torch.float32, device=device).unsqueeze(0)
            prediction = q_online(current_batch)[0]

            loss = F.smooth_l1_loss(prediction, target)
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()
            pending.popleft()


        while True:
            action = get_action_random(training_env, observation, q_online, epsilon) if explore_random else get_action_expert(training_env, observation, q_online, epsilon)
            next_observation, reward, terminated, truncated, info = training_env.step(action)
            features = encode(observation, action)
            pending.append((features, reward))

            if len(pending) == n:
                update_oldest(None if terminated or truncated else next_observation)

            if terminated or truncated:
                while pending:
                    update_oldest()
                break
            observation = next_observation

    return List, Tuple, train_an_episode_q_learning


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## Full episode training (Behaviour cloning)
    """)
    return


@app.cell
def _(F, device, encode, nn, np, torch):
    def train_an_episode_behaviour_cloning(
        seed: int,
        model: nn.Module,
        optimizer: torch.optim.Adam,
        training_env,
    ):
        observation = training_env.reset(seed)
        while True:
            # Current action
            actions = training_env.legal_placements()
            features = np.stack([encode(observation, a) for a in actions])
            batch = torch.as_tensor(features, dtype=torch.float32, device=device)
            scores = model(batch)  # Gradients must reach every action score.

            # Teacher action (target)
            teacher_action = training_env.misamino_choice()
            teacher_index = actions.index(teacher_action)
            loss = F.cross_entropy(
                scores.unsqueeze(0),
                torch.tensor([teacher_index], dtype=torch.long, device=device),
            )
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()

            observation, _, terminated, truncated, _ = training_env.step(teacher_action)
            if terminated or truncated:
                break

    return (train_an_episode_behaviour_cloning,)


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## Full episode training (Monte carlo learning)
    """)
    return


@app.cell
def _(
    F,
    List,
    PlacementEnv,
    Tuple,
    device,
    encode,
    get_action_expert,
    get_action_random,
    nn,
    tetris_py,
    torch,
):
    def train_an_episode_monte_carlo(
        seed: int,
        q_online: nn.Module,
        optimizer: torch.optim.Adam,
        training_env: PlacementEnv,
        gamma: float,
        epsilon: float = 0.05,
        explore_random: bool = False,
    ):
        observation = training_env.reset(seed)
        steps: List[Tuple[dict, tetris_py.Placement, int]] = []

        # Episode generation
        while True:
            action = get_action_random(training_env, observation, q_online, epsilon) if explore_random else get_action_expert(training_env, observation, q_online, epsilon)
            next_observation, reward, terminated, truncated, info = training_env.step(action)
            steps.append((observation, action, reward))

            observation = next_observation
            if terminated or truncated:
                break

        # target update
        G = 0
        optimizer.zero_grad(set_to_none=True)
        for obs, act, re in reversed(steps):
            G = re + gamma * G
            x = torch.as_tensor(
                encode(obs, act), dtype=torch.float32, device=device
            ).unsqueeze(0)
            target = torch.tensor([G], dtype=torch.float32, device=device)
            (F.smooth_l1_loss(q_online(x), target) / len(steps)).backward()
        optimizer.step()


    return (train_an_episode_monte_carlo,)


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## Checkpoint helpers
    """)
    return


@app.cell
def _(ENCODING_VERSION, PlacementQFunction, device, feature_dim, gamma, torch):
    import json as _json
    import os as _os
    import pathlib as _pathlib
    import tempfile as _tempfile

    checkpoint_root = (
        _pathlib.Path("/home/ntd/Projects/tetris/tetris_ai")
        / "checkpoints" / f"encoding_v{ENCODING_VERSION}"
    )
    checkpoint_dir = checkpoint_root / "shared"
    training_methods = ("q_learning", "monte_carlo", "behaviour_cloning")

    def init_training_model(method):
        """Resume shared weights; import the newest legacy model on the first run."""
        if method not in training_methods:
            raise ValueError(f"Unknown training method: {method}")
        model = PlacementQFunction(feature_dim).to(device)
        optimizer = torch.optim.Adam(model.parameters(), lr=1e-4)
        source = checkpoint_dir / "latest.pt"
        if not source.exists():
            legacy = (
                checkpoint_root / "latest.pt",
                checkpoint_root / "behaviour_cloning" / "latest.pt",
            )
            source = max(
                (path for path in legacy if path.exists()),
                key=lambda path: path.stat().st_mtime_ns,
                default=None,
            )
        completed = 0
        if source is not None:
            saved = torch.load(source, map_location=device, weights_only=True)
            if (saved["feature_dim"] != feature_dim or
                    saved.get("encoding_version") != ENCODING_VERSION):
                raise ValueError("Checkpoint does not match the current feature encoder.")
            source_method = saved.get("training_method", "q_learning")
            if source_method not in training_methods:
                raise ValueError("Checkpoint has an unknown training method.")
            model.load_state_dict(saved["model"])
            if source_method == method and "optimizer" in saved:
                optimizer.load_state_dict(saved["optimizer"])
            completed = int(saved.get("episodes_trained", 0))
            print(f"Training {method} from {source} ({source_method} weights)")
        return model, optimizer, completed

    def save_training_model(method, model, optimizer, episodes_trained, evaluation,
                            save_history=False):
        """Atomically update latest; archive immutable weights every 1,000 episodes."""
        if method not in training_methods:
            raise ValueError(f"Unknown training method: {method}")
        if not evaluation or episodes_trained < 1:
            raise ValueError("A completed evaluation and episode count are required")
        if save_history != (episodes_trained % 1_000 == 0):
            raise ValueError("Archive a versioned model only every 1,000 episodes")
        summary = {
            "training_method": method,
            "episodes_trained": episodes_trained,
            "encoding_version": ENCODING_VERSION,
            "feature_dim": feature_dim,
            **evaluation,
        }
        checkpoint = {
            "model": model.state_dict(),
            "optimizer": optimizer.state_dict(),
            "feature_dim": feature_dim,
            "encoding_version": ENCODING_VERSION,
            "episodes_trained": episodes_trained,
            "training_method": method,
            "evaluation": summary,
        }
        if method in ("q_learning", "monte_carlo"):
            checkpoint["gamma"] = gamma
        checkpoint_dir.mkdir(parents=True, exist_ok=True)
        latest = checkpoint_dir / "latest.pt"
        versioned = (checkpoint_dir / "models" / f"episodes_{episodes_trained:06d}.pt"
                     if save_history else None)
        if versioned is not None:
            versioned.parent.mkdir(parents=True, exist_ok=True)
        # A partial write must never corrupt latest.pt or leave a partial archive.
        with _tempfile.NamedTemporaryFile(
            mode="wb", dir=checkpoint_dir, prefix=".checkpoint-", suffix=".pt",
            delete=False,
        ) as file:
            temporary = _pathlib.Path(file.name)
            try:
                torch.save(checkpoint, file)
                file.flush()
                _os.fsync(file.fileno())
            except BaseException:
                temporary.unlink(missing_ok=True)
                raise
        try:
            if versioned is not None:
                _os.link(temporary, versioned)  # Fails rather than overwriting history.
            _os.replace(temporary, latest)
        finally:
            temporary.unlink(missing_ok=True)

        # Lightweight per-evaluation trend; versioned *model* history stays at 1,000.
        with (checkpoint_dir / "evaluations.jsonl").open("a") as file:
            file.write(_json.dumps(summary, sort_keys=True) + "\n")
        return latest, versioned

    return checkpoint_dir, init_training_model, save_training_model


@app.cell
def _(checkpoint_dir, env, evaluate_episode):
    from replay_visualizer import save_replay_archive

    def archive_evaluation_replay(model, episodes_trained, training_method):
        """Save a separate greedy replay and color GIF at a 10k milestone."""
        replay = evaluate_episode(model)
        replay["episodes_trained"] = episodes_trained
        replay["training_method"] = training_method
        return save_replay_archive(
            replay, lambda: type(env)(piece_cap=env.piece_cap),
            checkpoint_dir / "replays",
        )

    return (archive_evaluation_replay,)


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## Profiling
    """)
    return


@app.cell(disabled=True)
def _(
    PlacementQFunction,
    device,
    env,
    feature_dim,
    torch,
    train_an_episode_behaviour_cloning,
):
    import cProfile
    import pstats

    _test_env = type(env)(piece_cap=10)
    _test_model = PlacementQFunction(feature_dim).to(device)
    _test_optimizer = torch.optim.Adam(_test_model.parameters(), lr=1e-4)

    _profiler = cProfile.Profile()
    torch.cuda.synchronize()
    _profiler.enable()
    train_an_episode_behaviour_cloning(
        seed=42,
        model=_test_model,
        optimizer=_test_optimizer,
        training_env=_test_env,
    )
    torch.cuda.synchronize()
    _profiler.disable()

    pstats.Stats(_profiler).sort_stats("cumtime").print_stats(20)
    return


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ## Training loop

    Both buttons resume the same `shared/latest.pt`; the button chooses the
    training method. On the first run, the newest existing Q or BC checkpoint
    supplies the weights. Switching methods starts a fresh optimizer. Every
    100 total episodes, evaluate greedily on three fixed seeds and atomically
    update `shared/latest.pt`. Archive immutable models in `shared/models/`
    every 1,000 total episodes; keep evaluation summaries in
    `shared/evaluations.jsonl`. Every 10,000 episodes, archive an evaluation
    replay JSON and colored animated GIF in `shared/replays/`. A final partial
    batch also updates `latest.pt`.
    Survival is measured in placed pieces, since this mode has zero gravity.
    """)
    return


@app.cell
def _(mo):
    N_EPISODES = 49000
    q_train_button = mo.ui.run_button(label=f"Train Q-learning ({N_EPISODES} episodes)")
    mc_train_button = mo.ui.run_button(label=f"Train Monte carlo ({N_EPISODES} episodes)")
    bc_train_button = mo.ui.run_button(
        label=f"Train behavioural cloning ({N_EPISODES//50} episodes)"
    )
    mo.hstack([q_train_button, mc_train_button, bc_train_button], justify="start")
    return N_EPISODES, bc_train_button, mc_train_button, q_train_button


@app.cell
def _(
    N_EPISODES,
    archive_evaluation_replay,
    bc_train_button,
    checkpoint_dir,
    env,
    evaluate_benchmark,
    gamma,
    init_training_model,
    mc_train_button,
    mo,
    q_train_button,
    save_training_model,
    train_an_episode_behaviour_cloning,
    train_an_episode_monte_carlo,
    train_an_episode_q_learning,
):
    mo.stop(
        not (q_train_button.value or mc_train_button.value or bc_train_button.value),
        mo.md("Click a training button above."),
    )
    if sum((q_train_button.value, mc_train_button.value, bc_train_button.value)) != 1:
        raise RuntimeError("Run one training method at a time.")
    if q_train_button.value:
        training_method = "q_learning"
    elif mc_train_button.value:
        training_method = "monte_carlo"
    else:
        training_method = "behaviour_cloning"

    trained_model, trained_optimizer, completed_episodes = init_training_model(
        training_method
    )
    episodes_this_run = (N_EPISODES // 50) if bc_train_button.value else N_EPISODES
    if episodes_this_run < 1:
        raise ValueError("At least one training episode is required")
    episodes_trained = completed_episodes + episodes_this_run
    for _milestone in range(
        (completed_episodes // 1_000 + 1) * 1_000, episodes_trained + 1, 1_000
    ):
        _archive = checkpoint_dir / "models" / f"episodes_{_milestone:06d}.pt"
        if _archive.exists():
            raise FileExistsError(f"Refusing to overwrite {_archive}")
    for _milestone in range(
        (completed_episodes // 10_000 + 1) * 10_000, episodes_trained + 1, 10_000
    ):
        for _suffix in (".json", ".gif"):
            _archive = checkpoint_dir / "replays" / f"episodes_{_milestone:06d}{_suffix}"
            if _archive.exists():
                raise FileExistsError(f"Refusing to overwrite {_archive}")

    _train_env = type(env)(piece_cap=env.piece_cap)  # Leave the demo board alone.
    trained_model.train()
    latest_evaluation = None
    latest_checkpoint = None
    latest_archive = None
    _one_time = True
    for _episode in range(completed_episodes, episodes_trained):
        if training_method == "q_learning":
            train_an_episode_q_learning(
                seed=10_000 + _episode,
                q_online=trained_model,
                optimizer=trained_optimizer,
                training_env=_train_env,
                gamma=gamma,
                n=35,
                epsilon=0.1,
                explore_random=True
            )
        elif training_method == "behaviour_cloning":
            train_an_episode_behaviour_cloning(
                seed=10_000 + _episode,
                model=trained_model,
                optimizer=trained_optimizer,
                training_env=_train_env,
            )
        else:
            train_an_episode_monte_carlo(
                seed=10_000 + _episode,
                q_online=trained_model,
                optimizer=trained_optimizer,
                training_env=_train_env,
                gamma=gamma,
                epsilon=0.1,
                explore_random=True,
            )
        _finished = _episode + 1
        if _finished % 100 == 0 or _finished == episodes_trained:
            latest_evaluation = evaluate_benchmark(trained_model)
            latest_checkpoint, latest_archive = save_training_model(
                training_method, trained_model, trained_optimizer, _finished,
                latest_evaluation, save_history=(_finished % 1_000 == 0),
            )
            if _finished % 10_000 == 0 or _one_time:
                _one_time = False
                _replay_json, _replay_gif = archive_evaluation_replay(
                    trained_model, _finished, training_method,
                )
                print(f"Archived replay {_replay_json} and GIF {_replay_gif}")
            print(
                f"{training_method}: {_episode - completed_episodes + 1}/"
                f"{episodes_this_run} episodes; saved latest at {_finished}"
                f"{' + archived model' if latest_archive else ''}; "
                f"eval mean score {latest_evaluation['mean_score']:.1f}, "
                f"lines {latest_evaluation['mean_lines_cleared']:.1f}, "
                f"attack {latest_evaluation['mean_attack_sent']:.1f}, "
                f"survived {latest_evaluation['mean_pieces_placed']:.1f}/"
                f"{latest_evaluation['piece_cap']} pieces, "
                f"cap rate {latest_evaluation['piece_cap_rate']:.0%}"
            )
    return (
        episodes_trained,
        latest_archive,
        latest_checkpoint,
        latest_evaluation,
        trained_model,
        training_method,
    )


@app.cell
def _(mo):
    refresh_charts = mo.ui.run_button(label="Refresh training charts")
    mo.vstack([
        mo.md(
            "### Training progress\n\n"
            "Read-only plots of the saved greedy evaluations. Refresh after a "
            "training run to load new results; this does not start training."
        ),
        refresh_charts,
    ])
    return (refresh_charts,)


@app.cell
def _(checkpoint_dir, mo, refresh_charts):
    import json
    import math
    from html import escape

    def load_evaluation_history(path):
        """Return all saved evaluations, using the latest result per episode."""
        if not path.exists():
            return []
        with path.open() as file:
            records = [json.loads(line) for line in file if line.strip()]
        if not records:
            return []
        by_episode = {int(record["episodes_trained"]): record for record in records}
        return [by_episode[episode] for episode in sorted(by_episode)]

    def evaluation_chart(records, title, key, tick_format="count", scale=1):
        """Draw an SVG chart without adding a plotting-library dependency."""
        width, height = 560, 250
        left, right, top, bottom = 66, 35, 30, 43
        plot_width = width - left - right
        plot_height = height - top - bottom
        episodes = [int(record["episodes_trained"]) for record in records]
        values = [float(record[key]) * scale for record in records]
        if episodes[0] == episodes[-1]:
            x_min = x_max = episodes[0]
            x_ticks = (episodes[0],)
        else:
            # Round the episode axis to evenly spaced, readable milestones.
            magnitude = 10 ** max(
                0, math.floor(math.log10((episodes[-1] - episodes[0]) / 5))
            )
            for multiple in (1, 2, 5, 10):
                x_step = multiple * magnitude
                x_min = episodes[0] // x_step * x_step
                x_max = (episodes[-1] + x_step - 1) // x_step * x_step
                if (x_max - x_min) // x_step <= 6:
                    break
            x_ticks = range(x_min, x_max + x_step, x_step)
        # Smooth over up to ten saved evaluations, regardless of their cadence.
        smoothed = [
            sum(values[max(0, i - 9):i + 1]) / min(i + 1, 10)
            for i in range(len(values))
        ]
        if tick_format == "percent":
            step, intervals = 20, 5
        elif max(values) <= 0:
            step, intervals = (0.1 if tick_format == "ratio" else 1), 4
        else:
            peak = max(values)
            magnitude = 10 ** math.floor(math.log10(peak / 5))
            # Use 1, 2 or 5 times a power of ten for four to six grid intervals.
            step = next(
                multiple * magnitude for multiple in (1, 2, 5, 10)
                if peak <= 6 * multiple * magnitude
            )
            intervals = max(4, math.ceil(peak / step))
        ymax = intervals * step

        def x(episode):
            if x_min == x_max:
                return left + plot_width / 2
            return left + plot_width * (
                (episode - x_min) / (x_max - x_min)
            )

        def y(value):
            return top + plot_height * (1 - value / ymax)

        def path(series):
            return " ".join(
                f"{'M' if i == 0 else 'L'}{x(episode):.1f},{y(value):.1f}"
                for i, (episode, value) in enumerate(zip(episodes, series))
            )

        def label(value):
            if tick_format == "percent":
                return f"{value:.0f}%"
            decimals = max(0, -math.floor(math.log10(step)))
            if tick_format == "ratio":
                decimals = max(2, decimals)
            return f"{value:,.{decimals}f}"

        parts = [
            f'<svg viewBox="0 0 {width} {height}" role="img" '
            f'aria-label="{escape(title)} by total training episodes" '
            'style="width:100%;height:auto;background:#fff;border:1px solid '
            '#e2e8f0;border-radius:8px">',
            f'<text x="{left}" y="21" font-size="15" font-weight="600" '
            f'fill="#0f172a">{escape(title)}</text>',
        ]
        for index in range(intervals + 1):
            value = index * step
            position = y(value)
            parts.append(
                f'<line x1="{left}" y1="{position:.1f}" x2="{width - right}" '
                f'y2="{position:.1f}" stroke="#e2e8f0"/>'
                f'<text x="{left - 9}" y="{position + 4:.1f}" text-anchor="end" '
                f'font-size="11" fill="#475569">{label(value)}</text>'
            )
        for episode in x_ticks:
            position = x(episode)
            parts.append(
                f'<line x1="{position:.1f}" y1="{top}" '
                f'x2="{position:.1f}" y2="{top + plot_height}" '
                'stroke="#f1f5f9"/>'
                f'<text x="{position:.1f}" y="{height - 12}" '
                f'text-anchor="middle" font-size="11" fill="#475569">'
                f'{episode:,}</text>'
            )
        parts.extend([
            f'<path d="{path(values)}" fill="none" stroke="#94a3b8" '
            'stroke-width="1.2" stroke-opacity="0.8"/>',
            f'<path d="{path(smoothed)}" fill="none" stroke="#2563eb" '
            'stroke-width="2.5"/>',
            f'<circle cx="{x(episodes[-1]):.1f}" cy="{y(values[-1]):.1f}" '
            'r="3" fill="#2563eb"/>',
            '</svg>',
        ])
        return "".join(parts)

    _ = refresh_charts.value
    _history = load_evaluation_history(checkpoint_dir / "evaluations.jsonl")
    if not _history:
        _charts_view = mo.md(
            "No saved evaluations found yet in `shared/evaluations.jsonl`."
        )
    else:
        _metrics = (
            ("Mean score", "mean_score", "count", 1),
            ("Pieces survived", "mean_pieces_placed", "count", 1),
            ("Lines cleared", "mean_lines_cleared", "count", 1),
            ("Attack sent", "mean_attack_sent", "count", 1),
            ("Lines per piece", "lines_per_piece", "ratio", 1),
            ("Piece-cap rate", "piece_cap_rate", "percent", 100),
        )
        _methods = ", ".join(sorted({
            record.get("training_method", "unknown") for record in _history
        }))
        _charts_view = mo.vstack([
            mo.md(
                f"**{len(_history)} evaluations**, from "
                f"**{_history[0]['episodes_trained']:,}** to "
                f"**{_history[-1]['episodes_trained']:,}** total episodes "
                f"(all saved history; methods: `{_methods}`). "
                "Each point is a greedy evaluation on three fixed seeds, "
                "not a training-episode result. Gray: raw evaluation; "
                "blue: trailing average of up to 10 evaluations."
            ),
            mo.Html(
                '<div style="display:grid;grid-template-columns:repeat('
                'auto-fit,minmax(min(100%,380px),1fr));gap:12px">'
                + "".join(
                    evaluation_chart(_history, title, key, tick_format, scale)
                    for title, key, tick_format, scale in _metrics
                )
                + "</div>"
            ),
        ])
    _charts_view
    return


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Final evaluation replay and checkpoint summary
    """)
    return


@app.cell
def _(
    checkpoint_dir,
    episodes_trained,
    evaluate_episode,
    latest_archive,
    latest_checkpoint,
    latest_evaluation,
    mo,
    trained_model,
    training_method,
):
    from json import dumps as _dumps

    if latest_checkpoint is None or latest_evaluation is None:
        raise RuntimeError("Training ended without a saved checkpoint and evaluation")
    replay_data = evaluate_episode(trained_model)
    replay_data["episodes_trained"] = episodes_trained
    replay_data["training_method"] = training_method
    replay_path = checkpoint_dir / "replays" / "eval.json"
    replay_path.parent.mkdir(parents=True, exist_ok=True)
    replay_path.write_text(_dumps(replay_data))
    _relative_dir = checkpoint_dir.relative_to(checkpoint_dir.parent.parent)
    _archive_text = (
        f", and archived `{_relative_dir / 'models' / latest_archive.name}`"
        if latest_archive else ""
    )
    mo.md(
        f"{training_method}: trained through **{episodes_trained} total episodes**. "
        f"Saved `{_relative_dir / latest_checkpoint.name}`{_archive_text}. "
        f"Greedy evaluation over {len(latest_evaluation['games'])} fixed seeds: "
        f"mean score **{latest_evaluation['mean_score']:.1f}**, "
        f"lines **{latest_evaluation['mean_lines_cleared']:.1f}**, "
        f"attack **{latest_evaluation['mean_attack_sent']:.1f}**, "
        f"pieces survived **{latest_evaluation['mean_pieces_placed']:.1f}/"
        f"{latest_evaluation['piece_cap']}**, "
        f"piece-cap rate **{latest_evaluation['piece_cap_rate']:.0%}**. "
        f"Per-seed results are in `{_relative_dir / 'evaluations.jsonl'}`. "
        f"The greedy replay has **{len(replay_data['frames'])} placements**."
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
            _height, _width = min(len(board), 20), len(board[0])
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

        _shorten_moves = []
        for move in _frame["moves"]:
            n = len(_shorten_moves)
            if n == 0:
                _shorten_moves.append((move, 1))
                continue
            last_move, amt = _shorten_moves[n-1]
            if last_move == move:
                _shorten_moves[n-1] = (move, amt+1)
            else:
                _shorten_moves.append((move, 1))

        _moves = ", ".join(f"{move}{"" if i == 1 else (f"x{i}")}" for move, i in _shorten_moves) or "(hard drop immediately)"
        _replay_view = mo.vstack(
            [
                mo.md(
                    f"**Seed {replay_data['seed']} — placement {_index + 1}/{len(_frames)}**  \n"
                    f"Piece: {_frame.get('piece', 'unknown')}, "
                    f"hold: {_frame.get('use_hold', 'unknown')}, "
                    f"rotation: {_frame.get('rotation', 'unknown')}.  \n"
                    f"Moves: {_moves}.   \nReward: {_frame['reward']}; "
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


@app.cell(hide_code=True)
def _(mo):
    mo.md(r"""
    ### Color evaluation replay

    Load the saved `replays/eval.json` without starting training. Scrub through
    the placements, or switch between the chosen landing and the board after
    the lock. Hold, next queue, and attack stats are recovered by replaying the
    saved seed in a separate environment; the training game is not touched.
    """)
    return


@app.cell
def _(mo):
    refresh_color_replay = mo.ui.run_button(label="Reload saved color replay")
    refresh_color_replay
    return (refresh_color_replay,)


@app.cell
def _(checkpoint_dir, env, mo, refresh_color_replay):
    from json import loads as _load_replay_json
    from replay_visualizer import reconstruct_replay

    _ = refresh_color_replay.value
    _replay_path = checkpoint_dir / "replays" / "eval.json"
    if _replay_path.is_file():
        color_replay_data = _load_replay_json(_replay_path.read_text())
        color_replay_states = reconstruct_replay(
            color_replay_data, lambda: type(env)(piece_cap=env.piece_cap)
        )
    else:
        color_replay_data = {"frames": []}
        color_replay_states = []

    _count = len(color_replay_data["frames"])
    color_replay_slider = mo.ui.slider(
        start=0, stop=max(1, _count - 1), step=1, value=0,
        disabled=_count < 2, show_value=True, label="Placement index",
    )
    color_after_lock = mo.ui.switch(label="After lock / line clear")
    if _count:
        _color_controls = mo.hstack(
            [color_replay_slider, color_after_lock], justify="start"
        )
    else:
        _color_controls = mo.md(f"No saved evaluation replay at `{_replay_path}` yet.")
    _color_controls
    return (
        color_after_lock,
        color_replay_data,
        color_replay_slider,
        color_replay_states,
    )


@app.cell
def _(
    color_after_lock,
    color_replay_data,
    color_replay_slider,
    color_replay_states,
    mo,
):
    from replay_visualizer import render_replay

    if color_replay_states:
        _color_panel = mo.Html(render_replay(
            color_replay_data, color_replay_states,
            int(color_replay_slider.value), after=color_after_lock.value,
        ))
    else:
        _color_panel = mo.md("No saved placements to display.")
    _color_panel
    return


@app.cell(disabled=True)
def _(
    ENCODING_VERSION,
    PlacementQFunction,
    checkpoint_dir,
    device,
    evaluate_episode,
    feature_dim,
    torch,
):
    _saved = torch.load(
        checkpoint_dir / "models" / "episodes_000210.pt",
        map_location=device,
        weights_only=True,
    )
    assert _saved["feature_dim"] == feature_dim
    assert _saved["encoding_version"] == ENCODING_VERSION

    old_model = PlacementQFunction(feature_dim).to(device)
    old_model.load_state_dict(_saved["model"])
    old_model.eval()

    replay = evaluate_episode(old_model)
    print(len(replay["frames"]), sum(f["reward"] for f in replay["frames"]))
    return


if __name__ == "__main__":
    app.run()
