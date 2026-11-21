"""Smoke and regression tests for the optional headless Python module."""

import unittest

import tetris_py as t


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
    )


class BindingsTests(unittest.TestCase):
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
        self.assertEqual((result.lines_cleared, result.attack_packets), (4, [14]))

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
        bot = t.MisaMinoBot(limits)
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
            outcomes.append(clone.apply_placement(p).spin)
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
