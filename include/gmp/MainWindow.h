#pragma once

#include <QMainWindow>

namespace gmp {

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);
};

}  // namespace gmp
