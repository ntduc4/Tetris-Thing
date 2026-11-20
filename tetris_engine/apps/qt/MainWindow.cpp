#include "MainWindow.hpp"
#include "RulesDialog.hpp"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace tetris::qtapp {
namespace {
constexpr const char *ActionNames[] = {"Left", "Right", "Soft drop", "Rotate CW",
                                       "Rotate CCW", "Rotate 180", "Hold", "Hard drop"};

int saved_int(QSettings &settings, const char *name, int fallback, int min, int max) {
  bool valid = false;
  int value = settings.value(name).toString().toInt(&valid);
  return valid && value >= min && value <= max ? value : fallback;
}

double saved_double(QSettings &settings, const char *name, double fallback, double max) {
  bool valid = false;
  double value = settings.value(name).toString().toDouble(&valid);
  return valid && std::isfinite(value) && value >= 0 && value <= max ? value : fallback;
}

template <typename T, size_t N>
void save_table(QSettings &settings, const char *name, const std::array<T, N> &table) {
  QStringList entries;
  for (T value : table)
    entries << QString::number(value);
  settings.setValue(name, entries);
}

template <typename T, size_t N>
void load_table(QSettings &settings, const char *name, std::array<T, N> &table) {
  const QStringList entries = settings.value(name).toStringList();
  if (entries.size() != static_cast<qsizetype>(N))
    return;
  std::array<T, N> loaded;
  for (size_t i = 0; i < N; ++i) {
    bool valid = false;
    auto value = entries[static_cast<qsizetype>(i)].toULongLong(&valid);
    if (!valid || value > std::numeric_limits<T>::max())
      return;
    loaded[i] = static_cast<T>(value);
  }
  table = loaded;
}

void save_config(QSettings &settings, const char *group, const GameConfig &c) {
  settings.beginGroup(group);
  settings.setValue("preset", static_cast<int>(c.preset));
  settings.setValue("width", c.width);
  settings.setValue("height", c.height);
  settings.setValue("gravity", c.gravity);
  settings.setValue("gravityIncrease", c.gravity_increase);
  settings.setValue("gravityMargin", static_cast<qulonglong>(c.gravity_margin));
  settings.setValue("lockDelay", c.lock_delay);
  settings.setValue("lockResets", c.lock_resets);
  settings.setValue("garbageDelay", c.garbage_delay);
  settings.setValue("spin", static_cast<int>(c.spin));
  settings.setValue("rotation", static_cast<int>(c.rotation));
  settings.setValue("bag", static_cast<int>(c.bag));
  settings.setValue("attack", static_cast<int>(c.attack));
  save_table(settings, "attackNormal", c.attacks.normal);
  save_table(settings, "attackMini", c.attacks.mini);
  save_table(settings, "attackFull", c.attacks.full);
  settings.setValue("attackB2B", c.attacks.b2b_bonus);
  settings.setValue("attackPC", c.attacks.perfect_clear_bonus);
  settings.setValue("attackCombo", c.attacks.combo_multiplier);
  save_table(settings, "scoreNormal", c.scores.normal);
  save_table(settings, "scoreMini", c.scores.mini);
  save_table(settings, "scoreFull", c.scores.full);
  settings.setValue("scoreCombo", static_cast<qulonglong>(c.scores.combo_bonus));
  settings.setValue("scorePC", static_cast<qulonglong>(c.scores.perfect_clear_bonus));
  settings.setValue("hardPoints", c.scores.hard_drop_point);
  settings.setValue("softPoints", c.scores.soft_drop_point);
  settings.setValue("das", c.das);
  settings.setValue("arr", c.arr);
  settings.setValue("softDrop", c.soft_drop);
  settings.setValue("sprintGoal", c.sprint_goal);
  settings.setValue("blitzSeconds", c.blitz_seconds);
  settings.setValue("winsToWin", c.wins_to_win);
  settings.endGroup();
}

void load_config(QSettings &settings, const char *group, GameConfig &c) {
  settings.beginGroup(group);
  c.preset = static_cast<Preset>(saved_int(settings, "preset", static_cast<int>(c.preset), 0, 2));
  c.width = saved_int(settings, "width", c.width, 4, 20);
  c.height = saved_int(settings, "height", c.height, 4, 80);
  c.gravity = saved_double(settings, "gravity", c.gravity, 40);
  c.gravity_increase = saved_double(settings, "gravityIncrease", c.gravity_increase, 1);
  c.gravity_margin = saved_int(settings, "gravityMargin", c.gravity_margin, 0, 1000000);
  c.lock_delay = saved_int(settings, "lockDelay", c.lock_delay, 0, 600);
  c.lock_resets = saved_int(settings, "lockResets", c.lock_resets, 0, 1000);
  c.garbage_delay = saved_int(settings, "garbageDelay", c.garbage_delay, 0, 600);
  c.spin = static_cast<SpinRule>(saved_int(settings, "spin", static_cast<int>(c.spin), 0, 3));
  c.rotation = static_cast<RotationRule>(saved_int(settings, "rotation", static_cast<int>(c.rotation), 0, 2));
  c.bag = static_cast<BagRule>(saved_int(settings, "bag", static_cast<int>(c.bag), 0, 2));
  c.attack = static_cast<AttackRule>(saved_int(settings, "attack", static_cast<int>(c.attack), 0, 4));
  load_table(settings, "attackNormal", c.attacks.normal);
  load_table(settings, "attackMini", c.attacks.mini);
  load_table(settings, "attackFull", c.attacks.full);
  c.attacks.b2b_bonus = saved_int(settings, "attackB2B", c.attacks.b2b_bonus, 0, 1000);
  c.attacks.perfect_clear_bonus = saved_int(settings, "attackPC", c.attacks.perfect_clear_bonus, 0, 1000);
  c.attacks.combo_multiplier = saved_double(settings, "attackCombo", c.attacks.combo_multiplier, 100);
  load_table(settings, "scoreNormal", c.scores.normal);
  load_table(settings, "scoreMini", c.scores.mini);
  load_table(settings, "scoreFull", c.scores.full);
  c.scores.combo_bonus = saved_int(settings, "scoreCombo", c.scores.combo_bonus, 0, 1000000);
  c.scores.perfect_clear_bonus = saved_int(settings, "scorePC", c.scores.perfect_clear_bonus, 0, 1000000);
  c.scores.hard_drop_point = saved_int(settings, "hardPoints", c.scores.hard_drop_point, 0, 10000);
  c.scores.soft_drop_point = saved_int(settings, "softPoints", c.scores.soft_drop_point, 0, 10000);
  c.das = saved_int(settings, "das", c.das, 0, 60);
  c.arr = saved_int(settings, "arr", c.arr, 0, 60);
  c.soft_drop = saved_int(settings, "softDrop", c.soft_drop, 1, 80);
  c.sprint_goal = saved_int(settings, "sprintGoal", c.sprint_goal, 1, 1000);
  c.blitz_seconds = saved_int(settings, "blitzSeconds", c.blitz_seconds, 1, 3600);
  c.wins_to_win = saved_int(settings, "winsToWin", c.wins_to_win, 1, 20);
  settings.endGroup();
}

void save_bindings(QSettings &settings, const Bindings (&bindings)[2]) {
  for (int player = 0; player < 2; ++player) {
    QStringList keys;
    for (int key : bindings[player])
      keys << QString::number(key);
    settings.setValue(QString("bindings/player%1").arg(player + 1), keys);
  }
}

void load_bindings(QSettings &settings, Bindings (&bindings)[2]) {
  Bindings loaded[2];
  std::vector<int> used{Qt::Key_F};
  for (int player = 0; player < 2; ++player) {
    QStringList keys = settings.value(QString("bindings/player%1").arg(player + 1)).toStringList();
    if (keys.size() != InputCount)
      return;
    for (int input = 0; input < InputCount; ++input) {
      bool valid = false;
      int key = keys[input].toInt(&valid);
      if (!valid || key < Qt::Key_Space || key == Qt::Key_unknown ||
          std::find(used.begin(), used.end(), key) != used.end())
        return;
      loaded[player][input] = key;
      used.push_back(key);
    }
  }
  std::copy(std::begin(loaded), std::end(loaded), bindings);
}

class BindingsDialog : public QDialog {
public:
  explicit BindingsDialog(Bindings (&bindings)[2], QWidget *parent)
      : QDialog(parent), _bindings(bindings) {
    setWindowTitle("Keyboard controls");
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("One key per action; keys cannot overlap. "
                                 "F toggles solo drawing mode.", this));
    auto *grid = new QFormLayout;
    for (int player = 0; player < 2; ++player) {
      grid->addRow(new QLabel(player == 0 ? "Player 1" : "Player 2", this));
      for (int input = 0; input < InputCount; ++input) {
        auto *field = new QKeySequenceEdit(QKeySequence(_bindings[player][input]), this);
        field->setObjectName(QString("binding_%1_%2").arg(player).arg(input));
        field->setMaximumSequenceLength(1);
        _fields[player][input] = field;
        grid->addRow(ActionNames[input], field);
      }
    }
    layout->addLayout(grid);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                         QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
      Bindings proposed[2];
      std::vector<int> used{Qt::Key_F};
      for (int player = 0; player < 2; ++player) {
        for (int input = 0; input < InputCount; ++input) {
          auto sequence = _fields[player][input]->keySequence();
          if (sequence.isEmpty() || sequence[0].keyboardModifiers() != Qt::NoModifier) {
            QMessageBox::warning(this, "Invalid key", "Use one unmodified key per action.");
            return;
          }
          int key = sequence[0].key();
          if (key == Qt::Key_Enter) key = Qt::Key_Return;
          if (std::find(used.begin(), used.end(), key) != used.end()) {
            QMessageBox::warning(this, "Duplicate key", "Keys must be unique. F is reserved for drawing.");
            return;
          }
          used.push_back(key);
          proposed[player][input] = key;
        }
      }
      std::copy(std::begin(proposed), std::end(proposed), _bindings);
      accept();
    });
  }

private:
  Bindings (&_bindings)[2];
  QKeySequenceEdit *_fields[2][InputCount]{};
};
} // namespace

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  setWindowTitle("Tetris — Solo / Local / Model");
  resize(820, 740);
  setFocusPolicy(Qt::StrongFocus);

  _bindings[0] = {Qt::Key_A, Qt::Key_D, Qt::Key_S, Qt::Key_W,
                  Qt::Key_Q, Qt::Key_E, Qt::Key_C, Qt::Key_Space};
  _bindings[1] = {Qt::Key_Left, Qt::Key_Right, Qt::Key_Down, Qt::Key_Up,
                  Qt::Key_J, Qt::Key_K, Qt::Key_L, Qt::Key_Return};
  {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TetrisEngine", "Tetris");
    load_config(settings, "solo", _solo_rules);
    load_config(settings, "room", _room_rules);
    load_bindings(settings, _bindings);
  }
  // Model board height comes from the checkpoint when it loads. These rules
  // are intentionally independent of saved solo/local room settings.
  _model_rules.width = 10;
  _model_rules.height = 10;
  _model_rules.gravity = 0;
  _model_rules.gravity_increase = 0;
  _model_rules.garbage_delay = 0;

  auto *outer = new QVBoxLayout(this);
  auto *menu = new QHBoxLayout;
  _mode = new QComboBox(this);
  _mode->setObjectName("mode");
  _mode->addItems({"Zen (no goal)", "Sprint", "Blitz", "Custom / Marathon",
                   "Local room (2P)", "Turn-based vs latest model"});
  menu->addWidget(_mode);
  _rules = new QPushButton("Rules…", this);
  _rules->setObjectName("rules");
  menu->addWidget(_rules);
  auto *keys = new QPushButton("Keybinds…", this);
  keys->setObjectName("keybinds");
  menu->addWidget(keys);
  auto *start = new QPushButton("Restart", this);
  start->setObjectName("restart");
  menu->addWidget(start);
  _next = new QPushButton("Next round", this);
  _next->setObjectName("nextRound");
  menu->addWidget(_next);
  outer->addLayout(menu);

  auto *editor = new QHBoxLayout;
  _edit = new QPushButton("Draw mode (F)", this);
  _edit->setObjectName("drawMode");
  editor->addWidget(_edit);
  _palette = new QComboBox(this);
  _palette->setObjectName("palette");
  _palette->addItems({"Erase", "I · cyan", "O · yellow", "T · purple",
                      "S · green", "Z · red", "J · blue", "L · orange",
                      "Garbage", "Unclearable"});
  _palette->setCurrentIndex(8);
  editor->addWidget(_palette);
  editor->addStretch();
  outer->addLayout(editor);

  auto *boards = new QHBoxLayout;
  for (int i = 0; i < 2; ++i) {
    _panels[i] = new QWidget(this);
    auto *column = new QVBoxLayout;
    _panels[i]->setLayout(column);
    _titles[i] = new QLabel(i == 0 ? "PLAYER 1" : "PLAYER 2", _panels[i]);
    column->addWidget(_titles[i]);
    auto *field = new QHBoxLayout;
    field->setContentsMargins(0, 0, 0, 0);
    field->setSpacing(4);
    auto *side = new QVBoxLayout;
    side->setContentsMargins(0, 0, 0, 0);
    side->setSpacing(8);
    _holds[i] = new PieceHoldWidget(_panels[i]);
    _holds[i]->setObjectName(i == 0 ? "hold1" : "hold2");
    side->addWidget(_holds[i]);
    _streaks[i] = new QLabel(_panels[i]);
    _streaks[i]->setObjectName(i == 0 ? "streak1" : "streak2");
    _streaks[i]->setFixedWidth(90);
    _streaks[i]->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    _streaks[i]->setTextFormat(Qt::RichText);
    _streaks[i]->setStyleSheet("background: #151c2b; border: 1px solid #354159; padding: 4px;");
    side->addWidget(_streaks[i]);
    side->addStretch();
    field->addLayout(side);
    _boards[i] = new BoardWidget(_panels[i]);
    _boards[i]->setObjectName(i == 0 ? "board1" : "board2");
    field->addWidget(_boards[i], 1);
    _queues[i] = new PieceQueueWidget(_panels[i]);
    _queues[i]->setObjectName(i == 0 ? "queue1" : "queue2");
    field->addWidget(_queues[i]);
    column->addLayout(field, 1);
    _status[i] = new QLabel(_panels[i]);
    _status[i]->setObjectName(i == 0 ? "status1" : "status2");
    _status[i]->setTextFormat(Qt::RichText);
    _status[i]->setMinimumHeight(55);
    column->addWidget(_status[i]);
    boards->addWidget(_panels[i], 1);
  }
  outer->addLayout(boards, 1);
  _message = new QLabel(this);
  _message->setObjectName("message");
  _message->setWordWrap(true);
  outer->addWidget(_message);
  auto *help = new QLabel(
       "Default P1: A/D move, S down, W/Q rotate, E 180, C hold, Space drop. "
       "Default P2: ←/→ move, ↓ down, ↑/J rotate, K 180, L hold, Enter drop. "
      "F = solo draw mode; left-drag paint, right-drag erase.", this);
  help->setWordWrap(true);
  outer->addWidget(help);

  for (auto *button : {_rules, keys, start, _next, _edit})
    button->setFocusPolicy(Qt::NoFocus);
  connect(_mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this] { restart(); });
  connect(start, &QPushButton::clicked, this, [this] { restart(); });
  connect(_next, &QPushButton::clicked, this, [this] { next_round(); });
  connect(_rules, &QPushButton::clicked, this, [this] { open_rules(); });
  connect(keys, &QPushButton::clicked, this, [this] { open_bindings(); });
  connect(_edit, &QPushButton::clicked, this, [this] { toggle_edit(); });
  connect(_palette, QOverload<int>::of(&QComboBox::activated), this,
          [this] { setFocus(); });
  _boards[0]->set_paint_handler([this](int row, int col, bool erase) {
    paint(row, col, erase);
  });

  _timer = new QTimer(this);
  _timer->setTimerType(Qt::PreciseTimer);
  _timer->setInterval(8);
  connect(_timer, &QTimer::timeout, this, [this] { frame(); });
  qApp->installEventFilter(this);
  restart();
  _timer->start();
}

bool MainWindow::is_room() const { return _mode->currentIndex() == 4; }
bool MainWindow::is_model_match() const { return _mode->currentIndex() == 5; }
bool MainWindow::is_match() const { return is_room() || is_model_match(); }
bool MainWindow::can_control_model_match() const {
  return !is_model_match() || (_model && _model->ready() &&
                               !_model->busy() && !_model_failed);
}

const Engine *MainWindow::current_engine(size_t player) const {
  if (_match)
    return player < _match->player_count() ? &_match->player(player) : nullptr;
  return player == 0 && _solo ? &_solo->engine() : nullptr;
}

Engine *MainWindow::current_engine(size_t player) {
  return const_cast<Engine *>(std::as_const(*this).current_engine(player));
}

void MainWindow::clear_keys() {
  for (auto &player : _held)
    player = {};
}

void MainWindow::restart() {
  clear_keys();
  _model.reset();
  _model_failed = false;
  for (int i = 0; i < 2; ++i) {
    _boards[i]->set_engine(nullptr);
    _holds[i]->set_piece(std::nullopt);
    _queues[i]->set_pieces({});
  }
  _solo.reset();
  _match.reset();
  const uint64_t seed = _round_seed = _seed++;
  if (is_match()) {
    std::vector<std::unique_ptr<Engine>> players;
    for (int i = 0; i < 2; ++i)
      players.push_back(std::make_unique<Engine>(
          make_engine(is_model_match() ? _model_rules : _room_rules)));
    _match = std::make_unique<Match>(
        std::move(players), is_model_match() ? _model_rules.wins_to_win
                                            : _room_rules.wins_to_win);
    _match->start(seed);
  } else {
    SoloSettings objective;
    if (_mode->currentIndex() == 1) objective.mode = SoloMode::Sprint;
    if (_mode->currentIndex() == 2) objective.mode = SoloMode::Blitz;
    objective.line_goal = _solo_rules.sprint_goal;
    objective.time_limit_ticks = static_cast<uint64_t>(_solo_rules.blitz_seconds) * 60;
    _solo = std::make_unique<SoloSession>(make_engine(_solo_rules), objective);
    _solo->start(seed);
  }
  for (int i = 0; i < 2; ++i) {
    _boards[i]->set_engine(current_engine(i));
    _panels[i]->setVisible(i == 0 || is_match());
  }
  _titles[1]->setText(is_model_match() ? "LATEST MODEL" : "PLAYER 2");
  _edit_row = std::min<int>(VisibleHeight, current_engine()->board().get_height()) - 1;
  _edit_col = current_engine()->board().get_width() / 2;
  _message->setText("Ready. Rules and keybinds can be changed from the toolbar.");
  _last_frame = std::chrono::steady_clock::now();
  _accumulated = std::chrono::nanoseconds{};
  refresh();
  setFocus();
  if (is_model_match())
    start_model();
}

void MainWindow::start_model() {
  _model_failed = false;
  _message->setText("Loading latest model…");
  _model = std::make_unique<ModelOpponent>(
      [this](int height) {
        clear_keys();
        if (_model_rules.height != height) {
          _model_rules.height = height;
          // A different encoding is a new match, not a continuation with
          // incompatible board state. Detach widgets before replacing engines.
          for (auto *board : _boards)
            board->set_engine(nullptr);
          std::vector<std::unique_ptr<Engine>> players;
          for (int i = 0; i < 2; ++i)
            players.push_back(std::make_unique<Engine>(make_engine(_model_rules)));
          _match = std::make_unique<Match>(std::move(players), _model_rules.wins_to_win);
          _match->start(_round_seed);
          for (int i = 0; i < 2; ++i)
            _boards[i]->set_engine(current_engine(i));
        }
        _message->setText("Model ready. Your turn — hard drop to make it respond.");
        refresh();
      },
      [this](const Placement &placement) {
        if (!is_model_match() || !_match || _match->round_over())
          return;
        try {
          _match->apply_placement(1, placement);
          if (!_match->round_over())
            _message->setText("Your turn.");
        } catch (const std::exception &error) {
          _model_failed = true;
          _message->setText(QString("Model placement failed: %1. Restart to retry.").arg(error.what()));
        }
        refresh();
      },
      [this](const QString &error) {
        _model_failed = true;
        _message->setText(QString("Model unavailable: %1 Restart to retry.").arg(error));
        refresh();
      }, this);
  _model->start();
}

void MainWindow::next_round() {
  if (!_match || !_match->round_over() || _match->match_winner())
    return;
  clear_keys();
  _model.reset();
  _round_seed = _seed++;
  _match->start(_round_seed);
  _message->setText(is_model_match() ? "Loading latest model…" : "Next round.");
  refresh();
  setFocus();
  if (is_model_match())
    start_model();
}

void MainWindow::open_rules() {
  if (is_model_match())
    return;
  _timer->stop();
  clear_keys();
  auto &rules = is_room() ? _room_rules : _solo_rules;
  RulesDialog dialog(rules, is_room(), this);
  bool accepted = dialog.exec() == QDialog::Accepted;
  if (accepted) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TetrisEngine", "Tetris");
    save_config(settings, is_room() ? "room" : "solo", rules);
    restart();
  }
  _last_frame = std::chrono::steady_clock::now();
  _accumulated = std::chrono::nanoseconds{};
  _timer->start();
  setFocus();
}

void MainWindow::open_bindings() {
  _timer->stop();
  clear_keys();
  BindingsDialog dialog(_bindings, this);
  if (dialog.exec() == QDialog::Accepted) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TetrisEngine", "Tetris");
    save_bindings(settings, _bindings);
  }
  _last_frame = std::chrono::steady_clock::now();
  _accumulated = std::chrono::nanoseconds{};
  _timer->start();
  setFocus();
}

void MainWindow::toggle_edit() {
  if (!_solo || (_solo->finished() && !_solo->engine().edit_mode()))
    return;
  Engine &engine = _solo->engine();
  if (!engine.set_edit_mode(!engine.edit_mode())) {
    _message->setText("Erase cells overlapping the active piece before leaving draw mode.");
  } else {
    clear_keys();
    _message->setText(engine.edit_mode() ? "Draw mode: click/drag to paint; right click to erase."
                                         : "Playing.");
  }
  refresh();
  setFocus();
}

void MainWindow::paint(int row, int col, bool erase) {
  if (!_solo || !_solo->engine().edit_mode())
    return;
  Cell colors[] = {Cell::Empty, Cell::I, Cell::O, Cell::T, Cell::S,
                   Cell::Z, Cell::J, Cell::L, Cell::Garbage, Cell::Unclearable};
  _solo->engine().paint_cell(row, col, erase ? Cell::Empty : colors[_palette->currentIndex()]);
  _edit_row = row;
  _edit_col = col;
  refresh();
}

void MainWindow::action(size_t player, Input input) {
  if ((is_model_match() && player != 0) || !can_control_model_match())
    return;
  Engine *game = current_engine(player);
  if (!game || game->game_over() || game->edit_mode() ||
      (_solo && _solo->finished()) || (_match && _match->round_over()))
    return;
  switch (input) {
  case Input::Left: game->try_move_left(); break;
  case Input::Right: game->try_move_right(); break;
  case Input::Down:
    game->try_soft_drop((is_model_match() ? _model_rules :
                         is_room() ? _room_rules : _solo_rules).soft_drop);
    break;
  case Input::CW: game->try_rotate_cw(); break;
  case Input::CCW: game->try_rotate_ccw(); break;
  case Input::Half: game->try_rotate_180(); break;
  case Input::Hold: game->hold(); break;
  case Input::Drop:
    if (_match) {
      const LockResult lock = _match->hard_drop(player);
      if (is_model_match() && lock.locked && !_match->round_over()) {
        clear_keys();
        _message->setText("Model thinking…");
        _model->choose(_match->player(1));
      }
    } else {
      _solo->hard_drop();
    }
    break;
  default: break;
  }
  refresh();
}

void MainWindow::repeat(size_t player, const GameConfig &settings) {
  HeldKeys &held = _held[player];
  int horizontal = held.down[static_cast<int>(Input::Left)] &&
                   held.down[static_cast<int>(Input::Right)]
                       ? held.last_horizontal
                       : held.down[static_cast<int>(Input::Left)] ? -1
                       : held.down[static_cast<int>(Input::Right)] ? 1 : 0;
  if (horizontal != 0) {
    int &ticks = horizontal < 0 ? held.left_ticks : held.right_ticks;
    if (++ticks >= std::max(1, settings.das) &&
        (settings.arr == 0 || (ticks - std::max(1, settings.das)) % settings.arr == 0)) {
      if (settings.arr == 0) {
        Engine *game = current_engine(player);
        if (game && !game->game_over())
          horizontal < 0 ? game->try_move_left(-1) : game->try_move_right(-1);
      } else {
        action(player, horizontal < 0 ? Input::Left : Input::Right);
      }
    }
  }
  if (held.down[static_cast<int>(Input::Down)])
    action(player, Input::Down);
}

bool MainWindow::handle_key(int key, bool pressed) {
  if (key == Qt::Key_Enter) key = Qt::Key_Return;
  if (key == Qt::Key_F && !is_match()) {
    if (pressed) toggle_edit();
    return true;
  }
  if (_solo && _solo->engine().edit_mode()) {
    if (!pressed) return true;
    auto &board = _solo->engine().board();
    if (key == Qt::Key_Left || key == Qt::Key_A) _edit_col = std::max(0, _edit_col - 1);
    if (key == Qt::Key_Right || key == Qt::Key_D) _edit_col = std::min<int>(board.get_width() - 1, _edit_col + 1);
    if (key == Qt::Key_Down || key == Qt::Key_S) _edit_row = std::max(0, _edit_row - 1);
    if (key == Qt::Key_Up || key == Qt::Key_W) _edit_row = std::min<int>(std::min<int>(VisibleHeight, board.get_height()) - 1, _edit_row + 1);
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
      _palette->setCurrentIndex(key - Qt::Key_0);
      paint(_edit_row, _edit_col, false);
    }
    refresh();
    return true;
  }
  for (int player = 0; player < (is_room() ? 2 : 1); ++player) {
    for (int i = 0; i < InputCount; ++i) {
      if (_bindings[player][i] != key) continue;
      HeldKeys &held = _held[player];
      if (pressed && !held.down[i]) {
        held.down[i] = true;
        if (i == static_cast<int>(Input::Left)) {
          held.left_ticks = 0;
          held.last_horizontal = -1;
        } else if (i == static_cast<int>(Input::Right)) {
          held.right_ticks = 0;
          held.last_horizontal = 1;
        }
        action(player, static_cast<Input>(i));
      } else if (!pressed) {
        held.down[i] = false;
      }
      return true;
    }
  }
  return false;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::WindowDeactivate) {
    clear_keys();
    return QWidget::eventFilter(watched, event);
  }
  if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)
    return QWidget::eventFilter(watched, event);
  if (QApplication::activeModalWidget() || QApplication::activeWindow() != window())
    return QWidget::eventFilter(watched, event);
  auto *focus = QApplication::focusWidget();
  if (qobject_cast<QComboBox *>(focus) || qobject_cast<QAbstractSpinBox *>(focus) ||
      qobject_cast<QLineEdit *>(focus) || qobject_cast<QKeySequenceEdit *>(focus))
    return QWidget::eventFilter(watched, event);
  auto *key = static_cast<QKeyEvent *>(event);
  if (key->isAutoRepeat())
    return true;
  if (key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
    return QWidget::eventFilter(watched, event);
  return handle_key(key->key(), event->type() == QEvent::KeyPress) ||
         QWidget::eventFilter(watched, event);
}

void MainWindow::frame() {
  using namespace std::chrono;
  auto now = steady_clock::now();
  _accumulated += duration_cast<nanoseconds>(now - _last_frame);
  _last_frame = now;
  constexpr nanoseconds frame_time(1000000000 / 60);
  // Never run an unbounded number of simulation frames after a suspended window.
  _accumulated = std::min(_accumulated, frame_time * 8);
  while (_accumulated >= frame_time) {
    if (_solo && !_solo->finished()) {
      if (!_solo->engine().edit_mode()) repeat(0, _solo_rules);
      _solo->tick();
    }
    if (is_model_match() && _match && !_match->round_over() &&
        can_control_model_match()) {
      repeat(0, _model_rules);
    }
    if (is_room() && _match && !_match->round_over()) {
      repeat(0, _room_rules);
      repeat(1, _room_rules);
      _match->tick();
    }
    _accumulated -= frame_time;
  }
  refresh();
}

void MainWindow::refresh() {
  _boards[0]->set_editable(_solo && _solo->engine().edit_mode());
  _boards[0]->set_cursor(_edit_row, _edit_col);
  const auto &rules = is_model_match() ? _model_rules :
                      is_room() ? _room_rules : _solo_rules;
  for (int i = 0; i < 2; ++i) {
    const Engine *game = current_engine(i);
    if (!game) continue;
    _queues[i]->set_pieces(game->preview_queue());
    _holds[i]->set_piece(game->hold_piece());
    auto counter = [](const QString &label, const QString &value, bool active) {
      return QString("<span style='color:%1'>%2<br><b>%3</b></span>")
          .arg(active ? "#f8c45c" : "#8392a9", label, value);
    };
    const uint32_t b2b = game->back_to_back_count();
    const uint32_t combo = game->combo();
    QString streaks = counter("B2B", b2b ? QString("x%1").arg(b2b) : "—", b2b > 0) +
                      "<br><br>" +
                      counter("COMBO", combo ? QString("x%1").arg(combo) : "—", combo > 0);
    if (rules.attack == AttackRule::Season2) {
      const uint32_t charge = game->surge_charge();
      streaks += "<br><br>" + counter(
          "★ SURGE", charge ? QString("+%1").arg(charge)
                               : QString("%1 / 4").arg(std::min<uint32_t>(b2b, 4)),
          charge > 0);
    }
    _streaks[i]->setText(streaks);
    _status[i]->setText(QString("Lines: %1   Score: %2   Incoming: %3%4")
        .arg(game->lines_cleared()).arg(game->score())
        .arg(game->pending_garbage_lines())
        .arg(_match ? QString("   Wins: %1").arg(_match->wins(i)) : QString()));
    _boards[i]->update();
  }
  _edit->setVisible(!is_match());
  _palette->setVisible(!is_match());
  _rules->setEnabled(!is_model_match());
  _rules->setToolTip(is_model_match() ? "Model mode uses fixed 10x10 training rules." : "");
  _edit->setText(_solo && _solo->engine().edit_mode() ? "Resume (F)" : "Draw mode (F)");
  _next->setVisible(is_match());
  _next->setEnabled(_match && _match->round_over() && !_match->match_winner());
  if (_solo && _solo->finished())
    _message->setText(_solo->succeeded() ? "Goal complete! Restart to play again."
                                          : "Game over. Restart to play again.");
  if (_match && _match->round_over()) {
    QString message = _match->round_winner()
        ? is_model_match()
            ? (*_match->round_winner() == 0 ? "You win!" : "Model wins.")
            : QString("Player %1 wins the round.").arg(*_match->round_winner() + 1)
        : QString("Round draw.");
    if (_match->match_winner()) message += " Match won! Press Restart for a new match.";
    else message += " Press Next round.";
    _message->setText(message);
  }
}

} // namespace tetris::qtapp
