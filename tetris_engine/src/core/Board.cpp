#include "core/Board.hpp"
#include "core/Core.hpp"
#include <vector>

namespace tetris {

Board::Board() : _width{BoardWidth}, _height(BoardHeight) {
  this->_cells = std::vector(this->_width * this->_height, Cell::Empty);
  this->exceed_max_height = false;
}

Board::Board(uint16_t width, uint16_t height) : _width{width}, _height(height) {
  this->_cells = std::vector(this->_width * this->_height, Cell::Empty);
  this->exceed_max_height = false;
}

Cell Board::get(uint16_t row, uint16_t col) const {
  if (row >= this->_height || col >= this->_width)
    return Cell::Empty;
  const size_t i = static_cast<size_t>(row) * _width + col;
  return this->_cells[i];
}

void Board::set(uint16_t row, uint16_t col, Cell cell) {
  if (row >= this->_height || col >= this->_width)
    return;
  const size_t i = static_cast<size_t>(row) * _width + col;
  this->_cells[i] = cell;
}

bool Board::occupied(uint16_t row, uint16_t col) const {
  if (row >= this->_height || col >= this->_width)
    return true;
  const size_t i = static_cast<size_t>(row) * _width + col;
  return this->_cells[i] != Cell::Empty;
}

uint16_t Board::clear_lines() {};

uint16_t Board::column_height(uint16_t col) const {
  if (col >= _width)
    return _height;
  for (uint16_t row = this->_height - 1; row >= 0; row--) {
    const size_t i = static_cast<size_t>(row) * _width + col;
    if (_cells[i] != Cell::Empty)
      return row + 1;
  }
  return 0;
}
uint16_t Board::aggregate_height() const {}

uint16_t Board::max_height() const {
  for (size_t i = _cells.size() - 1; i >= 0; i--) {
    if (_cells[i] != Cell::Empty)
      return i / _width + 1;
  }
  return 0;
}

bool Board::topout_zone_blocked() const {}

void Board::addGarbage(uint16_t freeCol) {}

std::vector<std::vector<Cell>> Board::render() const {}

} // namespace tetris
