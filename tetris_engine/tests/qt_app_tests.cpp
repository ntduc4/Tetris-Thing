#include "BoardWidget.hpp"
#include "MainWindow.hpp"
#include "RulesDialog.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>
#include <QTimer>

#include <iostream>

using namespace tetris;
using namespace tetris::qtapp;

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  MainWindow window;
  window.show();
  window.activateWindow();
  window.setFocus();
  app.processEvents();
  auto check = [](bool valid, const char *message) {
    if (!valid) {
      std::cerr << "Qt app test failed: " << message << '\n';
      std::exit(1);
    }
  };

  const Engine *solo = window.current_engine();
  check(solo && solo->active_piece(), "Zen should start with an active piece");
  check(cell_color(Cell::I) != cell_color(Cell::O) &&
            cell_color(Cell::J) != cell_color(Cell::L) &&
            cell_color(Cell::Garbage) != cell_color(Cell::Empty),
        "tetrominoes and garbage must have distinct palette colors");
  int initial_col = solo->active_piece()->pos.col;
  QTest::keyClick(&window, Qt::Key_D);
  check(solo->active_piece()->pos.col == initial_col + 1,
        "player 1 keyboard movement must reach the engine");
  auto *draw = window.findChild<QPushButton *>("drawMode");
  auto *palette = window.findChild<QComboBox *>("palette");
  auto *board = static_cast<BoardWidget *>(window.findChild<QWidget *>("board1"));
  draw->click();
  check(solo->edit_mode(), "solo draw mode should open");
  palette->setCurrentIndex(8);
  int size = std::min((board->width() - 16) / solo->board().get_width(),
                      (board->height() - 16) / std::min<int>(VisibleHeight, solo->board().get_height()));
  QPoint target((board->width() - size * solo->board().get_width()) / 2 + size / 2,
                (board->height() - size * VisibleHeight) / 2 +
                    (VisibleHeight - 1) * size + size / 2);
  QTest::mouseClick(board, Qt::LeftButton, {}, target);
  check(solo->board().get(0, 0) == Cell::Garbage,
        "mouse draw should paint garbage in the chosen cell");
  app.processEvents();
  check(board->grab().toImage().pixelColor(target) == cell_color(Cell::Garbage),
        "painted garbage must render in its palette color");
  QTest::mouseClick(board, Qt::RightButton, {}, target);
  check(solo->board().get(0, 0) == Cell::Empty,
        "right click should erase the cell");
  draw->click();
  check(!solo->edit_mode(), "solo draw mode should resume gameplay");

  // Applying solo rules rebuilds only the solo board.
  QTimer::singleShot(0, [&window] {
    auto *dialog = static_cast<RulesDialog *>(window.findChild<QDialog *>());
    dialog->findChild<QSpinBox *>("boardWidth")->setValue(12);
    dialog->findChild<QDoubleSpinBox *>("gravity")->setValue(3.0);
    dialog->findChild<QComboBox *>("spinRule")->setCurrentIndex(3);
    dialog->findChild<QComboBox *>("rotationRule")->setCurrentIndex(1);
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
  });
  window.findChild<QPushButton *>("rules")->click();
  check(window.current_engine()->board().get_width() == 12,
        "solo rules should apply the custom board width");
  check(!window.current_engine()->try_rotate_cw(),
        "solo rule selection should disable rotation when requested");
  int before_gravity = window.current_engine()->active_piece()->pos.row;
  window.current_engine()->tick();
  check(window.current_engine()->active_piece()->pos.row == before_gravity - 3,
        "solo gravity setting should change falling speed");

  window.findChild<QComboBox *>("mode")->setCurrentIndex(4);
  check(window.current_engine(0) && window.current_engine(1),
        "local room should create two engines");
  check(window.current_engine(0)->board().get_width() == 10,
        "room rules should be independent from solo rules");
  check(window.current_engine(0)->try_rotate_cw(),
        "room rotation rules should be independent of solo rotation rules");
  check(window.current_engine(0)->preview_queue() ==
            window.current_engine(1)->preview_queue(),
        "room players should share a piece sequence");
  window.activateWindow();
  window.setFocus();
  app.processEvents();
  int p1_col = window.current_engine(0)->active_piece()->pos.col;
  int p2_col = window.current_engine(1)->active_piece()->pos.col;
  QTest::keyClick(&window, Qt::Key_D);
  QTest::keyClick(&window, Qt::Key_Right);
  check(window.current_engine(0)->active_piece()->pos.col == p1_col + 1 &&
            window.current_engine(1)->active_piece()->pos.col == p2_col + 1,
        "both players must move independently on the same keyboard");
  QTest::keyClick(&window, Qt::Key_Space);
  check(window.current_engine(0)->board().max_height() > 0 &&
            window.current_engine(1)->board().max_height() == 0,
        "player 1 hard drop should only lock on player 1's board");
  QTest::keyClick(&window, Qt::Key_Return);
  check(window.current_engine(1)->board().max_height() > 0,
        "player 2 hard drop should lock on player 2's board");

  QTimer::singleShot(0, [&window] {
    auto *dialog = static_cast<RulesDialog *>(window.findChild<QDialog *>());
    dialog->findChild<QSpinBox *>("boardWidth")->setValue(14);
    dialog->findChild<QDoubleSpinBox *>("gravity")->setValue(2.0);
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
  });
  window.findChild<QPushButton *>("rules")->click();
  check(window.current_engine(0)->board().get_width() == 14 &&
            window.current_engine(1)->board().get_width() == 14,
        "local room rules must apply to both players");
  int before_room = window.current_engine(0)->active_piece()->pos.row;
  window.current_engine(0)->tick();
  window.current_engine(1)->tick();
  check(window.current_engine(0)->active_piece()->pos.row == before_room - 2 &&
            window.current_engine(1)->active_piece()->pos.row == before_room - 2,
        "local room timing rules should affect both engines equally");
  QTimer::singleShot(0, [&window] {
    auto *dialog = window.findChild<QDialog *>();
    dialog->findChild<QKeySequenceEdit *>("binding_0_0")
        ->setKeySequence(QKeySequence(Qt::Key_H));
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
  });
  window.findChild<QPushButton *>("keybinds")->click();
  window.activateWindow();
  window.setFocus();
  app.processEvents();
  int changed_col = window.current_engine(0)->active_piece()->pos.col;
  QTest::keyClick(&window, Qt::Key_H);
  check(window.current_engine(0)->active_piece()->pos.col == changed_col - 1,
        "rebinding a key should update the shared keyboard controls");
  std::cout << "Qt solo editor, keyboard, and local room tests passed\n";
  return 0;
}
