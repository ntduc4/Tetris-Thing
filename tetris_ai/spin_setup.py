"""Qt-authored bottom-anchored templates for opt-in AI opening positions.

The pattern occupies a small box. Per-row mode controls only cells *outside*
that box: filled rows become garbage, overhang rows remain empty. The engine
generates the starting board; mirrored templates also swap S/Z and J/L painted
cells and scripted active, held and next pieces. The seeded 7-bag stays unchanged.
"""

from dataclasses import dataclass
import json
from pathlib import Path

CELLS = ".IOTSZJLGX"
PIECES = "IOTSZJL"
BOARD_WIDTH = 10
BOARD_HEIGHT = 40
VISIBLE_HEIGHT = 20


@dataclass(frozen=True)
class SpinSetup:
    rows_bottom_up: tuple[str, ...]
    row_modes_bottom_up: tuple[str, ...]
    active_piece: str
    hold_piece: str | None
    next_pieces: str
    legacy_full_board: bool = False

    @property
    def width(self) -> int:
        return len(self.rows_bottom_up[0])

    @property
    def height(self) -> int:
        return len(self.rows_bottom_up)

    def _generate(self, engine, seed, tetris_py, clean_garbage_layers,
                  cheese_garbage_layers):
        cells = tetris_py.Cell.__members__
        names = {".": "Empty", "G": "Garbage", "X": "Unclearable"}
        pattern = [[cells[names.get(char, char)] for char in row]
                   for row in self.rows_bottom_up]
        piece_types = tetris_py.PieceType.__members__
        scripted = [piece_types[p] for p in self.active_piece + self.next_pieces]
        hold = piece_types[self.hold_piece] if self.hold_piece is not None else None
        return engine.generate_opening(
            pattern, [mode == "filled" for mode in self.row_modes_bottom_up],
            clean_garbage_layers, cheese_garbage_layers, seed, scripted, hold,
            not self.legacy_full_board)

    def render(self, seed: int, *, clean_garbage_layers: int = 0,
               cheese_garbage_layers: int = 0) -> tuple[tuple[str, ...], int, bool]:
        """Preview the engine-generated full board, horizontal offset and mirror."""
        import tetris_py

        config = tetris_py.EngineConfig()
        config.width = BOARD_WIDTH
        config.height = BOARD_HEIGHT
        engine = tetris_py.Engine(config)
        engine.reset(seed)
        transform = self._generate(
            engine, seed, tetris_py, clean_garbage_layers, cheese_garbage_layers)
        board = tuple("".join(CELLS[int(cell)] for cell in row)
                      for row in engine.board.render())
        return board, transform.offset, transform.mirrored

    def apply(self, engine, seed: int, tetris_py, *,
              clean_garbage_layers: int = 0, cheese_garbage_layers: int = 0):
        """Reset and generate an opening; return the chosen engine transform."""
        engine.reset(seed)
        transform = self._generate(
            engine, seed, tetris_py, clean_garbage_layers, cheese_garbage_layers)
        if not engine.spawn_next_piece():
            raise ValueError("Spin template blocks the active piece's spawn position")
        return transform


def load_spin_setup(path: str | Path) -> SpinSetup:
    """Load v2 templates, or v1 fixed full-board setups without mirroring."""
    with Path(path).open(encoding="utf-8") as file:
        data = json.load(file)
    if not isinstance(data, dict):
        raise ValueError("Spin setup must be a JSON object")
    if (type(data.get("width")) is not int or data["width"] != BOARD_WIDTH or
            type(data.get("height")) is not int or data["height"] != BOARD_HEIGHT):
        raise ValueError("Spin setup requires a 10×40 board")
    active = data.get("active_piece")
    hold = data.get("hold_piece", object())
    following = data.get("next_pieces")
    if (not isinstance(active, str) or len(active) != 1 or active not in PIECES or
            hold is not None and (not isinstance(hold, str) or
                                  len(hold) != 1 or hold not in PIECES) or
            not isinstance(following, str) or len(following) > 63 or
            any(piece not in PIECES for piece in following)):
        raise ValueError("Invalid active, held or next pieces (use IOTSZJL)")

    version = data.get("version")
    if type(version) is not int or version not in (1, 2):
        raise ValueError("Unsupported spin setup version (expected 1 or 2)")
    if version == 1:
        board = data.get("board_rows_bottom_up")
        if (not isinstance(board, list) or len(board) != BOARD_HEIGHT or
                any(not isinstance(row, str) or len(row) != BOARD_WIDTH or
                    any(cell not in CELLS for cell in row) for row in board) or
                any(row != "." * BOARD_WIDTH for row in board[VISIBLE_HEIGHT:])):
            raise ValueError("Legacy setup needs 40 bottom-up rows; upper 20 must be empty")
        return SpinSetup(tuple(board[:VISIBLE_HEIGHT]),
                         ("overhang",) * VISIBLE_HEIGHT,
                         active, hold, following, legacy_full_board=True)

    template = data.get("template")
    if not isinstance(template, dict):
        raise ValueError("Spin setup needs a template object")
    width, height = template.get("width"), template.get("height")
    rows, modes = template.get("rows_bottom_up"), template.get("row_modes_bottom_up")
    if (type(width) is not int or not 1 <= width <= BOARD_WIDTH or
            type(height) is not int or not 1 <= height <= VISIBLE_HEIGHT or
            not isinstance(rows, list) or len(rows) != height or
            any(not isinstance(row, str) or len(row) != width or
                any(cell not in CELLS for cell in row) for row in rows) or
            not isinstance(modes, list) or len(modes) != height or
            any(mode not in ("filled", "overhang") for mode in modes)):
        raise ValueError("Invalid template: expected 1..10 columns, 1..20 rows "
                         "and one filled/overhang mode for each bottom-up row")
    return SpinSetup(tuple(rows), tuple(modes), active, hold, following)
