#include "core/test_support.hpp"

#include "core/Piece.hpp"

#include <vector>

namespace {

bool same_offsets(const std::vector<tetris::Offset> &lhs,
                  const std::vector<tetris::Offset> &rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (size_t i = 0; i < lhs.size(); ++i)
    if (lhs[i].row != rhs[i].row || lhs[i].col != rhs[i].col)
      return false;
  return true;
}

void test_piece_helper_construction() {
  using namespace tetris;

  const struct {
    PieceType type;
    Cell cell;
  } cases[] = {{PieceType::I, Cell::I}, {PieceType::O, Cell::O},
               {PieceType::T, Cell::T}, {PieceType::S, Cell::S},
               {PieceType::Z, Cell::Z}, {PieceType::J, Cell::J},
               {PieceType::L, Cell::L}};

  for (const auto &test_case : cases) {
    Piece piece = get_from_piece_type(test_case.type);
    require(piece.type() == test_case.type,
            "helper preserves requested piece type");
    require(piece.cell_type() == test_case.cell,
            "helper maps piece type to correct cell type");
    require(piece.rotation() == DefaultRotation,
            "helper starts pieces at default rotation");
  }
}

void test_piece_spawn_defaults() {
  using namespace tetris;

  ActivePiece active = spawn_from_piece_type(PieceType::T);
  require(active.pos.row == DefaultSpawnRowOffset,
          "spawn uses default row offset");
  require(active.pos.col == DefaultSpawnColOffset,
          "spawn uses default column offset");
  require(active.piece.rotation() == DefaultRotation,
          "spawn uses default rotation");
  require(active.piece.cell_type() == Cell::T, "spawn maps to the right cell type");
}

void test_piece_rotation_methods() {
  using namespace tetris;

  Piece piece = get_from_piece_type(PieceType::T);
  piece.rotate_cw();
  require(piece.rotation() == Rotation::East,
          "clockwise rotation advances to east");

  piece.rotate_180();
  require(piece.rotation() == Rotation::West,
          "180 rotation advances by two states");

  piece.rotate_ccw();
  require(piece.rotation() == Rotation::South,
          "counterclockwise rotation moves back one state");

  piece.rotate_to(Rotation::North);
  require(piece.rotation() == Rotation::North,
          "rotate_to sets the requested rotation");

  piece.rotate_ccw();
  require(piece.rotation() == Rotation::West,
          "counterclockwise rotation wraps from north to west");
}

void test_piece_current_shape_and_render() {
  using namespace tetris;

  Piece o_piece = get_from_piece_type(PieceType::O);
  auto north_shape = o_piece.current_shape();
  require(north_shape.size() == 4, "current_shape returns four occupied cells");
  require(north_shape[0].row == 1 && north_shape[0].col == 1,
          "O piece shape matches expected north offsets");
  o_piece.rotate_cw();
  auto east_shape = o_piece.current_shape();
  require(same_offsets(east_shape, north_shape),
          "O piece shape is rotation invariant");

  Piece i_piece = get_from_piece_type(PieceType::I);
  auto rendered_north = i_piece.render();
  require(count_cells(rendered_north, Cell::I) == 4,
          "render marks exactly four occupied cells for I piece");
  require(rendered_north[2][0] == Cell::I && rendered_north[2][3] == Cell::I,
          "north I piece renders as a horizontal line");

  i_piece.rotate_cw();
  auto rendered_east = i_piece.render();
  require(count_cells(rendered_east, Cell::I) == 4,
          "rotated I piece still renders four occupied cells");
  require(rendered_east[0][2] == Cell::I && rendered_east[3][2] == Cell::I,
          "east I piece renders as a vertical line");
}

void test_piece_render_uses_piece_cell_type() {
  using namespace tetris;

  Piece l_piece = get_from_piece_type(PieceType::L);
  auto rendered = l_piece.render();
  require(count_cells(rendered, Cell::L) == 4,
          "render uses the piece cell type for occupied cells");
  require(count_cells(rendered, Cell::Empty) == 12,
          "render leaves all non-shape cells empty");
}

void test_cells_of_returns_absolute_cell_positions() {
  using namespace tetris;

  ActivePiece active{.piece = get_from_piece_type(PieceType::O),
                     .pos = Position{.row = 5, .col = 7}};
  auto cells = cells_of(active);

  require(cells.size() == 4, "cells_of returns four absolute occupied cells");
  require(cells[0].row == 6 && cells[0].col == 8,
          "cells_of offsets are translated by piece position");
  require(cells[3].row == 7 && cells[3].col == 9,
          "cells_of preserves each occupied cell location");
}

} // namespace

int main() {
  test_piece_helper_construction();
  test_piece_spawn_defaults();
  test_piece_rotation_methods();
  test_piece_current_shape_and_render();
  test_piece_render_uses_piece_cell_type();
  test_cells_of_returns_absolute_cell_positions();
  return 0;
}
