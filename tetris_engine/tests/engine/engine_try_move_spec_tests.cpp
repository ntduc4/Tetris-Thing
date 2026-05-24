#include "engine/test_support.hpp"

#define private public
#include "engine/Engine.hpp"
#undef private

#include <deque>
#include <memory>

namespace {

class TrackingRandomizer : public FakeRandomizer {
public:
  tetris::PieceType pop() override {
    ++pop_count;
    return FakeRandomizer::pop();
  }

  size_t pop_count = 0;
};

class NoRotationSystem : public tetris::RotationSystem {
public:
  std::optional<tetris::Offset> kick_offset(tetris::PieceType, tetris::Rotation,
                                            tetris::Rotation,
                                            uint8_t) const override {
    return std::nullopt;
  }

  std::optional<tetris::ActivePiece>
  try_rotate_cw(const tetris::Board &, const tetris::ActivePiece &) const override {
    return std::nullopt;
  }

  std::optional<tetris::ActivePiece>
  try_rotate_ccw(const tetris::Board &, const tetris::ActivePiece &) const override {
    return std::nullopt;
  }

  std::optional<tetris::ActivePiece>
  try_rotate_180(const tetris::Board &, const tetris::ActivePiece &) const override {
    return std::nullopt;
  }
};

class InPlaceRotationSystem : public tetris::RotationSystem {
public:
  std::optional<tetris::Offset> kick_offset(tetris::PieceType, tetris::Rotation,
                                            tetris::Rotation,
                                            uint8_t) const override {
    return std::nullopt;
  }

  std::optional<tetris::ActivePiece>
  try_rotate_cw(const tetris::Board &board,
                const tetris::ActivePiece &piece) const override {
    return rotate_if_placeable(board, piece, 1);
  }

  std::optional<tetris::ActivePiece>
  try_rotate_ccw(const tetris::Board &board,
                 const tetris::ActivePiece &piece) const override {
    return rotate_if_placeable(board, piece, -1);
  }

  std::optional<tetris::ActivePiece>
  try_rotate_180(const tetris::Board &board,
                 const tetris::ActivePiece &piece) const override {
    return rotate_if_placeable(board, piece, 2);
  }

private:
  static std::optional<tetris::ActivePiece>
  rotate_if_placeable(const tetris::Board &board, const tetris::ActivePiece &piece,
                      int turns) {
    tetris::ActivePiece rotated{.piece = piece.piece, .pos = piece.pos};
    if (turns < 0) {
      rotated.piece.rotate_ccw();
    } else {
      for (int i = 0; i < turns; ++i)
        rotated.piece.rotate_cw();
    }
    return board.collide(rotated) ? std::optional<tetris::ActivePiece>(rotated)
                                  : std::nullopt;
  }
};

class MultiKickRotationSystem : public tetris::RotationSystem {
public:
  std::optional<tetris::Offset> kick_offset(tetris::PieceType, tetris::Rotation,
                                            tetris::Rotation,
                                            uint8_t check_num) const override {
    if (check_num == 0)
      return tetris::Offset{.row = 0, .col = -1};
    if (check_num == 1)
      return tetris::Offset{.row = 0, .col = 2};
    if (check_num == 2)
      return tetris::Offset{.row = -1, .col = 1};
    return std::nullopt;
  }

  std::optional<tetris::ActivePiece>
  try_rotate_cw(const tetris::Board &board,
                const tetris::ActivePiece &piece) const override {
    return rotate_with_kicks(board, piece, 1, {{0, -1}, {0, 2}, {-1, 1}});
  }

  std::optional<tetris::ActivePiece>
  try_rotate_ccw(const tetris::Board &board,
                 const tetris::ActivePiece &piece) const override {
    return rotate_with_kicks(board, piece, -1, {{0, 1}, {-1, 0}, {0, -2}});
  }

  std::optional<tetris::ActivePiece>
  try_rotate_180(const tetris::Board &board,
                 const tetris::ActivePiece &piece) const override {
    return rotate_with_kicks(board, piece, 2, {{-1, 0}, {0, 1}, {-1, 1}});
  }

private:
  static std::optional<tetris::ActivePiece>
  rotate_with_kicks(const tetris::Board &board, const tetris::ActivePiece &piece,
                    int turns,
                    std::initializer_list<tetris::Offset> kicks) {
    tetris::ActivePiece rotated{.piece = piece.piece, .pos = piece.pos};
    if (turns == -1) {
      rotated.piece.rotate_ccw();
    } else {
      for (int i = 0; i < turns; ++i)
        rotated.piece.rotate_cw();
    }

    if (board.collide(rotated))
      return rotated;

    for (tetris::Offset kick : kicks) {
      tetris::ActivePiece kicked{.piece = rotated.piece,
                                 .pos = tetris::Position{
                                     .row = static_cast<int16_t>(rotated.pos.row + kick.row),
                                     .col = static_cast<int16_t>(rotated.pos.col + kick.col)}};
      if (board.collide(kicked))
        return kicked;
    }

    return std::nullopt;
  }
};

class NotchRotationSystem : public tetris::RotationSystem {
public:
  std::optional<tetris::Offset> kick_offset(tetris::PieceType, tetris::Rotation,
                                            tetris::Rotation,
                                            uint8_t check_num) const override {
    if (check_num == 0)
      return tetris::Offset{.row = -1, .col = 0};
    if (check_num == 1)
      return tetris::Offset{.row = -1, .col = 1};
    if (check_num == 2)
      return tetris::Offset{.row = 0, .col = 1};
    return std::nullopt;
  }

  std::optional<tetris::ActivePiece>
  try_rotate_cw(const tetris::Board &board,
                const tetris::ActivePiece &piece) const override {
    return rotate_with_kicks(board, piece, 1, {{-1, 0}, {-1, 1}, {0, 1}});
  }

  std::optional<tetris::ActivePiece>
  try_rotate_ccw(const tetris::Board &board,
                 const tetris::ActivePiece &piece) const override {
    return rotate_with_kicks(board, piece, -1, {{0, 1}, {-1, 1}, {-1, 0}});
  }

  std::optional<tetris::ActivePiece>
  try_rotate_180(const tetris::Board &board,
                 const tetris::ActivePiece &piece) const override {
    return rotate_with_kicks(board, piece, 2, {{-1, 0}, {-2, 0}, {-1, 1}});
  }

private:
  static std::optional<tetris::ActivePiece>
  rotate_with_kicks(const tetris::Board &board, const tetris::ActivePiece &piece,
                    int turns,
                    std::initializer_list<tetris::Offset> kicks) {
    tetris::ActivePiece rotated{.piece = piece.piece, .pos = piece.pos};
    if (turns == -1) {
      rotated.piece.rotate_ccw();
    } else {
      for (int i = 0; i < turns; ++i)
        rotated.piece.rotate_cw();
    }

    if (board.collide(rotated))
      return rotated;

    for (tetris::Offset kick : kicks) {
      tetris::ActivePiece kicked{.piece = rotated.piece,
                                 .pos = tetris::Position{
                                     .row = static_cast<int16_t>(rotated.pos.row + kick.row),
                                     .col = static_cast<int16_t>(rotated.pos.col + kick.col)}};
      if (board.collide(kicked))
        return kicked;
    }

    return std::nullopt;
  }
};

struct EngineSnapshot {
  std::vector<std::vector<tetris::Cell>> board_render;
  std::optional<tetris::ActivePiece> active_piece;
  std::optional<tetris::PieceType> hold_piece;
  bool game_over;
  bool can_hold;
  uint32_t lines_cleared;
  uint64_t score;
  uint32_t combo;
  bool back_to_back;
  int16_t spawn_col_offset;
  int16_t spawn_row_offset;
  uint64_t tick_count;
  uint32_t gravity_progress;
  bool grounded_last_tick;
  uint16_t lock_timer;
  uint16_t lock_resets_used;
  bool last_tick_was_player_action;
  tetris::SpinContext spin_context;
  std::optional<tetris::Placement> last_successful_placement;
  std::deque<tetris::PendingGarbage> pending_garbage;
};

bool same_piece(const std::optional<tetris::ActivePiece> &lhs,
                const std::optional<tetris::ActivePiece> &rhs) {
  if (lhs.has_value() != rhs.has_value())
    return false;
  if (!lhs.has_value())
    return true;
  return lhs->piece.type() == rhs->piece.type() &&
         lhs->piece.rotation() == rhs->piece.rotation() &&
         lhs->pos.row == rhs->pos.row && lhs->pos.col == rhs->pos.col;
}

bool same_placement(const std::optional<tetris::Placement> &lhs,
                    const std::optional<tetris::Placement> &rhs) {
  if (lhs.has_value() != rhs.has_value())
    return false;
  if (!lhs.has_value())
    return true;
  return lhs->moves == rhs->moves && lhs->type == rhs->type &&
         lhs->final_pos.row == rhs->final_pos.row &&
         lhs->final_pos.col == rhs->final_pos.col &&
         lhs->final_rotation == rhs->final_rotation;
}

bool same_spin_context(const tetris::SpinContext &lhs,
                       const tetris::SpinContext &rhs) {
  return lhs.last_movement == rhs.last_movement &&
         same_piece(lhs.previous_piece, rhs.previous_piece) &&
         lhs.used_kick == rhs.used_kick && lhs.kick_index == rhs.kick_index &&
         lhs.last_move_was_player_action == rhs.last_move_was_player_action;
}

bool same_pending_garbage(const std::deque<tetris::PendingGarbage> &lhs,
                         const std::deque<tetris::PendingGarbage> &rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].event.lines != rhs[i].event.lines ||
        lhs[i].event.hole_col != rhs[i].event.hole_col ||
        lhs[i].delay_ticks != rhs[i].delay_ticks)
      return false;
  }
  return true;
}

void set_active_piece(tetris::Engine &engine, const tetris::ActivePiece &piece) {
  engine._active_piece.emplace(piece);
}

EngineSnapshot snapshot(const tetris::Engine &engine) {
  return {.board_render = engine.board().render(),
          .active_piece = engine.active_piece(),
          .hold_piece = engine.hold_piece(),
          .game_over = engine.game_over(),
          .can_hold = engine.can_hold(),
          .lines_cleared = engine.lines_cleared(),
          .score = engine.score(),
          .combo = engine.combo(),
          .back_to_back = engine.back_to_back(),
          .spawn_col_offset = engine._spawn_col_offset,
          .spawn_row_offset = engine._spawn_row_offset,
          .tick_count = engine._tick_count,
          .gravity_progress = engine._gravity_progress,
          .grounded_last_tick = engine._grounded_last_tick,
          .lock_timer = engine._lock_timer,
          .lock_resets_used = engine._lock_resets_used,
          .last_tick_was_player_action = engine._last_tick_was_player_action,
          .spin_context = engine._spin_context,
          .last_successful_placement = engine._last_successful_placement,
          .pending_garbage = engine._pending_garbage};
}

bool same_snapshot(const EngineSnapshot &lhs, const EngineSnapshot &rhs) {
  return lhs.board_render == rhs.board_render &&
         same_piece(lhs.active_piece, rhs.active_piece) &&
         lhs.hold_piece == rhs.hold_piece && lhs.game_over == rhs.game_over &&
         lhs.can_hold == rhs.can_hold &&
         lhs.lines_cleared == rhs.lines_cleared && lhs.score == rhs.score &&
         lhs.combo == rhs.combo && lhs.back_to_back == rhs.back_to_back &&
         lhs.spawn_col_offset == rhs.spawn_col_offset &&
         lhs.spawn_row_offset == rhs.spawn_row_offset &&
         lhs.tick_count == rhs.tick_count &&
         lhs.gravity_progress == rhs.gravity_progress &&
         lhs.grounded_last_tick == rhs.grounded_last_tick &&
         lhs.lock_timer == rhs.lock_timer &&
         lhs.lock_resets_used == rhs.lock_resets_used &&
         lhs.last_tick_was_player_action == rhs.last_tick_was_player_action &&
         same_spin_context(lhs.spin_context, rhs.spin_context) &&
         same_placement(lhs.last_successful_placement,
                        rhs.last_successful_placement) &&
         same_pending_garbage(lhs.pending_garbage, rhs.pending_garbage);
}

void require_no_state_change_after_false(const tetris::Engine &engine,
                                         const EngineSnapshot &before,
                                         bool result,
                                         const char *failure_message) {
  require(!result, failure_message);
  require(same_snapshot(before, snapshot(engine)),
          "false result should not change any engine state");
}

void require_no_state_change_after_lock_result(const tetris::Engine &engine,
                                               const EngineSnapshot &before,
                                               const tetris::LockResult &result,
                                               const char *failure_message) {
  require(!result.game_over && result.lines_cleared == 0 && !result.perfect_clear &&
              result.spin == tetris::SpinType::None && result.attack_sent == 0,
          failure_message);
  require(same_snapshot(before, snapshot(engine)),
          "failed lock-style action should not change any engine state");
}

void seed_nontrivial_state(tetris::Engine &engine) {
  engine._hold_piece = tetris::PieceType::S;
  engine._game_over = false;
  engine._can_hold = false;
  engine._lines_cleared = 7;
  engine._score = 1234;
  engine._combo = 2;
  engine._back_to_back = true;
  engine._tick_count = 42;
  engine._gravity_progress = 9;
  engine._grounded_last_tick = true;
  engine._lock_timer = 3;
  engine._lock_resets_used = 1;
  engine._last_tick_was_player_action = true;
  engine._spin_context.last_movement = tetris::Movement::Left;
  engine._spin_context.previous_piece.emplace(
      tetris::spawn_from_piece_type(tetris::PieceType::I));
  engine._spin_context.used_kick = true;
  engine._spin_context.kick_index = 1;
  engine._spin_context.last_move_was_player_action = true;
  engine._last_successful_placement = tetris::Placement{
      .moves = {tetris::Movement::Left, tetris::Movement::CW},
      .type = tetris::PieceType::T,
      .final_pos = tetris::Position{.row = 5, .col = 4},
      .final_rotation = tetris::Rotation::East};
  engine._pending_garbage.push_back(
      {.event = tetris::GarbageEvent{.lines = 2, .hole_col = 4},
       .delay_ticks = 6});
}

void test_try_move_methods_return_false_and_preserve_state_when_no_active_piece() {
  using namespace tetris;

  Engine engine(Board(10, 24), make_rotation_system(), make_randomizer());
  engine._active_piece.reset();
  seed_nontrivial_state(engine);

  EngineSnapshot before = snapshot(engine);
  require_no_state_change_after_false(engine, before, engine.try_move_left(3),
                                      "try_move_left should fail without an active piece");
  require_no_state_change_after_false(engine, before, engine.try_move_right(3),
                                      "try_move_right should fail without an active piece");
  require_no_state_change_after_false(engine, before, engine.try_soft_drop(3),
                                      "try_soft_drop should fail without an active piece");
  require_no_state_change_after_false(engine, before, engine.try_rotate_cw(),
                                      "try_rotate_cw should fail without an active piece");
  require_no_state_change_after_false(engine, before, engine.try_rotate_ccw(),
                                      "try_rotate_ccw should fail without an active piece");
  require_no_state_change_after_false(engine, before, engine.try_rotate_180(),
                                      "try_rotate_180 should fail without an active piece");
}

void test_active_piece_actions_noop_when_active_piece_is_already_colliding() {
  using namespace tetris;

  Board board(10, 24);
  board.set(4, 4, Cell::Garbage);
  Engine engine(board, std::make_unique<MultiKickRotationSystem>(),
                make_randomizer());
  ActivePiece active = spawn_from_piece_type(PieceType::O);
  active.pos.row = 3;
  active.pos.col = 3;
  set_active_piece(engine, active);
  seed_nontrivial_state(engine);
  set_active_piece(engine, active);

  EngineSnapshot before = snapshot(engine);
  require_no_state_change_after_false(
      engine, before, engine.try_move_left(1),
      "try_move_left should fail when the active piece is already colliding");
  require_no_state_change_after_false(
      engine, before, engine.try_move_right(1),
      "try_move_right should fail when the active piece is already colliding");
  require_no_state_change_after_false(
      engine, before, engine.try_soft_drop(1),
      "try_soft_drop should fail when the active piece is already colliding");
  require_no_state_change_after_false(
      engine, before, engine.try_rotate_cw(),
      "try_rotate_cw should fail when the active piece is already colliding");
  require_no_state_change_after_false(
      engine, before, engine.try_rotate_ccw(),
      "try_rotate_ccw should fail when the active piece is already colliding");
  require_no_state_change_after_false(
      engine, before, engine.try_rotate_180(),
      "try_rotate_180 should fail when the active piece is already colliding");
  require_no_state_change_after_lock_result(
      engine, before, engine.hard_drop(),
      "hard_drop should no-op when the active piece is already colliding");
  require_no_state_change_after_lock_result(
      engine, before, engine.lock_active_piece(),
      "lock_active_piece should no-op when the active piece is already colliding");
}

void test_active_piece_actions_noop_when_game_is_over() {
  using namespace tetris;

  Engine engine(Board(10, 24), std::make_unique<MultiKickRotationSystem>(),
                make_randomizer());
  ActivePiece active = spawn_from_piece_type(PieceType::T);
  active.pos.row = 10;
  active.pos.col = 4;
  set_active_piece(engine, active);
  seed_nontrivial_state(engine);
  set_active_piece(engine, active);
  engine._game_over = true;

  EngineSnapshot before = snapshot(engine);
  require_no_state_change_after_false(
      engine, before, engine.try_move_left(1),
      "try_move_left should fail when the game is over");
  require_no_state_change_after_false(
      engine, before, engine.try_move_right(1),
      "try_move_right should fail when the game is over");
  require_no_state_change_after_false(
      engine, before, engine.try_soft_drop(1),
      "try_soft_drop should fail when the game is over");
  require_no_state_change_after_false(
      engine, before, engine.try_rotate_cw(),
      "try_rotate_cw should fail when the game is over");
  require_no_state_change_after_false(
      engine, before, engine.try_rotate_ccw(),
      "try_rotate_ccw should fail when the game is over");
  require_no_state_change_after_false(
      engine, before, engine.try_rotate_180(),
      "try_rotate_180 should fail when the game is over");
  require_no_state_change_after_lock_result(
      engine, before, engine.hard_drop(),
      "hard_drop should no-op when the game is over");
  require_no_state_change_after_lock_result(
      engine, before, engine.lock_active_piece(),
      "lock_active_piece should no-op when the game is over");
}

void test_try_move_left_supports_partial_and_furthest_movement() {
  using namespace tetris;

  {
    Engine engine(Board(10, 24), make_rotation_system(), make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::O);
    active.pos.row = 10;
    active.pos.col = 5;
    set_active_piece(engine, active);

    require(engine.try_move_left(-1),
            "negative left amount should move to the furthest reachable column");
    require(engine.active_piece()->pos.col == -1,
            "negative left amount should move the piece to the left wall");
  }

  {
    Board board(10, 24);
    board.set(11, 3, Cell::Garbage);
    Engine engine(board, make_rotation_system(), make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::O);
    active.pos.row = 10;
    active.pos.col = 5;
    set_active_piece(engine, active);

    require(engine.try_move_left(5),
            "left movement should return true when the piece can move partially");
    require(engine.active_piece()->pos.col == 3,
            "left movement should stop at the last collision-free column");
  }

  {
    Board board(10, 24);
    ActivePiece active = spawn_from_piece_type(PieceType::O);
    active.pos.row = 10;
    active.pos.col = -1;
    board.set(11, 0, Cell::Garbage);
    Engine engine(board, make_rotation_system(), make_randomizer());
    set_active_piece(engine, active);
    seed_nontrivial_state(engine);
    set_active_piece(engine, active);
    EngineSnapshot before = snapshot(engine);

    require_no_state_change_after_false(engine, before, engine.try_move_left(1),
                                        "left movement should fail when blocked immediately");
  }
}

void test_try_move_right_supports_partial_and_furthest_movement() {
  using namespace tetris;

  {
    Engine engine(Board(10, 24), make_rotation_system(), make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::O);
    active.pos.row = 10;
    active.pos.col = 1;
    set_active_piece(engine, active);

    require(engine.try_move_right(-1),
            "negative right amount should move to the furthest reachable column");
    require(engine.active_piece()->pos.col == 7,
            "negative right amount should move the piece to the right wall");
  }

  {
    Board board(10, 24);
    board.set(11, 8, Cell::Garbage);
    Engine engine(board, make_rotation_system(), make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::O);
    active.pos.row = 10;
    active.pos.col = 3;
    set_active_piece(engine, active);

    require(engine.try_move_right(5),
            "right movement should return true when the piece can move partially");
    require(engine.active_piece()->pos.col == 5,
            "right movement should stop at the last collision-free column");
  }

  {
    Board board(10, 24);
    ActivePiece active = spawn_from_piece_type(PieceType::O);
    active.pos.row = 10;
    active.pos.col = 7;
    board.set(11, 9, Cell::Garbage);
    Engine engine(board, make_rotation_system(), make_randomizer());
    set_active_piece(engine, active);
    seed_nontrivial_state(engine);
    set_active_piece(engine, active);
    EngineSnapshot before = snapshot(engine);

    require_no_state_change_after_false(engine, before, engine.try_move_right(1),
                                        "right movement should fail when blocked immediately");
  }
}

void test_try_soft_drop_supports_partial_and_furthest_movement() {
  using namespace tetris;

  {
    Engine engine(Board(10, 24), make_rotation_system(), make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::O);
    active.pos.row = 10;
    active.pos.col = 3;
    set_active_piece(engine, active);

    require(engine.try_soft_drop(-1),
            "negative soft drop should move to the furthest reachable row");
    require(engine.active_piece()->pos.row == -1,
            "negative soft drop should place the piece on the floor");
  }

  {
    Board board(10, 24);
    board.set(3, 4, Cell::Garbage);
    Engine engine(board, make_rotation_system(), make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::O);
    active.pos.row = 10;
    active.pos.col = 3;
    set_active_piece(engine, active);

    require(engine.try_soft_drop(20),
            "soft drop should return true when the piece can move part of the requested amount");
    require(engine.active_piece()->pos.row == 3,
            "soft drop should stop at the last collision-free row");
  }

  {
    Board board(10, 24);
    board.set(5, 4, Cell::Garbage);
    Engine engine(board, make_rotation_system(), make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::O);
    active.pos.row = 5;
    active.pos.col = 3;
    set_active_piece(engine, active);
    seed_nontrivial_state(engine);
    set_active_piece(engine, active);
    EngineSnapshot before = snapshot(engine);

    require_no_state_change_after_false(engine, before, engine.try_soft_drop(1),
                                        "soft drop should fail when blocked immediately");
  }
}

void test_try_rotate_methods_cover_in_place_kick_and_failure_cases() {
  using namespace tetris;

  {
    Engine engine(Board(10, 24), std::make_unique<InPlaceRotationSystem>(),
                  make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::T);
    active.pos.row = 10;
    active.pos.col = 4;
    set_active_piece(engine, active);

    require(engine.try_rotate_cw(),
            "clockwise rotation should succeed in open space with an in-place rotation system");
    require(engine.active_piece()->piece.rotation() == Rotation::East,
            "clockwise rotation should update the active piece rotation");
    require(engine.active_piece()->pos.col == 4,
            "in-place rotation should not move the piece when no kick is needed");
  }

  {
    Engine engine(Board(10, 24), std::make_unique<InPlaceRotationSystem>(),
                  make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::T);
    active.pos.row = 10;
    active.pos.col = 4;
    active.piece.rotate_cw();
    set_active_piece(engine, active);

    require(engine.try_rotate_ccw(),
            "counterclockwise rotation should succeed in open space");
    require(engine.active_piece()->piece.rotation() == Rotation::North,
            "counterclockwise rotation should update the active piece rotation");
  }

  {
    Engine engine(Board(10, 24), std::make_unique<InPlaceRotationSystem>(),
                  make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::T);
    active.pos.row = 10;
    active.pos.col = 4;
    set_active_piece(engine, active);

    require(engine.try_rotate_180(),
            "180 rotation should succeed in open space");
    require(engine.active_piece()->piece.rotation() == Rotation::South,
            "180 rotation should update the active piece rotation");
  }

  {
    Board board(10, 24);
    board.set(11, 5, Cell::Garbage);
    board.set(12, 5, Cell::Garbage);
    Engine engine(board, std::make_unique<NoRotationSystem>(), make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::T);
    active.pos.row = 10;
    active.pos.col = 4;
    set_active_piece(engine, active);
    seed_nontrivial_state(engine);
    set_active_piece(engine, active);
    EngineSnapshot before = snapshot(engine);

    require_no_state_change_after_false(engine, before, engine.try_rotate_cw(),
                                        "rotation should fail and preserve state when no rotation path exists");
  }

  {
    Board board(10, 24);
    board.set(11, 5, Cell::Garbage);
    board.set(12, 5, Cell::Garbage);
    Engine engine(board, std::make_unique<MultiKickRotationSystem>(),
                  make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::T);
    active.pos.row = 10;
    active.pos.col = 4;
    set_active_piece(engine, active);

    require(engine.try_rotate_cw(),
            "rotation should succeed when the rotation system finds a later valid kick");
    require(engine.active_piece()->piece.rotation() == Rotation::East,
            "kicked rotation should update the active piece rotation");
    require(engine.active_piece()->pos.col == 6,
            "kicked rotation should apply the successful multi-kick offset");
  }

  {
    Board board(10, 24);
    board.set(11, 5, Cell::Garbage);
    board.set(12, 5, Cell::Garbage);
    board.set(11, 6, Cell::Garbage);
    board.set(12, 6, Cell::Garbage);
    board.set(11, 7, Cell::Garbage);
    board.set(12, 7, Cell::Garbage);
    Engine engine(board, std::make_unique<MultiKickRotationSystem>(),
                  make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::T);
    active.pos.row = 10;
    active.pos.col = 4;
    set_active_piece(engine, active);
    seed_nontrivial_state(engine);
    set_active_piece(engine, active);
    EngineSnapshot before = snapshot(engine);

    require_no_state_change_after_false(engine, before, engine.try_rotate_cw(),
                                        "rotation should fail and preserve state when all kick options are blocked");
  }

  {
    Board board(10, 24);
    board.set(12, 4, Cell::Garbage);
    board.set(12, 5, Cell::Garbage);
    board.set(11, 6, Cell::Garbage);
    Engine engine(board, std::make_unique<NotchRotationSystem>(),
                  make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::L);
    active.pos.row = 10;
    active.pos.col = 4;
    set_active_piece(engine, active);

    require(engine.try_rotate_cw(),
            "rotation should succeed when a notch-style upward kick is required");
    require(engine.active_piece()->piece.rotation() == Rotation::East,
            "notch rotation should update the active piece rotation");
    require(engine.active_piece()->pos.row == 9 &&
                engine.active_piece()->pos.col == 5,
            "notch rotation should use the upward-right kick path");
  }

  {
    Board board(10, 24);
    board.set(12, 4, Cell::Garbage);
    board.set(12, 5, Cell::Garbage);
    board.set(11, 6, Cell::Garbage);
    board.set(10, 6, Cell::Garbage);
    board.set(11, 5, Cell::Garbage);
    Engine engine(board, std::make_unique<NotchRotationSystem>(),
                  make_randomizer());
    ActivePiece active = spawn_from_piece_type(PieceType::L);
    active.pos.row = 10;
    active.pos.col = 4;
    set_active_piece(engine, active);
    seed_nontrivial_state(engine);
    set_active_piece(engine, active);
    EngineSnapshot before = snapshot(engine);

    require_no_state_change_after_false(
        engine, before, engine.try_rotate_cw(),
        "notch-style rotation should fail and preserve state when every kick path is blocked");
  }
}

void test_hard_drop_locks_without_spawning_next_piece() {
  using namespace tetris;

  auto randomizer = std::make_unique<TrackingRandomizer>();
  TrackingRandomizer *randomizer_ptr = randomizer.get();
  Engine engine(Board(10, 24), make_rotation_system(), std::move(randomizer));

  ActivePiece active = spawn_from_piece_type(PieceType::O);
  active.pos.row = 10;
  active.pos.col = 3;
  set_active_piece(engine, active);

  engine.hard_drop();

  require(randomizer_ptr->pop_count == 0,
          "hard drop should not spawn or consume the next queue piece");
  require(engine.board().get(0, 4) == Cell::O && engine.board().get(1, 4) == Cell::O &&
              engine.board().get(0, 5) == Cell::O && engine.board().get(1, 5) == Cell::O,
          "hard drop should lock the active piece at its lowest reachable position");
}

} // namespace

int main() {
  test_try_move_methods_return_false_and_preserve_state_when_no_active_piece();
  test_active_piece_actions_noop_when_active_piece_is_already_colliding();
  test_active_piece_actions_noop_when_game_is_over();
  test_try_move_left_supports_partial_and_furthest_movement();
  test_try_move_right_supports_partial_and_furthest_movement();
  test_try_soft_drop_supports_partial_and_furthest_movement();
  test_try_rotate_methods_cover_in_place_kick_and_failure_cases();
  test_hard_drop_locks_without_spawning_next_piece();
  return 0;
}
