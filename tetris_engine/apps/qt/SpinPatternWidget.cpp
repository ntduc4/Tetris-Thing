#include "SpinPatternWidget.hpp"
#include "BoardWidget.hpp"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace tetris::qtapp {
namespace {
constexpr const char *Cells = ".IOTSZJLGX";
}

SpinPatternWidget::SpinPatternWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(200, 180);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setCursor(Qt::CrossCursor);
}

void SpinPatternWidget::resize_pattern(int width, int height) {
  _width = width;
  _rows.resize(height, QString(width, '.'));
  _filled.resize(height, false);
  for (auto &row : _rows)
    row = row.left(width).leftJustified(width, '.');
  update();
}

void SpinPatternWidget::set_pattern(const std::vector<QString> &rows,
                                    const std::vector<bool> &filled) {
  _width = rows.front().size();
  _rows = rows;
  _filled = filled;
  update();
}

QSize SpinPatternWidget::sizeHint() const {
  return QSize(_width * 30 + 65, pattern_height() * 30 + 16);
}

QRect SpinPatternWidget::grid_rect() const {
  const int size = std::clamp(std::min((width() - 65) / _width,
                                       (height() - 16) / pattern_height()), 1, 48);
  const int x = (width() + 45 - _width * size) / 2;
  const int y = (height() - pattern_height() * size) / 2;
  return QRect(x, y, _width * size, pattern_height() * size);
}

QRect SpinPatternWidget::row_mode_rect(int row) const {
  const QRect grid = grid_rect();
  const int size = grid.width() / _width;
  return QRect(grid.x() - 45, grid.y() + (pattern_height() - 1 - row) * size,
               40, size);
}

void SpinPatternWidget::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.fillRect(rect(), QColor("#0d1320"));
  const QRect grid = grid_rect();
  const int size = grid.width() / _width;
  const QColor line("#354159");
  for (int row = 0; row < pattern_height(); ++row) {
    const QRect mode = row_mode_rect(row);
    painter.fillRect(mode.adjusted(0, 1, 0, -1),
                     _filled[row] ? QColor("#674522") : QColor("#253048"));
    painter.setPen(QColor("#f5f7ff"));
    painter.drawText(mode, Qt::AlignCenter, _filled[row] ? "F" : "O");
    for (int col = 0; col < _width; ++col) {
      const int y = grid.y() + (pattern_height() - 1 - row) * size;
      const QRect cell(grid.x() + col * size, y, size, size);
      const int code = QString::fromLatin1(Cells).indexOf(_rows[row][col]);
      painter.fillRect(cell.adjusted(1, 1, -1, -1),
                       cell_color(static_cast<Cell>(code)));
      painter.setPen(line);
      painter.drawRect(cell.adjusted(0, 0, -1, -1));
    }
  }
}

void SpinPatternWidget::paint_at(const QPoint &point, bool erase) {
  const QRect grid = grid_rect();
  if (!grid.contains(point)) {
    for (int row = 0; row < pattern_height(); ++row)
      if (row_mode_rect(row).contains(point)) {
        _filled[row] = !_filled[row];
        update();
        return;
      }
    return;
  }
  const int size = grid.width() / _width;
  const int col = (point.x() - grid.x()) / size;
  const int row = pattern_height() - 1 - (point.y() - grid.y()) / size;
  _rows[row][col] = erase ? '.' : Cells[static_cast<int>(_brush)];
  update();
}

void SpinPatternWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)
    paint_at(event->position().toPoint(), event->button() == Qt::RightButton);
}

void SpinPatternWidget::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() & (Qt::LeftButton | Qt::RightButton)) {
    if (grid_rect().contains(event->position().toPoint()))
      paint_at(event->position().toPoint(), event->buttons() & Qt::RightButton);
  }
}

} // namespace tetris::qtapp
