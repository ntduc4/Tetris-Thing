#pragma once

#include "core/Piece.hpp"

#include <QWidget>

#include <optional>
#include <vector>

namespace tetris::qtapp {

/** Five upcoming tetrominoes in their full 4x4 spawn grids. */
class PieceQueueWidget : public QWidget {
public:
  explicit PieceQueueWidget(QWidget *parent = nullptr);
  void set_pieces(std::vector<PieceType> pieces);
  const std::vector<PieceType> &pieces() const { return _pieces; }
  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  std::vector<PieceType> _pieces;
};

/** The held tetromino, or an empty 4x4 slot before the first hold. */
class PieceHoldWidget : public QWidget {
public:
  explicit PieceHoldWidget(QWidget *parent = nullptr);
  void set_piece(std::optional<PieceType> piece);
  std::optional<PieceType> piece() const { return _piece; }
  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  std::optional<PieceType> _piece;
};

} // namespace tetris::qtapp
