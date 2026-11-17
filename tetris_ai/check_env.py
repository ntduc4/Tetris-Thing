"""Smoke-test the training runtime; run with `make -C tetris_engine ai-check`."""

from pathlib import Path
import sys


def main() -> None:
    if sys.version_info[:2] != (3, 14):
        raise RuntimeError("Use the Python 3.14 Poetry environment in tetris_ai")

    import marimo
    import numpy as np
    import torch

    try:
        import tetris_py as t
    except ImportError as exc:
        raise RuntimeError(
            "tetris_py is unavailable; run `make -C tetris_engine ai-setup` "
            "from the repository root"
        ) from exc

    build_dir = Path(__file__).resolve().parent.parent / "tetris_engine" / "build"
    if Path(t.__file__).resolve().parent != build_dir:
        raise RuntimeError(f"Expected the local tetris_py build in {build_dir}, got {t.__file__}")

    config = t.EngineConfig()
    config.gravity = 0
    config.attack = t.AttackRule.Off
    game = t.Engine(config)
    if not hasattr(t, "Placement") or not hasattr(game, "generate_active_piece_placement"):
        raise RuntimeError("Placement bindings are missing; run `make -C tetris_engine ai-setup`")

    def trace() -> tuple:
        game.reset(42)
        if not game.spawn_next_piece():
            raise RuntimeError("Seeded engine failed to spawn a piece")
        active = game.active_piece
        first = (active.piece.type, tuple(game.preview_queue()))
        lock = game.hard_drop()
        if not lock.locked or lock.game_over or not game.spawn_next_piece():
            raise RuntimeError("Engine failed a first-piece lock and respawn")
        return first, game.board.render(), game.active_piece.piece.type, tuple(game.preview_queue())

    if (game.board.width, game.board.height) != (10, 40) or config.preview_count != 5:
        raise RuntimeError("Unexpected engine board or preview defaults")
    if trace() != trace():
        raise RuntimeError("Engine reset(seed) did not reproduce the same transition")
    checkpoint = game.clone()
    placements = game.generate_active_piece_placement()
    if not placements:
        raise RuntimeError("Engine did not generate any reachable placements")
    lock = game.apply_placement(placements[0])
    if not lock.locked or lock.score_delta != game.score - checkpoint.score:
        raise RuntimeError("Placement replay did not produce a measured lock")
    game.restore(checkpoint)
    if game.board.render() != checkpoint.board.render() or game.preview_queue() != checkpoint.preview_queue():
        raise RuntimeError("Placement restore did not recover board and RNG/queue")

    if torch.version.hip is None or not torch.cuda.is_available():
        raise RuntimeError("ROCm PyTorch GPU unavailable; check rocminfo and the Poetry ROCm wheel")
    device = torch.device("cuda")  # PyTorch uses the cuda device API for ROCm.
    features = torch.from_numpy(np.ones((2, 4), dtype=np.float32)).to(device)
    layer = torch.nn.Linear(4, 1).to(device)
    layer(features).square().mean().backward()
    torch.cuda.synchronize()
    if layer.weight.grad is None or not torch.isfinite(layer.weight.grad).all().item():
        raise RuntimeError("ROCm forward/backward smoke test failed")

    print(f"Python {sys.version.split()[0]} | NumPy {np.__version__} | marimo {marimo.__version__}")
    print(f"PyTorch {torch.__version__} | ROCm {torch.version.hip} | {torch.cuda.get_device_name(0)}")
    print(f"tetris_py {t.__file__} | seeded engine reset/lock/respawn OK")
    print(f"Runtime ready; {len(placements)} active placements, replay and RNG restore OK.")


if __name__ == "__main__":
    main()
