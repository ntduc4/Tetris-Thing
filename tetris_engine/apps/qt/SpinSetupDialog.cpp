#include "SpinSetupDialog.hpp"
#include "GameConfig.hpp"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QVBoxLayout>

#include <vector>

namespace tetris::qtapp {
namespace {
constexpr const char *Cells = ".IOTSZJLGX";
constexpr const char *Pieces = "IOTSZJL";
constexpr int BoardWidth = 10;
constexpr int BoardHeight = 40;

QString parse_pieces(const QString &input, QString *error) {
  QString result;
  for (QChar c : input.toUpper()) {
    if (c.isSpace() || c == ',') continue;
    if (!QString::fromLatin1(Pieces).contains(c)) {
      *error = "Next pieces must use I O T S Z J L only.";
      return {};
    }
    result += c;
  }
  if (result.size() > 63)
    *error = "At most 63 next pieces are supported.";
  return result;
}

int piece_index(const QJsonValue &value) {
  const QString text = value.toString();
  return text.size() == 1 ? QString::fromLatin1(Pieces).indexOf(text) : -1;
}

std::vector<PieceType> scripted_pieces(int active, const QString &next) {
  std::vector<PieceType> pieces{static_cast<PieceType>(active)};
  for (QChar piece : next)
    pieces.push_back(static_cast<PieceType>(
        QString::fromLatin1(Pieces).indexOf(piece)));
  return pieces;
}

int mirrored_piece_index(int index) {
  switch (index) {
  case 3: return 4; // S/Z
  case 4: return 3;
  case 5: return 6; // J/L
  case 6: return 5;
  default: return index;
  }
}

// A template is only useful if every possible seed-selected transform can
// spawn its scripted first piece. Keep this check in the authoring UI, not in
// the normal game or the training loop.
bool all_transforms_spawn(const std::vector<QString> &rows,
                          const std::vector<bool> &filled,
                          int active, int hold, const QString &next) {
  GameConfig rules;
  rules.width = BoardWidth;
  rules.height = BoardHeight;
  rules.gravity = 0;
  rules.gravity_increase = 0;
  const int width = rows.front().size();
  for (int mirror = 0; mirror < 2; ++mirror)
    for (int x = 0; x <= BoardWidth - width; ++x) {
      Engine preview = make_engine(rules);
      preview.reset(0);
      preview.set_opening_pieces(scripted_pieces(
                                     mirror ? mirrored_piece_index(active) : active,
                                     next),
                                 hold < 0 ? std::nullopt :
                                            std::optional{static_cast<PieceType>(hold)});
      preview.set_edit_mode(true);
      for (size_t row = 0; row < rows.size(); ++row) {
        QString cells(BoardWidth, filled[row] ? QChar('G') : QChar('.'));
        for (int col = 0; col < width; ++col)
          cells[x + (mirror ? width - 1 - col : col)] = rows[row][col];
        for (int col = 0; col < BoardWidth; ++col)
          if (cells[col] != '.')
            preview.paint_cell(row, col, static_cast<Cell>(
                QString::fromLatin1(Cells).indexOf(cells[col])));
      }
      preview.set_edit_mode(false);
      if (!preview.spawn_next_piece()) return false;
    }
  return true;
}
} // namespace

SpinSetupDialog::SpinSetupDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("AI spin template editor");
  resize(530, 710);
  auto *layout = new QVBoxLayout(this);
  auto *guide = new QLabel(
      "Draw a small bottom-anchored template. F fills the rest of that row "
      "with garbage; O (overhang) leaves the rest empty. Click F/O to toggle. "
      "Each AI reset randomly shifts and mirrors this drawing using its seed; "
      "S/Z and J/L opening pieces mirror with it. "
      "The drawing is not a playable game.", this);
  guide->setWordWrap(true);
  layout->addWidget(guide);

  auto *controls = new QFormLayout;
  _width = new QSpinBox(this);
  _width->setObjectName("setupWidth");
  _width->setRange(1, BoardWidth);
  _width->setValue(3);
  controls->addRow("Template columns:", _width);
  _height = new QSpinBox(this);
  _height->setObjectName("setupHeight");
  _height->setRange(1, 20);
  _height->setValue(3);
  controls->addRow("Template rows:", _height);
  _palette = new QComboBox(this);
  _palette->setObjectName("setupPalette");
  _palette->addItems({"Erase", "I · cyan", "O · yellow", "T · purple",
                      "S · green", "Z · red", "J · blue", "L · orange",
                      "Garbage", "Unclearable"});
  _palette->setCurrentIndex(8);
  controls->addRow("Paint:", _palette);
  _active = new QComboBox(this);
  _active->setObjectName("setupActive");
  _active->addItems({"I", "O", "T", "S", "Z", "J", "L"});
  _active->setCurrentIndex(2);
  controls->addRow("First active piece:", _active);
  _hold = new QComboBox(this);
  _hold->setObjectName("setupHold");
  _hold->addItems({"Empty", "I", "O", "T", "S", "Z", "J", "L"});
  controls->addRow("Starting hold:", _hold);
  _next = new QLineEdit(this);
  _next->setObjectName("setupNext");
  _next->setPlaceholderText("e.g. I O T S Z (or IOTSZ)");
  controls->addRow("Next pieces:", _next);
  layout->addLayout(controls);
  layout->addWidget(new QLabel(
      "Left-drag paints; right-drag erases. Only cells inside the box are "
      "painted. The 7-bag resumes after your chosen next pieces.", this));
  _pattern = new SpinPatternWidget(this);
  _pattern->setObjectName("setupPattern");
  layout->addWidget(_pattern, 1);
  connect(_width, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this] { _pattern->resize_pattern(_width->value(), _height->value()); });
  connect(_height, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this] { _pattern->resize_pattern(_width->value(), _height->value()); });
  connect(_palette, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) { _pattern->set_brush(static_cast<Cell>(index)); });
  _pattern->set_brush(Cell::Garbage);

  auto *buttons = new QHBoxLayout;
  auto *open = new QPushButton("Open JSON…", this);
  open->setObjectName("setupOpen");
  auto *save_button = new QPushButton("Save JSON…", this);
  save_button->setObjectName("setupSave");
  auto *close = new QPushButton("Close", this);
  buttons->addWidget(open);
  buttons->addWidget(save_button);
  buttons->addStretch();
  buttons->addWidget(close);
  layout->addLayout(buttons);
  connect(close, &QPushButton::clicked, this, &QDialog::accept);
  connect(open, &QPushButton::clicked, this, [this] {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open AI spin template", {}, "JSON (*.json)");
    if (path.isEmpty()) return;
    QString error;
    if (!load(path, &error))
      QMessageBox::warning(this, "Could not open template", error);
  });
  connect(save_button, &QPushButton::clicked, this, [this] {
    const QString path = QFileDialog::getSaveFileName(
        this, "Save AI spin template", "spin_setup.json", "JSON (*.json)");
    if (path.isEmpty()) return;
    QString error;
    if (!save(path, &error))
      QMessageBox::warning(this, "Could not save template", error);
  });
}

bool SpinSetupDialog::save(const QString &path, QString *error) const {
  QString problem;
  const QString next = parse_pieces(_next->text(), &problem);
  if (problem.isEmpty() &&
      !all_transforms_spawn(_pattern->rows(), _pattern->filled(),
                            _active->currentIndex(), _hold->currentIndex() - 1, next))
    problem = "At least one horizontal/mirrored position blocks the first piece's spawn.";
  if (!problem.isEmpty()) {
    if (error) *error = problem;
    return false;
  }
  QJsonArray rows, modes;
  for (const QString &row : _pattern->rows()) rows.append(row);
  for (bool filled : _pattern->filled())
    modes.append(filled ? "filled" : "overhang");
  QJsonObject setup{
      {"version", 2}, {"width", BoardWidth}, {"height", BoardHeight},
      {"template", QJsonObject{
           {"width", _pattern->pattern_width()},
           {"height", _pattern->pattern_height()},
           {"rows_bottom_up", rows}, {"row_modes_bottom_up", modes}}},
      {"active_piece", _active->currentText()},
      {"hold_piece", _hold->currentIndex() ? QJsonValue(_hold->currentText())
                                          : QJsonValue(QJsonValue::Null)},
      {"next_pieces", next}};
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(QJsonDocument(setup).toJson(QJsonDocument::Indented)) < 0 ||
      !file.commit()) {
    if (error) *error = file.errorString();
    return false;
  }
  if (error) error->clear();
  return true;
}

bool SpinSetupDialog::load(const QString &path, QString *error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error) *error = file.errorString();
    return false;
  }
  QJsonParseError parse_error;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
  const QJsonObject setup = document.object();
  const int active = piece_index(setup.value("active_piece"));
  const QJsonValue hold_value = setup.value("hold_piece");
  const int hold = hold_value.isNull() ? -1 : piece_index(hold_value);
  QString problem;
  const QString next = parse_pieces(setup.value("next_pieces").toString(), &problem);
  const bool common = parse_error.error == QJsonParseError::NoError &&
      document.isObject() && setup.value("width").toInt() == BoardWidth &&
      setup.value("height").toInt() == BoardHeight &&
      active >= 0 && setup.contains("hold_piece") &&
      (hold_value.isNull() || hold >= 0) &&
      setup.value("next_pieces").isString() && problem.isEmpty();
  int width = 0, height = 0;
  QJsonArray rows_json, modes_json;
  if (setup.value("version").toInt() == 2) {
    const QJsonObject pattern = setup.value("template").toObject();
    width = pattern.value("width").toInt();
    height = pattern.value("height").toInt();
    rows_json = pattern.value("rows_bottom_up").toArray();
    modes_json = pattern.value("row_modes_bottom_up").toArray();
  } else if (setup.value("version").toInt() == 1) {
    // Keep the previously saved full-width editor files loadable.
    width = BoardWidth;
    height = 20;
    const QJsonArray old = setup.value("board_rows_bottom_up").toArray();
    if (old.size() == BoardHeight) {
      for (int row = 0; row < height; ++row) rows_json.append(old[row]);
      for (int row = height; row < BoardHeight; ++row)
        if (old[row].toString() != QString(BoardWidth, '.'))
          problem = "Legacy setup has occupied hidden spawn rows.";
      for (int row = 0; row < height; ++row) modes_json.append("overhang");
    }
  }
  bool valid = common && problem.isEmpty() && width >= 1 && width <= BoardWidth &&
      height >= 1 && height <= 20 && rows_json.size() == height &&
      modes_json.size() == height;
  std::vector<QString> rows;
  std::vector<bool> filled;
  for (int row = 0; valid && row < height; ++row) {
    const QString cells = rows_json[row].toString();
    const QString mode = modes_json[row].toString();
    if (!rows_json[row].isString() || cells.size() != width ||
        !modes_json[row].isString() ||
        (mode != "filled" && mode != "overhang")) {
      valid = false;
      break;
    }
    for (QChar cell : cells)
      if (!QString::fromLatin1(Cells).contains(cell))
        valid = false;
    rows.push_back(cells);
    filled.push_back(mode == "filled");
  }
  if (valid && !all_transforms_spawn(rows, filled, active, hold, next))
    problem = "At least one horizontal/mirrored position blocks the first piece's spawn.";
  if (!valid || !problem.isEmpty()) {
    if (error) *error = problem.isEmpty()
        ? "Invalid 10×40 spin template (1..10 columns, 1..20 bottom rows)."
        : problem;
    return false;
  }
  _width->setValue(width);
  _height->setValue(height);
  _pattern->set_pattern(rows, filled);
  _active->setCurrentIndex(active);
  _hold->setCurrentIndex(hold + 1);
  _next->setText(next);
  if (error) error->clear();
  return true;
}

} // namespace tetris::qtapp
