#include "engine/Match.hpp"

#include <stdexcept>
#include <utility>

namespace tetris {
Match::Match(std::vector<std::unique_ptr<Engine>> players,
             uint16_t wins_to_win)
    : _players(std::move(players)), _wins(_players.size(), 0),
      _targets(_players.size()),
      _wins_to_win(wins_to_win) {
  if (_players.size() < 2 || wins_to_win == 0)
    throw std::invalid_argument("match requires at least two players and one win");
  for (const auto &player : _players)
    if (!player)
      throw std::invalid_argument("match players must not be null");
}

void Match::start(uint64_t seed) {
  _seed = seed;
  _attack_sequence = 0;
  _started = true;
  _round_over = false;
  _round_winner.reset();
  for (size_t i = 0; i < _players.size(); ++i) {
    _players[i]->reset(seed);
    _players[i]->spawn_next_piece();
  }
  check_winner();
}

void Match::set_target(size_t sender, std::optional<size_t> target) {
  if (sender >= _players.size() ||
      (target && (*target >= _players.size() || *target == sender)))
    throw std::out_of_range("invalid match target");
  _targets[sender] = target;
}

void Match::deliver(size_t sender, uint16_t lines) {
  if (lines == 0)
    return;
  auto send_to = [&](size_t target) {
    uint16_t hole = static_cast<uint16_t>((_seed + sender +
                       _attack_sequence++) % _players[target]->board().get_width());
    _players[target]->receive_garbage(lines, hole);
  };
  if (_targets[sender] && !_players[*_targets[sender]]->game_over()) {
    send_to(*_targets[sender]);
    return;
  }
  for (size_t step = 1; step < _players.size(); ++step) {
    size_t target = (sender + step) % _players.size();
    if (!_players[target]->game_over()) {
      send_to(target);
      return;
    }
  }
}

std::vector<TickResult> Match::tick() {
  if (!_started)
    throw std::logic_error("start the round before ticking");
  if (_round_over)
    return std::vector<TickResult>(_players.size());
  std::vector<TickResult> results;
  results.reserve(_players.size());
  for (auto &player : _players) {
    TickResult result = player->tick();
    if (result.locked && !player->game_over())
      player->spawn_next_piece();
    results.push_back(result);
  }
  for (size_t i = 0; i < results.size(); ++i)
    deliver(i, results[i].attack_sent);
  check_winner();
  return results;
}

LockResult Match::hard_drop(size_t index) {
  if (!_started)
    throw std::logic_error("start the round before dropping");
  if (_round_over)
    return {};
  LockResult result = player(index).hard_drop();
  if (result.locked) {
    if (!player(index).game_over())
      player(index).spawn_next_piece();
    deliver(index, result.attack_sent);
    check_winner();
  }
  return result;
}

void Match::check_winner() {
  if (_round_over)
    return;
  size_t alive = 0, last = 0;
  for (size_t i = 0; i < _players.size(); ++i)
    if (!_players[i]->game_over()) {
      ++alive;
      last = i;
    }
  if (alive <= 1) {
    _round_over = true;
  }
  if (alive == 1) {
    _round_winner = last;
    if (++_wins[last] >= _wins_to_win)
      _match_winner = last;
  }
}
} // namespace tetris
