#include "engine/test_support.hpp"
#include "engine/Engine.hpp"

#include <array>
#include <memory>
#include <utility>
#include <vector>

namespace {
using namespace tetris;

void test_i_kicks_are_distinct() {
  using R = Rotation;
  // Ordered (row up, column right); reference: TETR.IO SRS+ and
  // guideline I 90-degree kick tables. Each includes the basic test.
  const std::array<std::pair<R, R>, 8> transitions = {{
      {R::North, R::East}, {R::East, R::North},
      {R::East, R::South}, {R::South, R::East},
      {R::South, R::West}, {R::West, R::South},
      {R::West, R::North}, {R::North, R::West}}};
  const std::array<std::vector<Offset>, 8> plus = {{
      {{0, 0}, {0, 1}, {0, -2}, {-1, -2}, {2, 1}},
      {{0, 0}, {0, -1}, {0, 2}, {-2, -1}, {1, 2}},
      {{0, 0}, {0, -1}, {0, 2}, {2, -1}, {-1, 2}},
      {{0, 0}, {0, -2}, {0, 1}, {1, -2}, {-2, 1}},
      {{0, 0}, {0, 2}, {0, -1}, {1, 2}, {-2, -1}},
      {{0, 0}, {0, 1}, {0, -2}, {2, 1}, {-1, -2}},
      {{0, 0}, {0, 1}, {0, -2}, {-2, 1}, {1, -2}},
      {{0, 0}, {0, -1}, {0, 2}, {-1, 2}, {2, -1}}
  }};
  const std::array<std::vector<Offset>, 8> guideline = {{
      {{0, 0}, {0, -2}, {0, 1}, {-1, -2}, {2, 1}},
      {{0, 0}, {0, 2}, {0, -1}, {1, 2}, {-2, -1}},
      {{0, 0}, {0, -1}, {0, 2}, {2, -1}, {-1, 2}},
      {{0, 0}, {0, 1}, {0, -2}, {-2, 1}, {1, -2}},
      {{0, 0}, {0, 2}, {0, -1}, {1, 2}, {-2, -1}},
      {{0, 0}, {0, -2}, {0, 1}, {-1, -2}, {2, 1}},
      {{0, 0}, {0, 1}, {0, -2}, {-2, 1}, {1, -2}},
      {{0, 0}, {0, -1}, {0, 2}, {2, -1}, {-1, 2}}
  }};
  const std::array<std::vector<Offset>, 8> other_pieces = {{
      {{0, 0}, {0, -1}, {1, -1}, {-2, 0}, {-2, -1}},
      {{0, 0}, {0, 1}, {-1, 1}, {2, 0}, {2, 1}},
      {{0, 0}, {0, 1}, {-1, 1}, {2, 0}, {2, 1}},
      {{0, 0}, {0, -1}, {1, -1}, {-2, 0}, {-2, -1}},
      {{0, 0}, {0, 1}, {1, 1}, {-2, 0}, {-2, 1}},
      {{0, 0}, {0, -1}, {-1, -1}, {2, 0}, {2, -1}},
      {{0, 0}, {0, -1}, {-1, -1}, {2, 0}, {2, -1}},
      {{0, 0}, {0, 1}, {1, 1}, {-2, 0}, {-2, 1}}
  }};
  for (size_t i = 0; i < transitions.size(); ++i) {
    auto [from, to] = transitions[i];
    require(SRSPlus{}.kick_offsets(PieceType::I, from, to) == plus[i],
            "SRS+ I 90-degree kicks must follow the symmetric table");
    require(SRSX{}.kick_offsets(PieceType::I, from, to) == guideline[i],
            "SRS-X I 90-degree kicks must follow guideline SRS");
    require(SRSPlus{}.kick_offsets(PieceType::T, from, to) == other_pieces[i] &&
                SRSX{}.kick_offsets(PieceType::T, from, to) == other_pieces[i],
            "both variants must use ordered guideline JLSTZ 90-degree kicks");
  }
}

void test_srs_x_180_kicks() {
  using R = Rotation;
  const std::array<R, 4> from = {R::North, R::East, R::South, R::West};
  const std::array<R, 4> to = {R::South, R::West, R::North, R::East};
  const std::array<std::vector<Offset>, 4> standard = {{
      {{0, 0}, {0, 1}, {0, 2}, {-1, 1}, {-1, 2}, {0, -1},
       {0, -2}, {-1, -1}, {-1, -2}, {1, 0}, {0, 3}, {0, -3}},
      {{0, 0}, {-1, 0}, {-2, 0}, {-1, -1}, {-2, -1}, {1, 0},
       {2, 0}, {1, -1}, {2, -1}, {0, 1}, {-3, 0}, {3, 0}},
      {{0, 0}, {0, -1}, {0, -2}, {1, -1}, {1, -2}, {0, 1},
       {0, 2}, {1, 1}, {1, 2}, {-1, 0}, {0, -3}, {0, 3}},
      {{0, 0}, {-1, 0}, {-2, 0}, {-1, 1}, {-2, 1}, {1, 0},
       {2, 0}, {1, 1}, {2, 1}, {0, -1}, {-3, 0}, {3, 0}}
  }};
  const std::array<std::vector<Offset>, 4> i_kicks = {{
      {{0, 0}, {0, -1}, {0, -2}, {0, 1}, {0, 2}, {-1, 0}},
      {{0, 0}, {-1, 0}, {-2, 0}, {1, 0}, {2, 0}, {0, -1}},
      {{0, 0}, {0, 1}, {0, 2}, {0, -1}, {0, -2}, {1, 0}},
      {{0, 0}, {-1, 0}, {-2, 0}, {1, 0}, {2, 0}, {0, 1}}
  }};
  for (size_t index = 0; index < 4; ++index) {
    require(SRSX{}.kick_offsets(PieceType::T, from[index], to[index]) ==
                standard[index],
            "SRS-X must use ordered Heboris JLSTZ 180-degree kicks");
    require(SRSX{}.kick_offsets(PieceType::I, from[index], to[index]) ==
                i_kicks[index],
            "SRS-X must use ordered Heboris I 180-degree kicks");
    require(SRSPlus{}.kick_offsets(PieceType::T, from[index], to[index]) !=
                standard[index],
            "SRS+ 180-degree kicks must remain distinct from SRS-X");
    require(SRSPlus{}.kick_offsets(PieceType::O, from[index], to[index]) ==
                std::vector<Offset>{{0, 0}} &&
                SRSX{}.kick_offsets(PieceType::O, from[index], to[index]) ==
                std::vector<Offset>{{0, 0}},
            "O-piece rotation should not wall-kick in either variant");
  }
}

void test_engine_uses_selected_rotation() {
  Board board(10, 24);
  board.set(10, 2, Cell::Garbage); // Blocks in-place I rotation, not the spawn shape.
  Engine plus(board, std::make_unique<SRSPlus>(), make_randomizer(),
              nullptr, nullptr, nullptr, 5, 0, 10);
  Engine x(board, std::make_unique<SRSX>(), make_randomizer(),
           nullptr, nullptr, nullptr, 5, 0, 10);
  require(plus.spawn_next_piece() && x.spawn_next_piece(),
          "both rotation variants should spawn the test I piece");
  require(plus.try_rotate_cw() && x.try_rotate_cw(),
          "both rotation variants should find a legal kick");
  require(plus.active_piece()->pos.col == 1 && x.active_piece()->pos.col == -2,
          "the engine must select different I kicks for SRS+ and SRS-X");
}
} // namespace

int main() {
  test_i_kicks_are_distinct();
  test_srs_x_180_kicks();
  test_engine_uses_selected_rotation();
  return 0;
}
