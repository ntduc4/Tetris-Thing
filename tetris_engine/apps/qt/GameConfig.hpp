#pragma once

#include "engine/Engine.hpp"

namespace tetris::qtapp {

enum class SpinRule { AllMiniPlus, AllMini, TOnly, Off };
enum class RotationRule { SRSPlus, SRSX, Off };
enum class BagRule { Seven, Fourteen, Random };
enum class AttackRule { Season2, Season1, Guideline, Custom, Off };
enum class Preset { TetrioLike, Classic, Custom };

struct GameConfig {
  Preset preset = Preset::TetrioLike;
  int width = 10;
  int height = 40; // Includes the hidden rows above the 20-row playfield.
  double gravity = DefaultGravity;
  double gravity_increase = 0;
  uint64_t gravity_margin = DefaultGravityMarginTime;
  int lock_delay = DefaultLockDelay;
  int lock_resets = 15;
  int garbage_delay = 20; // Approximate incoming garbage travel time at 60 ticks/s.
  SpinRule spin = SpinRule::AllMiniPlus;
  RotationRule rotation = RotationRule::SRSPlus;
  BagRule bag = BagRule::Seven;
  AttackRule attack = AttackRule::Season2;
  AttackTable attacks;
  ScoreTable scores;
  int das = 10; // Ticks before held movement repeats.
  int arr = 2;  // Ticks between held movements; zero means instant to wall.
  int soft_drop = 10; // Cells per tick while held.
  int sprint_goal = 40;
  int blitz_seconds = 120;
  int wins_to_win = 2;
};

GameConfig classic_config();
Engine make_engine(const GameConfig &settings);

} // namespace tetris::qtapp
