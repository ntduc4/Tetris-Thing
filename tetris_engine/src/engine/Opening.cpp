#include "engine/Engine.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <utility>

namespace tetris {
namespace {

PieceType mirrored_piece(PieceType piece) {
  switch (piece) {
  case PieceType::S: return PieceType::Z;
  case PieceType::Z: return PieceType::S;
  case PieceType::J: return PieceType::L;
  case PieceType::L: return PieceType::J;
  default: return piece;
  }
}

Cell mirrored_cell(Cell cell) {
  switch (cell) {
  case Cell::S: return Cell::Z;
  case Cell::Z: return Cell::S;
  case Cell::J: return Cell::L;
  case Cell::L: return Cell::J;
  default: return cell;
  }
}

} // namespace

Engine::OpeningTransform Engine::generate_opening(
    const std::vector<std::vector<Cell>> &pattern,
    const std::vector<bool> &filled_rows,
    uint16_t clean_garbage_layers, uint16_t cheese_garbage_layers,
    uint64_t seed, const std::vector<PieceType> &pieces,
    std::optional<PieceType> hold, bool allow_mirror) {
  if (_active_piece || _game_over || _last_lock_result || _tick_count ||
      _score || _edit_mode || _board.max_height() || !_opening_pieces.empty() ||
      _hold_piece)
    throw std::logic_error("opening must be generated after reset, before spawning");

  const size_t width = _board.get_width();
  const size_t rows = pattern.size();
  const size_t pattern_width = rows ? pattern.front().size() : 0;
  if (filled_rows.size() != rows || pattern_width > width ||
      (rows && !pattern_width) ||
      !std::all_of(pattern.begin(), pattern.end(), [=](const auto &row) {
        return row.size() == pattern_width &&
               std::all_of(row.begin(), row.end(), [](Cell cell) {
                 return cell <= Cell::Unclearable;
               });
      }) ||
      rows + clean_garbage_layers + cheese_garbage_layers >
          std::min<size_t>(20, _board.get_height()))
    throw std::invalid_argument("opening pattern and garbage must fit the visible board");
  if (pieces.size() > 64 || (pieces.empty() && hold) ||
      !std::all_of(pieces.begin(), pieces.end(), [](PieceType piece) {
        return piece <= PieceType::L;
      }) || (hold && *hold > PieceType::L))
    throw std::invalid_argument("opening requires valid scripted pieces and hold");

  // Independent of the 7-bag and incoming-garbage RNG streams.
  std::mt19937_64 rng(seed ^ 0xD1B54A32D192ED03ULL);
  OpeningTransform transform;
  if (rows) {
    transform.offset = std::uniform_int_distribution<uint16_t>(
        0, static_cast<uint16_t>(width - pattern_width))(rng);
    transform.mirrored = allow_mirror &&
        std::uniform_int_distribution<int>(0, 1)(rng) != 0;
  }

  Board generated(_board.get_width(), _board.get_height());
  auto garbage_row = [&](size_t row, uint16_t hole) {
    for (uint16_t col = 0; col < width; ++col)
      if (col != hole) generated.set(row, col, Cell::Garbage);
  };
  size_t row_offset = 0;
  if (clean_garbage_layers) {
    const auto hole = std::uniform_int_distribution<uint16_t>(
        0, static_cast<uint16_t>(width - 1))(rng);
    for (uint16_t row = 0; row < clean_garbage_layers; ++row)
      garbage_row(row, hole);
    row_offset += clean_garbage_layers;
  }
  for (uint16_t row = 0; row < cheese_garbage_layers; ++row)
    garbage_row(row_offset + row, std::uniform_int_distribution<uint16_t>(
        0, static_cast<uint16_t>(width - 1))(rng));
  row_offset += cheese_garbage_layers;

  for (size_t row = 0; row < rows; ++row) {
    if (filled_rows[row])
      for (uint16_t col = 0; col < width; ++col)
        generated.set(row_offset + row, col, Cell::Garbage);
    for (size_t col = 0; col < pattern_width; ++col)
      generated.set(row_offset + row,
                    transform.offset + (transform.mirrored ?
                        pattern_width - 1 - col : col),
                    transform.mirrored ? mirrored_cell(pattern[row][col]) :
                                         pattern[row][col]);
  }

  std::vector<PieceType> scripted = pieces;
  if (transform.mirrored)
    for (PieceType &piece : scripted) piece = mirrored_piece(piece);
  if (transform.mirrored && hold) hold = mirrored_piece(*hold);
  const PieceType first = scripted.empty() ? next_piece() : scripted.front();
  if (generated.collide(spawn_piece(first)))
    throw std::invalid_argument("opening blocks the active piece's spawn position");

  if (!scripted.empty()) set_opening_pieces(scripted, hold);
  _board = std::move(generated);
  return transform;
}

} // namespace tetris
