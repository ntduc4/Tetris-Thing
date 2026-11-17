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
  /** Commands before the final hard drop; hold is represented separately. */
  std::vector<Movement> moves;
  PieceType type = PieceType::T;
  /** Grounded root coordinate and rotation after the hard drop. */
  Position final_pos{};
  Rotation final_rotation = Rotation::North;
  bool use_hold = false;
  uint16_t drop_distance = 0;
  /** Spin metadata at lock time, after the hard drop clears it if it moved. */
  SpinContext spin_context;
};

} // namespace tetris
