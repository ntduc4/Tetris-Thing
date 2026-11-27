"""Shallow search should re-rank actions without leaking the next bag piece."""

from pathlib import Path
import sys
from types import SimpleNamespace
import unittest

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from placement_search import choose_placement_index


class SearchEnv:
    def __init__(self):
        self.state = 0
        self.last_preview = None

    def checkpoint(self):
        return self.state

    def restore(self, state):
        self.state = state

    def observe(self):
        return {"preview": (1, 2), "hold_piece": None}

    def legal_placements(self):
        values = (2, 1) if self.state == 0 else ((5,) if self.state == 1 else (10,))
        return [SimpleNamespace(value=value, use_hold=False) for value in values]

    def step(self, action):
        self.state = 1 if action.value == 2 else 2
        return {"preview": (2, 99, 99), "hold_piece": None}, 0, False, False, {}


class IdentityModel(torch.nn.Module):
    def forward(self, x):
        return x[:, 0]


class SearchTests(unittest.TestCase):
    def test_search_reorders_root_without_hidden_preview_or_mutating_env(self):
        env = SearchEnv()
        previews = []

        def encode(state, action):
            previews.append(tuple(state["preview"]))
            return np.array([action.value], dtype=np.float32)

        model = IdentityModel()
        self.assertEqual(choose_placement_index(env, model, encode, width=2), 1)
        self.assertEqual(env.state, 0)
        self.assertTrue(model.training)
        self.assertEqual(previews[:2], [(1, 2), (1, 2)])
        self.assertTrue(all(preview == (2,) for preview in previews[2:]))


if __name__ == "__main__":
    unittest.main()
