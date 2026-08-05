#pragma once

#include "util/util_IntrusiveList.hpp"
#include "util/util_TypedStorage.hpp"

namespace nn::util {

template <typename T> requires std::is_integral_v<T>
constexpr inline bool IsAligned(T value, T align) {
    return value % align == 0;
}

template <typename T> requires std::is_integral_v<T>
constexpr inline T AlignUp(T value, T align) {
    return (value + align - 1) / align * align;
}

} // namespace nn::util