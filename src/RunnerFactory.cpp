#include "gmp/RunnerFactory.h"

namespace gmp {

std::unique_ptr<Runner> CreateLocalRunner();
std::unique_ptr<Runner> CreateWslRunner();
std::unique_ptr<Runner> CreateRemoteRunner();

std::unique_ptr<Runner> CreateRunner(RunnerKind kind) {
  switch (kind) {
    case RunnerKind::kLocal:
      return CreateLocalRunner();
    case RunnerKind::kWsl:
      return CreateWslRunner();
    case RunnerKind::kRemote:
      return CreateRemoteRunner();
    default:
      return CreateLocalRunner();
  }
}

}  // namespace gmp
