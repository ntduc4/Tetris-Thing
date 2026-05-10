#include "core/Core.hpp"
#include "core/Piece.hpp"
#include <iostream>
using namespace std;
using namespace tetris;
int main(int argc, char *argv[]) {

  for (auto rotation :
       {Rotation::North, Rotation::East, Rotation::South, Rotation::West}) {
    Piece l = Piece(PieceType::L, Cell::L, rotation);
    auto piece_shape = l.render();
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

  return 0;
}
