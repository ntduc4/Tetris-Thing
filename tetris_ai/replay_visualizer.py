"""Read-only, colored playback of the notebook's saved evaluation replay."""

from functools import lru_cache
from html import escape
import json
import os
from pathlib import Path
import tempfile


VISIBLE_ROWS = 20

# Cell values in tetris_py: empty, I/O/T/S/Z/J/L, garbage, unclearable.
COLORS = (
    "#111827", "#40c9ef", "#f8cc45", "#ae71ed", "#56d68a",
    "#f36a70", "#568ae9", "#f5a44d", "#78849b", "#495267",
)
PIECE_COLORS = dict(zip("IOTSZJL", COLORS[1:8]))
PIECE_SHAPES = {
    "I": ("IIII",),
    "O": ("OO", "OO"),
    "T": (".T.", "TTT"),
    "S": (".SS", "SS."),
    "Z": ("ZZ.", ".ZZ"),
    "J": ("J..", "JJJ"),
    "L": ("..L", "LLL"),
}

GIF_BACKGROUND = "#101010"
GIF_PANEL = "#18243a"
GIF_GRID = "#151f32"
GIF_GRID_LINE = "#29364b"
GIF_BORDER = "#4a5a75"
GIF_TEXT = "#eef4ff"
GIF_MUTED = "#aabbd7"
GIF_TILE_EDGE = "#101729"


def _blend_color(background, foreground, opacity):
    background = bytes.fromhex(background[1:])
    foreground = bytes.fromhex(foreground[1:])
    return "#" + "".join(
        f"{round(a * (1 - opacity) + b * opacity):02x}"
        for a, b in zip(background, foreground)
    )


# Every frame uses the same indexed palette, so a GIF can be written one frame
# at a time instead of holding an entire (up to 500-placement) game in memory.
GIF_PALETTE_COLORS = tuple(dict.fromkeys((
    GIF_BACKGROUND, GIF_PANEL, GIF_GRID, GIF_GRID_LINE, GIF_BORDER,
    GIF_TEXT, GIF_MUTED, GIF_TILE_EDGE, *COLORS[1:],
    *(_blend_color(GIF_GRID, color, 0.25) for color in COLORS[1:8]),
    *(_blend_color(color, "#ffffff", 0.25) for color in COLORS[1:10]),
)))
GIF_COLOR_INDEX = {color: i for i, color in enumerate(GIF_PALETTE_COLORS)}
GIF_PALETTE = (
    b"".join(bytes.fromhex(color[1:]) for color in GIF_PALETTE_COLORS)
    .ljust(768, b"\0")
)


def reconstruct_replay(replay, make_env):
    """Recover public hold/queue/stats without changing evaluation or its JSON.

    The saved frames have boards and placements but not queue/hold snapshots.
    Replaying the same seed against a *fresh default* environment recovers them.
    Reject mismatches instead of displaying invented queue or attack statistics.
    """
    env = make_env()
    observation = env.reset(int(replay["seed"]))
    states = []
    total_attack = 0

    def active_cells(public_observation):
        if public_observation["active_piece"] is None:
            return ()
        engine = getattr(env, "_engine", None)
        active = engine.active_piece if engine is not None else None
        if active is None:
            return ()
        return tuple(
            (active.pos.row + offset.row, active.pos.col + offset.col)
            for offset in active.piece.current_shape()
        )

    def board_cells(board):
        # evaluate_episode stores tuple rows in memory; JSON loads list rows.
        return tuple(tuple(row) for row in board)

    for index, frame in enumerate(replay["frames"]):
        if board_cells(observation["board"]) != board_cells(frame["before"]):
            raise ValueError(f"Replay board diverged before placement {index + 1}")
        before_active = active_cells(observation)

        expected = (
            frame["piece"], bool(frame["use_hold"]), frame["rotation"],
            tuple(frame["moves"]),
            frozenset(tuple(cell) for cell in frame["landing"]),
        )
        matches = [
            action for action in env.legal_placements()
            if (
                action.type.name, bool(action.use_hold), action.final_rotation.name,
                tuple(move.name for move in action.moves),
                frozenset((cell.row, cell.col) for cell in action.cells()),
            ) == expected
        ]
        if len(matches) != 1:
            raise ValueError(
                f"Replay placement {index + 1} matched {len(matches)} legal actions"
            )

        next_observation, _, terminated, truncated, info = env.step(matches[0])
        if (board_cells(next_observation["board"]) != board_cells(frame["after"])
                or next_observation["score"] != frame["score"]
                or info["lines_cleared"] != frame["lines"]):
            raise ValueError(f"Replay board or stats diverged after placement {index + 1}")

        attack_after = total_attack + info["attack_sent"]
        states.append({
            "before": observation,
            "after": next_observation,
            "active_cells_before": before_active,
            "active_cells_after": active_cells(next_observation),
            "attack_before": total_attack,
            "attack_after": attack_after,
            "attack_this_move": info["attack_sent"],
            "spin": info["spin"],
            "top_out": terminated,
        })
        observation = next_observation
        total_attack = attack_after
        if (terminated or truncated) and index + 1 != len(replay["frames"]):
            raise ValueError(f"Replay continued after placement {index + 1} ended the game")
    return states


def _tile(x, y, color, size, *, ghost=False):
    if ghost:
        return (
            f'<rect x="{x + 2}" y="{y + 2}" width="{size - 4}" '
            f'height="{size - 4}" rx="2" fill="{color}" fill-opacity=".25" '
            f'stroke="{color}" stroke-width="2"/>'
        )
    return (
        f'<rect x="{x + 1}" y="{y + 1}" width="{size - 2}" '
        f'height="{size - 2}" rx="2" fill="{color}" stroke="#101729"/>'
        f'<path d="M{x + 4} {y + size - 5} V{y + 4} H{x + size - 5}" '
        'fill="none" stroke="#ffffff" stroke-opacity=".25" stroke-width="2"/>'
    )


def board_svg(board, landing=(), piece=None, *, active_cells=(), active_piece=None):
    """Draw a 20-row grid; allow minos to overflow it into the surrounding UI."""
    width = len(board[0])
    landing = {tuple(cell) for cell in landing}
    size = 24
    parts = [
        f'<svg viewBox="0 0 {width * size} {VISIBLE_ROWS * size}" role="img" '
        f'aria-label="Tetris board, {width} columns and {VISIBLE_ROWS} rows" '
        f'style="display:block;width:{width * size}px;max-width:100%;'
        'height:auto;margin:auto;overflow:visible">'
    ]
    # Only the board has a grid. Minos above row 19 render at negative SVG y
    # and can overlap the surrounding UI; the outer replay frame clips them.
    for row in range(VISIBLE_ROWS - 1, -1, -1):
        for col in range(width):
            x, y = col * size, (VISIBLE_ROWS - 1 - row) * size
            parts.append(
                f'<rect x="{x}" y="{y}" width="{size}" height="{size}" '
                'fill="#151f32" stroke="#29364b" stroke-width=".6"/>'
            )
    for row, cells in enumerate(board):
        for col, cell in enumerate(cells):
            if cell:
                parts.append(_tile(
                    col * size, (VISIBLE_ROWS - 1 - row) * size,
                    COLORS[cell], size,
                ))
    for row, col in landing:
        parts.append(_tile(
            col * size, (VISIBLE_ROWS - 1 - row) * size,
            PIECE_COLORS[piece], size, ghost=True,
        ))
    for row, col in active_cells:
        parts.append(_tile(
            col * size, (VISIBLE_ROWS - 1 - row) * size,
            PIECE_COLORS["IOTSZJL"[active_piece]], size,
        ))
    parts.append(
        '<rect x="1" y="1" '
        f'width="{width * size - 2}" height="{VISIBLE_ROWS * size - 2}" '
        'fill="none" stroke="#4a5a75" stroke-width="2"/>'
    )
    parts.append("</svg>")
    return "".join(parts)


def _piece_svg(piece):
    if piece is None:
        return '<div style="height:52px;display:grid;place-items:center;color:#73839d">—</div>'
    if isinstance(piece, int):
        piece = "IOTSZJL"[piece]
    shape = PIECE_SHAPES[piece]
    size = 22
    x0 = (104 - len(shape[0]) * size) // 2
    y0 = (52 - len(shape) * size) // 2
    cells = "".join(
        _tile(x0 + col * size, y0 + row * size, PIECE_COLORS[piece], size)
        for row, line in enumerate(shape)
        for col, cell in enumerate(line)
        if cell != "."
    )
    return (
        f'<svg viewBox="0 0 104 52" role="img" aria-label="{piece} piece" '
        'style="width:104px;height:52px;display:block">'
        f'{cells}</svg>'
    )


def _panel(label, piece, note=""):
    return (
        '<div style="background:#18243a;border:1px solid #344360;'
        'border-radius:8px;padding:8px;text-align:center;min-width:104px">'
        f'<div style="font-size:11px;letter-spacing:.13em;color:#aabbd7">'
        f'{escape(label)}</div>{_piece_svg(piece)}'
        f'<div style="font-size:11px;color:#aabbd7">{escape(note)}</div></div>'
    )


def render_replay(replay, states, index, *, after=False):
    """Render a saved placement as a self-contained TETR.IO-style HTML panel."""
    frame, state = replay["frames"][index], states[index]
    phase = "after" if after else "before"
    observation = state[phase]
    board = frame[phase]
    landing = () if after else frame["landing"]
    svg = board_svg(
        board, landing, frame["piece"] if landing else None,
        active_cells=state[f"active_cells_{phase}"],
        active_piece=observation["active_piece"],
    )
    attack = state[f"attack_{phase}"]
    stats = (
        ("SCORE", observation["score"]),
        ("LINES", observation["lines_cleared"]),
        ("ATTACK", attack),
        ("PIECES", observation["pieces_placed"]),
        ("COMBO", max(0, observation["combo"])),
        ("B2B", observation["back_to_back_count"]),
        ("GARBAGE", observation["pending_garbage_lines"]),
    )
    cards = "".join(
        '<div style="background:#18243a;border:1px solid #344360;'
        'border-radius:7px;padding:7px 9px">'
        f'<div style="color:#aabbd7;font-size:10px;letter-spacing:.12em">'
        f'{label}</div><strong style="font-size:19px">{value:,}</strong></div>'
        for label, value in stats
    )
    queue = "".join(
        _panel(f"NEXT {i + 1}", piece)
        for i, piece in enumerate(observation["preview"])
    ) or _panel("NEXT", None)
    hold = _panel(
        "HOLD", observation["hold_piece"],
        "ready" if observation["can_hold"] else "unavailable",
    )
    active = _panel("CURRENT", observation["active_piece"])
    chosen = escape(frame["piece"])
    move = (
        f'{chosen} · {"hold" if frame["use_hold"] else "no hold"} · '
        f'{escape(frame["rotation"])} · {frame["lines"]} line(s) · '
        f'+{state["attack_this_move"]} attack · reward {frame["reward"]:.2f}'
    )
    spin = {1: "mini spin", 2: "full spin"}.get(state["spin"])
    return (
        '<div style="background:#101a2c;color:#eef4ff;border:1px solid #354766;'
        'border-radius:12px;padding:16px;max-width:790px;overflow:hidden;'
        'font-family:system-ui,sans-serif">'
        '<div style="display:flex;justify-content:space-between;gap:10px;'
        'flex-wrap:wrap;margin-bottom:12px">'
        f'<strong style="letter-spacing:.1em">EVALUATION REPLAY</strong>'
        f'<span style="color:#aabbd7">Seed {int(replay["seed"])} · '
        f'piece {index + 1}/{len(replay["frames"])} · '
        f'{"AFTER LOCK" if after else "BEFORE / LANDING"}</span></div>'
        '<div style="display:flex;gap:12px;justify-content:center;'
        'align-items:flex-start;flex-wrap:wrap">'
        f'<div style="display:grid;gap:10px">{hold}{active}</div>'
        f'<div style="width:min(240px,100%);flex:none;position:relative;'
        f'z-index:2;overflow:visible">{svg}'
        f'<div style="text-align:center;font-size:11px;color:#aabbd7;'
        'margin-top:6px">20×10 grid · minos may overflow the board</div></div>'
        f'<div style="display:grid;gap:6px">{queue}</div></div>'
        '<div style="display:grid;grid-template-columns:repeat(auto-fit,'
        'minmax(90px,1fr));gap:7px;margin-top:14px">'
        f'{cards}</div>'
        '<div style="color:#cbd9ed;font-size:12px;margin-top:12px">'
        f'Chosen placement: {move}'
        f'{" · " + spin if spin else ""}'
        f'{" · TOP OUT" if after and state["top_out"] else ""}'
        '</div></div>'
    )


@lru_cache(maxsize=1)
def _gif_fonts():
    from PIL import ImageFont

    return ImageFont.load_default(size=17), ImageFont.load_default(size=13)


def _gif_tile(draw, x, y, size, color, *, ghost=False):
    if ghost:
        draw.rectangle(
            (x + 2, y + 2, x + size - 3, y + size - 3),
            fill=GIF_COLOR_INDEX[_blend_color(GIF_GRID, color, 0.25)],
            outline=GIF_COLOR_INDEX[color], width=2,
        )
    else:
        draw.rectangle(
            (x + 1, y + 1, x + size - 2, y + size - 2),
            fill=GIF_COLOR_INDEX[color], outline=GIF_COLOR_INDEX[GIF_TILE_EDGE],
        )
        draw.line(
            (x + 3, y + 3, x + size - 4, y + 3),
            fill=GIF_COLOR_INDEX[_blend_color(color, "#ffffff", 0.25)], width=2,
        )


def _gif_piece_panel(draw, x, y, label, piece, *, width=150, height=94):
    font, small_font = _gif_fonts()
    draw.rounded_rectangle(
        (x, y, x + width, y + height), radius=7,
        fill=GIF_COLOR_INDEX[GIF_PANEL], outline=GIF_COLOR_INDEX[GIF_BORDER],
    )
    draw.text((x + 9, y + 6), label, font=small_font,
              fill=GIF_COLOR_INDEX[GIF_MUTED])
    if piece is None:
        draw.text((x + width // 2 - 4, y + 44), "—", font=font,
                  fill=GIF_COLOR_INDEX[GIF_MUTED])
        return
    if isinstance(piece, int):
        piece = "IOTSZJL"[piece]
    shape = PIECE_SHAPES[piece]
    size = 20
    left = x + (width - len(shape[0]) * size) // 2
    top = y + 32 + (height - 32 - len(shape) * size) // 2
    for row, line in enumerate(shape):
        for col, cell in enumerate(line):
            if cell != ".":
                _gif_tile(draw, left + col * size, top + row * size, size,
                          PIECE_COLORS[piece])


def render_replay_image(replay, states, index, *, after=False):
    """Draw a fixed 20-row grid; minos overlap UI until the image edge."""
    from PIL import Image, ImageDraw

    frame, state = replay["frames"][index], states[index]
    phase = "after" if after else "before"
    observation = state[phase]
    board = frame[phase]
    landing = set() if after else {tuple(cell) for cell in frame["landing"]}
    width = len(board[0])
    cell_size = 24
    board_x = (650 - width * cell_size) // 2
    board_y = 130
    font, small_font = _gif_fonts()
    image = Image.new("P", (650, 800), GIF_COLOR_INDEX[GIF_BACKGROUND])
    image.putpalette(GIF_PALETTE)
    draw = ImageDraw.Draw(image)
    draw.text((20, 15), "EVALUATION REPLAY", font=font,
              fill=GIF_COLOR_INDEX[GIF_TEXT])
    draw.text(
        (20, 40),
        f"EPISODE {replay['episodes_trained']:,}   SEED {replay['seed']}   "
        f"PIECE {index + 1}/{len(replay['frames'])}   "
        f"{'AFTER LOCK' if after else 'LANDING'}",
        font=small_font, fill=GIF_COLOR_INDEX[GIF_MUTED],
    )

    # Only board cells get a grid. The background above is the surrounding
    # replay UI, not four additional dark board rows.
    for row in range(VISIBLE_ROWS - 1, -1, -1):
        for col in range(width):
            x = board_x + col * cell_size
            y = board_y + (VISIBLE_ROWS - 1 - row) * cell_size
            draw.rectangle(
                (x, y, x + cell_size - 1, y + cell_size - 1),
                fill=GIF_COLOR_INDEX[GIF_GRID],
                outline=GIF_COLOR_INDEX[GIF_GRID_LINE],
            )
    draw.rectangle(
        (board_x + 1, board_y + 1,
         board_x + width * cell_size - 2,
         board_y + VISIBLE_ROWS * cell_size - 2),
        outline=GIF_COLOR_INDEX[GIF_BORDER], width=2,
    )

    _gif_piece_panel(draw, 24, 70, "HOLD", observation["hold_piece"])
    _gif_piece_panel(draw, 24, 180, "CURRENT", observation["active_piece"])
    previews = observation["preview"]
    for i in range(5):
        _gif_piece_panel(
            draw, 475, 70 + i * 94, f"NEXT {i + 1}",
            previews[i] if i < len(previews) else None, width=150, height=88,
        )
    draw.text((24, 315), f"CHOSEN: {frame['piece']}", font=font,
              fill=GIF_COLOR_INDEX[GIF_TEXT])
    draw.text((24, 342), "HOLD USED" if frame["use_hold"] else "NO HOLD",
              font=small_font, fill=GIF_COLOR_INDEX[GIF_MUTED])
    draw.text((24, 368), f"LINES: {frame['lines']}  ATK: +{state['attack_this_move']}",
              font=small_font, fill=GIF_COLOR_INDEX[GIF_MUTED])

    stats = (
        ("SCORE", observation["score"]),
        ("LINES", observation["lines_cleared"]),
        ("ATTACK", state[f"attack_{phase}"]),
        ("PIECES", observation["pieces_placed"]),
        ("COMBO", max(0, observation["combo"])),
        ("B2B", observation["back_to_back_count"]),
        ("GARBAGE", observation["pending_garbage_lines"]),
    )
    for i, (label, value) in enumerate(stats):
        x = 20 + (i % 4) * 158
        y = 650 + (i // 4) * 46
        draw.rounded_rectangle(
            (x, y, x + 150, y + 40), radius=5,
            fill=GIF_COLOR_INDEX[GIF_PANEL], outline=GIF_COLOR_INDEX[GIF_BORDER],
        )
        draw.text((x + 7, y + 3), label, font=small_font,
                  fill=GIF_COLOR_INDEX[GIF_MUTED])
        draw.text((x + 7, y + 18), f"{value:,}", font=font,
                  fill=GIF_COLOR_INDEX[GIF_TEXT])
    draw.text(
        (20, 764),
        "20x10 grid - minos may overflow - clipped at frame edge"
        + (" - TOP OUT" if after and state["top_out"] else ""),
        font=small_font, fill=GIF_COLOR_INDEX[GIF_MUTED],
    )
    # Draw minos last, on the entire frame. They can overlap the header or
    # nearby panels; Pillow clips only where the outer image ends.
    for row, cells in enumerate(board):
        for col, cell in enumerate(cells):
            if cell:
                _gif_tile(
                    draw, board_x + col * cell_size,
                    board_y + (VISIBLE_ROWS - 1 - row) * cell_size,
                    cell_size, COLORS[cell],
                )
    for row, col in landing:
        _gif_tile(
            draw, board_x + col * cell_size,
            board_y + (VISIBLE_ROWS - 1 - row) * cell_size,
            cell_size, PIECE_COLORS[frame["piece"]], ghost=True,
        )
    for row, col in state[f"active_cells_{phase}"]:
        _gif_tile(
            draw, board_x + col * cell_size,
            board_y + (VISIBLE_ROWS - 1 - row) * cell_size,
            cell_size, PIECE_COLORS["IOTSZJL"[observation["active_piece"]]],
        )
    return image


def save_replay_archive(replay, make_env, replay_dir):
    """Atomically save immutable JSON and a streaming, two-frame-per-move GIF."""
    from PIL import GifImagePlugin

    episode = int(replay["episodes_trained"])
    # if episode < 1 or episode % 10_000:
    #     raise ValueError("Replay archives are written at 10,000-episode milestones")
    if not replay["frames"]:
        raise ValueError("Cannot archive an empty replay")
    replay_dir = Path(replay_dir)
    stem = f"episodes_{episode:06d}"
    gif_path = replay_dir / f"{stem}.gif"
    json_path = replay_dir / f"{stem}.json"
    if gif_path.exists() or json_path.exists():
        raise FileExistsError(f"Refusing to overwrite {stem} replay archive")

    states = reconstruct_replay(replay, make_env)
    replay_dir.mkdir(parents=True, exist_ok=True)
    gif_temp = json_temp = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=replay_dir, prefix=".replay-", suffix=".gif", delete=False,
        ) as file:
            gif_temp = Path(file.name)
            header_written = False
            for index in range(len(states)):
                for after in (False, True):
                    image = render_replay_image(replay, states, index, after=after)
                    if not header_written:
                        header, _ = GifImagePlugin.getheader(
                            image, info={"loop": 0, "optimize": False}
                        )
                        for part in header:
                            file.write(part)
                        header_written = True
                    for part in GifImagePlugin.getdata(
                        image, duration=220 if after else 160, disposal=2,
                    ):
                        file.write(part)
            file.write(b";")
            file.flush()
            os.fsync(file.fileno())

        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=replay_dir,
            prefix=".replay-", suffix=".json", delete=False,
        ) as file:
            json_temp = Path(file.name)
            file.write(json.dumps(replay))
            file.flush()
            os.fsync(file.fileno())

        os.link(gif_temp, gif_path)
        try:
            os.link(json_temp, json_path)
        except BaseException:
            gif_path.unlink()
            raise
    finally:
        if gif_temp is not None:
            gif_temp.unlink(missing_ok=True)
        if json_temp is not None:
            json_temp.unlink(missing_ok=True)
    return json_path, gif_path
