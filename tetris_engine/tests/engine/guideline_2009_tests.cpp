#include "engine/test_support.hpp"

#include "engine/Engine.hpp"
#include "engine/Match.hpp"
#include "engine/SoloSession.hpp"
#include <array>
#include <memory>
#include <stdexcept>

namespace {
using namespace tetris;

class OnlyPiece : public Randomizer {
public:
  explicit OnlyPiece(PieceType type) : _type(type) {}
  void seed(uint64_t) override {}
  PieceType peek(uint8_t = 0) const override { return _type; }
  std::vector<PieceType> preview(uint8_t count) const override {
    return std::vector<PieceType>(count, _type);
  }
  PieceType pop() override { return _type; }
  std::unique_ptr<Randomizer> clone() const override {
    return std::make_unique<OnlyPiece>(*this);
  }
  void restore_from(const Randomizer &other) override {
    auto source = dynamic_cast<const OnlyPiece *>(&other);
    if (!source)
      throw std::invalid_argument("incompatible test randomizer");
    *this = *source;
  }
private:
  PieceType _type;
};

Engine make_game(PieceType piece, Board board = Board(10, 40)) {
  Engine game(board, std::make_unique<GuidelineSRS>(),
              std::make_unique<OnlyPiece>(piece),
              std::make_unique<GuidelineTSpin>(),
              std::make_unique<GuidelineScore>(),
              std::make_unique<GuidelineAttack>(),
              5, 3, 18, 0, 0, 0, 30, true);
  game.set_gravity(0);
  return game;
}

void test_rotation_and_spin() {
  const GuidelineSRS srs;
  require(srs.kick_offsets(PieceType::I, Rotation::North, Rotation::East) ==
              std::vector<Offset>({{0, 0}, {0, -2}, {0, 1}, {-1, -2}, {2, 1}}) &&
          srs.kick_offsets(PieceType::T, Rotation::North, Rotation::East) ==
              std::vector<Offset>({{0, 0}, {0, -1}, {1, -1}, {-2, 0}, {-2, -1}}) &&
          srs.kick_offsets(PieceType::I, Rotation::North, Rotation::South).empty() &&
          !srs.allows_half_turn(), "Guideline SRS must use five ordered 90-degree tests");
  Engine game = make_game(PieceType::T);
  require(game.spawn_next_piece() && !game.try_rotate_180() &&
              game.try_rotate_cw() && game.try_rotate_ccw(),
          "a Guideline game accepts CW/CCW but never a half turn");
  Board empty(10, 40);
  ActivePiece t = spawn_from_piece_type(PieceType::T, 17, 3);
  require(GuidelineTSpin{}.detect(empty, t, {
              .last_movement = Movement::HalfRotation}) == SpinType::None,
          "180 input cannot score a 2009 T-spin");
  Board mini_slot(10, 40);
  mini_slot.set(5, 3, Cell::J);
  mini_slot.set(3, 3, Cell::J);
  mini_slot.set(3, 5, Cell::J);
  t = spawn_from_piece_type(PieceType::T, 2, 3);
  t.piece.rotate_cw();
  require(GuidelineTSpin{}.detect(mini_slot, t, {
              .last_movement = Movement::CW}) == SpinType::Mini &&
          GuidelineTSpin{}.detect(mini_slot, t, {
              .last_movement = Movement::CW,
              .guideline_point_five = true}) == SpinType::Full,
          "a fifth kick promotes later uninterrupted rotations to a full T-spin");

  Engine kick_game = make_game(PieceType::T);
  require(kick_game.spawn_next_piece() && kick_game.set_edit_mode(true),
          "fifth kick setup must spawn and enable editing");
  for (Position cell : {Position{18, 4}, Position{20, 3},
                        Position{16, 2}, Position{16, 4}})
    require(kick_game.paint_cell(cell.row, cell.col, Cell::J),
            "fifth kick obstruction must be paintable");
  require(kick_game.set_edit_mode(false),
          "fifth kick obstruction must not overlap the active piece");
  require(kick_game.try_rotate_cw() &&
              kick_game.active_piece()->pos == Position{15, 2},
          "fifth kick must be required to clear the obstruction");
  require(kick_game.try_rotate_ccw(),
          "the T should rotate again without leaving the slot");
  require(kick_game.hard_drop().spin == SpinType::Full,
          "a fifth-point kick must remain full after another rotation");
}

void test_generation_and_block_out() {
  for (auto [type, expected_row] :
       {std::pair{PieceType::I, 17}, std::pair{PieceType::T, 17},
        std::pair{PieceType::O, 18}, std::pair{PieceType::J, 17},
        std::pair{PieceType::L, 17}, std::pair{PieceType::S, 17},
        std::pair{PieceType::Z, 17}}) {
    Engine game = make_game(type);
    require(game.spawn_next_piece() &&
                game.active_piece()->pos.row == expected_row &&
                game.active_piece()->piece.rotation() == Rotation::North,
            "initial generation must drop one unobstructed row");
  }
  Board blocked(10, 40);
  blocked.set(21, 4, Cell::J); // T's generation cell, but not its lower pose.
  Engine block_out = make_game(PieceType::T, blocked);
  require(!block_out.spawn_next_piece() && block_out.game_over(),
          "a blocked generation cell is Block Out even if the next row is free");

  Board ledge(10, 40);
  ledge.set(19, 3, Cell::J); // Blocks only the immediate generation drop.
  Engine no_initial_drop = make_game(PieceType::T, ledge);
  require(no_initial_drop.spawn_next_piece() &&
              no_initial_drop.active_piece()->pos.row == 18 &&
              !no_initial_drop.game_over(),
          "obstructed initial descent must not Block Out a valid spawn");
}

void test_skyline_lock_out() {
  Board above(10, 40);
  above.set(19, 4, Cell::J);
  above.set(19, 5, Cell::J);
  Engine lock_out = make_game(PieceType::O, above);
  require(lock_out.spawn_next_piece() &&
              lock_out.hard_drop().game_over && lock_out.game_over(),
          "locking all four cells in the buffer zone is Lock Out");

  Board peeking(10, 40);
  peeking.set(18, 4, Cell::J);
  peeking.set(18, 5, Cell::J);
  Engine still_playing = make_game(PieceType::O, peeking);
  require(still_playing.spawn_next_piece() &&
              still_playing.hard_drop().locked && !still_playing.game_over(),
          "locking across the skyline is not Lock Out");
}

void test_guideline_scoring() {
  GuidelineScore score;
  require(score.score({.lines_cleared = 1, .level = 3}) == 300 &&
              score.score({.lines_cleared = 4, .back_to_back = true,
                           .level = 3}) == 3600 &&
              score.score({.spin = SpinType::Mini, .level = 3}) == 300 &&
              score.score({.lines_cleared = 1, .spin = SpinType::Mini,
                           .back_to_back = true, .level = 3}) == 900 &&
              score.score({.lines_cleared = 2, .spin = SpinType::Full,
                           .level = 3}) == 3600,
          "Guideline scores scale by level, with B2B on qualifying clears only");
  require(score.score({.lines_cleared = 1, .perfect_clear = true,
                       .combo = 10, .hard_drop_cells = 4,
                       .soft_drop_cells = 2, .level = 2}) == 210,
          "drop points stay unscaled; all clears and combos add no 2009 points");
  Engine game = make_game(PieceType::I);
  game.set_level(3);
  require(game.level() == 3 && game.spawn_next_piece(),
          "the selected scoring level is observable");
  auto snapshot = game.clone();
  game.set_level(4);
  game.restore(snapshot);
  require(game.level() == 3, "restore must include the scoring level");

  Engine gravity = make_game(PieceType::I);
  gravity.set_level(1);
  require(gravity.spawn_next_piece(), "level-one gravity test should spawn");
  const auto start_row = gravity.active_piece()->pos.row;
  for (int tick = 0; tick < 59; ++tick)
    require(!gravity.tick().moved_down, "level one falls no faster than 1 row/s");
  require(gravity.tick().moved_down &&
              gravity.active_piece()->pos.row == start_row - 1,
          "level-one fall speed is one row per sixty ticks");
  gravity.reset(1);
  gravity.set_level(15);
  require(gravity.spawn_next_piece() && gravity.tick().moved_down &&
              gravity.active_piece()->pos.row <= start_row - 2,
          "level fifteen falls multiple rows per tick");
}

void test_hold_restarts_at_generation() {
  Engine game = make_game(PieceType::T);
  require(game.spawn_next_piece() && game.try_rotate_cw() && game.hold() &&
              game.active_piece()->piece.rotation() == Rotation::North &&
              game.active_piece()->pos.row == 17 && !game.can_hold() &&
              !game.hold(),
          "holding regenerates a north-facing piece and cannot repeat until lock");
  require(game.hard_drop().locked && game.spawn_next_piece() &&
              game.hold() && game.active_piece()->pos.row == 17,
          "the held piece regenerates after the next lock");

  Engine blocked_hold = make_game(PieceType::T);
  require(blocked_hold.spawn_next_piece() &&
              blocked_hold.set_edit_mode(true) &&
              blocked_hold.paint_cell(21, 4, Cell::J) &&
              blocked_hold.set_edit_mode(false),
          "a held T can be blocked at its generation position");
  Placement illegal_hold{.moves = {Movement::HardDrop},
                         .type = PieceType::T,
                         .use_hold = true};
  bool rejected = false;
  try {
    blocked_hold.apply_placement(illegal_hold);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected && !blocked_hold.game_over() && blocked_hold.can_hold() &&
              blocked_hold.active_piece().has_value(),
          "an invalid hold placement must roll back its Block Out");
  require(!blocked_hold.hold() && blocked_hold.game_over(),
          "a hold that generates into occupied cells is Block Out");
}

void test_extended_lock_down() {
  Engine game = make_game(PieceType::T);
  require(game.spawn_next_piece() && game.try_soft_drop(-1) &&
              game.board().grounded(*game.active_piece()),
          "piece must land before its lock-down timer starts");
  require(!game.tick().locked, "one grounded tick must not lock");
  for (int move = 0; move < 15; ++move)
    require(move % 2 ? game.try_move_right() : game.try_move_left(),
            "grounded horizontal movement should succeed");
  require(game.tick().locked,
          "extended lock down must exhaust its 15 grounded move resets");
}

void test_extended_lock_resets_on_new_low() {
  Board ledge(10, 40);
  for (int col : {3, 4, 5})
    ledge.set(0, col, Cell::J);
  Engine game = make_game(PieceType::T, ledge);
  require(game.spawn_next_piece() && game.try_soft_drop(-1),
          "a T can land on the raised ledge");
  for (int move = 0; move < 14; ++move)
    require(move % 2 ? game.try_move_right() : game.try_move_left(),
            "a grounded piece should use move resets on the ledge");
  require(game.try_move_right() && game.try_move_right() &&
              game.try_move_right() && !game.board().grounded(*game.active_piece()),
          "exhausted lock budget still permits movement while airborne");
  game.set_gravity(1);
  require(game.tick().moved_down && !game.game_over() &&
              game.active_piece().has_value(),
          "falling below the previous lowest row renews the reset budget");
  require(game.try_move_right() && !game.tick().locked,
          "the renewed budget prevents an immediate lock on landing");
}

void test_airborne_moves_exhaust_lock_budget() {
  Board ledge(10, 40);
  for (int col : {3, 4, 5})
    ledge.set(0, col, Cell::J);
  Engine game = make_game(PieceType::T, ledge);
  require(game.spawn_next_piece() && game.try_soft_drop(-1),
          "a T can land on the ledge");
  for (int move = 0; move < 10; ++move)
    require(move % 2 ? game.try_move_right() : game.try_move_left(),
            "ten grounded moves should spend ten of the 15 resets");
  for (int i = 0; i < 4; ++i)
    require(game.try_move_right(), "the piece can leave the ledge");
  require(!game.board().grounded(*game.active_piece()) &&
              game.try_move_left() && !game.board().grounded(*game.active_piece()),
          "the fifteenth movement can happen after the first landing, while airborne");
  require(game.try_move_left(3) && game.board().grounded(*game.active_piece()) &&
              game.tick().locked,
          "returning to the same height with no moves left must lock immediately");
}

void test_rotation_does_not_fake_a_new_low() {
  Board ledge(10, 40);
  ledge.set(0, 3, Cell::J);
  Engine game = make_game(PieceType::T, ledge);
  require(game.spawn_next_piece() && game.try_soft_drop(-1),
          "a T can land one row above the floor");
  for (int move = 0; move < 14; ++move)
    require(move % 2 ? game.try_move_right() : game.try_move_left(),
            "fourteen moves should spend fourteen lock resets");
  const int row_before = game.active_piece()->pos.row;
  require(game.try_rotate_cw() &&
              game.active_piece()->pos.row == row_before &&
              game.board().grounded(*game.active_piece()) &&
              game.tick().locked,
          "a changed shape at the same root row must not renew the lock budget");
}

void test_fixed_goal_marathon() {
  Engine game = make_game(PieceType::I);
  game.set_level(4);
  SoloSession session(std::move(game));
  auto quad = [&] {
    Engine &engine = session.engine();
    require(engine.set_edit_mode(true), "marathon setup should be editable");
    for (uint16_t row = 0; row < 4; ++row)
      for (uint16_t col = 0; col < 10; ++col)
        if (col != 5)
          require(engine.paint_cell(row, col, Cell::J),
                  "quad setup should be paintable");
    require(engine.set_edit_mode(false) && engine.try_rotate_cw(),
            "an I should turn into the well");
    require(session.hard_drop().lines_cleared == 4,
            "Marathon must count all four cleared lines");
  };
  session.start(1);
  for (int i = 0; i < 9; ++i)
    quad();
  require(session.engine().level() == 4, "starting at level 4 needs 40 lines");
  quad();
  require(session.engine().level() == 5 && !session.finished(),
          "Marathon advances after reaching the level's fixed goal");
  session.start(1);
  require(session.engine().level() == 4 && session.engine().lines_cleared() == 0,
          "restarting restores the selected starting level");
  for (int i = 0; i < 37; ++i)
    quad();
  require(session.engine().level() == 15 &&
              session.engine().lines_cleared() == 148 && !session.finished(),
          "fixed-goal Marathon is still running before 150 lines");
  quad();
  require(session.finished() && session.succeeded() &&
              session.engine().lines_cleared() == 152,
          "Marathon completes after crossing the 150-line goal");
}

void test_counter_attack_and_eight_line_holes() {
  Board well(10, 40);
  for (uint16_t row = 0; row < 4; ++row)
    for (uint16_t col = 0; col < 10; ++col)
      if (col != 5)
        well.set(row, col, Cell::J);
  Engine counter = make_game(PieceType::I, well);
  require(counter.spawn_next_piece() && counter.try_rotate_cw(),
          "vertical I must fit the four-row well");
  counter.receive_garbage(6, 3);
  const LockResult clear = counter.hard_drop();
  require(clear.lines_cleared == 4 && clear.attack_sent == 0 &&
              counter.pending_garbage_lines() == 0 &&
              counter.board().get(0, 3) == Cell::Empty &&
              counter.board().get(1, 3) == Cell::Empty &&
              counter.board().get(0, 4) == Cell::Garbage,
          "counter first, then rise remaining ready lines even after a clear");

  Engine random = make_game(PieceType::T);
  require(random.spawn_next_piece(), "a live piece must receive incoming lines");
  random.receive_random_garbage(17);
  auto snapshot = random.clone();
  require(random.pending_garbage_lines() == 17 &&
              random.hard_drop().locked && !random.game_over(),
          "all ready Guideline lines rise after a lock, without an eight-line cap");
  auto hole_at = [&](int row) {
    int hole = -1;
    for (int col = 0; col < 10; ++col)
      if (random.board().get(row, col) == Cell::Empty)
        hole = col;
    return hole;
  };
  const int last = hole_at(0), middle = hole_at(1), first = hole_at(9);
  require(last >= 0 && middle >= 0 && first >= 0 &&
              last != middle && middle != first &&
              hole_at(8) == middle && hole_at(16) == first,
          "new random holes occur only after each eight lines actually rise");
  const auto risen = random.board().render();
  random.restore(snapshot);
  require(random.hard_drop().locked && random.board().render() == risen,
          "restoring a Guideline state restores its garbage RNG and queue");
}

void test_multiplayer_broadcast_and_top_out() {
  std::vector<std::unique_ptr<Engine>> players;
  for (int i = 0; i < 3; ++i)
    players.push_back(std::make_unique<Engine>(make_game(PieceType::I)));
  Match match(std::move(players));
  match.start(7);
  Engine &sender = match.player(0);
  require(sender.set_edit_mode(true), "sender's well can be edited");
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 10; ++col)
      if (col != 5)
        require(sender.paint_cell(row, col, Cell::J), "fill the four-row well");
  require(sender.set_edit_mode(false) && sender.try_rotate_cw(),
          "I can turn into the attack well");
  require(match.hard_drop(0).attack_sent == 4 &&
              match.player(1).pending_garbage_lines() == 4 &&
              match.player(2).pending_garbage_lines() == 4 &&
              match.player(1).pending_garbage()[0].random_hole &&
              match.player(2).pending_garbage()[0].random_hole,
          "untargeted Guideline attack reaches every other player with rising holes");
  require(match.hard_drop(1).locked && match.hard_drop(2).locked &&
              match.player(1).board().render() ==
                  match.player(2).board().render(),
          "broadcast lines and holes match when players receive the same attacks");
  match.set_target(0, 1);
  require(sender.set_edit_mode(true), "targeted attack well can be edited");
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 10; ++col)
      if (col != 5)
        require(sender.paint_cell(row, col, Cell::J), "fill targeted attack well");
  require(sender.set_edit_mode(false) && sender.try_rotate_cw() &&
              match.hard_drop(0).attack_sent == 5 &&
              match.player(1).pending_garbage_lines() == 5 &&
              match.player(2).pending_garbage_lines() == 0,
          "an explicit target overrides the Guideline broadcast");

  std::vector<std::unique_ptr<Engine>> blocked_players;
  for (int i = 0; i < 2; ++i)
    blocked_players.push_back(std::make_unique<Engine>(make_game(PieceType::I)));
  Match blocked_match(std::move(blocked_players));
  blocked_match.start(8);
  Engine &blocked = blocked_match.player(0);
  require(blocked.set_edit_mode(true) &&
              blocked.paint_cell(20, 5, Cell::J) &&
              blocked.set_edit_mode(false),
          "the I can block its next generation cell");
  require(blocked_match.hard_drop(0).game_over &&
              blocked.game_over() && blocked_match.round_over(),
          "Match reports Block Out on the generation after a lock");

  Engine top_out = make_game(PieceType::T);
  require(top_out.spawn_next_piece() && top_out.set_edit_mode(true) &&
              top_out.paint_cell(39, 0, Cell::J) &&
              top_out.set_edit_mode(false),
          "a stack can reach the top of the twenty-row buffer");
  top_out.receive_random_garbage(1);
  require(top_out.hard_drop().game_over && top_out.game_over(),
          "garbage pushing a block above row 39 causes Top Out");
}
} // namespace

int main() {
  test_rotation_and_spin();
  test_generation_and_block_out();
  test_skyline_lock_out();
  test_guideline_scoring();
  test_hold_restarts_at_generation();
  test_extended_lock_down();
  test_extended_lock_resets_on_new_low();
  test_airborne_moves_exhaust_lock_budget();
  test_rotation_does_not_fake_a_new_low();
  test_fixed_goal_marathon();
  test_counter_attack_and_eight_line_holes();
  test_multiplayer_broadcast_and_top_out();
}
