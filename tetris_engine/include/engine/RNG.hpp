#pragma once

#include "core/Piece.hpp"

#include <cstdint>
#include <vector>

namespace tetris {

/** Interface for deterministic or random piece generation. */
class Randomizer {
public:
  /** Destroys a randomizer through the base interface. */
  virtual ~Randomizer() = default;

  /** Resets the randomizer state with a deterministic seed. */
  virtual void seed(uint64_t seed) = 0;
  /** Returns the piece at the requested preview index without consuming it. */
  virtual PieceType peek(uint8_t index = 0) const = 0;
  /** Returns the next requested number of preview pieces. */
  virtual std::vector<PieceType> preview(uint8_t amount) const = 0;
  /** Consumes and returns the next piece in sequence. */
  virtual PieceType pop() = 0;
};

/** Classic 7-bag generator. */
class SevenBag : public Randomizer {};
/** Extended 14-bag generator. */
class FourteenBag : public Randomizer {};
/** Fully random piece generator. */
class Random : public Randomizer {};

} // namespace tetris
