#pragma once

#include "engine/Spin.hpp"

#include <cstdint>

namespace tetris {

// Captures the lock outcome needed to compute outgoing garbage.
struct AttackContext {
  uint16_t lines_cleared = 0;
  SpinType spin = SpinType::None;
  bool perfect_clear = false;
  bool back_to_back = false;
  uint32_t combo = 0;
};

// Describes garbage produced by a player action.
struct AttackEvent {
  uint16_t garbage_lines = 0;
  bool target_all = false;
};

class AttackScheme {
public:
  virtual ~AttackScheme() = default;

  // Returns the number of garbage lines sent for a lock outcome.
  virtual uint16_t garbage_to_send(const AttackContext &ctx) const = 0;
};

// TETR.IO Season 2 attack table.
class TetrioSS2 : public AttackScheme {};
// TETR.IO Season 1 attack table.
class TetrioSS1 : public AttackScheme {};
// Jstris-style attack table.
class JSTris : public AttackScheme {};

} // namespace tetris
