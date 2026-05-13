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
  virtual std::optional<Offset> rotate_cw(const Board &b, ActivePiece p);
  virtual std::optional<Offset> rotate_ccw(const Board &b, ActivePiece p);
  virtual std::optional<Offset> rotate_180(const Board &b, ActivePiece p);
};

class SRS : RotationSystem {};
class None : RotationSystem {};

} // namespace tetris
