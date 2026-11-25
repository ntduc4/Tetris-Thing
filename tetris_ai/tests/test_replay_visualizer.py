"""Saved replay rendering must not need training or alter the live environment."""

import ast
import copy
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from xml.etree import ElementTree

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from replay_visualizer import (
    COLORS, GIF_BACKGROUND, GIF_COLOR_INDEX, GIF_GRID, board_svg,
    reconstruct_replay, render_replay, render_replay_image, save_replay_archive,
)


NOTEBOOK = ast.parse((Path(__file__).resolve().parents[1] / "train.py").read_text())


class FakePlacement:
    type = SimpleNamespace(name="I")
    use_hold = False
    final_rotation = SimpleNamespace(name="North")
    moves = (SimpleNamespace(name="HardDrop"),)

    def cells(self):
        return (SimpleNamespace(row=0, col=0),)


class FakeEnv:
    instances = []

    def __init__(self, piece_cap=5):
        self.piece_cap = piece_cap
        self.action = FakePlacement()
        self.steps = 0
        FakeEnv.instances.append(self)

    def reset(self, seed):
        assert seed == 42
        return {
            "board": ((0, 0), (0, 0)), "active_piece": 0, "hold_piece": None,
            "preview": (1, 2, 3), "can_hold": True, "score": 0,
            "lines_cleared": 0, "pieces_placed": 0, "combo": 0,
            "back_to_back_count": 0, "pending_garbage_lines": 0,
        }

    def legal_placements(self):
        return (self.action,)

    def step(self, action):
        assert action is self.action
        self.steps += 1
        return {
            "board": ((1, 0), (0, 0)), "active_piece": None, "hold_piece": 2,
            "preview": (), "can_hold": False, "score": 5,
            "lines_cleared": 0, "pieces_placed": 1, "combo": 0,
            "back_to_back_count": 0, "pending_garbage_lines": 0,
        }, 0.5, True, False, {
            "lines_cleared": 0, "attack_sent": 3, "spin": 0,
        }


def sample_replay():
    return {"seed": 42, "frames": [{
        "before": [[0, 0], [0, 0]],
        "after": [[1, 0], [0, 0]],
        "landing": [[0, 0]], "piece": "I", "use_hold": False,
        "rotation": "North", "moves": ["HardDrop"],
        "score": 5, "lines": 0, "reward": 0.5,
    }]}


class ReplayVisualizerTests(unittest.TestCase):
    def test_recovers_real_queue_and_attack_from_separate_environment(self):
        replay = sample_replay()
        original = copy.deepcopy(replay)
        live_env = FakeEnv()
        states = reconstruct_replay(replay, lambda: FakeEnv())
        self.assertEqual(replay, original)
        self.assertEqual(live_env.steps, 0)
        self.assertEqual(FakeEnv.instances[-1].steps, 1)
        self.assertEqual(states[0]["before"]["preview"], (1, 2, 3))
        self.assertIsNone(states[0]["before"]["hold_piece"])
        self.assertEqual(states[0]["after"]["hold_piece"], 2)
        self.assertEqual((states[0]["attack_before"], states[0]["attack_after"]),
                         (0, 3))

    def test_archives_in_memory_evaluation_tuple_boards(self):
        replay = sample_replay()
        frame = replay["frames"][0]
        frame["before"] = tuple(tuple(row) for row in frame["before"])
        frame["after"] = tuple(tuple(row) for row in frame["after"])
        replay["episodes_trained"] = 10_000
        self.assertEqual(len(reconstruct_replay(replay, FakeEnv)), 1)
        with tempfile.TemporaryDirectory() as directory:
            json_path, gif_path = save_replay_archive(replay, FakeEnv, directory)
            self.assertEqual(json.loads(json_path.read_text())["frames"][0]["before"],
                             [[0, 0], [0, 0]])
            with Image.open(gif_path) as gif:
                self.assertEqual(gif.n_frames, 2)
        frame["after"] = ((0, 0), (0, 0))
        with self.assertRaisesRegex(ValueError, "diverged after placement"):
            reconstruct_replay(replay, FakeEnv)

    def test_rejects_diverging_boards_and_actions_instead_of_faking_stats(self):
        replay = sample_replay()
        replay["frames"][0]["landing"] = [[1, 1]]
        with self.assertRaisesRegex(ValueError, "matched 0 legal actions"):
            reconstruct_replay(replay, FakeEnv)
        replay = sample_replay()
        replay["frames"][0]["after"][0][0] = 0
        with self.assertRaisesRegex(ValueError, "diverged after placement"):
            reconstruct_replay(replay, FakeEnv)

    def test_board_grid_stays_twenty_rows_while_minos_extend_above_it(self):
        board = [[0, 0] for _ in range(40)]
        board[0][0] = 1
        board[19][1] = 3
        board[20][1] = 8
        board[23][0] = 5
        board[30][1] = 9
        svg = board_svg(
            board, [(19, 0), (20, 0), (23, 0), (24, 0)], "T",
            active_cells=((22, 1), (25, 1)), active_piece=1,
        )
        root = ElementTree.fromstring(svg)
        self.assertEqual(root.attrib["viewBox"], "0 0 48 480")
        self.assertIn("overflow:visible", root.attrib["style"])
        self.assertEqual(svg.count('fill="#151f32"'), 40)
        self.assertIn(f'fill="{COLORS[1]}"', svg)
        self.assertIn(f'fill="{COLORS[3]}"', svg)
        self.assertIn(f'fill="{COLORS[8]}"', svg)  # Row 20, above the grid.
        self.assertIn(f'fill="{COLORS[5]}"', svg)  # Row 23, over the UI.
        self.assertIn('x="25" y="-23" width="22"', svg)
        self.assertIn('x="1" y="-95" width="22"', svg)
        self.assertIn('x="25" y="-263" width="22"', svg)
        self.assertEqual(svg.count('fill-opacity=".25"'), 4)
        self.assertIn('x="2" y="-118" width="20"', svg)
        self.assertIn(f'fill="{COLORS[2]}"', svg)  # Active mino above grid.
        self.assertIn('x="1" y="457" width="22"', svg)  # Row 0 at bottom.
        self.assertEqual(
            ElementTree.fromstring(board_svg([[0, 0]], [(25, 1)], "O"))
            .attrib["viewBox"],
            root.attrib["viewBox"],
        )

    def test_gif_draws_minos_over_header_without_expanding_board_grid(self):
        replay = sample_replay()
        board = [[0] * 10 for _ in range(40)]
        board[20][3] = 8
        board[23][4] = 5
        board[24][5] = 3
        board[26][6] = 2
        replay["episodes_trained"] = 10_000
        replay["frames"][0]["before"] = board
        replay["frames"][0]["landing"] = [[24, 0]]
        observation = FakeEnv().reset(42)
        observation["active_piece"] = 0
        states = [{
            "before": observation, "after": observation,
            "active_cells_before": ((22, 2),), "active_cells_after": (),
            "attack_before": 0, "attack_after": 0,
            "attack_this_move": 0, "spin": 0, "top_out": False,
        }]
        image = render_replay_image(replay, states, 0)
        self.assertEqual(image.size, (650, 800))
        x, y = (650 - 240) // 2, 130
        self.assertEqual(image.getpixel((x + 3 * 24 + 12, y - 24 + 12)),
                         GIF_COLOR_INDEX[COLORS[8]])
        self.assertEqual(image.getpixel((x + 5 * 24 + 12, 22)),
                         GIF_COLOR_INDEX[COLORS[3]])
        self.assertEqual(image.getpixel((x + 6 * 24 + 12, 1)),
                         GIF_COLOR_INDEX[GIF_BACKGROUND])
        self.assertEqual(image.getpixel((x + 6 * 24 + 12, y + 12)),
                         GIF_COLOR_INDEX[GIF_GRID])

    def test_archives_immutable_json_and_two_gif_frames_per_placement(self):
        replay = sample_replay()
        replay["episodes_trained"] = 10_000
        replay["training_method"] = "q_learning"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            json_path, gif_path = save_replay_archive(replay, FakeEnv, root)
            self.assertEqual(json_path.name, "episodes_010000.json")
            self.assertEqual(gif_path.name, "episodes_010000.gif")
            self.assertEqual(json.loads(json_path.read_text()), replay)
            with Image.open(gif_path) as gif:
                self.assertEqual(gif.size, (650, 800))
                self.assertEqual(gif.n_frames, 2)
                gif.seek(0)
                self.assertEqual(gif.info["duration"], 160)
                gif.seek(1)
                self.assertEqual(gif.info["duration"], 220)
            self.assertEqual(list(root.glob(".replay-*")), [])
            original = gif_path.read_bytes()
            with self.assertRaises(FileExistsError):
                save_replay_archive(replay, FakeEnv, root)
            self.assertEqual(gif_path.read_bytes(), original)
            replay["episodes_trained"] = 10_001
            with self.assertRaisesRegex(ValueError, "10,000-episode"):
                save_replay_archive(replay, FakeEnv, root)

    def test_notebook_10k_helper_evaluates_separately_from_training(self):
        helper_cell = copy.deepcopy(next(
            cell for cell in NOTEBOOK.body
            if isinstance(cell, ast.FunctionDef)
            and {"checkpoint_dir", "env", "evaluate_episode"} <=
                {arg.arg for arg in cell.args.args}
        ))
        helper_cell.decorator_list = []
        namespace = {}
        exec(compile(ast.Module(body=[helper_cell], type_ignores=[]),
                     "train.py", "exec"), namespace)
        model = object()
        evaluated = []
        live_env = FakeEnv()
        with tempfile.TemporaryDirectory() as directory:
            archive = namespace["_"](
                checkpoint_dir=Path(directory), env=live_env,
                evaluate_episode=lambda passed_model:
                    evaluated.append(passed_model) or sample_replay(),
            )[0]
            json_path, gif_path = archive(model, 10_000, "q_learning")
            self.assertEqual(evaluated, [model])
            self.assertEqual(live_env.steps, 0)
            self.assertEqual(json.loads(json_path.read_text())["training_method"],
                             "q_learning")
            self.assertEqual(json.loads(json_path.read_text())["episodes_trained"],
                             10_000)
            with Image.open(gif_path) as gif:
                self.assertEqual(gif.n_frames, 2)

    def test_before_and_after_show_corresponding_hold_queue_and_stats(self):
        replay = sample_replay()
        states = reconstruct_replay(replay, FakeEnv)
        before = render_replay(replay, states, 0)
        after = render_replay(replay, states, 0, after=True)
        self.assertIn("BEFORE / LANDING", before)
        self.assertIn("NEXT 3", before)
        self.assertIn("O piece", before)
        self.assertIn('fill-opacity=".25"', before)
        self.assertIn("20×10 grid", before)
        self.assertIn("max-width:790px;overflow:hidden", before)
        self.assertIn("z-index:2;overflow:visible", before)
        self.assertIn("ready", before)
        self.assertIn("AFTER LOCK", after)
        self.assertNotIn("NEXT 3", after)
        self.assertNotIn('fill-opacity=".25"', after)
        self.assertIn("TOP OUT", after)
        self.assertIn("T piece", after)  # Piece moved to hold.
        self.assertIn("+3 attack", after)
        replay["frames"][0]["rotation"] = "<script>"
        self.assertIn("&lt;script&gt;", render_replay(replay, states, 0))

    def test_notebook_color_loader_does_not_depend_on_training_run(self):
        loader = copy.deepcopy(next(
            cell for cell in NOTEBOOK.body
            if isinstance(cell, ast.FunctionDef)
            and {"checkpoint_dir", "env", "refresh_color_replay"} <=
                {arg.arg for arg in cell.args.args}
        ))
        self.assertNotIn("trained_model", {arg.arg for arg in loader.args.args})
        self.assertNotIn("replay_data", {arg.arg for arg in loader.args.args})

        loader.decorator_list = []
        namespace = {}
        exec(compile(ast.Module(body=[loader], type_ignores=[]),
                     "train.py", "exec"), namespace)
        html = []
        text = []
        mo = SimpleNamespace(
            ui=SimpleNamespace(
                slider=lambda **kwargs: SimpleNamespace(value=kwargs["value"]),
                switch=lambda **kwargs: SimpleNamespace(value=False),
            ),
            hstack=lambda items, **kwargs: items,
            md=lambda message: text.append(message),
            Html=lambda content: html.append(content),
        )
        with tempfile.TemporaryDirectory() as directory:
            replay_path = Path(directory) / "replays/eval.json"
            replay_path.parent.mkdir()
            original = json.dumps(sample_replay())
            replay_path.write_text(original)
            live_env = FakeEnv()
            after_lock, data, slider, states = namespace["_"](
                checkpoint_dir=Path(directory), env=live_env, mo=mo,
                refresh_color_replay=SimpleNamespace(value=False),
            )
            self.assertEqual(len(states), 1)
            self.assertEqual(data["seed"], 42)
            self.assertEqual(slider.value, 0)
            self.assertEqual(live_env.steps, 0)
            self.assertEqual(replay_path.read_text(), original)

            viewer = copy.deepcopy(next(
                cell for cell in NOTEBOOK.body
                if isinstance(cell, ast.FunctionDef)
                and {"color_after_lock", "color_replay_data", "color_replay_states"} <=
                    {arg.arg for arg in cell.args.args}
            ))
            viewer.decorator_list = []
            exec(compile(ast.Module(body=[viewer], type_ignores=[]),
                         "train.py", "exec"), namespace)
            namespace["_"](
                color_after_lock=after_lock, color_replay_data=data,
                color_replay_slider=slider, color_replay_states=states, mo=mo,
            )
            self.assertIn("NEXT 3", html[-1])
            after_lock.value = True
            namespace["_"](
                color_after_lock=after_lock, color_replay_data=data,
                color_replay_slider=slider, color_replay_states=states, mo=mo,
            )
            self.assertIn("AFTER LOCK", html[-1])
            self.assertEqual(replay_path.read_text(), original)

        namespace["_"](
            color_after_lock=after_lock, color_replay_data={"frames": []},
            color_replay_slider=slider, color_replay_states=[], mo=mo,
        )
        self.assertIn("No saved placements", text[-1])


if __name__ == "__main__":
    unittest.main()
