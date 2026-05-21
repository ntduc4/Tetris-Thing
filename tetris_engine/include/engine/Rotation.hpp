#pragma once

#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"

#include <optional>

namespace tetris {

class RotationSystem {
public:
  virtual ~RotationSystem() = default;

  // Returns the kick offset to test for a rotation transition.
  virtual std::optional<Offset> kick_offset(PieceType piece, Rotation from,
                                            Rotation to,
                                            uint8_t check_num) const = 0;
  // Attempts a clockwise rotation and returns the rotated piece on success.
  virtual std::optional<ActivePiece>
  try_rotate_cw(const Board &board, const ActivePiece &piece) const = 0;
  // Attempts a counterclockwise rotation and returns the rotated piece on success.
  virtual std::optional<ActivePiece>
  try_rotate_ccw(const Board &board, const ActivePiece &piece) const = 0;
  // Attempts a 180-degree rotation and returns the rotated piece on success.
  virtual std::optional<ActivePiece>
  try_rotate_180(const Board &board, const ActivePiece &piece) const = 0;
};

// Standard rotation system with wall-kick support.
class SRS : public RotationSystem {};
// Rotation system that rejects all rotations.
class None : public RotationSystem {};

} // namespace tetris
