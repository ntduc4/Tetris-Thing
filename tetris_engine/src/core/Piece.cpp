#include "core/Piece.hpp"
#include "core/Core.hpp"
#include <array>

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

Piece::Piece(PieceType type, Cell cellType, Rotation initialRotation)
    : _type{type}, _cell_type{cellType}, _rotation{initialRotation} {}

const PieceType Piece::type() const { return _type; }

void Piece::rotate_to(Rotation new_rotation) { _rotation = new_rotation; }

Rotation Piece::rotation() const { return _rotation; }

std::vector<Offset> Piece::current_shape() const {
  std::array<std::array<Offset, 4>, static_cast<size_t>(Rotation::Count)>
      curShape;

  switch (_type) {
  case tetris::PieceType::I:
    curShape = IPiece;
    break;
  case tetris::PieceType::T:
    curShape = TPiece;
    break;
  case tetris::PieceType::O:
    curShape = OPiece;
    break;
  case tetris::PieceType::J:
    curShape = JPiece;
    break;
  case tetris::PieceType::L:
    curShape = LPiece;
    break;
  case tetris::PieceType::S:
    curShape = SPiece;
    break;
  case tetris::PieceType::Z:
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

} // namespace tetris
