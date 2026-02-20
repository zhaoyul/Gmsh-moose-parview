#include "ProcessRunner.h"

namespace gmp {

ProcessRunner::ProcessRunner(QObject* parent) : Runner(parent) {
  connect(&proc_, &QProcess::started, this, &Runner::started);
  connect(&proc_, &QProcess::readyReadStandardOutput, this, [this]() {
    emit std_out(QString::fromUtf8(proc_.readAllStandardOutput()));
  });
  connect(&proc_, &QProcess::readyReadStandardError, this, [this]() {
    emit std_err(QString::fromUtf8(proc_.readAllStandardError()));
  });
  connect(&proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, &Runner::finished);
}

void ProcessRunner::start_process(const RunSpec& spec) {
  proc_.setProgram(spec.program);
  proc_.setArguments(spec.args);
  if (!spec.working_dir.isEmpty()) {
    proc_.setWorkingDirectory(spec.working_dir);
  }
  if (!spec.env.isEmpty()) {
    proc_.setProcessEnvironment(spec.env);
  }
  proc_.start();
}

void ProcessRunner::stop() {
  if (proc_.state() == QProcess::NotRunning) {
    return;
  }
  proc_.kill();
}

}  // namespace gmp
