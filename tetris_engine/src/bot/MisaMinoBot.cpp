#include "bot/MisaMinoBot.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tetris {
namespace {

// These are deliberately engine-specific weights, not the original bot's
// tuned TOJ/22-row parameter table. Preserve the *strategy*: distinguish
// buried from side-accessible holes and their depth, evaluate the resulting
// stack, attack efficiency, hold, B2B and actual spins across a bounded beam.
double stack_value(const Board &board, std::optional<PieceType> hold, bool b2b,
                   uint32_t combo, int attack, int lines, double spin_value,
                   int pending, bool terminal, int t_distance) {
  const int width = board.get_width();
  const int height = board.get_height();
  std::vector<int> heights(width);
  std::vector<uint8_t> occupied(width * height);
  double hole_cost = 0;
  int transitions = 0;
  int aggregate = 0;
  int max_height = 0;
  const int visible_height = std::min<int>(height, VisibleHeight);
  // Scoring probes the same board cells for heights, holes, transitions and
  // T cavities. Scan once instead of repeatedly calling Board::get and
  // Board::max_height for every simulated candidate.
  for (int row = 0; row < height; ++row)
    for (int col = 0; col < width; ++col) {
      const bool filled = board.get(row, col) != Cell::Empty;
      occupied[row * width + col] = filled;
      if (filled) {
        heights[col] = row + 1;
        max_height = row + 1;
      }
    }
  auto filled = [&](int row, int col) {
    return occupied[row * width + col] != 0;
  };
  for (int col = 0; col < width; ++col)
    aggregate += heights[col];
  for (int col = 0; col < width; ++col) {
    bool previous_hole = false;
    for (int row = 0; row < heights[col]; ++row) {
      if (filled(row, col)) {
        previous_hole = false;
        continue;
      }
      // A side approach requires more than a single shallow neighbor;
      // consecutive buried gaps are one cavity, not independent openings.
      const bool open_left = col > 1 && heights[col - 1] <= row + 1 &&
                             heights[col - 2] <= row + 1;
      const bool open_right = col + 2 < width &&
                              heights[col + 1] <= row + 1 &&
                              heights[col + 2] <= row + 1;
      const double depth_factor =
          1.0 + 2.0 * std::max(0, visible_height - row) / visible_height;
      if (open_left || open_right) {
        hole_cost += 14.0 * depth_factor;
        previous_hole = false;
      } else {
        hole_cost += (previous_hole ? 9.0 : 36.0) * depth_factor;
        previous_hole = true;
      }
    }
  }
  int bumpiness = 0;
  for (int col = 1; col < width; ++col)
    bumpiness += std::abs(heights[col] - heights[col - 1]);
  for (int row = 0; row < max_height; ++row) {
    bool previous = true; // Filled side walls.
    for (int col = 0; col < width; ++col) {
      const bool cell_occupied = filled(row, col);
      transitions += previous != cell_occupied;
      previous = cell_occupied;
    }
    transitions += !previous;
  }

  const int danger = std::max(0, max_height - height + 4);
  double value =
      -hole_cost - 1.7 * transitions - 2.2 * bumpiness -
      0.8 * aggregate - 3.0 * max_height * max_height / height -
      35.0 * danger * danger + 27.0 * attack + 7.0 * lines + spin_value +
      (b2b ? 8.0 : 0.0) + 3.0 * std::min(combo, 8u) - 12.0 * pending;
  if (hold == PieceType::T)
    value += 12.0;
  else if (hold == PieceType::I)
    value += 8.0;

  // Reward a shallow T-shaped cavity only when a T is in the visible future
  // or hold. This is a setup hint, NOT a spin classifier or a proof of
  // reachability.
  if (t_distance >= 0 && t_distance < 4) {
    for (int row = 1; row + 2 < height; ++row) {
      for (int col = 1; col + 1 < width; ++col) {
        if (filled(row, col) || filled(row - 1, col - 1) ||
            filled(row - 1, col) || filled(row - 1, col + 1) ||
            !filled(row, col - 1) || !filled(row, col + 1) ||
            filled(row + 1, col))
          continue;
        // Support the shoulders without encouraging an unfillable deep hole.
        if (row > 1 && !filled(row - 2, col))
          continue;
        value += 18.0 / (t_distance + 1);
      }
    }
  }
  if (terminal)
    value -= 100000.0;
  return value;
}

double spin_reward(const Placement &action, const LockResult &lock) {
  if (lock.spin == SpinType::None || lock.lines_cleared == 0)
    return 0;
  if (action.type == PieceType::T)
    return lock.spin == SpinType::Full ? 65.0 + 10.0 * lock.lines_cleared
                                       : 35.0 + 5.0 * lock.lines_cleared;
  return lock.spin == SpinType::Full ? 30.0 : 18.0;
}

// Do not request a hold with an empty slot if the next piece is not visible.
std::vector<Placement> actions(const Engine &game, size_t used,
                               size_t preview_size) {
  auto result = game.generate_active_piece_placement();
  if (game.can_hold() && (game.hold_piece() || used < preview_size)) {
    auto held = game.generate_hold_piece_placement();
    result.insert(result.end(), std::make_move_iterator(held.begin()),
                  std::make_move_iterator(held.end()));
  }
  return result;
}

int distance_to_t(std::optional<PieceType> hold,
                  const std::vector<PieceType> &visible, size_t used) {
  if (hold == PieceType::T)
    return 0;
  for (size_t i = used; i < visible.size(); ++i)
    if (visible[i] == PieceType::T)
      return static_cast<int>(i - used + 1);
  return -1;
}

// Include the board and all state that affects the continuation or its
// evaluation. This never reads the RNG or the preview beyond the root snapshot.
std::string state_key(const Engine &game, size_t used, int attack, int lines,
                      double spins) {
  std::string key;
  const auto &board = game.board();
  key.reserve(board.get_width() * board.get_height() + 80);
  for (int row = 0; row < board.get_height(); ++row)
    for (int col = 0; col < board.get_width(); ++col)
      key.push_back(static_cast<char>(board.get(row, col)));
  key.push_back(static_cast<char>(
      game.hold_piece() ? static_cast<int>(*game.hold_piece()) + 1 : 0));
  key.push_back(static_cast<char>(
      game.active_piece()
          ? static_cast<int>(game.active_piece()->piece.type()) + 1
          : 0));
  key.push_back(static_cast<char>(game.can_hold()));
  key += ":" + std::to_string(used) + ":" + std::to_string(game.combo()) + ":" +
         std::to_string(game.back_to_back()) + ":" +
         std::to_string(game.back_to_back_count()) + ":" +
         std::to_string(game.pending_garbage_lines()) + ":" +
         std::to_string(attack) + ":" + std::to_string(lines) + ":" +
         std::to_string(static_cast<int>(spins));
  return key;
}

struct Node {
  Engine game;
  size_t used = 0;  // Pieces consumed from the root's visible preview.
  size_t first = 0; // Index in the root's engine-generated actions.
  int attack = 0;
  int lines = 0;
  double spins = 0;
  double score = 0;
  bool terminal = false;
};

struct Candidate {
  std::optional<Placement> action; // Empty when carrying a terminal leaf.
  size_t parent = 0;
  size_t used = 0;
  size_t first = 0;
  int attack = 0;
  int lines = 0;
  double spins = 0;
  double score = 0;
  bool terminal = false;
};

using PlacementCache = std::unordered_map<std::string, std::vector<Placement>>;

// Placement reachability depends on the current board/piece/hold and, for
// empty hold, the next known piece. All states created by this episode search
// are freshly spawned, so they have the same initial movement/spin context.
// Never inspect a simulated state's preview: it may contain pieces not visible
// at the root of this search.
std::string placement_key(const Engine &game,
                          const std::vector<PieceType> &visible, size_t used) {
  const auto &board = game.board();
  std::string key;
  key.reserve(board.get_width() * board.get_height() + 100);
  for (int row = 0; row < board.get_height(); ++row)
    for (int col = 0; col < board.get_width(); ++col)
      key.push_back(static_cast<char>(board.get(row, col)));
  const auto &active = *game.active_piece();
  key += ":" + std::to_string(board.get_width()) + ":" +
         std::to_string(board.get_height()) + ":" +
         std::to_string(static_cast<int>(active.piece.type())) + ":" +
         std::to_string(static_cast<int>(active.piece.rotation())) + ":" +
         std::to_string(active.pos.row) + ":" +
         std::to_string(active.pos.col) + ":" +
         std::to_string(game.can_hold()) + ":" +
         std::to_string(game.hold_piece()
                            ? static_cast<int>(*game.hold_piece()) + 1
                            : 0) +
         ":" + std::to_string(game.combo()) + ":" +
         std::to_string(game.back_to_back_count()) + ":" +
         std::to_string(game.pending_garbage_lines()) + ":" +
         std::to_string(game.score());
  if (game.can_hold() && !game.hold_piece()) {
    key += ":" + std::to_string(used < visible.size()
                                    ? static_cast<int>(visible[used]) + 1
                                    : 0);
  }
  return key;
}

std::optional<Placement> choose_with_cache(const Engine &engine,
                                           const MisaMinoConfig &config,
                                           const PlacementCache *previous,
                                           PlacementCache *current,
                                           size_t *reused_searches) {
  if (engine.game_over() || !engine.active_piece() || engine.edit_mode())
    return std::nullopt;

  // Snapshot once. Neither later Engine::preview_queue() calls nor a hold at
  // the edge of the preview may reveal a new piece to this search.
  const auto visible = engine.preview_queue();
  auto legal_actions = [&](const Engine &game, size_t used,
                           std::vector<Placement> &uncached)
      -> const std::vector<Placement> & {
    if (!current) {
      uncached = actions(game, used, visible.size());
      return uncached;
    }
    auto key = placement_key(game, visible, used);
    auto [it, inserted] = current->try_emplace(std::move(key));
    if (inserted) {
      if (previous) {
        auto hit = previous->find(it->first);
        if (hit != previous->end()) {
          it->second = hit->second;
          ++*reused_searches;
          return it->second;
        }
      }
      it->second = actions(game, used, visible.size());
    }
    return it->second;
  };
  std::vector<Placement> root_generated;
  const auto &root_actions = legal_actions(engine, 0, root_generated);
  if (root_actions.empty())
    return std::nullopt;

  std::vector<Node> frontier;
  frontier.push_back({engine.clone()});
  size_t expanded = 0;
  size_t best_first = 0;
  // A garbage rise can expose a hidden packet hole on the cloned board. With
  // incoming garbage, evaluate the pre-rise locked board and stop at one ply.
  const bool pending = engine.pending_garbage_lines() != 0;
  // When hold is empty, it may spend one preview *before* locking. Cap the
  // horizon so hold and non-hold branches can be compared at equal depth.
  const size_t known_placements =
      std::max<size_t>(1, visible.size() + (engine.hold_piece() ? 1 : 0));
  const int depth_limit =
      pending ? 1
              : static_cast<int>(
                    std::min<size_t>(config.search_depth, known_placements));

  auto run_parents = [&](size_t count, auto &&process) {
    const size_t hardware = std::max(1u, std::thread::hardware_concurrency());
    const size_t workers = std::min({config.search_threads, hardware, count});
    if (workers == 1) {
      auto work = engine.clone();
      for (size_t i = 0; i < count; ++i)
        process(i, work);
      return;
    }
    std::atomic<size_t> next_parent{0};
    std::vector<std::exception_ptr> errors(workers);
    auto run_worker = [&](size_t worker_index) {
      try {
        auto work = engine.clone();
        for (size_t i = next_parent.fetch_add(1); i < count;
             i = next_parent.fetch_add(1))
          process(i, work);
      } catch (...) {
        errors[worker_index] = std::current_exception();
      }
    };
    std::vector<std::jthread> threads;
    threads.reserve(workers - 1);
    for (size_t i = 1; i < workers; ++i)
      threads.emplace_back(run_worker, i);
    run_worker(0);
    for (auto &thread : threads)
      thread.join();
    for (const auto &error : errors)
      if (error)
        std::rethrow_exception(error);
  };

  for (int depth = 0; depth < depth_limit; ++depth) {
    const bool last_layer = depth + 1 == depth_limit;
    std::vector<Candidate> pool;
    bool has_best = false;
    double layer_best_score = 0;
    size_t layer_best_first = 0;
    auto consider = [&](double score, size_t first) {
      if (!has_best || score > layer_best_score) {
        layer_best_score = score;
        layer_best_first = first;
        has_best = true;
      }
    };
    // Generate reachable actions before dispatching workers: the optional
    // per-episode placement cache must only be mutated by this thread.
    std::vector<std::vector<Placement>> generated(frontier.size());
    std::vector<const std::vector<Placement> *> legal_by_parent(frontier.size());
    size_t action_count = 0;
    // Fresh choose() calls have no mutable placement cache. Generate each
    // independent beam parent's placements concurrently as well as scoring
    // them; this is the expensive reachability search on tall boards.
    if (depth && !current && frontier.size() > 1)
      run_parents(frontier.size(), [&](size_t index, Engine &) {
        const auto &parent = frontier[index];
        if (!parent.terminal && parent.game.active_piece())
          generated[index] = actions(parent.game, parent.used, visible.size());
      });
    for (size_t parent_index = 0; parent_index < frontier.size();
         ++parent_index) {
      const auto &parent = frontier[parent_index];
      if (parent.terminal || !parent.game.active_piece())
        continue;
      const auto &legal =
          !depth ? root_actions :
          !current && frontier.size() > 1 ? generated[parent_index] :
          legal_actions(parent.game, parent.used, generated[parent_index]);
      legal_by_parent[parent_index] = &legal;
      action_count += legal.size();
    }
    // The original search discarded an incomplete layer. Detect that before
    // simulating it so the budget and chosen root action stay the same.
    if (depth && action_count &&
        (expanded >= config.node_budget ||
         action_count > config.node_budget - expanded))
      break;

    struct ParentResult {
      std::vector<Candidate> candidates;
      std::optional<std::pair<double, size_t>> best;
    };
    std::vector<ParentResult> results(frontier.size());
    auto process_parent = [&](size_t parent_index, Engine &work) {
      const auto &parent = frontier[parent_index];
      auto &result = results[parent_index];
      if (parent.terminal || !parent.game.active_piece()) {
        if (depth) {
          if (last_layer)
            result.best = {parent.score, parent.first};
          else
            result.candidates.push_back(
                {.action = std::nullopt,
                 .parent = parent_index,
                 .used = parent.used,
                 .first = parent.first,
                 .attack = parent.attack,
                 .lines = parent.lines,
                 .spins = parent.spins,
                 .score = parent.score,
                 .terminal = parent.terminal});
        }
        return;
      }
      const auto &legal = *legal_by_parent[parent_index];
      if (!last_layer)
        result.candidates.reserve(legal.size());
      for (size_t index = 0; index < legal.size(); ++index) {
        const auto &action = legal[index];
        work.restore(parent.game);
        const auto lock = work.apply_placement(action);
        size_t used = parent.used;
        if (action.use_hold && !parent.game.hold_piece())
          ++used;

        // If incoming garbage was ready, work.board() may now contain its
        // secret hole. Reconstruct the pre-rise board from public landing
        // cells.
        std::optional<Board> pre_rise_board;
        if (pending) {
          pre_rise_board = parent.game.board();
          const auto piece_cell = get_from_piece_type(action.type).cell_type();
          for (Position cell : action.cells())
            pre_rise_board->set(cell.row, cell.col, piece_cell);
          if (pre_rise_board->clear_lines() != lock.lines_cleared)
            throw std::logic_error("pre-rise board disagrees with engine lock");
        }
        const Board &board = pending ? *pre_rise_board : work.board();

        bool terminal = !pending && work.game_over();
        if (!pending && !terminal && used < visible.size()) {
          // spawn_next_piece() is only called when the popped piece is within
          // the root's visible preview (also after an empty-slot hold).
          terminal = !work.spawn_next_piece();
          ++used;
        }
        const int attack = parent.attack + lock.attack_sent;
        const int lines = parent.lines + lock.lines_cleared;
        const double spins = parent.spins + spin_reward(action, lock);
        const int incoming = pending && !lock.lines_cleared
                                 ? parent.game.pending_garbage_lines()
                                 : work.pending_garbage_lines();
        const double score =
            stack_value(board, work.hold_piece(), work.back_to_back(),
                        work.combo(), attack, lines, spins, incoming, terminal,
                        distance_to_t(work.hold_piece(), visible, used));
        if (last_layer) {
          if (!result.best || score > result.best->first)
            result.best = {score, depth ? parent.first : index};
        } else {
          result.candidates.push_back(
               {.action = action,
                .parent = parent_index,
                .used = used,
                .first = depth ? parent.first : index,
                .attack = attack,
                .lines = lines,
                .spins = spins,
                .score = score,
                .terminal = terminal});
        }
      }
    };
    run_parents(frontier.size(), process_parent);
    expanded += action_count;
    // Merge in parent order: equal scores keep their original stable order,
    // and all workers still compete in the same global beam.
    for (auto &result : results) {
      if (last_layer) {
        if (result.best)
          consider(result.best->first, result.best->second);
      } else {
        pool.insert(pool.end(), std::make_move_iterator(result.candidates.begin()),
                    std::make_move_iterator(result.candidates.end()));
      }
    }
    if (last_layer ? !has_best : pool.empty())
      break;
    if (last_layer) {
      best_first = layer_best_first;
      break;
    }
    std::stable_sort(pool.begin(), pool.end(),
                     [](const Candidate &a, const Candidate &b) {
                       return a.score > b.score;
                     });
    best_first = pool.front().first;
    std::unordered_set<std::string> seen;
    std::vector<Node> next;
    next.reserve(std::min(config.beam_width, pool.size()));
    for (const auto &candidate : pool) {
      auto game = frontier[candidate.parent].game.clone();
      if (candidate.action) {
        game.apply_placement(*candidate.action);
        // The candidate has already accounted for an empty-slot hold.
        const size_t consumed_by_hold =
            candidate.action->use_hold &&
            !frontier[candidate.parent].game.hold_piece();
        if (candidate.used > frontier[candidate.parent].used + consumed_by_hold)
          game.spawn_next_piece(); // Also reproduce a known-piece spawn
                                   // failure.
      }
      // Build the full continuation key only for contenders, not every
      // simulated placement in the pool (often thousands for one layer).
      if (!seen.insert(state_key(game, candidate.used, candidate.attack,
                                 candidate.lines, candidate.spins)).second)
        continue;
      next.push_back({std::move(game), candidate.used, candidate.first,
                      candidate.attack, candidate.lines, candidate.spins,
                      candidate.score, candidate.terminal});
      if (next.size() >= config.beam_width)
        break;
    }
    frontier = std::move(next);
  }
  return root_actions[best_first];
}

} // namespace

MisaMinoBot::MisaMinoBot(MisaMinoConfig config) : _config(config) {
  if (!config.search_depth || !config.beam_width || !config.node_budget ||
      !config.search_threads)
    throw std::invalid_argument("MisaMino search limits must be positive");
}

std::optional<Placement> MisaMinoBot::choose(const Engine &engine) const {
  return choose_with_cache(engine, _config, nullptr, nullptr, nullptr);
}

MisaMinoEpisode MisaMinoBot::play_episode(Engine &engine,
                                         size_t piece_cap) const {
  if (!piece_cap)
    throw std::invalid_argument("piece_cap must be positive");
  MisaMinoEpisode result;
  result.game_over = engine.game_over();
  if (engine.game_over() || engine.edit_mode())
    return result;
  if (!engine.active_piece() && !engine.spawn_next_piece()) {
    result.game_over = engine.game_over();
    return result;
  }
  PlacementCache previous;
  PlacementCache current;
  result.turns.reserve(std::min<size_t>(piece_cap, 128));
  for (size_t turn = 0; turn < piece_cap && !engine.game_over(); ++turn) {
    current.clear();
    auto choice = choose_with_cache(engine, _config, &previous, &current,
                                    &result.reused_searches);
    if (!choice)
      break;
    auto lock = engine.apply_placement(*choice);
    if (!lock.locked)
      throw std::logic_error("teacher placement did not lock");
    result.turns.push_back({std::move(*choice), lock});
    if (!engine.game_over() && !engine.spawn_next_piece() && !engine.game_over())
      throw std::logic_error("engine failed to spawn after teacher placement");
    previous.swap(current);
  }
  result.game_over = engine.game_over();
  result.truncated = !result.game_over && result.turns.size() == piece_cap;
  return result;
}

} // namespace tetris
