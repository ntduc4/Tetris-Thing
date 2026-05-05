#pragma once

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
};

} // namespace tetris
