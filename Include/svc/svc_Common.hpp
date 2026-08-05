#pragma once

#if defined(__aarch64__)

#include "svc/aarch64/aarch64_Common.hpp"

namespace nn::svc {
using namespace aarch64;
} // namespace nn::svc

#else
    #error "Unsupported architecture"
#endif