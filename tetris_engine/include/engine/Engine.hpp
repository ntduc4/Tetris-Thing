#pragma once

#include "core/Board.hpp"
#include "core/Rotation.hpp"
#include <memory>
namespace tetris {

class Engine {
public:
  Engine(const Board &board, std::unique_ptr<RotationSystem> &rotationSystem)
      : _board{board}, _rotationSystem{std::move(rotationSystem)} {};

private:
  Board _board;
  std::unique_ptr<RotationSystem> _rotationSystem;
};

} // namespace tetris
