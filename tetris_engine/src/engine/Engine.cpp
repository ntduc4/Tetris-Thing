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
}

std::vector<PieceType> Engine::preview_queue() const {
  return _randomizer->preview(_preview_count);
}

bool Engine::spawn_next_piece(bool clutch_clear) {
  ActivePiece nextPiece = spawn_from_piece_type(
      _randomizer->peek(), _spawn_row_offset, _spawn_col_offset);

  if (_board.collide(nextPiece)) {
    if (!clutch_clear)
      return false;

    while (nextPiece.pos.col < _board.get_height() - PieceDimension) {
      nextPiece.pos.col++;
      if (!_board.collide(nextPiece)) {
        _randomizer->pop();
        _active_piece.emplace(nextPiece);
        _can_hold = true;
        return true;
      }
    }

    return false;
  }

  _randomizer->pop();
  _active_piece.emplace(nextPiece);
  _can_hold = true;
  return true;
}

bool Engine::hold(bool ignore_hold) {
  if (!ignore_hold && !_can_hold)
    return false;

  if (!_hold_piece.has_value()) {
    if (!_active_piece.has_value())
      return false;
    PieceType temp = _active_piece->piece.type();
    if (spawn_next_piece()) {
      _hold_piece = temp;
      _can_hold = false;
      return true;
    } else
      return false;
  }

  ActivePiece nextPiece = spawn_from_piece_type(
      _hold_piece.value(), _spawn_row_offset, _spawn_col_offset);
  if (_board.collide(nextPiece))
    return false;

  _hold_piece = _active_piece.value().piece.type();
  _active_piece.emplace(nextPiece);
  _can_hold = false;
  return true;
}

bool Engine::try_move_left() {
  // TODO: Implement left movement.
  return false;
}

bool Engine::try_move_right() {
  // TODO: Implement right movement.
  return false;
}

bool Engine::try_soft_drop(uint16_t) {
  // TODO: Implement soft drop.
  return false;
}

bool Engine::try_rotate_cw() {
  // TODO: Implement clockwise rotation.
  return false;
}

bool Engine::try_rotate_ccw() {
  // TODO: Implement counterclockwise rotation.
  return false;
}

bool Engine::try_rotate_180() {
  // TODO: Implement 180-degree rotation.
  return false;
}

std::optional<ActivePiece> Engine::ghost_piece() const {
  std::optional<ActivePiece> res = _active_piece;
  if (res.has_value()) {
    while (res->pos.row > -PieceDimension && !_board.collide(res.value()))
      res->pos.row--;
    res->pos.row++;
  }

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
  // TODO: Implement hard drop resolution.
  return {};
}

LockResult Engine::lock_active_piece() {
  // TODO: Lock the active piece and resolve line clears, update score, check
  // spin, etc...
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
