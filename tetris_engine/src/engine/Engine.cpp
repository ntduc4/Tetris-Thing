#include "engine/Engine.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include "engine/Movement.hpp"
#include "engine/Spin.hpp"
#include <optional>
#include <vector>

namespace tetris {

void Engine::reset(uint64_t seed) {
  // TODO: Implement full engine reset.
  _randomizer->seed(seed);
  _board.reset();
  _can_hold = true;
  _game_over = false;
  _active_piece = std::nullopt;
  _hold_piece = std::nullopt;
  _spin_context = {};
}

std::vector<PieceType> Engine::preview_queue() const {
  return _randomizer->preview(_preview_count);
}

bool Engine::spawn_next_piece(bool clutch_clear) {
  if (_game_over)
    return false;

  ActivePiece nextPiece = spawn_from_piece_type(
      _randomizer->peek(), _spawn_row_offset, _spawn_col_offset);

  if (_board.collide(nextPiece)) {
    if (!clutch_clear)
      return false;

    while (nextPiece.pos.col < _board.get_height() - PieceDimension) {
      nextPiece.pos.col++;
      if (!_board.collide(nextPiece)) {
        _randomizer->pop();
        _active_piece = nextPiece;
        _can_hold = true;
        return true;
      }
    }

    return false;
  }

  _randomizer->pop();
  _active_piece = nextPiece;
  _can_hold = true;
  return true;
}

bool Engine::hold(bool ignore_hold) {
  if (_game_over || !_active_piece.has_value())
    return false;

  if (!ignore_hold && !_can_hold)
    return false;

  if (!_hold_piece.has_value()) {
    PieceType temp = _active_piece->piece.type();
    if (spawn_next_piece()) {
      _hold_piece = temp;
      _can_hold = false;
      _spin_context = {};
      return true;
    } else
      return false;
  }

  ActivePiece nextPiece = spawn_from_piece_type(
      _hold_piece.value(), _spawn_row_offset, _spawn_col_offset);
  if (_board.collide(nextPiece))
    return false;

  _hold_piece = _active_piece.value().piece.type();
  _active_piece = nextPiece;
  _can_hold = false;
  _spin_context = {};
  return true;
}

bool Engine::try_move_left(int16_t amount) {
  if (_game_over || !_active_piece.has_value())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;

  for (int32_t amt = amount < 0 ? _board.get_width() : amount; amt > 0; amt--) {
    p.pos.col--;
    if (_board.collide(p)) {
      p.pos.col++;
      if (p.pos.col == _active_piece.value().pos.col)
        return false;
      _active_piece = p;
      _spin_context.kick_offset = std::nullopt;
      _spin_context.last_movement = Movement::Left;
      return true;
    }
  }

  _active_piece = p;
  _spin_context.kick_offset = std::nullopt;
  _spin_context.last_movement = Movement::Left;
  return true;
}

bool Engine::try_move_right(int16_t amount) {
  if (_game_over || !_active_piece.has_value())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;

  for (int32_t amt = amount < 0 ? _board.get_width() : amount; amt > 0; amt--) {
    p.pos.col++;
    if (_board.collide(p)) {
      p.pos.col--;
      if (p.pos.col == _active_piece.value().pos.col)
        return false;
      _active_piece = p;
      _spin_context.kick_offset = std::nullopt;
      _spin_context.last_movement = Movement::Right;
      return true;
    }
  }

  _active_piece = p;
  _spin_context.kick_offset = std::nullopt;
  _spin_context.last_movement = Movement::Right;
  return true;
}

bool Engine::try_soft_drop(int16_t amount) {
  if (_game_over || !_active_piece.has_value())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;

  for (int32_t amt = amount < 0 ? _board.get_height() : amount; amt > 0;
       amt--) {
    p.pos.row--;
    if (_board.collide(p)) {
      p.pos.row++;
      if (p.pos.row == _active_piece.value().pos.row)
        return false;
      _active_piece = p;
      _spin_context.kick_offset = std::nullopt;
      _spin_context.last_movement = Movement::SoftDrop;
      return true;
    }
  }

  _active_piece = p;
  _spin_context.kick_offset = std::nullopt;
  _spin_context.last_movement = Movement::SoftDrop;
  return true;
}

bool Engine::try_rotate_cw() {
  if (_game_over || !_active_piece.has_value())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;

  Rotation r = p.piece.rotation();
  p.piece.rotate_cw();
  if (!_board.collide(p)) {
    _active_piece = p;
    _spin_context.kick_offset = std::nullopt;
    _spin_context.last_movement = Movement::CW;
    return true;
  }

  std::vector offsets =
      _rotation_system->kick_offsets(p.piece.type(), r, p.piece.rotation());
  for (Offset o : offsets) {
    p.pos = {.row = static_cast<int16_t>(_active_piece->pos.row + o.row),
             .col = static_cast<int16_t>(_active_piece->pos.col + o.col)};
    if (!_board.collide(p)) {
      _active_piece = p;
      _spin_context.kick_offset = o;
      _spin_context.last_movement = Movement::CW;
      return true;
    }
  }

  return false;
}

bool Engine::try_rotate_ccw() {
  if (_game_over || !_active_piece.has_value())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;

  Rotation r = p.piece.rotation();
  p.piece.rotate_ccw();
  if (!_board.collide(p)) {
    _active_piece = p;
    _spin_context.kick_offset = std::nullopt;
    _spin_context.last_movement = Movement::CCW;
    return true;
  }

  std::vector offsets =
      _rotation_system->kick_offsets(p.piece.type(), r, p.piece.rotation());
  for (Offset o : offsets) {
    p.pos = {.row = static_cast<int16_t>(_active_piece->pos.row + o.row),
             .col = static_cast<int16_t>(_active_piece->pos.col + o.col)};
    if (!_board.collide(p)) {
      _active_piece = p;
      _spin_context.kick_offset = o;
      _spin_context.last_movement = Movement::CCW;
      return true;
    }
  }

  return false;
}

bool Engine::try_rotate_180() {
  if (_game_over || !_active_piece.has_value())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;

  Rotation r = p.piece.rotation();
  p.piece.rotate_180();
  if (!_board.collide(p)) {
    _active_piece = p;
    _spin_context.kick_offset = std::nullopt;
    _spin_context.last_movement = Movement::HalfRotation;
    return true;
  }

  std::vector offsets =
      _rotation_system->kick_offsets(p.piece.type(), r, p.piece.rotation());
  for (Offset o : offsets) {
    p.pos = {.row = static_cast<int16_t>(_active_piece->pos.row + o.row),
             .col = static_cast<int16_t>(_active_piece->pos.col + o.col)};
    if (!_board.collide(p)) {
      _active_piece = p;
      _spin_context.kick_offset = o;
      _spin_context.last_movement = Movement::HalfRotation;
      return true;
    }
  }

  return false;
}

std::optional<ActivePiece> Engine::ghost_piece() const {
  std::optional<ActivePiece> res = _active_piece;

  if (!res.has_value() || _board.collide(res.value()))
    return std::nullopt;

  while (res->pos.row > -PieceDimension && !_board.grounded(res.value()))
    res->pos.row--;

  return res;
}

uint16_t Engine::hard_drop_distance() const {
  if (!_active_piece.has_value())
    return 0;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return 0;
  uint16_t res = 0;
  for (; !_board.grounded(p) && res <= _board.get_height() + PieceDimension;
       res++)
    ;

  return res;
}

StepResult Engine::step(Action) {
  // TODO: Dispatch a player action.
  return {};
}

TickResult Engine::tick() {
  // TODO: Advance gravity and lock timing.
  return {};
}

LockResult Engine::hard_drop() {
  if (_game_over || !_active_piece.has_value())
    return {};
  try_soft_drop(-1);
  return lock_active_piece();
}

LockResult Engine::lock_active_piece() {
  // TODO: Lock the active piece and resolve line clears, update score, check
  // spin, etc...
  if (_game_over || !_active_piece.has_value())
    return {};

  ActivePiece p = _active_piece.value();

  // Grounded test, do nothing if not grounded
  if (!_board.grounded(p))
    return {};
  for (Offset o : p.piece.current_shape())
    _board.set(o.row + p.pos.row, o.col + p.pos.col, p.piece.cell_type());

  _active_piece = std::nullopt;
  uint16_t lc = _board.clear_lines();

  return {.game_over = false,
          .lines_cleared = lc,
          .perfect_clear = false,
          .spin = SpinType::None,
          .attack_sent = 0};
}

void Engine::receive_garbage(uint16_t lines, uint16_t hole_col) {
  // TODO: Apply incoming garbage.
}

void Engine::receive_garbage(const GarbageEvent &garbage) {
  // TODO: Apply a garbage event.
}

std::vector<Placement> Engine::generate_active_piece_placement() const {
  return {};
}

std::vector<Placement> Engine::generate_hold_piece_placement() const {
  return {};
}

} // namespace tetris
