"""CPU-only inference worker for the Qt turn-based match.

V5 scores roots, then scores Qt-simulated continuations for the best six.
V3 checkpoints keep the original one-request greedy protocol. No engine,
notebook, optimizer, or hidden piece source is loaded in this process.
"""

import json
import sys

import numpy as np
import torch
from torch import nn
from torch.nn import functional as F


ENCODING_VERSION = 3
BOARD_WIDTH = 10
PREVIEW_COUNT = 5
# Three board planes plus the fixed active/hold/preview/action fields in v3.
FIXED_FEATURES = 79
V5_FEATURES = 85


def encode(observation, placement):
    """Match train.py's encoding_v3 feature order for a visible placement."""
    board = np.asarray(observation["board"])
    height, width = board.shape
    occupied = (board != 0).astype(np.float32).ravel()
    garbage = (board == 8).astype(np.float32).ravel()
    landing = np.zeros((height, width), dtype=np.float32)
    for row, col in placement["cells"]:
        landing[row, col] = 1.0

    def piece_one_hot(piece):
        values = np.zeros(8, dtype=np.float32)
        values[7 if piece is None else piece] = 1.0
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

    preview = observation["preview"]
    if len(preview) > PREVIEW_COUNT:
        raise ValueError("More preview pieces than the configured input size")
    previews = [
        piece_one_hot(preview[i] if i < len(preview) else None)
        for i in range(PREVIEW_COUNT)
    ]

    candidate_piece = np.zeros(7, dtype=np.float32)
    candidate_piece[placement["type"]] = 1.0
    candidate_rotation = np.zeros(4, dtype=np.float32)
    candidate_rotation[placement["final_rotation"]] = 1.0
    action = np.array([
        placement["final_pos"][0] / height,
        placement["final_pos"][1] / width,
        float(placement["use_hold"]),
        placement["drop_distance"] / height,
    ], dtype=np.float32)

    return np.concatenate((
        occupied, garbage, landing.ravel(),
        piece_one_hot(observation["active_piece"]), active_rotation, active,
        piece_one_hot(observation["hold_piece"]), *previews,
        candidate_piece, candidate_rotation, action,
        np.array([observation["pending_garbage_lines"] / height], dtype=np.float32),
    ))


def board_height_from_features(feature_dim, version=3):
    cells, remainder = divmod(
        feature_dim - (V5_FEATURES if version == 5 else FIXED_FEATURES),
        3 * BOARD_WIDTH,
    )
    if remainder or not 4 <= cells <= 80:
        raise ValueError("Checkpoint feature size is not a supported 10-column board")
    return cells


def load_weights(path):
    checkpoint = torch.load(path, map_location="cpu", weights_only=True)
    version = checkpoint.get("encoding_version")
    if version not in (3, 5):
        raise ValueError("Checkpoint encoding version is not supported")
    feature_dim = checkpoint["feature_dim"]
    height = board_height_from_features(feature_dim, version)
    state = checkpoint["model"]
    if version == 5:
        model = PlacementCNN(feature_dim)
        model.load_state_dict(state)
        model.eval()
        return model, height
    layers = []
    sizes = (feature_dim, 1024, 512, 128, 1)
    for index, (size_in, size_out) in enumerate(
            zip(sizes, sizes[1:])):
        key = 2 * index
        weight, bias = state[f"layers.{key}.weight"], state[f"layers.{key}.bias"]
        if tuple(weight.shape) != (size_out, size_in) or tuple(bias.shape) != (size_out,):
            raise ValueError("Checkpoint layer shapes do not match the saved model")
        layers.append((weight, bias))
    return layers, height


class PlacementCNN(nn.Module):
    """The v5 notebook architecture, with its checkpoint feature order."""

    def __init__(self, feature_dim):
        super().__init__()
        height = board_height_from_features(feature_dim, 5)
        self.board_features = 3 * height * BOARD_WIDTH
        self.height = height
        self.spatial = nn.Sequential(
            nn.Conv2d(3, 32, kernel_size=3, padding=1), nn.ReLU(),
            nn.Conv2d(32, 32, kernel_size=3, padding=1), nn.ReLU(),
            nn.Flatten(), nn.Linear(32 * height * BOARD_WIDTH, 128), nn.ReLU(),
        )
        self.head = nn.Sequential(
            nn.Linear(128 + V5_FEATURES, 128), nn.ReLU(), nn.Linear(128, 1),
        )

    def forward(self, x):
        board = x[:, :self.board_features].reshape(-1, 3, self.height, BOARD_WIDTH)
        return self.head(torch.cat(
            (self.spatial(board), x[:, self.board_features:]), dim=1
        )).squeeze(-1)


def encode_v5(observation, placement):
    base = encode(observation, placement)
    b2b = float(observation["back_to_back"])
    state = np.array([
        b2b,
        np.log1p(observation["back_to_back_count"]) / np.log1p(20.0),
        np.log1p(observation["combo"]) / np.log1p(20.0),
    ], dtype=np.float32)
    spin = np.zeros(3, dtype=np.float32)
    spin[placement["spin"]] = 1
    return np.concatenate((base, state, spin))


def score_actions(observation, layers, height):
    board = np.asarray(observation["board"])
    if board.shape != (height, BOARD_WIDTH) or not observation["actions"]:
        raise ValueError("Board dimensions do not match the checkpoint")
    encoder = encode_v5 if isinstance(layers, PlacementCNN) else encode
    features = np.stack([encoder(observation, a) for a in observation["actions"]])
    with torch.inference_mode():
        scores = torch.from_numpy(features)
        if isinstance(layers, PlacementCNN):
            return layers(scores).tolist()
        if features.shape[1] != layers[0][0].shape[1]:
            raise ValueError("Model feature size mismatch")
        for index, (weight, bias) in enumerate(layers):
            scores = F.linear(scores, weight, bias)
            if index < len(layers) - 1:
                scores = F.relu(scores)
        return scores.squeeze(-1).tolist()


def choose(observation, layers, height):
    scores = score_actions(observation, layers, height)
    return max(range(len(scores)), key=scores.__getitem__)


def main():
    try:
        torch.set_num_threads(1)
        layers, height = load_weights(sys.argv[1])
        print(json.dumps({"ready": True, "width": BOARD_WIDTH, "height": height}),
              flush=True)
        for line in sys.stdin:
            request = json.loads(line)
            if request.get("phase") == "roots" and isinstance(layers, PlacementCNN):
                scores = score_actions(request, layers, height)
                shortlist = sorted(range(len(scores)), key=scores.__getitem__,
                                   reverse=True)[:6]
                result = {"shortlist": shortlist, "scores": scores}
            elif request.get("phase") == "leaves" and isinstance(layers, PlacementCNN):
                scores = list(request["scores"])
                for branch in request["branches"]:
                    index = branch["root"]
                    value = branch["reward"]
                    if branch.get("state") is not None:
                        value += 0.99 * max(score_actions(branch["state"], layers, height))
                    scores[index] = value
                result = {"index": max(range(len(scores)), key=scores.__getitem__)}
            else:
                result = {"index": choose(request, layers, height)}
            print(json.dumps(result), flush=True)
    except Exception as error:
        print(json.dumps({"error": str(error)}), flush=True)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
