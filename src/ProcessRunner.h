#pragma once

#include <QProcess>

#include "gmp/Runner.h"

namespace gmp {

class ProcessRunner : public Runner {
  Q_OBJECT
 public:
  explicit ProcessRunner(QObject* parent = nullptr);

  void stop() override;

 protected:
  void start_process(const RunSpec& spec);

  QProcess proc_;
};

}  // namespace gmp
