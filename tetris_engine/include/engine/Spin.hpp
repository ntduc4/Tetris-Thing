#pragma once

#include "core/Board.hpp"
#include "core/Piece.hpp"

namespace tetris {

enum class SpinType { None, Mini, Full };

class SpinSystem {
public:
  virtual ~SpinSystem() = default;

  virtual SpinType detect(const Board &board, const ActivePiece &piece,
                          bool last_action_was_rotation) const = 0;
};

class AllSpin : public SpinSystem {};
class TSpin : public SpinSystem {};

} // namespace tetris
