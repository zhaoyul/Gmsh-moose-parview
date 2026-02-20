#pragma once

#include <memory>

#include "gmp/Runner.h"

namespace gmp {

enum class RunnerKind {
  kLocal,
  kWsl,
  kRemote,
};

std::unique_ptr<Runner> CreateRunner(RunnerKind kind);

}  // namespace gmp
