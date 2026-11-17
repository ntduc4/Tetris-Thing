#include "engine/test_support.hpp"

#include "engine/Engine.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace {

tetris::Engine make_game(
    std::unique_ptr<tetris::Randomizer> randomizer = std::make_unique<tetris::SevenBag>()) {
  tetris::AttackTable attacks;
  attacks.normal[1] = 3;
  return tetris::Engine(
      tetris::Board(10, 24), std::make_unique<tetris::SRSPlus>(),
      std::move(randomizer), std::make_unique<tetris::AllSpin>(),
      std::make_unique<tetris::Classical>(),
      std::make_unique<tetris::TableAttack>(attacks), 5, 3, 19, 0);
}

void test_generation_replay_and_atomic_rejection() {
  auto game = make_game();
  game.reset(42);
  require(game.spawn_next_piece(), "spawn for placement search");
  auto board = game.board().render();
  auto queue = game.preview_queue();
  auto active = game.active_piece();
  auto normal = game.generate_active_piece_placement();
  auto held = game.generate_hold_piece_placement();
  require(!normal.empty() && !held.empty(), "both placement branches must exist");
  require(normal.size() < 2000 && held.size() < 2000,
          "placement search should deduplicate equivalent endpoints");
  require(board == game.board().render() && queue == game.preview_queue() &&
              active == game.active_piece(),
          "enumeration must not mutate the live game or RNG");

  for (const auto &branch : {normal, held}) {
    std::set<std::tuple<int, int, int, tetris::SpinType>> outcomes;
    for (const auto &placement : branch) {
      require(!placement.moves.empty() &&
                  placement.moves.back() == tetris::Movement::HardDrop &&
                  std::count(placement.moves.begin(), placement.moves.end(),
                             tetris::Movement::HardDrop) == 1,
              "a placement must end in exactly one locking hard drop");
      auto preview = game.clone();
      auto result = preview.apply_placement(placement);
      require(result.locked && !preview.active_piece(),
              "every placement must replay and lock");
      require(result.score_delta == preview.score() - game.score(),
              "score delta must be the measured result of the lock");
      require(preview.last_lock_result()->spin == result.spin &&
                  preview.last_lock_result()->attack_sent == result.attack_sent,
              "spin and attack metadata must come from the engine");
      require(outcomes.insert({placement.final_pos.row, placement.final_pos.col,
                               static_cast<int>(placement.final_rotation),
                               result.spin}).second,
              "each pose and effective spin must occur once per hold branch");
    }
  }
  require(board == game.board().render() && queue == game.preview_queue(),
          "previewing all actions must not change live state");

  auto bad = normal.front();
  ++bad.final_pos.col;
  try {
    game.apply_placement(bad);
    require(false, "forged placement must be rejected");
  } catch (const std::invalid_argument &) {
  }
  bad = normal.front();
  bad.spin_context.last_movement = tetris::Movement::Right;
  try {
    game.apply_placement(bad);
    require(false, "forged pre-drop context must be rejected");
  } catch (const std::invalid_argument &) {
  }
  bad = normal.front();
  bad.moves.pop_back();
  try {
    game.apply_placement(bad);
    require(false, "a placement without a terminal hard drop must be rejected");
  } catch (const std::invalid_argument &) {
  }
  bad = held.front();
  bad.moves.push_back(tetris::Movement::HardDrop);
  try {
    game.apply_placement(bad);
    require(false, "a second hard drop must be rejected");
  } catch (const std::invalid_argument &) {
  }
  bad = held.front();
  bad.moves.insert(bad.moves.begin(), tetris::Movement::HardDrop);
  try {
    game.apply_placement(bad);
    require(false, "an early hard drop must be rejected");
  } catch (const std::invalid_argument &) {
  }
  require(board == game.board().render() && queue == game.preview_queue() &&
              active == game.active_piece() && !game.hold_piece() && game.can_hold(),
          "rejected placement must restore the live game, including RNG");

  require(game.hold(), "hold should work outside enumeration");
  require(game.generate_hold_piece_placement().empty(),
          "hold placements must respect the one-hold-per-piece gate");
}

void test_same_context_restore_and_repeated_rollouts() {
  auto game = make_game();
  game.reset(17);
  game.spawn_next_piece();
  auto checkpoint = game.clone();
  auto working = game.clone();
  auto actions = game.generate_hold_piece_placement();
  require(!actions.empty(), "hold search should use the visible next piece");

  auto reference = working.apply_placement(actions.front());
  require(working.spawn_next_piece(), "simulation should spawn the next piece");
  auto next_piece = working.active_piece();
  auto next_queue = working.preview_queue();
  auto next_board = working.board().render();
  for (int i = 0; i < 3; ++i) {
    working.restore(checkpoint);
    require(working.preview_queue() == checkpoint.preview_queue() &&
                working.active_piece() == checkpoint.active_piece() &&
                working.board().render() == checkpoint.board().render(),
            "restore must recover the full pre-action observation");
    auto result = working.apply_placement(actions.front());
    require(result.score_delta == reference.score_delta &&
                result.attack_sent == reference.attack_sent &&
                result.spin == reference.spin,
            "same action after restore must reproduce the lock");
    require(working.spawn_next_piece() &&
                working.active_piece() == next_piece &&
                working.preview_queue() == next_queue &&
                working.board().render() == next_board,
            "restore must reproduce RNG/bag state beyond the visible preview");
  }
  require(game.board().render() == checkpoint.board().render(),
          "rollout must leave original engine untouched");

  auto unrelated = make_game();
  try {
    unrelated.restore(checkpoint);
    require(false, "unrelated rule context must not be restored");
  } catch (const std::invalid_argument &) {
  }

  auto timed = make_game();
  timed.reset(17);
  timed.spawn_next_piece();
  timed.receive_garbage(3, 4);
  timed.tick();
  auto timed_checkpoint = timed.clone();
  require(timed.hold(), "hold should change state after checkpoint");
  auto pending_before = timed_checkpoint.pending_garbage();
  timed.apply_placement(timed.generate_active_piece_placement().front());
  timed.restore(timed_checkpoint);
  require(timed.tick_count() == timed_checkpoint.tick_count() &&
              timed.pending_garbage_lines() == 3 &&
              timed.pending_garbage()[0].delay_ticks == pending_before[0].delay_ticks &&
              timed.hold_piece() == timed_checkpoint.hold_piece() &&
              timed.can_hold() == timed_checkpoint.can_hold(),
          "restore must recover timing, incoming garbage and hold state");
  timed.set_attack_scheme(std::make_unique<tetris::JSTris>());
  try {
    timed.restore(timed_checkpoint);
    require(false, "rule changes must invalidate earlier checkpoints");
  } catch (const std::invalid_argument &) {
  }
}

void test_line_clear_attack_and_score() {
  auto game = make_game();
  for (unsigned seed = 0; seed < 100; ++seed) {
    game.reset(seed);
    if (game.preview_queue().front() == tetris::PieceType::I)
      break;
  }
  require(game.spawn_next_piece() &&
              game.active_piece()->piece.type() == tetris::PieceType::I,
          "need an I-piece to clear the test well");
  require(game.set_edit_mode(true), "test fixture may enter edit mode");
  for (int col = 0; col < 10; ++col)
    if (col < 3 || col > 6)
      require(game.paint_cell(0, col, tetris::Cell::Garbage), "paint test well");
  game.paint_cell(1, 9, tetris::Cell::Garbage); // Not a perfect clear.
  require(game.set_edit_mode(false), "test fixture may exit edit mode");

  bool found = false;
  for (const auto &placement : game.generate_active_piece_placement()) {
    auto branch = game.clone();
    auto result = branch.apply_placement(placement);
    if (result.lines_cleared == 1) {
      require(result.attack_sent == 3 && result.score_delta > 0 &&
                  result.spin == branch.last_lock_result()->spin,
              "placement outcome must expose real lines, attack, score and spin");
      found = true;
      break;
    }
  }
  require(found, "BFS should reach the one-line clear in the test board");
}

void paint_mini_single_well(tetris::Engine &game) {
  require(game.set_edit_mode(true), "enter edit mode for T mini fixture");
  for (int row = 0; row < game.board().get_height(); ++row)
    for (int col = 0; col < game.board().get_width(); ++col)
      require(game.paint_cell(row, col, tetris::Cell::Empty),
              "clear previous fixture");
  // An East-facing T at root (-1, 7) fills (0,8) and clears row zero.
  // Its lower corners are occupied; (2,7) is occupied and (2,9) is open,
  // so rotating it into place while grounded produces a T mini.
  for (int col = 0; col < 10; ++col)
    if (col != 8)
      require(game.paint_cell(0, col, tetris::Cell::Garbage),
              "paint mini single row");
  require(game.paint_cell(2, 7, tetris::Cell::Garbage) &&
              game.paint_cell(3, 0, tetris::Cell::Garbage),
          "paint mini corner and prevent perfect clear");
  require(game.set_edit_mode(false), "exit edit mode for T mini fixture");
}

void test_mini_spin_and_non_spin_at_same_pose_keep_distinct_b2b() {
  using namespace tetris;
  Engine game(Board(10, 24), std::make_unique<SRSPlus>(),
              std::make_unique<Random>(), std::make_unique<TSpin>(),
              std::make_unique<Classical>(), std::make_unique<TetrioSS2>(),
              5, 3, 19, 0);
  // Random bag permits two successive T pieces for a real B2B setup.
  bool found_seed = false;
  for (unsigned seed = 0; seed < 1000; ++seed) {
    game.reset(seed);
    auto next = game.preview_queue();
    if (next[0] == PieceType::T && next[1] == PieceType::T) {
      found_seed = true;
      break;
    }
  }
  require(found_seed && game.spawn_next_piece(), "need two successive T pieces");
  paint_mini_single_well(game);

  auto at_target = [](const Placement &p) {
    return p.final_pos == Position{-1, 7} &&
           p.final_rotation == Rotation::East;
  };
  bool established_b2b = false;
  for (const auto &p : game.generate_active_piece_placement()) {
    if (!at_target(p) || p.drop_distance != 0)
      continue;
    auto preview = game.clone();
    auto result = preview.apply_placement(p);
    if (result.spin != SpinType::Mini || result.lines_cleared != 1)
      continue;
    require(p.moves.size() >= 2 && p.moves.back() == Movement::HardDrop &&
                p.spin_context.last_movement == p.moves[p.moves.size() - 2],
            "spin metadata must retain the grounded rotation before locking");
    game.apply_placement(p);
    established_b2b = game.back_to_back();
    break;
  }
  require(established_b2b, "first T mini single should establish B2B");
  require(game.spawn_next_piece() &&
              game.active_piece()->piece.type() == PieceType::T,
          "second T piece should be available for the B2B comparison");
  paint_mini_single_well(game);

  std::optional<Placement> mini, ordinary;
  int mini_count = 0, ordinary_count = 0;
  for (const auto &p : game.generate_active_piece_placement()) {
    if (!at_target(p))
      continue;
    auto preview = game.clone();
    auto result = preview.apply_placement(p);
    if (result.spin == SpinType::Mini) {
      mini = p;
      ++mini_count;
    } else if (result.spin == SpinType::None) {
      ordinary = p;
      ++ordinary_count;
    }
  }
  require(mini_count == 1 && ordinary_count == 1,
          "same pose must retain one mini and one non-spin, not duplicate paths");
  require(mini->drop_distance == 0 && ordinary->drop_distance > 0,
          "grounded rotation should spin; moving hard drop should not");
  require(mini->moves.back() == Movement::HardDrop &&
              ordinary->moves.back() == Movement::HardDrop,
          "both outcomes must explicitly end with the lock command");
  require(ordinary->spin_context.last_movement == Movement::CW,
          "non-spin should retain the last pre-drop move, not erase it");

  auto preserved = game.clone();
  auto mini_result = preserved.apply_placement(*mini);
  auto broken = game.clone();
  auto ordinary_result = broken.apply_placement(*ordinary);
  require(mini_result.lines_cleared == 1 &&
              ordinary_result.lines_cleared == 1 &&
              mini_result.spin == SpinType::Mini &&
              ordinary_result.spin == SpinType::None,
          "both actions clear the same line, but only the spin scores a mini");
  require(preserved.back_to_back() && !broken.back_to_back(),
          "T mini single preserves B2B whereas an ordinary single breaks it");
  require(preserved.board().render() == broken.board().render(),
          "both actions have the same board outcome");
}

void test_hold_keeps_same_pose_and_spin_as_separate_action() {
  auto game = make_game(std::make_unique<tetris::Random>());
  bool found_seed = false;
  for (unsigned seed = 0; seed < 1000; ++seed) {
    game.reset(seed);
    auto next = game.preview_queue();
    if (next[0] == tetris::PieceType::T &&
        next[1] == tetris::PieceType::T) {
      found_seed = true;
      break;
    }
  }
  require(found_seed && game.spawn_next_piece(), "need same-type hold option");
  auto normal = game.generate_active_piece_placement().front();
  auto held = game.generate_hold_piece_placement().front();
  auto without_hold = game.clone();
  auto normal_result = without_hold.apply_placement(normal);
  auto with_hold = game.clone();
  auto held_result = with_hold.apply_placement(held);
  require(!normal.use_hold && held.use_hold &&
              normal.type == held.type &&
              normal.final_pos == held.final_pos &&
              normal.final_rotation == held.final_rotation &&
              normal_result.spin == held_result.spin &&
              without_hold.board().render() == with_hold.board().render(),
          "same-pose hold and non-hold actions may lock identically");
  require(!without_hold.hold_piece() &&
              with_hold.hold_piece() == tetris::PieceType::T &&
              without_hold.preview_queue() != with_hold.preview_queue(),
          "hold actions must remain distinct because their future state differs");
}

void test_all_builtin_bag_restores() {
  std::vector<std::unique_ptr<tetris::Randomizer>> bags;
  bags.push_back(std::make_unique<tetris::SevenBag>());
  bags.push_back(std::make_unique<tetris::FourteenBag>());
  bags.push_back(std::make_unique<tetris::Random>());
  for (auto &bag : bags) {
    auto game = make_game(std::move(bag));
    game.reset(99);
    game.spawn_next_piece();
    auto snapshot = game.clone();
    auto held = game.generate_hold_piece_placement();
    require(!held.empty(), "all built-in bags should allow hold placements");
    game.apply_placement(held.front());
    game.spawn_next_piece();
    auto next_piece = game.active_piece();
    auto queue = game.preview_queue();
    game.restore(snapshot);
    game.apply_placement(held.front());
    game.spawn_next_piece();
    require(game.active_piece() == next_piece && game.preview_queue() == queue,
            "restore must reproduce RNG state for every built-in bag type");
  }
}

} // namespace

int main() {
  test_generation_replay_and_atomic_rejection();
  test_same_context_restore_and_repeated_rollouts();
  test_line_clear_attack_and_score();
  test_mini_spin_and_non_spin_at_same_pose_keep_distinct_b2b();
  test_hold_keeps_same_pose_and_spin_as_separate_action();
  test_all_builtin_bag_restores();
  return 0;
}
