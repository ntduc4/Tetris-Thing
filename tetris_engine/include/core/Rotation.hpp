#pragma once

#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"

#include <optional>

namespace tetris {

class RotationSystem {
public:
  virtual ~RotationSystem() = default;

  virtual std::optional<Offset> kick_offset(PieceType piece, Rotation from,
                                            Rotation to,
                                            uint8_t check_num) const = 0;
  virtual std::optional<ActivePiece>
  try_rotate_cw(const Board &board, const ActivePiece &piece) const = 0;
  virtual std::optional<ActivePiece>
  try_rotate_ccw(const Board &board, const ActivePiece &piece) const = 0;
  virtual std::optional<ActivePiece>
  try_rotate_180(const Board &board, const ActivePiece &piece) const = 0;
};

class SRS : public RotationSystem {};
class None : public RotationSystem {};

} // namespace tetris
