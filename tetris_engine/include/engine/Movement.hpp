#pragma once

#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include "core/Rotation.hpp"

#include <cstdint>
#include <optional>

namespace tetris {

enum class Movement : uint8_t {
  Left,
  Right,
  SoftDrop,
  HardDrop,
  CW,
  CCW,
  HalfRotation
};

// Returns true when the active piece overlaps blocked or out-of-bounds cells.
bool collides(const Board &board, const ActivePiece &piece);
// Returns true when the active piece can be placed on the board.
bool can_place(const Board &board, const ActivePiece &piece);

// Returns a copy of the piece shifted one column left.
ActivePiece move_left(const ActivePiece &piece);
// Returns a copy of the piece shifted one column right.
ActivePiece move_right(const ActivePiece &piece);
// Returns a copy of the piece shifted downward by the requested amount.
ActivePiece move_down(const ActivePiece &piece, uint16_t amount);
// Returns a copy of the piece with an arbitrary offset applied.
ActivePiece apply_offset(const ActivePiece &piece, const Offset &offset);

// Applies a movement if legal and mutates the piece in place.
bool try_move(const Board &board, ActivePiece &piece, Movement movement);
// Attempts to move the active piece left.
bool try_move_left(const Board &board, ActivePiece &piece);
// Attempts to move the active piece right.
bool try_move_right(const Board &board, ActivePiece &piece);
// Attempts to move the active piece downward.
bool try_soft_drop(const Board &board, ActivePiece &piece,
                   uint16_t amount = 1);

// Attempts a clockwise rotation using the provided rotation system.
std::optional<ActivePiece>
try_rotate_cw(const Board &board, const ActivePiece &piece,
              const RotationSystem &rotation_system);
// Attempts a counterclockwise rotation using the provided rotation system.
std::optional<ActivePiece>
try_rotate_ccw(const Board &board, const ActivePiece &piece,
               const RotationSystem &rotation_system);
// Attempts a 180-degree rotation using the provided rotation system.
std::optional<ActivePiece>
try_rotate_180(const Board &board, const ActivePiece &piece,
               const RotationSystem &rotation_system);

// Returns the resting position reached by a hard drop.
ActivePiece hard_drop_position(const Board &board, const ActivePiece &piece);

// Writes the active piece into the board cells.
void lock_piece(Board &board, const ActivePiece &piece);

} // namespace tetris
