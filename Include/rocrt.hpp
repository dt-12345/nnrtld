#pragma once

#include "types.hpp"
#include "elf.h"

namespace nn::rocrt {

constexpr inline std::uint32_t MODULE_HEADER_SIGNATURE = 0x30444f4d; // MOD0
constexpr inline std::uint8_t NN_SDK_VERSION_MAJOR = 0x15;
constexpr inline std::uint8_t NN_SDK_VERSION_MINOR = 0x4;
constexpr inline std::uint8_t NN_SDK_VERSION_MICRO = 0x0;

constexpr inline std::uint32_t ARM_ENTRYPOINT_BRANCH = 0xea000000;
constexpr inline std::uint32_t AARCH64_ENTRYPOINT_BRANCH = 0x14000002;

struct ModuleHeader {
    std::uint32_t signature;
    std::int32_t  dynamicOffset;
    std::int32_t  bssStartOffset;
    std::int32_t  bssEndOffset;
    std::int32_t  ehFrameHdrStartOffset;
    std::int32_t  ehFrameHdrEndOffset;
    std::int32_t  roMduleOffset;
    std::int32_t  relroStartOffset;
    std::int32_t  fullRelroEndOffset;
    std::int32_t  nxDebuglinkStartOffset;
    std::int32_t  nxDebuglinkEndOffset;
    std::int32_t  noteGnuBuildIdStartOffset;
    std::int32_t  noteGnuBuildIdEndOffset;
};

struct ModuleVersion {
    std::uint32_t sdkMajor;
    std::uint32_t sdkMinor;
    std::uint32_t sdkPatch;
    std::uint32_t rocrtVersion;
};

struct RocrtInit {
    std::uint32_t entryInstruction;
    std::int32_t  rocrtOffset;
    std::int32_t  rocrtVersionOffset;
};

struct RocrtVersion {
    std::uint32_t major;
    std::uint32_t minor;
    std::uint32_t patch;
};

inline bool HasStoredSdkVersion(const rocrt::RocrtInit* init) {
    const auto instr = init->entryInstruction;
    return instr == rocrt::ARM_ENTRYPOINT_BRANCH || instr == 0 || instr == rocrt::AARCH64_ENTRYPOINT_BRANCH;
}

} // namespace nn::rocrt