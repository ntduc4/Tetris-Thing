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
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>
#include <vector>

namespace tetris::qtapp {
namespace {
constexpr const char *ActionNames[] = {"Left", "Right", "Soft drop", "Rotate CW",
                                       "Rotate CCW", "Rotate 180", "Hold", "Hard drop"};

QString piece_text(PieceType piece) {
  Cell cell = get_from_piece_type(piece).cell_type();
  char symbol = "IOTSZJL"[static_cast<int>(piece)];
  return QString("<span style='color:%1'>%2</span>")
      .arg(cell_color(cell).name(), QString(QChar(symbol)));
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
  setWindowTitle("Tetris — Zen / Local Room");
  resize(820, 740);
  setFocusPolicy(Qt::StrongFocus);

  _bindings[0] = {Qt::Key_A, Qt::Key_D, Qt::Key_S, Qt::Key_W,
                  Qt::Key_Q, Qt::Key_E, Qt::Key_C, Qt::Key_Space};
  _bindings[1] = {Qt::Key_Left, Qt::Key_Right, Qt::Key_Down, Qt::Key_Up,
                  Qt::Key_J, Qt::Key_K, Qt::Key_L, Qt::Key_Return};

  auto *outer = new QVBoxLayout(this);
  auto *menu = new QHBoxLayout;
  _mode = new QComboBox(this);
  _mode->setObjectName("mode");
  _mode->addItems({"Zen (no goal)", "Sprint", "Blitz", "Custom / Marathon", "Local room (2P)"});
  menu->addWidget(_mode);
  auto *rules = new QPushButton("Rules…", this);
  rules->setObjectName("rules");
  menu->addWidget(rules);
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
    auto *title = new QLabel(i == 0 ? "PLAYER 1" : "PLAYER 2", _panels[i]);
    column->addWidget(title);
    _boards[i] = new BoardWidget(_panels[i]);
    _boards[i]->setObjectName(i == 0 ? "board1" : "board2");
    column->addWidget(_boards[i], 1);
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
      "P1: A/D move, S down, W/Q rotate, E 180, C hold, Space drop. "
      "P2: ←/→ move, ↓ down, ↑/J rotate, K 180, L hold, Enter drop. "
      "F = solo draw mode; left-drag paint, right-drag erase.", this);
  help->setWordWrap(true);
  outer->addWidget(help);

  for (auto *button : {rules, keys, start, _next, _edit})
    button->setFocusPolicy(Qt::NoFocus);
  connect(_mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this] { restart(); });
  connect(start, &QPushButton::clicked, this, [this] { restart(); });
  connect(_next, &QPushButton::clicked, this, [this] { next_round(); });
  connect(rules, &QPushButton::clicked, this, [this] { open_rules(); });
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
  for (auto *board : _boards)
    board->set_engine(nullptr);
  _solo.reset();
  _match.reset();
  const uint64_t seed = _seed++;
  if (is_room()) {
    std::vector<std::unique_ptr<Engine>> players;
    for (int i = 0; i < 2; ++i)
      players.push_back(std::make_unique<Engine>(make_engine(_room_rules)));
    _match = std::make_unique<Match>(std::move(players), _room_rules.wins_to_win);
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
    _panels[i]->setVisible(i == 0 || is_room());
  }
  _edit_row = std::min<int>(VisibleHeight, current_engine()->board().get_height()) - 1;
  _edit_col = current_engine()->board().get_width() / 2;
  _message->setText("Ready. Rules and keybinds can be changed from the toolbar.");
  _last_frame = std::chrono::steady_clock::now();
  _accumulated = std::chrono::nanoseconds{};
  refresh();
  setFocus();
}

void MainWindow::next_round() {
  if (!_match || !_match->round_over() || _match->match_winner())
    return;
  clear_keys();
  _match->start(_seed++);
  _message->setText("Next round.");
  refresh();
  setFocus();
}

void MainWindow::open_rules() {
  _timer->stop();
  clear_keys();
  auto &rules = is_room() ? _room_rules : _solo_rules;
  RulesDialog dialog(rules, is_room(), this);
  bool accepted = dialog.exec() == QDialog::Accepted;
  if (accepted)
    restart();
  _last_frame = std::chrono::steady_clock::now();
  _accumulated = std::chrono::nanoseconds{};
  _timer->start();
  setFocus();
}

void MainWindow::open_bindings() {
  _timer->stop();
  clear_keys();
  BindingsDialog dialog(_bindings, this);
  dialog.exec();
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
  Engine *game = current_engine(player);
  if (!game || game->game_over() || game->edit_mode() ||
      (_solo && _solo->finished()) || (_match && _match->round_over()))
    return;
  switch (input) {
  case Input::Left: game->try_move_left(); break;
  case Input::Right: game->try_move_right(); break;
  case Input::Down:
    game->try_soft_drop((is_room() ? _room_rules : _solo_rules).soft_drop);
    break;
  case Input::CW: game->try_rotate_cw(); break;
  case Input::CCW: game->try_rotate_ccw(); break;
  case Input::Half: game->try_rotate_180(); break;
  case Input::Hold: game->hold(); break;
  case Input::Drop:
    if (_match) _match->hard_drop(player);
    else _solo->hard_drop();
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
  if (key == Qt::Key_F && !is_room()) {
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
    if (_match && !_match->round_over()) {
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
  for (int i = 0; i < 2; ++i) {
    const Engine *game = current_engine(i);
    if (!game) continue;
    QString previews;
    for (PieceType piece : game->preview_queue()) previews += piece_text(piece) + "  ";
    QString hold = game->hold_piece() ? piece_text(*game->hold_piece()) : "−";
    _status[i]->setText(QString("Hold: %1   Next: %2<br>Lines: %3   Score: %4   Incoming: %5%6")
        .arg(hold, previews).arg(game->lines_cleared()).arg(game->score())
        .arg(game->pending_garbage_lines())
        .arg(_match ? QString("   Wins: %1").arg(_match->wins(i)) : QString()));
    _boards[i]->update();
  }
  _edit->setVisible(!is_room());
  _palette->setVisible(!is_room());
  _edit->setText(_solo && _solo->engine().edit_mode() ? "Resume (F)" : "Draw mode (F)");
  _next->setVisible(is_room());
  _next->setEnabled(_match && _match->round_over() && !_match->match_winner());
  if (_solo && _solo->finished())
    _message->setText(_solo->succeeded() ? "Goal complete! Restart to play again."
                                          : "Game over. Restart to play again.");
  if (_match && _match->round_over()) {
    QString message = _match->round_winner()
        ? QString("Player %1 wins the round.").arg(*_match->round_winner() + 1)
        : QString("Round draw.");
    if (_match->match_winner()) message += " Match won! Press Restart for a new match.";
    else message += " Press Next round.";
    _message->setText(message);
  }
}

} // namespace tetris::qtapp
