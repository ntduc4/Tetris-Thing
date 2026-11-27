"""Guard the Qt inference protocol against drifting from the notebook encoder."""

import ast
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import qt_opponent


ROOT = Path(__file__).resolve().parents[1]
CHECKPOINT = ROOT / "checkpoints/encoding_v3/shared/latest.pt"


def notebook_encoder():
    notebook = ast.parse((ROOT / "train.py").read_text())
    cell = next(
        node for node in notebook.body
        if isinstance(node, ast.FunctionDef) and
        any(isinstance(inner, ast.FunctionDef) and inner.name == "encode"
            for inner in node.body)
    )
    source = next(
        inner for inner in cell.body
        if isinstance(inner, ast.FunctionDef) and inner.name == "encode"
    )
    namespace = {
        "np": np,
        "PREVIEW_COUNT": 5,
        "tetris_py": SimpleNamespace(
            Placement=object,
            Cell=SimpleNamespace(Empty=0, Garbage=8),
        ),
    }
    exec(compile(ast.Module(body=[source], type_ignores=[]), "train.py", "exec"),
         namespace)
    return namespace["encode"]


class ModelOpponentTests(unittest.TestCase):
    def test_encoding_matches_notebook(self):
        encode_reference = notebook_encoder()
        rng = np.random.default_rng(18)
        for height, use_hold in ((10, False), (10, True),
                                 (40, False), (40, True)):
            for _ in range(10):
                board = rng.integers(0, 10, size=(height, 10)).tolist()
                cells = [[int(rng.integers(0, 10)), int(rng.integers(0, 10))]
                         for _ in range(4)]
                request = {
                    "board": board,
                    "active_piece": 2,
                    "rotation": 3,
                    "position": [6, 3],
                    "hold_piece": 4 if use_hold else None,
                    "can_hold": not use_hold,
                    "preview": [6, 5, 4, 3, 2],
                    "pending_garbage_lines": 8,
                    "back_to_back": True, "back_to_back_count": 3,
                    "combo": 2,
                }
                candidate = {
                    "type": 1,
                    "final_rotation": 2,
                    "final_pos": [-1, 4],
                    "use_hold": use_hold,
                    "drop_distance": 7,
                    "cells": cells,
                    "spin": 0,
                }
                placement = SimpleNamespace(
                    type=candidate["type"],
                    final_rotation=candidate["final_rotation"],
                    final_pos=SimpleNamespace(row=-1, col=4),
                    use_hold=use_hold,
                    drop_distance=7,
                    spin=0,
                    cells=lambda: [SimpleNamespace(row=row, col=col)
                                   for row, col in cells],
                )
                np.testing.assert_array_equal(
                    qt_opponent.encode_v5(request, candidate),
                    encode_reference(request, placement),
                )

    def test_v5_search_protocol_ranks_simulated_followups(self):
        torch = qt_opponent.torch
        model = qt_opponent.PlacementCNN(1285)
        for parameter in model.parameters():
            parameter.data.zero_()
        with tempfile.TemporaryDirectory() as directory:
            checkpoint = Path(directory) / "latest.pt"
            torch.save({"feature_dim": 1285, "encoding_version": 5,
                        "model": model.state_dict()}, checkpoint)
            request = {
                "phase": "roots", "board": [[0] * 10 for _ in range(40)],
                "active_piece": 0, "rotation": 0, "position": [20, 3],
                "hold_piece": None, "can_hold": True,
                "preview": [1, 2, 3, 4, 5], "pending_garbage_lines": 0,
                "back_to_back": False, "back_to_back_count": 0, "combo": 0,
                "actions": [
                    {"type": 0, "final_rotation": 0, "final_pos": [0, col],
                     "drop_distance": 20, "use_hold": False, "spin": 0,
                     "cells": [[0, col + offset] for offset in range(4)]}
                    for col in (0, 3)
                ],
            }
            with subprocess.Popen(
                [sys.executable, str(ROOT / "qt_opponent.py"), str(checkpoint)],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
            ) as process:
                self.assertEqual(json.loads(process.stdout.readline()),
                                 {"ready": True, "width": 10, "height": 40})
                process.stdin.write(json.dumps(request) + "\n")
                process.stdin.flush()
                roots = json.loads(process.stdout.readline())
                self.assertEqual(set(roots["shortlist"]), {0, 1})
                process.stdin.write(json.dumps({
                    "phase": "leaves", "scores": roots["scores"],
                    "branches": [{"root": 0, "reward": -1},
                                 {"root": 1, "reward": 3,
                                  "state": {**request, "preview": [2, 3, 4, 5]}}],
                }) + "\n")
                process.stdin.flush()
                self.assertEqual(json.loads(process.stdout.readline()),
                                 {"index": 1})
            self.assertEqual(process.returncode, 0)

    def test_board_height_is_encoded_in_checkpoint_size(self):
        self.assertEqual(qt_opponent.board_height_from_features(379), 10)
        self.assertEqual(qt_opponent.board_height_from_features(1279), 40)
        with self.assertRaisesRegex(ValueError, "feature size"):
            qt_opponent.board_height_from_features(380)

    def test_forty_row_checkpoint_responds_over_protocol(self):
        torch = qt_opponent.torch
        sizes = (1279, 1024, 512, 128, 1)
        state = {}
        for layer, (size_in, size_out) in enumerate(zip(sizes, sizes[1:])):
            state[f"layers.{2 * layer}.weight"] = torch.zeros(size_out, size_in)
            state[f"layers.{2 * layer}.bias"] = torch.zeros(size_out)
        with tempfile.TemporaryDirectory() as directory:
            checkpoint = Path(directory) / "latest.pt"
            torch.save({"feature_dim": 1279, "encoding_version": 3,
                        "model": state}, checkpoint)
            request = {
                "board": [[0] * 10 for _ in range(40)],
                "active_piece": 0, "rotation": 0, "position": [36, 3],
                "hold_piece": None, "can_hold": True,
                "preview": [1, 2, 3, 4, 5], "pending_garbage_lines": 0,
                "actions": [
                    {"type": 0, "final_rotation": 0, "final_pos": [0, 0],
                     "drop_distance": 36, "use_hold": False,
                     "cells": [[0, col] for col in range(4)]}
                ],
            }
            with subprocess.Popen(
                [sys.executable, str(ROOT / "qt_opponent.py"), str(checkpoint)],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
            ) as process:
                self.assertEqual(json.loads(process.stdout.readline()),
                                 {"ready": True, "width": 10, "height": 40})
                process.stdin.write(json.dumps(request) + "\n")
                process.stdin.flush()
                self.assertEqual(json.loads(process.stdout.readline()), {"index": 0})
            self.assertEqual(process.returncode, 0)

    @unittest.skipUnless(CHECKPOINT.is_file(), "no saved model checkpoint")
    def test_saved_checkpoint_responds_over_protocol(self):
        saved = qt_opponent.torch.load(
            CHECKPOINT, map_location="cpu", weights_only=True
        )
        height = qt_opponent.board_height_from_features(saved["feature_dim"])
        request = {
            "board": [[0] * 10 for _ in range(height)],
            "active_piece": 0,
            "rotation": 0,
            "position": [height - 4, 3],
            "hold_piece": None,
            "can_hold": True,
            "preview": [1, 2, 3, 4, 5],
            "pending_garbage_lines": 0,
            "actions": [
                {"type": 0, "final_rotation": 0, "final_pos": [0, col],
                 "drop_distance": height - 4, "use_hold": False,
                 "cells": [[0, col + offset] for offset in range(4)]}
                for col in (0, 3)
            ],
        }
        with subprocess.Popen(
            [sys.executable, str(ROOT / "qt_opponent.py"), str(CHECKPOINT)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
        ) as process:
            self.assertEqual(json.loads(process.stdout.readline()),
                             {"ready": True, "width": 10, "height": height})
            process.stdin.write(json.dumps(request) + "\n")
            process.stdin.flush()
            result = json.loads(process.stdout.readline())
            self.assertIn(result["index"], (0, 1))
        self.assertEqual(process.returncode, 0)


if __name__ == "__main__":
    unittest.main()
