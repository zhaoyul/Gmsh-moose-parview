#include "ProcessRunner.h"

namespace gmp {

class LocalRunner final : public ProcessRunner {
 public:
  using ProcessRunner::ProcessRunner;

  void start(const RunSpec& spec) override {
    start_process(spec);
  }
};

std::unique_ptr<Runner> CreateLocalRunner() {
  return std::make_unique<LocalRunner>();
}

}  // namespace gmp
