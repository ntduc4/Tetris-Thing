#include "MainWindow.hpp"
#include "SpinSetupDialog.hpp"
#include "SpinPatternWidget.hpp"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <cstdlib>
#include <iostream>

using namespace tetris;
using namespace tetris::qtapp;

namespace {
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "Qt spin setup test failed: " << message << '\n';
    std::exit(1);
  }
}
} // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QTemporaryDir temporary;
  check(temporary.isValid(), "temporary directory exists");
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());

  MainWindow window;
  window.show();
  app.processEvents();
  auto before = window.current_engine()->board().render();
  QTimer::singleShot(0, [&] {
    auto *dialog = static_cast<SpinSetupDialog *>(window.findChild<QDialog *>());
    check(dialog && dialog->findChild<QWidget *>("setupPattern"),
          "spin setup toolbar opens its own editor");
    dialog->reject();
  });
  window.findChild<QPushButton *>("spinSetup")->click();
  check(window.current_engine()->board().render() == before,
        "opening the editor does not modify a live game");

  SpinSetupDialog editor;
  editor.show();
  app.processEvents();
  auto *board = static_cast<SpinPatternWidget *>(
      editor.findChild<QWidget *>("setupPattern"));
  auto *palette = editor.findChild<QComboBox *>("setupPalette");
  const QRect grid = board->grid_rect();
  const int cell = grid.width() / 3;
  const QPoint bottom_left(grid.left() + cell / 2,
                           grid.top() + 2 * cell + cell / 2);
  palette->setCurrentIndex(8); // Garbage
  QTest::mouseClick(board, Qt::LeftButton, {}, bottom_left);
  palette->setCurrentIndex(3); // T
  QTest::mouseClick(board, Qt::LeftButton, {},
                    bottom_left + QPoint(cell, 0));
  QTest::mouseClick(board, Qt::LeftButton, {},
                    board->row_mode_rect(1).center());
  check(board->filled() == std::vector<bool>({true, false, false}),
        "each template row can independently fill outside or leave overhang");
  auto *active = editor.findChild<QComboBox *>("setupActive");
  auto *hold = editor.findChild<QComboBox *>("setupHold");
  auto *next = editor.findChild<QLineEdit *>("setupNext");
  active->setCurrentText("T");
  hold->setCurrentText("I");
  next->setText("O, S Z");
  const QString path = temporary.filePath("spin.json");
  QString error;
  check(editor.save(path, &error), "save should succeed");
  QFile file(path);
  check(file.open(QIODevice::ReadOnly), "saved JSON can be opened");
  QJsonObject saved = QJsonDocument::fromJson(file.readAll()).object();
  file.close();
  check(saved.value("version").toInt() == 2 &&
            saved.value("width").toInt() == 10 &&
            saved.value("height").toInt() == 40 &&
            saved.value("active_piece").toString() == "T" &&
            saved.value("hold_piece").toString() == "I" &&
            saved.value("next_pieces").toString() == "OSZ",
        "setup persists the initial pieces and format");
  const auto pattern = saved.value("template").toObject();
  const auto rows = pattern.value("rows_bottom_up").toArray();
  const auto modes = pattern.value("row_modes_bottom_up").toArray();
  check(pattern.value("width").toInt() == 3 &&
            pattern.value("height").toInt() == 3 &&
            rows.size() == 3 && rows[0].toString() == "GT." &&
            modes[0].toString() == "filled" &&
            modes[1].toString() == "overhang",
        "bottom-up template stores its n×m drawing and fill modes");
  SpinSetupDialog reopened;
  check(reopened.load(path, &error), "saved setup can be loaded again");
  check(reopened.findChild<QComboBox *>("setupHold")->currentText() == "I" &&
            reopened.findChild<QLineEdit *>("setupNext")->text() == "OSZ" &&
            reopened.findChild<QSpinBox *>("setupWidth")->value() == 3,
        "editor restores size, hold and next sequence");
  const QString again = temporary.filePath("roundtrip.json");
  check(reopened.save(again, &error), "loaded setup can be saved again");
  QFile copied(again);
  check(copied.open(QIODevice::ReadOnly) &&
            QJsonDocument::fromJson(copied.readAll()).object() == saved,
        "JSON roundtrip keeps the painted board and opening");

  saved.insert("active_piece", "not a piece");
  const QString invalid = temporary.filePath("bad.json");
  QFile bad(invalid);
  check(bad.open(QIODevice::WriteOnly), "create invalid fixture");
  bad.write(QJsonDocument(saved).toJson());
  bad.close();
  check(!reopened.load(invalid, &error) && !error.isEmpty(),
        "invalid setups must be rejected before editing the board");
  check(reopened.save(again, &error), "invalid load leaves the editor usable");
  reopened.findChild<QSpinBox *>("setupWidth")->setValue(5);
  reopened.findChild<QSpinBox *>("setupHeight")->setValue(4);
  check(reopened.save(again, &error), "resized template remains valid");
  QFile resized(again);
  check(resized.open(QIODevice::ReadOnly), "resized template is readable");
  const QJsonObject resized_pattern =
      QJsonDocument::fromJson(resized.readAll()).object().value("template").toObject();
  check(resized_pattern.value("width").toInt() == 5 &&
            resized_pattern.value("height").toInt() == 4 &&
            resized_pattern.value("rows_bottom_up").toArray()[0].toString() == "GT..." &&
            resized_pattern.value("row_modes_bottom_up").toArray()[3].toString() == "overhang",
        "n×m resizing preserves painted cells and initializes new rows");
  resized.close();
  reopened.findChild<QSpinBox *>("setupWidth")->setValue(3);
  reopened.show();
  app.processEvents();
  auto *reopened_board =
      static_cast<SpinPatternWidget *>(reopened.findChild<QWidget *>("setupPattern"));
  reopened.findChild<QSpinBox *>("setupHeight")->setValue(20);
  const QRect reopened_grid = reopened_board->grid_rect();
  const int square = reopened_grid.width() / 3;
  const QPoint blocked_spawn(reopened_grid.left() + square / 2,
                              reopened_grid.top() + square / 2);
  reopened.findChild<QComboBox *>("setupPalette")->setCurrentIndex(8);
  for (int col = 0; col < 3; ++col)
    QTest::mouseClick(reopened_board, Qt::LeftButton, {},
                      blocked_spawn + QPoint(col * square, 0));
  QTest::mouseClick(reopened_board, Qt::LeftButton, {},
                    reopened_board->row_mode_rect(19).center());
  check(!reopened.save(again, &error) && error.contains("spawn"),
        "editor rejects a template whose transforms block the first spawn");

  QJsonObject legacy{
      {"version", 1}, {"width", 10}, {"height", 40},
      {"active_piece", "T"}, {"hold_piece", QJsonValue(QJsonValue::Null)},
      {"next_pieces", ""}};
  QJsonArray old_board;
  old_board.append("G.........");
  for (int row = 1; row < 40; ++row) old_board.append("..........");
  legacy.insert("board_rows_bottom_up", old_board);
  const QString old_path = temporary.filePath("v1.json");
  QFile old(old_path);
  check(old.open(QIODevice::WriteOnly), "create legacy fixture");
  old.write(QJsonDocument(legacy).toJson());
  old.close();
  check(reopened.load(old_path, &error) &&
            reopened.findChild<QSpinBox *>("setupWidth")->value() == 10,
        "the modular editor can still import existing fixed-board setups");
  std::cout << "Qt spin setup editor and JSON roundtrip passed\n";
  return 0;
}
