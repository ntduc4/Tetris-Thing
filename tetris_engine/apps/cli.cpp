#include "core/Board.hpp"
#include "core/Core.hpp"
#include "core/Piece.hpp"
#include <iostream>
using namespace std;
using namespace tetris;
int main(int argc, char *argv[]) {

  Board board{};

  board.set(1, 1, Cell::Garbage);

  for (int i = board.getHeight() - 1; i >= 0; i--) {
    for (int j = 0; j < board.getWidth(); j++)
      cout << (board.get(i, j) == Cell::Empty ? "." : "#");
    cout << endl;
  }

  // for (auto rotation :
  //      {Rotation::North, Rotation::East, Rotation::South, Rotation::West})
  //      {
  //   Piece s = Piece(PieceType::T, Cell::T, rotation);
  //   auto piece_shape = s.render();
  //   for (int i = piece_shape.size() - 1; i >= 0; i--) {
  //     auto row = piece_shape[i];
  //     for (Cell cell : row) {
  //       if (cell != Cell::Empty)
  //         cout << "#";
  //       else
  //         cout << ".";
  //     }
  //     cout << endl;
  //   }
  //   cout << endl;
  // }

  return 0;
}
