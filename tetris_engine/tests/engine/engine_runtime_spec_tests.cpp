#include "engine/test_support.hpp"

#include "engine/Engine.hpp"
#include <optional>

namespace {

void test_engine_constructor_defaults() {
  tetris::Engine engine(tetris::Board(10, 24), make_rotation_system(),
                        make_randomizer(), make_spin_system(),
                        make_score_system(), make_attack_scheme(), 5);

  require(engine.board().get_width() == 10,
          "engine should expose the board passed to its constructor");
  require(engine.board().get_height() == 24,
          "engine should preserve board dimensions");
  require(engine.active_piece().value().piece.type() == tetris::PieceType::I,
          "engine should start with a default active piece placeholder");
  require(!engine.hold_piece().has_value(),
          "engine should start with an empty hold slot");
  require(!engine.game_over(), "engine should not start in game-over state");
  require(engine.can_hold(), "engine should allow hold before the first lock");
  require(engine.lines_cleared() == 0,
          "engine should start with zero cleared lines");
  require(engine.score() == 0, "engine should start with zero score");
  require(engine.combo() == 0, "engine should start with zero combo");
  require(!engine.back_to_back(),
          "engine should start with back-to-back disabled");
}

void test_engine_reset_should_reseed_and_restore_defaults() {
  tetris::Engine engine(tetris::Board(10, 24), make_rotation_system(),
                        make_randomizer(), make_spin_system(),
                        make_score_system(), make_attack_scheme(), 5);

  engine.reset(42);

  require(engine.active_piece().value().piece.type() == tetris::PieceType::I,
          "engine reset should respawn a new active piece from the randomizer");
  require(!engine.hold_piece().has_value(),
          "engine reset should clear the hold slot");
  require(engine.preview_queue().size() == 5,
          "engine reset should rebuild the requested preview queue");
}

void test_engine_spawn_hold_and_preview_flow() {
  tetris::Engine engine(tetris::Board(10, 24), make_rotation_system(),
                        make_randomizer(), make_spin_system(),
                        make_score_system(), make_attack_scheme(), 3);

  require(engine.spawn_next_piece(),
          "engine should spawn the next piece when the spawn area is free");
  require(engine.active_piece().value().piece.type() == tetris::PieceType::I,
          "engine spawn should consume the first queued piece");
  require(engine.preview_queue().size() == 3,
          "engine preview should expose the configured preview length");
  require(engine.hold(), "engine should allow holding the current piece");
  require(engine.hold_piece().has_value() &&
              engine.hold_piece().value() == tetris::PieceType::I,
          "engine hold should store the current piece type");
  require(!engine.can_hold(),
          "engine should disable repeated holds until the current piece locks");
}

void test_engine_movement_and_rotation_actions() {
  tetris::Engine engine(tetris::Board(10, 24), make_rotation_system(),
                        make_randomizer(), make_spin_system(),
                        make_score_system(), make_attack_scheme(), 5);

  engine.spawn_next_piece();

  require(engine.try_move_left(), "engine should move left when unobstructed");
  require(engine.try_move_right(),
          "engine should move right when unobstructed");
  require(engine.try_soft_drop(2),
          "engine should soft drop by the requested amount");
  require(engine.try_rotate_cw(), "engine should rotate clockwise when legal");
  require(engine.try_rotate_ccw(),
          "engine should rotate counterclockwise when legal");
  require(engine.try_rotate_180(),
          "engine should rotate 180 degrees when legal");

  tetris::StepResult left_step = engine.step(tetris::Action::Left);
  require(left_step.moved && !left_step.locked,
          "engine step should report a successful movement action");
}

void test_engine_drop_tick_and_lock_resolution() {
  tetris::Engine engine(tetris::Board(10, 24), make_rotation_system(),
                        make_randomizer(), make_spin_system(),
                        make_score_system(), make_attack_scheme(), 5);

  engine.spawn_next_piece();

  require(engine.hard_drop_distance() > 0,
          "engine should report the available hard drop distance");
  std::optional<tetris::ActivePiece> ghost = engine.ghost_piece();
  require(ghost.value().pos.row < engine.active_piece().value().pos.row,
          "engine ghost piece should rest below the active piece");

  tetris::TickResult tick = engine.tick();
  require(tick.moved_down || tick.locked,
          "engine tick should either apply gravity or lock the piece");

  tetris::LockResult hard_drop = engine.hard_drop();
  require(hard_drop.lines_cleared <= 4,
          "engine hard drop should resolve a lock and at most a tetris clear");

  tetris::LockResult lock = engine.lock_active_piece();
  require(
      lock.attack_sent >= 0,
      "engine lock should compute outgoing garbage through the attack scheme");
}

void test_engine_garbage_and_game_over_flow() {
  tetris::Engine engine(tetris::Board(10, 24), make_rotation_system(),
                        make_randomizer(), make_spin_system(),
                        make_score_system(), make_attack_scheme(), 5);

  engine.receive_garbage(2, 3);
  require(engine.board().get(0, 3) == tetris::Cell::Empty,
          "engine garbage receive should preserve the requested hole column");

  engine.receive_garbage(tetris::GarbageEvent{.lines = 20, .hole_col = 4});
  require(engine.board().touch_max_height() || engine.game_over(),
          "engine oversized garbage should eventually top out the board");
}

} // namespace

int main() {
  test_engine_constructor_defaults();
  test_engine_reset_should_reseed_and_restore_defaults();
  test_engine_spawn_hold_and_preview_flow();
  test_engine_movement_and_rotation_actions();
  test_engine_drop_tick_and_lock_resolution();
  test_engine_garbage_and_game_over_flow();
  return 0;
}
