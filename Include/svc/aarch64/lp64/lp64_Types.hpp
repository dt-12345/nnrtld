#pragma once

#include "types.hpp"

namespace nn::svc::aarch64::lp64 {

enum MemoryState : std::uint32_t {
    MemoryState_Free             = 0x00,
    MemoryState_Io               = 0x01,
    MemoryState_Static           = 0x02,
    MemoryState_Code             = 0x03,
    MemoryState_CodeData         = 0x04,
    MemoryState_Normal           = 0x05,
    MemoryState_Shared           = 0x06,
    MemoryState_Alias            = 0x07,
    MemoryState_AliasCode        = 0x08,
    MemoryState_AliasCodeData    = 0x09,
    MemoryState_Ipc              = 0x0a,
    MemoryState_Stack            = 0x0b,
    MemoryState_ThreadLocal      = 0x0c,
    MemoryState_Transfered       = 0x0d,
    MemoryState_SharedTransfered = 0x0e,
    MemoryState_SharedCode       = 0x0f,
    MemoryState_Inaccessible     = 0x10,
    MemoryState_NonSecureIpc     = 0x11,
    MemoryState_NonDeviceIpc     = 0x12,
    MemoryState_Kernel           = 0x13,
    MemoryState_GeneratedCode    = 0x14,
    MemoryState_CodeOut          = 0x15,
    MemoryState_Coverage         = 0x16,
    MemoryState_Insecure         = 0x17,
};

enum MemoryAttribute : std::uint32_t {
    MemoryAttribute_Locked              = 1 << 0,
    MemoryAttribute_IpcLocked           = 1 << 1,
    MemoryAttribute_DeviceShared        = 1 << 2,
    MemoryAttribute_Uncached            = 1 << 3,
    MemoryAttribute_PermissionLocked    = 1 << 4,
    MemoryAttribute_GpuSharable         = 1 << 5,
    MemoryAttribute_GpuShared           = 1 << 6,
};

enum MemoryPermission : std::uint32_t {
    MemoryPermission_Read       = 1 << 0,
    MemoryPermission_Write      = 1 << 1,
    MemoryPermission_Execute    = 1 << 2,
    MemoryPermission_DontCare   = 1 << 0x1c,
};

struct MemoryInfo {
    std::uint64_t address;
    std::uint64_t size;
    MemoryState state;
    MemoryAttribute attribute;
    MemoryPermission permission;
    std::uint32_t ipcRefCount;
    std::uint32_t deviceRefCount;
    std::uint32_t padding;
};
static_assert(sizeof(MemoryInfo) == 0x28);

} // namespace nn::svc::aarch64::lp64