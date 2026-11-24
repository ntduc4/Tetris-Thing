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

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <random>
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
  /** Hole is chosen when lines actually rise (2009 Guideline matches only). */
  bool random_hole = false;
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
  uint16_t lines_cleared = 0;
  uint16_t attack_sent = 0;
  std::vector<uint16_t> attack_packets;
};

/** Describes the outcome of locking the active piece. */
struct LockResult {
  bool game_over = false;
  uint16_t lines_cleared = 0;
  bool perfect_clear = false;
  SpinType spin = SpinType::None;
  uint16_t attack_sent = 0;
  bool locked = false;
  std::vector<uint16_t> attack_packets;
  /** Score awarded by this lock, including hard/soft drop points. */
  uint64_t score_delta = 0;
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
          int16_t spawn_row_offset = DefaultSpawnRowOffset,
          double gravity = DefaultGravity,
          double gravity_increase = DefaultGravityIncrease,
          uint64_t gravity_margin_time = DefaultGravityMarginTime,
          uint64_t lock_delay = DefaultLockDelay,
          bool guideline_2009 = false)
      : _board{board}, _rotation_system{std::move(rotation_system)},
        _randomizer{std::move(randomizer)},
        _spin_system{std::move(spin_system)},
        _score_system{std::move(score_system)},
        _attack_scheme{std::move(attack_scheme)}, _preview_count{preview_count},
        _guideline_2009{guideline_2009},
        _spawn_col_offset{guideline_2009 ? int16_t{3} : spawn_col_offset},
        _spawn_row_offset{guideline_2009 ? int16_t{18} : spawn_row_offset},
        _starting_gravity{gravity},
        _gravity{gravity}, _gravity_increase{gravity_increase},
        _gravity_margin_time{gravity_margin_time}, _lock_delay{lock_delay} {
    if (guideline_2009 &&
        (board.get_width() != 10 || board.get_height() != 40 ||
         preview_count == 0 || preview_count > 6))
      throw std::invalid_argument(
          "2009 Guideline requires a 10x40 board and 1..6 previews");
    if (guideline_2009)
      set_level(1);
  }

  /** Resets engine state and reseeds the piece source. */
  void reset(uint64_t seed = 0);

  /** Independent copy of the current state, rules, RNG/bag, and queue. */
  Engine clone() const;
  /** Cheap in-place state/RNG restore from a clone with the same rule context. */
  void restore(const Engine &snapshot);

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
  /** Number of consecutive B2B bonus clears (first difficult clear is zero). */
  uint32_t back_to_back_count() const { return _back_to_back_count; }
  /** Charged surge attack for the selected attack scheme, if any. */
  uint32_t surge_charge() const {
    return _attack_scheme ? _attack_scheme->surge_charge(_back_to_back_count) : 0;
  }
  /** Number of simulation ticks since reset (60 ticks = one second in the CLI). */
  uint64_t tick_count() const { return _tick_count; }
  /** The 2009 Guideline uses 20 visible rows and a 20-row buffer zone. */
  bool guideline_2009() const { return _guideline_2009; }
  uint16_t level() const { return _level; }
  /** Set the level used by level-dependent score systems (1..15). */
  void set_level(uint16_t level);
  /** Last successful lock, if any. */
  const std::optional<LockResult> &last_lock_result() const {
    return _last_lock_result;
  }
  /** Total lines of incoming garbage still queued. */
  uint32_t pending_garbage_lines() const;
  /** Number of distinct incoming garbage packets. */
  size_t pending_garbage_packets() const { return _pending_garbage.size(); }
  /** Snapshot of queued packets in arrival order (including remaining travel time). */
  std::vector<PendingGarbage> pending_garbage() const {
    return {_pending_garbage.begin(), _pending_garbage.end()};
  }

  /** Change gravity, in cells per tick, while the game is running. */
  void set_gravity(double cells_per_tick);
  /** Change the rate at which gravity increases after the margin time. */
  void set_gravity_increase(double per_tick, uint64_t margin_tick);
  /** Change grounded lock delay and the number of move/rotation resets allowed. */
  void set_lock_delay(uint64_t ticks, uint16_t reset_limit = 15);
  /** Set the travel time before incoming garbage can rise on a non-clearing lock. */
  void set_garbage_delay(uint16_t ticks) { _garbage_queue_delay = ticks; }
  void set_spin_system(std::unique_ptr<SpinSystem> system) {
    _spin_system = std::move(system);
    _rule_context = std::make_shared<int>(0);
  }
  void set_score_system(std::unique_ptr<ScoreSystem> system) {
    _score_system = std::move(system);
    _rule_context = std::make_shared<int>(0);
  }
  void set_attack_scheme(std::unique_ptr<AttackScheme> scheme) {
    _attack_scheme = std::move(scheme);
    _rule_context = std::make_shared<int>(0);
  }

  /** Pause play to paint board cells. Exiting fails while the piece collides. */
  bool set_edit_mode(bool enabled);
  bool edit_mode() const { return _edit_mode; }
  /** Paint any cell, including garbage or empty, while editing. */
  bool paint_cell(uint16_t row, uint16_t col, Cell cell);

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
  /** Queue Guideline garbage; choose its hole only as lines rise, in groups of 8. */
  void receive_random_garbage(uint16_t lines);

  /** Reachable hard-drop placements without hold; actions execute before any tick. */
  std::vector<Placement> generate_active_piece_placement() const;
  /** Reachable hard-drop placements after a legal hold. */
  std::vector<Placement> generate_hold_piece_placement() const;
  /** Atomically replay and lock a validated placement; does not spawn next piece. */
  LockResult apply_placement(const Placement &placement);

private:
  // When adding runtime state, update clone() and restore() together.
  Board _board;
  std::optional<ActivePiece> _active_piece;
  std::optional<PieceType> _hold_piece;
  bool _game_over = false;
  bool _can_hold = true;
  uint32_t _lines_cleared = 0;
  uint64_t _score = 0;
  uint32_t _combo = 0;
  bool _back_to_back = false;
  uint32_t _back_to_back_count = 0;
  uint16_t _level = 1;
  bool _guideline_2009 = false;
  bool _has_landed = false;
  int16_t _spawn_col_offset = 0;
  int16_t _spawn_row_offset = 0;
  int16_t _lowest_row_reached = 0;

  /** Tick-driven runtime state. */
  uint64_t _tick_count = 0;
  uint32_t _gravity_progress = 0;
  bool _grounded_last_tick = false;
  uint16_t _lock_timer = 0;
  uint16_t _lock_resets_used = 0;
  uint16_t _lock_reset_limit = 15;
  bool _last_tick_was_player_action = false;
  double _starting_gravity = 0.0;
  double _gravity = 0.0;
  double _gravity_counter = 0.0;
  double _gravity_increase = 0.0;
  uint64_t _gravity_margin_time = 0;
  uint64_t _lock_delay = 0;

  /** Spin and placement tracking for lock resolution. */
  SpinContext _spin_context;
  std::optional<Placement> _last_successful_placement;
  std::optional<LockResult> _last_lock_result;
  uint32_t _soft_drop_cells = 0;
  uint16_t _hard_drop_cells = 0;
  bool _edit_mode = false;

  /** Incoming packets: legacy modes cap each rise at eight lines. */
  std::deque<PendingGarbage> _pending_garbage;
  uint16_t _garbage_queue_delay = 0;
  std::mt19937_64 _garbage_rng;
  std::optional<uint16_t> _guideline_hole;
  uint8_t _guideline_hole_rows = 0;

  /** Restarts grounded lock timing after a successful player movement. */
  void on_player_move(bool was_grounded, uint16_t cells_moved = 1);
  ActivePiece spawn_piece(PieceType type) const;
  void descended_to(const ActivePiece &piece);
  /** Applies ready packets in arrival order; Guideline rises after any lock. */
  void apply_ready_garbage();
  /** Commit an already-validated hard-drop landing without searching again. */
  LockResult lock_hard_drop_landing(const ActivePiece &landing);
  std::vector<Placement> search_placements(bool use_hold) const;

  std::unique_ptr<RotationSystem> _rotation_system;
  std::unique_ptr<Randomizer> _randomizer;
  std::unique_ptr<SpinSystem> _spin_system;
  std::unique_ptr<ScoreSystem> _score_system;
  std::unique_ptr<AttackScheme> _attack_scheme;
  std::shared_ptr<const int> _rule_context = std::make_shared<int>(0);

  uint8_t _preview_count;
};

} // namespace tetris
