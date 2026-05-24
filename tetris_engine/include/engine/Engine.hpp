#pragma once

#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include "engine/Attack.hpp"
#include "engine/Movement.hpp"
#include "engine/RNG.hpp"
#include "engine/Rotation.hpp"
#include "engine/Score.hpp"
#include "engine/Spin.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

namespace tetris {

/** Represents an incoming garbage packet. */
struct GarbageEvent {
  uint16_t lines = 0;
  uint16_t hole_col = 0;
};

/** Tracks garbage that has been received but not yet applied to the board. */
struct PendingGarbage {
  GarbageEvent event;
  uint16_t delay_ticks = 0;
};

/** Describes the outcome of a player-issued action. */
struct StepResult {
  bool moved = false;
  bool locked = false;
  bool spawned = false;
  bool held = false;
  bool game_over = false;
  uint16_t lines_cleared = 0;
};

/** Describes the outcome of a gravity tick. */
struct TickResult {
  bool moved_down = false;
  bool locked = false;
  bool game_over = false;
};

/** Describes the outcome of locking the active piece. */
struct LockResult {
  bool game_over = false;
  uint16_t lines_cleared = 0;
  bool perfect_clear = false;
  SpinType spin = SpinType::None;
  uint16_t attack_sent = 0;
};

/**
 * Single-player Tetris engine runtime.
 *
 * Engine owns board state, active/held pieces, queue preview state, scoring
 * state, garbage state, and injected rule systems.
 */
class Engine {
public:
  /**
   * Creates a single-player engine with pluggable rules and services.
   *
   * @param board Initial board state.
   * @param rotation_system Rotation and kick implementation.
   * @param randomizer Piece source implementation.
   * @param spin_system Optional spin classifier.
   * @param score_system Optional scoring rules.
   * @param attack_scheme Optional garbage attack rules.
   * @param preview_count Number of preview pieces exposed by preview_queue().
   * @param spawn_col_offset Root column used when spawning pieces.
   * @param spawn_row_offset Root row used when spawning pieces.
   */
  Engine(Board board, std::unique_ptr<RotationSystem> rotation_system,
         std::unique_ptr<Randomizer> randomizer,
         std::unique_ptr<SpinSystem> spin_system = nullptr,
         std::unique_ptr<ScoreSystem> score_system = nullptr,
         std::unique_ptr<AttackScheme> attack_scheme = nullptr,
         uint8_t preview_count = DefaultQueueLength,
         int16_t spawn_col_offset = DefaultSpawnColOffset,
         int16_t spawn_row_offset = DefaultSpawnRowOffset)
      : _board{board}, _rotation_system{std::move(rotation_system)},
        _randomizer{std::move(randomizer)},
        _spin_system{std::move(spin_system)},
        _score_system{std::move(score_system)},
        _attack_scheme{std::move(attack_scheme)}, _preview_count{preview_count},
        _spawn_col_offset{spawn_col_offset},
        _spawn_row_offset{spawn_row_offset} {};

  /** Resets engine state and reseeds the piece source. */
  void reset(uint64_t seed = 0);

  /** Returns the current board state. */
  const Board &board() const { return _board; }
  /** Returns the currently active falling piece. */
  const std::optional<ActivePiece> &active_piece() const {
    return _active_piece;
  }
  /** Returns the held piece if one exists. */
  std::optional<PieceType> hold_piece() const { return _hold_piece; }
  /** Returns the visible preview queue. */
  std::vector<PieceType> preview_queue() const;
  /** Renders the current board state. */
  std::vector<std::vector<Cell>> render() const;

  /** Returns whether the engine has reached game over. */
  bool game_over() const { return _game_over; }
  /** Returns whether hold is currently available. */
  bool can_hold() const { return _can_hold; }
  /** Returns the total cleared lines tracked by the engine. */
  uint32_t lines_cleared() const { return _lines_cleared; }
  /** Returns the accumulated score. */
  uint64_t score() const { return _score; }
  /** Returns the current combo counter. */
  uint32_t combo() const { return _combo; }
  /** Returns whether back-to-back is currently active. */
  bool back_to_back() const { return _back_to_back; }

  /**
   * Spawns the next piece from the randomizer.
   *
   * @param clutch_clear Whether to search for a shifted valid spawn position
   * when the default spawn position is blocked.
   * @return true when a piece was spawned and consumed from the randomizer.
   */
  bool spawn_next_piece(bool clutch_clear = false);
  /**
   * Swaps the active piece with the hold slot.
   *
   * @param ignore_hold Whether to bypass the current can_hold() gate.
   * @return true when the hold operation succeeds.
   */
  bool hold(bool ignore_hold = false);

  /** Attempts to move the active piece left by up to amount columns. */
  bool try_move_left(int16_t amount = 1);
  /** Attempts to move the active piece right by up to amount columns. */
  bool try_move_right(int16_t amount = 1);
  /** Attempts to move the active piece down by up to amount rows. */
  bool try_soft_drop(int16_t amount = 1);
  /** Attempts to rotate the active piece clockwise. */
  bool try_rotate_cw();
  /** Attempts to rotate the active piece counterclockwise. */
  bool try_rotate_ccw();
  /** Attempts to rotate the active piece by 180 degrees. */
  bool try_rotate_180();

  /** Returns the ghost placement for the active piece. */
  std::optional<ActivePiece> ghost_piece() const;
  /** Returns how far the active piece can hard drop. */
  uint16_t hard_drop_distance() const;

  /** Applies a high-level player action. */
  StepResult step(Action action);
  /** Advances the game by one gravity tick. */
  TickResult tick();

  /** Hard drops, locks, and resolves the active piece. */
  LockResult hard_drop();
  /** Locks the current active piece and resolves resulting state. */
  LockResult lock_active_piece();

  /** Adds incoming garbage with the requested hole column. */
  void receive_garbage(uint16_t lines, uint16_t hole_col);
  /** Adds an incoming garbage packet. */
  void receive_garbage(const GarbageEvent &garbage);

  /** Generates all possible placements for the active piece. */
  std::vector<Placement> generate_active_piece_placement() const;
  /** Generates all possible placements for the held piece. */
  std::vector<Placement> generate_hold_piece_placement() const;

private:
  Board _board;
  std::optional<ActivePiece> _active_piece;
  std::optional<PieceType> _hold_piece;
  bool _game_over = false;
  bool _can_hold = true;
  uint32_t _lines_cleared = 0;
  uint64_t _score = 0;
  uint32_t _combo = 0;
  bool _back_to_back = false;
  int16_t _spawn_col_offset = 0;
  int16_t _spawn_row_offset = 0;

  /** Tick-driven runtime state. */
  uint64_t _tick_count = 0;
  uint32_t _gravity_progress = 0;
  bool _grounded_last_tick = false;
  uint16_t _lock_timer = 0;
  uint16_t _lock_resets_used = 0;
  bool _last_tick_was_player_action = false;

  /** Spin and placement tracking for lock resolution. */
  SpinContext _spin_context;
  std::optional<Placement> _last_successful_placement;

  /** Pending garbage state processed by tick progression. */
  std::deque<PendingGarbage> _pending_garbage;
  uint16_t _garbage_queue_delay = 0;

  std::unique_ptr<RotationSystem> _rotation_system;
  std::unique_ptr<Randomizer> _randomizer;
  std::unique_ptr<SpinSystem> _spin_system;
  std::unique_ptr<ScoreSystem> _score_system;
  std::unique_ptr<AttackScheme> _attack_scheme;

  uint8_t _preview_count;
};

} // namespace tetris
