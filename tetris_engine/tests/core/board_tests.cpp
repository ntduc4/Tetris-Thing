#include "core/test_support.hpp"

#include "core/Board.hpp"
#include "core/Piece.hpp"

namespace {

void test_board_constructors_and_dimensions() {
  using namespace tetris;

  Board default_board;
  require(default_board.getWidth() == BoardWidth,
          "default board uses standard width");
  require(default_board.getHeight() == BoardHeight,
          "default board uses standard height");

  Board custom_board(4, 6);
  require(custom_board.getWidth() == 4, "custom board width is stored");
  require(custom_board.getHeight() == 6, "custom board height is stored");
}

void test_board_get_set_and_occupied() {
  using namespace tetris;

  Board board(4, 4);
  require(board.get(0, 0) == Cell::Empty, "board starts empty");
  require(!board.occupied(0, 0), "empty board cell is not occupied");
  require(board.occupied(10, 10), "out of bounds cells are treated occupied");
  require(board.get(10, 10) == Cell::Empty,
          "out of bounds get returns empty cell");

  board.set(1, 2, Cell::L);
  require(board.get(1, 2) == Cell::L, "set stores cell value");
  require(board.occupied(1, 2), "set cell becomes occupied");

  board.set(9, 9, Cell::I);
  require(board.get(1, 2) == Cell::L,
          "out of bounds set does not affect valid cells");
}

void test_board_column_and_aggregate_height() {
  using namespace tetris;

  Board board(4, 5);
  board.set(0, 0, Cell::I);
  board.set(2, 1, Cell::T);
  board.set(4, 2, Cell::O);

  require(board.column_height(0) == 1, "single bottom block has height one");
  require(board.column_height(1) == 3,
          "highest occupied row determines column height");
  require(board.column_height(2) == 5,
          "top cell contributes full board height");
  require(board.column_height(3) == 0, "empty column has zero height");
  require(board.column_height(10) == 5,
          "out of bounds column height returns board height");
  require(board.aggregate_height() == 9,
          "aggregate height sums all column heights");
  require(board.max_height() == 5, "max height reports the tallest occupied row");
}

void test_board_render_returns_cell_grid() {
  using namespace tetris;

  Board board(3, 2);
  board.set(0, 1, Cell::S);
  board.set(1, 2, Cell::Z);

  auto rendered = board.render();
  require(rendered.size() == 2, "render returns one row per board row");
  require(rendered[0].size() == 3,
          "render returns one column per board column");
  require(rendered[0][1] == Cell::S, "render includes lower row cells");
  require(rendered[1][2] == Cell::Z, "render includes upper row cells");
  require(rendered[1][1] == Cell::Empty, "render preserves empty cells");
}

void test_board_spawnable_checks_bounds_and_collisions() {
  using namespace tetris;

  Board board(10, 24);
  ActivePiece piece = spawn_from_piece_type(PieceType::O);

  require(board.spawnable(piece), "default spawn is valid on empty board");

  board.set(piece.pos.row + 1, piece.pos.col + 1, Cell::Garbage);
  require(!board.spawnable(piece), "occupied spawn cell blocks piece");

  ActivePiece left_piece = spawn_from_piece_type(PieceType::O);
  left_piece.pos.col = -2;
  require(!board.spawnable(left_piece),
          "piece outside left boundary is invalid");

  ActivePiece right_piece = spawn_from_piece_type(PieceType::O);
  right_piece.pos.col = 8;
  require(!board.spawnable(right_piece),
          "piece outside right boundary is invalid");

  ActivePiece top_piece = spawn_from_piece_type(PieceType::O);
  top_piece.pos.row = 23;
  require(!board.spawnable(top_piece),
          "piece outside top boundary is invalid");
}

void test_clear_lines_handles_none_single_multiple_and_unclearable() {
  using namespace tetris;

  Board unchanged(4, 4);
  unchanged.set(0, 0, Cell::J);
  require(unchanged.clear_lines() == 0, "incomplete lines are not cleared");
  require(unchanged.get(0, 0) == Cell::J,
          "board stays unchanged when no lines clear");

  Board single(4, 4);
  for (uint16_t col = 0; col < single.getWidth(); ++col)
    single.set(0, col, Cell::Garbage);
  single.set(1, 1, Cell::L);
  require(single.clear_lines() == 1, "single full line clears");
  require(single.get(0, 1) == Cell::L, "cells above fall into cleared row");

  Board multiple(4, 4);
  for (uint16_t col = 0; col < multiple.getWidth(); ++col) {
    multiple.set(0, col, Cell::Garbage);
    multiple.set(1, col, Cell::Garbage);
  }
  multiple.set(2, 0, Cell::T);
  multiple.set(2, 1, Cell::T);
  require(multiple.clear_lines() == 2, "two full lines are cleared");
  require(multiple.get(0, 0) == Cell::T, "upper cells fall after clear");
  require(multiple.get(1, 0) == Cell::Empty,
          "cells above cleared rows shift down");

  Board unclearable(4, 4);
  for (uint16_t col = 0; col < unclearable.getWidth(); ++col)
    unclearable.set(0, col, Cell::Unclearable);
  require(unclearable.clear_lines() == 0,
          "unclearable cells prevent a row from clearing");
  require(unclearable.get(0, 0) == Cell::Unclearable,
          "unclearable row remains in place");
}

void test_clear_lines_resets_touch_max_height() {
  using namespace tetris;

  Board board(4, 4);
  board.set(2, 0, Cell::J);
  board.addGarbage(1, 1);
  require(board.touch_max_height(), "garbage push can set touch max height");

  for (uint16_t col = 0; col < board.getWidth(); ++col)
    board.set(0, col, Cell::Garbage);
  require(board.clear_lines() == 1, "line clear still succeeds after top-out flag");
  require(!board.touch_max_height(),
          "clearing lines resets the touch max height flag");
}

void test_add_garbage_inserts_rows_and_tracks_top_out() {
  using namespace tetris;

  Board board(4, 4);
  board.set(2, 0, Cell::J);
  board.addGarbage(1, 1);

  require(board.touch_max_height(), "pushing into top row marks top-out state");
  require(board.get(0, 0) == Cell::Garbage, "garbage fills blocked columns");
  require(board.get(0, 1) == Cell::Empty, "garbage keeps the free column open");
}

void test_add_garbage_caps_at_remaining_height() {
  using namespace tetris;

  auto setup_board = []() {
    Board board(4, 24);
    for (uint16_t row = 0; row < 20; ++row)
      board.set(row, 0, Cell::T);
    return board;
  };

  Board board_five = setup_board();
  board_five.addGarbage(1, 5);

  require(board_five.touch_max_height(),
          "capped garbage marks the board as touching max height");
  require(board_five.max_height() == 24,
          "garbage only raises the stack to the board ceiling");
  require(board_five.get(0, 0) == Cell::Garbage,
          "first inserted garbage row fills blocked columns");
  require(board_five.get(3, 0) == Cell::Garbage,
          "fourth inserted garbage row is still added");
  require(board_five.get(4, 0) == Cell::T,
          "existing stack shifts upward by the capped amount");
  require(board_five.get(0, 1) == Cell::Empty,
          "garbage preserves the requested hole column");

  Board board_twenty_four = setup_board();
  board_twenty_four.addGarbage(1, 24);

  require(board_twenty_four.touch_max_height(),
          "oversized garbage requests still mark top-out state");
  require(board_twenty_four.max_height() == 24,
          "oversized garbage requests stop at the ceiling");
  require(board_twenty_four.get(3, 0) == Cell::Garbage,
          "oversized garbage still only inserts the remaining rows");
  require(board_twenty_four.get(4, 0) == Cell::T,
          "oversized garbage shifts the stack by the capped amount");
}

} // namespace

int main() {
  test_board_constructors_and_dimensions();
  test_board_get_set_and_occupied();
  test_board_column_and_aggregate_height();
  test_board_render_returns_cell_grid();
  test_board_spawnable_checks_bounds_and_collisions();
  test_clear_lines_handles_none_single_multiple_and_unclearable();
  test_clear_lines_resets_touch_max_height();
  test_add_garbage_inserts_rows_and_tracks_top_out();
  test_add_garbage_caps_at_remaining_height();
  return 0;
}
