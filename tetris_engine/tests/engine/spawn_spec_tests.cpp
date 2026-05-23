#include "engine/test_support.hpp"

#include "engine/Engine.hpp"

#include <memory>

namespace {

class TrackingRandomizer : public FakeRandomizer {
public:
  tetris::PieceType pop() override {
    ++pop_count;
    return FakeRandomizer::pop();
  }

  size_t pop_count = 0;
};

void test_spawn_next_piece_returns_false_without_clutch_clear_when_spawn_is_blocked() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Board board(10, 24);
  board.set(DefaultSpawnRowOffset + 2, DefaultSpawnColOffset, Cell::Garbage);
  Engine engine(board, make_rotation_system(), std::move(randomizer));

  const PieceType expected_piece = randomizer_ptr->peek();

  require(!engine.spawn_next_piece(false),
          "spawn should fail when the spawn area is blocked and clutch clear is off");
  require(randomizer_ptr->peek() == expected_piece,
          "failed spawn should not advance the randomizer");
  require(randomizer_ptr->pop_count == 0,
          "failed spawn should not pop from the randomizer");
  require(!engine.active_piece().has_value(),
          "failed spawn should leave the active piece unset");
}

void test_spawn_next_piece_spawns_and_pops_once_when_spawn_is_open() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Engine engine(Board(10, 24), make_rotation_system(), std::move(randomizer));

  const PieceType expected_piece = randomizer_ptr->peek();
  const PieceType next_piece = randomizer_ptr->preview(2)[1];

  require(engine.spawn_next_piece(false),
          "spawn should succeed when the spawn area is open");
  require(randomizer_ptr->pop_count == 1,
          "successful spawn should pop exactly once");
  require(randomizer_ptr->peek() == next_piece,
          "successful spawn should advance the randomizer exactly once");
  require(engine.active_piece().has_value(),
          "successful spawn should set the active piece");
  require(engine.active_piece()->piece.type() == expected_piece,
          "successful spawn should activate the queued piece");
}

void test_spawn_next_piece_clutch_clear_spawns_and_pops_once_when_shifted_spawn_fits() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Board board(10, 24);
  board.set(DefaultSpawnRowOffset + 2, DefaultSpawnColOffset, Cell::Garbage);
  Engine engine(board, make_rotation_system(), std::move(randomizer));

  const PieceType expected_piece = randomizer_ptr->peek();
  const PieceType next_piece = randomizer_ptr->preview(2)[1];

  require(engine.spawn_next_piece(true),
          "clutch clear should spawn when the piece can be shifted into a valid spawn position");
  require(randomizer_ptr->pop_count == 1,
          "clutch-clear spawn should pop exactly once");
  require(randomizer_ptr->peek() == next_piece,
          "clutch-clear spawn should advance the randomizer exactly once");
  require(engine.active_piece().has_value(),
          "clutch-clear spawn should set the active piece");
  require(engine.active_piece()->piece.type() == expected_piece,
          "clutch-clear spawn should activate the queued piece");
}

void test_spawn_next_piece_returns_false_when_clutch_clear_would_require_above_board_spawn() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Engine engine(Board(10, 24), make_rotation_system(), std::move(randomizer), nullptr,
                nullptr, nullptr, DefaultQueueLength, DefaultSpawnColOffset, 24);

  const PieceType expected_piece = randomizer_ptr->peek();

  require(!engine.spawn_next_piece(true),
          "clutch clear should fail when spawning would require placing the piece above the board");
  require(randomizer_ptr->peek() == expected_piece,
          "failed clutch-clear spawn should not advance the randomizer");
  require(randomizer_ptr->pop_count == 0,
          "failed clutch-clear spawn should not pop from the randomizer");
  require(!engine.active_piece().has_value(),
          "failed clutch-clear spawn should leave the active piece unset");
}

} // namespace

int main() {
  test_spawn_next_piece_returns_false_without_clutch_clear_when_spawn_is_blocked();
  test_spawn_next_piece_spawns_and_pops_once_when_spawn_is_open();
  test_spawn_next_piece_clutch_clear_spawns_and_pops_once_when_shifted_spawn_fits();
  test_spawn_next_piece_returns_false_when_clutch_clear_would_require_above_board_spawn();
  return 0;
}
