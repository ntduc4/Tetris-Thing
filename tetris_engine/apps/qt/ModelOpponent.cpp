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
       ai_dir.filePath("checkpoints/encoding_v5/shared/latest.pt"))).absoluteFilePath();
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
  _snapshot = std::make_unique<Engine>(game.clone());

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
        {"spin", static_cast<int>(placement.spin)},
        {"cells", cells},
    });
  }
  const auto hold = game.hold_piece();
  const QJsonObject request{
      {"phase", "roots"},
      {"board", board},
      {"active_piece", static_cast<int>(active->piece.type())},
      {"rotation", static_cast<int>(active->piece.rotation())},
      {"position", position(active->pos.row, active->pos.col)},
      {"hold_piece", hold ? QJsonValue(static_cast<int>(*hold)) : QJsonValue(QJsonValue::Null)},
      {"can_hold", game.can_hold()},
      {"preview", preview},
      {"pending_garbage_lines", static_cast<int>(game.pending_garbage_lines())},
      {"combo", static_cast<int>(game.combo())},
      {"back_to_back", game.back_to_back()},
      {"back_to_back_count", static_cast<int>(game.back_to_back_count())},
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
    if (_waiting && response.value("shortlist").isArray() && _snapshot) {
      const QJsonArray scores = response.value("scores").toArray();
      if (scores.size() != static_cast<qsizetype>(_placements.size())) {
        fail("Invalid model search scores.");
        return;
      }
      const QJsonArray root_preview = [&] {
        QJsonArray result;
        for (PieceType piece : _snapshot->preview_queue())
          result.append(static_cast<int>(piece));
        return result;
      }();
      QJsonArray branches;
      for (const QJsonValue candidate : response.value("shortlist").toArray()) {
        const int index = candidate.toInt(-1);
        if (index < 0 || static_cast<size_t>(index) >= _placements.size()) {
          fail("Invalid model search candidate.");
          return;
        }
        Engine work = _snapshot->clone();
        const int before_height = work.board().max_height();
        const int before_b2b = work.back_to_back_count();
        const bool empty_hold = !work.hold_piece();
        const auto &placement = _placements[index];
        const auto lock = work.apply_placement(placement);
        if (!lock.locked) {
          fail("Could not simulate model placement.");
          return;
        }
        const int consumed = 1 + static_cast<int>(placement.use_hold && empty_hold);
        if (!work.game_over() && root_preview.size() < consumed)
          continue; // Keep the root Q score; the next piece is not public.
        if (!work.game_over())
          work.spawn_next_piece();
        const bool terminal = work.game_over();
        const double reward = 2.0 * lock.attack_sent +
            (static_cast<int>(work.back_to_back_count()) - before_b2b) +
            lock.lines_cleared -
            0.1 * (work.board().max_height() - before_height) +
            0.05 - (terminal ? 50.0 : 0.0);
        QJsonObject branch{{"root", index}, {"reward", reward}};
        // Never inspect newly generated preview pieces. Empty-hold branches
        // consume an extra known piece before the next active piece spawns.
        if (!terminal && root_preview.size() >= consumed && work.active_piece()) {
          QJsonArray child_preview;
          for (int i = consumed; i < root_preview.size(); ++i)
            child_preview.append(root_preview[i]);
          QJsonArray child_actions;
          auto placements = work.generate_active_piece_placement();
          if (work.can_hold() && (work.hold_piece() || !child_preview.isEmpty())) {
            auto held = work.generate_hold_piece_placement();
            placements.insert(placements.end(), held.begin(), held.end());
          }
          for (const Placement &action : placements) {
            QJsonArray cells;
            for (Position cell : action.cells())
              cells.append(position(cell.row, cell.col));
            child_actions.append(QJsonObject{
                {"type", static_cast<int>(action.type)},
                {"final_rotation", static_cast<int>(action.final_rotation)},
                {"final_pos", position(action.final_pos.row, action.final_pos.col)},
                {"drop_distance", static_cast<int>(action.drop_distance)},
                {"use_hold", action.use_hold},
                {"spin", static_cast<int>(action.spin)}, {"cells", cells},
            });
          }
          if (!child_actions.isEmpty()) {
            QJsonArray child_board;
            for (const auto &row : work.board().render()) {
              QJsonArray cells;
              for (Cell cell : row)
                cells.append(static_cast<int>(cell));
              child_board.append(cells);
            }
            const auto &active = *work.active_piece();
            const auto hold = work.hold_piece();
            branch.insert("state", QJsonObject{
                {"board", child_board},
                {"active_piece", static_cast<int>(active.piece.type())},
                {"rotation", static_cast<int>(active.piece.rotation())},
                {"position", position(active.pos.row, active.pos.col)},
                {"hold_piece", hold ? QJsonValue(static_cast<int>(*hold))
                                    : QJsonValue(QJsonValue::Null)},
                {"can_hold", work.can_hold()}, {"preview", child_preview},
                {"pending_garbage_lines", static_cast<int>(work.pending_garbage_lines())},
                {"combo", static_cast<int>(work.combo())},
                {"back_to_back", work.back_to_back()},
                {"back_to_back_count", static_cast<int>(work.back_to_back_count())},
                {"actions", child_actions},
            });
          }
        }
        branches.append(branch);
      }
      const QJsonObject request{{"phase", "leaves"}, {"scores", scores},
                                {"branches", branches}};
      if (_process.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n') < 0)
        fail(_process.errorString());
      else
        _timeout.start(60000);
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
    _snapshot.reset();
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
  _snapshot.reset();
  if (_process.state() != QProcess::NotRunning)
    _process.kill();
  _on_error(reason);
}

} // namespace tetris::qtapp
