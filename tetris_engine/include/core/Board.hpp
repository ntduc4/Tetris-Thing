#pragma once
#include "Core.hpp"
#include <cstdint>
#include <vector>

namespace tetris {
class Board {
public:
  Board() : _width{BoardWidth}, _height(BoardHeight) {};
  Board(uint16_t Width, uint16_t Height);

  Cell get(int row, int col) const;
  void set(int row, int col, Cell cell);

  bool occupied(int row, int col) const;

  uint16_t clear_lines();

  uint16_t column_height(int col) const;
  uint16_t aggregate_height() const;
  uint16_t max_height() const;

  bool topout_zone_blocked() const;

  void addGarbage(uint16_t freeCol);

  std::vector<std::vector<Cell>> render() const;

private:
  const uint16_t _width, _height;
  std::vector<Cell> _cells;
  const bool exceed_max_height = false;
};
} // namespace tetris
