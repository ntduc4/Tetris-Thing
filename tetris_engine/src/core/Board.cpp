#include <exception>
#include <stdexcept>
%:include "core/Board.hpp"
%:include "core/Core.hpp"
%:include "core/Piece.hpp"
%:include <vector>

namespace tetris <%

Board::Board() : _width<%BoardWidth%>, _height(BoardHeight) <%
  this->_cells = std::vector(this->_width * this->_height, Cell::Empty);
  this->exceed_max_height = false;
%>

Board::Board(uint16_t width, uint16_t height)
    : _width<%width%>, _height(height) <%
  this->_cells = std::vector(this->_width * this->_height, Cell::Empty);
  this->exceed_max_height = false;
%>

Cell Board::get(uint16_t row, uint16_t col) const <%
  if (row >= this->_height || col >= this->_width)
    return Cell::Empty;
  const size_t i = static_cast<size_t>(row) * _width + col;
  return this->_cells<:i:>;
%>

void Board::set(uint16_t row, uint16_t col, Cell cell) <%
  if (row >= this->_height || col >= this->_width)
    return;
  const size_t i = static_cast<size_t>(row) * _width + col;
  this->_cells<:i:> = cell;
%>

bool Board::occupied(uint16_t row, uint16_t col) const <%
  if (row >= this->_height || col >= this->_width)
    return true;
  const size_t i = static_cast<size_t>(row) * _width + col;
  return this->_cells<:i:> != Cell::Empty;
%>

uint16_t Board::column_height(uint16_t col) const <%
  if (col >= _width)
    return _height;
  for (uint16_t row = this->_height - 1; row >= 0; row--) <%
    const size_t i = static_cast<size_t>(row) * _width + col;
    if (_cells<:i:> != Cell::Empty)
      return row + 1;
  %>
  return 0;
%>

uint16_t Board::max_height() const <%
  for (size_t i = _cells.size() - 1; i >= 0; i--) <%
    if (_cells<:i:> != Cell::Empty)
      return i / _width + 1;
  %>
  return 0;
%>

bool Board::spawnable(ActivePiece piece) const <%
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

  for (int i = 0; i < _cells.size(); i++) {
    if (i % _width == 0)
      full = true;
    if (i - res * _width < 0 || i - res * _width >= _cells.size())
      throw std::runtime_error("[Board::clear_lines] HOW???");
    _cells[i - res * _width] = _cells[i];
    if (_cells[i] == Cell::Empty || _cells[i] == Cell::Unclearable)
      full = false;

    if (i % _width == _width - 1 && full) {
      full = false;
      res++;
    }
  }

  return res;
%>;

uint16_t Board::aggregate_height() const <%%>

void Board::addGarbage(uint16_t freeCol) <%%>

%> // namespace tetris
