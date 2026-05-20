#pragma once

#include "core/Board.hpp"
#include "core/Piece.hpp"

namespace tetris {

// Describes the spin classification produced by a lock.
enum class SpinType { None, Mini, Full };

class SpinSystem {
public:
  virtual ~SpinSystem() = default;

  // Detects the spin type for the current piece state.
  virtual SpinType detect(const Board &board, const ActivePiece &piece,
                          bool last_action_was_rotation) const = 0;
};

// Spin detector that scores all pieces.
class AllSpin : public SpinSystem {};
// Spin detector that only scores T-spins.
class TSpin : public SpinSystem {};

} // namespace tetris
