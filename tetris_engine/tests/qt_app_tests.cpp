#include "BoardWidget.hpp"
#include "MainWindow.hpp"
#include "PieceQueueWidget.hpp"
#include "RulesDialog.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QImage>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <iostream>
#include <optional>

using namespace tetris;
using namespace tetris::qtapp;

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QTemporaryDir settings_dir;
  if (!settings_dir.isValid())
    return 1;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
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
  auto check_preview = [&check](QWidget *widget, const QRect &slot,
                                std::optional<PieceType> type) {
    const QImage image = widget->grab().toImage();
    const int cell = std::min((slot.width() - 10) / 4, (slot.height() - 10) / 4);
    const int left = slot.x() + (slot.width() - 4 * cell) / 2;
    const int top = slot.y() + (slot.height() - 4 * cell) / 2;
    const auto cells = type ? std::optional{get_from_piece_type(*type).render()}
                            : std::nullopt;
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        Cell piece_cell = cells ? (*cells)[row][col] : Cell::Empty;
        const QColor expected = piece_cell == Cell::Empty
                                    ? QColor("#1c2637") : cell_color(piece_cell);
        QPoint center(left + col * cell + cell / 2,
                      top + (3 - row) * cell + cell / 2);
        check(image.pixelColor(center) == expected,
              "the preview must render every cell in its full 4x4 grid");
      }
    }
  };

  const Engine *solo = window.current_engine();
  check(solo && solo->active_piece(), "Zen should start with an active piece");
  auto *queue1 = static_cast<PieceQueueWidget *>(window.findChild<QWidget *>("queue1"));
  auto *queue2 = static_cast<PieceQueueWidget *>(window.findChild<QWidget *>("queue2"));
  auto *hold1 = static_cast<PieceHoldWidget *>(window.findChild<QWidget *>("hold1"));
  auto *hold2 = static_cast<PieceHoldWidget *>(window.findChild<QWidget *>("hold2"));
  auto *streak1 = window.findChild<QLabel *>("streak1");
  auto *streak2 = window.findChild<QLabel *>("streak2");
  check(queue1 && queue2 && queue1->pieces().size() == 5 &&
            queue1->pieces() == solo->preview_queue(),
        "solo must render five upcoming pieces in the queue");
  check(hold1 && hold2 && !hold1->piece() && !hold2->piece(),
        "both players' hold slots must start empty");
  check(streak1 && streak2 && streak1->text().contains("B2B") &&
            streak1->text().contains("COMBO") &&
            streak1->text().contains("SURGE") &&
            streak1->text().contains("0 / 4"),
        "solo must show B2B, combo and empty Season 2 surge beside the board");
  app.processEvents();
  const QRect hold_slot(3, 28, hold1->width() - 6, hold1->height() - 31);
  check_preview(hold1, hold_slot, std::nullopt);
  const int slot_height = std::min(82, (queue1->height() - 28) / 5);
  for (int i = 0; i < 5; ++i)
    check_preview(queue1, QRect(3, 28 + i * slot_height,
                                queue1->width() - 6, slot_height - 3),
                  queue1->pieces()[i]);
  bool painted_piece = false;
  const QColor expected = cell_color(get_from_piece_type(queue1->pieces()[0]).cell_type());
  auto preview_image = queue1->grab().toImage();
  for (int y = 0; y < preview_image.height() && !painted_piece; ++y)
    for (int x = 0; x < preview_image.width(); ++x)
      if (preview_image.pixelColor(x, y) == expected)
        painted_piece = true;
  check(painted_piece, "the next queue must show piece shapes, not just letters");
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
  auto solo_before_drop = queue1->pieces();
  QTest::keyClick(&window, Qt::Key_Space);
  check(queue1->pieces().size() == 5 &&
            queue1->pieces()[0] == solo_before_drop[1] &&
            queue1->pieces() == solo->preview_queue(),
        "the five-piece preview must advance after a solo drop");
  const auto solo_hold_type = solo->active_piece()->piece.type();
  const auto queue_before_hold = queue1->pieces();
  QTest::keyClick(&window, Qt::Key_C);
  check(hold1->piece() == solo_hold_type &&
            queue1->pieces()[0] == queue_before_hold[1],
        "holding a new piece should fill the solo hold slot and advance the queue");
  check_preview(hold1, hold_slot, solo_hold_type);
  QTest::keyClick(&window, Qt::Key_C);
  check(hold1->piece() == solo_hold_type,
        "a second hold before locking must leave the preview unchanged");
  QTest::keyClick(&window, Qt::Key_Space);
  const auto before_swap = solo->active_piece()->piece.type();
  const auto queue_before_swap = queue1->pieces();
  QTest::keyClick(&window, Qt::Key_C);
  check(hold1->piece() == before_swap &&
            solo->active_piece()->piece.type() == solo_hold_type &&
            queue1->pieces() == queue_before_swap,
        "swapping the held piece should update hold without consuming the queue");
  check_preview(hold1, hold_slot, before_swap);
  window.findChild<QPushButton *>("restart")->click();
  solo = window.current_engine();
  check(!hold1->piece() && queue1->pieces() == solo->preview_queue(),
        "restarting solo must clear hold and refresh the queue");
  check_preview(hold1, hold_slot, std::nullopt);

  // Applying solo rules rebuilds only the solo board.
  QTimer::singleShot(0, [&window] {
    auto *dialog = static_cast<RulesDialog *>(window.findChild<QDialog *>());
    dialog->findChild<QSpinBox *>("boardWidth")->setValue(12);
    dialog->findChild<QDoubleSpinBox *>("gravity")->setValue(3.0);
    dialog->findChild<QComboBox *>("spinRule")->setCurrentIndex(3);
    dialog->findChild<QComboBox *>("rotationRule")->setCurrentIndex(2);
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
  check(queue1->pieces() == window.current_engine(0)->preview_queue() &&
            queue2->pieces() == window.current_engine(1)->preview_queue() &&
            queue1->pieces().size() == 5 && queue2->pieces().size() == 5,
        "the local room must render a five-piece queue for each player");
  check(streak1->text().contains("SURGE") && streak2->text().contains("SURGE"),
        "both room players must have their own Season 2 surge indicator");
  check(!hold1->piece() && !hold2->piece(),
        "switching to the local room must clear both hold previews");
  app.processEvents();
  auto *board2 = window.findChild<QWidget *>("board2");
  check(hold1->isVisible() && hold2->isVisible() &&
            hold1->geometry().right() < board->geometry().left() &&
            board->geometry().right() < queue1->geometry().left() &&
            hold2->geometry().right() < board2->geometry().left() &&
            board2->geometry().right() < queue2->geometry().left(),
        "each local board must display hold on the left and queue on the right");
  check(streak1->isVisible() && streak2->isVisible() &&
            streak1->geometry().top() > hold1->geometry().bottom() &&
            streak2->geometry().top() > hold2->geometry().bottom() &&
            streak1->geometry().right() < board->geometry().left() &&
            streak2->geometry().right() < board2->geometry().left(),
        "B2B, combo and surge should display alongside each board below hold");
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
  const auto room_queue1 = queue1->pieces();
  const auto room_queue2 = queue2->pieces();
  QTest::keyClick(&window, Qt::Key_Space);
  check(window.current_engine(0)->board().max_height() > 0 &&
            window.current_engine(1)->board().max_height() == 0,
        "player 1 hard drop should only lock on player 1's board");
  check(queue1->pieces()[0] == room_queue1[1] && queue2->pieces() == room_queue2,
        "dropping must only advance the active player's queue");
  auto queue1_after_drop = queue1->pieces();
  const auto p2_hold_type = window.current_engine(1)->active_piece()->piece.type();
  QTest::keyClick(&window, Qt::Key_L);
  check(queue2->pieces()[0] == room_queue2[1] &&
            queue1->pieces() == queue1_after_drop &&
            hold2->piece() == p2_hold_type && !hold1->piece(),
        "holding a new piece must update only that player's hold and queue");
  check_preview(hold2, hold_slot, p2_hold_type);
  QTest::keyClick(&window, Qt::Key_Return);
  check(window.current_engine(1)->board().max_height() > 0,
        "player 2 hard drop should lock on player 2's board");
  const auto p2_before_swap = window.current_engine(1)->active_piece()->piece.type();
  const auto queue2_before_swap = queue2->pieces();
  QTest::keyClick(&window, Qt::Key_L);
  check(hold2->piece() == p2_before_swap &&
            queue2->pieces() == queue2_before_swap && !hold1->piece(),
        "swapping player 2's hold must leave both queues and player 1's hold unchanged");

  QTimer::singleShot(0, [&window] {
    auto *dialog = static_cast<RulesDialog *>(window.findChild<QDialog *>());
    dialog->findChild<QSpinBox *>("boardWidth")->setValue(14);
    dialog->findChild<QDoubleSpinBox *>("gravity")->setValue(2.0);
    dialog->findChild<QComboBox *>("rotationRule")->setCurrentIndex(1);
    dialog->findChild<QComboBox *>("attackRule")->setCurrentIndex(1);
    dialog->findChild<QLineEdit *>("attackTable_0")->setText("0, 9, 1, 2, 4");
    dialog->findChild<QLineEdit *>("scoreTable_0")->setText("0, 111, 300, 500, 800");
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
  });
  window.findChild<QPushButton *>("rules")->click();
  check(window.current_engine(0)->board().get_width() == 14 &&
            window.current_engine(1)->board().get_width() == 14,
        "local room rules must apply to both players");
  check(!hold1->piece() && !hold2->piece(),
        "applying room rules must reset both hold slots");
  check(streak1->text().contains("B2B") && streak2->text().contains("COMBO") &&
            !streak1->text().contains("SURGE") &&
            !streak2->text().contains("SURGE"),
        "B2B and combo must remain visible while non-Season-2 surge is hidden");
  check_preview(hold2, hold_slot, std::nullopt);
  bool kept_srs_x = false;
  QTimer::singleShot(0, [&window, &kept_srs_x] {
    auto *dialog = window.findChild<QDialog *>();
    kept_srs_x = dialog->findChild<QComboBox *>("rotationRule")->currentIndex() == 1;
    dialog->reject();
  });
  window.findChild<QPushButton *>("rules")->click();
  check(kept_srs_x, "the room must keep its SRS-X selection after applying rules");
  GameConfig selected;
  RulesDialog selection(selected, true);
  selection.findChild<QComboBox *>("rotationRule")->setCurrentIndex(1);
  auto *attack_choice = selection.findChild<QComboBox *>("attackRule");
  check(attack_choice->itemText(2).contains("Guideline"),
        "the former Jstris rule should be labeled Guideline");
  attack_choice->setCurrentIndex(2);
  selection.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
  check(selected.attack == AttackRule::Guideline &&
            selected.rotation == RotationRule::SRSX &&
            SRSX{}.kick_offsets(PieceType::I, Rotation::North, Rotation::East) !=
                SRSPlus{}.kick_offsets(PieceType::I, Rotation::North, Rotation::East),
        "Qt should expose Guideline attack at the old selector index and SRS-X rotation");
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
    dialog->findChild<QKeySequenceEdit *>("binding_1_0")
        ->setKeySequence(QKeySequence(Qt::Key_U));
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
  PieceQueueWidget samples;
  samples.resize(90, 500);
  samples.set_pieces({PieceType::I, PieceType::O, PieceType::T,
                      PieceType::S, PieceType::Z});
  for (int i = 0; i < 5; ++i)
    check_preview(&samples, QRect(3, 28 + i * 82, 84, 79), samples.pieces()[i]);
  samples.set_pieces({PieceType::J, PieceType::L});
  for (int i = 0; i < 5; ++i)
    check_preview(&samples, QRect(3, 28 + i * 82, 84, 79),
                  i < 2 ? std::optional{samples.pieces()[i]} : std::nullopt);

  window.hide();
  {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TetrisEngine", "Tetris");
    check(settings.contains("solo/width") && settings.contains("room/width") &&
              settings.contains("bindings/player1") &&
              settings.contains("bindings/player2"),
          "accepted solo, room and both players' keybinds should save automatically");
  }
  {
    MainWindow restored;
    restored.show();
    restored.activateWindow();
    restored.setFocus();
    app.processEvents();
    check(restored.current_engine()->board().get_width() == 12 &&
              !restored.current_engine()->try_rotate_cw(),
          "reopening should apply saved solo board and spin/rotation settings");
    restored.findChild<QComboBox *>("mode")->setCurrentIndex(4);
    check(restored.current_engine(0)->board().get_width() == 14 &&
              restored.current_engine(1)->board().get_width() == 14 &&
              restored.current_engine(0)->try_rotate_cw() &&
              !restored.findChild<QLabel *>("streak1")->text().contains("SURGE"),
          "reopening should apply separate room rules to both boards");
    bool restored_tables = false;
    QTimer::singleShot(0, [&restored, &restored_tables] {
      auto *dialog = restored.findChild<QDialog *>();
      restored_tables = dialog->findChild<QLineEdit *>("attackTable_0")->text() ==
                            "0, 9, 1, 2, 4" &&
                        dialog->findChild<QLineEdit *>("scoreTable_0")->text() ==
                            "0, 111, 300, 500, 800" &&
                        dialog->findChild<QDoubleSpinBox *>("gravity")->value() == 2.0;
      dialog->findChild<QSpinBox *>("boardWidth")->setValue(18);
      dialog->reject();
    });
    restored.findChild<QPushButton *>("rules")->click();
    check(restored_tables && restored.current_engine(0)->board().get_width() == 14,
          "saved custom tables and timing should reload; cancelling edits should change nothing");
    QTimer::singleShot(0, [&restored] {
      auto *dialog = restored.findChild<QDialog *>();
      dialog->findChild<QKeySequenceEdit *>("binding_1_0")
          ->setKeySequence(QKeySequence(Qt::Key_Z));
      dialog->reject();
    });
    restored.findChild<QPushButton *>("keybinds")->click();
    restored.activateWindow();
    restored.setFocus();
    app.processEvents();
    int p1_left = restored.current_engine(0)->active_piece()->pos.col;
    int p2_left = restored.current_engine(1)->active_piece()->pos.col;
    QTest::keyClick(&restored, Qt::Key_H);
    QTest::keyClick(&restored, Qt::Key_U);
    check(restored.current_engine(0)->active_piece()->pos.col == p1_left - 1 &&
              restored.current_engine(1)->active_piece()->pos.col == p2_left - 1,
          "both players' saved keybinds should work on reopening");
    restored.hide();
  }
  {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TetrisEngine", "Tetris");
    check(settings.value("room/width").toInt() == 14 &&
              settings.value("bindings/player2").toStringList().front() ==
                  QString::number(Qt::Key_U),
          "cancelled rule and keybind changes must not be persisted");
    settings.setValue("solo/width", 999);
    settings.setValue("bindings/player1", QStringList{QString::number(Qt::Key_F)});
  }
  {
    MainWindow recovered;
    check(recovered.current_engine()->board().get_width() == 10,
          "invalid saved rules should fall back to a safe default");
    recovered.show();
    recovered.activateWindow();
    recovered.setFocus();
    app.processEvents();
    int col = recovered.current_engine()->active_piece()->pos.col;
    QTest::keyClick(&recovered, Qt::Key_A);
    check(recovered.current_engine()->active_piece()->pos.col == col - 1,
          "invalid saved bindings should restore the default set");
    recovered.hide();
  }
  {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TetrisEngine", "Tetris");
    settings.clear();
    settings.sync();
  }
  MainWindow streak_window;
  streak_window.show();
  streak_window.activateWindow();
  streak_window.setFocus();
  app.processEvents();
  auto *streak_label = streak_window.findChild<QLabel *>("streak1");
  auto *streak_editor = streak_window.findChild<QPushButton *>("drawMode");
  auto prepare_board = [&](bool quad, bool single) {
    Engine *game = streak_window.current_engine();
    streak_editor->click();
    check(game->edit_mode(), "streak setup should enter draw mode");
    for (uint16_t row = 0; row < game->board().get_height(); ++row)
      for (uint16_t col = 0; col < game->board().get_width(); ++col)
        if (game->board().get(row, col) != Cell::Empty)
          game->paint_cell(row, col, Cell::Empty);
    if (quad) {
      for (uint16_t row = 0; row < 4; ++row)
        for (uint16_t col = 0; col < 10; ++col)
          if (col != 5)
            game->paint_cell(row, col, Cell::Garbage);
    }
    if (single) {
      auto ghost = game->ghost_piece();
      check(ghost.has_value(), "single-clear setup needs a valid ghost piece");
      bool gaps[10]{};
      for (Offset offset : ghost->piece.current_shape())
        if (ghost->pos.row + offset.row == 0)
          gaps[ghost->pos.col + offset.col] = true;
      for (uint16_t col = 0; col < 10; ++col)
        if (!gaps[col])
          game->paint_cell(0, col, Cell::Garbage);
      game->paint_cell(1, 9, Cell::Garbage); // Keep it from being a perfect clear.
    }
    streak_editor->click();
    check(!game->edit_mode(), "streak setup should resume play");
  };
  int quads = 0;
  for (int locks = 0; quads < 5 && locks < 45; ++locks) {
    Engine *game = streak_window.current_engine();
    bool is_i = game->active_piece()->piece.type() == PieceType::I;
    prepare_board(is_i, false);
    if (is_i)
      QTest::keyClick(&streak_window, Qt::Key_W);
    QTest::keyClick(&streak_window, Qt::Key_Space);
    check(!game->game_over() && game->active_piece(),
          "streak test must continue after each drop");
    if (is_i) {
      ++quads;
      check(game->back_to_back_count() == static_cast<uint32_t>(quads - 1),
            "quad should increase the on-screen B2B count");
    }
  }
  check(quads == 5 && streak_window.current_engine()->surge_charge() == 4 &&
            streak_label->text().contains("B2B<br><b>x4</b>") &&
            streak_label->text().contains("SURGE<br><b>+4</b>"),
        "Season 2 indicators should show B2B x4 and four charged surge lines");
  prepare_board(false, true);
  QTest::keyClick(&streak_window, Qt::Key_Space);
  check(streak_window.current_engine()->last_lock_result()->lines_cleared == 1 &&
            streak_window.current_engine()->last_lock_result()->attack_sent == 4 &&
            streak_window.current_engine()->surge_charge() == 0 &&
            streak_label->text().contains("SURGE<br><b>0 / 4</b>"),
        "a normal single should send the charged surge and clear its indicator");
  prepare_board(false, true);
  QTest::keyClick(&streak_window, Qt::Key_Space);
  const auto combo = streak_window.current_engine()->combo();
  check(combo > 0 &&
            streak_label->text().contains(QString("COMBO<br><b>x%1</b>").arg(combo)),
        "the side combo indicator should update after successive clears");
  streak_window.findChild<QPushButton *>("restart")->click();
  check(streak_window.current_engine()->back_to_back_count() == 0 &&
            streak_window.current_engine()->combo() == 0 &&
            streak_window.current_engine()->surge_charge() == 0 &&
            streak_label->text().contains("SURGE<br><b>0 / 4</b>"),
        "restarting must reset all side indicators");
  streak_window.hide();
  MainWindow tank_window;
  tank_window.show();
  tank_window.activateWindow();
  tank_window.setFocus();
  tank_window.findChild<QComboBox *>("mode")->setCurrentIndex(4);
  app.processEvents();
  Engine *defender = tank_window.current_engine(1);
  defender->receive_garbage(12, 3);
  check(defender->pending_garbage_lines() == 12 && defender->board().max_height() == 0,
        "room garbage should wait in the queue while a piece is falling");
  QTest::keyClick(&tank_window, Qt::Key_Return);
  check(defender->pending_garbage_lines() == 12,
        "room garbage should not rise before its default travel delay");
  for (int tick = 0; tick < 20; ++tick)
    defender->tick();
  QTest::keyClick(&tank_window, Qt::Key_Return);
  check(defender->pending_garbage_lines() == 4 &&
            tank_window.findChild<QLabel *>("status2")->text().contains("Incoming: 4"),
        "a room placement should take only eight ready lines and display the rest");
  std::cout << "Qt solo editor, keyboard, and local room tests passed\n";
  return 0;
}
