#include "MainWindow.hpp"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <cstdlib>
#include <iostream>

using namespace tetris;
using namespace tetris::qtapp;

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QTemporaryDir files;
  if (!files.isValid())
    return 1;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, files.path());
  const QString checkpoint = files.filePath("latest.pt");
  QFile dummy(checkpoint);
  if (!dummy.open(QIODevice::WriteOnly))
    return 1;
  dummy.close();
  const QString log_path = files.filePath("requests.jsonl");
  qputenv("TETRIS_MODEL_PYTHON", TETRIS_TEST_PYTHON);
  qputenv("TETRIS_MODEL_SCRIPT", TETRIS_TEST_MODEL_STUB);
  qputenv("TETRIS_MODEL_CHECKPOINT", checkpoint.toLocal8Bit());
  qputenv("TETRIS_MODEL_STUB_LOG", log_path.toLocal8Bit());
  qputenv("TETRIS_MODEL_STUB_DELAY", "0.3");

  auto check = [](bool valid, const char *message) {
    if (!valid) {
      std::cerr << "Model match test failed: " << message << '\n';
      std::exit(1);
    }
  };
  MainWindow window;
  window.show();
  window.activateWindow();
  window.setFocus();
  auto *mode = window.findChild<QComboBox *>("mode");
  auto *message = window.findChild<QLabel *>("message");
  auto *rules = window.findChild<QPushButton *>("rules");
  auto *next = window.findChild<QPushButton *>("nextRound");
  mode->setCurrentIndex(5);
  check(QTest::qWaitFor([&] { return message->text().contains("Model ready"); }, 5000),
        "the fake model should become ready");
  check(window.current_engine(0) && window.current_engine(1) &&
            window.current_engine(0)->board().get_height() == 10 &&
            window.current_engine(1)->board().get_width() == 10 &&
            !rules->isEnabled() && next->isVisible() &&
            window.findChild<QWidget *>("board2")->isVisible(),
        "model mode needs two fixed-size visible boards and no editable rules");
  const auto *human = window.current_engine(0);
  const auto *bot = window.current_engine(1);
  check(human->preview_queue() == bot->preview_queue(),
        "both sides must start with the same piece sequence");
  auto initial_bot = bot->active_piece();
  QTest::qWait(100);
  check(bot->active_piece() == initial_bot &&
            human->active_piece()->pos.row == initial_bot->pos.row,
        "turn-based mode must not tick gravity on either board");
  QTest::keyClick(&window, Qt::Key_Return);
  check(bot->active_piece() == initial_bot &&
            bot->board().max_height() == 0,
        "P2 keys must not control the model");

  QTest::keyClick(&window, Qt::Key_Space);
  check(message->text().contains("thinking") &&
            human->board().max_height() > 0 &&
            bot->board().max_height() == 0,
        "the model must wait until the human locks");
  const auto actions = bot->generate_active_piece_placement();
  check(!actions.empty(), "the bot must have legal engine placements");
  Engine expected = bot->clone();
  check(expected.apply_placement(actions.front()).locked,
        "the test's first legal placement should lock");
  if (!expected.game_over())
    expected.spawn_next_piece();

  const auto before_wait = human->active_piece();
  QTest::keyClick(&window, Qt::Key_D);
  QTest::keyClick(&window, Qt::Key_Space);
  check(human->active_piece() == before_wait,
        "human inputs must be paused while the model chooses");
  check(QTest::qWaitFor([&] { return bot->board().max_height() > 0; }, 5000),
        "the model should place a piece after the human");
  check(bot->board().render() == expected.board().render() &&
            bot->preview_queue() == expected.preview_queue(),
        "the model should apply exactly the engine placement it chose");
  QFile log(log_path);
  check(log.open(QIODevice::ReadOnly), "the fake worker must receive a request");
  auto request = QJsonDocument::fromJson(log.readLine()).object();
  check(request.keys().size() == 9 &&
            request.value("board").toArray().size() == 10 &&
            request.value("preview").toArray().size() == 5 &&
            request.value("actions").toArray().size() > 0 &&
            !request.contains("seed") && !request.contains("randomizer"),
        "worker input must be limited to public state and reachable placements");
  QTest::keyClick(&window, Qt::Key_D);
  check(human->active_piece()->pos.col == before_wait->pos.col + 1,
        "human controls should resume after the bot's placement");

  QTest::keyClick(&window, Qt::Key_Space);
  mode->setCurrentIndex(4);
  check(window.current_engine(1)->board().get_height() == 40,
        "switching modes must create a normal local-room match");
  QTest::qWait(450);
  check(window.current_engine(1)->board().max_height() == 0,
        "an in-flight model reply must not act in a later mode");

  qputenv("TETRIS_MODEL_CHECKPOINT", files.filePath("missing.pt").toLocal8Bit());
  mode->setCurrentIndex(5);
  check(message->text().contains("Model unavailable") &&
            message->text().contains("checkpoint"),
        "a missing checkpoint should show a visible error");
  auto frozen = window.current_engine(0)->active_piece();
  QTest::keyClick(&window, Qt::Key_Space);
  check(window.current_engine(0)->active_piece() == frozen,
        "no human moves should proceed without an available opponent");

  qputenv("TETRIS_MODEL_CHECKPOINT", checkpoint.toLocal8Bit());
  qputenv("TETRIS_MODEL_STUB_INDEX", "99999");
  window.findChild<QPushButton *>("restart")->click();
  check(QTest::qWaitFor([&] { return message->text().contains("Model ready"); }, 5000),
        "restarting after a missing checkpoint should reload the opponent");
  QTest::keyClick(&window, Qt::Key_Space);
  check(QTest::qWaitFor([&] {
          return message->text().contains("invalid placement");
        }, 5000) && window.current_engine(1)->board().max_height() == 0,
        "an invalid model reply must not apply an arbitrary move");
  qunsetenv("TETRIS_MODEL_STUB_INDEX");

  window.findChild<QPushButton *>("restart")->click();
  check(QTest::qWaitFor([&] { return message->text().contains("Model ready"); }, 5000),
        "restarting should load a fresh model process");
  window.current_engine(1)->receive_garbage(8, 3);
  QTest::keyClick(&window, Qt::Key_Space);
  check(QTest::qWaitFor([&] { return next->isEnabled(); }, 5000) &&
            message->text().contains("You win"),
        "a model top-out should award the human a round win");
  next->click();
  check(QTest::qWaitFor([&] { return message->text().contains("Model ready"); }, 5000) &&
            window.current_engine(0)->board().max_height() == 0 &&
            window.current_engine(1)->board().max_height() == 0,
        "Next round should reset both boards and reload the latest model");

  qputenv("TETRIS_MODEL_STUB_HEIGHT", "40");
  window.findChild<QPushButton *>("restart")->click();
  check(QTest::qWaitFor([&] {
          return message->text().contains("Model ready") &&
                 window.current_engine(0)->board().get_height() == 40;
        }, 5000) && window.current_engine(1)->board().get_height() == 40,
        "the match boards must follow the loaded checkpoint's feature dimensions");
  QTest::keyClick(&window, Qt::Key_Space);
  check(QTest::qWaitFor([&] {
          return window.current_engine(1)->board().max_height() > 0;
        }, 5000), "the model should also play with a 40-row checkpoint");
  qunsetenv("TETRIS_MODEL_STUB_HEIGHT");

  if (qEnvironmentVariableIsSet("TETRIS_TEST_REAL_MODEL")) {
    qunsetenv("TETRIS_MODEL_PYTHON");
    qunsetenv("TETRIS_MODEL_SCRIPT");
    qunsetenv("TETRIS_MODEL_CHECKPOINT");
    mode->setCurrentIndex(4);
    mode->setCurrentIndex(5);
    check(QTest::qWaitFor([&] { return message->text().contains("Model ready"); }, 30000),
          "the actual saved model should load in the Qt app");
    check(window.current_engine(0)->board().get_height() == 10,
          "the current checkpoint should restore its original 10-row board");
    QTest::keyClick(&window, Qt::Key_Space);
    check(QTest::qWaitFor([&] {
            return window.current_engine(1)->board().max_height() > 0;
          }, 30000), "the actual saved model should place a piece");
  }

  std::cout << "Qt turn-based model match tests passed\n";
  return 0;
}
