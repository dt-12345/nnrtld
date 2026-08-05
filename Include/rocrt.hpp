#pragma once

#include "types.hpp"
#include "elf.h"

namespace nn::rocrt {

constexpr inline std::uint32_t MODULE_HEADER_SIGNATURE = 0x30444f4d; // MOD0
constexpr inline std::uint32_t ARM_ENTRYPOINT_BRANCH = 0xea000000;
constexpr inline std::uint32_t AARCH64_ENTRYPOINT_BRANCH = 0x14000002;

struct ModuleHeader {
    std::uint32_t signature;
    std::uint32_t dynamic_offset;
    std::uint32_t bss_start_offset;
    std::uint32_t bss_end_offset;
    std::uint32_t eh_frame_hdr_start_offset;
    std::uint32_t eh_frame_hdr_end_offset;
    std::uint32_t ro_module_offset;
    std::uint32_t relro_start_offset;
    std::uint32_t full_relro_end_offset;
    std::uint32_t nx_debuglink_start_offset;
    std::uint32_t nx_debuglink_end_offset;
    std::uint32_t note_gnu_build_id_start_offset;
    std::uint32_t note_gnu_build_id_end_offset;
};

struct ModuleVersion {
    std::uint32_t sdk_major;
    std::uint32_t sdk_minor;
    std::uint32_t sdk_patch;
    std::uint32_t rocrt_version;
};

struct RocrtInit {
    std::uint32_t entry_instruction;
    std::uint32_t rocrt_offset;
    std::uint32_t rocrt_version_offset;
};

struct RocrtVersion {
    std::uint32_t major;
    std::uint32_t minor;
    std::uint32_t patch;
};

inline bool HasStoredSdkVersion(const rocrt::RocrtInit* init) {
    const auto entry_instr = init->entry_instruction;
    return entry_instr == rocrt::ARM_ENTRYPOINT_BRANCH || entry_instr == 0 || entry_instr == rocrt::AARCH64_ENTRYPOINT_BRANCH;
}

namespace detail {

void Initialize(uintptr_t aslr_base, Elf64_Dyn* dyn);

} // namespace detail

} // namespace nn::rocrt