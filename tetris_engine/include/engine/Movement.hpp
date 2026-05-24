#pragma once

#include "core/Piece.hpp"
#include <cstdint>
#include <vector>

namespace tetris {

/** Fine-grained movement labels used for placement and spin metadata. */
enum class Movement : uint8_t {
  None,
  Left,
  Right,
  SoftDrop,
  HardDrop,
  CW,
  CCW,
  HalfRotation,
  Gravity
};

/** Records a final piece placement and the moves that reached it. */
struct Placement {
  std::vector<Movement> moves;
  PieceType type;
  Position final_pos;
  Rotation final_rotation;
};

} // namespace tetris
