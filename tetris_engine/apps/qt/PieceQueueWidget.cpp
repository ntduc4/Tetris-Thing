#include "PieceQueueWidget.hpp"

#include "BoardWidget.hpp"

#include <QPainter>

#include <algorithm>
#include <utility>

namespace tetris::qtapp {
namespace {

void paint_slot(QPainter &painter, const QRect &slot,
                std::optional<PieceType> type) {
  constexpr int grid_size = static_cast<int>(PieceDimension);
  painter.fillRect(slot, QColor("#151c2b"));
  painter.setPen(QColor("#354159"));
  painter.drawRect(slot.adjusted(0, 0, -1, -1));

  const int cell = std::max(1, std::min((slot.width() - 10) / grid_size,
                                        (slot.height() - 10) / grid_size));
  const int left = slot.x() + (slot.width() - grid_size * cell) / 2;
  const int top = slot.y() + (slot.height() - grid_size * cell) / 2;
  const auto cells = type ? std::optional{get_from_piece_type(*type).render()}
                          : std::nullopt;
  for (int row = 0; row < grid_size; ++row) {
    for (int col = 0; col < grid_size; ++col) {
      QRect square(left + col * cell, top + (grid_size - 1 - row) * cell,
                   cell, cell);
      painter.fillRect(square, QColor("#1c2637"));
      painter.setPen(QColor("#39445b"));
      painter.drawRect(square.adjusted(0, 0, -1, -1));
      if (cells && (*cells)[row][col] != Cell::Empty) {
        QRect mino = square.adjusted(1, 1, -1, -1);
        painter.fillRect(mino, cell_color((*cells)[row][col]));
        painter.fillRect(QRect(mino.x(), mino.y(), mino.width(), 2),
                         QColor(255, 255, 255, 55));
      }
    }
  }
}

} // namespace

PieceQueueWidget::PieceQueueWidget(QWidget *parent) : QWidget(parent) {
  setFixedWidth(90);
  setMinimumHeight(270);
  setFocusPolicy(Qt::NoFocus);
}

void PieceQueueWidget::set_pieces(std::vector<PieceType> pieces) {
  if (_pieces == pieces)
    return;
  _pieces = std::move(pieces);
  update();
}

QSize PieceQueueWidget::sizeHint() const { return {90, 500}; }

void PieceQueueWidget::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.fillRect(rect(), QColor("#0d1320"));
  painter.setPen(QColor("#c8d4ec"));
  painter.drawText(QRect(0, 1, width(), 22), Qt::AlignCenter, "NEXT");

  const int slot_height = std::min(82, (height() - 28) / 5);
  for (int i = 0; i < 5; ++i) {
    QRect slot(3, 28 + i * slot_height, width() - 6, slot_height - 3);
    paint_slot(painter, slot, i < static_cast<int>(_pieces.size())
                                  ? std::optional{_pieces[i]} : std::nullopt);
  }
}

PieceHoldWidget::PieceHoldWidget(QWidget *parent) : QWidget(parent) {
  setFixedSize(90, 110);
  setFocusPolicy(Qt::NoFocus);
}

void PieceHoldWidget::set_piece(std::optional<PieceType> piece) {
  if (_piece == piece)
    return;
  _piece = piece;
  update();
}

QSize PieceHoldWidget::sizeHint() const { return {90, 110}; }

void PieceHoldWidget::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.fillRect(rect(), QColor("#0d1320"));
  painter.setPen(QColor("#c8d4ec"));
  painter.drawText(QRect(0, 1, width(), 22), Qt::AlignCenter, "HOLD");
  paint_slot(painter, QRect(3, 28, width() - 6, height() - 31), _piece);
}

} // namespace tetris::qtapp
