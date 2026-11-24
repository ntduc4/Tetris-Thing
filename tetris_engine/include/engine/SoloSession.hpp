#pragma once

#include "engine/Engine.hpp"
#include <cstdint>
#include <utility>

namespace tetris {

enum class SoloMode { Marathon, Sprint, Blitz };

struct SoloSettings {
  SoloMode mode = SoloMode::Marathon;
  uint32_t line_goal = 40;
  uint64_t time_limit_ticks = 120 * 60;
};

/** Solo objectives on top of the shared engine; one tick is one frame. */
class SoloSession {
public:
  explicit SoloSession(Engine engine, SoloSettings settings = {})
      : _engine(std::move(engine)), _settings(settings),
        _guideline_start_level(_engine.level()) {}

  void start(uint64_t seed = 0);
  TickResult tick();
  LockResult hard_drop();

  Engine &engine() { return _engine; }
  const Engine &engine() const { return _engine; }
  bool finished() const { return _finished; }
  bool succeeded() const { return _finished && !_engine.game_over(); }
  uint64_t elapsed_ticks() const { return _engine.tick_count(); }

private:
  void check_finished();
  Engine _engine;
  SoloSettings _settings;
  uint16_t _guideline_start_level = 1;
  bool _started = false;
  bool _finished = false;
};
} // namespace tetris
