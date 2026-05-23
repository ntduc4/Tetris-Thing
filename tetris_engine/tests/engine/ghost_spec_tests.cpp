#include "engine/test_support.hpp"

#define private public
#include "engine/Engine.hpp"
#undef private

namespace {

void fill_floating_row(tetris::Board &board, uint16_t row, uint16_t hole_col) {
  for (uint16_t col = 0; col < board.get_width(); ++col)
    if (col != hole_col)
      board.set(row, col, tetris::Cell::Garbage);
}

void require_ghost_equals(const tetris::Engine &engine, tetris::PieceType type,
                          tetris::Rotation rotation, int16_t row, int16_t col,
                          const char *message) {
  std::optional<tetris::ActivePiece> ghost = engine.ghost_piece();

  require(ghost.has_value(),
          std::string("Ghost piece don't exist: ") + message);
  require(ghost->piece.type() == type,
          std::string("Wrong piece type: ") + message);
  require(ghost->piece.rotation() == rotation,
          std::string("Wrong rotation: ") + message);
  require(ghost->pos.row == row, std::string("Wrong row: ") + message);
  require(ghost->pos.col == col, std::string("Wrong col: ") + message);
}

void test_ghost_piece_returns_grounded_o_piece_on_empty_board() {
  using namespace tetris;

  Engine engine(Board(10, 24), make_rotation_system(), make_randomizer());
  ActivePiece active = spawn_from_piece_type(PieceType::O);
  active.pos.row = 8;
  active.pos.col = 3;
  engine._active_piece.emplace(active);

  require_ghost_equals(
      engine, PieceType::O, Rotation::North, -1, 3,
      "ghost_piece should ground an O piece on an empty board");
  require(engine.active_piece()->pos.row == active.pos.row,
          "ghost_piece should not mutate the active O piece");
}

void test_ghost_piece_returns_grounded_rotated_t_piece_on_empty_board() {
  using namespace tetris;

  Engine engine(Board(10, 24), make_rotation_system(), make_randomizer());
  ActivePiece active = spawn_from_piece_type(PieceType::T);
  active.piece.rotate_cw();
  active.pos.row = 11;
  active.pos.col = 5;
  engine._active_piece.emplace(active);

  require_ghost_equals(
      engine, PieceType::T, Rotation::East, -1, 5,
      "ghost_piece should ground a rotated T piece on an empty board");
  require(engine.active_piece()->pos.row == active.pos.row,
          "ghost_piece should not mutate the active rotated T piece");
}

void test_ghost_piece_stops_on_existing_stack() {
  using namespace tetris;

  Board board(10, 24);
  board.set(3, 4, Cell::Garbage);
  board.set(3, 5, Cell::Garbage);
  Engine engine(board, make_rotation_system(), make_randomizer());

  ActivePiece active = spawn_from_piece_type(PieceType::O);
  active.pos.row = 9;
  active.pos.col = 3;
  engine._active_piece.emplace(active);

  require_ghost_equals(
      engine, PieceType::O, Rotation::North, 3, 3,
      "ghost_piece should stop the piece on top of blocking cells");
  require(engine.active_piece()->pos.row == active.pos.row,
          "ghost_piece should not mutate the active piece");
}

void test_ghost_piece_handles_floating_single_hole_layers_for_multiple_shapes() {
  using namespace tetris;

  {
    Board board(10, 24);
    fill_floating_row(board, 6, 5);
    Engine engine(board, make_rotation_system(), make_randomizer());

    ActivePiece active = spawn_from_piece_type(PieceType::I);
    active.piece.rotate_cw();
    active.pos.row = 12;
    active.pos.col = 3;
    engine._active_piece.emplace(active);

    require_ghost_equals(engine, PieceType::I, Rotation::East, 0, 3,
                         "vertical I piece should fall through a one-cell "
                         "floating hole to the ground");
    require(engine.active_piece()->pos.row == active.pos.row,
            "ghost_piece should not mutate the active vertical I piece");
  }

  {
    Board board(10, 24);
    fill_floating_row(board, 6, 5);
    Engine engine(board, make_rotation_system(), make_randomizer());

    ActivePiece active = spawn_from_piece_type(PieceType::I);
    active.pos.row = 12;
    active.pos.col = 3;
    engine._active_piece.emplace(active);

    require_ghost_equals(engine, PieceType::I, Rotation::North, 5, 3,
                         "horizontal I piece should rest on the floating layer "
                         "instead of passing through the hole");
    require(engine.active_piece()->pos.row == active.pos.row,
            "ghost_piece should not mutate the active horizontal I piece");
  }

  {
    Board board(10, 24);
    fill_floating_row(board, 6, 5);
    Engine engine(board, make_rotation_system(), make_randomizer());

    ActivePiece active = spawn_from_piece_type(PieceType::T);
    active.piece.rotate_180();
    active.pos.row = 12;
    active.pos.col = 4;
    engine._active_piece.emplace(active);

    require_ghost_equals(engine, PieceType::T, Rotation::South, 5, 4,
                         "T piece should settle with its center cell in the "
                         "floating hole and its other cells above the layer");
  }

  {
    Board board(10, 24);
    fill_floating_row(board, 6, 5);
    Engine engine(board, make_rotation_system(), make_randomizer());

    ActivePiece active = spawn_from_piece_type(PieceType::J);
    active.piece.rotate_cw();
    active.pos.row = 12;
    active.pos.col = 4;
    engine._active_piece.emplace(active);

    require_ghost_equals(engine, PieceType::J, Rotation::East, 4, 4,
                         "J piece should tuck one column into the floating "
                         "hole and land lower than flat pieces");
  }

  {
    Board board(10, 24);
    fill_floating_row(board, 6, 5);
    Engine engine(board, make_rotation_system(), make_randomizer());

    ActivePiece active = spawn_from_piece_type(PieceType::L);
    active.piece.rotate_180();
    active.piece.rotate_cw();
    active.pos.row = 12;
    active.pos.col = 4;
    engine._active_piece.emplace(active);

    require_ghost_equals(engine, PieceType::L, Rotation::West, 4, 4,
                         "L piece should also tuck into the floating hole and "
                         "land lower than flat pieces");
  }

  {
    Board board(10, 24);
    fill_floating_row(board, 6, 4);
    Engine engine(board, make_rotation_system(), make_randomizer());

    ActivePiece active = spawn_from_piece_type(PieceType::S);
    active.piece.rotate_cw();
    active.pos.row = 12;
    active.pos.col = 2;
    engine._active_piece.emplace(active);

    require_ghost_equals(engine, PieceType::S, Rotation::East, 5, 2,
                         "S piece should rest on a floating layer with one "
                         "cell dipping into the hole");
  }

  {
    Board board(10, 24);
    fill_floating_row(board, 6, 4);
    Engine engine(board, make_rotation_system(), make_randomizer());

    ActivePiece active = spawn_from_piece_type(PieceType::Z);
    active.piece.rotate_180();
    active.piece.rotate_cw();
    active.pos.row = 12;
    active.pos.col = 4;
    engine._active_piece.emplace(active);

    require_ghost_equals(engine, PieceType::Z, Rotation::West, 5, 4,
                         "Z piece should rest on a floating layer with one "
                         "cell dipping into the hole");
  }
}

} // namespace

int main() {
  test_ghost_piece_returns_grounded_o_piece_on_empty_board();
  test_ghost_piece_returns_grounded_rotated_t_piece_on_empty_board();
  test_ghost_piece_stops_on_existing_stack();
  test_ghost_piece_handles_floating_single_hole_layers_for_multiple_shapes();
  return 0;
}
