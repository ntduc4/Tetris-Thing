#include "engine/test_support.hpp"

#define private public
#include "engine/Engine.hpp"
#undef private

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

void require_active_piece_equals(const tetris::Engine &engine,
                                 tetris::PieceType type, uint16_t row,
                                 uint16_t col, const char *message) {
  require(engine.active_piece().has_value(), message);
  require(engine.active_piece()->piece.type() == type, message);
  require(engine.active_piece()->pos.row == row, message);
  require(engine.active_piece()->pos.col == col, message);
}

void test_hold_returns_false_when_hold_is_disallowed_and_ignore_hold_is_false() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Engine engine(Board(10, 24), make_rotation_system(), std::move(randomizer));

  engine._active_piece.emplace(spawn_from_piece_type(PieceType::L));
  engine._hold_piece = PieceType::O;
  engine._can_hold = false;

  require(!engine.hold(false),
          "hold should fail when can_hold is false and ignore_hold is false");
  require_active_piece_equals(engine, PieceType::L, DefaultSpawnRowOffset,
                              DefaultSpawnColOffset,
                              "failed hold should preserve the active piece");
  require(engine.hold_piece().has_value() && engine.hold_piece().value() == PieceType::O,
          "failed hold should preserve the held piece");
  require(!engine.can_hold(), "failed hold should preserve can_hold");
  require(randomizer_ptr->pop_count == 0,
          "failed hold should not pop the randomizer");
}

void test_hold_returns_false_when_active_piece_is_missing() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Engine engine(Board(10, 24), make_rotation_system(), std::move(randomizer));

  engine._active_piece.reset();
  engine._can_hold = true;

  require(!engine.hold(true),
          "hold should fail when there is no active piece even if ignore_hold is true");
  require(!engine.active_piece().has_value(),
          "failed hold without an active piece should leave the active slot empty");
  require(!engine.hold_piece().has_value(),
          "failed hold without an active piece should not create a held piece");
  require(engine.can_hold(),
          "failed hold without an active piece should preserve can_hold");
  require(randomizer_ptr->pop_count == 0,
          "failed hold without an active piece should not pop the randomizer");
}

void test_hold_with_empty_hold_spawns_next_queue_piece_and_stores_previous_active_piece() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Engine engine(Board(10, 24), make_rotation_system(), std::move(randomizer));

  engine._active_piece.emplace(spawn_from_piece_type(PieceType::L));
  engine._hold_piece.reset();
  engine._can_hold = true;

  const PieceType next_queue_piece = randomizer_ptr->peek();

  require(engine.hold(false),
          "hold should succeed when hold is empty and the next queue piece can spawn");
  require_active_piece_equals(engine, next_queue_piece, DefaultSpawnRowOffset,
                              DefaultSpawnColOffset,
                              "successful empty-hold should replace the active piece with the next queue piece");
  require(engine.hold_piece().has_value() && engine.hold_piece().value() == PieceType::L,
          "successful empty-hold should store the previous active piece type");
  require(!engine.can_hold(),
          "successful hold should always disable hold until the next reset point");
  require(randomizer_ptr->pop_count == 1,
          "successful empty-hold should pop the randomizer exactly once");
}

void test_hold_with_empty_hold_returns_false_when_next_queue_piece_cannot_spawn() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Board board(10, 24);
  board.set(DefaultSpawnRowOffset + 2, DefaultSpawnColOffset, Cell::Garbage);
  Engine engine(board, make_rotation_system(), std::move(randomizer));

  engine._active_piece.emplace(spawn_from_piece_type(PieceType::L));
  engine._hold_piece.reset();
  engine._can_hold = true;

  require(!engine.hold(false),
          "hold should fail when hold is empty and the next queue piece cannot spawn");
  require_active_piece_equals(engine, PieceType::L, DefaultSpawnRowOffset,
                              DefaultSpawnColOffset,
                              "failed empty-hold should preserve the active piece");
  require(!engine.hold_piece().has_value(),
          "failed empty-hold should preserve an empty hold slot");
  require(engine.can_hold(), "failed empty-hold should preserve can_hold");
  require(randomizer_ptr->pop_count == 0,
          "failed empty-hold should not pop the randomizer");
}

void test_hold_swaps_with_existing_hold_piece_when_spawnable() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Engine engine(Board(10, 24), make_rotation_system(), std::move(randomizer));

  engine._active_piece.emplace(spawn_from_piece_type(PieceType::T));
  engine._hold_piece = PieceType::O;
  engine._can_hold = true;

  require(engine.hold(false),
          "hold should succeed when the held piece can spawn into the active slot");
  require_active_piece_equals(engine, PieceType::O, DefaultSpawnRowOffset,
                              DefaultSpawnColOffset,
                              "successful hold swap should activate the previously held piece");
  require(engine.hold_piece().has_value() && engine.hold_piece().value() == PieceType::T,
          "successful hold swap should store the previous active piece type");
  require(!engine.can_hold(),
          "successful hold swap should always disable hold until the next reset point");
  require(randomizer_ptr->pop_count == 0,
          "successful hold swap should not pop the randomizer");
}

void test_hold_returns_false_when_existing_hold_piece_cannot_spawn() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Board board(10, 24);
  board.set(DefaultSpawnRowOffset + 1, DefaultSpawnColOffset + 1, Cell::Garbage);
  Engine engine(board, make_rotation_system(), std::move(randomizer));

  engine._active_piece.emplace(spawn_from_piece_type(PieceType::T));
  engine._hold_piece = PieceType::O;
  engine._can_hold = true;

  require(!engine.hold(false),
          "hold should fail when the held piece cannot spawn into the active slot");
  require_active_piece_equals(engine, PieceType::T, DefaultSpawnRowOffset,
                              DefaultSpawnColOffset,
                              "failed hold swap should preserve the active piece");
  require(engine.hold_piece().has_value() && engine.hold_piece().value() == PieceType::O,
          "failed hold swap should preserve the held piece");
  require(engine.can_hold(), "failed hold swap should preserve can_hold");
  require(randomizer_ptr->pop_count == 0,
          "failed hold swap should not pop the randomizer");
}

void test_hold_can_ignore_can_hold_but_still_ends_disabled_after_success() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Engine engine(Board(10, 24), make_rotation_system(), std::move(randomizer));

  engine._active_piece.emplace(spawn_from_piece_type(PieceType::T));
  engine._hold_piece = PieceType::O;
  engine._can_hold = false;

  require(engine.hold(true),
          "ignore_hold should allow a hold even when can_hold is false");
  require_active_piece_equals(engine, PieceType::O, DefaultSpawnRowOffset,
                              DefaultSpawnColOffset,
                              "successful ignored hold should still swap in the held piece");
  require(engine.hold_piece().has_value() && engine.hold_piece().value() == PieceType::T,
          "successful ignored hold should still store the previous active piece type");
  require(!engine.can_hold(),
          "successful ignored hold should still leave can_hold false");
  require(randomizer_ptr->pop_count == 0,
          "successful ignored hold should not pop the randomizer for a swap");
}

} // namespace

int main() {
  test_hold_returns_false_when_hold_is_disallowed_and_ignore_hold_is_false();
  test_hold_returns_false_when_active_piece_is_missing();
  test_hold_with_empty_hold_spawns_next_queue_piece_and_stores_previous_active_piece();
  test_hold_with_empty_hold_returns_false_when_next_queue_piece_cannot_spawn();
  test_hold_swaps_with_existing_hold_piece_when_spawnable();
  test_hold_returns_false_when_existing_hold_piece_cannot_spawn();
  test_hold_can_ignore_can_hold_but_still_ends_disabled_after_success();
  return 0;
}
