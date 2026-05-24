#pragma once

#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include "engine/Movement.hpp"

#include <optional>

namespace tetris {

/** Describes the spin classification produced by a lock. */
enum class SpinType { None, Mini, Full };

/** Carries the move metadata needed to classify spins accurately. */
struct SpinContext {
  /** Final board state should be passed separately as an immutable view. */
  Movement last_movement = Movement::None;
  /** Piece state before the last successful movement. */
  std::optional<ActivePiece> previous_piece = std::nullopt;
  /** Whether the last successful rotation required a kick. */
  bool used_kick = false;
  /** Which kick test succeeded, when applicable. */
  std::optional<Offset> kick_offset = std::nullopt;
  /** Whether the current lock was reached by player input or passive gravity.
   */
  bool last_move_was_player_action = false;
};

/** Interface for classifying spin moves at lock time. */
class SpinSystem {
public:
  /** Destroys a spin system through the base interface. */
  virtual ~SpinSystem() = default;

  /** Detects the spin type for the current piece state. */
  virtual SpinType detect(const Board &board, const ActivePiece &piece,
                          const SpinContext &context) const = 0;
};

/** Spin detector that scores all pieces. */
class AllSpin : public SpinSystem {};
/** Spin detector that only scores T-spins. */
class TSpin : public SpinSystem {};

} // namespace tetris
