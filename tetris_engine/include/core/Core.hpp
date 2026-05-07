#pragma once
#include <cstdint>
namespace tetris {

constexpr uint16_t BoardWidth = 10;
constexpr uint16_t BoardHeight = 49;
constexpr uint16_t VisibleHeight = 20;
constexpr uint8_t DefaultQueueLength = 5;
constexpr uint8_t DefaultSpawnColOffset = 19;
constexpr uint8_t DefaultSpawnRowOffset = 3;
constexpr uint8_t PieceDimension = 4;

enum class Cell : uint8_t { Empty, I, O, T, S, Z, J, L, Garbage, Unclearable };

enum class Rotation : uint8_t { North, East, South, West, Count };

constexpr Rotation DefaultRotation = Rotation::North;

struct Position {
  int16_t row;
  int16_t col;
};

struct Offset {
  int8_t row;
  int8_t col;
};

enum class Action {
  Left,
  Right,
  SoftDrop,
  HardDrop,
  RotateCW,
  RotateCCW,
  Rotate180,
  Hold
};

struct Placement {
  Position pos;
  Rotation rotation;
};

} // namespace tetris
