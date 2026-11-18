#include "bot/MisaMinoBot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tetris {
namespace {

// These are deliberately engine-specific weights, not the original bot's
// tuned TOJ/22-row parameter table. Preserve the *strategy*: evaluate the
// resulting stack, attack efficiency, hold, B2B, and actual spins across a
// bounded, deduplicated beam of visible future placements.
double stack_value(const Board &board, std::optional<PieceType> hold, bool b2b,
                   uint32_t combo, int attack, int lines, double spin_value,
                   int pending, bool terminal, int t_distance) {
  const int width = board.get_width();
  const int height = board.get_height();
  std::vector<int> heights(width);
  int holes = 0;
  int open_holes = 0;
  int transitions = 0;
  int aggregate = 0;
  for (int col = 0; col < width; ++col) {
    heights[col] = board.column_height(col);
    aggregate += heights[col];
    for (int row = 0; row < heights[col]; ++row) {
      if (board.get(row, col) != Cell::Empty)
        continue;
      // Open-sided holes are less costly in a soft-drop/slide-under game.
      const bool open_left = col > 0 && heights[col - 1] <= row + 1;
      const bool open_right = col + 1 < width && heights[col + 1] <= row + 1;
      if (open_left || open_right)
        ++open_holes;
      else
        ++holes;
    }
  }
  int bumpiness = 0;
  for (int col = 1; col < width; ++col)
    bumpiness += std::abs(heights[col] - heights[col - 1]);
  for (int row = 0; row < board.max_height(); ++row) {
    bool previous = true; // Filled side walls.
    for (int col = 0; col < width; ++col) {
      bool occupied = board.get(row, col) != Cell::Empty;
      transitions += previous != occupied;
      previous = occupied;
    }
    transitions += !previous;
  }

  const int max_height = board.max_height();
  const int danger = std::max(0, max_height - height + 4);
  double value =
      -36.0 * holes - 14.0 * open_holes - 1.7 * transitions - 2.2 * bumpiness -
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
        if (board.get(row, col) != Cell::Empty ||
            board.get(row - 1, col - 1) != Cell::Empty ||
            board.get(row - 1, col) != Cell::Empty ||
            board.get(row - 1, col + 1) != Cell::Empty ||
            board.get(row, col - 1) == Cell::Empty ||
            board.get(row, col + 1) == Cell::Empty ||
            board.get(row + 1, col) != Cell::Empty)
          continue;
        // Support the shoulders without encouraging an unfillable deep hole.
        if (row > 1 && board.get(row - 2, col) == Cell::Empty)
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
  std::string key;
};

} // namespace

MisaMinoBot::MisaMinoBot(MisaMinoConfig config) : _config(config) {
  if (!config.search_depth || !config.beam_width || !config.node_budget)
    throw std::invalid_argument("MisaMino search limits must be positive");
}

std::optional<Placement> MisaMinoBot::choose(const Engine &engine) const {
  if (engine.game_over() || !engine.active_piece() || engine.edit_mode())
    return std::nullopt;

  // Snapshot once. Neither later Engine::preview_queue() calls nor a hold at
  // the edge of the preview may reveal a new piece to this search.
  const auto visible = engine.preview_queue();
  auto root_actions = actions(engine, 0, visible.size());
  if (root_actions.empty())
    return std::nullopt;

  std::vector<Node> frontier;
  frontier.push_back({engine.clone()});
  auto work = engine.clone();
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
                    std::min<size_t>(_config.search_depth, known_placements));

  for (int depth = 0; depth < depth_limit; ++depth) {
    std::vector<Candidate> pool;
    bool complete = true;
    for (size_t parent_index = 0; parent_index < frontier.size();
         ++parent_index) {
      const auto &parent = frontier[parent_index];
      if (parent.terminal || !parent.game.active_piece()) {
        if (depth) {
          pool.push_back(
              {.action = std::nullopt,
               .parent = parent_index,
               .used = parent.used,
               .first = parent.first,
               .attack = parent.attack,
               .lines = parent.lines,
               .spins = parent.spins,
               .score = parent.score,
               .terminal = parent.terminal,
               .key = state_key(parent.game, parent.used, parent.attack,
                                parent.lines, parent.spins)});
        }
        continue;
      }
      auto legal = depth ? actions(parent.game, parent.used, visible.size())
                         : root_actions;
      for (size_t index = 0; index < legal.size(); ++index) {
        if (depth && expanded >= _config.node_budget) {
          complete = false;
          break;
        }
        const auto &action = legal[index];
        work.restore(parent.game);
        const auto lock = work.apply_placement(action);
        size_t used = parent.used;
        if (action.use_hold && !parent.game.hold_piece())
          ++used;

        // If incoming garbage was ready, work.board() may now contain its
        // secret hole. Reconstruct the pre-rise board from public landing
        // cells.
        Board board = pending ? parent.game.board() : work.board();
        if (pending) {
          const auto piece_cell = get_from_piece_type(action.type).cell_type();
          for (Position cell : action.cells())
            board.set(cell.row, cell.col, piece_cell);
          if (board.clear_lines() != lock.lines_cleared)
            throw std::logic_error("pre-rise board disagrees with engine lock");
        }

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
        pool.push_back(
            {.action = action,
             .parent = parent_index,
             .used = used,
             .first = depth ? parent.first : index,
             .attack = attack,
             .lines = lines,
             .spins = spins,
             .score = score,
             .terminal = terminal,
             .key = pending ? std::string{}
                            : state_key(work, used, attack, lines, spins)});
        ++expanded;
      }
      if (!complete)
        break;
    }
    // Do not choose from a partially expanded layer: it would favor the first
    // beam parent solely because the budget ran out.
    if (!complete || pool.empty())
      break;
    std::stable_sort(pool.begin(), pool.end(),
                     [](const Candidate &a, const Candidate &b) {
                       return a.score > b.score;
                     });
    best_first = pool.front().first;
    if (depth + 1 == depth_limit)
      break;

    std::unordered_set<std::string> seen;
    std::vector<Node> next;
    next.reserve(std::min(_config.beam_width, pool.size()));
    for (const auto &candidate : pool) {
      if (!seen.insert(candidate.key).second)
        continue;
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
      next.push_back({std::move(game), candidate.used, candidate.first,
                      candidate.attack, candidate.lines, candidate.spins,
                      candidate.score, candidate.terminal});
      if (next.size() >= _config.beam_width)
        break;
    }
    frontier = std::move(next);
  }
  return root_actions[best_first];
}

} // namespace tetris
