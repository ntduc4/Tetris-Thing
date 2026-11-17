#pragma once

#include "core/Piece.hpp"
#include <cstdint>
#include <optional>
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

/** Last movement and successful wall kick, used to classify a lock's spin. */
struct SpinContext {
  Movement last_movement = Movement::None;
  std::optional<Offset> kick_offset = std::nullopt;
  bool operator==(const SpinContext &) const = default;
};

/** One reachable, replayable hard-drop action. Outcome values belong to LockResult. */
struct Placement {
  /** Replayable commands ending in exactly one locking HardDrop; hold is separate. */
  std::vector<Movement> moves;
  PieceType type = PieceType::T;
  /** Grounded root coordinate and rotation after the hard drop. */
  Position final_pos{};
  Rotation final_rotation = Rotation::North;
  bool use_hold = false;
  uint16_t drop_distance = 0;
  /** Last move and kick before HardDrop; a moving drop cancels spin at lock. */
  SpinContext spin_context;

  /** The four board-space cells occupied just before line clearing. */
  std::vector<Position> cells() const {
    Piece piece = get_from_piece_type(type);
    piece.rotate_to(final_rotation);
    std::vector<Position> result;
    result.reserve(4);
    for (Offset offset : piece.current_shape())
      result.push_back({static_cast<int16_t>(final_pos.row + offset.row),
                        static_cast<int16_t>(final_pos.col + offset.col)});
    return result;
  }
};

} // namespace tetris
