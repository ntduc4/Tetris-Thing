#pragma once

#include "GameConfig.hpp"

#include <QDialog>

#include <array>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

namespace tetris::qtapp {

class RulesDialog : public QDialog {
public:
  explicit RulesDialog(GameConfig &config, bool multiplayer, QWidget *parent = nullptr);

private:
  void load(const GameConfig &config);
  void save();
  void mark_custom();

  GameConfig &_config;
  QComboBox *_preset, *_spin, *_rotation, *_bag, *_attack;
  QSpinBox *_width, *_height, *_margin, *_lock, *_resets, *_garbage;
  QSpinBox *_das, *_arr, *_soft, *_goal, *_time, *_wins;
  QDoubleSpinBox *_gravity, *_acceleration, *_attack_combo;
  QSpinBox *_attack_b2b, *_attack_pc, *_score_combo, *_score_pc;
  QSpinBox *_hard_points, *_soft_points;
  std::array<QLineEdit *, 3> _attack_lines{}, _score_lines{};
  bool _loading = false;
};

} // namespace tetris::qtapp
