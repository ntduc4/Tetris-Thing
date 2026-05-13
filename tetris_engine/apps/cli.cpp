#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include <iostream>
#include <unordered_set>
using namespace std;
using namespace tetris;

char cell_to_char(Cell cell) {
  switch (cell) {
  case Cell::I:
    return 'I';
  case Cell::O:
    return 'O';
  case Cell::T:
    return 'T';
  case Cell::J:
    return 'J';
  case Cell::L:
    return 'L';
  case Cell::S:
    return 'S';
  case Cell::Z:
    return 'Z';
  case Cell::Unclearable:
    return '@';
  case Cell::Garbage:
    return '#';
  default:
    return '.';
  }
}

void printBoard(const Board &board) {
  for (int i = board.getHeight() - 1; i >= 0; i--) {
    for (int j = 0; j < board.getWidth(); j++)
      cout << (board.get(i, j) == Cell::Empty ? "." : "#");
    cout << endl;
  }
}

void printBoardFull(const Board &board, const ActivePiece &piece) {
  unordered_set<int> offsets;
  for (Offset offset : piece.piece.current_shape())
    offsets.insert((piece.pos.row + offset.row) * board.getWidth() +
                   piece.pos.col + offset.col);

  for (int i = board.getHeight() - 1; i >= 0; i--) {
    for (int j = 0; j < board.getWidth(); j++) {
      char boardCell = cell_to_char(board.get(i, j));
      int id = i * board.getWidth() + j;
      if (offsets.find(id) != offsets.end())
        boardCell = cell_to_char(piece.piece.cell_type());
      cout << boardCell;
    }
    cout << endl;
  }
}

void printPieceProfile(const Piece &p) {
  Piece c = p;
  for (auto rotation :
       {Rotation::North, Rotation::East, Rotation::South, Rotation::West}) {
    c.rotate_to(rotation);
    auto piece_shape = c.render();
    for (int i = piece_shape.size() - 1; i >= 0; i--) {
      auto row = piece_shape[i];
      for (Cell cell : row) {
        if (cell != Cell::Empty)
          cout << "#";
        else
          cout << ".";
      }
      cout << endl;
    }
    cout << endl;
  }
}

int main(int argc, char *argv[]) {
  Board board(10, 27);

  board.set(18, 1, Cell::Garbage);
  board.set(18, 2, Cell::Garbage);
  board.set(18, 3, Cell::Garbage);
  board.set(18, 4, Cell::Garbage);
  board.set(18, 5, Cell::Garbage);
  for (int j = 0; j < 8; j++)
    for (int i = 0; i < board.getWidth(); i++)
      board.set(j, i, Cell::Garbage);

  board.set(6, 6, Cell::Empty);
  board.addGarbage(1, 1);

  // ActivePiece p{.piece = get_from_piece_type(PieceType::I),
  //               .pos = Position{.row = DefaultSpawnRowOffset,
  //                               .col = DefaultSpawnColOffset}};
  ActivePiece p = spawn_from_piece_type(PieceType::I);
  printPieceProfile(p.piece);
  p.piece.rotate_cw();

  cout << "Active piece: (row=" << p.pos.row << ", col=" << p.pos.col << ")\n";

  cout << "Max height: " << board.max_height() << endl;
  cout << "Maxed height: " << (board.touch_max_height() ? "True" : "False")
       << endl;
  cout << "Spawnable: " << (board.spawnable(p) ? "True" : "False") << endl;
  printBoardFull(board, p);
  int lineCleared = board.clear_lines();
  cout << "Cleared " << lineCleared << " lines" << endl;
  cout << endl << "After: " << endl;
  cout << "Max height: " << board.max_height() << endl;
  cout << "Maxed height: " << (board.touch_max_height() ? "True" : "False")
       << endl;
  cout << "Spawnable: " << (board.spawnable(p) ? "True" : "False") << endl;
  printBoardFull(board, p);
  board.addGarbage(2, 6);
  cout << endl << "Max height: " << board.max_height() << endl;
  cout << "Maxed height: " << (board.touch_max_height() ? "True" : "False")
       << endl;
  cout << "Spawnable: " << (board.spawnable(p) ? "True" : "False") << endl;
  printBoardFull(board, p);

  return 0;
}
