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

extern "C" void* memcpy(void* __restrict dst, const void* src, size_t size);
extern "C" void* memset(void* dst, std::int32_t value, size_t size);
extern "C" std::int32_t strcmp(const char* s1, const char* s2);
extern "C" size_t strlen(const char* s);

#define NN_ASSERT(EXPR) do { if (!(EXPR)) { while (true) { __asm__ __volatile__("" ::: "memory"); } } } while (0)