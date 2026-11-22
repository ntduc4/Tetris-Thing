#include "bot/MisaMinoBot.hpp"
#include "engine/test_support.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using namespace tetris;

Engine make_game(uint16_t height = 10, uint8_t preview = 5,
                 std::unique_ptr<Randomizer> bag = std::make_unique<SevenBag>()) {
  return Engine(Board(10, height), std::make_unique<SRSPlus>(),
                std::move(bag), std::make_unique<TSpin>(),
                std::make_unique<Classical>(), std::make_unique<TetrioSS2>(),
                preview, 3, std::min<int>(height - 4, 17), 0);
}

bool same_action(const Placement &a, const Placement &b) {
  return a.moves == b.moves && a.type == b.type &&
         a.final_pos == b.final_pos &&
         a.final_rotation == b.final_rotation && a.use_hold == b.use_hold &&
         a.drop_distance == b.drop_distance &&
         a.spin_context == b.spin_context && a.spin == b.spin;
}

void test_legal_replayable_and_no_live_mutation() {
  auto game = make_game();
  game.reset(31);
  require(game.spawn_next_piece(), "spawn bot test piece");
  MisaMinoBot bot({.search_depth = 3, .beam_width = 5, .node_budget = 1200});
  for (int turn = 0; turn < 8 && !game.game_over(); ++turn) {
    const auto before = game.clone();
    const auto choice = bot.choose(game);
    require(choice.has_value(), "bot must choose from the live legal actions");
    const auto repeat = bot.choose(game);
    require(repeat && same_action(*choice, *repeat),
            "bot should give a deterministic choice from unchanged state");
    require(game.board().render() == before.board().render() &&
                game.active_piece() == before.active_piece() &&
                game.hold_piece() == before.hold_piece() &&
                game.preview_queue() == before.preview_queue() &&
                game.can_hold() == before.can_hold() &&
                game.score() == before.score() &&
                game.pending_garbage_lines() == before.pending_garbage_lines(),
            "choosing must not mutate board, bag, hold or counters");
    auto legal = game.generate_active_piece_placement();
    auto held = game.generate_hold_piece_placement();
    legal.insert(legal.end(), held.begin(), held.end());
    require(std::any_of(legal.begin(), legal.end(),
                        [&](const Placement &p) { return same_action(p, *choice); }),
            "selected move must be an actual engine-generated path");
    const auto lock = game.apply_placement(*choice);
    require(lock.locked, "selected path must execute through the engine");
    if (!game.game_over())
      game.spawn_next_piece();
  }
}

void test_full_episode_matches_fresh_search() {
  MisaMinoBot bot({.search_depth = 3, .beam_width = 12, .node_budget = 4000});
  size_t total_reused = 0;
  for (uint64_t seed : {31u, 42u, 93u}) {
    auto episode_game = make_game();
    auto reference = make_game();
    episode_game.reset(seed);
    reference.reset(seed);
    require(reference.spawn_next_piece(), "reference game must start");
    const auto episode = bot.play_episode(episode_game, 12);
    for (const auto &turn : episode.turns) {
      const auto selected = bot.choose(reference);
      require(selected && same_action(*selected, turn.placement),
              "cached episode search must choose exactly as fresh search");
      const auto lock = reference.apply_placement(*selected);
      require(lock.locked && lock.spin == turn.lock.spin &&
                  lock.lines_cleared == turn.lock.lines_cleared &&
                  lock.attack_sent == turn.lock.attack_sent &&
                  lock.score_delta == turn.lock.score_delta,
              "episode must record actual engine lock results");
      if (!reference.game_over())
        reference.spawn_next_piece();
    }
    require(episode.game_over == reference.game_over() &&
                episode.truncated ==
                    (!reference.game_over() && episode.turns.size() == 12) &&
                episode_game.board().render() == reference.board().render() &&
                episode_game.preview_queue() == reference.preview_queue() &&
                episode_game.hold_piece() == reference.hold_piece() &&
                episode_game.score() == reference.score(),
            "episode must reproduce sequential engine play");
    total_reused += episode.reused_searches;
  }
  require(total_reused > 0, "episode must reuse previous beam searches");

  auto single = make_game();
  single.reset(17);
  const auto one = bot.play_episode(single, 1);
  require(one.turns.size() == 1 && (one.truncated || one.game_over),
          "piece cap must stop a generated episode");
  try {
    bot.play_episode(single, 0);
    require(false, "zero piece cap must be rejected");
  } catch (const std::invalid_argument &) {
  }
}

void test_parallel_search_matches_serial() {
  MisaMinoBot serial({.search_depth = 3, .beam_width = 12,
                      .node_budget = 4000, .search_threads = 1});
  MisaMinoBot parallel({.search_depth = 3, .beam_width = 12,
                        .node_budget = 4000, .search_threads = 4});
  for (uint16_t height : {10, 40}) {
    for (uint64_t seed : {3u, 31u, 42u, 93u}) {
      auto game = make_game(height);
      game.reset(seed);
      require(game.spawn_next_piece(), "parallel fixture must spawn");
      for (int turn = 0; turn < 4 && !game.game_over(); ++turn) {
        const auto a = serial.choose(game);
        const auto b = parallel.choose(game);
        require(a && b && same_action(*a, *b),
                "parallel search must keep the serial beam decision and path");
        auto legal = game.generate_active_piece_placement();
        if (game.can_hold()) {
          auto held = game.generate_hold_piece_placement();
          legal.insert(legal.end(), held.begin(), held.end());
        }
        const auto &executed = turn % 2 ? legal[legal.size() / 2] : *a;
        require(game.apply_placement(executed).locked,
                "teacher and learner-visited choices must lock");
        if (!game.game_over())
          game.spawn_next_piece();
      }
    }
  }
  auto game = make_game(40);
  game.reset(7);
  require(game.spawn_next_piece(), "budget fixture must spawn");
  MisaMinoBot limited_serial({.search_depth = 3, .beam_width = 12,
                              .node_budget = 100, .search_threads = 1});
  MisaMinoBot limited_parallel({.search_depth = 3, .beam_width = 12,
                                .node_budget = 100, .search_threads = 4});
  const auto a = limited_serial.choose(game);
  const auto b = limited_parallel.choose(game);
  require(a && b && same_action(*a, *b),
          "parallel search must discard the same incomplete layer");
}

void test_hold_gate_and_no_active_piece() {
  auto game = make_game();
  MisaMinoBot bot({.search_depth = 2, .beam_width = 3, .node_budget = 800});
  require(!bot.choose(game), "no active piece means no choice");
  game.reset(8);
  require(game.spawn_next_piece() && game.hold(), "fixture must allow a hold");
  const auto chosen = bot.choose(game);
  require(chosen && !chosen->use_hold,
          "a used hold cannot be requested again this turn");
  require(game.apply_placement(*chosen).locked, "post-hold action must lock");
  require(game.spawn_next_piece(), "next turn after hold should spawn");
  const auto next = bot.choose(game);
  require(next && game.apply_placement(*next).locked,
          "bot must also work with a populated hold slot");
  for (const auto bad : {MisaMinoConfig{0, 4, 500},
                          MisaMinoConfig{1, 0, 500},
                          MisaMinoConfig{1, 4, 0},
                          MisaMinoConfig{1, 4, 500, 0}}) {
    try {
      MisaMinoBot invalid(bad);
      require(false, "zero search limit must be rejected");
    } catch (const std::invalid_argument &) {
    }
  }
}

// The bot must not peek at an unseen piece, even when considering an empty
// hold after the last visible preview. Two different hidden tails are used.
class GuardedBag final : public Randomizer {
public:
  GuardedBag(PieceType hidden, std::shared_ptr<bool> exposed)
      : _hidden(hidden), _exposed(std::move(exposed)) {}
  void seed(uint64_t) override { _index = 0; }
  PieceType peek(uint8_t offset = 0) const override {
    if (_index + offset > 3)
      throw std::logic_error("bot peeked beyond the visible preview");
    if (_index + offset == 3) {
      *_exposed = true;
      return _hidden;
    }
    return _index + offset == 0 ? PieceType::T
           : _index + offset == 1 ? PieceType::O : PieceType::I;
  }
  std::vector<PieceType> preview(uint8_t amount) const override {
    std::vector<PieceType> result;
    for (uint8_t i = 0; i < amount; ++i)
      result.push_back(peek(i));
    return result;
  }
  PieceType pop() override {
    auto piece = peek();
    ++_index;
    return piece;
  }
  std::unique_ptr<Randomizer> clone() const override {
    return std::make_unique<GuardedBag>(*this);
  }
  void restore_from(const Randomizer &other) override {
    *this = dynamic_cast<const GuardedBag &>(other);
  }

private:
  PieceType _hidden;
  std::shared_ptr<bool> _exposed;
  size_t _index = 0;
};

void test_preview_boundary_and_hidden_garbage_holes() {
  auto left_exposed = std::make_shared<bool>(false);
  auto right_exposed = std::make_shared<bool>(false);
  auto left = make_game(10, 2, std::make_unique<GuardedBag>(
                                      PieceType::I, left_exposed));
  auto right = make_game(10, 2, std::make_unique<GuardedBag>(
                                       PieceType::Z, right_exposed));
  left.reset();
  right.reset();
  require(left.spawn_next_piece() && right.spawn_next_piece(),
          "both guarded games must spawn the same known piece");
  MisaMinoBot bot({.search_depth = 4, .beam_width = 4,
                   .node_budget = 800});
  auto a = bot.choose(left);
  auto b = bot.choose(right);
  require(a && b && same_action(*a, *b) &&
              !*left_exposed && !*right_exposed,
          "hidden pieces cannot affect the selected placement");

  // Ready garbage is resolved by Engine::apply_placement and can reveal the
  // hole on its clone. With incoming packets the bot stops at one ply and
  // evaluates the reconstructed, pre-rise public board instead.
  auto garbage_a = make_game();
  auto garbage_b = make_game();
  garbage_a.reset(98);
  garbage_b.reset(98);
  require(garbage_a.spawn_next_piece() && garbage_b.spawn_next_piece(),
          "garbage fixtures spawn identically");
  garbage_a.receive_garbage(3, 1);
  garbage_b.receive_garbage(3, 8);
  auto pick_a = bot.choose(garbage_a);
  auto pick_b = bot.choose(garbage_b);
  require(pick_a && pick_b && same_action(*pick_a, *pick_b),
          "hidden garbage holes must not change bot decision");
  require(garbage_a.pending_garbage_lines() == 3 &&
              garbage_b.pending_garbage_lines() == 3,
          "garbage lookahead must not mutate the live engine");
  const auto episode_a = bot.play_episode(garbage_a, 1);
  const auto episode_b = bot.play_episode(garbage_b, 1);
  require(episode_a.turns.size() == 1 && episode_b.turns.size() == 1 &&
              same_action(episode_a.turns[0].placement,
                          episode_b.turns[0].placement),
          "an episode must not choose based on secret garbage holes");
}

void paint_mini_well(Engine &game) {
  require(game.set_edit_mode(true), "fixture paint mode");
  for (int row = 0; row < game.board().get_height(); ++row)
    for (int col = 0; col < game.board().get_width(); ++col)
      require(game.paint_cell(row, col, Cell::Empty), "clear fixture board");
  for (int col = 0; col < 10; ++col)
    if (col != 8)
      require(game.paint_cell(0, col, Cell::Garbage), "paint T-mini row");
  require(game.paint_cell(2, 7, Cell::Garbage) &&
              game.paint_cell(3, 0, Cell::Garbage) &&
              game.set_edit_mode(false),
          "finish T-mini fixture");
}

void test_spin_and_nonspin_same_board() {
  // Same T-mini-single fixture as placement_search_tests. Hard-dropping the
  // same pose from above is a non-spin; a grounded rotation is a mini.
  // Disable attack scoring so the classification itself (not just outgoing
  // attack) is what makes the spin path attractive to the teacher.
  Engine game(Board(10, 24), std::make_unique<SRSPlus>(),
              std::make_unique<Random>(), std::make_unique<TSpin>(),
              std::make_unique<Classical>(), nullptr, 5, 3, 17, 0);
  bool found = false;
  for (int seed = 0; seed < 1000; ++seed) {
    game.reset(seed);
    if (game.preview_queue()[0] == PieceType::T) {
      found = true;
      break;
    }
  }
  require(found && game.spawn_next_piece(), "need a T for spin fixture");
  paint_mini_well(game);

  std::optional<Placement> mini, ordinary;
  for (const auto &placement : game.generate_active_piece_placement()) {
    if (placement.final_pos != Position{-1, 7} ||
        placement.final_rotation != Rotation::East)
      continue;
    auto test = game.clone();
    auto lock = test.apply_placement(placement);
    if (lock.spin == SpinType::Mini && lock.lines_cleared == 1)
      mini = placement;
    if (lock.spin == SpinType::None && lock.lines_cleared == 1)
      ordinary = placement;
  }
  require(mini && ordinary, "engine must offer both spin-sensitive paths");
  auto mini_result = game.clone();
  auto ordinary_result = game.clone();
  mini_result.apply_placement(*mini);
  ordinary_result.apply_placement(*ordinary);
  require(mini_result.board().render() == ordinary_result.board().render(),
          "same landing board should not collapse distinct spin outcomes");

  MisaMinoBot bot({.search_depth = 1, .beam_width = 5,
                   .node_budget = 1000});
  auto selected = bot.choose(game);
  require(selected && selected->type == PieceType::T,
          "spin fixture should have a legal T choice");
  auto simulation = game.clone();
  auto lock = simulation.apply_placement(*selected);
  require(lock.spin != SpinType::None && lock.lines_cleared > 0,
          "spin-aware search should prefer a real engine-classified spin clear");
}

void test_hold_t_spin_setup() {
  auto game = make_game(24, 5, std::make_unique<Random>());
  bool found = false;
  for (int seed = 0; seed < 1000; ++seed) {
    game.reset(seed);
    const auto preview = game.preview_queue();
    if (preview[0] == PieceType::T &&
        preview[1] != PieceType::T &&
        preview[2] != PieceType::T) {
      found = true;
      break;
    }
  }
  require(found && game.spawn_next_piece() && game.hold(),
          "save a T in hold while playing another piece");
  require(game.apply_placement(game.generate_active_piece_placement().front()).locked,
          "lock the held turn's piece");
  require(game.spawn_next_piece() && game.hold_piece() == PieceType::T &&
              game.can_hold() && game.active_piece()->piece.type() != PieceType::T,
          "prepare a non-T active piece with a T in hold");
  paint_mini_well(game);

  MisaMinoBot bot({.search_depth = 1, .beam_width = 5, .node_budget = 1000});
  auto choice = bot.choose(game);
  require(choice && choice->use_hold && choice->type == PieceType::T,
          "bot should retrieve held T for a spin clear");
  auto result = game.apply_placement(*choice);
  require(result.locked && result.spin != SpinType::None &&
              result.lines_cleared == 1,
          "held T-spin must be classified and cleared by the engine");
}

} // namespace

int main() {
  test_legal_replayable_and_no_live_mutation();
  test_full_episode_matches_fresh_search();
  test_parallel_search_matches_serial();
  test_hold_gate_and_no_active_piece();
  test_preview_boundary_and_hidden_garbage_holes();
  test_spin_and_nonspin_same_board();
  test_hold_t_spin_setup();
  return 0;
}
