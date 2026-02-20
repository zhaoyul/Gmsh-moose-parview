#include "ProcessRunner.h"

namespace gmp {

class RemoteRunner final : public ProcessRunner {
 public:
  using ProcessRunner::ProcessRunner;

  void start(const RunSpec& spec) override {
    // Placeholder: remote execution via SSH will be added in Phase 2+.
    // For now, fallback to local execution to keep the pipeline runnable.
    start_process(spec);
  }
};

std::unique_ptr<Runner> CreateRemoteRunner() {
  return std::make_unique<RemoteRunner>();
}

}  // namespace gmp
