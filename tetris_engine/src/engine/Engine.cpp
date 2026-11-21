#include "core/Core.hpp"
#include "core/Piece.hpp"

#include "engine/Engine.hpp"
#include "engine/Movement.hpp"
#include "engine/Spin.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tetris {

namespace {

bool execute_move(Engine &engine, Movement move) {
  switch (move) {
  case Movement::Left: return engine.try_move_left();
  case Movement::Right: return engine.try_move_right();
  case Movement::SoftDrop: return engine.try_soft_drop();
  case Movement::CW: return engine.try_rotate_cw();
  case Movement::CCW: return engine.try_rotate_ccw();
  case Movement::HalfRotation: return engine.try_rotate_180();
  default: return false; // The terminal hard drop is validated separately.
  }
}

bool rotation_move(Movement move) {
  return move == Movement::CW || move == Movement::CCW ||
         move == Movement::HalfRotation;
}

using SearchKey = std::tuple<int, int, int, int, int, int, int>;
using PlacementKey = std::tuple<int, int, int, SpinType>;

template <typename Tuple> struct TupleHash {
  size_t operator()(const Tuple &key) const noexcept {
    size_t hash = 0;
    std::apply([&](const auto &...values) {
      ((hash ^= std::hash<int>{}(static_cast<int>(values)) +
                0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2)), ...);
    }, key);
    return hash;
  }
};

SearchKey search_key(const ActivePiece &piece, const SpinContext &context) {
  bool rotated = rotation_move(context.last_movement);
  return {piece.pos.row, piece.pos.col, static_cast<int>(piece.piece.rotation()),
          rotated ? static_cast<int>(context.last_movement) : 0,
          rotated && context.kick_offset.has_value(),
          rotated && context.kick_offset ? context.kick_offset->row : 0,
          rotated && context.kick_offset ? context.kick_offset->col : 0};
}

} // namespace

Engine Engine::clone() const {
  Engine copy(_board, _rotation_system->clone(), _randomizer->clone(),
              _spin_system ? _spin_system->clone() : nullptr,
              _score_system ? _score_system->clone() : nullptr,
              _attack_scheme ? _attack_scheme->clone() : nullptr,
              _preview_count, _spawn_col_offset, _spawn_row_offset,
              _starting_gravity, _gravity_increase, _gravity_margin_time,
              _lock_delay);
  copy._active_piece = _active_piece;
  copy._hold_piece = _hold_piece;
  copy._game_over = _game_over;
  copy._can_hold = _can_hold;
  copy._lines_cleared = _lines_cleared;
  copy._score = _score;
  copy._combo = _combo;
  copy._back_to_back = _back_to_back;
  copy._back_to_back_count = _back_to_back_count;
  copy._tick_count = _tick_count;
  copy._gravity_progress = _gravity_progress;
  copy._grounded_last_tick = _grounded_last_tick;
  copy._lock_timer = _lock_timer;
  copy._lock_resets_used = _lock_resets_used;
  copy._lock_reset_limit = _lock_reset_limit;
  copy._last_tick_was_player_action = _last_tick_was_player_action;
  copy._gravity = _gravity;
  copy._gravity_counter = _gravity_counter;
  copy._spin_context = _spin_context;
  copy._last_successful_placement = _last_successful_placement;
  copy._last_lock_result = _last_lock_result;
  copy._soft_drop_cells = _soft_drop_cells;
  copy._hard_drop_cells = _hard_drop_cells;
  copy._edit_mode = _edit_mode;
  copy._pending_garbage = _pending_garbage;
  copy._garbage_queue_delay = _garbage_queue_delay;
  copy._rule_context = _rule_context;
  return copy;
}

void Engine::restore(const Engine &snapshot) {
  if (this == &snapshot)
    return;
  if (_rule_context != snapshot._rule_context)
    throw std::invalid_argument("restore requires a clone of the same rule context");
  // Restore the bag and RNG in place: no rule allocations per candidate.
  _randomizer->restore_from(*snapshot._randomizer);
  _board = snapshot._board;
  _active_piece = snapshot._active_piece;
  _hold_piece = snapshot._hold_piece;
  _game_over = snapshot._game_over;
  _can_hold = snapshot._can_hold;
  _lines_cleared = snapshot._lines_cleared;
  _score = snapshot._score;
  _combo = snapshot._combo;
  _back_to_back = snapshot._back_to_back;
  _back_to_back_count = snapshot._back_to_back_count;
  _spawn_col_offset = snapshot._spawn_col_offset;
  _spawn_row_offset = snapshot._spawn_row_offset;
  _tick_count = snapshot._tick_count;
  _gravity_progress = snapshot._gravity_progress;
  _grounded_last_tick = snapshot._grounded_last_tick;
  _lock_timer = snapshot._lock_timer;
  _lock_resets_used = snapshot._lock_resets_used;
  _lock_reset_limit = snapshot._lock_reset_limit;
  _last_tick_was_player_action = snapshot._last_tick_was_player_action;
  _starting_gravity = snapshot._starting_gravity;
  _gravity = snapshot._gravity;
  _gravity_counter = snapshot._gravity_counter;
  _gravity_increase = snapshot._gravity_increase;
  _gravity_margin_time = snapshot._gravity_margin_time;
  _lock_delay = snapshot._lock_delay;
  _spin_context = snapshot._spin_context;
  _last_successful_placement = snapshot._last_successful_placement;
  _last_lock_result = snapshot._last_lock_result;
  _soft_drop_cells = snapshot._soft_drop_cells;
  _hard_drop_cells = snapshot._hard_drop_cells;
  _edit_mode = snapshot._edit_mode;
  _pending_garbage = snapshot._pending_garbage;
  _garbage_queue_delay = snapshot._garbage_queue_delay;
}

void Engine::reset(uint64_t seed) {
  _randomizer->seed(seed);
  _board.reset();
  _can_hold = true;
  _game_over = false;
  _active_piece = std::nullopt;
  _hold_piece = std::nullopt;
  _spin_context = {};
  _tick_count = 0;
  _gravity_counter = 0;
  _gravity = _starting_gravity;
  _gravity_progress = 0;
  _grounded_last_tick = false;
  _lock_timer = 0;
  _lock_resets_used = 0;
  _last_tick_was_player_action = false;
  _lines_cleared = 0;
  _score = 0;
  _combo = 0;
  _back_to_back = false;
  _back_to_back_count = 0;
  _last_successful_placement.reset();
  _last_lock_result.reset();
  _pending_garbage.clear();
  _soft_drop_cells = 0;
  _hard_drop_cells = 0;
  _edit_mode = false;
}

void Engine::set_gravity(double cells_per_tick) {
  if (!std::isfinite(cells_per_tick) || cells_per_tick < 0)
    throw std::invalid_argument("gravity must be a finite non-negative number");
  _gravity = cells_per_tick;
}

void Engine::set_gravity_increase(double per_tick, uint64_t margin_tick) {
  if (!std::isfinite(per_tick) || per_tick < 0)
    throw std::invalid_argument("gravity increase must be finite and non-negative");
  _gravity_increase = per_tick;
  _gravity_margin_time = margin_tick;
}

void Engine::set_lock_delay(uint64_t ticks, uint16_t reset_limit) {
  _lock_delay = ticks;
  _lock_reset_limit = reset_limit;
}

uint32_t Engine::pending_garbage_lines() const {
  uint32_t lines = 0;
  for (const auto &packet : _pending_garbage)
    lines += packet.event.lines;
  return lines;
}

bool Engine::set_edit_mode(bool enabled) {
  if (!enabled && _active_piece && _board.collide(*_active_piece))
    return false;
  _edit_mode = enabled;
  return true;
}

bool Engine::paint_cell(uint16_t row, uint16_t col, Cell cell) {
  if (!_edit_mode || row >= _board.get_height() || col >= _board.get_width())
    return false;
  _board.set(row, col, cell);
  return true;
}

void Engine::on_player_move(bool was_grounded) {
  if (!was_grounded || _lock_resets_used < _lock_reset_limit) {
    _lock_timer = 0;
    if (was_grounded)
      ++_lock_resets_used;
  }
  _grounded_last_tick = _board.grounded(*_active_piece);
  _last_tick_was_player_action = true;
}

std::vector<PieceType> Engine::preview_queue() const {
  return _randomizer->preview(_preview_count);
}

bool Engine::spawn_next_piece(bool clutch_clear) {
  if (_game_over || _edit_mode || _active_piece)
    return false;

  ActivePiece nextPiece = spawn_from_piece_type(
      _randomizer->peek(), _spawn_row_offset, _spawn_col_offset);

  if (_board.collide(nextPiece)) {
    if (!clutch_clear) {
      if (!_active_piece)
        _game_over = true;
      return false;
    }

    while (nextPiece.pos.row + PieceDimension < _board.get_height()) {
      nextPiece.pos.row++;
      if (!_board.collide(nextPiece)) {
        _randomizer->pop();
        _active_piece = nextPiece;
        _can_hold = true;
        _spin_context = {};
        _lock_timer = _lock_resets_used = 0;
        _gravity_counter = 0;
        _grounded_last_tick = false;
        _soft_drop_cells = _hard_drop_cells = 0;
        return true;
      }
    }

    if (!_active_piece)
      _game_over = true;
    return false;
  }

  _randomizer->pop();
  _active_piece = nextPiece;
  _can_hold = true;
  _spin_context = {};
  _lock_timer = _lock_resets_used = 0;
  _gravity_counter = 0;
  _grounded_last_tick = false;
  _soft_drop_cells = _hard_drop_cells = 0;
  return true;
}

bool Engine::hold(bool ignore_hold) {
  if (_game_over || _edit_mode || !_active_piece.has_value())
    return false;

  if (!ignore_hold && !_can_hold)
    return false;

  if (!_hold_piece.has_value()) {
    ActivePiece next = spawn_from_piece_type(_randomizer->peek(),
                                             _spawn_row_offset, _spawn_col_offset);
    if (_board.collide(next))
      return false;
    _hold_piece = _active_piece->piece.type();
    _randomizer->pop();
    _active_piece = next;
  } else {
    ActivePiece nextPiece = spawn_from_piece_type(
        _hold_piece.value(), _spawn_row_offset, _spawn_col_offset);
    if (_board.collide(nextPiece))
      return false;

    _hold_piece = _active_piece->piece.type();
    _active_piece = nextPiece;
  }
  _can_hold = false;
  _spin_context = {};
  _lock_timer = _lock_resets_used = 0;
  _gravity_counter = 0;
  _grounded_last_tick = false;
  _soft_drop_cells = _hard_drop_cells = 0;
  return true;
}

bool Engine::try_move_left(int16_t amount) {
  if (_game_over || _edit_mode || !_active_piece.has_value() || amount == 0)
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;
  bool was_grounded = _board.grounded(p);

  for (int32_t amt = amount < 0 ? _board.get_width() : amount; amt > 0; amt--) {
    p.pos.col--;
    if (_board.collide(p)) {
      p.pos.col++;
      if (p.pos.col == _active_piece.value().pos.col)
        return false;
      _active_piece = p;
      _spin_context.kick_offset = std::nullopt;
      _spin_context.last_movement = Movement::Left;
      on_player_move(was_grounded);
      return true;
    }
  }

  _active_piece = p;
  _spin_context.kick_offset = std::nullopt;
  _spin_context.last_movement = Movement::Left;
  on_player_move(was_grounded);
  return true;
}

bool Engine::try_move_right(int16_t amount) {
  if (_game_over || _edit_mode || !_active_piece.has_value() || amount == 0)
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;
  bool was_grounded = _board.grounded(p);

  for (int32_t amt = amount < 0 ? _board.get_width() : amount; amt > 0; amt--) {
    p.pos.col++;
    if (_board.collide(p)) {
      p.pos.col--;
      if (p.pos.col == _active_piece.value().pos.col)
        return false;
      _active_piece = p;
      _spin_context.kick_offset = std::nullopt;
      _spin_context.last_movement = Movement::Right;
      on_player_move(was_grounded);
      return true;
    }
  }

  _active_piece = p;
  _spin_context.kick_offset = std::nullopt;
  _spin_context.last_movement = Movement::Right;
  on_player_move(was_grounded);
  return true;
}

bool Engine::try_soft_drop(int16_t amount) {
  if (_game_over || _edit_mode || !_active_piece.has_value() || amount == 0)
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;
  bool was_grounded = _board.grounded(p);
  int16_t old_row = p.pos.row;

  for (int32_t amt = amount < 0 ? _board.get_height() : amount; amt > 0;
       amt--) {
    p.pos.row--;
    if (_board.collide(p)) {
      p.pos.row++;
      if (p.pos.row == _active_piece.value().pos.row)
        return false;
      _active_piece = p;
      _spin_context.kick_offset = std::nullopt;
      _spin_context.last_movement = Movement::SoftDrop;
      _soft_drop_cells += old_row - p.pos.row;
      on_player_move(was_grounded);
      return true;
    }
  }

  _active_piece = p;
  _spin_context.kick_offset = std::nullopt;
  _spin_context.last_movement = Movement::SoftDrop;
  _soft_drop_cells += old_row - p.pos.row;
  on_player_move(was_grounded);
  return true;
}

bool Engine::try_rotate_cw() {
  if (_game_over || _edit_mode || !_active_piece.has_value() ||
      !_rotation_system->allows_rotation())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;
  bool was_grounded = _board.grounded(p);

  Rotation r = p.piece.rotation();
  p.piece.rotate_cw();
  if (!_board.collide(p)) {
    _active_piece = p;
    _spin_context.kick_offset = std::nullopt;
    _spin_context.last_movement = Movement::CW;
    on_player_move(was_grounded);
    return true;
  }

  std::vector offsets =
      _rotation_system->kick_offsets(p.piece.type(), r, p.piece.rotation());
  for (Offset o : offsets) {
    p.pos = {.row = static_cast<int16_t>(_active_piece->pos.row + o.row),
             .col = static_cast<int16_t>(_active_piece->pos.col + o.col)};
    if (!_board.collide(p)) {
      _active_piece = p;
      _spin_context.kick_offset = o;
      _spin_context.last_movement = Movement::CW;
      on_player_move(was_grounded);
      return true;
    }
  }

  return false;
}

bool Engine::try_rotate_ccw() {
  if (_game_over || _edit_mode || !_active_piece.has_value() ||
      !_rotation_system->allows_rotation())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;
  bool was_grounded = _board.grounded(p);

  Rotation r = p.piece.rotation();
  p.piece.rotate_ccw();
  if (!_board.collide(p)) {
    _active_piece = p;
    _spin_context.kick_offset = std::nullopt;
    _spin_context.last_movement = Movement::CCW;
    on_player_move(was_grounded);
    return true;
  }

  std::vector offsets =
      _rotation_system->kick_offsets(p.piece.type(), r, p.piece.rotation());
  for (Offset o : offsets) {
    p.pos = {.row = static_cast<int16_t>(_active_piece->pos.row + o.row),
             .col = static_cast<int16_t>(_active_piece->pos.col + o.col)};
    if (!_board.collide(p)) {
      _active_piece = p;
      _spin_context.kick_offset = o;
      _spin_context.last_movement = Movement::CCW;
      on_player_move(was_grounded);
      return true;
    }
  }

  return false;
}

bool Engine::try_rotate_180() {
  if (_game_over || _edit_mode || !_active_piece.has_value() ||
      !_rotation_system->allows_rotation())
    return false;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return false;
  bool was_grounded = _board.grounded(p);

  Rotation r = p.piece.rotation();
  p.piece.rotate_180();
  if (!_board.collide(p)) {
    _active_piece = p;
    _spin_context.kick_offset = std::nullopt;
    _spin_context.last_movement = Movement::HalfRotation;
    on_player_move(was_grounded);
    return true;
  }

  std::vector offsets =
      _rotation_system->kick_offsets(p.piece.type(), r, p.piece.rotation());
  for (Offset o : offsets) {
    p.pos = {.row = static_cast<int16_t>(_active_piece->pos.row + o.row),
             .col = static_cast<int16_t>(_active_piece->pos.col + o.col)};
    if (!_board.collide(p)) {
      _active_piece = p;
      _spin_context.kick_offset = o;
      _spin_context.last_movement = Movement::HalfRotation;
      on_player_move(was_grounded);
      return true;
    }
  }

  return false;
}

std::optional<ActivePiece> Engine::ghost_piece() const {
  std::optional<ActivePiece> res = _active_piece;

  if (!res.has_value() || _board.collide(res.value()))
    return std::nullopt;

  while (res->pos.row > -PieceDimension && !_board.grounded(res.value()))
    res->pos.row--;

  return res;
}

std::vector<std::vector<Cell>> Engine::render() const {
  auto cells = _board.render();
  if (_active_piece && !_board.collide(*_active_piece)) {
    for (Offset offset : _active_piece->piece.current_shape_array()) {
      int row = _active_piece->pos.row + offset.row;
      int col = _active_piece->pos.col + offset.col;
      if (row >= 0 && col >= 0 && row < _board.get_height() &&
          col < _board.get_width())
        cells[row][col] = _active_piece->piece.cell_type();
    }
  }
  return cells;
}

uint16_t Engine::hard_drop_distance() const {
  if (!_active_piece.has_value())
    return 0;

  ActivePiece p = _active_piece.value();

  if (_board.collide(p))
    return 0;
  uint16_t res = 0;
  for (; !_board.grounded(p) && res <= _board.get_height() + PieceDimension;
       res++)
    p.pos.row--;

  return res;
}

TickResult Engine::tick() {
  if (_game_over || _edit_mode)
    return {.game_over = _game_over};

  ++_tick_count;
  for (auto &packet : _pending_garbage)
    if (packet.delay_ticks > 0)
      --packet.delay_ticks;
  if (_tick_count > _gravity_margin_time)
    _gravity += _gravity_increase;

  if (!_active_piece)
    return {};

  ActivePiece p = _active_piece.value();
  TickResult res{};
  if (!_board.grounded(p)) {
    _gravity_counter += _gravity;
    int steps = static_cast<int>(std::min<double>(
        std::floor(_gravity_counter), _board.get_height()));
    for (int i = 0; i < steps; ++i) {
      --p.pos.row;
      if (_board.collide(p)) {
        ++p.pos.row;
        break;
      }
      res.moved_down = true;
    }
    if (steps > 0)
      _gravity_counter -= std::floor(_gravity_counter);
    if (res.moved_down) {
      _active_piece = p;
      _spin_context.kick_offset = std::nullopt;
      _spin_context.last_movement = Movement::Gravity;
    }
  }

  bool grounded = _board.grounded(*_active_piece);
  if (grounded) {
    // Landing starts the delay; a previously grounded piece counts this tick.
    if (_grounded_last_tick || !res.moved_down)
      ++_lock_timer;
    if (_lock_delay == 0 || _lock_timer >= _lock_delay) {
      LockResult lock = lock_active_piece();
      res.locked = lock.locked;
      res.lines_cleared = lock.lines_cleared;
      res.attack_sent = lock.attack_sent;
      res.attack_packets = std::move(lock.attack_packets);
      res.game_over = lock.game_over;
    }
  } else {
    _lock_timer = 0;
  }
  _grounded_last_tick = grounded && !res.locked;
  return res;
}

LockResult Engine::hard_drop() {
  if (_game_over || _edit_mode || !_active_piece || _board.collide(*_active_piece))
    return {};
  ActivePiece p = *_active_piece;
  uint16_t distance = 0;
  while (!_board.grounded(p)) {
    --p.pos.row;
    ++distance;
  }
  if (distance > 0) {
    _active_piece = p;
    _spin_context.last_movement = Movement::HardDrop;
    _spin_context.kick_offset.reset();
  }
  _hard_drop_cells = distance;
  return lock_active_piece();
}

LockResult Engine::lock_active_piece() {
  if (_game_over || _edit_mode || !_active_piece)
    return {};

  ActivePiece p = _active_piece.value();

  // Grounded test, do nothing if not grounded
  if (!_board.grounded(p))
    return {};
  SpinType spin = _spin_system ? _spin_system->detect(_board, p, _spin_context)
                               : SpinType::None;
  for (Offset o : p.piece.current_shape_array())
    _board.set(o.row + p.pos.row, o.col + p.pos.col, p.piece.cell_type());

  _active_piece = std::nullopt;
  bool cleared_garbage = false;
  uint16_t lc = _board.clear_lines(&cleared_garbage);
  bool perfect_clear = lc > 0 && _board.max_height() == 0;
  if (lc != 0) {
    _combo = _last_lock_result && _last_lock_result->lines_cleared ? _combo + 1 : 0;
  } else {
    _combo = 0;
  }
  bool difficult = (lc > 0 && (spin != SpinType::None || lc >= 4 ||
                              (perfect_clear && _attack_scheme &&
                               _attack_scheme->perfect_clear_keeps_b2b()))) ||
                   (lc == 0 && _attack_scheme &&
                    _attack_scheme->no_line_spin_keeps_b2b(spin, p.piece.type()));
  bool back_to_back_bonus = difficult && _back_to_back;
  const uint32_t surge_on_break = lc > 0 && !difficult ? surge_charge() : 0;
  if (lc > 0 || difficult) {
    if (back_to_back_bonus && _back_to_back_count < UINT32_MAX)
      ++_back_to_back_count;
    else if (!difficult)
      _back_to_back_count = 0;
    _back_to_back = difficult;
  }

  ScoreEvent score_event{.lines_cleared = lc,
                         .spin = spin,
                         .perfect_clear = perfect_clear,
                         .back_to_back = back_to_back_bonus,
                         .combo = _combo,
                         .hard_drop_cells = _hard_drop_cells,
                         .soft_drop_cells = static_cast<uint16_t>(std::min<uint32_t>(
                             _soft_drop_cells, UINT16_MAX))};
  const uint64_t score_before = _score;
  if (_score_system)
    _score += _score_system->score(score_event);
  std::vector<uint16_t> outgoing_packets;
  if (_attack_scheme)
    outgoing_packets = _attack_scheme->garbage_packets(
        AttackContext{.lines_cleared = lc,
                       .spin = spin,
                       .perfect_clear = perfect_clear,
                       .back_to_back = back_to_back_bonus,
                       .combo = _combo,
                       .surge_on_break = surge_on_break,
                       .piece = p.piece.type(),
                       .cleared_garbage = cleared_garbage});
  // Full blocking: cancel oldest incoming lines before anything is sent.
  for (auto &packet : _pending_garbage) {
    for (auto &outgoing : outgoing_packets) {
      uint16_t cancelled = std::min(outgoing, packet.event.lines);
      packet.event.lines -= cancelled;
      outgoing -= cancelled;
    }
  }
  outgoing_packets.erase(std::remove(outgoing_packets.begin(), outgoing_packets.end(), 0),
                         outgoing_packets.end());
  uint16_t outgoing = static_cast<uint16_t>(std::accumulate(
      outgoing_packets.begin(), outgoing_packets.end(), 0u));
  _pending_garbage.erase(
      std::remove_if(_pending_garbage.begin(), _pending_garbage.end(),
                     [](const PendingGarbage &packet) { return packet.event.lines == 0; }),
      _pending_garbage.end());
  // Clearing a line tanks the remaining garbage, even if the clear sent no attack.
  if (lc == 0)
    apply_ready_garbage();
  _lines_cleared += lc;
  _spin_context = {};
  _gravity_counter = 0;
  _lock_timer = _lock_resets_used = 0;
  _soft_drop_cells = 0;
  _hard_drop_cells = 0;
  LockResult result{.game_over = _game_over,
                    .lines_cleared = lc,
                    .perfect_clear = perfect_clear,
                    .spin = spin,
                    .attack_sent = outgoing,
                    .locked = true,
                    .attack_packets = std::move(outgoing_packets),
                    .score_delta = _score - score_before};
  _last_lock_result = result;
  return result;
}

void Engine::receive_garbage(uint16_t lines, uint16_t hole_col) {
  receive_garbage(GarbageEvent{.lines = lines, .hole_col = hole_col});
}

void Engine::receive_garbage(const GarbageEvent &garbage) {
  if (_game_over || garbage.lines == 0 ||
      garbage.hole_col >= _board.get_width())
    return;
  _pending_garbage.push_back({.event = garbage,
                              .delay_ticks = _garbage_queue_delay});
}

void Engine::apply_ready_garbage() {
  uint16_t remaining = 8;
  while (!_pending_garbage.empty() &&
         _pending_garbage.front().delay_ticks == 0 && remaining > 0) {
    auto &packet = _pending_garbage.front();
    uint16_t lines = std::min(remaining, packet.event.lines);
    _board.add_garbage(packet.event.hole_col, lines);
    remaining -= lines;
    packet.event.lines -= lines;
    if (packet.event.lines == 0)
      _pending_garbage.pop_front();
    if (_board.touch_max_height()) {
      _game_over = true;
      break;
    }
  }
}

std::vector<Placement> Engine::generate_active_piece_placement() const {
  return search_placements(false);
}

std::vector<Placement> Engine::generate_hold_piece_placement() const {
  return search_placements(true);
}

std::vector<Placement> Engine::search_placements(bool use_hold) const {
  if (_game_over || _edit_mode || !_active_piece || (use_hold && !_can_hold))
    return {};
  Engine scratch = clone();
  if (use_hold && !scratch.hold())
    return {};

  struct Node {
    ActivePiece piece;
    SpinContext spin;
    uint32_t soft_drop_cells;
    size_t parent;
    Movement move;
  };
  const size_t root = std::numeric_limits<size_t>::max();
  std::vector<Node> queue;
  std::unordered_set<SearchKey, TupleHash<SearchKey>> visited;
  std::unordered_set<PlacementKey, TupleHash<PlacementKey>> endpoints;
  queue.push_back({*scratch._active_piece, scratch._spin_context,
                   scratch._soft_drop_cells, root, Movement::HardDrop});
  visited.insert(search_key(queue.front().piece, queue.front().spin));
  std::vector<Placement> placements;
  constexpr std::array moves = {Movement::Left, Movement::Right,
                                Movement::SoftDrop, Movement::CW,
                                Movement::CCW, Movement::HalfRotation};

  // Keep parent indices rather than copying the entire path for every
  // reachable movement state. Reconstruct only distinct locking paths.
  for (size_t head = 0; head < queue.size(); ++head) {
    const Node node = queue[head];
    scratch._active_piece = node.piece;
    scratch._spin_context = node.spin;
    auto ghost = scratch.ghost_piece();
    if (!ghost)
      throw std::logic_error("reachable piece has no valid hard drop");
    const uint16_t distance = node.piece.pos.row - ghost->pos.row;
    SpinContext at_lock = distance ? SpinContext{Movement::HardDrop, std::nullopt}
                                   : node.spin;
    SpinType effective_spin =
        scratch._spin_system
            ? scratch._spin_system->detect(scratch._board, *ghost, at_lock)
            : SpinType::None;
    // Different kick/rotation paths may produce the same lock result. Keep
    // one per actual spin outcome, but never merge a mini with a non-spin.
    if (endpoints.insert({ghost->pos.row, ghost->pos.col,
                          static_cast<int>(ghost->piece.rotation()),
                          effective_spin}).second) {
      std::vector<Movement> path;
      for (size_t at = head; queue[at].parent != root; at = queue[at].parent)
        path.push_back(queue[at].move);
      std::reverse(path.begin(), path.end());
      path.push_back(Movement::HardDrop);
      placements.push_back({.moves = std::move(path),
                            .type = ghost->piece.type(),
                            .final_pos = ghost->pos,
                            .final_rotation = ghost->piece.rotation(),
                            .use_hold = use_hold,
                            .drop_distance = distance,
                            .spin_context = node.spin});
    }

    for (Movement move : moves) {
      scratch._active_piece = node.piece;
      scratch._spin_context = node.spin;
      scratch._soft_drop_cells = node.soft_drop_cells;
      scratch._grounded_last_tick = false;
      scratch._lock_timer = scratch._lock_resets_used = 0;
      scratch._last_tick_was_player_action = false;
      if (!execute_move(scratch, move))
        continue;
      SearchKey key = search_key(*scratch._active_piece, scratch._spin_context);
      if (visited.insert(key).second) {
        queue.push_back({*scratch._active_piece, scratch._spin_context,
                         scratch._soft_drop_cells, head, move});
      }
    }
  }
  return placements;
}

LockResult Engine::apply_placement(const Placement &placement) {
  if (_game_over || _edit_mode || !_active_piece)
    throw std::invalid_argument("cannot place without a live active piece");

  // Only the active/hold/RNG and movement metadata can change before the lock.
  // Keep the board untouched until validation succeeds; bad actions roll back
  // without cloning the rules or the board on every placement.
  const auto active = _active_piece;
  const auto held = _hold_piece;
  const bool can_hold_before = _can_hold;
  const auto spin = _spin_context;
  const auto soft_drop = _soft_drop_cells;
  const auto hard_drop_cells_before = _hard_drop_cells;
  const auto gravity_counter = _gravity_counter;
  const auto grounded = _grounded_last_tick;
  const auto timer = _lock_timer;
  const auto resets = _lock_resets_used;
  const auto last_action = _last_tick_was_player_action;
  auto random_before = placement.use_hold ? _randomizer->clone() : nullptr;
  if (random_before)
    _randomizer->restore_from(*random_before); // Reject unsupported custom RNGs first.

  auto rollback = [&] {
    _active_piece = active;
    _hold_piece = held;
    _can_hold = can_hold_before;
    _spin_context = spin;
    _soft_drop_cells = soft_drop;
    _hard_drop_cells = hard_drop_cells_before;
    _gravity_counter = gravity_counter;
    _grounded_last_tick = grounded;
    _lock_timer = timer;
    _lock_resets_used = resets;
    _last_tick_was_player_action = last_action;
    if (random_before)
      _randomizer->restore_from(*random_before);
  };
  try {
    if (placement.use_hold && !hold())
      throw std::invalid_argument("hold is unavailable");
    if (_active_piece->piece.type() != placement.type)
      throw std::invalid_argument("placement piece type does not match");
    if (placement.moves.empty() ||
        placement.moves.back() != Movement::HardDrop)
      throw std::invalid_argument("placement must end with a hard drop");
    for (size_t i = 0; i + 1 < placement.moves.size(); ++i)
      if (!execute_move(*this, placement.moves[i]))
        throw std::invalid_argument("placement has an illegal movement");

    auto ghost = ghost_piece();
    if (!ghost || ghost->pos != placement.final_pos ||
        ghost->piece.rotation() != placement.final_rotation ||
        hard_drop_distance() != placement.drop_distance)
      throw std::invalid_argument("placement final position does not match path");
    if (_spin_context != placement.spin_context)
      throw std::invalid_argument("placement pre-drop context does not match path");
  } catch (...) {
    rollback();
    throw;
  }

  LockResult result = hard_drop();
  if (!result.locked)
    throw std::logic_error("valid placement failed to lock");
  return result;
}

} // namespace tetris
