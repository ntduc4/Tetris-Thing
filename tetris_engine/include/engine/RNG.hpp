#pragma once

#include "core/Piece.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

namespace tetris {

/** Interface for deterministic or random piece generation. */
class Randomizer {
public:
  /** Destroys a randomizer through the base interface. */
  virtual ~Randomizer() = default;

  /** Resets the randomizer state with a deterministic seed. */
  virtual void seed(uint64_t seed) = 0;
  /** Returns the piece at the requested preview index without consuming it. */
  virtual PieceType peek(uint8_t index = 0) const = 0;
  /** Returns the next requested number of preview pieces. */
  virtual std::vector<PieceType> preview(uint8_t amount) const = 0;
  /** Consumes and returns the next piece in sequence. */
  virtual PieceType pop() = 0;
  /** Deep copy, including RNG and queued pieces. Custom randomizers must override. */
  virtual std::unique_ptr<Randomizer> clone() const {
    throw std::logic_error("randomizer does not support cloning");
  }
  /** Copy RNG/queue into an existing randomizer of the same type. */
  virtual void restore_from(const Randomizer &) {
    throw std::logic_error("randomizer does not support restoring");
  }
};

/** Classic 7-bag generator. */
class SevenBag : public Randomizer {
public:
  explicit SevenBag(uint64_t seed_value = 0) { seed(seed_value); }
  void seed(uint64_t seed_value) override;
  PieceType peek(uint8_t index = 0) const override;
  std::vector<PieceType> preview(uint8_t amount) const override;
  PieceType pop() override;
  std::unique_ptr<Randomizer> clone() const override {
    return std::make_unique<SevenBag>(*this);
  }
  void restore_from(const Randomizer &other) override {
    auto source = dynamic_cast<const SevenBag *>(&other);
    if (!source) throw std::invalid_argument("incompatible randomizer snapshot");
    *this = *source;
  }

private:
  void fill(size_t amount) const;
  mutable std::mt19937_64 _rng;
  mutable std::deque<PieceType> _queue;
};
/** Extended 14-bag generator. */
class FourteenBag : public Randomizer {
public:
  explicit FourteenBag(uint64_t seed_value = 0) { seed(seed_value); }
  void seed(uint64_t seed_value) override;
  PieceType peek(uint8_t index = 0) const override;
  std::vector<PieceType> preview(uint8_t amount) const override;
  PieceType pop() override;
  std::unique_ptr<Randomizer> clone() const override {
    return std::make_unique<FourteenBag>(*this);
  }
  void restore_from(const Randomizer &other) override {
    auto source = dynamic_cast<const FourteenBag *>(&other);
    if (!source) throw std::invalid_argument("incompatible randomizer snapshot");
    *this = *source;
  }

private:
  void fill(size_t amount) const;
  mutable std::mt19937_64 _rng;
  mutable std::deque<PieceType> _queue;
};
/** Fully random piece generator. */
class Random : public Randomizer {
public:
  explicit Random(uint64_t seed_value = 0) { seed(seed_value); }
  void seed(uint64_t seed_value) override;
  PieceType peek(uint8_t index = 0) const override;
  std::vector<PieceType> preview(uint8_t amount) const override;
  PieceType pop() override;
  std::unique_ptr<Randomizer> clone() const override {
    return std::make_unique<Random>(*this);
  }
  void restore_from(const Randomizer &other) override {
    auto source = dynamic_cast<const Random *>(&other);
    if (!source) throw std::invalid_argument("incompatible randomizer snapshot");
    *this = *source;
  }

private:
  void fill(size_t amount) const;
  mutable std::mt19937_64 _rng;
  mutable std::deque<PieceType> _queue;
};

} // namespace tetris
