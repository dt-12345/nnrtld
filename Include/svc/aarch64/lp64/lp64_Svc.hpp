#pragma once

#include "svc/aarch64/lp64/lp64_Types.hpp"

namespace nn::svc::aarch64::lp64 {

std::uint32_t SetMemoryPermission(const void* addr, std::uint64_t size, std::uint32_t perm);
std::uint32_t SetMemoryAttribute(const void* addr, std::uint64_t size, std::uint32_t mask, std::uint32_t value);
std::uint32_t QueryMemory(MemoryInfo* info, std::uint32_t* page_info, std::uint64_t addr);
std::uint32_t Break(std::uint32_t breakReason, uintptr_t address, uintptr_t size);
std::uint32_t OutputDebugString(const char *str, std::uint64_t size);
void ReturnFromException(std::uint32_t result);

} // namespace nn::svc::aarch64::lp64