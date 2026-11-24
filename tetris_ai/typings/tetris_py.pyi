"""
Headless Tetris engine bindings (board rows start at the bottom)
"""
from __future__ import annotations
import collections.abc
import typing
__all__: list[str] = ['ActivePiece', 'AttackRule', 'AttackTable', 'BagRule', 'Board', 'Cell', 'Engine', 'EngineConfig', 'GarbageEvent', 'LockResult', 'Match', 'Movement', 'Offset', 'PendingGarbage', 'Piece', 'PieceType', 'Placement', 'Position', 'Rotation', 'RotationRule', 'ScoreTable', 'SoloMode', 'SoloSession', 'SoloSettings', 'SpinContext', 'SpinRule', 'SpinType', 'TickResult']
class Cell:
    """
    Members:
    
      Empty
    
      I
    
      O
    
      T
    
      S
    
      Z
    
      J
    
      L
    
      Garbage
    
      Unclearable
    """
    Empty: typing.ClassVar[Cell]
    Garbage: typing.ClassVar[Cell]
    I: typing.ClassVar[Cell]
    J: typing.ClassVar[Cell]
    L: typing.ClassVar[Cell]
    O: typing.ClassVar[Cell]
    S: typing.ClassVar[Cell]
    T: typing.ClassVar[Cell]
    Unclearable: typing.ClassVar[Cell]
    Z: typing.ClassVar[Cell]
    __members__: typing.ClassVar[dict[str, Cell]]
    @typing.overload
    def __eq__(self, other: Cell) -> bool:
        ...
    @typing.overload
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __int__(self) -> int:
        ...
    @typing.overload
    def __ne__(self, other: Cell) -> bool:
        ...
    @typing.overload
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class PieceType:
    """
    Members:
    
      I
    
      O
    
      T
    
      S
    
      Z
    
      J
    
      L
    """
    I: typing.ClassVar[PieceType]
    J: typing.ClassVar[PieceType]
    L: typing.ClassVar[PieceType]
    O: typing.ClassVar[PieceType]
    S: typing.ClassVar[PieceType]
    T: typing.ClassVar[PieceType]
    Z: typing.ClassVar[PieceType]
    __members__: typing.ClassVar[dict[str, PieceType]]
    @typing.overload
    def __eq__(self, other: PieceType) -> bool:
        ...
    @typing.overload
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __int__(self) -> int:
        ...
    @typing.overload
    def __ne__(self, other: PieceType) -> bool:
        ...
    @typing.overload
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class Rotation:
    """
    Members:
    
      North
    
      East
    
      South
    
      West
    """
    East: typing.ClassVar[Rotation]
    North: typing.ClassVar[Rotation]
    South: typing.ClassVar[Rotation]
    West: typing.ClassVar[Rotation]
    __members__: typing.ClassVar[dict[str, Rotation]]
    @typing.overload
    def __eq__(self, other: Rotation) -> bool:
        ...
    @typing.overload
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __int__(self) -> int:
        ...
    @typing.overload
    def __ne__(self, other: Rotation) -> bool:
        ...
    @typing.overload
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class Movement:
    """
    Members:

      None_

      Left

      Right

      SoftDrop

      HardDrop

      CW

      CCW

      HalfRotation

      Gravity
    """
    CCW: typing.ClassVar[Movement]
    CW: typing.ClassVar[Movement]
    Gravity: typing.ClassVar[Movement]
    HalfRotation: typing.ClassVar[Movement]
    HardDrop: typing.ClassVar[Movement]
    Left: typing.ClassVar[Movement]
    None_: typing.ClassVar[Movement]
    Right: typing.ClassVar[Movement]
    SoftDrop: typing.ClassVar[Movement]
    __members__: typing.ClassVar[dict[str, Movement]]
    @typing.overload
    def __eq__(self, other: Movement) -> bool:
        ...
    @typing.overload
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __int__(self) -> int:
        ...
    @typing.overload
    def __ne__(self, other: Movement) -> bool:
        ...
    @typing.overload
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class SpinType:
    """
    Members:
    
      None_
    
      Mini
    
      Full
    """
    Full: typing.ClassVar[SpinType]
    Mini: typing.ClassVar[SpinType]
    None_: typing.ClassVar[SpinType]
    __members__: typing.ClassVar[dict[str, SpinType]]
    @typing.overload
    def __eq__(self, other: SpinType) -> bool:
        ...
    @typing.overload
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __int__(self) -> int:
        ...
    @typing.overload
    def __ne__(self, other: SpinType) -> bool:
        ...
    @typing.overload
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class RotationRule:
    """
    Members:
    
      SRSPlus
    
      SRSX
    
      Off
    """
    Off: typing.ClassVar[RotationRule]
    Guideline: typing.ClassVar[RotationRule]
    SRSPlus: typing.ClassVar[RotationRule]
    SRSX: typing.ClassVar[RotationRule]
    __members__: typing.ClassVar[dict[str, RotationRule]]
    @typing.overload
    def __eq__(self, other: RotationRule) -> bool:
        ...
    @typing.overload
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __int__(self) -> int:
        ...
    @typing.overload
    def __ne__(self, other: RotationRule) -> bool:
        ...
    @typing.overload
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class BagRule:
    """
    Members:
    
      Seven
    
      Fourteen
    
      Random
    """
    Fourteen: typing.ClassVar[BagRule]
    Random: typing.ClassVar[BagRule]
    Seven: typing.ClassVar[BagRule]
    __members__: typing.ClassVar[dict[str, BagRule]]
    @typing.overload
    def __eq__(self, other: BagRule) -> bool:
        ...
    @typing.overload
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __int__(self) -> int:
        ...
    @typing.overload
    def __ne__(self, other: BagRule) -> bool:
        ...
    @typing.overload
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class SpinRule:
    """
    Members:
    
      AllMiniPlus
    
      AllMini
    
      TOnly
    
      Off
    """
    AllMini: typing.ClassVar[SpinRule]
    AllMiniPlus: typing.ClassVar[SpinRule]
    Guideline: typing.ClassVar[SpinRule]
    Off: typing.ClassVar[SpinRule]
    TOnly: typing.ClassVar[SpinRule]
    __members__: typing.ClassVar[dict[str, SpinRule]]
    @typing.overload
    def __eq__(self, other: SpinRule) -> bool:
        ...
    @typing.overload
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __int__(self) -> int:
        ...
    @typing.overload
    def __ne__(self, other: SpinRule) -> bool:
        ...
    @typing.overload
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class AttackRule:
    """
    Members:
    
      Season2
    
      Season1
    
      Jstris
    
      Custom
    
      Off
    """
    Custom: typing.ClassVar[AttackRule]
    Jstris: typing.ClassVar[AttackRule]
    Off: typing.ClassVar[AttackRule]
    Season1: typing.ClassVar[AttackRule]
    Season2: typing.ClassVar[AttackRule]
    __members__: typing.ClassVar[dict[str, AttackRule]]
    @typing.overload
    def __eq__(self, other: AttackRule) -> bool:
        ...
    @typing.overload
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __int__(self) -> int:
        ...
    @typing.overload
    def __ne__(self, other: AttackRule) -> bool:
        ...
    @typing.overload
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class SoloMode:
    """
    Members:
    
      Marathon
    
      Sprint
    
      Blitz
    """
    Blitz: typing.ClassVar[SoloMode]
    Marathon: typing.ClassVar[SoloMode]
    Sprint: typing.ClassVar[SoloMode]
    __members__: typing.ClassVar[dict[str, SoloMode]]
    @typing.overload
    def __eq__(self, other: SoloMode) -> bool:
        ...
    @typing.overload
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __int__(self) -> int:
        ...
    @typing.overload
    def __ne__(self, other: SoloMode) -> bool:
        ...
    @typing.overload
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class AttackTable:
    def __init__(self) -> None:
        ...
    @property
    def b2b_bonus(self) -> int:
        ...
    @b2b_bonus.setter
    def b2b_bonus(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def combo_multiplier(self) -> float:
        ...
    @combo_multiplier.setter
    def combo_multiplier(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None:
        ...
    @property
    def full(self) -> typing.Annotated[list[int], "FixedSize(5)"]:
        ...
    @full.setter
    def full(self, arg0: typing.Annotated[collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex], "FixedSize(5)"]) -> None:
        ...
    @property
    def mini(self) -> typing.Annotated[list[int], "FixedSize(5)"]:
        ...
    @mini.setter
    def mini(self, arg0: typing.Annotated[collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex], "FixedSize(5)"]) -> None:
        ...
    @property
    def normal(self) -> typing.Annotated[list[int], "FixedSize(5)"]:
        ...
    @normal.setter
    def normal(self, arg0: typing.Annotated[collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex], "FixedSize(5)"]) -> None:
        ...
    @property
    def perfect_clear_bonus(self) -> int:
        ...
    @perfect_clear_bonus.setter
    def perfect_clear_bonus(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
class ScoreTable:
    def __init__(self) -> None:
        ...
    @property
    def combo_bonus(self) -> int:
        ...
    @combo_bonus.setter
    def combo_bonus(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def full(self) -> typing.Annotated[list[int], "FixedSize(5)"]:
        ...
    @full.setter
    def full(self, arg0: typing.Annotated[collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex], "FixedSize(5)"]) -> None:
        ...
    @property
    def hard_drop_point(self) -> int:
        ...
    @hard_drop_point.setter
    def hard_drop_point(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def mini(self) -> typing.Annotated[list[int], "FixedSize(5)"]:
        ...
    @mini.setter
    def mini(self, arg0: typing.Annotated[collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex], "FixedSize(5)"]) -> None:
        ...
    @property
    def normal(self) -> typing.Annotated[list[int], "FixedSize(5)"]:
        ...
    @normal.setter
    def normal(self, arg0: typing.Annotated[collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex], "FixedSize(5)"]) -> None:
        ...
    @property
    def perfect_clear_bonus(self) -> int:
        ...
    @perfect_clear_bonus.setter
    def perfect_clear_bonus(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def soft_drop_point(self) -> int:
        ...
    @soft_drop_point.setter
    def soft_drop_point(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
class EngineConfig:
    """
    Rule choices and timing for new Engine, SoloSession, and Match instances.
    """
    attack: AttackRule
    attacks: AttackTable
    bag: BagRule
    guideline_2009: bool
    level: int
    rotation: RotationRule
    scores: ScoreTable
    spin: SpinRule
    def __init__(self) -> None:
        ...
    @property
    def garbage_delay(self) -> int:
        ...
    @garbage_delay.setter
    def garbage_delay(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def gravity(self) -> float:
        ...
    @gravity.setter
    def gravity(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None:
        ...
    @property
    def gravity_increase(self) -> float:
        ...
    @gravity_increase.setter
    def gravity_increase(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None:
        ...
    @property
    def gravity_margin(self) -> int:
        ...
    @gravity_margin.setter
    def gravity_margin(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def height(self) -> int:
        ...
    @height.setter
    def height(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def lock_delay(self) -> int:
        ...
    @lock_delay.setter
    def lock_delay(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def lock_resets(self) -> int:
        ...
    @lock_resets.setter
    def lock_resets(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def preview_count(self) -> int:
        ...
    @preview_count.setter
    def preview_count(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def width(self) -> int:
        ...
    @width.setter
    def width(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
class Position:
    @property
    def col(self) -> int:
        ...
    @property
    def row(self) -> int:
        ...
class Offset:
    @property
    def col(self) -> int:
        ...
    @property
    def row(self) -> int:
        ...
class SpinContext:
    @property
    def guideline_point_five(self) -> bool:
        ...
    @property
    def kick_offset(self) -> Offset | None:
        ...
    @property
    def last_movement(self) -> Movement:
        ...
class Placement:
    """
    Engine-generated placement with its final pose, movement path, and spin.
    """
    def cells(self) -> list[Position]:
        """
        Four landing cells before line clearing, in board row/column coordinates.
        """
    @property
    def drop_distance(self) -> int:
        ...
    @property
    def final_pos(self) -> Position:
        ...
    @property
    def final_rotation(self) -> Rotation:
        ...
    @property
    def moves(self) -> list[Movement]:
        ...
    @property
    def spin_context(self) -> SpinContext:
        ...
    @property
    def spin(self) -> SpinType:
        ...
    @property
    def type(self) -> PieceType:
        ...
    @property
    def use_hold(self) -> bool:
        ...
class Piece:
    def current_shape(self) -> list[Offset]:
        ...
    def render(self) -> typing.Annotated[list[typing.Annotated[list[Cell], "FixedSize(4)"]], "FixedSize(4)"]:
        ...
    @property
    def cell_type(self) -> Cell:
        ...
    @property
    def rotation(self) -> Rotation:
        ...
    @property
    def type(self) -> PieceType:
        ...
class ActivePiece:
    @property
    def piece(self) -> Piece:
        ...
    @property
    def pos(self) -> Position:
        ...
class Board:
    def aggregate_height(self) -> int:
        ...
    def column_height(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> int:
        ...
    def get(self, row: typing.SupportsInt | typing.SupportsIndex, col: typing.SupportsInt | typing.SupportsIndex) -> Cell:
        ...
    def max_height(self) -> int:
        ...
    def render(self) -> list[list[Cell]]:
        ...
    @property
    def height(self) -> int:
        ...
    @property
    def width(self) -> int:
        ...
class GarbageEvent:
    def __init__(self, lines: typing.SupportsInt | typing.SupportsIndex, hole_col: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def hole_col(self) -> int:
        ...
    @property
    def lines(self) -> int:
        ...
class PendingGarbage:
    @property
    def delay_ticks(self) -> int:
        ...
    @property
    def event(self) -> GarbageEvent:
        ...
class LockResult:
    @property
    def attack_packets(self) -> list[int]:
        ...
    @property
    def attack_sent(self) -> int:
        ...
    @property
    def game_over(self) -> bool:
        ...
    @property
    def lines_cleared(self) -> int:
        ...
    @property
    def locked(self) -> bool:
        ...
    @property
    def perfect_clear(self) -> bool:
        ...
    @property
    def score_delta(self) -> int:
        ...
    @property
    def spin(self) -> SpinType:
        ...
class TickResult:
    @property
    def attack_packets(self) -> list[int]:
        ...
    @property
    def attack_sent(self) -> int:
        ...
    @property
    def game_over(self) -> bool:
        ...
    @property
    def lines_cleared(self) -> int:
        ...
    @property
    def locked(self) -> bool:
        ...
    @property
    def moved_down(self) -> bool:
        ...
class OpeningTransform:
    @property
    def offset(self) -> int:
        ...
    @property
    def mirrored(self) -> bool:
        ...
class Engine:
    """
    Headless engine. Call reset(seed) and spawn_next_piece(); locks do not spawn.
    """
    def __init__(self, config: EngineConfig = ...) -> None:
        ...
    def apply_placement(self, placement: Placement) -> LockResult:
        """
        Validate and atomically lock a placement without spawning the next piece.
        """
    def clone(self) -> Engine:
        """
        Independent engine copy, including RNG and hidden bag state. Keep it out of model inputs.
        """
    def generate_active_piece_placement(self) -> list[Placement]:
        """
        Reachable hard-drop placements for the active piece.
        """
    def generate_hold_piece_placement(self) -> list[Placement]:
        """
        Reachable hard-drop placements after hold.
        """
    def generate_opening(self, pattern: collections.abc.Sequence[collections.abc.Sequence[Cell]], filled_rows: collections.abc.Sequence[bool], clean_garbage_layers: typing.SupportsInt | typing.SupportsIndex, cheese_garbage_layers: typing.SupportsInt | typing.SupportsIndex, seed: typing.SupportsInt | typing.SupportsIndex, pieces: collections.abc.Sequence[PieceType] = ..., hold: PieceType | None = None, allow_mirror: bool = True) -> OpeningTransform:
        """
        Generate the selected opening after reset and before spawning. Mirrored
        templates also mirror scripted S/Z and J/L pieces.
        """
    def hard_drop(self) -> LockResult:
        ...
    def hard_drop_distance(self) -> int:
        ...
    def hold(self, ignore_hold: bool = False) -> bool:
        ...
    def lock_active_piece(self) -> LockResult:
        ...
    def paint_cell(self, row: typing.SupportsInt | typing.SupportsIndex, col: typing.SupportsInt | typing.SupportsIndex, cell: Cell) -> bool:
        ...
    def pending_garbage(self) -> list[PendingGarbage]:
        """
        Copy of incoming packets in arrival order, including travel delays.
        """
    def preview_queue(self) -> list[PieceType]:
        ...
    @typing.overload
    def receive_garbage(self, lines: typing.SupportsInt | typing.SupportsIndex, hole_col: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @typing.overload
    def receive_garbage(self, event: GarbageEvent) -> None:
        ...
    def receive_random_garbage(self, lines: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def render(self) -> list[list[Cell]]:
        """
        Board copy with the active piece overlaid; row zero is the bottom.
        """
    def reset(self, seed: typing.SupportsInt | typing.SupportsIndex = 0) -> None:
        ...
    def restore(self, snapshot: Engine) -> None:
        """
        Restore in place from a clone with the same rule context, including RNG.
        """
    def set_edit_mode(self, arg0: bool) -> bool:
        ...
    def set_garbage_delay(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def set_level(self, level: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def set_gravity(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None:
        ...
    def set_gravity_increase(self, per_tick: typing.SupportsFloat | typing.SupportsIndex, margin_tick: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def set_lock_delay(self, ticks: typing.SupportsInt | typing.SupportsIndex, reset_limit: typing.SupportsInt | typing.SupportsIndex = 15) -> None:
        ...
    def set_opening_pieces(self, pieces: collections.abc.Sequence[PieceType], hold: PieceType | None = None) -> None:
        """
        Set scripted active/next pieces and optional hold before spawning. The seeded bag resumes when the sequence is exhausted.
        """
    def spawn_next_piece(self, clutch_clear: bool = False) -> bool:
        ...
    def tick(self) -> TickResult:
        ...
    def try_move_left(self, amount: typing.SupportsInt | typing.SupportsIndex = 1) -> bool:
        ...
    def try_move_right(self, amount: typing.SupportsInt | typing.SupportsIndex = 1) -> bool:
        ...
    def try_rotate_180(self) -> bool:
        ...
    def try_rotate_ccw(self) -> bool:
        ...
    def try_rotate_cw(self) -> bool:
        ...
    def try_soft_drop(self, amount: typing.SupportsInt | typing.SupportsIndex = 1) -> bool:
        ...
    @property
    def active_piece(self) -> ActivePiece | None:
        ...
    @property
    def back_to_back(self) -> bool:
        ...
    @property
    def back_to_back_count(self) -> int:
        ...
    @property
    def board(self) -> Board:
        ...
    @property
    def can_hold(self) -> bool:
        ...
    @property
    def combo(self) -> int:
        ...
    @property
    def edit_mode(self) -> bool:
        ...
    @property
    def game_over(self) -> bool:
        ...
    @property
    def guideline_2009(self) -> bool:
        ...
    @property
    def ghost_piece(self) -> ActivePiece | None:
        ...
    @property
    def hold_piece(self) -> PieceType | None:
        ...
    @property
    def last_lock_result(self) -> LockResult | None:
        ...
    @property
    def level(self) -> int:
        ...
    @property
    def lines_cleared(self) -> int:
        ...
    @property
    def pending_garbage_lines(self) -> int:
        ...
    @property
    def pending_garbage_packets(self) -> int:
        ...
    @property
    def score(self) -> int:
        ...
    @property
    def surge_charge(self) -> int:
        ...
    @property
    def tick_count(self) -> int:
        ...
class SoloSettings:
    mode: SoloMode
    def __init__(self) -> None:
        ...
    @property
    def line_goal(self) -> int:
        ...
    @line_goal.setter
    def line_goal(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    @property
    def time_limit_ticks(self) -> int:
        ...
    @time_limit_ticks.setter
    def time_limit_ticks(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
class SoloSession:
    """
    Solo objective on top of Engine; start(seed) spawns the first piece.
    """
    def __init__(self, config: EngineConfig = ..., settings: SoloSettings = ...) -> None:
        ...
    def hard_drop(self) -> LockResult:
        ...
    def start(self, seed: typing.SupportsInt | typing.SupportsIndex = 0) -> None:
        ...
    def tick(self) -> TickResult:
        ...
    @property
    def elapsed_ticks(self) -> int:
        ...
    @property
    def engine(self) -> Engine:
        ...
    @property
    def finished(self) -> bool:
        ...
    @property
    def succeeded(self) -> bool:
        ...
class Match:
    """
    Local multiplayer; start(seed) seeds all players and incoming holes.
    """
    def __init__(self, configs: collections.abc.Sequence[EngineConfig], wins_to_win: typing.SupportsInt | typing.SupportsIndex = 1) -> None:
        ...
    def hard_drop(self, player: typing.SupportsInt | typing.SupportsIndex) -> LockResult:
        ...
    def player(self, index: typing.SupportsInt | typing.SupportsIndex) -> Engine:
        ...
    def set_target(self, sender: typing.SupportsInt | typing.SupportsIndex, target: typing.SupportsInt | typing.SupportsIndex | None) -> None:
        ...
    def start(self, seed: typing.SupportsInt | typing.SupportsIndex = 0) -> None:
        ...
    def tick(self) -> list[TickResult]:
        ...
    def wins(self, index: typing.SupportsInt | typing.SupportsIndex) -> int:
        ...
    @property
    def match_winner(self) -> int | None:
        ...
    @property
    def player_count(self) -> int:
        ...
    @property
    def round_over(self) -> bool:
        ...
    @property
    def round_winner(self) -> int | None:
        ...
