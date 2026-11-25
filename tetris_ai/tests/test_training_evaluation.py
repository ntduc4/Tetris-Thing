"""Exercise notebook orchestration without running ROCm or real training."""

import ast
import copy
from html import escape
import json
import math
import os
import pathlib
from pathlib import Path
import re
import tempfile
from types import SimpleNamespace
import unittest
from xml.etree import ElementTree

import numpy as np
import torch


NOTEBOOK = ast.parse((Path(__file__).resolve().parents[1] / "train.py").read_text())


def notebook_helper(name, namespace):
    cell = next(
        node for node in NOTEBOOK.body
        if isinstance(node, ast.FunctionDef)
        and any(isinstance(inner, ast.FunctionDef) and inner.name == name
                for inner in node.body)
    )
    function = next(
        inner for inner in cell.body
        if isinstance(inner, ast.FunctionDef) and inner.name == name
    )
    exec(compile(ast.Module(body=[function], type_ignores=[]), "train.py", "exec"),
         namespace)
    return namespace[name]


def training_loop(namespace):
    cell = next(
        node for node in NOTEBOOK.body
        if isinstance(node, ast.FunctionDef)
        and {arg.arg for arg in node.args.args} >=
            {"N_EPISODES", "save_training_model", "evaluate_benchmark"}
    )
    cell = copy.deepcopy(cell)
    cell.decorator_list = []
    exec(compile(ast.Module(body=[cell], type_ignores=[]), "train.py", "exec"),
         namespace)
    return namespace["_"]


def sample_evaluation():
    return {
        "seeds": [987654321, 987654322, 987654323],
        "piece_cap": 3,
        "games": [{"seed": 987654321, "score": 20, "pieces_placed": 2}],
        "mean_score": 20.0,
        "mean_lines_cleared": 1.0,
        "mean_attack_sent": 4.0,
        "mean_pieces_placed": 2.0,
        "piece_cap_rate": 0.0,
    }


class FakeEnv:
    resets = []

    def __init__(self, piece_cap=3):
        self.piece_cap = piece_cap

    def reset(self, seed):
        self.seed = seed
        self.pieces = 0
        self.score = 0
        FakeEnv.resets.append(seed)
        return self.observation()

    def observation(self):
        return {"board": ((0, 0), (int(self.pieces > 0), 0)),
                "score": self.score}

    def legal_placements(self):
        return (0, 1)

    def step(self, action):
        self.pieces += 1
        self.score += 10 + action
        terminated = self.seed % 2 == 0 and self.pieces == 2
        truncated = not terminated and self.pieces == self.piece_cap
        return self.observation(), 0, terminated, truncated, {
            "lines_cleared": int(self.pieces % 2 == 1),
            "attack_sent": 2,
            "pieces_placed": self.pieces,
            "spin": int(self.pieces == 1),
            "perfect_clear": self.pieces == 1,
        }


class ConstantModel(torch.nn.Module):
    def forward(self, batch):
        if self.training or torch.is_grad_enabled():
            raise AssertionError("Evaluation must not train or track gradients")
        return -batch[:, 0]


class TrainingEvaluationTests(unittest.TestCase):
    def test_demo_step_is_opt_in_and_viewer_starts_at_opening(self):
        cell = next(
            node for node in NOTEBOOK.body
            if isinstance(node, ast.FunctionDef)
            and {"run_demo_step", "set_viewer_checkpoint"} <=
                {arg.arg for arg in node.args.args}
        )
        cell = copy.deepcopy(cell)
        cell.decorator_list = []
        namespace = {}
        exec(compile(ast.Module(body=[cell], type_ignores=[]),
                     "train.py", "exec"), namespace)
        demo_step = namespace["_"]

        class DemoEnv(FakeEnv):
            @property
            def pieces_placed(self):
                return self.pieces

            def checkpoint(self):
                return SimpleNamespace(pieces_placed=self.pieces)

        env = DemoEnv()
        observation = env.reset(42)
        opening = env.checkpoint()
        published = []
        device = torch.device("cpu")
        model = torch.nn.Sequential(torch.nn.Linear(1, 1),
                                    torch.nn.Flatten(0, 1))
        optimizer = torch.optim.SGD(model.parameters(), lr=1e-3)

        def run(clicked):
            return demo_step(
                action=0, chosen_index=0, device=device,
                encode=lambda state, action: np.array([action + 1], dtype=np.float32),
                env=env, gamma=0.99,
                np=np, observation=observation, optimizer=optimizer,
                q_online=model, run_demo_step=SimpleNamespace(value=clicked),
                scores=torch.zeros(2), set_viewer_checkpoint=published.append,
                torch=torch,
            )

        run(False)
        self.assertEqual(published, [])
        self.assertEqual(opening.pieces_placed, 0)
        self.assertEqual(env.pieces, 0)
        run(True)
        self.assertEqual((env.pieces, published[-1].pieces_placed), (1, 1))
        run(True)
        self.assertEqual((env.pieces, len(published)), (1, 1))

    def test_training_charts_use_round_y_axis_intervals(self):
        chart = notebook_helper("evaluation_chart", {
            "escape": escape, "math": math,
        })
        examples = (
            (42_814, "count", 1, ["0", "10,000", "20,000", "30,000",
                                   "40,000", "50,000"]),
            (51_200, "count", 1, ["0", "10,000", "20,000", "30,000",
                                   "40,000", "50,000", "60,000"]),
            (500, "count", 1, ["0", "100", "200", "300", "400", "500"]),
            (196, "count", 1, ["0", "50", "100", "150", "200"]),
            (39, "count", 1, ["0", "10", "20", "30", "40"]),
            (2, "count", 1, ["0.0", "0.5", "1.0", "1.5", "2.0"]),
            (0, "count", 1, ["0", "1", "2", "3", "4"]),
            (0.392, "ratio", 1, ["0.00", "0.10", "0.20", "0.30", "0.40"]),
            (1, "percent", 100, ["0%", "20%", "40%", "60%", "80%", "100%"]),
        )
        for maximum, tick_format, scale, expected in examples:
            with self.subTest(maximum=maximum, tick_format=tick_format):
                svg = chart([{"episodes_trained": 100, "value": maximum}],
                            "Metric", "value", tick_format, scale)
                root = ElementTree.fromstring(svg)
                labels = [
                    text.text for text in root.findall("text")
                    if text.get("text-anchor") == "end"
                ]
                self.assertEqual(labels, expected)

    def test_training_charts_use_even_episode_ticks(self):
        chart = notebook_helper("evaluation_chart", {
            "escape": escape, "math": math,
        })
        examples = (
            ([100, 1200, 5100, 51100, 51200],
             [0, 10000, 20000, 30000, 40000, 50000, 60000]),
            ([10300, 12000, 20200],
             [10000, 12000, 14000, 16000, 18000, 20000, 22000]),
            ([100, 500], [100, 200, 300, 400, 500]),
            ([100], [100]),
        )
        for episodes, expected in examples:
            with self.subTest(episodes=episodes):
                records = [
                    {"episodes_trained": episode, "value": index + 1}
                    for index, episode in enumerate(episodes)
                ]
                root = ElementTree.fromstring(
                    chart(records, "Metric", "value")
                )
                ticks = [
                    text for text in root.findall("text")
                    if text.get("y") == "238"
                ]
                self.assertEqual(
                    [int(text.text.replace(",", "")) for text in ticks], expected
                )
                gridlines = [
                    line for line in root.findall("line")
                    if line.get("x1") == line.get("x2")
                ]
                self.assertEqual(len(gridlines), len(ticks))
                positions = [float(text.get("x")) for text in ticks]
                if len(positions) > 2:
                    spacing = [
                        second - first
                        for first, second in zip(positions, positions[1:])
                    ]
                    self.assertLess(max(spacing) - min(spacing), 0.2)

    def test_benchmark_is_greedy_fixed_seed_and_restores_model_mode(self):
        namespace = {
            "device": torch.device("cpu"),
            "encode": lambda observation, action: np.array(
                [float(action)], dtype=np.float32),
            "env": FakeEnv(),
            "np": np,
            "torch": torch,
        }
        # The function closes over EVAL_SEEDS in its notebook cell.
        cell = next(node for node in NOTEBOOK.body
                    if isinstance(node, ast.FunctionDef)
                    and any(isinstance(inner, ast.FunctionDef)
                            and inner.name == "evaluate_benchmark"
                            for inner in node.body))
        statement = next(node for node in cell.body if isinstance(node, ast.Assign)
                         and any(isinstance(target, ast.Name)
                                 and target.id == "EVAL_SEEDS"
                                 for target in node.targets))
        exec(compile(ast.Module(body=[statement], type_ignores=[]), "train.py", "exec"),
             namespace)
        evaluate_benchmark = notebook_helper("evaluate_benchmark", namespace)
        model = ConstantModel()
        first = evaluate_benchmark(model)
        second = evaluate_benchmark(model)
        self.assertTrue(model.training)
        self.assertEqual(first, second)
        self.assertEqual(first["seeds"], [987654321, 987654322, 987654323])
        self.assertEqual(first["mean_pieces_placed"], 8 / 3)
        self.assertEqual(first["mean_score"], 80 / 3)
        self.assertEqual(first["mean_lines_cleared"], 5 / 3)
        self.assertEqual(first["mean_attack_sent"], 16 / 3)
        self.assertEqual(first["piece_cap_rate"], 2 / 3)
        self.assertEqual(first["attack_per_piece"], 2.0)
        self.assertEqual(first["lines_per_piece"], 5 / 8)
        self.assertEqual(first["games"][1]["max_stack_height"], 2)
        self.assertTrue(first["games"][1]["top_out"])
        self.assertEqual(FakeEnv.resets[-6:], first["seeds"] * 2)

    def test_checkpoint_latest_100_archive_1000_and_resume(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            namespace = {
                "_json": json, "_os": os, "_pathlib": pathlib, "_tempfile": tempfile,
                "checkpoint_root": root, "checkpoint_dir": root / "shared",
                "training_methods": ("q_learning", "monte_carlo", "behaviour_cloning"),
                "PlacementQFunction": lambda feature_dim: torch.nn.Linear(feature_dim, 1),
                "device": torch.device("cpu"), "feature_dim": 4,
                "ENCODING_VERSION": 3, "gamma": 0.99, "torch": torch,
            }
            init = notebook_helper("init_training_model", namespace)
            save = notebook_helper("save_training_model", namespace)
            model, optimizer, completed = init("q_learning")
            self.assertEqual(completed, 0)
            model(torch.ones(1, 4)).sum().backward()
            optimizer.step()
            evaluation = sample_evaluation()
            latest, archive = save(
                "q_learning", model, optimizer, 100, evaluation)
            self.assertIsNone(archive)
            self.assertTrue(latest.is_file())
            self.assertFalse((root / "shared/models").exists())
            first = torch.load(latest, map_location="cpu", weights_only=True)
            self.assertEqual(first["evaluation"]["mean_score"], 20.0)
            self.assertEqual(first["episodes_trained"], 100)
            model2, optimizer2, completed2 = init("q_learning")
            self.assertEqual(completed2, 100)
            self.assertTrue(optimizer2.state)
            self.assertTrue(torch.equal(next(model.parameters()),
                                        next(model2.parameters())))
            with self.assertRaises(ValueError):
                save("q_learning", model, optimizer, 200, evaluation,
                     save_history=True)
            _, archive = save("q_learning", model, optimizer, 1000, evaluation,
                              save_history=True)
            self.assertTrue(archive.is_file())
            save("q_learning", model, optimizer, 1100, evaluation)
            self.assertEqual(torch.load(archive, weights_only=True)["episodes_trained"],
                             1000)
            self.assertEqual(torch.load(latest, weights_only=True)["episodes_trained"],
                             1100)
            with self.assertRaises(FileExistsError):
                save("q_learning", model, optimizer, 1000, evaluation,
                     save_history=True)
            self.assertEqual(torch.load(latest, weights_only=True)["episodes_trained"],
                             1100)
            self.assertFalse(list((root / "shared").glob(".checkpoint-*")))
            _, switched_optimizer, switched_completed = init("behaviour_cloning")
            self.assertEqual(switched_completed, 1100)
            self.assertFalse(switched_optimizer.state)
            records = [
                json.loads(line) for line in (root / "shared/evaluations.jsonl")
                .read_text().splitlines()
            ]
            self.assertEqual([r["episodes_trained"] for r in records],
                             [100, 1000, 1100])

    def test_training_loop_saves_at_global_boundaries_and_final_partial(self):
        with tempfile.TemporaryDirectory() as directory:
            saves = []
            seeds = []
            eval_calls = []
            model = SimpleNamespace(train=lambda: None)
            optimizer = object()
            namespace = {}
            loop = training_loop(namespace)

            def save(method, passed_model, passed_optimizer, count, evaluation,
                     save_history=False):
                self.assertIs(passed_model, model)
                self.assertIs(passed_optimizer, optimizer)
                saves.append((count, save_history))
                return Path("latest.pt"), (Path(f"episodes_{count:06d}.pt")
                                           if save_history else None)

            result = loop(
                N_EPISODES=215,
                archive_evaluation_replay=lambda *_: self.fail("No 10k milestone"),
                bc_train_button=SimpleNamespace(value=False),
                checkpoint_dir=Path(directory),
                env=FakeEnv(),
                evaluate_benchmark=lambda _: eval_calls.append(1) or sample_evaluation(),
                gamma=0.99,
                init_training_model=lambda method: (model, optimizer, 940),
                mc_train_button=SimpleNamespace(value=False),
                mo=SimpleNamespace(stop=lambda condition, _: None,
                                   md=lambda text: text),
                q_train_button=SimpleNamespace(value=True),
                save_training_model=save,
                train_an_episode_behaviour_cloning=lambda **kwargs: None,
                train_an_episode_monte_carlo=lambda **kwargs: None,
                train_an_episode_q_learning=lambda **kwargs: seeds.append(kwargs["seed"]),
            )
            self.assertEqual(seeds, list(range(10_940, 11_155)))
            self.assertEqual(saves, [(1000, True), (1100, False), (1155, False)])
            self.assertEqual(len(eval_calls), 3)
            self.assertEqual(result[0], 1155)
            self.assertIsNone(result[1])
            self.assertEqual(result[2], Path("latest.pt"))

    def test_training_loop_archives_replay_once_at_each_crossed_10k_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            saves = []
            archives = []
            seeds = []
            model = SimpleNamespace(train=lambda: None)
            result = training_loop({})(
                N_EPISODES=4,
                archive_evaluation_replay=lambda passed_model, count, method: (
                    archives.append((passed_model, count, method))
                    or (root / f"replays/episodes_{count:06d}.json",
                        root / f"replays/episodes_{count:06d}.gif")
                ),
                bc_train_button=SimpleNamespace(value=False),
                checkpoint_dir=root, env=FakeEnv(),
                evaluate_benchmark=lambda _: sample_evaluation(),
                gamma=0.99,
                init_training_model=lambda _: (model, object(), 9_998),
                mc_train_button=SimpleNamespace(value=False),
                mo=SimpleNamespace(stop=lambda condition, message: None,
                                   md=lambda text: text),
                q_train_button=SimpleNamespace(value=True),
                save_training_model=lambda method, passed_model, optimizer, count,
                                           evaluation, save_history: (
                    saves.append((count, save_history))
                    or (root / "latest.pt", root / f"models/episodes_{count:06d}.pt"
                        if save_history else None)
                ),
                train_an_episode_behaviour_cloning=lambda **kwargs: None,
                train_an_episode_monte_carlo=lambda **kwargs: None,
                train_an_episode_q_learning=lambda **kwargs:
                    seeds.append(kwargs["seed"]),
            )
            self.assertEqual(saves, [(10_000, True), (10_002, False)])
            self.assertEqual(archives, [(model, 10_000, "q_learning")])
            self.assertEqual(seeds, [19_998, 19_999, 20_000, 20_001])
            self.assertEqual(result[0], 10_002)

            gif = root / "replays/episodes_010000.gif"
            gif.parent.mkdir()
            gif.write_bytes(b"existing replay")
            with self.assertRaisesRegex(FileExistsError, "episodes_010000.gif"):
                training_loop({})(
                    N_EPISODES=1,
                    archive_evaluation_replay=lambda *_:
                        self.fail("Must refuse overwrite before training"),
                    bc_train_button=SimpleNamespace(value=False),
                    checkpoint_dir=root, env=FakeEnv(),
                    evaluate_benchmark=lambda _: sample_evaluation(),
                    gamma=0.99,
                    init_training_model=lambda _: (model, object(), 9_999),
                    mc_train_button=SimpleNamespace(value=False),
                    mo=SimpleNamespace(stop=lambda condition, message: None,
                                       md=lambda text: text),
                    q_train_button=SimpleNamespace(value=True),
                    save_training_model=lambda *_args, **_kwargs:
                        self.fail("Must refuse overwrite before saving"),
                    train_an_episode_behaviour_cloning=lambda **kwargs: None,
                    train_an_episode_monte_carlo=lambda **kwargs: None,
                    train_an_episode_q_learning=lambda **kwargs:
                        self.fail("Must refuse overwrite before training"),
                )
            self.assertEqual(gif.read_bytes(), b"existing replay")

    def test_cloning_saves_short_final_batch_without_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            saves = []
            seeds = []
            loop = training_loop({})
            result = loop(
                N_EPISODES=89,
                archive_evaluation_replay=lambda *_: self.fail("No 10k milestone"),
                bc_train_button=SimpleNamespace(value=True),
                checkpoint_dir=Path(directory),
                env=FakeEnv(),
                evaluate_benchmark=lambda model: sample_evaluation(),
                gamma=0.99,
                init_training_model=lambda method: (
                    SimpleNamespace(train=lambda: None), object(), 12_010
                ),
                mc_train_button=SimpleNamespace(value=False),
                mo=SimpleNamespace(stop=lambda condition, message: None,
                                   md=lambda text: text),
                q_train_button=SimpleNamespace(value=False),
                save_training_model=lambda method, model, optimizer, count,
                                           evaluation, save_history: (
                    saves.append((method, count, save_history))
                    or (Path("latest.pt"), None)
                ),
                train_an_episode_behaviour_cloning=lambda **kwargs:
                    seeds.append(kwargs["seed"]),
                train_an_episode_monte_carlo=lambda **kwargs:
                    self.fail("Wrong training method"),
                train_an_episode_q_learning=lambda **kwargs:
                    self.fail("Wrong training method"),
            )
            self.assertEqual(seeds, [22_010])
            self.assertEqual(saves, [("behaviour_cloning", 12_011, False)])
            self.assertEqual(result[0], 12_011)

    def test_existing_archive_stops_before_training(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "models/episodes_013000.pt"
            archive.parent.mkdir()
            archive.write_bytes(b"existing history")
            loop = training_loop({})
            with self.assertRaisesRegex(FileExistsError, "episodes_013000"):
                loop(
                    N_EPISODES=10,
                    archive_evaluation_replay=lambda *_: self.fail("Already archived"),
                    bc_train_button=SimpleNamespace(value=False),
                    checkpoint_dir=root,
                    env=FakeEnv(),
                    evaluate_benchmark=lambda model: self.fail("Already archived"),
                    gamma=0.99,
                    init_training_model=lambda method: (
                        SimpleNamespace(train=lambda: None), object(), 12_995
                    ),
                    mc_train_button=SimpleNamespace(value=False),
                    mo=SimpleNamespace(stop=lambda condition, message: None,
                                       md=lambda text: text),
                    q_train_button=SimpleNamespace(value=True),
                    save_training_model=lambda *args, **kwargs:
                        self.fail("Already archived"),
                    train_an_episode_behaviour_cloning=lambda **kwargs: None,
                    train_an_episode_monte_carlo=lambda **kwargs: None,
                    train_an_episode_q_learning=lambda **kwargs:
                        self.fail("Must not train before archive check"),
                )
            self.assertEqual(archive.read_bytes(), b"existing history")

    def test_training_charts_read_full_history_without_training(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "evaluations.jsonl"
            records = [
                {
                    **sample_evaluation(),
                    "episodes_trained": episode,
                    "training_method": "monte_carlo",
                    "lines_per_piece": 0.25,
                    "mean_score": float(episode),
                }
                for episode in (100, 1200, 5100, 51100, 51200)
            ]
            records.append({**records[-2], "mean_score": 42.0})
            original = "".join(json.dumps(record) + "\n" for record in records)
            log.write_text(original)

            history = notebook_helper("load_evaluation_history", {"json": json})
            selected = history(log)
            self.assertEqual([r["episodes_trained"] for r in selected],
                             [100, 1200, 5100, 51100, 51200])
            self.assertEqual(selected[3]["mean_score"], 42.0)

            cell = next(
                node for node in NOTEBOOK.body
                if isinstance(node, ast.FunctionDef)
                and {arg.arg for arg in node.args.args} >=
                    {"checkpoint_dir", "refresh_charts"}
            )
            cell = copy.deepcopy(cell)
            cell.decorator_list = []
            namespace = {}
            exec(compile(ast.Module(body=[cell], type_ignores=[]),
                         "train.py", "exec"), namespace)

            class FakeMo:
                def __init__(self):
                    self.markdown = []
                    self.html = None

                def md(self, text):
                    self.markdown.append(text)
                    return text

                def Html(self, text):
                    self.html = text
                    return text

                def vstack(self, items):
                    return items

            mo = FakeMo()
            namespace["_"](checkpoint_dir=root, mo=mo,
                           refresh_charts=SimpleNamespace(value=False))
            self.assertEqual(mo.html.count("<svg "), 6)
            paths = re.findall(r'<path d="([^"]+)"', mo.html)
            self.assertEqual(len(paths), 12)
            self.assertTrue(all(path.count(" L") == 4 for path in paths))
            self.assertIn(">60,000</text>", mo.html)
            self.assertIn("5 evaluations", mo.markdown[0])
            self.assertIn("**100** to **51,200** total episodes", mo.markdown[0])
            self.assertIn("all saved history", mo.markdown[0])
            self.assertIn("three fixed seeds", mo.markdown[0])
            self.assertEqual(log.read_text(), original)
            self.assertEqual(list(root.iterdir()), [log])

            empty_mo = FakeMo()
            namespace["_"](checkpoint_dir=root / "missing",
                           mo=empty_mo,
                           refresh_charts=SimpleNamespace(value=False))
            self.assertIsNone(empty_mo.html)
            self.assertIn("No saved evaluations", empty_mo.markdown[0])


if __name__ == "__main__":
    unittest.main()
