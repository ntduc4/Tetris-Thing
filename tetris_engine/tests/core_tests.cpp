#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << '\n';
    std::exit(1);
  }
}

void test_piece_spawn_and_rotation() {
  using namespace tetris;

  ActivePiece active = spawn_from_piece_type(PieceType::T);
  require(active.pos.row == DefaultSpawnRowOffset,
          "spawn uses default row offset");
  require(active.pos.col == DefaultSpawnColOffset,
          "spawn uses default column offset");
  require(active.piece.rotation() == DefaultRotation,
          "spawn uses default rotation");
  require(active.piece.cell_type() == Cell::T, "piece type maps to T cell");

  active.piece.rotate_cw();
  require(active.piece.rotation() == Rotation::East,
          "clockwise rotation advances to east");

  active.piece.rotate_180();
  require(active.piece.rotation() == Rotation::West,
          "180 rotation advances by two states");

  active.piece.rotate_ccw();
  require(active.piece.rotation() == Rotation::South,
          "counterclockwise rotation moves back one state");
}

void test_piece_render_places_cells() {
  using namespace tetris;

  const Piece piece = get_from_piece_type(PieceType::O);
  const auto rendered = piece.render();

  require(rendered[1][1] == Cell::O, "O piece fills top-left cell");
  require(rendered[1][2] == Cell::O, "O piece fills top-right cell");
  require(rendered[2][1] == Cell::O, "O piece fills bottom-left cell");
  require(rendered[2][2] == Cell::O, "O piece fills bottom-right cell");
  require(rendered[0][0] == Cell::Empty, "render leaves empty cells untouched");
}

void test_board_set_get_and_occupied() {
  using namespace tetris;

  Board board(4, 4);
  require(board.get(0, 0) == Cell::Empty, "board starts empty");
  require(!board.occupied(0, 0), "empty board cell is not occupied");
  require(board.occupied(10, 10), "out of bounds cells are treated occupied");

  board.set(1, 2, Cell::L);
  require(board.get(1, 2) == Cell::L, "set stores cell value");
  require(board.occupied(1, 2), "set cell becomes occupied");

  board.set(9, 9, Cell::I);
  require(board.get(1, 2) == Cell::L,
          "out of bounds set does not affect valid cells");
}

void test_board_spawnable_checks_bounds_and_collisions() {
  using namespace tetris;

  Board board(10, 24);
  ActivePiece piece = spawn_from_piece_type(PieceType::O);

  require(board.spawnable(piece), "default spawn is valid on empty board");

  board.set(piece.pos.row + 1, piece.pos.col + 1, Cell::Garbage);
  require(!board.spawnable(piece), "occupied spawn cell blocks piece");

  piece.pos.col = -2;
  require(!board.spawnable(piece), "piece outside left boundary is invalid");
}

void test_clear_lines_removes_full_rows() {
  using namespace tetris;

  Board board(4, 4);
  for (uint16_t col = 0; col < board.getWidth(); ++col) {
    board.set(0, col, Cell::Garbage);
    board.set(1, col, Cell::Garbage);
  }

  board.set(2, 0, Cell::T);
  board.set(2, 1, Cell::T);

  require(board.clear_lines() == 2, "two full lines are cleared");
  require(board.get(0, 0) == Cell::T, "upper cells fall after clear");
  require(board.get(0, 1) == Cell::T, "upper cells preserve shape after clear");
  require(board.get(1, 0) == Cell::Empty, "cells above cleared rows shift down");
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

} // namespace

int main() {
  test_piece_spawn_and_rotation();
  test_piece_render_places_cells();
  test_board_set_get_and_occupied();
  test_board_spawnable_checks_bounds_and_collisions();
  test_clear_lines_removes_full_rows();
  test_add_garbage_inserts_rows_and_tracks_top_out();
  return 0;
}
