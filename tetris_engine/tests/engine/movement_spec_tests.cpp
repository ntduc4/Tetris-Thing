#include "engine/test_support.hpp"

#include "engine/Movement.hpp"

namespace {

void test_movement_helpers_shift_piece_positions() {
  tetris::ActivePiece piece = tetris::spawn_from_piece_type(tetris::PieceType::T);

  tetris::ActivePiece left = tetris::move_left(piece);
  require(left.pos.col == piece.pos.col - 1,
          "move_left should decrement the column by one");

  tetris::ActivePiece right = tetris::move_right(piece);
  require(right.pos.col == piece.pos.col + 1,
          "move_right should increment the column by one");

  tetris::ActivePiece down = tetris::move_down(piece, 3);
  require(down.pos.row == piece.pos.row - 3,
          "move_down should decrement the row by the requested amount");

  tetris::ActivePiece offset =
      tetris::apply_offset(piece, tetris::Offset{.row = -2, .col = 4});
  require(offset.pos.row == piece.pos.row - 2 &&
              offset.pos.col == piece.pos.col + 4,
          "apply_offset should translate both row and column");
}

void test_movement_legality_helpers() {
  tetris::Board board(10, 24);
  tetris::ActivePiece piece = tetris::spawn_from_piece_type(tetris::PieceType::O);

  require(!tetris::collides(board, piece),
          "collides should report false for a spawnable piece on an empty board");
  require(tetris::can_place(board, piece),
          "can_place should report true for a legal placement");

  board.set(piece.pos.row + 1, piece.pos.col + 1, tetris::Cell::Garbage);
  require(tetris::collides(board, piece),
          "collides should report true when the board overlaps the piece");
  require(!tetris::can_place(board, piece),
          "can_place should reject occupied placements");
}

void test_try_move_and_rotation_helpers() {
  tetris::Board board(10, 24);
  tetris::ActivePiece piece = tetris::spawn_from_piece_type(tetris::PieceType::T);
  FakeRotationSystem rotation_system;

  require(tetris::try_move(board, piece, tetris::Movement::Left),
          "try_move should apply a legal left movement");
  require(tetris::try_move_left(board, piece),
          "try_move_left should move left when legal");
  require(tetris::try_move_right(board, piece),
          "try_move_right should move right when legal");
  require(tetris::try_soft_drop(board, piece, 2),
          "try_soft_drop should move down when legal");

  auto cw = tetris::try_rotate_cw(board, piece, rotation_system);
  require(cw.has_value() && cw->piece.rotation() == tetris::Rotation::East,
          "try_rotate_cw should return the clockwise-rotated piece");

  auto ccw = tetris::try_rotate_ccw(board, piece, rotation_system);
  require(ccw.has_value() && ccw->piece.rotation() == tetris::Rotation::West,
          "try_rotate_ccw should return the counterclockwise-rotated piece");

  auto rot180 = tetris::try_rotate_180(board, piece, rotation_system);
  require(rot180.has_value() && rot180->piece.rotation() == tetris::Rotation::South,
          "try_rotate_180 should return the 180-rotated piece");
}

void test_hard_drop_and_lock_helpers() {
  tetris::Board board(10, 24);
  tetris::ActivePiece piece = tetris::spawn_from_piece_type(tetris::PieceType::O);

  tetris::ActivePiece dropped = tetris::hard_drop_position(board, piece);
  require(dropped.pos.row < piece.pos.row,
          "hard_drop_position should place the piece below its spawn row");

  tetris::lock_piece(board, dropped);
  require(board.max_height() > 0,
          "lock_piece should write the piece into the board grid");
}

} // namespace

int main() {
  test_movement_helpers_shift_piece_positions();
  test_movement_legality_helpers();
  test_try_move_and_rotation_helpers();
  test_hard_drop_and_lock_helpers();
  return 0;
}
