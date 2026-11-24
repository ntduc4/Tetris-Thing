#pragma once

#include "BoardWidget.hpp"
#include "GameConfig.hpp"
#include "ModelOpponent.hpp"
#include "PieceQueueWidget.hpp"
#include "engine/Match.hpp"
#include "engine/SoloSession.hpp"

#include <QWidget>

#include <array>
#include <chrono>
#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

namespace tetris::qtapp {

enum class Input : int { Left, Right, Down, CW, CCW, Half, Hold, Drop, Count };
constexpr int InputCount = static_cast<int>(Input::Count);
using Bindings = std::array<int, InputCount>;

class MainWindow : public QWidget {
public:
  explicit MainWindow(QWidget *parent = nullptr);
  Engine *current_engine(size_t player = 0);
  const Engine *current_engine(size_t player = 0) const;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  struct HeldKeys {
    std::array<bool, InputCount> down{};
    int left_ticks = 0;
    int right_ticks = 0;
    int last_horizontal = 0;
  };

  bool is_room() const;
  bool is_bot_match() const;
  bool is_model_match() const;
  bool is_misamino_match() const;
  bool is_match() const;
  bool can_control_bot_match() const;
  const GameConfig &bot_rules() const;
  void start_model();
  void start_misamino_turn();
  void restart();
  void next_round();
  void open_rules();
  void open_bindings();
  void toggle_edit();
  void paint(int row, int col, bool erase);
  void action(size_t player, Input input);
  void repeat(size_t player, const GameConfig &settings);
  static int horizontal_direction(const HeldKeys &held);
  void move_spawned_piece(size_t player, const GameConfig &settings);
  bool handle_key(int key, bool pressed);
  void frame();
  void refresh();
  void clear_keys();

  GameConfig _solo_rules, _room_rules, _model_rules, _misamino_rules;
  Bindings _bindings[2];
  HeldKeys _held[2];
  uint64_t _seed = 1;
  uint64_t _round_seed = 0;
  std::unique_ptr<SoloSession> _solo;
  std::unique_ptr<Match> _match;
  std::unique_ptr<ModelOpponent> _model;
  bool _opponent_failed = false;
  bool _bot_thinking = false;
  uint64_t _bot_generation = 0;
  QComboBox *_mode, *_opponent, *_bot_height, *_palette;
  QPushButton *_edit, *_next, *_rules;
  QLabel *_status[2], *_streaks[2], *_message;
  QLabel *_titles[2];
  QWidget *_panels[2];
  BoardWidget *_boards[2];
  PieceHoldWidget *_holds[2];
  PieceQueueWidget *_queues[2];
  QTimer *_timer;
  std::chrono::steady_clock::time_point _last_frame;
  std::chrono::nanoseconds _accumulated{};
  int _edit_row = 19, _edit_col = 5;
};

} // namespace tetris::qtapp
