#pragma once

#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include "engine/Movement.hpp"

#include <memory>
#include <optional>
#include <stdexcept>

namespace tetris {

/** Interface for classifying spin moves at lock time. */
class SpinSystem {
public:
  /** Destroys a spin system through the base interface. */
  virtual ~SpinSystem() = default;

  /** Detects the spin type for the current piece state. */
  virtual SpinType detect(const Board &board, const ActivePiece &piece,
                           const SpinContext &context) const = 0;
  virtual std::unique_ptr<SpinSystem> clone() const {
    throw std::logic_error("spin system does not support cloning");
  }
};

/** Spin detector that scores all pieces. */
class AllSpin : public SpinSystem {
public:
  explicit AllSpin(bool t_immobile_mini = true)
      : _t_immobile_mini(t_immobile_mini) {}
  SpinType detect(const Board &board, const ActivePiece &piece,
                   const SpinContext &context) const override;
  std::unique_ptr<SpinSystem> clone() const override {
    return std::make_unique<AllSpin>(*this);
  }

private:
  bool _t_immobile_mini;
};
/** Spin detector that only scores T-spins. */
class TSpin : public SpinSystem {
public:
  SpinType detect(const Board &board, const ActivePiece &piece,
                   const SpinContext &context) const override;
  std::unique_ptr<SpinSystem> clone() const override {
    return std::make_unique<TSpin>(*this);
  }
};

} // namespace tetris
