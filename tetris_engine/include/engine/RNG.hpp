#pragma once

#include "core/Piece.hpp"

#include <cstdint>
#include <vector>

namespace tetris {

class Randomizer {
public:
  virtual ~Randomizer() = default;

  virtual void seed(uint64_t seed) = 0;
  virtual PieceType peek(uint8_t index = 0) const = 0;
  virtual std::vector<PieceType> preview(uint8_t amount) const = 0;
  virtual PieceType pop() = 0;
};

class SevenBag : public Randomizer {};
class FourteenBag : public Randomizer {};
class Random : public Randomizer {};

} // namespace tetris
