#include "engine/test_support.hpp"

#include <array>
#include <limits>

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

void test_tetrio_season_two_attack() {
  using namespace tetris;
  TetrioSS2 season_two;
  // Three consecutive Singles are a combo x2: the first nonzero attack.
  constexpr std::array<uint16_t, 9> singles{0, 0, 1, 1, 1, 1, 2, 2, 2};
  for (uint32_t combo = 0; combo < singles.size(); ++combo)
    require(season_two.garbage_to_send(AttackContext{
                .lines_cleared = 1, .combo = combo}) == singles[combo],
            "Season 2 zero-base clears must use logarithmic combo attack");
  require(season_two.garbage_to_send(AttackContext{
              .lines_cleared = 1, .combo = 16}) == 3 &&
              season_two.garbage_to_send(AttackContext{
                  .lines_cleared = 2, .combo = 4}) == 2 &&
              season_two.garbage_to_send(AttackContext{
                  .lines_cleared = 4, .back_to_back = true, .combo = 4}) == 9,
          "Season 2 must round down and add B2B after scaling base attack");
  require(season_two.garbage_to_send(AttackContext{
               .lines_cleared = 1, .spin = SpinType::Mini,
               .combo = 2, .piece = PieceType::O}) == 1 &&
              season_two.garbage_to_send(AttackContext{
                  .lines_cleared = 1, .spin = SpinType::Mini,
                  .back_to_back = true, .piece = PieceType::O}) == 1 &&
              season_two.garbage_to_send(AttackContext{
                  .lines_cleared = 0, .back_to_back = true, .combo = 16}) == 0,
          "non-T minis can receive combo/B2B bonuses but no-line spins send none");
  require(season_two.garbage_to_send(AttackContext{
              .lines_cleared = 4, .combo = 4, .cleared_garbage = true}) == 9 &&
              season_two.garbage_to_send(AttackContext{
                  .lines_cleared = 2, .spin = SpinType::Full,
                  .cleared_garbage = true}) == 5 &&
              season_two.garbage_to_send(AttackContext{
                  .lines_cleared = 1, .spin = SpinType::Mini,
                  .cleared_garbage = true}) == 1 &&
              season_two.garbage_to_send(AttackContext{
                  .lines_cleared = 1, .cleared_garbage = true}) == 0,
          "garbage-clearing quads and spins gain a flat line, but singles do not");
  const AttackContext all_clear{.lines_cleared = 4, .perfect_clear = true,
                                 .combo = 4};
  require(season_two.garbage_to_send(all_clear) == 13 &&
              season_two.garbage_packets(all_clear) ==
                  std::vector<uint16_t>({8, 5}) &&
              season_two.garbage_packets(AttackContext{
                  .lines_cleared = 1, .surge_on_break = 5}) ==
                  std::vector<uint16_t>({2, 2, 1}),
          "Season 2 All Clear and Surge must be separate unmultiplied packets");
  const AttackContext huge{.lines_cleared = 4, .perfect_clear = true,
                            .combo = std::numeric_limits<uint32_t>::max(),
                            .surge_on_break = 10};
  require(season_two.garbage_to_send(huge) == UINT16_MAX &&
              season_two.garbage_packets(huge) ==
                  std::vector<uint16_t>({UINT16_MAX}),
          "very large combo attacks must saturate without wrapping packets");
}

void test_guideline_attack() {
  using namespace tetris;
  GuidelineAttack guideline;
  constexpr std::array<uint16_t, 13> singles{
      0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5, 5};
  for (uint32_t combo = 0; combo < singles.size(); ++combo)
    require(guideline.garbage_to_send(AttackContext{
                .lines_cleared = 1, .combo = combo}) == singles[combo],
            "Guideline singles use the standard additive combo table");
  require(guideline.garbage_to_send(AttackContext{
              .lines_cleared = 1, .combo = 100}) == 5 &&
              guideline.garbage_to_send(AttackContext{
                  .lines_cleared = 2, .combo = 4}) == 3 &&
              guideline.garbage_to_send(AttackContext{
                  .lines_cleared = 4, .back_to_back = true,
                  .combo = 4}) == 7 &&
              guideline.garbage_to_send(AttackContext{
                  .lines_cleared = 4, .perfect_clear = true,
                  .combo = 4}) == 16 &&
              guideline.garbage_to_send(AttackContext{
                  .lines_cleared = 0, .combo = 100}) == 0,
          "Guideline base, B2B, combo and perfect-clear bonuses must add");
  require(TableAttack{}.garbage_to_send(AttackContext{
              .lines_cleared = 1, .combo = 2}) == 0,
          "custom attack tables retain their configurable multiplier semantics");
}

} // namespace

int main() {
  test_randomizer_contract();
  test_rotation_system_contract();
  test_spin_system_contract();
  test_score_system_contract();
  test_attack_scheme_contract();
  test_tetrio_season_two_attack();
  test_guideline_attack();
  return 0;
}
