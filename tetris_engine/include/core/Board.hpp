#pragma once
#include "Core.hpp"
#include "core/Piece.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace tetris {
/** Board storage and collision model for a Tetris playfield. */
class Board {
public:
  /** Creates a board with the standard dimensions. */
  Board();
  /** Creates a board with custom dimensions. */
  Board(uint16_t width, uint16_t height);

  /** Returns the board width in cells. */
  uint16_t get_width() const;
  /** Returns the board height in cells. */
  uint16_t get_height() const;

  /** Returns the cell at a position, or Cell::Empty when out of bounds. */
  Cell get(uint16_t row, uint16_t col) const;
  /** Sets a cell when the position is inside the board. */
  void set(uint16_t row, uint16_t col, Cell cell);

  /** Returns whether a position is occupied or outside the board. */
  bool occupied(uint16_t row, uint16_t col) const;

  /** Clears full clearable lines and returns the number cleared. */
  uint16_t clear_lines();

  /** Returns the stack height of a column. */
  uint16_t column_height(uint16_t col) const;
  /** Returns the sum of all column heights. */
  uint16_t aggregate_height() const;
  /** Returns the highest occupied row plus one. */
  uint16_t max_height() const;
  /** Returns whether garbage pushed the stack to the board ceiling. */
  inline bool touch_max_height() const { return _touch_max_height; }

  /** Returns whether an active piece overlaps walls, floor, ceiling, or cells. */
  bool collide(const ActivePiece &piece) const;

  /** Returns whether a valid active piece cannot move one row lower. */
  bool grounded(const ActivePiece &piece) const;

  /** Inserts garbage rows from the bottom, preserving the requested hole column. */
  void add_garbage(uint16_t freeCol, uint16_t line_count);

  /** Returns a row-major copy of the current board cells. */
  std::vector<std::vector<Cell>> render() const;

  /** Clears all cells and resets top-out tracking. */
  void reset();

private:
  const uint16_t _width, _height;
  std::vector<Cell> _cells;
  bool _touch_max_height = false;

  /** Converts an in-bounds coordinate to a flat vector index. */
  inline size_t c2i(uint16_t row, uint16_t col) const {
    return static_cast<size_t>(row) * _width + col;
  };
  /** Converts a coordinate to an index, returning nullopt when out of bounds. */
  inline std::optional<size_t> cc2i(uint16_t row, uint16_t col) const {
    if (row >= this->_height || col >= this->_width)
      return std::nullopt;
    return c2i(row, col);
  };
};
} // namespace tetris
