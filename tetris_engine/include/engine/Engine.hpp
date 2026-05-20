#pragma once

#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include "core/Rotation.hpp"
#include "engine/Attack.hpp"
#include "engine/RNG.hpp"
#include "engine/Score.hpp"
#include "engine/Spin.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace tetris {

struct GarbageEvent {
  uint16_t lines = 0;
  uint16_t hole_col = 0;
};

struct StepResult {
  bool moved = false;
  bool locked = false;
  bool spawned = false;
  bool held = false;
  bool game_over = false;
  uint16_t lines_cleared = 0;
};

struct TickResult {
  bool moved_down = false;
  bool locked = false;
  bool game_over = false;
};

struct LockResult {
  bool game_over = false;
  uint16_t lines_cleared = 0;
  bool perfect_clear = false;
  SpinType spin = SpinType::None;
  uint16_t attack_sent = 0;
};

class Engine {
public:
  Engine(Board board, std::unique_ptr<RotationSystem> rotation_system,
         std::unique_ptr<Randomizer> randomizer,
         std::unique_ptr<SpinSystem> spin_system = nullptr,
         std::unique_ptr<ScoreSystem> score_system = nullptr,
         std::unique_ptr<AttackScheme> attack_scheme = nullptr,
         uint8_t preview_count = DefaultQueueLength)
      : _board{board}, _rotation_system{std::move(rotation_system)},
        _randomizer{std::move(randomizer)},
        _spin_system{std::move(spin_system)},
        _score_system{std::move(score_system)},
        _attack_scheme{std::move(attack_scheme)},
        _preview_count{preview_count} {};

  void reset(uint64_t seed = 0);

  const Board &board() const { return _board; }
  const ActivePiece &active_piece() const { return _active_piece; }
  std::optional<PieceType> hold_piece() const { return _hold_piece; }
  std::vector<PieceType> preview_queue() const;

  bool game_over() const { return _game_over; }
  bool can_hold() const { return _can_hold; }
  uint32_t lines_cleared() const { return _lines_cleared; }
  uint64_t score() const { return _score; }
  uint32_t combo() const { return _combo; }
  bool back_to_back() const { return _back_to_back; }

  bool spawn_next_piece();
  bool hold();

  bool try_move_left();
  bool try_move_right();
  bool try_soft_drop(uint16_t amount = 1);
  bool try_rotate_cw();
  bool try_rotate_ccw();
  bool try_rotate_180();

  ActivePiece ghost_piece() const;
  uint16_t hard_drop_distance() const;

  StepResult step(Action action);
  TickResult tick();

  LockResult hard_drop();
  LockResult lock_active_piece();

  void receive_garbage(uint16_t lines, uint16_t hole_col);
  void receive_garbage(const GarbageEvent &garbage);

private:
  Board _board;
  ActivePiece _active_piece = spawn_from_piece_type(PieceType::I);
  std::optional<PieceType> _hold_piece;
  bool _game_over = false;
  bool _can_hold = true;
  uint32_t _lines_cleared = 0;
  uint64_t _score = 0;
  uint32_t _combo = 0;
  bool _back_to_back = false;

  std::unique_ptr<RotationSystem> _rotation_system;
  std::unique_ptr<Randomizer> _randomizer;
  std::unique_ptr<SpinSystem> _spin_system;
  std::unique_ptr<ScoreSystem> _score_system;
  std::unique_ptr<AttackScheme> _attack_scheme;

  uint8_t _preview_count;
};

} // namespace tetris
