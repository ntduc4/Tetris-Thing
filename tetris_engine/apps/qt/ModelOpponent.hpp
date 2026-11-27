#pragma once

#include "engine/Engine.hpp"

#include <QObject>
#include <QProcess>
#include <QTimer>

#include <functional>
#include <memory>
#include <vector>

namespace tetris::qtapp {

/** Asynchronous inference bridge; only sends public engine state and legal actions. */
class ModelOpponent : public QObject {
public:
  ModelOpponent(std::function<void(int)> on_ready,
                std::function<void(const Placement &)> on_choice,
                std::function<void(const QString &)> on_error,
                QObject *parent = nullptr);
  ~ModelOpponent() override;

  void start();
  void choose(const Engine &game);
  bool ready() const { return _ready && !_failed; }
  bool busy() const { return _waiting; }

private:
  void read_output();
  void fail(const QString &reason);

  QProcess _process;
  QTimer _timeout;
  QByteArray _output, _errors;
  std::vector<Placement> _placements;
  std::unique_ptr<Engine> _snapshot;
  std::function<void(int)> _on_ready;
  std::function<void(const Placement &)> _on_choice;
  std::function<void(const QString &)> _on_error;
  bool _ready = false, _waiting = false, _failed = false;
};

} // namespace tetris::qtapp
