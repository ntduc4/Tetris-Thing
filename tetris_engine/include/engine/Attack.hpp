#pragma once

#include "engine/Spin.hpp"

#include <cstdint>

namespace tetris {

struct AttackContext {
  uint16_t lines_cleared = 0;
  SpinType spin = SpinType::None;
  bool perfect_clear = false;
  bool back_to_back = false;
  uint32_t combo = 0;
};

struct AttackEvent {
  uint16_t garbage_lines = 0;
  bool target_all = false;
};

class AttackScheme {
public:
  virtual ~AttackScheme() = default;

  virtual uint16_t garbage_to_send(const AttackContext &ctx) const = 0;
};

class TetrioSS2 : public AttackScheme {};
class TetrioSS1 : public AttackScheme {};
class JSTris : public AttackScheme {};

} // namespace tetris
