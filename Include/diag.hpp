#pragma once

#include "svc.hpp"
#include "util.hpp"

namespace nn::diag::detail {

[[noreturn]] ALWAYS_INLINE void Abort() {
    svc::Break(0, 0, 0);
    __builtin_unreachable();
}

ALWAYS_INLINE void Puts(const char* msg) {
    svc::OutputDebugString(msg, strlen(msg));
}

} // namespace nn::diag::detail