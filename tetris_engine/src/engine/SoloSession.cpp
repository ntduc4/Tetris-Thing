#include "engine/SoloSession.hpp"

#include <stdexcept>

namespace tetris {
void SoloSession::start(uint64_t seed) {
  _engine.reset(seed);
  _started = true;
  _finished = false;
  _engine.spawn_next_piece();
  check_finished();
}

void SoloSession::check_finished() {
  if (_engine.game_over() ||
      (_settings.mode == SoloMode::Sprint &&
       _engine.lines_cleared() >= _settings.line_goal) ||
      (_settings.mode == SoloMode::Blitz &&
       _engine.tick_count() >= _settings.time_limit_ticks))
    _finished = true;
}

TickResult SoloSession::tick() {
  if (!_started)
    throw std::logic_error("start solo session before ticking");
  if (_finished)
    return {.game_over = _engine.game_over()};
  TickResult result = _engine.tick();
  check_finished();
  if (result.locked && !_finished && !_engine.game_over())
    _engine.spawn_next_piece();
  check_finished();
  return result;
}

LockResult SoloSession::hard_drop() {
  if (!_started)
    throw std::logic_error("start solo session before dropping");
  if (_finished)
    return {};
  LockResult result = _engine.hard_drop();
  check_finished();
  if (result.locked && !_finished && !_engine.game_over())
    _engine.spawn_next_piece();
  check_finished();
  return result;
}
} // namespace tetris
