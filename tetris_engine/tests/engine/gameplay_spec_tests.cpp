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

class AlwaysMini : public SpinSystem {
public:
  SpinType detect(const Board &, const ActivePiece &,
                  const SpinContext &) const override {
    return SpinType::Mini;
  }
};

class OneLineAttack : public AttackScheme {
public:
  uint16_t garbage_to_send(const AttackContext &) const override { return 1; }
};

Engine make_game(Board board = Board(10, 24), double gravity = 1.0,
                 uint64_t delay = 2) {
  return Engine(board, std::make_unique<SRS>(), std::make_unique<SevenBag>(1),
                std::make_unique<AllSpin>(), std::make_unique<Classical>(),
                std::make_unique<TetrioSS2>(), 5, 3, 10, gravity, 0, 100, delay);
}

void fill_quad_well(Engine &game) {
  require(game.set_edit_mode(true), "editing should allow a quad setup");
  for (uint16_t row = 0; row < 4; ++row)
    for (uint16_t col = 0; col < 10; ++col)
      if (col != 5)
        require(game.paint_cell(row, col, Cell::Garbage),
                "quad rows should be paintable");
  require(game.set_edit_mode(false) && game.try_rotate_cw(),
          "vertical I should fit the quad well");
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
  require(game.back_to_back() && game.back_to_back_count() == 0,
          "a Season 2 All Clear should start a B2B chain");
  require(game.last_lock_result().has_value(), "last lock should be observable");
}

void test_season_two_streak_combo_and_surge() {
  Engine game(Board(10, 24), std::make_unique<SRS>(),
              std::make_unique<OnlyI>(), std::make_unique<TSpin>(),
              std::make_unique<Classical>(), std::make_unique<TetrioSS2>(),
              5, 3, 10, 0);
  game.reset(23);
  for (uint32_t clear = 0; clear < 6; ++clear) {
    require(game.spawn_next_piece(), "test I piece should spawn");
    fill_quad_well(game);
    require(game.hard_drop().lines_cleared == 4,
            "vertical I should clear a quad");
    require(game.back_to_back() && game.back_to_back_count() == clear &&
                game.combo() == clear,
            "consecutive difficult clears should grow B2B and combo");
    require(game.surge_charge() == (clear >= 4 ? clear : 0),
            "Season 2-like surge should begin at B2B x4 and grow per clear");
  }

  require(game.spawn_next_piece(), "no-clear I piece should spawn");
  require(game.hard_drop().lines_cleared == 0 && game.combo() == 0 &&
              game.back_to_back_count() == 5 && game.surge_charge() == 5,
          "a no-clear lock resets combo but preserves B2B and surge");

  require(game.spawn_next_piece() && game.set_edit_mode(true),
          "single-clear I piece should spawn into edit mode");
  for (uint16_t col = 0; col < 10; ++col)
    require(game.paint_cell(0, col, col >= 3 && col <= 6
                                       ? Cell::Empty : Cell::Garbage),
            "single-clear row should be paintable");
  require(game.paint_cell(1, 9, Cell::Garbage) && game.set_edit_mode(false),
          "the single should not be a perfect clear");
  LockResult released = game.hard_drop();
  require(released.lines_cleared == 1 && released.attack_sent == 5 &&
              released.attack_packets == std::vector<uint16_t>({2, 2, 1}) &&
              !game.back_to_back() && game.back_to_back_count() == 0 &&
              game.surge_charge() == 0 && game.combo() == 0,
          "a normal single should release three surge packets, then reset B2B and combo");
  require(TetrioSS2{}.garbage_to_send(AttackContext{
              .lines_cleared = 4, .perfect_clear = true, .combo = 5}) == 5 &&
              TetrioSS2{}.garbage_to_send(AttackContext{
                  .lines_cleared = 2, .spin = SpinType::Mini,
                  .piece = PieceType::O}) == 0,
          "Season 2 All Clears send five and non-T All-Minis send no base attack");
  require(TetrioSS2{}.garbage_packets(AttackContext{
              .lines_cleared = 4, .surge_on_break = 4}) ==
              std::vector<uint16_t>({4, 2, 1, 1}),
          "a clear attack and each segment of surge must be separate packets");
  require(TetrioSS1{}.surge_charge(5) == 0 &&
              TetrioSS1{}.garbage_to_send(AttackContext{
                  .lines_cleared = 1, .surge_on_break = 5}) == 0 &&
              TableAttack{}.surge_charge(5) == 0,
          "non-Season-2 attack schemes must not charge or send surge");
  game.reset(23);
  require(game.back_to_back_count() == 0 && game.combo() == 0 &&
              game.surge_charge() == 0,
          "reset must clear all visible streaks");
}

void test_season_two_non_t_minis_keep_b2b() {
  Engine game(Board(10, 24), std::make_unique<SRS>(),
              std::make_unique<OnlyI>(), std::make_unique<AlwaysMini>(),
              std::make_unique<Classical>(), std::make_unique<TetrioSS2>(),
              5, 3, 10, 0);
  game.reset(11);
  for (int i = 0; i < 2; ++i) {
    require(game.spawn_next_piece() && game.hard_drop().locked,
            "a mini spin without a clear should lock");
    require(game.back_to_back() && game.back_to_back_count() ==
                static_cast<uint32_t>(i) && game.combo() == 0 &&
                game.last_lock_result()->attack_sent == 0,
            "non-T All-Minis should grow B2B without combo or outgoing attack");
  }
}

void test_season_two_surge_routing() {
  std::vector<std::unique_ptr<Engine>> players;
  for (int i = 0; i < 2; ++i)
    players.push_back(std::make_unique<Engine>(
        Board(10, 40), std::make_unique<SRS>(), std::make_unique<OnlyI>(),
        std::make_unique<TSpin>(), std::make_unique<Classical>(),
        std::make_unique<TetrioSS2>(), 5, 3, 10, 0));
  Match match(std::move(players));
  match.start(23);
  for (int i = 0; i < 5; ++i) {
    fill_quad_well(match.player(0));
    require(match.hard_drop(0).lines_cleared == 4,
            "room player should build a five-quad surge");
  }
  require(match.player(0).surge_charge() == 4 &&
              match.player(1).pending_garbage_packets() == 5,
          "room players should maintain separate surge and garbage states");
  Engine &sender = match.player(0);
  require(sender.set_edit_mode(true), "room single setup should be editable");
  for (uint16_t col = 0; col < 10; ++col)
    if (col < 3 || col > 6)
      sender.paint_cell(0, col, Cell::Garbage);
  sender.paint_cell(1, 9, Cell::Garbage);
  require(sender.set_edit_mode(false), "room single setup should resume");
  sender.receive_garbage(1, 5);
  LockResult released = match.hard_drop(0);
  require(released.attack_sent == 3 &&
              released.attack_packets == std::vector<uint16_t>({1, 1, 1}) &&
              sender.pending_garbage_lines() == 0 &&
              match.player(1).pending_garbage_packets() == 8 &&
              match.player(1).pending_garbage_lines() == 28 &&
              match.player(1).surge_charge() == 0,
          "surge must cancel incoming first, then arrive as three separate attacks");
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
  require(crowded.pending_garbage_lines() == 25 && !crowded.game_over(),
          "garbage received between pieces should wait until a lock");
  require(crowded.spawn_next_piece() && crowded.set_edit_mode(true),
          "top-out test needs a falling piece and editor");
  crowded.paint_cell(23, 9, Cell::Garbage);
  require(crowded.set_edit_mode(false), "top-out test should resume");
  require(crowded.hard_drop().game_over && crowded.pending_garbage_lines() == 17,
          "a capped garbage rise should still top out when it pushes a block off-board");
}

void test_garbage_tanking_and_packet_order() {
  Engine game(Board(10, 40), std::make_unique<SRS>(),
              std::make_unique<OnlyI>(), std::make_unique<TSpin>(),
              std::make_unique<Classical>(), nullptr, 5, 3, 30, 0);
  game.receive_garbage(12, 3);
  game.receive_garbage(5, 7);
  require(game.board().max_height() == 0 && game.pending_garbage_packets() == 2,
          "incoming packets must remain queued even with no active piece");
  require(game.spawn_next_piece() && game.hard_drop().locked &&
              game.pending_garbage_lines() == 9 &&
              game.pending_garbage_packets() == 2 &&
              game.board().get(0, 3) == Cell::Empty &&
              game.board().get(0, 7) == Cell::Garbage,
          "the first placement must take only eight lines from the oldest packet");
  require(game.spawn_next_piece() && game.hard_drop().locked &&
              game.pending_garbage_lines() == 1 &&
              game.pending_garbage_packets() == 1 &&
              game.board().get(0, 7) == Cell::Empty &&
              game.board().get(4, 3) == Cell::Empty,
          "the second placement must finish the first packet before the next hole");
  require(game.spawn_next_piece() && game.hard_drop().locked &&
              game.pending_garbage_lines() == 0 &&
              game.board().get(0, 7) == Cell::Empty,
          "remaining packet fragments must rise on later placements");
  game.reset(5);
  require(game.pending_garbage_lines() == 0 && game.board().max_height() == 0,
          "a restart should clear both queued and risen garbage");
}

void test_garbage_travel_clear_and_cancellation() {
  Engine game(Board(10, 40), std::make_unique<SRS>(),
              std::make_unique<OnlyI>(), std::make_unique<TSpin>(),
              std::make_unique<Classical>(), std::make_unique<TetrioSS2>(),
              5, 3, 10, 0);
  game.set_garbage_delay(20);
  require(game.spawn_next_piece(), "travel test needs a piece");
  game.receive_garbage(12, 3);
  fill_quad_well(game);
  LockResult clear = game.hard_drop();
  require(clear.lines_cleared == 4 && clear.attack_sent == 0 &&
              game.pending_garbage_lines() == 7 && game.board().max_height() == 0,
          "a line clear should cancel pending garbage before travel ends, without rising");
  for (int i = 0; i < 19; ++i)
    game.tick();
  require(game.spawn_next_piece() && game.hard_drop().locked &&
              game.pending_garbage_lines() == 7 && game.board().max_height() < 8,
          "a non-clear before the travel delay ends must tank all queued lines");
  game.tick();
  require(game.spawn_next_piece() && game.hard_drop().locked &&
              game.pending_garbage_lines() == 0 && game.board().get(0, 3) == Cell::Empty &&
              game.board().get(0, 4) == Cell::Garbage,
          "ready garbage should rise on the next non-clearing placement");

  Engine clearing(Board(10, 40), std::make_unique<SRS>(),
                  std::make_unique<OnlyI>(), std::make_unique<TSpin>(),
                  std::make_unique<Classical>(), nullptr, 5, 3, 10, 0);
  require(clearing.spawn_next_piece() && clearing.set_edit_mode(true),
          "clearing test needs a piece and editable board");
  for (uint16_t col = 0; col < 10; ++col)
    if (col < 3 || col > 6)
      clearing.paint_cell(0, col, Cell::Garbage);
  clearing.paint_cell(1, 9, Cell::Garbage);
  require(clearing.set_edit_mode(false), "clearing test should resume");
  clearing.receive_garbage(10, 3);
  require(clearing.hard_drop().lines_cleared == 1 &&
              clearing.pending_garbage_lines() == 10 &&
              clearing.board().get(0, 4) == Cell::Empty,
          "a line clear with zero outgoing attack should still tank ready garbage");
  require(clearing.spawn_next_piece() && clearing.hard_drop().locked &&
              clearing.pending_garbage_lines() == 2,
          "the next non-clear should take a capped amount of the tanked packet");
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
  require(match.player(1).set_edit_mode(true), "top-out room should allow board setup");
  match.player(1).paint_cell(23, 9, Cell::Garbage);
  require(match.player(1).set_edit_mode(false), "room setup should resume");
  match.player(1).receive_garbage(25, 4);
  match.hard_drop(1);
  require(match.round_winner() == 0 && match.wins(0) == 1 &&
              !match.match_winner(),
          "a knocked-out player should award one round win");
  match.start(14);
  require(!match.round_winner() && !match.player(1).game_over() &&
              match.wins(0) == 1,
          "next round should reset boards but retain match wins");
  require(match.player(1).set_edit_mode(true), "second room round should allow setup");
  match.player(1).paint_cell(23, 9, Cell::Garbage);
  require(match.player(1).set_edit_mode(false), "second room round should resume");
  match.player(1).receive_garbage(25, 4);
  match.hard_drop(1);
  require(match.match_winner() == 0 && match.wins(0) == 2,
          "reaching the room win target should finish the match");

  std::vector<std::unique_ptr<Engine>> tied_players;
  for (int i = 0; i < 2; ++i)
    tied_players.push_back(std::make_unique<Engine>(make_game(Board(10, 24), 20, 0)));
  Match draw(std::move(tied_players));
  draw.start(19);
  for (int player = 0; player < 2; ++player) {
    require(draw.player(player).set_edit_mode(true), "draw setup should be editable");
    draw.player(player).paint_cell(23, 9, Cell::Garbage);
    require(draw.player(player).set_edit_mode(false), "draw setup should resume");
  }
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

void test_seeded_garbage_holes_are_symmetric_and_not_a_column_march() {
  std::vector<std::unique_ptr<Engine>> players;
  for (int i = 0; i < 4; ++i)
    players.push_back(std::make_unique<Engine>(
        Board(10, 200), std::make_unique<SRS>(), std::make_unique<OnlyI>(),
        std::make_unique<TSpin>(), std::make_unique<Classical>(),
        i < 2 ? std::make_unique<OneLineAttack>() : nullptr,
        5, 3, 180, 0));
  Match match(std::move(players));
  match.set_target(0, 2);
  match.set_target(1, 3);

  auto collect_holes = [&](uint64_t seed) {
    match.start(seed);
    std::vector<uint16_t> holes;
    for (int packet = 0; packet < 24; ++packet) {
      require(match.hard_drop(0).attack_sent == 1 &&
                  match.hard_drop(1).attack_sent == 1,
              "both senders must send one garbage packet per placement");
      require(match.hard_drop(2).locked && match.hard_drop(3).locked,
              "both recipients must be able to tank and receive each packet");
      int hole = -1;
      for (int col = 0; col < 10; ++col) {
        Cell left = match.player(2).board().get(0, col);
        Cell right = match.player(3).board().get(0, col);
        require(left == right, "the same packet should have the same hole for both players");
        if (left == Cell::Empty) {
          require(hole == -1, "a garbage line should have one hole");
          hole = col;
        } else {
          require(left == Cell::Garbage, "the bottom row should be garbage");
        }
      }
      require(hole != -1, "each received packet needs a hole");
      holes.push_back(static_cast<uint16_t>(hole));
    }
    return holes;
  };

  const auto first = collect_holes(42);
  require(collect_holes(42) == first,
          "restarting with the same seed must replay the garbage-hole sequence");
  require(collect_holes(43) != first,
          "different round seeds should produce different garbage-hole sequences");
  bool marches_right = true;
  for (size_t i = 1; i < first.size(); ++i)
    marches_right &= first[i] == (first[i - 1] + 1) % 10;
  require(!marches_right, "garbage holes should not march right one column per packet");
}
} // namespace

int main() {
  test_seeded_bags_and_kicks();
  test_lock_delay_and_reset();
  test_line_clear_score_and_attack();
  test_season_two_streak_combo_and_surge();
  test_season_two_non_t_minis_keep_b2b();
  test_season_two_surge_routing();
  test_spin_modes_and_dynamic_tables();
  test_garbage_cancel_and_topout();
  test_garbage_tanking_and_packet_order();
  test_garbage_travel_clear_and_cancellation();
  test_editor_pauses_and_rejects_colliding_exit();
  test_spawning_does_not_replace_a_falling_piece();
  test_solo_sprint_blitz_and_marathon();
  test_room_rounds_and_match_winner();
  test_multiplayer_attack_routing_and_same_seed();
  test_seeded_garbage_holes_are_symmetric_and_not_a_column_march();
  return 0;
}
