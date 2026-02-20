#include "ProcessRunner.h"

namespace gmp {

class WslRunner final : public ProcessRunner {
 public:
  using ProcessRunner::ProcessRunner;

  void start(const RunSpec& spec) override {
    RunSpec wsl_spec;
    wsl_spec.program = "wsl";
    wsl_spec.args = QStringList{"--", spec.program};
    wsl_spec.args.append(spec.args);
    wsl_spec.working_dir = spec.working_dir;
    wsl_spec.env = spec.env;
    start_process(wsl_spec);
  }
};

std::unique_ptr<Runner> CreateWslRunner() {
  return std::make_unique<WslRunner>();
}

}  // namespace gmp
