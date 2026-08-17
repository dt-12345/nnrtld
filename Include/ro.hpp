#pragma once

#include "ro/detail/detail_RoModule.hpp"
#include "rocrt.hpp"
#include "svc.hpp"

namespace nn::ro {

void BindEntry();

namespace detail {

using LookupGlobalManualFunc = uintptr_t (const RoModule*, const char*);
using RoModuleList = util::IntrusiveList<RoModule, RoModule::GetListNodeOffset()>;

constexpr inline std::size_t cSegmentAlignment = 0x1000; 

extern bool g_RoDebugFlag;
extern LookupGlobalManualFunc* g_LookupGlobalManualFunctionPointer;
extern util::TypedStorage<RoModuleList> g_ManualLoadList;
extern util::TypedStorage<RoModuleList> g_AutoLoadList;

void InitializeSelfModule(uintptr_t moduleBase, Elf64_Dyn* pDyn);
void Initialize(uintptr_t moduleBase, Elf64_Dyn* pDyn);

using QueryMemoryFunction = std::uint32_t (svc::MemoryInfo* pOutMemInfo, std::uint32_t* pOutPageInfo, std::uint64_t addr);
template <QueryMemoryFunction QueryFunc>
bool FindModuleHeader(const rocrt::ModuleHeader** pOutHeader, rocrt::ModuleVersion* pOutVersion, uintptr_t address);

template <QueryMemoryFunction QueryFunc>
static svc::MemoryInfo GetNextRegion(uintptr_t baseAddr) {
    std::uint32_t pageInfo = 0u;
    svc::MemoryInfo memoryInfo{};
    NN_ASSERT(QueryFunc(&memoryInfo, &pageInfo, baseAddr) == 0);
    return memoryInfo;
}

template <QueryMemoryFunction QueryFunc, typename CallbackT>
static void ForEachRegion(uintptr_t addr, CallbackT func) {
    while (true) {
        svc::MemoryInfo memoryInfo = GetNextRegion<QueryFunc>(addr);

        func(memoryInfo);

        const auto lastAddr = addr;
        addr = memoryInfo.address + memoryInfo.size;

        if (addr <= lastAddr) {
            break;
        }
    }
}

uintptr_t LookupGlobalAuto(const char* name);

using StartCallback = void (void);
StartCallback* GetSetUserExceptionHandlerReady();
StartCallback* GetInitializeModules();
StartCallback* GetFinalizeModules();

void Puts(const char* msg);
__attribute__((__noreturn__)) void Unexpected(const char* msg);

std::uint32_t GetRocrtVersion(uintptr_t address);

} // namespace detail

} // namespace nn::ro