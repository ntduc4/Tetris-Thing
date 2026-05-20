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

bool collides(const Board &board, const ActivePiece &piece);
bool can_place(const Board &board, const ActivePiece &piece);

ActivePiece move_left(const ActivePiece &piece);
ActivePiece move_right(const ActivePiece &piece);
ActivePiece move_down(const ActivePiece &piece, uint16_t amount);
ActivePiece apply_offset(const ActivePiece &piece, const Offset &offset);

bool try_move(const Board &board, ActivePiece &piece, Movement movement);
bool try_move_left(const Board &board, ActivePiece &piece);
bool try_move_right(const Board &board, ActivePiece &piece);
bool try_soft_drop(const Board &board, ActivePiece &piece,
                   uint16_t amount = 1);

std::optional<ActivePiece>
try_rotate_cw(const Board &board, const ActivePiece &piece,
              const RotationSystem &rotation_system);
std::optional<ActivePiece>
try_rotate_ccw(const Board &board, const ActivePiece &piece,
               const RotationSystem &rotation_system);
std::optional<ActivePiece>
try_rotate_180(const Board &board, const ActivePiece &piece,
               const RotationSystem &rotation_system);

ActivePiece hard_drop_position(const Board &board, const ActivePiece &piece);

void lock_piece(Board &board, const ActivePiece &piece);

} // namespace tetris
