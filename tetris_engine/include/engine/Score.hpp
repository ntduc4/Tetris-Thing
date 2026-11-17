#pragma once

#include "engine/Spin.hpp"

#include <cstdint>
#include <array>
#include <memory>
#include <stdexcept>

namespace tetris {

/** Captures the scoring inputs produced by a lock or drop. */
struct ScoreEvent {
  uint16_t lines_cleared = 0;
  SpinType spin = SpinType::None;
  bool perfect_clear = false;
  bool back_to_back = false;
  uint32_t combo = 0;
  uint16_t hard_drop_cells = 0;
  uint16_t soft_drop_cells = 0;
};

/** Interface for calculating points awarded by score events. */
class ScoreSystem {
public:
  /** Destroys a score system through the base interface. */
  virtual ~ScoreSystem() = default;

  /** Returns the score value awarded for a scoring event. */
  virtual uint64_t score(const ScoreEvent &event) const = 0;
  virtual std::unique_ptr<ScoreSystem> clone() const {
    throw std::logic_error("score system does not support cloning");
  }
};

/** Traditional line-clear scoring. */
struct ScoreTable {
  std::array<uint64_t, 5> normal{0, 100, 300, 500, 800};
  std::array<uint64_t, 5> mini{100, 200, 400, 800, 1600};
  std::array<uint64_t, 5> full{400, 800, 1200, 1600, 2600};
  uint64_t perfect_clear_bonus = 3500;
  uint64_t combo_bonus = 50;
  uint16_t hard_drop_point = 2;
  uint16_t soft_drop_point = 1;
};

class Classical : public ScoreSystem {
public:
  explicit Classical(ScoreTable table = {}) : _table(table) {}
  uint64_t score(const ScoreEvent &event) const override;
  std::unique_ptr<ScoreSystem> clone() const override {
    return std::make_unique<Classical>(*this);
  }

private:
  ScoreTable _table;
};
/** Time-limited score attack rules. */
class Blitz : public Classical {
public:
  std::unique_ptr<ScoreSystem> clone() const override {
    return std::make_unique<Blitz>(*this);
  }
};
/** Line-race scoring rules. */
class Sprint : public Classical {
public:
  std::unique_ptr<ScoreSystem> clone() const override {
    return std::make_unique<Sprint>(*this);
  }
};

} // namespace tetris
