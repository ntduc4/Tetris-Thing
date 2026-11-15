#include "engine/test_support.hpp"

#include "engine/Engine.hpp"
#include "engine/Match.hpp"
#include "engine/SoloSession.hpp"
#include <algorithm>
#include <memory>

namespace {
using namespace tetris;

class OnlyI : public Randomizer {
public:
  void seed(uint64_t) override {}
  PieceType peek(uint8_t = 0) const override { return PieceType::I; }
  std::vector<PieceType> preview(uint8_t count) const override {
    return std::vector<PieceType>(count, PieceType::I);
  }
  PieceType pop() override { return PieceType::I; }
};

Engine make_game(Board board = Board(10, 24), double gravity = 1.0,
                 uint64_t delay = 2) {
  return Engine(board, std::make_unique<SRS>(), std::make_unique<SevenBag>(1),
                std::make_unique<AllSpin>(), std::make_unique<Classical>(),
                std::make_unique<TetrioSS2>(), 5, 3, 10, gravity, 0, 100, delay);
}

void test_seeded_bags_and_kicks() {
  SevenBag first(42), second(42);
  require(first.preview(21) == second.preview(21),
          "seeded bags must provide identical previews");
  auto bag = first.preview(7);
  std::sort(bag.begin(), bag.end());
  require(std::adjacent_find(bag.begin(), bag.end()) == bag.end(),
          "each 7-bag must have one of every piece");
  auto kicks = SRS{}.kick_offsets(PieceType::I, Rotation::North, Rotation::East);
  require(kicks.size() == 5 && kicks[0] == Offset{0, 0} &&
              kicks[1] == Offset{0, 1},
          "SRS+ I kicks should start with an in-place and a rightward test");
  require(!None{}.allows_rotation(), "no-rotation mode must forbid rotation");
}

void test_lock_delay_and_reset() {
  Engine game = make_game(Board(10, 24), 20.0, 2);
  require(game.spawn_next_piece(), "piece must spawn");
  for (int i = 0; i < 2 && !game.board().grounded(*game.active_piece()); ++i)
    game.tick();
  require(game.active_piece().has_value() &&
              game.board().grounded(*game.active_piece()),
          "fast gravity should land the piece");
  require(game.try_move_left(), "grounded player movement should work");
  require(!game.tick().locked, "grounded movement should restart lock delay");
  TickResult result = game.tick();
  require(result.locked && !game.active_piece().has_value(),
          "piece must lock after the configured grounded delay");
  require(game.board().max_height() > 0, "locking should put cells on the board");
  require(game.spawn_next_piece(), "another piece should spawn after lock");
}

void test_line_clear_score_and_attack() {
  Board board(10, 24);
  for (uint16_t col = 0; col < 10; ++col)
    if (col < 3 || col > 6)
      board.set(0, col, Cell::Garbage);
  Engine game(board, std::make_unique<SRS>(), std::make_unique<OnlyI>(),
              std::make_unique<TSpin>(), std::make_unique<Classical>(),
              std::make_unique<TetrioSS2>(), 5, 3, 10, 0);
  require(game.spawn_next_piece(), "piece must spawn");
  LockResult lock = game.hard_drop();
  require(lock.locked && lock.lines_cleared == 1 && lock.perfect_clear,
          "I piece should finish the line and produce a perfect clear");
  require(game.score() >= 3600, "line and perfect clear should award score");
  require(lock.attack_sent == 5, "perfect-clear attack should be emitted");
  require(game.last_lock_result().has_value(), "last lock should be observable");
}

void test_spin_modes_and_dynamic_tables() {
  Board board(10, 24);
  ActivePiece piece = spawn_from_piece_type(PieceType::T, 5, 4);
  board.set(8, 4, Cell::Garbage);
  board.set(8, 6, Cell::Garbage);
  board.set(6, 4, Cell::Garbage);
  SpinContext rotation{.last_movement = Movement::CW};
  require(TSpin{}.detect(board, piece, rotation) == SpinType::Full,
          "three corners with both front corners should be a full T-spin");
  board.set(8, 6, Cell::Empty);
  board.set(6, 6, Cell::Garbage);
  require(TSpin{}.detect(board, piece, rotation) == SpinType::Mini,
          "three corners missing a front corner should be a mini");
  require(TSpin{}.detect(board, piece, SpinContext{}) == SpinType::None,
          "a T-spin requires the last successful action to be rotation");

  AttackTable table;
  table.normal[1] = 3;
  table.perfect_clear_bonus = 7;
  require(TableAttack(table).garbage_to_send(
              AttackContext{.lines_cleared = 1, .perfect_clear = true}) == 10,
          "attack table values should be configurable");
  ScoreTable scoring;
  scoring.normal[1] = 200;
  require(Classical(scoring).score(ScoreEvent{.lines_cleared = 1}) == 200,
          "score table values should be configurable");
}

void test_garbage_cancel_and_topout() {
  Engine game = make_game();
  require(game.spawn_next_piece(), "piece must spawn");
  game.receive_garbage(2, 3);
  require(game.pending_garbage_lines() == 2,
          "incoming garbage should queue while a piece is falling");
  game.hard_drop();
  require(game.board().get(0, 3) == Cell::Empty &&
              game.board().get(0, 4) == Cell::Garbage,
          "garbage should rise with the supplied hole after a lock");

  Engine crowded = make_game(Board(10, 24));
  crowded.receive_garbage(25, 4);
  require(crowded.game_over(), "garbage above the board must top out");
}

void test_editor_pauses_and_rejects_colliding_exit() {
  Engine game = make_game();
  require(game.spawn_next_piece(), "piece must spawn");
  auto occupied = game.active_piece()->piece.current_shape().front();
  int row = game.active_piece()->pos.row + occupied.row;
  int col = game.active_piece()->pos.col + occupied.col;
  uint64_t before = game.tick_count();
  require(game.set_edit_mode(true), "editor should open");
  require(game.paint_cell(row, col, Cell::T), "editor can paint colored minos");
  require(!game.set_edit_mode(false), "cannot resume while piece overlaps board");
  require(!game.tick().moved_down && game.tick_count() == before,
          "editor should pause simulation ticks");
  require(game.paint_cell(row, col, Cell::Empty), "editor can erase minos");
  require(game.set_edit_mode(false), "editor should resume after clearing overlap");
}

void test_spawning_does_not_replace_a_falling_piece() {
  Engine game = make_game();
  require(game.spawn_next_piece(), "initial spawn should work");
  auto active = game.active_piece();
  auto queue = game.preview_queue();
  require(!game.spawn_next_piece() && game.active_piece() == active &&
              game.preview_queue() == queue && !game.game_over(),
          "explicit spawn must not discard a falling piece or consume the queue");
}

void test_solo_sprint_blitz_and_marathon() {
  auto make_solo_engine = [] {
    return Engine(Board(10, 24), std::make_unique<SRS>(),
                  std::make_unique<OnlyI>(), std::make_unique<TSpin>(),
                  std::make_unique<Classical>(), std::make_unique<TetrioSS2>(),
                  5, 3, 10, 0);
  };

  SoloSession sprint(make_solo_engine(),
                     SoloSettings{.mode = SoloMode::Sprint, .line_goal = 1});
  sprint.start(23);
  require(sprint.engine().set_edit_mode(true), "sprint should permit editing");
  for (uint16_t col = 0; col < 10; ++col)
    if (col < 3 || col > 6)
      sprint.engine().paint_cell(0, col, Cell::Garbage);
  require(sprint.engine().set_edit_mode(false), "sprint should resume");
  require(sprint.hard_drop().lines_cleared == 1 && sprint.finished() &&
              sprint.succeeded() && !sprint.engine().active_piece(),
          "sprint should stop at its line goal without spawning another piece");
  uint64_t time = sprint.elapsed_ticks();
  sprint.tick();
  require(sprint.elapsed_ticks() == time,
          "finished solo mode should not continue simulation");
  sprint.start(23);
  require(!sprint.finished() && sprint.engine().lines_cleared() == 0,
          "restarting sprint should reset its objective and board");

  SoloSession blitz(make_solo_engine(),
                    SoloSettings{.mode = SoloMode::Blitz, .time_limit_ticks = 3});
  blitz.start();
  blitz.tick();
  blitz.tick();
  require(!blitz.finished(), "blitz should continue before the time limit");
  blitz.tick();
  require(blitz.finished() && blitz.succeeded() && blitz.elapsed_ticks() == 3,
          "blitz should finish exactly on its configured tick");

  SoloSession marathon(make_solo_engine());
  marathon.start();
  for (int i = 0; i < 5; ++i) marathon.tick();
  require(!marathon.finished(), "endless solo should have no time or line goal");
}

void test_room_rounds_and_match_winner() {
  std::vector<std::unique_ptr<Engine>> players;
  for (int i = 0; i < 2; ++i)
    players.push_back(std::make_unique<Engine>(make_game()));
  Match match(std::move(players), 2);
  match.start(13);
  match.player(1).receive_garbage(25, 4);
  match.hard_drop(1);
  require(match.round_winner() == 0 && match.wins(0) == 1 &&
              !match.match_winner(),
          "a knocked-out player should award one round win");
  match.start(14);
  require(!match.round_winner() && !match.player(1).game_over() &&
              match.wins(0) == 1,
          "next round should reset boards but retain match wins");
  match.player(1).receive_garbage(25, 4);
  match.hard_drop(1);
  require(match.match_winner() == 0 && match.wins(0) == 2,
          "reaching the room win target should finish the match");

  std::vector<std::unique_ptr<Engine>> tied_players;
  for (int i = 0; i < 2; ++i)
    tied_players.push_back(std::make_unique<Engine>(make_game(Board(10, 24), 20, 0)));
  Match draw(std::move(tied_players));
  draw.start(19);
  draw.player(0).receive_garbage(25, 3);
  draw.player(1).receive_garbage(25, 4);
  draw.tick();
  require(draw.round_over() && !draw.round_winner() && !draw.match_winner(),
          "simultaneous top-outs should end in a draw without awarding a win");
  draw.start(20);
  require(!draw.round_over() && draw.wins(0) == 0 && draw.wins(1) == 0,
          "a drawn round should allow a rematch without changing scores");
}

void test_multiplayer_attack_routing_and_same_seed() {
  std::vector<std::unique_ptr<Engine>> players;
  for (int i = 0; i < 2; ++i)
    players.push_back(std::make_unique<Engine>(Board(10, 24),
        std::make_unique<SRS>(), std::make_unique<OnlyI>(),
        std::make_unique<TSpin>(), std::make_unique<Classical>(),
        std::make_unique<TetrioSS2>(), 5, 3, 10));
  Match match(std::move(players));
  match.start(123);
  require(match.player(0).preview_queue() == match.player(1).preview_queue(),
          "matched players should share a deterministic seed");
  require(match.player(0).set_edit_mode(true), "editor can open in match");
  for (uint16_t col = 0; col < 10; ++col)
    if (col < 3 || col > 6)
      match.player(0).paint_cell(0, col, Cell::Garbage);
  require(match.player(0).set_edit_mode(false), "editor can resume in match");
  LockResult lock = match.hard_drop(0);
  require(lock.locked && lock.lines_cleared == 1 && lock.attack_sent == 5,
          "match should report outgoing perfect-clear attack");
  require(match.player(1).pending_garbage_lines() == 5,
          "match should queue attack on the opponent");
  match.hard_drop(1);
  require(match.player(1).pending_garbage_lines() == 0,
          "incoming garbage should rise when the opponent locks");
}
} // namespace

int main() {
  test_seeded_bags_and_kicks();
  test_lock_delay_and_reset();
  test_line_clear_score_and_attack();
  test_spin_modes_and_dynamic_tables();
  test_garbage_cancel_and_topout();
  test_editor_pauses_and_rejects_colliding_exit();
  test_spawning_does_not_replace_a_falling_piece();
  test_solo_sprint_blitz_and_marathon();
  test_room_rounds_and_match_winner();
  test_multiplayer_attack_routing_and_same_seed();
  return 0;
}
