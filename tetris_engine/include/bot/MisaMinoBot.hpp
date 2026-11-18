#pragma once

#include "engine/Engine.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace tetris {

/** Search limits for the MisaMino-inspired, placement-level bot. */
struct MisaMinoConfig {
  /** Total placements searched, including the current piece. */
  uint8_t search_depth = 3;
  /** Number of distinct states retained between placement layers. */
  size_t beam_width = 12;
  /** Maximum simulated placements; all root actions are always examined. */
  size_t node_budget = 4000;
};

/**
 * Independent adaptation of MisaMino's beam search and spin/attack-aware
 * evaluation. Not a port of its Windows code or a claim of move-for-move parity.
 *
 * The bot never changes the supplied Engine. It chooses an engine-generated,
 * replayable Placement; only the visible preview is used for lookahead. The
 * engine remains the authority on movement, holds, spins, clears and attacks.
 */
class MisaMinoBot {
public:
  explicit MisaMinoBot(MisaMinoConfig config = {});

  /** Returns nullopt if no active piece or legal placement is available. */
  std::optional<Placement> choose(const Engine &engine) const;

private:
  MisaMinoConfig _config;
};

} // namespace tetris
