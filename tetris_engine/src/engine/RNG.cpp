#include "engine/RNG.hpp"

#include <algorithm>
#include <array>

namespace tetris {
namespace {
constexpr std::array<PieceType, 7> Pieces = {PieceType::I, PieceType::O,
                                             PieceType::T, PieceType::S,
                                             PieceType::Z, PieceType::J,
                                             PieceType::L};
}

void SevenBag::seed(uint64_t value) {
  _rng.seed(value);
  _queue.clear();
}
void SevenBag::fill(size_t amount) const {
  while (_queue.size() < amount) {
    auto bag = Pieces;
    std::shuffle(bag.begin(), bag.end(), _rng);
    _queue.insert(_queue.end(), bag.begin(), bag.end());
  }
}
PieceType SevenBag::peek(uint8_t index) const {
  fill(static_cast<size_t>(index) + 1);
  return _queue[index];
}
std::vector<PieceType> SevenBag::preview(uint8_t amount) const {
  fill(amount);
  return {_queue.begin(), _queue.begin() + amount};
}
PieceType SevenBag::pop() {
  fill(1);
  PieceType piece = _queue.front();
  _queue.pop_front();
  return piece;
}

void FourteenBag::seed(uint64_t value) {
  _rng.seed(value);
  _queue.clear();
}
void FourteenBag::fill(size_t amount) const {
  while (_queue.size() < amount) {
    std::array<PieceType, 14> bag;
    for (size_t i = 0; i < Pieces.size(); ++i)
      bag[i] = bag[i + Pieces.size()] = Pieces[i];
    std::shuffle(bag.begin(), bag.end(), _rng);
    _queue.insert(_queue.end(), bag.begin(), bag.end());
  }
}
PieceType FourteenBag::peek(uint8_t index) const {
  fill(static_cast<size_t>(index) + 1);
  return _queue[index];
}
std::vector<PieceType> FourteenBag::preview(uint8_t amount) const {
  fill(amount);
  return {_queue.begin(), _queue.begin() + amount};
}
PieceType FourteenBag::pop() {
  fill(1);
  PieceType piece = _queue.front();
  _queue.pop_front();
  return piece;
}

void Random::seed(uint64_t value) {
  _rng.seed(value);
  _queue.clear();
}
void Random::fill(size_t amount) const {
  std::uniform_int_distribution<size_t> distribution(0, Pieces.size() - 1);
  while (_queue.size() < amount)
    _queue.push_back(Pieces[distribution(_rng)]);
}
PieceType Random::peek(uint8_t index) const {
  fill(static_cast<size_t>(index) + 1);
  return _queue[index];
}
std::vector<PieceType> Random::preview(uint8_t amount) const {
  fill(amount);
  return {_queue.begin(), _queue.begin() + amount};
}
PieceType Random::pop() {
  fill(1);
  PieceType piece = _queue.front();
  _queue.pop_front();
  return piece;
}
} // namespace tetris
