#include "engine/Engine.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include <optional>

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
      return true;
    }
  }

  _active_piece = p;
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
      return true;
    }
  }

  _active_piece = p;
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
      return true;
    }
  }

  _active_piece = p;
  return true;
}

bool Engine::try_rotate_cw() {
  if (_game_over || !_active_piece.has_value())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;

  return true;
}

bool Engine::try_rotate_ccw() {
  if (_game_over || !_active_piece.has_value())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;

  return true;
}

bool Engine::try_rotate_180() {
  if (_game_over || !_active_piece.has_value())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;

  return true;
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
  // TODO: Compute the hard drop distance.
  return 0;
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
