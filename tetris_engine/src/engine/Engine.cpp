#include "core/Core.hpp"
#include "core/Piece.hpp"

#include "engine/Engine.hpp"
#include "engine/Movement.hpp"
#include "engine/Spin.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace tetris {

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
    for (Offset offset : _active_piece->piece.current_shape()) {
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
  for (Offset o : p.piece.current_shape())
    _board.set(o.row + p.pos.row, o.col + p.pos.col, p.piece.cell_type());

  _active_piece = std::nullopt;
  uint16_t lc = _board.clear_lines();
  bool perfect_clear = lc > 0 && _board.max_height() == 0;
  if (lc != 0) {
    _combo = _last_lock_result && _last_lock_result->lines_cleared ? _combo + 1 : 0;
  } else {
    _combo = 0;
  }
  bool difficult = lc > 0 && (spin != SpinType::None || lc >= 4);
  bool back_to_back_bonus = difficult && _back_to_back;
  if (lc > 0)
    _back_to_back = difficult;

  ScoreEvent score_event{.lines_cleared = lc,
                         .spin = spin,
                         .perfect_clear = perfect_clear,
                         .back_to_back = back_to_back_bonus,
                         .combo = _combo,
                         .hard_drop_cells = _hard_drop_cells,
                         .soft_drop_cells = static_cast<uint16_t>(std::min<uint32_t>(
                             _soft_drop_cells, UINT16_MAX))};
  if (_score_system)
    _score += _score_system->score(score_event);
  uint16_t outgoing = 0;
  if (_attack_scheme)
    outgoing = _attack_scheme->garbage_to_send(
        AttackContext{.lines_cleared = lc,
                      .spin = spin,
                      .perfect_clear = perfect_clear,
                      .back_to_back = back_to_back_bonus,
                      .combo = _combo});
  // Full blocking: cancel oldest incoming lines before anything is sent.
  for (auto &packet : _pending_garbage) {
    uint16_t cancelled = std::min(outgoing, packet.event.lines);
    packet.event.lines -= cancelled;
    outgoing -= cancelled;
  }
  _pending_garbage.erase(
      std::remove_if(_pending_garbage.begin(), _pending_garbage.end(),
                     [](const PendingGarbage &packet) { return packet.event.lines == 0; }),
      _pending_garbage.end());
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
          .locked = true};
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
  if (!_active_piece && _garbage_queue_delay == 0)
    apply_ready_garbage();
}

void Engine::apply_ready_garbage() {
  while (!_pending_garbage.empty() &&
         _pending_garbage.front().delay_ticks == 0) {
    const auto packet = _pending_garbage.front();
    _pending_garbage.pop_front();
    _board.add_garbage(packet.event.hole_col, packet.event.lines);
    if (_board.touch_max_height()) {
      _game_over = true;
      break;
    }
  }
}

std::vector<Placement> Engine::generate_active_piece_placement() const {
  return {};
}

std::vector<Placement> Engine::generate_hold_piece_placement() const {
  return {};
}

} // namespace tetris
