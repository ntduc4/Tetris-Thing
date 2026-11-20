"""CPU-only inference worker for the Qt turn-based match.

One JSON line in contains public board state and engine-generated placements;
one JSON line out contains the index of the highest-scoring placement. No
engine, notebook, optimizer, or hidden piece source is loaded in this process.
"""

import json
import sys

import numpy as np
import torch
from torch.nn import functional as F


ENCODING_VERSION = 3
BOARD_WIDTH = 10
PREVIEW_COUNT = 5
# Three board planes plus the fixed active/hold/preview/action fields in v3.
FIXED_FEATURES = 79


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


def board_height_from_features(feature_dim):
    cells, remainder = divmod(feature_dim - FIXED_FEATURES, 3 * BOARD_WIDTH)
    if remainder or not 4 <= cells <= 80:
        raise ValueError("Checkpoint feature size is not a supported 10-column board")
    return cells


def load_weights(path):
    checkpoint = torch.load(path, map_location="cpu", weights_only=True)
    if checkpoint.get("encoding_version") != ENCODING_VERSION:
        raise ValueError("Checkpoint encoding version is not supported")
    feature_dim = checkpoint["feature_dim"]
    height = board_height_from_features(feature_dim)
    state = checkpoint["model"]
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


def choose(observation, layers, height):
    board = np.asarray(observation["board"])
    if board.shape != (height, BOARD_WIDTH) or not observation["actions"]:
        raise ValueError("Board dimensions do not match the checkpoint")
    features = np.stack([encode(observation, action) for action in observation["actions"]])
    if features.shape[1] != layers[0][0].shape[1]:
        raise ValueError("Model feature size mismatch")
    with torch.inference_mode():
        scores = torch.from_numpy(features)
        for index, (weight, bias) in enumerate(layers):
            scores = F.linear(scores, weight, bias)
            if index < len(layers) - 1:
                scores = F.relu(scores)
        return int(scores.squeeze(-1).argmax().item())


def main():
    try:
        torch.set_num_threads(1)
        layers, height = load_weights(sys.argv[1])
        print(json.dumps({"ready": True, "width": BOARD_WIDTH, "height": height}),
              flush=True)
        for line in sys.stdin:
            print(json.dumps({"index": choose(json.loads(line), layers, height)}),
                  flush=True)
    except Exception as error:
        print(json.dumps({"error": str(error)}), flush=True)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
