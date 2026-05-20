#pragma once

#include "engine/Spin.hpp"

#include <cstdint>

namespace tetris {

// Captures the scoring inputs produced by a lock or drop.
struct ScoreEvent {
  uint16_t lines_cleared = 0;
  SpinType spin = SpinType::None;
  bool perfect_clear = false;
  bool back_to_back = false;
  uint32_t combo = 0;
  uint16_t hard_drop_cells = 0;
  uint16_t soft_drop_cells = 0;
};

class ScoreSystem {
public:
  virtual ~ScoreSystem() = default;

  // Returns the score value awarded for a scoring event.
  virtual uint64_t score(const ScoreEvent &event) const = 0;
};

// Traditional line-clear scoring.
class Classical : public ScoreSystem {};
// Time-limited score attack rules.
class Blitz : public ScoreSystem {};
// Line-race scoring rules.
class Sprint : public ScoreSystem {};

} // namespace tetris
