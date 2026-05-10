#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include <iostream>
using namespace std;
using namespace tetris;

void printBoard(const Board &board) {
  for (int i = board.getHeight() - 1; i >= 0; i--) {
    for (int j = 0; j < board.getWidth(); j++)
      cout << (board.get(i, j) == Cell::Empty ? "." : "#");
    cout << endl;
  }
}

void printPieceProfile(const Piece &p) {
  for (auto rotation :
       {Rotation::North, Rotation::East, Rotation::South, Rotation::West}) {
    auto piece_shape = p.render();
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
  Board board(10, 13);

  board.set(12, 1, Cell::Garbage);
  for (int j = 0; j < 8; j++)
    for (int i = 0; i < board.getWidth(); i++)
      board.set(j, i, Cell::Garbage);

  board.set(6, 6, Cell::Empty);

  cout << "Max height: " << board.max_height() << endl;
  cout << "Maxed height: " << (board.touch_max_height() ? "True" : "False")
       << endl;
  printBoard(board);
  int lineCleared = board.clear_lines();
  cout << "Cleared " << lineCleared << " lines" << endl;
  cout << endl << "After: " << endl;
  cout << "Max height: " << board.max_height() << endl;
  cout << "Maxed height: " << (board.touch_max_height() ? "True" : "False")
       << endl;
  printBoard(board);
  board.addGarbage(2, 6);
  cout << endl << "Max height: " << board.max_height() << endl;
  cout << "Maxed height: " << (board.touch_max_height() ? "True" : "False")
       << endl;
  printBoard(board);

  return 0;
}
