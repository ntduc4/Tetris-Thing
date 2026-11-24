#include "MainWindow.hpp"

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <cstdlib>
#include <functional>
#include <iostream>

using namespace tetris;
using namespace tetris::qtapp;

namespace {
void check(bool valid, const char *message) {
  if (!valid) {
    std::cerr << "Qt controls test failed: " << message << '\n';
    std::exit(1);
  }
}

bool wait_for(const std::function<bool()> &condition, int timeout_ms = 750) {
  QElapsedTimer elapsed;
  elapsed.start();
  while (!condition() && elapsed.elapsed() < timeout_ms)
    QTest::qWait(5);
  return condition();
}

void focus_window(MainWindow &window) {
  window.show();
  window.activateWindow();
  window.setFocus();
  qApp->processEvents();
}
} // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QTemporaryDir settings_dir;
  check(settings_dir.isValid(), "temporary settings directory must exist");
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
  {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "TetrisEngine", "Tetris");
    settings.setValue("solo/das", 2);
    settings.setValue("solo/arr", 60);
    settings.setValue("room/das", 2);
    settings.setValue("room/arr", 60);
  }

  MainWindow window;
  focus_window(window);
  QTimer *timer = window.findChild<QTimer *>();
  check(timer != nullptr, "simulation timer must exist");
  Engine *solo = window.current_engine();
  const int spawn_col = solo->active_piece()->pos.col;

  auto charge_right = [&](int key, Engine *game) {
    const int start = game->active_piece()->pos.col;
    timer->start();
    QTest::keyPress(&window, static_cast<Qt::Key>(key));
    check(game->active_piece()->pos.col == start + 1,
          "a fresh horizontal press moves once immediately");
    check(wait_for([&] {
      return game->active_piece() && game->active_piece()->pos.col >= start + 2;
    }), "held movement must charge DAS and begin repeating");
    timer->stop();
  };

  charge_right(Qt::Key_D, solo);
  QTest::keyPress(&window, Qt::Key_Space);
  check(solo->active_piece() && solo->active_piece()->pos.col == spawn_col + 1,
        "charged direction moves the next piece on hard drop, before ARR");
  const auto board_after_drop = solo->board().render();
  const auto queue_after_drop = solo->preview_queue();
  QTest::keyPress(&window, Qt::Key_Space);
  check(solo->board().render() == board_after_drop &&
            solo->preview_queue() == queue_after_drop &&
            solo->active_piece()->pos.col == spawn_col + 1,
        "holding hard drop never auto-repeats");
  timer->start();
  QTest::qWait(80);
  timer->stop();
  check(solo->active_piece()->pos.col == spawn_col + 1,
        "entry movement must not restart or skip the held ARR interval");
  QTest::keyRelease(&window, Qt::Key_Space);
  QTest::keyRelease(&window, Qt::Key_D);
  QTest::keyPress(&window, Qt::Key_Space);
  check(solo->active_piece() && solo->active_piece()->pos.col == spawn_col,
        "releasing movement clears the stored DAS before the next piece");
  QTest::keyRelease(&window, Qt::Key_Space);

  window.findChild<QPushButton *>("restart")->click();
  solo = window.current_engine();
  charge_right(Qt::Key_D, solo);
  const int charged_col = solo->active_piece()->pos.col;
  QTest::keyPress(&window, Qt::Key_A);
  check(solo->active_piece()->pos.col == charged_col - 1,
        "opposite direction begins with one immediate move");
  QTest::keyRelease(&window, Qt::Key_A);
  check(solo->active_piece()->pos.col == charged_col,
        "releasing the opposite direction immediately resumes the held key");
  QTest::keyPress(&window, Qt::Key_Space);
  check(solo->active_piece() && solo->active_piece()->pos.col == spawn_col,
        "switching back must restart DAS instead of reusing the prior charge");
  QTest::keyRelease(&window, Qt::Key_Space);
  QTest::keyRelease(&window, Qt::Key_D);

  window.findChild<QPushButton *>("restart")->click();
  solo = window.current_engine();
  charge_right(Qt::Key_D, solo);
  QTest::keyPress(&window, Qt::Key_A);
  const int left_col = solo->active_piece()->pos.col;
  QTest::keyRelease(&window, Qt::Key_D);
  check(solo->active_piece()->pos.col == left_col,
        "releasing the older held direction restarts DAS without another move");
  QTest::keyPress(&window, Qt::Key_Space);
  check(solo->active_piece() && solo->active_piece()->pos.col == spawn_col,
        "either released direction must cancel the prior DAS charge");
  QTest::keyRelease(&window, Qt::Key_Space);
  QTest::keyRelease(&window, Qt::Key_A);

  window.findChild<QComboBox *>("mode")->setCurrentIndex(4);
  focus_window(window);
  Engine *p1 = window.current_engine(0);
  Engine *p2 = window.current_engine(1);
  charge_right(Qt::Key_D, p1);
  // The timer is stopped while charging each player's key; start it again
  // for the second player's independent DAS.
  charge_right(Qt::Key_Right, p2);
  const int p2_before_drop = p2->active_piece()->pos.col;
  QTest::keyPress(&window, Qt::Key_Space);
  check(p1->active_piece() && p1->active_piece()->pos.col == spawn_col + 1 &&
            p2->active_piece()->pos.col == p2_before_drop,
        "player 1's entry shift does not move player 2");
  QTest::keyPress(&window, Qt::Key_Return);
  check(p2->active_piece() && p2->active_piece()->pos.col == spawn_col + 1,
        "player 2 also keeps its own DAS charge across a hard drop");
  QTest::keyRelease(&window, Qt::Key_Space);
  QTest::keyRelease(&window, Qt::Key_Return);
  QTest::keyRelease(&window, Qt::Key_D);
  QTest::keyRelease(&window, Qt::Key_Right);
  window.hide();

  {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "TetrisEngine", "Tetris");
    settings.setValue("solo/arr", 0);
  }
  MainWindow instant;
  focus_window(instant);
  Engine *game = instant.current_engine();
  QTimer *instant_timer = instant.findChild<QTimer *>();
  const int initial = game->active_piece()->pos.col;
  QTest::keyPress(&instant, Qt::Key_D);
  check(wait_for([&] {
    return game->active_piece()->pos.col > initial + 1;
  }), "zero ARR charges and moves the old piece to the wall");
  instant_timer->stop();
  QTest::keyPress(&instant, Qt::Key_Space);
  check(game->active_piece() && !game->try_move_right(),
        "charged zero ARR must place the new piece against the wall immediately");
  QTest::keyPress(&instant, Qt::Key_C);
  check(game->hold_piece() && game->active_piece() && !game->try_move_right(),
        "a charged direction carries through a successful hold swap");

  game->set_lock_delay(0);
  check(game->try_soft_drop(-1) && game->board().grounded(*game->active_piece()),
        "the held piece can land before a natural lock");
  const auto board_before_lock = game->board().render();
  instant_timer->start();
  check(wait_for([&] {
    return game->board().render() != board_before_lock;
  }), "the natural lock must be processed by the Qt simulation timer");
  instant_timer->stop();
  check(game->active_piece() && !game->try_move_right(),
        "natural lock carries charged zero ARR to the spawned piece");
  QTest::keyRelease(&instant, Qt::Key_C);
  QTest::keyRelease(&instant, Qt::Key_Space);
  QTest::keyRelease(&instant, Qt::Key_D);

  instant.findChild<QPushButton *>("restart")->click();
  game = instant.current_engine();
  const int center = game->active_piece()->pos.col;
  instant_timer->start();
  QTest::keyPress(&instant, Qt::Key_D);
  check(wait_for([&] {
    return game->active_piece() && game->active_piece()->pos.col > center + 1;
  }), "zero ARR moves right to the wall after DAS");
  instant_timer->stop();
  const int right_wall = game->active_piece()->pos.col;
  QTest::keyPress(&instant, Qt::Key_A);
  check(game->active_piece()->pos.col == right_wall - 1,
        "switching direction with zero ARR initially moves one cell");
  QTest::keyRelease(&instant, Qt::Key_D);
  check(game->active_piece()->pos.col == right_wall - 1,
        "releasing the inactive key must not move a second cell with zero ARR");
  QTest::keyPress(&instant, Qt::Key_Space);
  check(game->active_piece() && game->active_piece()->pos.col == center,
        "the released direction must restart DAS before the next piece");
  QTest::keyRelease(&instant, Qt::Key_Space);
  instant_timer->start();
  check(wait_for([&] {
    return game->active_piece() && game->active_piece()->pos.col < center - 1;
  }), "holding the new direction past DAS should move to the other wall");
  instant_timer->stop();
  check(!game->try_move_left(), "charged zero ARR should reach the left wall");
  QTest::keyRelease(&instant, Qt::Key_A);
  instant.hide();

  {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "TetrisEngine", "Tetris");
    settings.setValue("solo/das", 0);
  }
  MainWindow zero_das;
  focus_window(zero_das);
  Engine *zero_game = zero_das.current_engine();
  zero_das.findChild<QTimer *>()->stop();
  QTest::keyPress(&zero_das, Qt::Key_D);
  check(!zero_game->try_move_right(),
        "zero DAS and zero ARR should move to the wall on key press");
  QTest::keyPress(&zero_das, Qt::Key_Space);
  check(zero_game->active_piece() && !zero_game->try_move_right(),
        "zero DAS should stay charged on an immediate hard drop");
  zero_das.findChild<QComboBox *>("palette")->setFocus();
  app.processEvents();
  zero_das.setFocus();
  app.processEvents();
  QTest::keyRelease(&zero_das, Qt::Key_Space);
  QTest::keyPress(&zero_das, Qt::Key_Space);
  check(zero_game->active_piece() && zero_game->active_piece()->pos.col == initial,
        "moving focus into a control must clear held movement");
  QTest::keyRelease(&zero_das, Qt::Key_D);
  QTest::keyRelease(&zero_das, Qt::Key_Space);
  std::cout << "Qt held movement, entry shifts, direction changes and ARR tests passed\n";
  return 0;
}
