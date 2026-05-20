#pragma once

#include "core/Board.hpp"
#include "core/Piece.hpp"
#include "core/Rotation.hpp"
#include "engine/Attack.hpp"
#include "engine/RNG.hpp"
#include "engine/Score.hpp"
#include "engine/Spin.hpp"

#include "core/test_support.hpp"

#include <memory>
#include <vector>

class FakeRotationSystem : public tetris::RotationSystem {
public:
  std::optional<tetris::Offset>
  kick_offset(tetris::PieceType, tetris::Rotation from, tetris::Rotation to,
              uint8_t check_num) const override {
    if (from == tetris::Rotation::North && to == tetris::Rotation::East &&
        check_num == 0)
      return tetris::Offset{.row = 0, .col = 1};
    return std::nullopt;
  }

  std::optional<tetris::ActivePiece>
  try_rotate_cw(const tetris::Board &, const tetris::ActivePiece &piece) const override {
    tetris::ActivePiece rotated{.piece = piece.piece, .pos = piece.pos};
    rotated.piece.rotate_cw();
    return rotated;
  }

  std::optional<tetris::ActivePiece>
  try_rotate_ccw(const tetris::Board &, const tetris::ActivePiece &piece) const override {
    tetris::ActivePiece rotated{.piece = piece.piece, .pos = piece.pos};
    rotated.piece.rotate_ccw();
    return rotated;
  }

  std::optional<tetris::ActivePiece>
  try_rotate_180(const tetris::Board &, const tetris::ActivePiece &piece) const override {
    tetris::ActivePiece rotated{.piece = piece.piece, .pos = piece.pos};
    rotated.piece.rotate_180();
    return rotated;
  }
};

class FakeRandomizer : public tetris::Randomizer {
public:
  void seed(uint64_t seed) override {
    seed_value = seed;
    index = static_cast<size_t>(seed % queue.size());
  }

  tetris::PieceType peek(uint8_t offset = 0) const override {
    return queue[(index + offset) % queue.size()];
  }

  std::vector<tetris::PieceType> preview(uint8_t amount) const override {
    std::vector<tetris::PieceType> pieces;
    for (uint8_t i = 0; i < amount; ++i)
      pieces.push_back(peek(i));
    return pieces;
  }

  tetris::PieceType pop() override {
    tetris::PieceType next = peek();
    index = (index + 1) % queue.size();
    return next;
  }

  uint64_t seed_value = 0;
  size_t index = 0;
  std::vector<tetris::PieceType> queue = {tetris::PieceType::I,
                                          tetris::PieceType::O,
                                          tetris::PieceType::T,
                                          tetris::PieceType::S,
                                          tetris::PieceType::Z,
                                          tetris::PieceType::J,
                                          tetris::PieceType::L};
};

class FakeSpinSystem : public tetris::SpinSystem {
public:
  tetris::SpinType detect(const tetris::Board &, const tetris::ActivePiece &piece,
                          bool last_action_was_rotation) const override {
    if (!last_action_was_rotation)
      return tetris::SpinType::None;
    return piece.piece.type() == tetris::PieceType::T ? tetris::SpinType::Full
                                                      : tetris::SpinType::Mini;
  }
};

class FakeScoreSystem : public tetris::ScoreSystem {
public:
  uint64_t score(const tetris::ScoreEvent &event) const override {
    return event.lines_cleared * 100 + event.combo * 50 +
           event.hard_drop_cells * 2 + event.soft_drop_cells;
  }
};

class FakeAttackScheme : public tetris::AttackScheme {
public:
  uint16_t garbage_to_send(const tetris::AttackContext &ctx) const override {
    return static_cast<uint16_t>(ctx.lines_cleared + ctx.combo +
                                 (ctx.back_to_back ? 1 : 0) +
                                 (ctx.perfect_clear ? 10 : 0));
  }
};

inline std::unique_ptr<tetris::RotationSystem> make_rotation_system() {
  return std::make_unique<FakeRotationSystem>();
}

inline std::unique_ptr<tetris::Randomizer> make_randomizer() {
  return std::make_unique<FakeRandomizer>();
}

inline std::unique_ptr<tetris::SpinSystem> make_spin_system() {
  return std::make_unique<FakeSpinSystem>();
}

inline std::unique_ptr<tetris::ScoreSystem> make_score_system() {
  return std::make_unique<FakeScoreSystem>();
}

inline std::unique_ptr<tetris::AttackScheme> make_attack_scheme() {
  return std::make_unique<FakeAttackScheme>();
}
