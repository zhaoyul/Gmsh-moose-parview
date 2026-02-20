#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace gmp {

struct RunSpec {
  QString program;
  QStringList args;
  QString working_dir;
  QProcessEnvironment env;
};

}  // namespace gmp
