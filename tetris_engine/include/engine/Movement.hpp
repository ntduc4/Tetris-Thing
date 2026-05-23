#include "core/Piece.hpp"
#include <cstdint>
#include <vector>

namespace tetris {

// Movement enum
enum class Movement : uint8_t {
  Left,
  Right,
  SoftDrop,
  HardDrop,
  CW,
  CCW,
  HalfRotation,
  Gravity,
  None
};

struct Placement {
  std::vector<Movement> moves;
  PieceType type;
  Position final_pos;
  Rotation final_rotation;
};

} // namespace tetris
