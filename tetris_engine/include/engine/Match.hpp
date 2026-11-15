#pragma once

#include "engine/Engine.hpp"
#include <cstddef>
#include <memory>
#include <optional>
#include <random>
#include <vector>

namespace tetris {

/** Deterministic, headless multiplayer match with round-robin targeting. */
class Match {
public:
  explicit Match(std::vector<std::unique_ptr<Engine>> players,
                 uint16_t wins_to_win = 1);

  /** Clear and reseed every board; start the next round. */
  void start(uint64_t seed = 0);
  /** Advance every board once; outgoing attacks are delivered simultaneously. */
  std::vector<TickResult> tick();
  /** Hard drop a player's piece and route its outgoing attack. */
  LockResult hard_drop(size_t player);

  Engine &player(size_t index) { return *_players.at(index); }
  const Engine &player(size_t index) const { return *_players.at(index); }
  size_t player_count() const { return _players.size(); }
  bool round_over() const { return _round_over; }
  std::optional<size_t> round_winner() const { return _round_winner; }
  std::optional<size_t> match_winner() const { return _match_winner; }
  uint16_t wins(size_t index) const { return _wins.at(index); }
  /** Target a specific opponent; omit to use the next surviving opponent. */
  void set_target(size_t sender, std::optional<size_t> target);

private:
  void deliver(size_t sender, uint16_t lines);
  void check_winner();

  std::vector<std::unique_ptr<Engine>> _players;
  std::vector<uint16_t> _wins;
  std::vector<std::optional<size_t>> _targets;
  /** Separate identically seeded incoming-hole streams for each player. */
  std::vector<std::mt19937_64> _garbage_rng;
  uint16_t _wins_to_win;
  bool _started = false;
  bool _round_over = false;
  std::optional<size_t> _round_winner;
  std::optional<size_t> _match_winner;
};
} // namespace tetris
