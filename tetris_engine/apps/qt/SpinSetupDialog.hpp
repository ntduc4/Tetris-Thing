#pragma once

#include "SpinPatternWidget.hpp"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace tetris::qtapp {

/** Editor for a seed-transformed, bottom-anchored AI opening template. */
class SpinSetupDialog : public QDialog {
public:
  explicit SpinSetupDialog(QWidget *parent = nullptr);
  bool save(const QString &path, QString *error = nullptr) const;
  bool load(const QString &path, QString *error = nullptr);

private:
  SpinPatternWidget *_pattern;
  QSpinBox *_width;
  QSpinBox *_height;
  QComboBox *_palette;
  QComboBox *_active;
  QComboBox *_hold;
  QLineEdit *_next;
};

} // namespace tetris::qtapp
