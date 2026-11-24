#pragma once

#include "engine/Engine.hpp"

#include <QWidget>

#include <vector>

namespace tetris::qtapp {

/** Paints a small bottom-up opening template independently of a live board. */
class SpinPatternWidget : public QWidget {
public:
  explicit SpinPatternWidget(QWidget *parent = nullptr);
  void resize_pattern(int width, int height);
  void set_pattern(const std::vector<QString> &rows,
                   const std::vector<bool> &filled);
  void set_brush(Cell brush) { _brush = brush; }
  int pattern_width() const { return _width; }
  int pattern_height() const { return static_cast<int>(_rows.size()); }
  const std::vector<QString> &rows() const { return _rows; }
  const std::vector<bool> &filled() const { return _filled; }
  QRect grid_rect() const;
  QRect row_mode_rect(int row) const;
  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

private:
  void paint_at(const QPoint &point, bool erase);
  int _width = 3;
  std::vector<QString> _rows{3, QString(3, '.')};
  std::vector<bool> _filled{true, true, false};
  Cell _brush = Cell::Garbage;
};

} // namespace tetris::qtapp
