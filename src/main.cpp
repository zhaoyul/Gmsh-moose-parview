#include <QApplication>
#include <QString>

#ifdef GMP_ENABLE_VTK_VIEWER
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#endif

#include "gmp/MainWindow.h"

int main(int argc, char** argv) {
#ifdef GMP_ENABLE_VTK_VIEWER
  QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
#endif
  QApplication app(argc, argv);

  gmp::MainWindow window;
  window.show();
  return app.exec();
}
