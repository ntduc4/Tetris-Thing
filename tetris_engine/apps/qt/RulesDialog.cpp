#include "RulesDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <cstdint>
#include <limits>

namespace tetris::qtapp {
namespace {
QSpinBox *number(int lo, int hi, QWidget *parent) {
  auto *field = new QSpinBox(parent);
  field->setRange(lo, hi);
  return field;
}

QDoubleSpinBox *decimal(double hi, int digits, QWidget *parent) {
  auto *field = new QDoubleSpinBox(parent);
  field->setRange(0, hi);
  field->setDecimals(digits);
  field->setSingleStep(digits > 2 ? 0.001 : 0.25);
  return field;
}

QComboBox *options(std::initializer_list<const char *> items, QWidget *parent) {
  auto *field = new QComboBox(parent);
  for (auto *item : items)
    field->addItem(item);
  return field;
}

template <typename T, size_t N>
QString format_table(const std::array<T, N> &values) {
  QStringList text;
  for (auto value : values)
    text << QString::number(value);
  return text.join(", ");
}

template <typename T, size_t N>
bool parse_table(const QString &text, std::array<T, N> &values) {
  auto parts = text.split(',');
  if (parts.size() != static_cast<qsizetype>(N))
    return false;
  for (size_t i = 0; i < N; ++i) {
    bool ok = false;
    auto value = parts[static_cast<qsizetype>(i)].trimmed().toULongLong(&ok);
    if (!ok || value > std::numeric_limits<T>::max())
      return false;
    values[i] = static_cast<T>(value);
  }
  return true;
}
} // namespace

RulesDialog::RulesDialog(GameConfig &config, bool multiplayer, QWidget *parent)
    : QDialog(parent), _config(config) {
  setWindowTitle(multiplayer ? "Local room rules" : "Solo / Zen rules");
  resize(520, 630);
  auto *layout = new QVBoxLayout(this);
  _preset = options({"TETR.IO-like", "Classic-style", "Custom"}, this);
  auto *header = new QFormLayout;
  header->addRow("Preset (approximate):", _preset);
  layout->addLayout(header);
  auto *tabs = new QTabWidget(this);
  layout->addWidget(tabs);

  auto *game = new QWidget(tabs);
  auto *g = new QFormLayout(game);
  _width = number(4, 20, game);
  _width->setObjectName("boardWidth");
  _height = number(4, 80, game);
  _height->setObjectName("boardHeight");
  _gravity = decimal(40, 4, game);
  _gravity->setObjectName("gravity");
  _acceleration = decimal(1, 5, game);
  _margin = number(0, 1000000, game);
  _lock = number(0, 600, game);
  _resets = number(0, 1000, game);
  _garbage = number(0, 600, game);
  _das = number(0, 60, game);
  _arr = number(0, 60, game);
  _das->setToolTip("Initial held left/right delay (18 ticks is about 0.3 seconds). "
                   "Charge carries across locks and hold swaps.");
  _arr->setToolTip("Held left/right repeat interval. Zero moves to the wall; "
                   "its timing carries across pieces.");
  _soft = number(1, 80, game);
  _goal = number(1, 1000, game);
  _time = number(1, 3600, game);
  _wins = number(1, 20, game);
  g->addRow("Board width:", _width);
  g->addRow("Internal height (20 visible):", _height);
  g->addRow("Gravity (cells / tick):", _gravity);
  g->addRow("Gravity increase / tick:", _acceleration);
  g->addRow("Increase begins at tick:", _margin);
  g->addRow("Lock delay (ticks):", _lock);
  g->addRow("Move resets per piece:", _resets);
  g->addRow("Incoming garbage delay (ticks):", _garbage);
  g->addRow("DAS (ticks):", _das);
  g->addRow("ARR (ticks; 0 = instant):", _arr);
  g->addRow("Soft drop (cells / tick):", _soft);
  g->addRow("Sprint goal (lines):", _goal);
  g->addRow("Blitz time (seconds):", _time);
  g->addRow("Room wins to win:", _wins);
  tabs->addTab(game, "Timing & board");

  auto *rules = new QWidget(tabs);
  auto *r = new QFormLayout(rules);
  _spin = options({"All-Mini+", "All-Mini", "T-spins", "No spins"}, rules);
  _spin->setObjectName("spinRule");
  _rotation = options({"SRS+", "SRS-X", "No rotation"}, rules);
  _rotation->setObjectName("rotationRule");
  _bag = options({"7-bag", "14-bag", "Random"}, rules);
  _attack = options({"Season 2 (1v1 attack)", "Season 1-like", "Guideline (2009 attack)",
                     "Custom table", "No attack"}, rules);
  _attack->setObjectName("attackRule");
  r->addRow("Spin detection:", _spin);
  r->addRow("Rotation:", _rotation);
  r->addRow("Randomizer:", _bag);
  r->addRow("Attack scheme:", _attack);
  auto *note = new QLabel("Preset attacks are approximations, not exact seasonal parity.\n"
                          "Custom tables below apply when 'Custom table' is selected.", rules);
  note->setWordWrap(true);
  r->addRow(note);
  tabs->addTab(rules, "Rules");

  auto *tables = new QWidget(tabs);
  auto *t = new QFormLayout(tables);
  constexpr const char *labels[] = {"Normal", "Mini spin", "Full spin"};
  for (int i = 0; i < 3; ++i) {
    _attack_lines[i] = new QLineEdit(tables);
    _attack_lines[i]->setObjectName(QString("attackTable_%1").arg(i));
    t->addRow(QString("%1 attack (0–4 lines):").arg(labels[i]), _attack_lines[i]);
  }
  _attack_b2b = number(0, 1000, tables);
  _attack_pc = number(0, 1000, tables);
  _attack_combo = decimal(100, 2, tables);
  t->addRow("Attack B2B bonus:", _attack_b2b);
  t->addRow("Attack perfect clear:", _attack_pc);
  t->addRow("Attack combo multiplier:", _attack_combo);
  for (int i = 0; i < 3; ++i) {
    _score_lines[i] = new QLineEdit(tables);
    _score_lines[i]->setObjectName(QString("scoreTable_%1").arg(i));
    t->addRow(QString("%1 score (0–4 lines):").arg(labels[i]), _score_lines[i]);
  }
  _score_combo = number(0, 1000000, tables);
  _score_pc = number(0, 1000000, tables);
  _hard_points = number(0, 10000, tables);
  _soft_points = number(0, 10000, tables);
  t->addRow("Score per combo:", _score_combo);
  t->addRow("Score perfect clear:", _score_pc);
  t->addRow("Hard drop points / cell:", _hard_points);
  t->addRow("Soft drop points / cell:", _soft_points);
  auto *scroll = new QScrollArea(tabs);
  scroll->setWidgetResizable(true);
  scroll->setWidget(tables);
  tabs->addTab(scroll, "Attack & score tables");

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                       QDialogButtonBox::Cancel, this);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, [this] { save(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(_preset, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
    if (index < 2)
      load(index == 0 ? GameConfig{} : classic_config());
  });

  for (auto *field : {_width, _height, _margin, _lock, _resets, _garbage,
                      _das, _arr, _soft, _goal, _time, _wins, _attack_b2b,
                      _attack_pc, _score_combo, _score_pc, _hard_points,
                      _soft_points})
    connect(field, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this] { mark_custom(); });
  for (auto *field : {_gravity, _acceleration, _attack_combo})
    connect(field, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this] { mark_custom(); });
  for (auto *field : {_spin, _rotation, _bag, _attack})
    connect(field, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { mark_custom(); });
  for (auto *field : _attack_lines)
    connect(field, &QLineEdit::textEdited, this, [this] { mark_custom(); });
  for (auto *field : _score_lines)
    connect(field, &QLineEdit::textEdited, this, [this] { mark_custom(); });
  load(config);
  _preset->setCurrentIndex(static_cast<int>(config.preset));
}

void RulesDialog::mark_custom() {
  if (!_loading)
    _preset->setCurrentIndex(2);
}

void RulesDialog::load(const GameConfig &c) {
  _loading = true;
  _width->setValue(c.width);
  _height->setValue(c.height);
  _gravity->setValue(c.gravity);
  _acceleration->setValue(c.gravity_increase);
  _margin->setValue(static_cast<int>(c.gravity_margin));
  _lock->setValue(c.lock_delay);
  _resets->setValue(c.lock_resets);
  _garbage->setValue(c.garbage_delay);
  _das->setValue(c.das);
  _arr->setValue(c.arr);
  _soft->setValue(c.soft_drop);
  _goal->setValue(c.sprint_goal);
  _time->setValue(c.blitz_seconds);
  _wins->setValue(c.wins_to_win);
  _spin->setCurrentIndex(static_cast<int>(c.spin));
  _rotation->setCurrentIndex(static_cast<int>(c.rotation));
  _bag->setCurrentIndex(static_cast<int>(c.bag));
  _attack->setCurrentIndex(static_cast<int>(c.attack));
  _attack_lines[0]->setText(format_table(c.attacks.normal));
  _attack_lines[1]->setText(format_table(c.attacks.mini));
  _attack_lines[2]->setText(format_table(c.attacks.full));
  _attack_b2b->setValue(c.attacks.b2b_bonus);
  _attack_pc->setValue(c.attacks.perfect_clear_bonus);
  _attack_combo->setValue(c.attacks.combo_multiplier);
  _score_lines[0]->setText(format_table(c.scores.normal));
  _score_lines[1]->setText(format_table(c.scores.mini));
  _score_lines[2]->setText(format_table(c.scores.full));
  _score_combo->setValue(static_cast<int>(c.scores.combo_bonus));
  _score_pc->setValue(static_cast<int>(c.scores.perfect_clear_bonus));
  _hard_points->setValue(c.scores.hard_drop_point);
  _soft_points->setValue(c.scores.soft_drop_point);
  _loading = false;
}

void RulesDialog::save() {
  GameConfig c = _config;
  c.width = _width->value();
  c.height = _height->value();
  c.gravity = _gravity->value();
  c.gravity_increase = _acceleration->value();
  c.gravity_margin = _margin->value();
  c.lock_delay = _lock->value();
  c.lock_resets = _resets->value();
  c.garbage_delay = _garbage->value();
  c.das = _das->value();
  c.arr = _arr->value();
  c.soft_drop = _soft->value();
  c.sprint_goal = _goal->value();
  c.blitz_seconds = _time->value();
  c.wins_to_win = _wins->value();
  c.spin = static_cast<SpinRule>(_spin->currentIndex());
  c.rotation = static_cast<RotationRule>(_rotation->currentIndex());
  c.bag = static_cast<BagRule>(_bag->currentIndex());
  c.attack = static_cast<AttackRule>(_attack->currentIndex());
  c.preset = static_cast<Preset>(_preset->currentIndex());
  if (!parse_table(_attack_lines[0]->text(), c.attacks.normal) ||
      !parse_table(_attack_lines[1]->text(), c.attacks.mini) ||
      !parse_table(_attack_lines[2]->text(), c.attacks.full) ||
      !parse_table(_score_lines[0]->text(), c.scores.normal) ||
      !parse_table(_score_lines[1]->text(), c.scores.mini) ||
      !parse_table(_score_lines[2]->text(), c.scores.full)) {
    QMessageBox::warning(this, "Invalid table",
                         "Each table needs exactly five non-negative integers "
                         "(for 0, 1, 2, 3, and 4 lines).");
    return;
  }
  c.attacks.b2b_bonus = _attack_b2b->value();
  c.attacks.perfect_clear_bonus = _attack_pc->value();
  c.attacks.combo_multiplier = _attack_combo->value();
  c.scores.combo_bonus = _score_combo->value();
  c.scores.perfect_clear_bonus = _score_pc->value();
  c.scores.hard_drop_point = _hard_points->value();
  c.scores.soft_drop_point = _soft_points->value();
  _config = c;
  accept();
}

} // namespace tetris::qtapp
