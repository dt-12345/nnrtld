#pragma once

#include "svc.hpp"
#include "util.hpp"

namespace nn::diag::detail {

inline void Abort() {
    svc::Break(0, 0, 0);
}

inline void Puts(const char* msg) {
    svc::OutputDebugString(msg, strlen(msg));
}

} // namespace nn::diag::detail