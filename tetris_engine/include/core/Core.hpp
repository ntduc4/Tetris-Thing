#pragma once
#include <cstdint>
namespace tetris {

/** Standard board width in cells. */
constexpr uint16_t BoardWidth = 10;
/** Internal board height, including hidden spawn rows. */
constexpr uint16_t BoardHeight = 49;
/** Number of visible playfield rows. */
constexpr uint16_t VisibleHeight = 20;
/** Default spawn column for a piece root position. */
constexpr int16_t DefaultSpawnColOffset = 3;
/** Default spawn row for a piece root position. */
constexpr int16_t DefaultSpawnRowOffset = 19;
/** Default number of pieces exposed in the preview queue. */
constexpr uint8_t DefaultQueueLength = 5;
/** Width and height of a piece's local render grid. */
constexpr uint8_t PieceDimension = 4;

/** Represents the content of one board cell. */
enum class Cell : uint8_t { Empty, I, O, T, S, Z, J, L, Garbage, Unclearable };

/** Represents a piece orientation. */
enum class Rotation : uint8_t { North, East, South, West, Count };

/** Default orientation used when constructing pieces. */
constexpr Rotation DefaultRotation = Rotation::North;

/**
 * Board-space root position for an active piece.
 *
 * Coordinates are signed because a piece root may be outside the board while
 * its occupied cells are still valid.
 */
struct Position {
  int16_t row;
  int16_t col;
};

/** Local row/column offset from a piece root position. */
struct Offset {
  int8_t row;
  int8_t col;
};

/** High-level player action accepted by the engine. */
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

} // namespace tetris
