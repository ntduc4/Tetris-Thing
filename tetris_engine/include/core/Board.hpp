#pragma once
#include "Core.hpp"
#include "core/Piece.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace tetris {
class Board {
public:
  Board();
  Board(uint16_t width, uint16_t height);

  uint16_t get_width() const;
  uint16_t get_height() const;

  Cell get(uint16_t row, uint16_t col) const;
  void set(uint16_t row, uint16_t col, Cell cell);

  bool occupied(uint16_t row, uint16_t col) const;

  uint16_t clear_lines();

  uint16_t column_height(uint16_t col) const;
  uint16_t aggregate_height() const;
  uint16_t max_height() const;
  inline bool touch_max_height() const { return _touch_max_height; }

  bool collide(ActivePiece piece) const;

  void add_garbage(uint16_t freeCol, uint16_t line_count);

  std::vector<std::vector<Cell>> render() const;

  void reset();

private:
  const uint16_t _width, _height;
  std::vector<Cell> _cells;
  bool _touch_max_height = false;

  // coordinate-to-index
  inline size_t c2i(uint16_t row, uint16_t col) const {
    return static_cast<size_t>(row) * _width + col;
  };
  // (bounded-)checked-coordinate-to-index
  inline std::optional<size_t> cc2i(uint16_t row, uint16_t col) const {
    if (row >= this->_height || col >= this->_width)
      return std::nullopt;
    return c2i(row, col);
  };
};
} // namespace tetris
