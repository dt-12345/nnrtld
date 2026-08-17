#include "ro.hpp"
#include "svc.hpp"

namespace nn::ro::detail {

// this is probably some enum
static std::uint32_t IsCodeMemory(const svc::MemoryInfo& info) {
    switch (info.state) {
        case svc::MemoryState_Code: return 1;
        case svc::MemoryState_AliasCode: return 1;
        default: return 0;
    }
}

template <QueryMemoryFunction QueryFunc>
bool FindModuleHeader(const rocrt::ModuleHeader** pOutHeader, rocrt::ModuleVersion* pOutVersion, uintptr_t address) {
    *pOutHeader = nullptr;
    *pOutVersion = {};

    svc::MemoryInfo info{};
    std::uint32_t pageInfo = 0;
    if (QueryFunc(&info, &pageInfo, address)) {
        return false;
    }

    // module should start with code
    if (info.state != svc::MemoryState_AliasCode && info.state != svc::MemoryState_Code) {
        return false;
    }

    // make sure we actually got the right address
    if (info.address != address) {
        return false;
    }

    // code should not be read-only
    if (info.permission != svc::MemoryPermission_Read && (info.permission & svc::MemoryPermission_Execute) == 0) {
        return false;
    }

    std::uint32_t moduleVersion;
    uintptr_t moduleHeaderAddr, rocrtVersionAddr;
    if ((info.permission & svc::MemoryPermission_Read) == 0) {
        do {
            address = info.address + info.size;
            if (QueryFunc(&info, &pageInfo, address)) {
                return false;
            }
        } while (info.permission & svc::MemoryPermission_Execute);
        
        const auto ro = info.permission == svc::MemoryPermission_Read;
        const auto code = IsCodeMemory(info);
        if (!ro || !code) {
            return false;
        }

        moduleVersion = GetRocrtVersion(address);
        moduleHeaderAddr = address + reinterpret_cast<const rocrt::RocrtInit*>(address)->rocrtOffset;
        rocrtVersionAddr = address + reinterpret_cast<const rocrt::RocrtInit*>(address)->rocrtVersionOffset;
    } else {
        const auto rocrt_init = reinterpret_cast<const rocrt::RocrtInit*>(address);
        moduleHeaderAddr = address + rocrt_init->rocrtOffset;
        if (rocrt::HasStoredSdkVersion(rocrt_init)) {
            rocrtVersionAddr = address + rocrt_init->rocrtVersionOffset;
            moduleVersion = 1;
        } else {
            rocrtVersionAddr = 0;
            moduleVersion = 0;
        }
    }

    rocrt::RocrtVersion sdkVersion;
    if (rocrtVersionAddr != 0) {
        sdkVersion = {};
        memcpy(&sdkVersion, reinterpret_cast<const void*>(rocrtVersionAddr), sizeof(sdkVersion));
    } else {
        sdkVersion.major = {};
    }

    const auto major = sdkVersion.major;
    const auto minor = sdkVersion.minor;
    const auto patch = sdkVersion.patch;
    const auto rocrt = moduleVersion ? ((std::int32_t)major > 18 ? 1 : moduleVersion != 1) : 0;

    if (major > 17) {
        if (!util::IsAligned(moduleHeaderAddr, alignof(rocrt::ModuleHeader)) || !util::IsAligned(rocrtVersionAddr, alignof(rocrt::RocrtVersion))) {
            return false;
        }
    }

    const auto pHeader = reinterpret_cast<const rocrt::ModuleHeader*>(moduleHeaderAddr);
    if (pHeader->signature != rocrt::MODULE_HEADER_SIGNATURE) {
        return false;
    }

    *pOutHeader = pHeader;
    pOutVersion->sdkMajor = major;
    pOutVersion->sdkMinor = minor;
    pOutVersion->sdkPatch = patch;
    pOutVersion->rocrtVersion = rocrt;
    return true;
}

template bool FindModuleHeader<svc::QueryMemory>(const rocrt::ModuleHeader** pOutHeader, rocrt::ModuleVersion* pOutVersion, uintptr_t address);

} // namespace nn::ro::detail