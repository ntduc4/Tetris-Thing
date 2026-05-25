#include "engine/test_support.hpp"

#define private public
#include "engine/Engine.hpp"
#undef private

#include <string>

namespace {

void fill_floating_row(tetris::Board &board, uint16_t row, uint16_t hole_col) {
  for (uint16_t col = 0; col < board.get_width(); ++col)
    if (col != hole_col)
      board.set(row, col, tetris::Cell::Garbage);
}

void require_hard_drop_distance_equals(const tetris::Engine &engine,
                                       uint16_t expected,
                                       const char *message) {
  uint16_t got = engine.hard_drop_distance();
  require(got == expected, std::string("Wrong hard drop distance: expected=") +
                               std::to_string(expected) + " got=" +
                               std::to_string(got) + " :: " + message);
}

void test_hard_drop_distance_on_empty_board_for_i_piece_orientations() {
  using namespace tetris;

  Engine engine(Board(10, 24), make_rotation_system(), make_randomizer());

  ActivePiece horizontal = spawn_from_piece_type(PieceType::I);
  horizontal.pos.row = 8;
  horizontal.pos.col = 3;
  engine._active_piece.emplace(horizontal);
  require_hard_drop_distance_equals(
      engine, 10,
      "horizontal I piece should drop to the floor on an empty board");
  require(engine.active_piece()->pos.row == horizontal.pos.row &&
              engine.active_piece()->pos.col == horizontal.pos.col,
          "hard_drop_distance should not mutate the active I piece");

  ActivePiece vertical = horizontal;
  vertical.piece.rotate_cw();
  engine._active_piece = vertical;
  require_hard_drop_distance_equals(
      engine, 8,
      "vertical I piece should travel fewer rows on an empty board");
  require(engine.active_piece()->pos.row == vertical.pos.row &&
              engine.active_piece()->pos.col == vertical.pos.col,
          "hard_drop_distance should leave the vertical I piece unchanged");
}

void test_hard_drop_distance_on_a_real_stack() {
  using namespace tetris;

  Board board(10, 24);
  board.set(3, 4, Cell::Garbage);
  board.set(3, 5, Cell::Garbage);

  Engine engine(board, make_rotation_system(), make_randomizer());
  ActivePiece active = spawn_from_piece_type(PieceType::J);
  active.piece.rotate_cw();
  active.pos.row = 8;
  active.pos.col = 3;
  engine._active_piece.emplace(active);

  require_hard_drop_distance_equals(
      engine, 5,
      "a rotated J piece should stop on top of a real stack");
  require(engine.active_piece()->pos.row == active.pos.row &&
              engine.active_piece()->pos.col == active.pos.col,
          "hard_drop_distance should not mutate the active stacked piece");
}

void test_hard_drop_distance_ignores_a_floating_stack_above_the_active_piece() {
  using namespace tetris;

  Board board(10, 24);
  fill_floating_row(board, 12, 4);

  Engine engine(board, make_rotation_system(), make_randomizer());
  ActivePiece active = spawn_from_piece_type(PieceType::O);
  active.pos.row = 3;
  active.pos.col = 3;
  engine._active_piece.emplace(active);

  require_hard_drop_distance_equals(
      engine, 4,
      "floating blocks above the piece should not change the drop distance");
  require(engine.active_piece()->pos.row == active.pos.row &&
              engine.active_piece()->pos.col == active.pos.col,
          "hard_drop_distance should not mutate the active piece under a "
          "floating stack");
}

void test_hard_drop_distance_returns_zero_for_a_colliding_active_piece() {
  using namespace tetris;

  Board board(10, 24);
  board.set(10, 4, Cell::Garbage);

  Engine engine(board, make_rotation_system(), make_randomizer());
  ActivePiece active = spawn_from_piece_type(PieceType::I);
  active.pos.row = 8;
  active.pos.col = 3;
  engine._active_piece.emplace(active);

  require_hard_drop_distance_equals(
      engine, 0,
      "a colliding active piece should report zero hard drop distance");
  require(engine.active_piece()->pos.row == active.pos.row &&
              engine.active_piece()->pos.col == active.pos.col,
          "hard_drop_distance should not mutate a colliding active piece");
}

} // namespace

int main() {
  test_hard_drop_distance_on_empty_board_for_i_piece_orientations();
  test_hard_drop_distance_on_a_real_stack();
  test_hard_drop_distance_ignores_a_floating_stack_above_the_active_piece();
  test_hard_drop_distance_returns_zero_for_a_colliding_active_piece();
  return 0;
}
