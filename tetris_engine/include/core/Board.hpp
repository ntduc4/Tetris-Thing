#pragma once
#include "Core.hpp"
#include <cstdint>
#include <vector>

namespace tetris {
class Board {
public:
  Board();
  Board(uint16_t width, uint16_t height);

  Cell get(uint16_t row, uint16_t col) const;
  void set(uint16_t row, uint16_t col, Cell cell);

  bool occupied(uint16_t row, uint16_t col) const;

  uint16_t clear_lines();

  uint16_t column_height(uint16_t col) const;
  uint16_t aggregate_height() const;
  uint16_t max_height() const;

  bool topout_zone_blocked() const;

  void addGarbage(uint16_t freeCol);

  std::vector<std::vector<Cell>> render() const;

private:
  const uint16_t _width, _height;
  std::vector<Cell> _cells;
  bool exceed_max_height;
};
} // namespace tetris
