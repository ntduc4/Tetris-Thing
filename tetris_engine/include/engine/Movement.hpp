#pragma once

#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"

#include <cstdint>

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

bool collides(const Board &board, const ActivePiece &piece);

ActivePiece move_left(const ActivePiece &piece);
ActivePiece move_right(const ActivePiece &piece);
ActivePiece move_down(const ActivePiece &piece, uint16_t amount);
ActivePiece apply_offset(const ActivePiece &piece, const Offset &offset);

ActivePiece hard_drop_position(const Board &board, const ActivePiece &piece);

void lock_piece(Board &board, const ActivePiece &piece);

} // namespace tetris
