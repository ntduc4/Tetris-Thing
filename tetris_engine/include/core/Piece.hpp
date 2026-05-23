#pragma once

#include <array>
#include <vector>

#include "./Core.hpp"

namespace tetris {

enum class PieceType { I, O, T, S, Z, J, L };

class Piece {
public:
  Piece(PieceType type, Cell cellType, Rotation initialRotation)
      : _type{type}, _cell_type{cellType}, _rotation{initialRotation} {};
  const PieceType type() const { return _type; };
  const Cell cell_type() const { return _cell_type; };
  void rotate_to(Rotation new_rotation) { _rotation = new_rotation; };
  Rotation rotation() const { return _rotation; };

  std::vector<Offset> current_shape() const;

  std::array<std::array<Cell, PieceDimension>, PieceDimension> render() const;

  inline void rotate_cw() {
    _rotation = (static_cast<Rotation>((static_cast<uint8_t>(_rotation) + 1) %
                                       static_cast<uint8_t>(Rotation::Count)));
  };
  inline void rotate_ccw() {
    uint8_t c = static_cast<uint8_t>(Rotation::Count);
    _rotation =
        (static_cast<Rotation>((static_cast<uint8_t>(_rotation) + c - 1) % c));
  };
  inline void rotate_180() {
    _rotation = static_cast<Rotation>((static_cast<uint8_t>(_rotation) + 2) %
                                      static_cast<uint8_t>(Rotation::Count));
  }

private:
  Rotation _rotation;
  const PieceType _type;
  const Cell _cell_type;
};

struct ActivePiece {
  Piece piece;
  Position pos;
};

std::vector<Offset> cells_of(const ActivePiece &piece);

// ========== Helpers ==========
Piece get_from_piece_type(PieceType type);
ActivePiece spawn_from_piece_type(PieceType type);
ActivePiece spawn_from_piece_type(PieceType type, int16_t row_offset,
                                  int16_t col_offset);
} // namespace tetris
