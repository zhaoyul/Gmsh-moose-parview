#pragma once

#include <QObject>
#include <QProcess>

#include "gmp/RunSpec.h"

namespace gmp {

class Runner : public QObject {
  Q_OBJECT
 public:
  explicit Runner(QObject* parent = nullptr) : QObject(parent) {}
  ~Runner() override = default;

  virtual void start(const RunSpec& spec) = 0;
  virtual void stop() = 0;

 signals:
  void started();
  void finished(int exit_code, QProcess::ExitStatus exit_status);
  void std_out(const QString& text);
  void std_err(const QString& text);
};

}  // namespace gmp
