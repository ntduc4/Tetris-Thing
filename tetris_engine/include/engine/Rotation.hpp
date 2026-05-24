#pragma once

#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"

#include <vector>

namespace tetris {

/** Interface for piece wall-kick data. */
class RotationSystem {
public:
  /** Destroys a rotation system through the base interface. */
  virtual ~RotationSystem() = default;

  /** Returns the ordered kick offsets to test for a rotation transition. */
  virtual std::vector<Offset> kick_offsets(PieceType piece, Rotation from,
                                           Rotation to) const = 0;
};

/** Standard rotation system with wall-kick support. */
class SRS : public RotationSystem {
public:
  std::vector<Offset> kick_offsets(PieceType, Rotation, Rotation) const override {
    return {};
  }
};
/** Rotation system that rejects all rotations. */
class None : public RotationSystem {
public:
  std::vector<Offset> kick_offsets(PieceType, Rotation, Rotation) const override {
    return {};
  }
};

} // namespace tetris
