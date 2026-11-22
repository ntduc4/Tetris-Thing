#pragma once

#include "engine/Engine.hpp"

#include <QColor>
#include <QWidget>

#include <functional>

namespace tetris::qtapp {

QColor cell_color(Cell cell);

class BoardWidget : public QWidget {
public:
  explicit BoardWidget(QWidget *parent = nullptr);
  void set_engine(const Engine *engine);
  void set_editable(bool editable);
  void set_cursor(int row, int col);
  void set_paint_handler(std::function<void(int, int, bool)> handler);
  QSize sizeHint() const override;
  QRect grid_rect() const;
  QRect garbage_meter_rect() const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

private:
  void paint_at(const QPoint &point, bool erase);
  const Engine *_engine = nullptr;
  bool _editable = false;
  int _cursor_row = 0, _cursor_col = 0;
  std::function<void(int, int, bool)> _handler;
};

} // namespace tetris::qtapp
