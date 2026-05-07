#pragma once

#include <array>
#include <vector>

#include "./Core.hpp"

namespace tetris {

enum class PieceType { I, O, T, S, Z, J, L };

class Piece {
public:
  Piece(PieceType type, Cell cellType, Rotation initialRotation);
  const PieceType type() const;
  void rotate_to(Rotation new_rotation);
  Rotation rotation() const;

  std::vector<Offset> current_shape() const;

  std::array<std::array<Cell, PieceDimension>, PieceDimension> render() const;

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
Rotation rotate_cw(Rotation r);
Rotation rotate_ccw(Rotation r);
Rotation rotate_180(Rotation r);

Piece get_from_piece_type(PieceType type);
} // namespace tetris
