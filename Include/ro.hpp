#pragma once

#include "ro/detail/detail_RoModule.hpp"
#include "rocrt.hpp"
#include "svc.hpp"

namespace nn::ro::detail {

using LookupGlobalManualFunc = uintptr_t (const RoModule*, const char*);
using RoModuleList = util::IntrusiveList<RoModule, RoModule::GetListNodeOffset()>;
using QueryMemoryFunction = std::uint32_t (svc::MemoryInfo* info, std::uint32_t* page_info, std::uint64_t addr);
using StartCallback = void (void);

extern bool g_RoDebugFlag;
extern LookupGlobalManualFunc* g_LookupGlobalManualFunctionPointer;
extern util::TypedStorage<RoModuleList> g_ManualLoadList;
extern util::TypedStorage<RoModuleList> g_AutoLoadList;

void Initialize(uintptr_t aslr_base, Elf64_Dyn* dyn);

void Abort(const char* msg);

template <QueryMemoryFunction QueryFunc>
bool FindModuleHeader(const rocrt::ModuleHeader** out_header, rocrt::ModuleVersion* out_version, uintptr_t address);

uintptr_t GetSymbolByName(const char* name);

void InitializeModules();
void FinalizeModules();

StartCallback* GetSetUserExceptionHandlerReady();
StartCallback* GetInitializeModules();
StartCallback* GetFinalizeModules();

std::uint32_t GetRocrtVersion(uintptr_t address);

} // namespace nn::ro::detail