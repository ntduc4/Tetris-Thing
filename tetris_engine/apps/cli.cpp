#include "engine/Engine.hpp"
#include "engine/SoloSession.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <poll.h>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>

using namespace tetris;

namespace {
class Terminal {
public:
  Terminal() {
    if (tcgetattr(STDIN_FILENO, &_old) != 0)
      return;
    termios raw = _old;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
      return;
    _active = true;
    std::cout << "\033[?1049h\033[?25l\033[2J" << std::flush;
  }
  ~Terminal() {
    if (_active) {
      std::cout << "\033[?25h\033[?1049l" << std::flush;
      tcsetattr(STDIN_FILENO, TCSANOW, &_old);
    }
  }
  bool active() const { return _active; }

private:
  termios _old{};
  bool _active = false;
};

char label(Cell cell) {
  switch (cell) {
  case Cell::I: return 'I';
  case Cell::O: return 'O';
  case Cell::T: return 'T';
  case Cell::S: return 'S';
  case Cell::Z: return 'Z';
  case Cell::J: return 'J';
  case Cell::L: return 'L';
  case Cell::Garbage: return '#';
  case Cell::Unclearable: return '@';
  default: return ' ';
  }
}

char label(PieceType piece) {
  return label(get_from_piece_type(piece).cell_type());
}

void display(const SoloSession &session, uint16_t cursor_row, uint16_t cursor_col,
             const std::string &message) {
  const Engine &game = session.engine();
  auto cells = game.render();
  auto ghost = game.ghost_piece();
  std::cout << "\033[H";
  const int height = std::min<int>(VisibleHeight, game.board().get_height());
  for (int row = height - 1; row >= 0; --row) {
    std::cout << '|';
    for (uint16_t col = 0; col < game.board().get_width(); ++col) {
      char symbol = label(cells[row][col]);
      if (symbol == ' ' && ghost)
        for (Offset offset : ghost->piece.current_shape())
          if (row == ghost->pos.row + offset.row &&
              col == ghost->pos.col + offset.col)
            symbol = ':';
      if (game.edit_mode() && row == cursor_row && col == cursor_col)
        std::cout << "\033[7m" << symbol << "\033[0m";
      else
        std::cout << symbol;
    }
    std::cout << "|\n";
  }
  std::cout << "hold: " << (game.hold_piece() ? label(*game.hold_piece()) : '-')
            << "  next: ";
  for (auto piece : game.preview_queue())
    std::cout << label(piece);
  std::cout << "\nscore: " << game.score() << "  lines: " << game.lines_cleared()
            << "  tick: " << game.tick_count() << "  incoming: "
            << game.pending_garbage_lines() << "\n";
  if (game.game_over())
    std::cout << "GAME OVER (r restart, q quit)                   \n";
  else if (session.finished())
    std::cout << "GOAL COMPLETE (r restart, q quit)               \n";
  else if (game.edit_mode())
    std::cout << "EDIT: WASD cursor, 0 erase, 1-7 pieces, 8 garbage, 9 solid, e resume\n";
  else
    std::cout << "A/D move, S down, W/X rotate, Z reverse, V 180, C hold, space drop, E edit\n";
  std::cout << message << "                                    \n"
            << "q quit; uppercase A/D to move to wall.\n" << std::flush;
}

bool input(SoloSession &session, char key, uint16_t &row, uint16_t &col,
           std::string &message, uint64_t seed) {
  Engine &game = session.engine();
  if (key == 'q' || key == 3)
    return false;
  if (key == 'r') {
    session.start(seed);
    message.clear();
    return true;
  }
  if (key == 'e') {
    message = game.set_edit_mode(!game.edit_mode())
                  ? "Editor toggled."
                  : "Clear cells under the active piece before resuming.";
    return true;
  }
  if (game.edit_mode()) {
    if (key == 'a' && col > 0) --col;
    if (key == 'd' && col + 1 < game.board().get_width()) ++col;
    if (key == 's' && row > 0) --row;
    if (key == 'w' && row + 1 < game.board().get_height() &&
        row + 1 < VisibleHeight) ++row;
    if (key >= '0' && key <= '9') {
      const Cell palette[] = {Cell::Empty, Cell::I, Cell::O, Cell::T,
                              Cell::S, Cell::Z, Cell::J, Cell::L,
                              Cell::Garbage, Cell::Unclearable};
      game.paint_cell(row, col, palette[key - '0']);
    }
    return true;
  }
  if (session.finished())
    return true;
  switch (key) {
  case 'a': game.try_move_left(); break;
  case 'd': game.try_move_right(); break;
  case 'A': game.try_move_left(-1); break;
  case 'D': game.try_move_right(-1); break;
  case 's': game.try_soft_drop(); break;
  case 'w': case 'x': game.try_rotate_cw(); break;
  case 'z': game.try_rotate_ccw(); break;
  case 'v': game.try_rotate_180(); break;
  case 'c': game.hold(); break;
  case ' ': {
    session.hard_drop();
    break;
  }
  default: break;
  }
  return true;
}

void usage() {
  std::cout << "tetris_cli [--seed N] [--width N] [--height N] [--gravity G] "
               "[--lock-delay ticks] [--spin t|all|none] "
               "[--mode marathon|sprint|blitz] [--goal lines] "
               "[--time seconds] [--ticks N]\n"
               "Runs a 60-tick-per-second terminal game. --ticks runs a headless smoke test.\n";
}
} // namespace

int main(int argc, char **argv) {
  try {
    uint64_t seed = 1;
    uint16_t width = 10, height = 40;
    double gravity = DefaultGravity;
    uint64_t lock_delay = DefaultLockDelay, headless_ticks = 0;
    std::string spin = "all";
    SoloSettings solo;
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--help") { usage(); return 0; }
      if (i + 1 >= argc) { usage(); return 1; }
      std::string value = argv[++i];
      if (arg == "--seed") seed = std::stoull(value);
      else if (arg == "--width") width = static_cast<uint16_t>(std::stoul(value));
      else if (arg == "--height") height = static_cast<uint16_t>(std::stoul(value));
      else if (arg == "--gravity") gravity = std::stod(value);
      else if (arg == "--lock-delay") lock_delay = std::stoull(value);
      else if (arg == "--spin") spin = value;
      else if (arg == "--ticks") headless_ticks = std::stoull(value);
      else if (arg == "--mode") {
        if (value == "marathon") solo.mode = SoloMode::Marathon;
        else if (value == "sprint") solo.mode = SoloMode::Sprint;
        else if (value == "blitz") solo.mode = SoloMode::Blitz;
        else { usage(); return 1; }
      }
      else if (arg == "--goal") solo.line_goal = std::stoul(value);
      else if (arg == "--time") solo.time_limit_ticks =
          static_cast<uint64_t>(std::stod(value) * 60);
      else { usage(); return 1; }
    }
    if (width < 4 || width > 40 || height < 4 || height > 80 ||
        (spin != "t" && spin != "all" && spin != "none")) {
      usage(); return 1;
    }
    std::unique_ptr<SpinSystem> spin_system;
    if (spin == "t") spin_system = std::make_unique<TSpin>();
    if (spin == "all") spin_system = std::make_unique<AllSpin>();
    Engine game(Board(width, height), std::make_unique<SRS>(),
                std::make_unique<SevenBag>(seed), std::move(spin_system),
                std::make_unique<Classical>(), std::make_unique<TetrioSS2>(),
                DefaultQueueLength, (width - 4) / 2,
                std::min<int>(height - 4, VisibleHeight - 3), gravity, 0,
                DefaultGravityMarginTime, lock_delay);
    SoloSession session(std::move(game), solo);
    session.start(seed);
    if (headless_ticks > 0) {
      for (uint64_t i = 0; i < headless_ticks && !session.finished(); ++i)
        session.tick();
      std::cout << "ticks=" << session.elapsed_ticks() << " lines="
                << session.engine().lines_cleared() << " score="
                << session.engine().score() << " game_over="
                << session.engine().game_over() << " finished="
                << session.finished() << '\n';
      return 0;
    }
    if (!isatty(STDIN_FILENO)) { usage(); return 1; }
    Terminal terminal;
    if (!terminal.active()) { std::cerr << "Cannot enter terminal input mode\n"; return 1; }
    uint16_t row = std::min<int>(VisibleHeight, height) - 1, col = width / 2;
    std::string message;
    bool running = true;
    auto next = std::chrono::steady_clock::now();
    while (running) {
      pollfd stdin_poll{STDIN_FILENO, POLLIN, 0};
      while (poll(&stdin_poll, 1, 0) > 0 && (stdin_poll.revents & POLLIN)) {
        char key;
        if (read(STDIN_FILENO, &key, 1) != 1) break;
        running = input(session, key, row, col, message, seed);
        if (!running) break;
      }
      if (!running) break;
      if (!session.finished())
        session.tick();
      display(session, row, col, message);
      next += std::chrono::microseconds(1000000 / 60);
      std::this_thread::sleep_until(next);
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
