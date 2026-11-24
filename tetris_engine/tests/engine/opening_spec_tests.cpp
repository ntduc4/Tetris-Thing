#include "engine/test_support.hpp"

#include "engine/Engine.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using namespace tetris;

Engine make_game() {
  return Engine(Board(10, 40), make_rotation_system(),
                std::make_unique<SevenBag>(), nullptr, nullptr, nullptr,
                DefaultQueueLength, DefaultSpawnColOffset, 17);
}

void test_seeded_opening_and_mirrored_pieces() {
  Engine game = make_game();
  Engine bag = make_game();
  const std::vector<std::vector<Cell>> pattern{
      {Cell::Empty, Cell::J, Cell::Empty}, {Cell::L, Cell::S, Cell::Z}};
  const std::vector<PieceType> scripted{
      PieceType::S, PieceType::Z, PieceType::J, PieceType::L,
      PieceType::I, PieceType::O, PieceType::T};
  bool saw_mirror = false, saw_unmirrored = false;
  for (uint64_t seed = 0; seed < 50; ++seed) {
    game.reset(seed);
    bag.reset(seed);
    auto result = game.generate_opening(
        pattern, {true, false}, 2, 3, seed, scripted, PieceType::J);
    auto board = game.board().render();
    for (int row = 0; row < 2; ++row) {
      int hole = -1;
      for (int col = 0; col < 10; ++col)
        if (board[row][col] == Cell::Empty) hole = col;
      require(hole >= 0 && board[0][hole] == Cell::Empty &&
                  board[1][hole] == Cell::Empty,
              "clean garbage rows use one shared hole");
    }
    const auto x = result.offset;
    require(board[5][x + 1] == (result.mirrored ? Cell::L : Cell::J) &&
                board[5][x] == Cell::Empty &&
                board[5][x + 2] == Cell::Empty &&
                board[5][x ? x - 1 : x + 3] == Cell::Garbage &&
                board[6][x + (result.mirrored ? 2 : 0)] ==
                    (result.mirrored ? Cell::J : Cell::L) &&
                board[6][x + 1] == (result.mirrored ? Cell::Z : Cell::S) &&
                board[6][x + (result.mirrored ? 0 : 2)] ==
                    (result.mirrored ? Cell::S : Cell::Z),
            "engine places filled/overhang rows above clean and cheese garbage");
    require(board[7] == std::vector<Cell>(10, Cell::Empty),
            "all higher rows stay empty");
    const std::vector<PieceType> expected = result.mirrored ?
        std::vector<PieceType>{PieceType::Z, PieceType::S, PieceType::L,
                               PieceType::J, PieceType::I, PieceType::O,
                               PieceType::T} : scripted;
    require(game.preview_queue() == std::vector<PieceType>(
                expected.begin(), expected.begin() + 5) &&
                game.hold_piece() == (result.mirrored ? PieceType::L : PieceType::J),
            "engine mirrors all scripted pairs and hold but not I/O/T");
    require(game.spawn_next_piece() &&
                game.active_piece()->piece.type() == expected.front(),
            "the mirrored active piece spawns at its normal engine position");
    require(game.preview_queue().back() == expected[5],
            "the mirrored next-piece prefix is consumed normally");
    game.reset(seed);
    auto again = game.generate_opening(
        pattern, {true, false}, 2, 3, seed, scripted, PieceType::J);
    require(again.offset == x && again.mirrored == result.mirrored &&
                game.board().render() == board,
            "an opening is reproducible independently of bag RNG");
    saw_mirror |= result.mirrored;
    saw_unmirrored |= !result.mirrored;
  }
  require(saw_mirror && saw_unmirrored, "the seed chooses both orientations");
  bag.reset(42);
  game.reset(42);
  game.generate_opening({}, {}, 1, 2, 42);
  require(game.preview_queue() == bag.preview_queue(),
          "garbage-only openings keep the normal seeded bag");
}

void test_rejects_invalid_boards_without_painting() {
  Engine game = make_game();
  game.reset(42);
  bool rejected = false;
  try {
    game.generate_opening({{Cell::Garbage}}, {true}, 20, 0, 42);
  } catch (const std::invalid_argument &) { rejected = true; }
  require(rejected && game.board().max_height() == 0,
          "oversize openings cannot modify the board");
  rejected = false;
  try {
    game.generate_opening({std::vector<Cell>(10, Cell::Garbage)},
                          {false}, 19, 0, 42, {PieceType::T});
  } catch (const std::invalid_argument &) { rejected = true; }
  require(rejected && game.board().max_height() == 0,
          "spawn-blocking openings cannot modify the board");
}
} // namespace

int main() {
  test_seeded_opening_and_mirrored_pieces();
  test_rejects_invalid_boards_without_painting();
  return 0;
}
