#include "ModelOpponent.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace tetris::qtapp {
namespace {
QString setting(const char *name, const QString &fallback) {
  const QByteArray value = qgetenv(name);
  return value.isEmpty() ? fallback : QString::fromLocal8Bit(value);
}

QJsonArray position(int row, int col) {
  return {row, col};
}
} // namespace

ModelOpponent::ModelOpponent(std::function<void(int)> on_ready,
                             std::function<void(const Placement &)> on_choice,
                             std::function<void(const QString &)> on_error,
                             QObject *parent)
    : QObject(parent), _on_ready(std::move(on_ready)),
      _on_choice(std::move(on_choice)), _on_error(std::move(on_error)) {
  _timeout.setSingleShot(true);
  connect(&_timeout, &QTimer::timeout, this, [this] {
    fail(_ready ? "Model did not answer in time." : "Model did not load in time.");
  });
  connect(&_process, &QProcess::readyReadStandardOutput, this,
          [this] { read_output(); });
  connect(&_process, &QProcess::readyReadStandardError, this, [this] {
    _errors += _process.readAllStandardError();
    if (_errors.size() > 4096)
      _errors.remove(0, _errors.size() - 4096);
  });
  connect(&_process, &QProcess::errorOccurred, this, [this] {
    fail(_process.errorString());
  });
  connect(&_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this](int, QProcess::ExitStatus) {
            if (!_failed)
              fail(QString("Model process exited. %1").arg(QString::fromUtf8(_errors).trimmed()));
          });
}

ModelOpponent::~ModelOpponent() {
  _timeout.stop();
  if (_process.state() != QProcess::NotRunning) {
    _process.kill();
    _process.waitForFinished(1000);
  }
}

void ModelOpponent::start() {
  const QDir ai_dir(QStringLiteral(TETRIS_AI_DIR));
  const QString script = QFileInfo(setting(
      "TETRIS_MODEL_SCRIPT", ai_dir.filePath("qt_opponent.py"))).absoluteFilePath();
  const QString checkpoint = QFileInfo(setting(
      "TETRIS_MODEL_CHECKPOINT",
      ai_dir.filePath("checkpoints/encoding_v3/shared/latest.pt"))).absoluteFilePath();
  if (!QFileInfo(script).isFile() || !QFileInfo(checkpoint).isFile()) {
    fail(QString("Missing model script or checkpoint: %1 / %2").arg(script, checkpoint));
    return;
  }
  const QString python = setting("TETRIS_MODEL_PYTHON", {});
  _process.setWorkingDirectory(ai_dir.absolutePath());
  if (python.isEmpty())
    _process.start("poetry", {"-C", ai_dir.absolutePath(), "run",
                              "python", script, checkpoint});
  else
    _process.start(python, {script, checkpoint});
  _timeout.start(60000);
}

void ModelOpponent::choose(const Engine &game) {
  if (!ready() || _waiting) {
    fail("Model is not ready for a placement.");
    return;
  }
  _placements = game.generate_active_piece_placement();
  if (game.can_hold()) {
    auto held = game.generate_hold_piece_placement();
    _placements.insert(_placements.end(), held.begin(), held.end());
  }
  if (_placements.empty()) {
    fail("No legal model placement.");
    return;
  }

  const auto &active = game.active_piece();
  if (!active) {
    fail("Model has no active piece.");
    return;
  }
  QJsonArray board;
  for (const auto &row : game.board().render()) {
    QJsonArray cells;
    for (Cell cell : row)
      cells.append(static_cast<int>(cell));
    board.append(cells);
  }
  QJsonArray preview;
  for (PieceType piece : game.preview_queue())
    preview.append(static_cast<int>(piece));
  QJsonArray actions;
  for (const Placement &placement : _placements) {
    QJsonArray cells;
    for (Position cell : placement.cells())
      cells.append(position(cell.row, cell.col));
    actions.append(QJsonObject{
        {"type", static_cast<int>(placement.type)},
        {"final_rotation", static_cast<int>(placement.final_rotation)},
        {"final_pos", position(placement.final_pos.row, placement.final_pos.col)},
        {"drop_distance", static_cast<int>(placement.drop_distance)},
        {"use_hold", placement.use_hold},
        {"cells", cells},
    });
  }
  const auto hold = game.hold_piece();
  const QJsonObject request{
      {"board", board},
      {"active_piece", static_cast<int>(active->piece.type())},
      {"rotation", static_cast<int>(active->piece.rotation())},
      {"position", position(active->pos.row, active->pos.col)},
      {"hold_piece", hold ? QJsonValue(static_cast<int>(*hold)) : QJsonValue(QJsonValue::Null)},
      {"can_hold", game.can_hold()},
      {"preview", preview},
      {"pending_garbage_lines", static_cast<int>(game.pending_garbage_lines())},
      {"actions", actions},
  };
  _waiting = true;
  if (_process.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n') < 0)
    fail(_process.errorString());
  else
    _timeout.start(60000);
}

void ModelOpponent::read_output() {
  _output += _process.readAllStandardOutput();
  if (_output.size() > 1024 * 1024) {
    fail("Model response is too large.");
    return;
  }
  qsizetype newline;
  while (!_failed && (newline = _output.indexOf('\n')) >= 0) {
    const QByteArray line = _output.left(newline);
    _output.remove(0, newline + 1);
    const QJsonDocument document = QJsonDocument::fromJson(line);
    if (!document.isObject()) {
      fail("Invalid model response.");
      return;
    }
    const QJsonObject response = document.object();
    if (response.value("error").isString()) {
      fail(response.value("error").toString());
      return;
    }
    if (!_ready && response.value("ready").toBool()) {
      if (response.value("width").toInt() != 10 ||
          response.value("height").toInt() < 4 ||
          response.value("height").toInt() > 80) {
        fail("Model returned unsupported board dimensions.");
        return;
      }
      _ready = true;
      _timeout.stop();
      _on_ready(response.value("height").toInt());
      continue;
    }
    const QJsonValue value = response.value("index");
    const int index = value.toInt(-1);
    if (!_waiting || !value.isDouble() || index < 0 ||
        static_cast<size_t>(index) >= _placements.size()) {
      fail("Model selected an invalid placement.");
      return;
    }
    _waiting = false;
    _timeout.stop();
    const Placement chosen = _placements[index];
    _placements.clear();
    _on_choice(chosen);
  }
}

void ModelOpponent::fail(const QString &reason) {
  if (_failed)
    return;
  _failed = true;
  _waiting = false;
  _ready = false;
  _timeout.stop();
  _placements.clear();
  if (_process.state() != QProcess::NotRunning)
    _process.kill();
  _on_error(reason);
}

} // namespace tetris::qtapp
