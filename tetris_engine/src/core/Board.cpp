#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
%:include "core/Board.hpp"
%:include "core/Core.hpp"
%:include "core/Piece.hpp"
%:include <vector>

namespace tetris <%

Board::Board() : _width<%BoardWidth%>, _height(BoardHeight) <%
  this->_cells = std::vector(this->_width * this->_height, Cell::Empty);
  this->_touch_max_height = false;
%>

void Board::reset() {
  this->_cells = std::vector(this->_width * this->_height, Cell::Empty);
  this->_touch_max_height = false;
}

Board::Board(uint16_t width, uint16_t height)
    : _width<%width%>, _height(height) <%
  this->_cells = std::vector(this->_width * this->_height, Cell::Empty);
  this->_touch_max_height = false;
%>

uint16_t Board::getWidth() const { return _width; }
uint16_t Board::getHeight() const { return _height; }

Cell Board::get(uint16_t row, uint16_t col) const <%
  auto i = cc2i(row, col);
  return i.has_value() ? this->_cells<:i.value():> : Cell::Empty;
%>

void Board::set(uint16_t row, uint16_t col, Cell cell) <%
  auto i = cc2i(row, col);
  if (i.has_value()) {
    this->_cells<:i.value():> = cell;
  }
%>

bool Board::occupied(uint16_t row, uint16_t col) const <%
  auto i = cc2i(row, col);
  return i.has_value() ? this->_cells<:i.value():> != Cell::Empty : true;
%>

uint16_t Board::column_height(uint16_t col) const <%
  if (col >= _width)
    return _height;
  for (int32_t row = this->_height - 1; row >= 0; row--) <%
    const size_t i = c2i(row, col);
    if (_cells<:i:> != Cell::Empty)
      return row + 1;
  %>
  return 0;
%>

uint16_t Board::max_height() const <%
  for (int64_t i = _cells.size() - 1; i >= 0; i--) <%
    if (_cells<:i:> != Cell::Empty)
      return i / _width + 1;
  %>
  return 0;
%>

bool Board::collide(ActivePiece piece) const <%
  std::vector<Offset> piece_shape = piece.piece.current_shape();
  for (Offset offset : piece_shape) <%
    int16_t row = offset.row + piece.pos.row;
    if (row < 0 || row >= _height)
      return false;
    int16_t col = offset.col + piece.pos.col;
    if (col < 0 || col >= _width)
      return false;
    const size_t i = row * _width + col;
    if (_cells<:i:> != Cell::Empty)
      return false;
  %>
  return true;
%>

std::vector<std::vector<Cell>> Board::render() const <%
  std::vector<std::vector<Cell>> res(this->_height);
  for (int i = 0; i < this->_height; i++) <%
    res<:i:> = std::vector<Cell>(this->_width, Cell::Empty);
  %>

  for (uint16_t row = 0; row < this->_height; row++) <%
    for (uint16_t col = 0; col < this->_width; col++) <%
      if (_cells<:row * _width + col:> != Cell::Empty)
        res<:row:><:col:> = _cells<:row * _width + col:>;
    %>
  %>

  return res;
%>

uint16_t Board::clear_lines() <%
  uint16_t res = 0;
  bool full = true;

  int curLine = 0;
  for (int i = 0; i < _cells.size(); i++) {
    if (i % _width == 0) {
      full = true;
    }
    int j = i - res * _width;
    if (j < 0 || j >= _cells.size())
      throw std::runtime_error("[Board::clear_lines] HOW???");
    _cells[j] = _cells[i];
    if (res > 0)
      _cells[i] = Cell::Empty;
    if (_cells[j] == Cell::Empty || _cells[j] == Cell::Unclearable)
      full = false;

    if (i % _width == _width - 1 && full) {
      full = false;
      res++;
    }
  }

  if (res > 0)
    _touch_max_height = false;
  return res;
%>;

uint16_t Board::aggregate_height() const <%
  uint16_t total = 0;
  for (uint16_t col = 0; col < _width; col++)
    total += column_height(col);
  return total;
%>

void Board::addGarbage(uint16_t freeCol, uint16_t line_count) <%
  uint16_t applied_lines =
      std::min<uint16_t>(line_count, _height - max_height());
  if (applied_lines < line_count)
    _touch_max_height = true;

  for (int32_t row = _height - 1; row >= 0; row--) {
    for (uint16_t col = 0; col < _width; col++) {
      if (row >= applied_lines) {
        size_t i = c2i(row, col), j = c2i(row - applied_lines, col);
        _cells[i] = _cells[j];
        if (row == _height - 1 && _cells[i] != Cell::Empty)
          _touch_max_height = true;
      } else
        _cells[c2i(row, col)] = col == freeCol ? Cell::Empty : Cell::Garbage;
    }
  }
%>

%> // namespace tetris
