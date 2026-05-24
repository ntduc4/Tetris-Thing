#include "engine/test_support.hpp"

namespace {

void test_randomizer_contract() {
  FakeRandomizer randomizer;
  randomizer.seed(2);

  require(randomizer.peek() == tetris::PieceType::T,
          "randomizer peek should reflect the seeded head piece");
  require(randomizer.peek(2) == tetris::PieceType::Z,
          "randomizer peek should support preview offsets");

  auto preview = randomizer.preview(4);
  require(preview.size() == 4, "randomizer preview should return requested size");
  require(preview[0] == tetris::PieceType::T && preview[3] == tetris::PieceType::J,
          "randomizer preview should preserve queue order");

  require(randomizer.pop() == tetris::PieceType::T,
          "randomizer pop should consume the head piece");
  require(randomizer.peek() == tetris::PieceType::S,
          "randomizer pop should advance the next piece");
}

void test_rotation_system_contract() {
  FakeRotationSystem rotation_system;

  auto kicks = rotation_system.kick_offsets(tetris::PieceType::T,
                                            tetris::Rotation::North,
                                            tetris::Rotation::East);
  require(kicks.size() == 1 && kicks[0].col == 1,
          "rotation system should expose kick data for a transition");
}

void test_spin_system_contract() {
  tetris::Board board(10, 24);
  FakeSpinSystem spin_system;

  tetris::ActivePiece t_piece = tetris::spawn_from_piece_type(tetris::PieceType::T);
  require(spin_system.detect(board, t_piece,
                             tetris::SpinContext{.last_movement = tetris::Movement::None}) ==
              tetris::SpinType::None,
          "spin detection should ignore non-rotation actions");
  require(spin_system.detect(board, t_piece,
                             tetris::SpinContext{.last_movement = tetris::Movement::CW}) ==
              tetris::SpinType::Full,
          "spin detection should identify a full T-spin when appropriate");
}

void test_score_system_contract() {
  FakeScoreSystem score_system;
  tetris::ScoreEvent event{.lines_cleared = 4,
                           .spin = tetris::SpinType::Full,
                           .perfect_clear = false,
                           .back_to_back = true,
                           .combo = 2,
                           .hard_drop_cells = 8,
                           .soft_drop_cells = 3};

  require(score_system.score(event) == 519,
          "score system should combine line, combo, and drop rewards");
}

void test_attack_scheme_contract() {
  FakeAttackScheme attack_scheme;
  tetris::AttackContext context{.lines_cleared = 4,
                                .spin = tetris::SpinType::Full,
                                .perfect_clear = true,
                                .back_to_back = true,
                                .combo = 3};

  require(attack_scheme.garbage_to_send(context) == 18,
          "attack scheme should compute garbage from lock context");
}

} // namespace

int main() {
  test_randomizer_contract();
  test_rotation_system_contract();
  test_spin_system_contract();
  test_score_system_contract();
  test_attack_scheme_contract();
  return 0;
}
