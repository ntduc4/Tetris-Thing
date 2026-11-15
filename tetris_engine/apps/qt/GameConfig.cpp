#include "GameConfig.hpp"

#include <algorithm>
#include <memory>

namespace tetris::qtapp {

GameConfig classic_config() {
  GameConfig config;
  config.preset = Preset::Classic;
  config.spin = SpinRule::TOnly;
  config.attack = AttackRule::Season1;
  config.gravity = 1.0 / 60.0;
  config.lock_delay = 30;
  config.garbage_delay = 0;
  return config;
}

Engine make_engine(const GameConfig &config) {
  std::unique_ptr<RotationSystem> rotation;
  if (config.rotation == RotationRule::Off)
    rotation = std::make_unique<None>();
  else if (config.rotation == RotationRule::SRSX)
    rotation = std::make_unique<SRSX>();
  else
    rotation = std::make_unique<SRSPlus>();

  std::unique_ptr<Randomizer> bag;
  switch (config.bag) {
  case BagRule::Seven: bag = std::make_unique<SevenBag>(); break;
  case BagRule::Fourteen: bag = std::make_unique<FourteenBag>(); break;
  case BagRule::Random: bag = std::make_unique<Random>(); break;
  }

  std::unique_ptr<SpinSystem> spin;
  switch (config.spin) {
  case SpinRule::AllMiniPlus: spin = std::make_unique<AllSpin>(true); break;
  case SpinRule::AllMini: spin = std::make_unique<AllSpin>(false); break;
  case SpinRule::TOnly: spin = std::make_unique<TSpin>(); break;
  case SpinRule::Off: break;
  }

  std::unique_ptr<AttackScheme> attack;
  switch (config.attack) {
  case AttackRule::Season2: attack = std::make_unique<TetrioSS2>(); break;
  case AttackRule::Season1: attack = std::make_unique<TetrioSS1>(); break;
  case AttackRule::Jstris: attack = std::make_unique<JSTris>(); break;
  case AttackRule::Custom: attack = std::make_unique<TableAttack>(config.attacks); break;
  case AttackRule::Off: break;
  }

  Engine engine(Board(config.width, config.height), std::move(rotation),
                std::move(bag), std::move(spin),
                std::make_unique<Classical>(config.scores), std::move(attack),
                DefaultQueueLength, (config.width - PieceDimension) / 2,
                std::min(config.height - static_cast<int>(PieceDimension),
                         static_cast<int>(VisibleHeight) - 3),
                config.gravity, config.gravity_increase, config.gravity_margin,
                config.lock_delay);
  engine.set_lock_delay(config.lock_delay, config.lock_resets);
  engine.set_garbage_delay(config.garbage_delay);
  return engine;
}

} // namespace tetris::qtapp
