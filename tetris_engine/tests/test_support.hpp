#pragma once

#include "core/Core.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

inline void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << '\n';
    std::exit(1);
  }
}

inline int count_cells(
    const std::array<std::array<tetris::Cell, tetris::PieceDimension>,
                     tetris::PieceDimension> &rendered,
    tetris::Cell cell) {
  int count = 0;
  for (const auto &row : rendered)
    for (tetris::Cell current : row)
      if (current == cell)
        ++count;
  return count;
}

void run_board_tests();
void run_piece_tests();
