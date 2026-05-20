#pragma once

#include "engine/Spin.hpp"

#include <cstdint>

namespace tetris {

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

  virtual uint64_t score(const ScoreEvent &event) const = 0;
};

class Classical : public ScoreSystem {};
class Blitz : public ScoreSystem {};
class Sprint : public ScoreSystem {};

} // namespace tetris
