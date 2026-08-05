#pragma once

#include "svc.hpp"
#include "util.hpp"

namespace nn::diag::detail {

// interestingly, none of these are marked no return based on the codegen
inline void RtldAbort() {
    svc::Break(0, 0, 0);
}

inline void RtldAbort(const char* msg) {
    svc::OutputDebugString(msg, strlen(msg));
    svc::Break(0, 0, 0);
}

inline void Puts(const char* msg) {
    svc::OutputDebugString(msg, strlen(msg));
}

} // namespace nn::diag::detail