#include "engine/Movement.hpp"

namespace tetris {

bool collides(const Board &board, const ActivePiece &piece) {
  // TODO: Implement collision detection.
  return false;
}

bool can_place(const Board &board, const ActivePiece &piece) {
  // TODO: Implement placement validation.
  return false;
}

ActivePiece move_left(const ActivePiece &piece) {
  // TODO: Return the piece shifted left.
  return piece;
}

ActivePiece move_right(const ActivePiece &piece) {
  // TODO: Return the piece shifted right.
  return piece;
}

ActivePiece move_down(const ActivePiece &piece, uint16_t amount) {
  // TODO: Return the piece shifted downward.
  return piece;
}

ActivePiece apply_offset(const ActivePiece &piece, const Offset &offset) {
  // TODO: Return the piece with an offset applied.
  return piece;
}

bool try_move(const Board &, ActivePiece &piece, Movement movement) {
  // TODO: Implement move attempts with collision checks.
  return false;
}

bool try_move_left(const Board &, ActivePiece &piece) {
  // TODO: Implement left move attempts.
  return false;
}

bool try_move_right(const Board &, ActivePiece &piece) {
  // TODO: Implement right move attempts.
  return false;
}

bool try_soft_drop(const Board &, ActivePiece &piece, uint16_t amount) {
  // TODO: Implement soft drop attempts.
  return false;
}

std::optional<ActivePiece>
try_rotate_cw(const Board &board, const ActivePiece &piece,
              const RotationSystem &rotation_system) {
  // TODO: Implement clockwise rotation attempts.
  return std::nullopt;
}

std::optional<ActivePiece>
try_rotate_ccw(const Board &board, const ActivePiece &piece,
               const RotationSystem &rotation_system) {
  // TODO: Implement counterclockwise rotation attempts.
  return std::nullopt;
}

std::optional<ActivePiece>
try_rotate_180(const Board &board, const ActivePiece &piece,
               const RotationSystem &rotation_system) {
  // TODO: Implement 180-degree rotation attempts.
  return std::nullopt;
}

ActivePiece hard_drop_position(const Board &, const ActivePiece &piece) {
  // TODO: Compute the hard drop landing position.
  return piece;
}

void lock_piece(Board &board, const ActivePiece &piece) {
  // TODO: Write the piece into the board.
}

} // namespace tetris
