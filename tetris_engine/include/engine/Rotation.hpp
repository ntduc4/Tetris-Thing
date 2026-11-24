#pragma once

#include "core/Core.hpp"
#include "core/Piece.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

namespace tetris {

/** Interface for piece wall-kick data. */
class RotationSystem {
public:
  /** Destroys a rotation system through the base interface. */
  virtual ~RotationSystem() = default;

  /** Returns the ordered kick offsets to test for a rotation transition. */
  virtual std::vector<Offset> kick_offsets(PieceType piece, Rotation from,
                                           Rotation to) const = 0;
  /** A ruleset can disable all rotation, including in-place rotation. */
  virtual bool allows_rotation() const { return true; }
  /** The 2009 Guideline only provides 90-degree rotations. */
  virtual bool allows_half_turn() const { return true; }
  virtual std::unique_ptr<RotationSystem> clone() const {
    throw std::logic_error("rotation system does not support cloning");
  }
};

/** TETR.IO SRS+: symmetric I-piece 90-degree kicks and conservative 180s. */
class SRS : public RotationSystem {
public:
  std::vector<Offset> kick_offsets(PieceType piece, Rotation from,
                                    Rotation to) const override;
  std::unique_ptr<RotationSystem> clone() const override {
    return std::make_unique<SRS>(*this);
  }
};
/** Explicit name for the existing SRS+ implementation (SRS stays for API compatibility). */
using SRSPlus = SRS;

/** TETR.IO SRS-X: guideline 90-degree kicks and Heboris-style 180s. */
class SRSX : public RotationSystem {
public:
  std::vector<Offset> kick_offsets(PieceType piece, Rotation from,
                                    Rotation to) const override;
  std::unique_ptr<RotationSystem> clone() const override {
    return std::make_unique<SRSX>(*this);
  }
};

/** 2009 Guideline SRS: ordered five-point 90-degree kicks, no 180 input. */
class GuidelineSRS : public RotationSystem {
public:
  std::vector<Offset> kick_offsets(PieceType piece, Rotation from,
                                   Rotation to) const override;
  bool allows_half_turn() const override { return false; }
  std::unique_ptr<RotationSystem> clone() const override {
    return std::make_unique<GuidelineSRS>(*this);
  }
};

/** Rotation system that rejects all rotations. */
class None : public RotationSystem {
public:
  bool allows_rotation() const override { return false; }
  std::vector<Offset> kick_offsets(PieceType, Rotation,
                                    Rotation) const override {
    return {};
  }
  std::unique_ptr<RotationSystem> clone() const override {
    return std::make_unique<None>(*this);
  }
};

} // namespace tetris
