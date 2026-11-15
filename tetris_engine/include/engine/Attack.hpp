#pragma once

#include "core/Piece.hpp"
#include "engine/Spin.hpp"

#include <cstdint>
#include <array>
#include <vector>

namespace tetris {

/** Captures the lock outcome needed to compute outgoing garbage. */
struct AttackContext {
  uint16_t lines_cleared = 0;
  SpinType spin = SpinType::None;
  bool perfect_clear = false;
  bool back_to_back = false;
  uint32_t combo = 0;
  uint32_t surge_on_break = 0;
  PieceType piece = PieceType::T;
};

/** Describes garbage produced by a player action. */
struct AttackEvent {
  uint16_t garbage_lines = 0;
  bool target_all = false;
};

/** Interface for multiplayer garbage attack calculation. */
class AttackScheme {
public:
  /** Destroys an attack scheme through the base interface. */
  virtual ~AttackScheme() = default;

  /** Returns the number of garbage lines sent for a lock outcome. */
  virtual uint16_t garbage_to_send(const AttackContext &ctx) const = 0;
  /** Outgoing attack packets; normally one packet, or three surge parts. */
  virtual std::vector<uint16_t> garbage_packets(const AttackContext &ctx) const {
    uint16_t lines = garbage_to_send(ctx);
    return lines ? std::vector<uint16_t>{lines} : std::vector<uint16_t>{};
  }
  /** Returns charged surge lines for a B2B count; zero for non-surge schemes. */
  virtual uint32_t surge_charge(uint32_t) const { return 0; }
  virtual bool perfect_clear_keeps_b2b() const { return false; }
  virtual bool no_line_spin_keeps_b2b(SpinType, PieceType) const { return false; }
};

/** Configurable base attack, combo, B2B, and perfect-clear values. */
struct AttackTable {
  std::array<uint16_t, 5> normal{0, 0, 1, 2, 4};
  std::array<uint16_t, 5> mini{0, 0, 1, 2, 4};
  std::array<uint16_t, 5> full{0, 2, 4, 6, 8};
  uint16_t b2b_bonus = 1;
  uint16_t perfect_clear_bonus = 5;
  double combo_multiplier = 0.25;
};

class TableAttack : public AttackScheme {
public:
  explicit TableAttack(AttackTable table = {}) : _table(table) {}
  uint16_t garbage_to_send(const AttackContext &ctx) const override;

private:
  AttackTable _table;
};

/** TETR.IO Season 2 attack table. */
class TetrioSS2 : public AttackScheme {
public:
  uint16_t garbage_to_send(const AttackContext &ctx) const override;
  std::vector<uint16_t> garbage_packets(const AttackContext &ctx) const override;
  uint32_t surge_charge(uint32_t b2b_count) const override;
  bool perfect_clear_keeps_b2b() const override { return true; }
  bool no_line_spin_keeps_b2b(SpinType spin, PieceType piece) const override {
    return spin == SpinType::Mini && piece != PieceType::T;
  }
};
/** TETR.IO Season 1 attack table. */
class TetrioSS1 : public AttackScheme {
public:
  uint16_t garbage_to_send(const AttackContext &ctx) const override;
};
/** Jstris-style attack table. */
class JSTris : public AttackScheme {
public:
  uint16_t garbage_to_send(const AttackContext &ctx) const override;
};

} // namespace tetris
