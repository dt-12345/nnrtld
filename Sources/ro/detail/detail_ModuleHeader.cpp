#include "ro.hpp"
#include "svc.hpp"

namespace nn::ro::detail {

template <QueryMemoryFunction QueryFunc>
bool FindModuleHeader(const rocrt::ModuleHeader** pOutHeader, rocrt::ModuleVersion* pOutVersion, uintptr_t address) {
    *pOutHeader = nullptr;
    *pOutVersion = {};

    std::uint32_t pageInfo = 0;
    svc::MemoryInfo info{};
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
    rocrt::RocrtVersion sdkVersion;
    
    if ((info.permission & svc::MemoryPermission_Read) == 0) {
        uintptr_t startAddr;
        do {
            startAddr = info.address + info.size;
            if (QueryFunc(&info, &pageInfo, startAddr)) {
                return false;
            }
        } while (info.permission & svc::MemoryPermission_Execute);
        
        if (info.permission != svc::MemoryPermission_Read) {
            return false;
        }

        if (info.state != svc::MemoryState_Code && info.state != svc::MemoryState_AliasCode) {
            return false;
        }

        moduleVersion = GetRocrtVersion(startAddr);
        moduleHeaderAddr = startAddr + reinterpret_cast<const rocrt::RocrtInit*>(startAddr)->rocrtOffset;
        rocrtVersionAddr = startAddr + reinterpret_cast<const rocrt::RocrtInit*>(startAddr)->rocrtVersionOffset;
        if (rocrtVersionAddr != 0) {
            sdkVersion = {};
            memcpy(&sdkVersion, reinterpret_cast<const void*>(rocrtVersionAddr), sizeof(sdkVersion));
        } else {
            sdkVersion = {};
        }
    } else {
        auto rocrt_init = reinterpret_cast<const rocrt::RocrtInit*>(address);
        moduleHeaderAddr = address + rocrt_init->rocrtOffset;
        if (rocrt::HasStoredSdkVersion(rocrt_init)) {
            rocrtVersionAddr = address + rocrt_init->rocrtVersionOffset;
            if (rocrtVersionAddr != 0) {
                sdkVersion = {};
                memcpy(&sdkVersion, reinterpret_cast<const void*>(rocrtVersionAddr), sizeof(sdkVersion));
            } else {
                sdkVersion = {};
            }
            moduleVersion = 1;
        } else {
            rocrtVersionAddr = 0;
            sdkVersion = {};
            moduleVersion = 0;
        }
    }

    const std::uint32_t rocrt_version = moduleVersion != 0 || sdkVersion.major > 18;

    if (sdkVersion.major >= 18
        && (!util::IsAligned(moduleHeaderAddr, alignof(rocrt::ModuleHeader)) || !util::IsAligned(rocrtVersionAddr, alignof(rocrt::RocrtVersion)))
    ) {
        return false;
    }

    if (reinterpret_cast<const rocrt::ModuleHeader*>(moduleHeaderAddr)->signature != rocrt::MODULE_HEADER_SIGNATURE) {
        return false;
    }

    *pOutHeader = reinterpret_cast<const rocrt::ModuleHeader*>(moduleHeaderAddr);
    pOutVersion->sdkMajor = sdkVersion.major;
    pOutVersion->sdkMinor = sdkVersion.minor;
    pOutVersion->sdkPatch = sdkVersion.patch;
    pOutVersion->rocrtVersion = rocrt_version;

    return true;
}

template bool FindModuleHeader<svc::QueryMemory>(const rocrt::ModuleHeader** pOutHeader, rocrt::ModuleVersion* pOutVersion, uintptr_t address);

} // namespace nn::ro::detail