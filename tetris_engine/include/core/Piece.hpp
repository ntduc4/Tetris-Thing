#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "./Core.hpp"

namespace tetris {

/** Tetromino piece identity. */
enum class PieceType : uint8_t { I, O, T, S, Z, J, L };

/** Immutable piece identity plus mutable rotation state. */
class Piece {
public:
  /**
   * Creates a piece with the requested type, cell value, and initial rotation.
   *
   * @param type Tetromino identity.
   * @param cellType Cell value used when rendering or locking this piece.
   * @param initialRotation Initial orientation.
   */
  Piece(PieceType type, Cell cellType, Rotation initialRotation)
      : _type{type}, _cell_type{cellType}, _rotation{initialRotation} {};
  /** Returns this piece's tetromino type. */
  const PieceType type() const { return _type; };
  /** Returns the board cell value associated with this piece. */
  const Cell cell_type() const { return _cell_type; };
  /** Sets the piece orientation directly. */
  void rotate_to(Rotation new_rotation) { _rotation = new_rotation; };
  /** Returns the current orientation. */
  Rotation rotation() const { return _rotation; };

  /** Returns the occupied cells in the piece's local 4x4 coordinate space. */
  std::vector<Offset> current_shape() const;

  /** Renders the piece into a 4x4 cell grid. */
  std::array<std::array<Cell, PieceDimension>, PieceDimension> render() const;

  /** Rotates the piece one step clockwise. */
  inline void rotate_cw() {
    _rotation = (static_cast<Rotation>((static_cast<uint8_t>(_rotation) + 1) %
                                       static_cast<uint8_t>(Rotation::Count)));
  };
  /** Rotates the piece one step counterclockwise. */
  inline void rotate_ccw() {
    uint8_t c = static_cast<uint8_t>(Rotation::Count);
    _rotation =
        (static_cast<Rotation>((static_cast<uint8_t>(_rotation) + c - 1) % c));
  };
  /** Rotates the piece by 180 degrees. */
  inline void rotate_180() {
    _rotation = static_cast<Rotation>((static_cast<uint8_t>(_rotation) + 2) %
                                      static_cast<uint8_t>(Rotation::Count));
  }

  bool operator==(const Piece &p) const = default;

private:
  Rotation _rotation;
  PieceType _type;
  Cell _cell_type;
};

/** A falling piece and its board-space root position. */
struct ActivePiece {
  Piece piece;
  Position pos;

  bool operator==(const ActivePiece &p) const = default;
};

/**
 * Returns the absolute board cells occupied by an active piece.
 *
 * @param piece Active piece to expand.
 * @return Four occupied board-space offsets.
 */
std::vector<Offset> cells_of(const ActivePiece &piece);

/** Creates a default piece from a tetromino type. */
Piece get_from_piece_type(PieceType type);
/** Spawns a piece at the default spawn position. */
ActivePiece spawn_from_piece_type(PieceType type);
/** Spawns a piece at the supplied root position. */
ActivePiece spawn_from_piece_type(PieceType type, int16_t row_offset,
                                  int16_t col_offset);
} // namespace tetris
