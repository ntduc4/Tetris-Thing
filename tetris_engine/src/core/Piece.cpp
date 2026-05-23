#include "core/Piece.hpp"
#include "core/Core.hpp"
#include <array>
#include <cstdint>

namespace tetris {
constexpr std::array<std::array<Offset, 4>,
                     static_cast<size_t>(Rotation::Count)>
    IPiece = {{{Offset{.row = 2, .col = 0}, Offset{.row = 2, .col = 1},
                Offset{.row = 2, .col = 2}, Offset{.row = 2, .col = 3}},
               {Offset{.row = 0, .col = 2}, Offset{.row = 1, .col = 2},
                Offset{.row = 2, .col = 2}, Offset{.row = 3, .col = 2}},
               {Offset{.row = 1, .col = 0}, Offset{.row = 1, .col = 1},
                Offset{.row = 1, .col = 2}, Offset{.row = 1, .col = 3}},
               {Offset{.row = 0, .col = 1}, Offset{.row = 1, .col = 1},
                Offset{.row = 2, .col = 1}, Offset{.row = 3, .col = 1}}}};

constexpr std::array<std::array<Offset, 4>,
                     static_cast<size_t>(Rotation::Count)>
    TPiece = {{{Offset{.row = 2, .col = 0}, Offset{.row = 2, .col = 1},
                Offset{.row = 3, .col = 1}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 1, .col = 1}, Offset{.row = 2, .col = 1},
                Offset{.row = 3, .col = 1}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 2, .col = 0}, Offset{.row = 2, .col = 1},
                Offset{.row = 1, .col = 1}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 2, .col = 0}, Offset{.row = 2, .col = 1},
                Offset{.row = 3, .col = 1}, Offset{.row = 1, .col = 1}}}};

constexpr std::array<std::array<Offset, 4>,
                     static_cast<size_t>(Rotation::Count)>
    OPiece = {{{Offset{.row = 1, .col = 1}, Offset{.row = 2, .col = 1},
                Offset{.row = 1, .col = 2}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 1, .col = 1}, Offset{.row = 2, .col = 1},
                Offset{.row = 1, .col = 2}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 1, .col = 1}, Offset{.row = 2, .col = 1},
                Offset{.row = 1, .col = 2}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 1, .col = 1}, Offset{.row = 2, .col = 1},
                Offset{.row = 1, .col = 2}, Offset{.row = 2, .col = 2}}}};

constexpr std::array<std::array<Offset, 4>,
                     static_cast<size_t>(Rotation::Count)>
    SPiece = {{{Offset{.row = 2, .col = 0}, Offset{.row = 3, .col = 1},
                Offset{.row = 2, .col = 1}, Offset{.row = 3, .col = 2}},
               {Offset{.row = 3, .col = 1}, Offset{.row = 2, .col = 2},
                Offset{.row = 2, .col = 1}, Offset{.row = 1, .col = 2}},
               {Offset{.row = 1, .col = 0}, Offset{.row = 2, .col = 1},
                Offset{.row = 1, .col = 1}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 3, .col = 0}, Offset{.row = 2, .col = 1},
                Offset{.row = 2, .col = 0}, Offset{.row = 1, .col = 1}}}};

constexpr std::array<std::array<Offset, 4>,
                     static_cast<size_t>(Rotation::Count)>
    ZPiece = {{{Offset{.row = 3, .col = 0}, Offset{.row = 3, .col = 1},
                Offset{.row = 2, .col = 1}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 3, .col = 2}, Offset{.row = 2, .col = 2},
                Offset{.row = 2, .col = 1}, Offset{.row = 1, .col = 1}},
               {Offset{.row = 2, .col = 0}, Offset{.row = 2, .col = 1},
                Offset{.row = 1, .col = 1}, Offset{.row = 1, .col = 2}},
               {Offset{.row = 3, .col = 1}, Offset{.row = 2, .col = 1},
                Offset{.row = 2, .col = 0}, Offset{.row = 1, .col = 0}}}};

constexpr std::array<std::array<Offset, 4>,
                     static_cast<size_t>(Rotation::Count)>
    LPiece = {{{Offset{.row = 3, .col = 2}, Offset{.row = 2, .col = 0},
                Offset{.row = 2, .col = 1}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 1, .col = 2}, Offset{.row = 3, .col = 1},
                Offset{.row = 2, .col = 1}, Offset{.row = 1, .col = 1}},
               {Offset{.row = 1, .col = 0}, Offset{.row = 2, .col = 0},
                Offset{.row = 2, .col = 1}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 3, .col = 0}, Offset{.row = 3, .col = 1},
                Offset{.row = 2, .col = 1}, Offset{.row = 1, .col = 1}}}};

constexpr std::array<std::array<Offset, 4>,
                     static_cast<size_t>(Rotation::Count)>
    JPiece = {{{Offset{.row = 3, .col = 0}, Offset{.row = 2, .col = 0},
                Offset{.row = 2, .col = 1}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 3, .col = 2}, Offset{.row = 3, .col = 1},
                Offset{.row = 2, .col = 1}, Offset{.row = 1, .col = 1}},
               {Offset{.row = 1, .col = 2}, Offset{.row = 2, .col = 0},
                Offset{.row = 2, .col = 1}, Offset{.row = 2, .col = 2}},
               {Offset{.row = 1, .col = 0}, Offset{.row = 3, .col = 1},
                Offset{.row = 2, .col = 1}, Offset{.row = 1, .col = 1}}}};

std::vector<Offset> Piece::current_shape() const {
  std::array<std::array<Offset, 4>, static_cast<size_t>(Rotation::Count)>
      curShape;

  switch (_type) {
  case PieceType::I:
    curShape = IPiece;
    break;
  case PieceType::T:
    curShape = TPiece;
    break;
  case PieceType::O:
    curShape = OPiece;
    break;
  case PieceType::J:
    curShape = JPiece;
    break;
  case PieceType::L:
    curShape = LPiece;
    break;
  case PieceType::S:
    curShape = SPiece;
    break;
  case PieceType::Z:
    curShape = ZPiece;
    break;
  }

  return std::vector<Offset>(curShape[static_cast<size_t>(_rotation)].begin(),
                             curShape[static_cast<size_t>(_rotation)].end());
}

std::array<std::array<Cell, PieceDimension>, PieceDimension>
Piece::render() const {
  std::array<std::array<Cell, PieceDimension>, PieceDimension> res{};
  for (auto &row : res)
    row.fill(Cell::Empty);

  for (Offset offset : current_shape()) {
    if (offset.row < PieceDimension && offset.row >= 0 &&
        offset.col < PieceDimension && offset.col >= 0)
      res[offset.row][offset.col] = _cell_type;
  }

  return res;
}

Cell get_cell_from_piece_type(PieceType type) {
  switch (type) {
  case PieceType::I:
    return Cell::I;
  case PieceType::T:
    return Cell::T;
  case PieceType::O:
    return Cell::O;
  case PieceType::J:
    return Cell::J;
  case PieceType::L:
    return Cell::L;
  case PieceType::S:
    return Cell::S;
  case PieceType::Z:
    return Cell::Z;
  default:
    return Cell::Empty;
  }
}

Piece get_from_piece_type(PieceType type) {
  std::array<std::array<Offset, 4>, static_cast<size_t>(Rotation::Count)>
      curShape;

  switch (type) {
  case PieceType::I:
    curShape = IPiece;
    break;
  case PieceType::T:
    curShape = TPiece;
    break;
  case PieceType::O:
    curShape = OPiece;
    break;
  case PieceType::J:
    curShape = JPiece;
    break;
  case PieceType::L:
    curShape = LPiece;
    break;
  case PieceType::S:
    curShape = SPiece;
    break;
  case PieceType::Z:
    curShape = ZPiece;
    break;
  }

  return Piece(type, get_cell_from_piece_type(type), DefaultRotation);
}

ActivePiece spawn_from_piece_type(PieceType type) {
  return {.piece = get_from_piece_type(type),
          .pos = Position{.row = DefaultSpawnRowOffset,
                          .col = DefaultSpawnColOffset}};
}

ActivePiece spawn_from_piece_type(PieceType type, uint16_t row_offset,
                                  uint16_t col_offset) {
  return {.piece = get_from_piece_type(type),
          .pos = Position{.row = row_offset, .col = col_offset}};
}

std::vector<Offset> cells_of(const ActivePiece &piece) {
  std::vector<Offset> cells;
  cells.reserve(4);
  for (Offset offset : piece.piece.current_shape()) {
    cells.push_back(
        Offset{.row = static_cast<int8_t>(piece.pos.row + offset.row),
               .col = static_cast<int8_t>(piece.pos.col + offset.col)});
  }
  return cells;
}

} // namespace tetris
