#include "core/Core.hpp"
#include "engine/Engine.hpp"
#include "engine/Match.hpp"
#include "engine/SoloSession.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace py = pybind11;
using namespace tetris;

namespace {

enum class RotationRule { SRSPlus, SRSX, Off };
enum class BagRule { Seven, Fourteen, Random };
enum class SpinRule { AllMiniPlus, AllMini, TOnly, Off };
enum class AttackRule { Season2, Season1, Jstris, Custom, Off };

/** Python-only rule selection; the headless engine and CLI do not depend on Python. */
struct EngineConfig {
  uint16_t width = 10;
  uint16_t height = 40;
  uint8_t preview_count = DefaultQueueLength;
  double gravity = DefaultGravity;
  double gravity_increase = DefaultGravityIncrease;
  uint64_t gravity_margin = DefaultGravityMarginTime;
  uint64_t lock_delay = DefaultLockDelay;
  uint16_t lock_resets = 15;
  uint16_t garbage_delay = 20;
  RotationRule rotation = RotationRule::SRSPlus;
  BagRule bag = BagRule::Seven;
  SpinRule spin = SpinRule::AllMiniPlus;
  AttackRule attack = AttackRule::Season2;
  AttackTable attacks;
  ScoreTable scores;
};

Engine make_engine(const EngineConfig &config) {
  if (config.width < PieceDimension || config.width > 20 ||
      config.height < PieceDimension || config.height > 80)
    throw std::invalid_argument("board width must be 4..20 and height 4..80");
  if (config.preview_count == 0 || config.preview_count > 20)
    throw std::invalid_argument("preview_count must be 1..20");
  if (!std::isfinite(config.gravity) || config.gravity < 0 ||
      !std::isfinite(config.gravity_increase) || config.gravity_increase < 0)
    throw std::invalid_argument("gravity and gravity_increase must be finite and non-negative");
  if (!std::isfinite(config.attacks.combo_multiplier) ||
      config.attacks.combo_multiplier < 0 || config.attacks.combo_multiplier > 100)
    throw std::invalid_argument("attack combo_multiplier must be between 0 and 100");

  std::unique_ptr<RotationSystem> rotation;
  switch (config.rotation) {
  case RotationRule::SRSPlus: rotation = std::make_unique<SRSPlus>(); break;
  case RotationRule::SRSX: rotation = std::make_unique<SRSX>(); break;
  case RotationRule::Off: rotation = std::make_unique<None>(); break;
  }

  std::unique_ptr<Randomizer> bag;
  switch (config.bag) {
  case BagRule::Seven: bag = std::make_unique<SevenBag>(); break;
  case BagRule::Fourteen: bag = std::make_unique<FourteenBag>(); break;
  case BagRule::Random: bag = std::make_unique<Random>(); break;
  }

  std::unique_ptr<SpinSystem> spin;
  switch (config.spin) {
  case SpinRule::AllMiniPlus: spin = std::make_unique<AllSpin>(true); break;
  case SpinRule::AllMini: spin = std::make_unique<AllSpin>(false); break;
  case SpinRule::TOnly: spin = std::make_unique<TSpin>(); break;
  case SpinRule::Off: break;
  }

  std::unique_ptr<AttackScheme> attack;
  switch (config.attack) {
  case AttackRule::Season2: attack = std::make_unique<TetrioSS2>(); break;
  case AttackRule::Season1: attack = std::make_unique<TetrioSS1>(); break;
  case AttackRule::Jstris: attack = std::make_unique<JSTris>(); break;
  case AttackRule::Custom: attack = std::make_unique<TableAttack>(config.attacks); break;
  case AttackRule::Off: break;
  }

  Engine engine(Board(config.width, config.height), std::move(rotation),
                std::move(bag), std::move(spin),
                std::make_unique<Classical>(config.scores), std::move(attack),
                config.preview_count, (config.width - PieceDimension) / 2,
                std::min<int>(config.height - PieceDimension, VisibleHeight - 3),
                config.gravity, config.gravity_increase, config.gravity_margin,
                config.lock_delay);
  engine.set_lock_delay(config.lock_delay, config.lock_resets);
  engine.set_garbage_delay(config.garbage_delay);
  return engine;
}

} // namespace

PYBIND11_MODULE(tetris_py, m) {
  m.doc() = "Headless Tetris engine bindings (board rows start at the bottom)";

  py::enum_<Cell>(m, "Cell")
      .value("Empty", Cell::Empty).value("I", Cell::I).value("O", Cell::O)
      .value("T", Cell::T).value("S", Cell::S).value("Z", Cell::Z)
      .value("J", Cell::J).value("L", Cell::L).value("Garbage", Cell::Garbage)
      .value("Unclearable", Cell::Unclearable);
  py::enum_<PieceType>(m, "PieceType")
      .value("I", PieceType::I).value("O", PieceType::O).value("T", PieceType::T)
      .value("S", PieceType::S).value("Z", PieceType::Z).value("J", PieceType::J)
      .value("L", PieceType::L);
  py::enum_<Rotation>(m, "Rotation")
      .value("North", Rotation::North).value("East", Rotation::East)
      .value("South", Rotation::South).value("West", Rotation::West);
  py::enum_<Movement>(m, "Movement")
      .value("None_", Movement::None).value("Left", Movement::Left)
      .value("Right", Movement::Right).value("SoftDrop", Movement::SoftDrop)
      .value("HardDrop", Movement::HardDrop).value("CW", Movement::CW)
      .value("CCW", Movement::CCW).value("HalfRotation", Movement::HalfRotation)
      .value("Gravity", Movement::Gravity);
  py::enum_<SpinType>(m, "SpinType")
      .value("None_", SpinType::None).value("Mini", SpinType::Mini)
      .value("Full", SpinType::Full);
  py::enum_<RotationRule>(m, "RotationRule")
      .value("SRSPlus", RotationRule::SRSPlus).value("SRSX", RotationRule::SRSX)
      .value("Off", RotationRule::Off);
  py::enum_<BagRule>(m, "BagRule")
      .value("Seven", BagRule::Seven).value("Fourteen", BagRule::Fourteen)
      .value("Random", BagRule::Random);
  py::enum_<SpinRule>(m, "SpinRule")
      .value("AllMiniPlus", SpinRule::AllMiniPlus).value("AllMini", SpinRule::AllMini)
      .value("TOnly", SpinRule::TOnly).value("Off", SpinRule::Off);
  py::enum_<AttackRule>(m, "AttackRule")
      .value("Season2", AttackRule::Season2).value("Season1", AttackRule::Season1)
      .value("Jstris", AttackRule::Jstris).value("Custom", AttackRule::Custom)
      .value("Off", AttackRule::Off);
  py::enum_<SoloMode>(m, "SoloMode")
      .value("Marathon", SoloMode::Marathon).value("Sprint", SoloMode::Sprint)
      .value("Blitz", SoloMode::Blitz);

  py::class_<AttackTable>(m, "AttackTable")
      .def(py::init<>())
      .def_readwrite("normal", &AttackTable::normal)
      .def_readwrite("mini", &AttackTable::mini)
      .def_readwrite("full", &AttackTable::full)
      .def_readwrite("b2b_bonus", &AttackTable::b2b_bonus)
      .def_readwrite("perfect_clear_bonus", &AttackTable::perfect_clear_bonus)
      .def_readwrite("combo_multiplier", &AttackTable::combo_multiplier);
  py::class_<ScoreTable>(m, "ScoreTable")
      .def(py::init<>())
      .def_readwrite("normal", &ScoreTable::normal)
      .def_readwrite("mini", &ScoreTable::mini)
      .def_readwrite("full", &ScoreTable::full)
      .def_readwrite("perfect_clear_bonus", &ScoreTable::perfect_clear_bonus)
      .def_readwrite("combo_bonus", &ScoreTable::combo_bonus)
      .def_readwrite("hard_drop_point", &ScoreTable::hard_drop_point)
      .def_readwrite("soft_drop_point", &ScoreTable::soft_drop_point);
  py::class_<EngineConfig>(m, "EngineConfig",
      "Rule choices and timing for new Engine, SoloSession, and Match instances.")
      .def(py::init<>())
      .def_readwrite("width", &EngineConfig::width)
      .def_readwrite("height", &EngineConfig::height)
      .def_readwrite("preview_count", &EngineConfig::preview_count)
      .def_readwrite("gravity", &EngineConfig::gravity)
      .def_readwrite("gravity_increase", &EngineConfig::gravity_increase)
      .def_readwrite("gravity_margin", &EngineConfig::gravity_margin)
      .def_readwrite("lock_delay", &EngineConfig::lock_delay)
      .def_readwrite("lock_resets", &EngineConfig::lock_resets)
      .def_readwrite("garbage_delay", &EngineConfig::garbage_delay)
      .def_readwrite("rotation", &EngineConfig::rotation)
      .def_readwrite("bag", &EngineConfig::bag)
      .def_readwrite("spin", &EngineConfig::spin)
      .def_readwrite("attack", &EngineConfig::attack)
      .def_readwrite("attacks", &EngineConfig::attacks)
      .def_readwrite("scores", &EngineConfig::scores);

  py::class_<Position>(m, "Position")
      .def_readonly("row", &Position::row).def_readonly("col", &Position::col);
  py::class_<Offset>(m, "Offset")
      .def_readonly("row", &Offset::row).def_readonly("col", &Offset::col);
  py::class_<SpinContext>(m, "SpinContext")
      .def_readonly("last_movement", &SpinContext::last_movement)
      .def_readonly("kick_offset", &SpinContext::kick_offset);
  py::class_<Placement>(m, "Placement",
      "Engine-generated placement with its final pose and recorded movement path.")
      .def_readonly("moves", &Placement::moves)
      .def_readonly("type", &Placement::type)
      .def_readonly("final_pos", &Placement::final_pos)
      .def_readonly("final_rotation", &Placement::final_rotation)
      .def_readonly("use_hold", &Placement::use_hold)
      .def_readonly("drop_distance", &Placement::drop_distance)
      .def_readonly("spin_context", &Placement::spin_context)
      .def("cells", &Placement::cells,
           "Four landing cells before line clearing, in board row/column coordinates.");
  py::class_<Piece>(m, "Piece")
      .def_property_readonly("type", &Piece::type)
      .def_property_readonly("rotation", &Piece::rotation)
      .def_property_readonly("cell_type", &Piece::cell_type)
      .def("current_shape", &Piece::current_shape)
      .def("render", &Piece::render);
  py::class_<ActivePiece>(m, "ActivePiece")
      .def_readonly("piece", &ActivePiece::piece)
      .def_readonly("pos", &ActivePiece::pos);
  py::class_<Board>(m, "Board")
      .def_property_readonly("width", &Board::get_width)
      .def_property_readonly("height", &Board::get_height)
      .def("get", &Board::get, py::arg("row"), py::arg("col"))
      .def("render", &Board::render)
      .def("max_height", &Board::max_height)
      .def("column_height", &Board::column_height)
      .def("aggregate_height", &Board::aggregate_height);

  py::class_<GarbageEvent>(m, "GarbageEvent")
      .def(py::init([](uint16_t lines, uint16_t hole_col) {
        return GarbageEvent{lines, hole_col};
      }), py::arg("lines"), py::arg("hole_col"))
      .def_readonly("lines", &GarbageEvent::lines)
      .def_readonly("hole_col", &GarbageEvent::hole_col);
  py::class_<PendingGarbage>(m, "PendingGarbage")
      .def_readonly("event", &PendingGarbage::event)
      .def_readonly("delay_ticks", &PendingGarbage::delay_ticks);
  py::class_<LockResult>(m, "LockResult")
      .def_readonly("locked", &LockResult::locked)
      .def_readonly("game_over", &LockResult::game_over)
      .def_readonly("lines_cleared", &LockResult::lines_cleared)
      .def_readonly("perfect_clear", &LockResult::perfect_clear)
      .def_readonly("spin", &LockResult::spin)
      .def_readonly("attack_sent", &LockResult::attack_sent)
      .def_readonly("attack_packets", &LockResult::attack_packets)
      .def_readonly("score_delta", &LockResult::score_delta);
  py::class_<TickResult>(m, "TickResult")
      .def_readonly("moved_down", &TickResult::moved_down)
      .def_readonly("locked", &TickResult::locked)
      .def_readonly("game_over", &TickResult::game_over)
      .def_readonly("lines_cleared", &TickResult::lines_cleared)
      .def_readonly("attack_sent", &TickResult::attack_sent)
      .def_readonly("attack_packets", &TickResult::attack_packets);

  py::class_<Engine>(m, "Engine",
      "Headless engine. Call reset(seed) and spawn_next_piece(); locks do not spawn.")
      .def(py::init([](const EngineConfig &config) {
        return std::make_unique<Engine>(make_engine(config));
      }), py::arg("config") = EngineConfig{})
      .def("reset", &Engine::reset, py::arg("seed") = 0)
      .def("clone", [](const Engine &engine) {
        return std::make_unique<Engine>(engine.clone());
      }, "Independent engine copy, including RNG and hidden bag state. Keep it out of model inputs.")
      .def("restore", &Engine::restore, py::arg("snapshot"),
           "Restore in place from a clone with the same rule context, including RNG.")
      .def("spawn_next_piece", &Engine::spawn_next_piece,
           py::arg("clutch_clear") = false)
      .def("hold", &Engine::hold, py::arg("ignore_hold") = false)
      .def("try_move_left", &Engine::try_move_left, py::arg("amount") = 1)
      .def("try_move_right", &Engine::try_move_right, py::arg("amount") = 1)
      .def("try_soft_drop", &Engine::try_soft_drop, py::arg("amount") = 1)
      .def("try_rotate_cw", &Engine::try_rotate_cw)
      .def("try_rotate_ccw", &Engine::try_rotate_ccw)
      .def("try_rotate_180", &Engine::try_rotate_180)
      .def("hard_drop", &Engine::hard_drop)
      .def("lock_active_piece", &Engine::lock_active_piece)
      .def("hard_drop_distance", &Engine::hard_drop_distance)
      .def("tick", &Engine::tick)
      .def("set_gravity", &Engine::set_gravity)
      .def("set_gravity_increase", &Engine::set_gravity_increase,
           py::arg("per_tick"), py::arg("margin_tick"))
      .def("set_lock_delay", &Engine::set_lock_delay,
           py::arg("ticks"), py::arg("reset_limit") = 15)
      .def("set_garbage_delay", &Engine::set_garbage_delay)
      .def("set_edit_mode", &Engine::set_edit_mode)
      .def("paint_cell", &Engine::paint_cell,
           py::arg("row"), py::arg("col"), py::arg("cell"))
      .def("receive_garbage", py::overload_cast<uint16_t, uint16_t>(
           &Engine::receive_garbage), py::arg("lines"), py::arg("hole_col"))
      .def("receive_garbage", py::overload_cast<const GarbageEvent &>(
           &Engine::receive_garbage), py::arg("event"))
      .def("preview_queue", &Engine::preview_queue)
      .def("generate_active_piece_placement", &Engine::generate_active_piece_placement,
           "Reachable hard-drop placements for the active piece.")
      .def("generate_hold_piece_placement", &Engine::generate_hold_piece_placement,
           "Reachable hard-drop placements after hold.")
      .def("apply_placement", &Engine::apply_placement, py::arg("placement"),
           "Validate and atomically lock a placement without spawning the next piece.")
      .def("render", &Engine::render,
           "Board copy with the active piece overlaid; row zero is the bottom.")
      .def_property_readonly("board", &Engine::board, py::return_value_policy::reference_internal)
      .def_property_readonly("active_piece", [](const Engine &engine) {
        return std::optional<ActivePiece>(engine.active_piece());
      })
      .def_property_readonly("ghost_piece", &Engine::ghost_piece)
      .def_property_readonly("hold_piece", &Engine::hold_piece)
      .def_property_readonly("last_lock_result", [](const Engine &engine) {
        return std::optional<LockResult>(engine.last_lock_result());
      })
      .def_property_readonly("game_over", &Engine::game_over)
      .def_property_readonly("can_hold", &Engine::can_hold)
      .def_property_readonly("edit_mode", &Engine::edit_mode)
      .def_property_readonly("lines_cleared", &Engine::lines_cleared)
      .def_property_readonly("score", &Engine::score)
      .def_property_readonly("combo", &Engine::combo)
      .def_property_readonly("back_to_back", &Engine::back_to_back)
      .def_property_readonly("back_to_back_count", &Engine::back_to_back_count)
      .def_property_readonly("surge_charge", &Engine::surge_charge)
      .def_property_readonly("tick_count", &Engine::tick_count)
      .def_property_readonly("pending_garbage_lines", &Engine::pending_garbage_lines)
      .def_property_readonly("pending_garbage_packets", &Engine::pending_garbage_packets)
      .def("pending_garbage", &Engine::pending_garbage,
           "Copy of incoming packets in arrival order, including travel delays.");

  py::class_<SoloSettings>(m, "SoloSettings")
      .def(py::init<>())
      .def_readwrite("mode", &SoloSettings::mode)
      .def_readwrite("line_goal", &SoloSettings::line_goal)
      .def_readwrite("time_limit_ticks", &SoloSettings::time_limit_ticks);
  py::class_<SoloSession>(m, "SoloSession",
      "Solo objective on top of Engine; start(seed) spawns the first piece.")
      .def(py::init([](const EngineConfig &config, const SoloSettings &settings) {
        return std::make_unique<SoloSession>(make_engine(config), settings);
      }), py::arg("config") = EngineConfig{}, py::arg("settings") = SoloSettings{})
      .def("start", &SoloSession::start, py::arg("seed") = 0)
      .def("tick", &SoloSession::tick)
      .def("hard_drop", &SoloSession::hard_drop)
      .def_property_readonly("engine", [](SoloSession &session) -> Engine & {
        return session.engine();
      }, py::return_value_policy::reference_internal)
      .def_property_readonly("finished", &SoloSession::finished)
      .def_property_readonly("succeeded", &SoloSession::succeeded)
      .def_property_readonly("elapsed_ticks", &SoloSession::elapsed_ticks);

  py::class_<Match>(m, "Match",
      "Local multiplayer; start(seed) seeds all players and incoming holes.")
      .def(py::init([](const std::vector<EngineConfig> &configs, uint16_t wins_to_win) {
        std::vector<std::unique_ptr<Engine>> players;
        players.reserve(configs.size());
        for (const auto &config : configs)
          players.push_back(std::make_unique<Engine>(make_engine(config)));
        return std::make_unique<Match>(std::move(players), wins_to_win);
      }), py::arg("configs"), py::arg("wins_to_win") = 1)
      .def("start", &Match::start, py::arg("seed") = 0)
      .def("tick", &Match::tick)
      .def("hard_drop", &Match::hard_drop, py::arg("player"))
      .def("player", [](Match &match, size_t index) -> Engine & {
        return match.player(index);
      }, py::arg("index"), py::return_value_policy::reference_internal)
      .def("set_target", &Match::set_target, py::arg("sender"), py::arg("target"))
      .def("wins", &Match::wins, py::arg("index"))
      .def_property_readonly("player_count", &Match::player_count)
      .def_property_readonly("round_over", &Match::round_over)
      .def_property_readonly("round_winner", &Match::round_winner)
      .def_property_readonly("match_winner", &Match::match_winner);
}
