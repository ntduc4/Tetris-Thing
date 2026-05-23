#include "engine/Engine.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include <optional>

namespace tetris {

void Engine::reset(uint64_t seed) {
  // TODO: Implement full engine reset.
  _randomizer->seed(seed);
  _board.reset();
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
        return true;
      }
    }

    return false;
  }

  _randomizer->pop();
  _active_piece.emplace(nextPiece);
  return true;
}

bool Engine::hold(bool ignore_hold) {
  // TODO: Implement hold behavior.
  return false;
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
  // TODO: Return the ghost piece position.
  return _active_piece;
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
  // TODO: Lock the active piece and resolve line clears.
  return {};
}

void Engine::receive_garbage(uint16_t lines, uint16_t hole_col) {
  // TODO: Apply incoming garbage.
}

void Engine::receive_garbage(const GarbageEvent &garbage) {
  // TODO: Apply a garbage event.
}

} // namespace tetris
