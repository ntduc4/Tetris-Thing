#include "engine/Rotation.hpp"
#include "engine/Spin.hpp"
#include "engine/Score.hpp"
#include "engine/Attack.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tetris {
namespace {
bool is_rotation(Movement movement) {
  return movement == Movement::CW || movement == Movement::CCW ||
         movement == Movement::HalfRotation;
}

// Offsets are {row (up), column (right)} in board coordinates.
std::vector<Offset> non_i_90(Rotation from, Rotation to) {
  using R = Rotation;
  if ((from == R::North && to == R::East) ||
      (from == R::South && to == R::East))
    return {{0, 0}, {0, -1}, {1, -1}, {-2, 0}, {-2, -1}};
  if ((from == R::East && to == R::North) ||
      (from == R::East && to == R::South))
    return {{0, 0}, {0, 1}, {-1, 1}, {2, 0}, {2, 1}};
  if ((from == R::South && to == R::West) ||
      (from == R::North && to == R::West))
    return {{0, 0}, {0, 1}, {1, 1}, {-2, 0}, {-2, 1}};
  if ((from == R::West && to == R::South) ||
      (from == R::West && to == R::North))
    return {{0, 0}, {0, -1}, {-1, -1}, {2, 0}, {2, -1}};
  return {};
}

// Symmetric I-piece kicks of SRS+ (rather than guideline SRS).
std::vector<Offset> i_90(Rotation from, Rotation to) {
  using R = Rotation;
  if (from == R::North && to == R::East)
    return {{0, 0}, {0, 1}, {0, -2}, {-1, -2}, {2, 1}};
  if (from == R::East && to == R::South)
    return {{0, 0}, {0, -1}, {0, 2}, {2, -1}, {-1, 2}};
  if (from == R::South && to == R::West)
    return {{0, 0}, {0, 2}, {0, -1}, {1, 2}, {-2, -1}};
  if (from == R::West && to == R::North)
    return {{0, 0}, {0, 1}, {0, -2}, {-2, 1}, {1, -2}};
  if (from == R::North && to == R::West)
    return {{0, 0}, {0, -1}, {0, 2}, {-1, 2}, {2, -1}};
  if (from == R::West && to == R::South)
    return {{0, 0}, {0, 1}, {0, -2}, {2, 1}, {-1, -2}};
  if (from == R::South && to == R::East)
    return {{0, 0}, {0, -2}, {0, 1}, {1, -2}, {-2, 1}};
  if (from == R::East && to == R::North)
    return {{0, 0}, {0, -1}, {0, 2}, {-2, -1}, {1, 2}};
  return {};
}

std::vector<Offset> i_90_srs(Rotation from, Rotation to) {
  using R = Rotation;
  if (from == R::North && to == R::East)
    return {{0, 0}, {0, -2}, {0, 1}, {-1, -2}, {2, 1}};
  if (from == R::East && to == R::North)
    return {{0, 0}, {0, 2}, {0, -1}, {1, 2}, {-2, -1}};
  if (from == R::East && to == R::South)
    return {{0, 0}, {0, -1}, {0, 2}, {2, -1}, {-1, 2}};
  if (from == R::South && to == R::East)
    return {{0, 0}, {0, 1}, {0, -2}, {-2, 1}, {1, -2}};
  if (from == R::South && to == R::West)
    return {{0, 0}, {0, 2}, {0, -1}, {1, 2}, {-2, -1}};
  if (from == R::West && to == R::South)
    return {{0, 0}, {0, -2}, {0, 1}, {-1, -2}, {2, 1}};
  if (from == R::West && to == R::North)
    return {{0, 0}, {0, 1}, {0, -2}, {-2, 1}, {1, -2}};
  if (from == R::North && to == R::West)
    return {{0, 0}, {0, -1}, {0, 2}, {2, -1}, {-1, 2}};
  return {};
}

// Heboris / NullpoMino SRS-X. Values are {up, right}; all tests are ordered.
std::vector<Offset> half_turn_x(PieceType piece, Rotation from) {
  using R = Rotation;
  if (piece == PieceType::I) {
    switch (from) {
    case R::North: return {{0, 0}, {0, -1}, {0, -2}, {0, 1}, {0, 2}, {-1, 0}};
    case R::East: return {{0, 0}, {-1, 0}, {-2, 0}, {1, 0}, {2, 0}, {0, -1}};
    case R::South: return {{0, 0}, {0, 1}, {0, 2}, {0, -1}, {0, -2}, {1, 0}};
    case R::West: return {{0, 0}, {-1, 0}, {-2, 0}, {1, 0}, {2, 0}, {0, 1}};
    default: return {};
    }
  }
  switch (from) {
  case R::North:
    return {{0, 0}, {0, 1}, {0, 2}, {-1, 1}, {-1, 2}, {0, -1},
            {0, -2}, {-1, -1}, {-1, -2}, {1, 0}, {0, 3}, {0, -3}};
  case R::East:
    return {{0, 0}, {-1, 0}, {-2, 0}, {-1, -1}, {-2, -1}, {1, 0},
            {2, 0}, {1, -1}, {2, -1}, {0, 1}, {-3, 0}, {3, 0}};
  case R::South:
    return {{0, 0}, {0, -1}, {0, -2}, {1, -1}, {1, -2}, {0, 1},
            {0, 2}, {1, 1}, {1, 2}, {-1, 0}, {0, -3}, {0, 3}};
  case R::West:
    return {{0, 0}, {-1, 0}, {-2, 0}, {-1, 1}, {-2, 1}, {1, 0},
            {2, 0}, {1, 1}, {2, 1}, {0, -1}, {-3, 0}, {3, 0}};
  default: return {};
  }
}

std::vector<Offset> half_turn(PieceType piece, Rotation from) {
  using R = Rotation;
  if (piece == PieceType::I || piece == PieceType::S ||
      piece == PieceType::Z) {
    switch (from) {
    case R::North: return {{0, 0}, {1, 0}};
    case R::East: return {{0, 0}, {0, 1}};
    case R::South: return {{0, 0}, {-1, 0}};
    case R::West: return {{0, 0}, {0, -1}};
    default: return {};
    }
  }
  switch (from) {
  case R::North: return {{0, 0}, {1, 0}, {1, 1}, {1, -1}, {0, 1}, {0, -1}};
  case R::East: return {{0, 0}, {0, 1}, {2, 1}, {1, 1}, {2, 0}, {1, 0}};
  case R::South: return {{0, 0}, {-1, 0}, {-1, -1}, {-1, 1}, {0, -1}, {0, 1}};
  case R::West: return {{0, 0}, {0, -1}, {2, -1}, {1, -1}, {2, 0}, {1, 0}};
  default: return {};
  }
}

bool immobile(const Board &board, const ActivePiece &piece) {
  if (!board.grounded(piece))
    return false;
  for (Position delta : {Position{0, -1}, Position{0, 1}, Position{1, 0}}) {
    ActivePiece moved = piece;
    moved.pos.row += delta.row;
    moved.pos.col += delta.col;
    if (!board.collide(moved))
      return false;
  }
  return true;
}

} // namespace

std::vector<Offset> SRS::kick_offsets(PieceType piece, Rotation from,
                                      Rotation to) const {
  if (from == to || from == Rotation::Count || to == Rotation::Count)
    return {};
  if (piece == PieceType::O)
    return {{0, 0}};
  if ((static_cast<unsigned>(from) + 2) % 4 == static_cast<unsigned>(to))
    return half_turn(piece, from);
  return piece == PieceType::I ? i_90(from, to) : non_i_90(from, to);
}

std::vector<Offset> SRSX::kick_offsets(PieceType piece, Rotation from,
                                       Rotation to) const {
  if (from == to || from == Rotation::Count || to == Rotation::Count)
    return {};
  if (piece == PieceType::O)
    return {{0, 0}};
  if ((static_cast<unsigned>(from) + 2) % 4 == static_cast<unsigned>(to))
    return half_turn_x(piece, from);
  return piece == PieceType::I ? i_90_srs(from, to) : non_i_90(from, to);
}

SpinType TSpin::detect(const Board &board, const ActivePiece &piece,
                       const SpinContext &context) const {
  if (piece.piece.type() != PieceType::T || !is_rotation(context.last_movement))
    return SpinType::None;
  int center_row = piece.pos.row + 2;
  int center_col = piece.pos.col + 1;
  std::array<bool, 4> corners = {
      board.occupied(center_row + 1, center_col - 1),
      board.occupied(center_row + 1, center_col + 1),
      board.occupied(center_row - 1, center_col - 1),
      board.occupied(center_row - 1, center_col + 1)};
  if (std::count(corners.begin(), corners.end(), true) < 3)
    return SpinType::None;
  bool front = false;
  switch (piece.piece.rotation()) {
  case Rotation::North: front = corners[0] && corners[1]; break;
  case Rotation::East: front = corners[1] && corners[3]; break;
  case Rotation::South: front = corners[2] && corners[3]; break;
  case Rotation::West: front = corners[0] && corners[2]; break;
  default: break;
  }
  // SRS' last kick lifts the T by two rows; this promotes a mini to full.
  if (front || (context.kick_offset &&
                std::abs(context.kick_offset->row) == 2 &&
                std::abs(context.kick_offset->col) == 1))
    return SpinType::Full;
  return SpinType::Mini;
}

SpinType AllSpin::detect(const Board &board, const ActivePiece &piece,
                         const SpinContext &context) const {
  if (!is_rotation(context.last_movement))
    return SpinType::None;
  if (piece.piece.type() == PieceType::T) {
    SpinType corners = TSpin{}.detect(board, piece, context);
    if (corners != SpinType::None)
      return corners;
    return _t_immobile_mini && immobile(board, piece) ? SpinType::Mini
                                                      : SpinType::None;
  }
  return immobile(board, piece) ? SpinType::Mini : SpinType::None;
}

uint64_t Classical::score(const ScoreEvent &event) const {
  const size_t lines = std::min<size_t>(event.lines_cleared, 4);
  uint64_t points = 0;
  if (event.spin == SpinType::Full)
    points = _table.full[lines];
  else if (event.spin == SpinType::Mini)
    points = _table.mini[lines];
  else
    points = _table.normal[lines];
  if (event.back_to_back && (event.spin != SpinType::None || lines == 4))
    points = points * 3 / 2;
  if (lines != 0)
    points += _table.combo_bonus * event.combo;
  if (event.perfect_clear && lines != 0)
    points += _table.perfect_clear_bonus;
  return points + event.hard_drop_cells * _table.hard_drop_point +
         event.soft_drop_cells * _table.soft_drop_point;
}

uint16_t TableAttack::garbage_to_send(const AttackContext &ctx) const {
  if (ctx.lines_cleared == 0)
    return 0;
  size_t lines = std::min<size_t>(ctx.lines_cleared, 4);
  uint32_t base = ctx.spin == SpinType::Full ? _table.full[lines]
                : ctx.spin == SpinType::Mini ? _table.mini[lines]
                                             : _table.normal[lines];
  if (ctx.back_to_back && (ctx.spin != SpinType::None || ctx.lines_cleared >= 4))
    base += _table.b2b_bonus;
  base = static_cast<uint32_t>(std::floor(base *
      (1.0 + _table.combo_multiplier * ctx.combo)));
  if (ctx.perfect_clear)
    base += _table.perfect_clear_bonus;
  return static_cast<uint16_t>(std::min<uint32_t>(base, UINT16_MAX));
}

namespace {
uint16_t season_two_base(const AttackContext &ctx) {
  if (ctx.perfect_clear)
    return 5;
  if (ctx.spin == SpinType::Mini && ctx.piece != PieceType::T)
    return 0;
  return TableAttack{}.garbage_to_send(ctx);
}
} // namespace

uint16_t TetrioSS2::garbage_to_send(const AttackContext &ctx) const {
  uint64_t attack = season_two_base(ctx);
  return static_cast<uint16_t>(std::min<uint64_t>(attack + ctx.surge_on_break,
                                                   UINT16_MAX));
}

std::vector<uint16_t> TetrioSS2::garbage_packets(const AttackContext &ctx) const {
  uint16_t base = season_two_base(ctx);
  uint32_t surge = garbage_to_send(ctx) - base;
  std::vector<uint16_t> packets;
  if (base)
    packets.push_back(base);
  // Round remainders into the first two packets.
  for (uint32_t part : {(surge + 2) / 3, (surge + 1) / 3, surge / 3})
    if (part)
      packets.push_back(static_cast<uint16_t>(part));
  return packets;
}

uint32_t TetrioSS2::surge_charge(uint32_t b2b_count) const {
  // Approximation for non-Quick Play Season 2: charge four lines at B2B x4,
  // then one more for each additional difficult clear.
  return b2b_count >= 4 ? b2b_count : 0;
}

uint16_t TetrioSS1::garbage_to_send(const AttackContext &ctx) const {
  AttackTable table;
  table.perfect_clear_bonus = 10;
  return TableAttack(table).garbage_to_send(ctx);
}

uint16_t JSTris::garbage_to_send(const AttackContext &ctx) const {
  return TetrioSS1{}.garbage_to_send(ctx);
}
} // namespace tetris
