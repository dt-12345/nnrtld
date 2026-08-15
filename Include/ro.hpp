#pragma once

#include "ro/detail/detail_RoModule.hpp"
#include "rocrt.hpp"
#include "svc.hpp"

namespace nn::ro {

void BindEntry();

namespace detail {

using LookupGlobalManualFunc = uintptr_t (const RoModule*, const char*);
using RoModuleList = util::IntrusiveList<RoModule, RoModule::GetListNodeOffset()>;

extern bool g_RoDebugFlag;
extern LookupGlobalManualFunc* g_LookupGlobalManualFunctionPointer;
extern util::TypedStorage<RoModuleList> g_ManualLoadList;
extern util::TypedStorage<RoModuleList> g_AutoLoadList;

void InitializeSelfModule(uintptr_t moduleBase, Elf64_Dyn* pDyn);
void Initialize(uintptr_t moduleBase, Elf64_Dyn* pDyn);

using QueryMemoryFunction = std::uint32_t (svc::MemoryInfo* pOutMemInfo, std::uint32_t* pOutPageInfo, std::uint64_t addr);
template <QueryMemoryFunction QueryFunc>
bool FindModuleHeader(const rocrt::ModuleHeader** pOutHeader, rocrt::ModuleVersion* pOutVersion, uintptr_t address);

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