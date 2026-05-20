#include "engine/Engine.hpp"

namespace tetris {

void Engine::reset(uint64_t seed) {
  // TODO: Implement full engine reset.
}

std::vector<PieceType> Engine::preview_queue() const {
  // TODO: Return the engine preview queue.
  return {};
}

bool Engine::spawn_next_piece() {
  // TODO: Spawn the next piece.
  return false;
}

bool Engine::hold() {
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

ActivePiece Engine::ghost_piece() const {
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
