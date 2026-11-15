#include "BoardWidget.hpp"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace tetris::qtapp {

QColor cell_color(Cell cell) {
  switch (cell) {
  case Cell::I: return QColor("#28c9d9"); // cyan
  case Cell::O: return QColor("#f2d64b"); // yellow
  case Cell::T: return QColor("#a86ada"); // purple
  case Cell::S: return QColor("#59c66b"); // green
  case Cell::Z: return QColor("#e75d68"); // red
  case Cell::J: return QColor("#5875dc"); // blue
  case Cell::L: return QColor("#f3a44c"); // orange
  case Cell::Garbage: return QColor("#818b9c");
  case Cell::Unclearable: return QColor("#394150");
  default: return QColor("#151c2b");
  }
}

BoardWidget::BoardWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(150, 270);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMouseTracking(true);
  setFocusPolicy(Qt::NoFocus);
}

void BoardWidget::set_engine(const Engine *engine) {
  _engine = engine;
  update();
}

void BoardWidget::set_editable(bool editable) {
  _editable = editable;
  setCursor(editable ? Qt::CrossCursor : Qt::ArrowCursor);
  update();
}

void BoardWidget::set_cursor(int row, int col) {
  _cursor_row = row;
  _cursor_col = col;
  update();
}

void BoardWidget::set_paint_handler(std::function<void(int, int, bool)> handler) {
  _handler = std::move(handler);
}

QSize BoardWidget::sizeHint() const {
  int width = _engine ? _engine->board().get_width() : 10;
  int height = _engine ? std::min<int>(VisibleHeight, _engine->board().get_height()) : 20;
  return QSize(width * 25 + 18, height * 25 + 18);
}

QRect BoardWidget::grid_rect() const {
  if (!_engine)
    return {};
  int width = _engine->board().get_width();
  int height = std::min<int>(VisibleHeight, _engine->board().get_height());
  int size = std::max(1, std::min((this->width() - 16) / width,
                                   (this->height() - 16) / height));
  return QRect((this->width() - size * width) / 2,
               (this->height() - size * height) / 2,
               size * width, size * height);
}

void BoardWidget::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.fillRect(rect(), QColor("#0d1320"));
  if (!_engine)
    return;
  auto playfield = grid_rect();
  const int columns = _engine->board().get_width();
  const int rows = std::min<int>(VisibleHeight, _engine->board().get_height());
  const int size = playfield.width() / columns;
  painter.fillRect(playfield, QColor("#253048"));
  auto cells = _engine->render();
  auto ghost = _engine->ghost_piece();
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < columns; ++col) {
      Cell cell = cells[row][col];
      QRect square(playfield.x() + col * size + 1,
                   playfield.y() + (rows - 1 - row) * size + 1,
                   size - 2, size - 2);
      painter.fillRect(square, cell_color(cell));
      if (cell == Cell::Empty && ghost) {
        for (auto offset : ghost->piece.current_shape()) {
          if (row == ghost->pos.row + offset.row &&
              col == ghost->pos.col + offset.col) {
            QColor tint = cell_color(ghost->piece.cell_type());
            tint.setAlpha(130);
            painter.setPen(QPen(tint, 2));
            painter.drawRect(square.adjusted(2, 2, -2, -2));
            break;
          }
        }
      }
      if (cell != Cell::Empty) {
        QColor shine = QColor(255, 255, 255, cell == Cell::Unclearable ? 20 : 55);
        painter.fillRect(QRect(square.x(), square.y(), square.width(), 2), shine);
      }
    }
  }
  if (_editable) {
    painter.setPen(QPen(QColor("#f5f7ff"), 2));
    painter.drawRect(playfield.adjusted(-2, -2, 1, 1));
    QRect cursor(playfield.x() + _cursor_col * size,
                 playfield.y() + (rows - 1 - _cursor_row) * size, size, size);
    painter.drawRect(cursor.adjusted(1, 1, -1, -1));
  }
}

void BoardWidget::paint_at(const QPoint &point, bool erase) {
  if (!_engine || !_editable || !_handler)
    return;
  QRect field = grid_rect();
  if (!field.contains(point))
    return;
  int size = field.width() / _engine->board().get_width();
  int col = (point.x() - field.x()) / size;
  int row = std::min<int>(VisibleHeight, _engine->board().get_height()) - 1 -
            (point.y() - field.y()) / size;
  _handler(row, col, erase);
  update();
}

void BoardWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)
    paint_at(event->position().toPoint(), event->button() == Qt::RightButton);
}

void BoardWidget::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() & (Qt::LeftButton | Qt::RightButton))
    paint_at(event->position().toPoint(), event->buttons() & Qt::RightButton);
}

} // namespace tetris::qtapp
