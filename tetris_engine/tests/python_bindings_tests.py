"""Smoke and regression tests for the optional headless Python module."""

import unittest
import ast
import json
import random
from dataclasses import dataclass
from pathlib import Path
import sys
import tempfile

import tetris_py as t

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tetris_ai"))
from spin_setup import load_spin_setup


def i_piece_seed():
    game = t.Engine()
    for seed in range(100):
        game.reset(seed)
        game.spawn_next_piece()
        if game.active_piece.piece.type == t.PieceType.I:
            return seed
    raise AssertionError("could not find an I-piece seed")


def paint_single_well(game):
    assert game.set_edit_mode(True)
    for col in range(10):
        if col < 3 or col > 6:
            assert game.paint_cell(0, col, t.Cell.Garbage)
    assert game.paint_cell(1, 9, t.Cell.Garbage)  # Not a perfect clear.
    assert game.set_edit_mode(False)


def placement_key(placement):
    kick = placement.spin_context.kick_offset
    return (
        placement.use_hold, int(placement.type),
        placement.final_pos.row, placement.final_pos.col,
        int(placement.final_rotation), placement.drop_distance,
        tuple(int(move) for move in placement.moves),
        int(placement.spin_context.last_movement),
        None if kick is None else (kick.row, kick.col),
        int(placement.spin),
    )


class BindingsTests(unittest.TestCase):
    def test_scripted_opening_prefilled_hold_and_seeded_fallback(self):
        cfg = t.EngineConfig()
        cfg.gravity = 0
        game = t.Engine(cfg)
        reference = t.Engine(cfg)
        game.reset(42)
        reference.reset(42)
        fallback = reference.preview_queue()
        game.set_opening_pieces([t.PieceType.T, t.PieceType.I], t.PieceType.S)
        self.assertEqual(game.preview_queue()[:5],
                         [t.PieceType.T, t.PieceType.I, *fallback[:3]])
        self.assertEqual(game.hold_piece, t.PieceType.S)
        self.assertTrue(game.spawn_next_piece())
        self.assertEqual(game.active_piece.piece.type, t.PieceType.T)
        self.assertEqual(game.preview_queue()[:5],
                         [t.PieceType.I, *fallback[:4]])
        snapshot = game.clone()
        self.assertTrue(game.hold())
        self.assertEqual(game.active_piece.piece.type, t.PieceType.S)
        self.assertEqual(game.hold_piece, t.PieceType.T)
        self.assertEqual(game.preview_queue()[0], t.PieceType.I)
        game.restore(snapshot)
        self.assertEqual(game.active_piece.piece.type, t.PieceType.T)
        self.assertEqual(game.hold_piece, t.PieceType.S)
        self.assertEqual(game.preview_queue()[0], t.PieceType.I)
        with self.assertRaises(RuntimeError):
            game.set_opening_pieces([t.PieceType.O])
        self.assertTrue(game.hard_drop().locked)
        self.assertTrue(game.spawn_next_piece())
        self.assertEqual(game.active_piece.piece.type, t.PieceType.I)
        self.assertEqual(game.preview_queue(), fallback)
        game.reset(42)
        self.assertIsNone(game.hold_piece)
        self.assertEqual(game.preview_queue(), fallback)
        with self.assertRaises(ValueError):
            game.set_opening_pieces([])

        empty_hold = t.Engine(cfg)
        empty_hold.reset(42)
        empty_hold.set_opening_pieces(
            [t.PieceType.T, t.PieceType.O, t.PieceType.I])
        self.assertTrue(empty_hold.spawn_next_piece())
        next_before = empty_hold.preview_queue()
        invalid = t.Engine(cfg)
        invalid.reset(42)
        invalid.set_opening_pieces(
            [t.PieceType.T, t.PieceType.S, t.PieceType.I])
        self.assertTrue(invalid.spawn_next_piece())
        wrong_held_action = invalid.generate_hold_piece_placement()[0]
        with self.assertRaises(ValueError):
            empty_hold.apply_placement(wrong_held_action)
        self.assertEqual(empty_hold.preview_queue(), next_before)
        self.assertIsNone(empty_hold.hold_piece)
        self.assertTrue(empty_hold.can_hold)
        self.assertEqual(empty_hold.active_piece.piece.type, t.PieceType.T)
        self.assertTrue(empty_hold.hold())
        self.assertEqual(empty_hold.active_piece.piece.type, t.PieceType.O)
        self.assertEqual(empty_hold.preview_queue()[0], t.PieceType.I)

    def test_qt_spin_setup_json_loads_into_engine(self):
        board = ["." * 10 for _ in range(40)]
        board[0] = "G........."
        example = {
            "version": 1, "width": 10, "height": 40,
            "board_rows_bottom_up": board,
            "active_piece": "T", "hold_piece": "I", "next_pieces": "OTS",
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "spin.json"
            path.write_text(json.dumps(example), encoding="utf-8")
            setup = load_spin_setup(path)
            config = t.EngineConfig()
            config.gravity = 0
            game = t.Engine(config)
            setup.apply(game, 42, t)
            self.assertEqual(game.board.get(0, 0), t.Cell.Garbage)
            self.assertEqual(game.active_piece.piece.type, t.PieceType.T)
            self.assertEqual(game.hold_piece, t.PieceType.I)
            self.assertEqual(game.preview_queue()[:3],
                             [t.PieceType.O, t.PieceType.T, t.PieceType.S])
            self.assertTrue(game.generate_active_piece_placement())
            self.assertTrue(game.generate_hold_piece_placement())
            reference = t.Engine(config)
            reference.reset(42)
            self.assertEqual(game.preview_queue()[3:],
                             reference.preview_queue()[:2])
            game.reset(42)
            self.assertEqual(game.board.get(0, 0), t.Cell.Empty)
            self.assertIsNone(game.hold_piece)
            self.assertEqual(game.preview_queue(), reference.preview_queue())

            bad = dict(example)
            bad["board_rows_bottom_up"] = board[:]
            bad["board_rows_bottom_up"][20] = "X" + "." * 9
            path.write_text(json.dumps(bad), encoding="utf-8")
            with self.assertRaises(ValueError):
                load_spin_setup(path)
            bad = dict(example, active_piece="INVALID")
            path.write_text(json.dumps(bad), encoding="utf-8")
            with self.assertRaises(ValueError):
                load_spin_setup(path)
            bad = dict(example, version=2)
            path.write_text(json.dumps(bad), encoding="utf-8")
            with self.assertRaises(ValueError):
                load_spin_setup(path)

            # The notebook's environment must opt in on reset, rather than
            # changing the training loop or the default seeded opening.
            notebook = ast.parse((Path(__file__).resolve().parents[2] /
                                  "tetris_ai/train.py").read_text())
            cell = next(node for node in notebook.body
                        if isinstance(node, ast.FunctionDef) and
                        any(isinstance(child, ast.ClassDef) and
                            child.name == "PlacementEnv" for child in node.body))
            classes = [child for child in cell.body
                       if isinstance(child, ast.ClassDef) and
                       child.name in ("EnvCheckpoint", "PlacementEnv")]
            namespace = {
                "tetris_py": t, "dataclass": dataclass,
                "BOARD_WIDTH": 10, "BOARD_HEIGHT": 40, "PREVIEW_COUNT": 5,
            }
            exec(compile(ast.Module(body=classes, type_ignores=[]),
                         "train.py", "exec"), namespace)
            Env = namespace["PlacementEnv"]
            env = Env(piece_cap=3)
            default = env.reset(42)
            self.assertEqual(default["board"][0][0], int(t.Cell.Empty))
            self.assertIsNone(default["hold_piece"])
            # Restore the valid setup file after invalid-data tests.
            path.write_text(json.dumps(example), encoding="utf-8")
            loaded = load_spin_setup(path)
            opening = env.reset(42, spin_setup=loaded)
            self.assertEqual(opening["board"][0][0], int(t.Cell.Garbage))
            self.assertEqual(opening["active_piece"], int(t.PieceType.T))
            self.assertEqual(opening["hold_piece"], int(t.PieceType.I))
            self.assertEqual(opening["preview"][:3],
                             (int(t.PieceType.O), int(t.PieceType.T),
                              int(t.PieceType.S)))
            checkpoint = env.checkpoint()
            action = env.legal_placements()[0]
            env.step(action)
            self.assertEqual(env.restore(checkpoint), opening)
            self.assertTrue(env.legal_placements())
            self.assertEqual(env.reset(42), default)
            blocked = dict(example)
            blocked["board_rows_bottom_up"] = board[:]
            blocked["board_rows_bottom_up"][19] = "....G....."
            path.write_text(json.dumps(blocked), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "spawn"):
                env.reset(42, spin_setup=load_spin_setup(path))
            self.assertTrue(env.done)

            modular = {
                "version": 2, "width": 10, "height": 40,
                "template": {
                    "width": 3, "height": 3,
                    "rows_bottom_up": [".G.", "G..", "G.."],
                    "row_modes_bottom_up": ["filled", "filled", "overhang"],
                },
                "active_piece": "T", "hold_piece": "I", "next_pieces": "OTS",
            }
            path.write_text(json.dumps(modular), encoding="utf-8")
            template = load_spin_setup(path)
            transforms = set()
            for seed in range(40):
                expanded, x, mirrored = template.render(seed)
                transforms.add((x, mirrored))
                self.assertEqual(template.render(seed), (expanded, x, mirrored))
                self.assertEqual(expanded[0][:x], "G" * x)
                self.assertEqual(expanded[0][x + 3:], "G" * (7 - x))
                self.assertEqual(expanded[0][x:x + 3], ".G.")
                self.assertEqual(expanded[1][x:x + 3],
                                 "..G" if mirrored else "G..")
                self.assertEqual(expanded[2][:x], "." * x)
                self.assertEqual(expanded[2][x + 3:], "." * (7 - x))
                self.assertEqual(expanded[2][x:x + 3],
                                 "..G" if mirrored else "G..")
                self.assertTrue(all(row == "." * 10 for row in expanded[3:]))
                opening = env.reset(seed, spin_setup=template)
                self.assertEqual(
                    opening["board"],
                    tuple(tuple(".IOTSZJLGX".index(cell) for cell in row)
                          for row in expanded))
                self.assertEqual(opening["active_piece"], int(t.PieceType.T))
                self.assertEqual(opening["hold_piece"], int(t.PieceType.I))
                self.assertEqual(opening["preview"][:3],
                                 (int(t.PieceType.O), int(t.PieceType.T),
                                  int(t.PieceType.Z if mirrored else t.PieceType.S)))
                baseline = t.Engine(config)
                baseline.reset(seed)
                self.assertEqual(opening["preview"][3:],
                                 tuple(int(p) for p in baseline.preview_queue()[:2]))
                self.assertTrue(baseline.spawn_next_piece())
                self.assertEqual(env._engine.active_piece.pos.col,
                                 baseline.active_piece.pos.col)
            self.assertGreater(len({x for x, _ in transforms}), 1)
            self.assertEqual({flip for _, flip in transforms}, {False, True})
            self.assertEqual(env.reset(42), default)

            modular["template"]["row_modes_bottom_up"] = ["filled", "wrong", "overhang"]
            path.write_text(json.dumps(modular), encoding="utf-8")
            with self.assertRaises(ValueError):
                load_spin_setup(path)
            modular["template"]["row_modes_bottom_up"] = ["filled", "filled", "overhang"]
            modular["template"]["width"] = 11
            path.write_text(json.dumps(modular), encoding="utf-8")
            with self.assertRaises(ValueError):
                load_spin_setup(path)

    def test_generated_opening_mirrors_scripted_pieces_and_garbage(self):
        config = t.EngineConfig()
        config.width, config.height = 10, 40
        game = t.Engine(config)
        pattern = [[t.Cell.Garbage, t.Cell.Empty, t.Cell.Empty]]
        pieces = [t.PieceType.S, t.PieceType.Z, t.PieceType.J,
                  t.PieceType.L, t.PieceType.I, t.PieceType.O, t.PieceType.T]
        transforms = set()
        changing_holes = False
        for seed in range(50):
            game.reset(seed)
            transform = game.generate_opening(
                pattern, [False], 2, 3, seed, pieces, t.PieceType.J)
            transforms.add((transform.offset, transform.mirrored))
            board = game.board.render()
            clean_holes = [[col for col, cell in enumerate(board[row])
                            if cell == t.Cell.Empty] for row in range(2)]
            self.assertEqual(len(clean_holes[0]), 1)
            self.assertEqual(clean_holes[0], clean_holes[1])
            cheese_holes = [[col for col, cell in enumerate(board[row])
                             if cell == t.Cell.Empty] for row in range(2, 5)]
            self.assertTrue(all(len(holes) == 1 for holes in cheese_holes))
            changing_holes |= len({holes[0] for holes in cheese_holes}) > 1
            self.assertEqual(
                [int(cell) for cell in board[5]],
                [int(t.Cell.Garbage) if col == transform.offset + (
                    2 if transform.mirrored else 0) else int(t.Cell.Empty)
                 for col in range(10)])
            self.assertTrue(all(cell == t.Cell.Empty for row in board[6:]
                                for cell in row))
            mirror = {t.PieceType.S: t.PieceType.Z,
                      t.PieceType.Z: t.PieceType.S,
                      t.PieceType.J: t.PieceType.L,
                      t.PieceType.L: t.PieceType.J}
            expected = [mirror.get(piece, piece) if transform.mirrored else piece
                        for piece in pieces]
            self.assertEqual(game.preview_queue()[:5], expected[:5])
            self.assertEqual(
                game.hold_piece, t.PieceType.L if transform.mirrored else t.PieceType.J)
            self.assertTrue(game.spawn_next_piece())
            self.assertEqual(game.active_piece.piece.type, expected[0])
            self.assertEqual(game.preview_queue()[:5], expected[1:6])
            game.reset(seed)
            again = game.generate_opening(
                pattern, [False], 2, 3, seed, pieces, t.PieceType.J)
            self.assertEqual((again.offset, again.mirrored),
                             (transform.offset, transform.mirrored))
            self.assertEqual(game.board.render(), board)
        self.assertEqual({mirror for _, mirror in transforms}, {False, True})
        self.assertGreater(len({offset for offset, _ in transforms}), 1)
        self.assertTrue(changing_holes)

        game.reset(0)
        with self.assertRaisesRegex(ValueError, "visible"):
            game.generate_opening([[t.Cell.Empty]], [False], 20, 0, 0)
        self.assertEqual(game.board.max_height(), 0)
        with self.assertRaisesRegex(ValueError, "visible"):
            game.generate_opening([[t.Cell.Empty, t.Cell.Empty]], [], 0, 0, 0)
        self.assertEqual(game.board.max_height(), 0)
        game.generate_opening([], [], 1, 0, 0)
        with self.assertRaisesRegex(RuntimeError, "after reset"):
            game.generate_opening([], [], 1, 0, 0)

    def test_environment_selects_spin_and_garbage_on_python_side(self):
        from spin_setup import SpinSetup

        notebook = ast.parse((Path(__file__).resolve().parents[2] /
                              "tetris_ai/train.py").read_text())
        cell = next(node for node in notebook.body
                    if isinstance(node, ast.FunctionDef) and
                    any(isinstance(child, ast.ClassDef) and
                        child.name == "PlacementEnv" for child in node.body))
        classes = [child for child in cell.body
                   if isinstance(child, ast.ClassDef) and
                   child.name in ("EnvCheckpoint", "PlacementEnv")]
        namespace = {
            "tetris_py": t, "dataclass": dataclass,
            "BOARD_WIDTH": 10, "BOARD_HEIGHT": 40, "PREVIEW_COUNT": 5,
        }
        exec(compile(ast.Module(body=classes, type_ignores=[]),
                     "train.py", "exec"), namespace)
        Env = namespace["PlacementEnv"]
        setup = SpinSetup(("X..", "G.."), ("overhang", "filled"),
                          "J", "S", "ZL")
        env = Env(spin_setups=(setup,), spin_chance=0.5,
                  clean_garbage_layers=(1, 2),
                  cheese_garbage_layers=(1, 3))
        selected = set()
        cheese_counts = set()
        for seed in range(40):
            rng = random.Random(seed ^ 0xA0761D6478BD642F)
            has_spin = rng.random() < 0.5
            if has_spin:
                rng.choice((setup,))
            clean, cheese = rng.randint(1, 2), rng.randint(1, 3)
            selected.add(has_spin)
            cheese_counts.add(cheese)
            obs = env.reset(seed)
            self.assertEqual(obs, env.reset(seed))
            self.assertTrue(all(row.count(int(t.Cell.Empty)) == 1
                                for row in obs["board"][:clean + cheese]))
            if has_spin:
                rendered, _, mirrored = setup.render(
                    seed, clean_garbage_layers=clean,
                    cheese_garbage_layers=cheese)
                self.assertEqual(obs["board"], tuple(
                    tuple(".IOTSZJLGX".index(cell) for cell in row)
                    for row in rendered))
                self.assertEqual(obs["active_piece"],
                                 int(t.PieceType.L if mirrored else t.PieceType.J))
                self.assertEqual(obs["hold_piece"],
                                 int(t.PieceType.Z if mirrored else t.PieceType.S))
                self.assertEqual(obs["preview"][:2], (
                    int(t.PieceType.S if mirrored else t.PieceType.Z),
                    int(t.PieceType.J if mirrored else t.PieceType.L)))
            else:
                self.assertIsNone(obs["hold_piece"])
                self.assertTrue(all(cell != int(t.Cell.Unclearable)
                                    for row in obs["board"] for cell in row))
                self.assertTrue(all(cell == int(t.Cell.Empty)
                                    for row in obs["board"][clean + cheese:]
                                    for cell in row))
        self.assertEqual(selected, {False, True})
        self.assertGreater(len(cheese_counts), 1)
        forced_spin = env.reset(42, top_is_spin=True)
        self.assertEqual(sum(cell == int(t.Cell.Unclearable)
                             for row in forced_spin["board"] for cell in row), 1)
        forced_garbage = env.reset(42, top_is_spin=False)
        self.assertEqual(sum(cell == int(t.Cell.Unclearable)
                             for row in forced_garbage["board"] for cell in row), 0)
        other = SpinSetup(("..X",), ("overhang",), "I", None, "")
        candidates = Env(spin_setups=(setup, other), spin_chance=1)
        chosen = set()
        for seed in range(30):
            rng = random.Random(seed ^ 0xA0761D6478BD642F)
            rng.random()  # Spin-chance draw precedes template selection.
            expected = rng.choice((setup, other))
            chosen.add(expected.active_piece)
            opening = candidates.reset(seed)
            rendered, _, mirrored = expected.render(seed)
            self.assertEqual(opening["board"], tuple(
                tuple(".IOTSZJLGX".index(cell) for cell in row)
                for row in rendered))
            active = ("L" if mirrored else "J") if expected is setup else "I"
            self.assertEqual(
                opening["active_piece"], int(t.PieceType.__members__[active]))
        self.assertEqual(chosen, {"I", "J"})
        with self.assertRaisesRegex(ValueError, "top_is_spin"):
            env.reset(42, spin_setup=setup, top_is_spin=False)
        self.assertTrue(env.done)
        with self.assertRaises(ValueError):
            Env(spin_chance=1)
        with self.assertRaises(ValueError):
            Env(cheese_garbage_layers=(3, 2))
        with self.assertRaises(ValueError):
            Env(spin_setups=(SpinSetup(("." * 10,) * 20,
                                      ("overhang",) * 20, "T", None, ""),),
                spin_chance=1, clean_garbage_layers=1).reset(42)

    def test_2009_guideline_profile(self):
        cfg = t.EngineConfig()
        cfg.guideline_2009 = True
        cfg.level = 3
        cfg.rotation = t.RotationRule.Off
        cfg.bag = t.BagRule.Random
        cfg.spin = t.SpinRule.Off
        cfg.attack = t.AttackRule.Season2
        game = t.Engine(cfg)
        game.reset(i_piece_seed())
        self.assertTrue(game.guideline_2009)
        self.assertEqual(game.level, 3)
        self.assertTrue(game.spawn_next_piece())
        self.assertEqual(game.active_piece.piece.type, t.PieceType.I)
        self.assertFalse(game.try_rotate_180())
        self.assertTrue(game.set_edit_mode(True))
        for col in range(10):
            if col not in (3, 4, 5, 6):
                self.assertTrue(game.paint_cell(0, col, t.Cell.Garbage))
        self.assertTrue(game.paint_cell(1, 9, t.Cell.Garbage))
        self.assertTrue(game.set_edit_mode(False))
        distance = game.hard_drop_distance()
        lock = game.hard_drop()
        self.assertEqual((lock.lines_cleared, lock.attack_sent), (1, 0))
        self.assertEqual(lock.score_delta, 300 + 2 * distance)
        game.set_level(5)
        self.assertEqual(game.level, 5)
        cfg.height = 24
        with self.assertRaises(ValueError):
            t.Engine(cfg)
        cfg.height = 40
        cfg.preview_count = 7
        with self.assertRaises(ValueError):
            t.Engine(cfg)

    def test_guideline_attack_rule_replaces_jstris(self):
        self.assertEqual(t.AttackRule.Guideline.name, "Guideline")
        self.assertFalse(hasattr(t.AttackRule, "Jstris"))
        cfg = t.EngineConfig()
        cfg.attack = t.AttackRule.Guideline
        game = t.Engine(cfg)
        game.reset(i_piece_seed())
        self.assertTrue(game.spawn_next_piece())
        self.assertTrue(game.set_edit_mode(True))
        for row in range(4):
            for col in range(10):
                if col != 5:
                    self.assertTrue(game.paint_cell(row, col, t.Cell.T))
        self.assertTrue(game.set_edit_mode(False))
        self.assertTrue(game.try_rotate_cw())
        result = game.hard_drop()
        self.assertEqual((result.lines_cleared, result.attack_packets), (4, [4]))

    def test_misamino_full_episode_matches_stepwise_search(self):
        cfg = t.EngineConfig()
        cfg.height = 10
        cfg.gravity = 0
        cfg.gravity_increase = 0
        game = t.Engine(cfg)
        reference = t.Engine(cfg)
        game.reset(42)
        reference.reset(42)
        self.assertTrue(reference.spawn_next_piece())
        bot = t.MisaMinoBot()
        with self.assertRaises(ValueError):
            bot.play_episode(game, piece_cap=0)
        episode = bot.play_episode(game, piece_cap=12)
        self.assertTrue(episode.turns)
        self.assertGreater(episode.reused_searches, 0)
        for turn in episode.turns:
            choice = bot.choose(reference)
            self.assertEqual(placement_key(turn.placement), placement_key(choice))
            lock = reference.apply_placement(choice)
            self.assertEqual((turn.lock.spin, turn.lock.attack_sent,
                              turn.lock.lines_cleared, turn.lock.score_delta),
                             (lock.spin, lock.attack_sent,
                              lock.lines_cleared, lock.score_delta))
            if not reference.game_over:
                reference.spawn_next_piece()
        self.assertEqual(episode.game_over, reference.game_over)
        self.assertEqual(episode.truncated,
                         not episode.game_over and len(episode.turns) == 12)
        self.assertEqual(game.board.render(), reference.board.render())
        self.assertEqual(game.preview_queue(), reference.preview_queue())
        self.assertEqual(game.score, reference.score)

    def test_misamino_bot_returns_replayable_legal_placements(self):
        cfg = t.EngineConfig()
        cfg.height = 10
        cfg.gravity = 0
        cfg.gravity_increase = 0
        game = t.Engine(cfg)
        limits = t.MisaMinoConfig()
        limits.search_depth = 2
        limits.beam_width = 4
        limits.node_budget = 1000
        limits.search_threads = 2
        bot = t.MisaMinoBot(limits)
        self.assertEqual(limits.search_threads, 2)
        self.assertIsNone(bot.choose(game))
        game.reset(17)
        self.assertTrue(game.spawn_next_piece())

        for _ in range(4):
            before = (game.board.render(), game.preview_queue(),
                      game.active_piece.pos.row, game.hold_piece, game.score)
            choice = bot.choose(game)
            self.assertIsInstance(choice, t.Placement)
            legal = (game.generate_active_piece_placement() +
                     game.generate_hold_piece_placement())
            self.assertEqual(sum(placement_key(a) == placement_key(choice)
                                 for a in legal), 1)
            self.assertEqual((game.board.render(), game.preview_queue(),
                              game.active_piece.pos.row, game.hold_piece,
                              game.score), before)
            self.assertTrue(game.apply_placement(choice).locked)
            if game.game_over or not game.spawn_next_piece():
                break

        limits.search_depth = 0
        with self.assertRaises(ValueError):
            t.MisaMinoBot(limits)

    def test_misamino_spin_path_matches_python_placement(self):
        cfg = t.EngineConfig()
        cfg.height = 24
        cfg.gravity = 0
        cfg.attack = t.AttackRule.Off
        cfg.spin = t.SpinRule.TOnly
        game = t.Engine(cfg)
        for seed in range(100):
            game.reset(seed)
            if game.preview_queue()[0] == t.PieceType.T:
                break
        else:
            self.fail("could not find a T-piece seed")
        self.assertTrue(game.spawn_next_piece())
        self.assertTrue(game.set_edit_mode(True))
        for col in range(10):
            if col != 8:
                self.assertTrue(game.paint_cell(0, col, t.Cell.Garbage))
        self.assertTrue(game.paint_cell(2, 7, t.Cell.Garbage))
        self.assertTrue(game.paint_cell(3, 0, t.Cell.Garbage))
        self.assertTrue(game.set_edit_mode(False))

        limits = t.MisaMinoConfig()
        limits.search_depth = 1
        bot = t.MisaMinoBot(limits)
        choice = bot.choose(game)
        legal = game.generate_active_piece_placement()
        self.assertEqual(sum(placement_key(p) == placement_key(choice)
                             for p in legal), 1)
        matching_pose = [p for p in legal if
                         (p.final_pos.row, p.final_pos.col,
                          p.final_rotation) ==
                         (-1, 7, t.Rotation.East)]
        self.assertEqual(len(matching_pose), 2)
        outcomes = []
        for p in matching_pose:
            clone = game.clone()
            result = clone.apply_placement(p)
            self.assertEqual(p.spin, result.spin)
            outcomes.append(p.spin)
        self.assertIn(t.SpinType.None_, outcomes)
        self.assertIn(t.SpinType.Mini, outcomes)
        result = game.apply_placement(choice)
        self.assertNotEqual(result.spin, t.SpinType.None_)
        self.assertEqual(result.lines_cleared, 1)

    def test_engine_observation_actions_and_explicit_spawn(self):
        cfg = t.EngineConfig()
        cfg.gravity = 0
        cfg.rotation = t.RotationRule.SRSX
        cfg.attack = t.AttackRule.Off
        game = t.Engine(cfg)
        game.reset(19)
        self.assertIsNone(game.active_piece)
        self.assertEqual((game.board.width, game.board.height), (10, 40))
        self.assertEqual(game.board.get(0, 0), t.Cell.Empty)
        self.assertEqual(game.board.render(), [[t.Cell.Empty] * 10 for _ in range(40)])
        queue = game.preview_queue()
        self.assertEqual(len(queue), cfg.preview_count)
        self.assertTrue(game.spawn_next_piece())
        self.assertTrue(hasattr(t, "Placement"))
        before = (game.board.render(), game.preview_queue(), game.active_piece.pos.row)
        actions = game.generate_active_piece_placement()
        held_actions = game.generate_hold_piece_placement()
        self.assertTrue(actions)
        self.assertTrue(held_actions)
        self.assertFalse(actions[0].use_hold)
        self.assertTrue(held_actions[0].use_hold)
        landing_cells = actions[0].cells()
        self.assertEqual(len(landing_cells), 4)
        self.assertTrue(all(0 <= cell.row < 40 and 0 <= cell.col < 10
                            for cell in landing_cells))
        rotated = next(p for p in actions if p.final_rotation != t.Rotation.North)
        rotated_cells = rotated.cells()
        self.assertEqual(len({(cell.row, cell.col) for cell in rotated_cells}), 4)
        rotated_branch = game.clone()
        rotated_lock = rotated_branch.apply_placement(rotated)
        self.assertTrue(rotated_lock.locked)
        if rotated_lock.lines_cleared == 0:
            self.assertTrue(all(rotated_branch.board.get(cell.row, cell.col)
                                == t.Cell(rotated.type.value + 1)
                                for cell in rotated_cells))
        self.assertEqual((game.board.render(), game.preview_queue(),
                          game.active_piece.pos.row), before)
        branch = game.clone()
        lock = branch.apply_placement(held_actions[0])
        self.assertTrue(lock.locked)
        self.assertEqual(lock.score_delta, branch.score - game.score)
        if lock.lines_cleared == 0:
            self.assertTrue(all(branch.board.get(cell.row, cell.col)
                                == t.Cell(held_actions[0].type.value + 1)
                                for cell in held_actions[0].cells()))
        self.assertEqual(game.board.render(), before[0])
        branch.restore(game)
        self.assertEqual(branch.preview_queue(), game.preview_queue())
        self.assertEqual(branch.board.render(), game.board.render())
        active = game.active_piece
        self.assertIsInstance(active.pos.row, int)
        self.assertEqual(len(active.piece.current_shape()), 4)
        self.assertEqual(len(active.piece.render()), 4)
        self.assertEqual(game.render()[active.pos.row + active.piece.current_shape()[0].row]
                         [active.pos.col + active.piece.current_shape()[0].col],
                         active.piece.cell_type)
        self.assertIsNotNone(game.ghost_piece)
        self.assertTrue(game.try_move_left())
        self.assertEqual(game.active_piece.pos.col, active.pos.col - 1)
        self.assertTrue(game.try_rotate_cw())
        self.assertTrue(game.hold())
        self.assertEqual(game.hold_piece, active.piece.type)
        self.assertFalse(game.hold())
        self.assertTrue(game.hard_drop().locked)
        self.assertIsNone(game.active_piece)  # Engine.hard_drop does not spawn.
        self.assertIsNotNone(game.last_lock_result)
        self.assertTrue(game.spawn_next_piece())

        other = t.Engine(cfg)
        other.reset(19)
        self.assertEqual(other.preview_queue(), queue)
        cfg.rotation = t.RotationRule.Off
        off = t.Engine(cfg)
        off.reset(19)
        off.spawn_next_piece()
        self.assertFalse(off.try_rotate_cw())

    def test_queued_garbage_and_seeded_reset(self):
        cfg = t.EngineConfig()
        cfg.gravity = 0
        cfg.attack = t.AttackRule.Off
        cfg.garbage_delay = 2
        game = t.Engine(cfg)
        game.reset(4)
        game.receive_garbage(t.GarbageEvent(12, 4))
        self.assertEqual((game.pending_garbage_lines, game.pending_garbage_packets), (12, 1))
        self.assertEqual((game.pending_garbage()[0].event.hole_col,
                          game.pending_garbage()[0].delay_ticks), (4, 2))
        game.spawn_next_piece()
        self.assertTrue(game.hard_drop().locked)
        self.assertEqual(game.pending_garbage_lines, 12)
        game.tick()
        game.tick()
        game.spawn_next_piece()
        self.assertTrue(game.hard_drop().locked)
        self.assertEqual(game.pending_garbage_lines, 4)
        self.assertEqual(game.board.get(0, 4), t.Cell.Empty)
        self.assertEqual(game.board.get(0, 5), t.Cell.Garbage)
        game.reset(4)
        self.assertEqual((game.pending_garbage_lines, game.board.max_height()), (0, 0))

    def test_custom_attack_score_and_editor(self):
        cfg = t.EngineConfig()
        cfg.gravity = 0
        cfg.attack = t.AttackRule.Custom
        cfg.spin = t.SpinRule.Off
        cfg.attacks.normal = [0, 3, 1, 2, 4]
        cfg.scores.normal = [0, 200, 300, 500, 800]
        game = t.Engine(cfg)
        game.reset(i_piece_seed())
        game.spawn_next_piece()
        paint_single_well(game)
        lock = game.hard_drop()
        self.assertEqual(lock.lines_cleared, 1)
        self.assertEqual((lock.attack_sent, lock.attack_packets), (3, [3]))
        self.assertGreaterEqual(game.score, 200)
        self.assertEqual(game.last_lock_result.lines_cleared, 1)

    def test_match_routing_and_solo_objective(self):
        cfg = t.EngineConfig()
        cfg.gravity = 0
        cfg.garbage_delay = 0
        cfg.attack = t.AttackRule.Custom
        cfg.attacks.normal = [0, 3, 1, 2, 4]
        seed = i_piece_seed()
        match = t.Match([cfg, cfg])

        def send_single():
            match.start(seed)
            self.assertEqual(match.player(0).preview_queue(),
                             match.player(1).preview_queue())
            match.set_target(0, 1)
            self.assertEqual(len(match.tick()), 2)
            paint_single_well(match.player(0))
            lock = match.hard_drop(0)
            self.assertEqual(lock.attack_sent, 3)
            self.assertEqual(match.player(1).pending_garbage_lines, 3)
            return match.player(1).pending_garbage()[0].event.hole_col

        self.assertEqual(send_single(), send_single())
        self.assertTrue(match.hard_drop(1).locked)
        self.assertEqual(match.player(1).pending_garbage_lines, 0)
        match.set_target(0, None)

        settings = t.SoloSettings()
        settings.mode = t.SoloMode.Sprint
        settings.line_goal = 1
        solo = t.SoloSession(cfg, settings)
        solo.start(seed)
        paint_single_well(solo.engine)
        self.assertEqual(solo.hard_drop().lines_cleared, 1)
        self.assertTrue(solo.finished)
        self.assertTrue(solo.succeeded)
        self.assertIsNone(solo.engine.active_piece)
        self.assertFalse(solo.tick().locked)
        self.assertEqual(solo.elapsed_ticks, 0)

    def test_invalid_configuration(self):
        cfg = t.EngineConfig()
        cfg.width = 3
        with self.assertRaises(ValueError):
            t.Engine(cfg)
        cfg.width = 10
        cfg.attacks.combo_multiplier = float("nan")
        with self.assertRaises(ValueError):
            t.Engine(cfg)
        with self.assertRaises(ValueError):
            t.Match([t.EngineConfig()])


if __name__ == "__main__":
    unittest.main()
