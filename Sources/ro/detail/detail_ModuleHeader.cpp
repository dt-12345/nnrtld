#include "ro.hpp"
#include "svc.hpp"

namespace nn::ro::detail {

template <QueryMemoryFunction QueryFunc>
bool FindModuleHeader(const rocrt::ModuleHeader** out_header, rocrt::ModuleVersion* out_version, uintptr_t address) {
    *out_header = nullptr;
    *out_version = {};

    rocrt::RocrtVersion sdk_version;
    std::uint32_t page_info;
    svc::MemoryInfo info{};
    if (QueryFunc(&info, &page_info, address)) {
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

    std::uint32_t module_version;
    uintptr_t module_header_addr, rocrt_version_addr;
    
    if ((info.permission & svc::MemoryPermission_Read) == 0) {
        uintptr_t start_address;
        do {
            start_address = info.address + info.size;
            if (QueryFunc(&info, &page_info, start_address)) {
                return false;
            }
        } while (info.permission & svc::MemoryPermission_Execute);
        
        if (info.permission != svc::MemoryPermission_Read) {
            return false;
        }

        if (info.state != svc::MemoryState_Code && info.state != svc::MemoryState_AliasCode) {
            return false;
        }

        module_version = GetRocrtVersion(start_address);
        const auto version_offset = reinterpret_cast<const rocrt::RocrtInit*>(start_address)->rocrt_version_offset;
        module_header_addr = start_address + reinterpret_cast<const rocrt::RocrtInit*>(start_address)->rocrt_offset;

        rocrt_version_addr = start_address + version_offset;
        if (rocrt_version_addr == 0) {
            sdk_version = {};
            memcpy(&sdk_version, reinterpret_cast<const void*>(rocrt_version_addr), sizeof(sdk_version));
        } else {
            sdk_version = {};
        }
    } else {
        auto rocrt_init = reinterpret_cast<const rocrt::RocrtInit*>(address);
        module_header_addr = address + rocrt_init->rocrt_offset;
        if (rocrt::HasStoredSdkVersion(rocrt_init)) {
            rocrt_version_addr = address + rocrt_init->rocrt_version_offset;
            if (rocrt_version_addr == 0) {
                sdk_version = {};
                memcpy(&sdk_version, reinterpret_cast<const void*>(rocrt_version_addr), sizeof(sdk_version));
            } else {
                sdk_version = {};
            }
            module_version = 1;
        } else {
            rocrt_version_addr = 0;
            sdk_version = {};
            module_version = 0;
        }
    }

    const std::uint32_t rocrt_version = module_version != 0 || sdk_version.major > 18;

    if (sdk_version.major >= 18
        && (!util::IsAligned(module_header_addr, alignof(rocrt::ModuleHeader)) || !util::IsAligned(rocrt_version_addr, alignof(rocrt::RocrtVersion)))
    ) {
        return false;
    }

    if (reinterpret_cast<const rocrt::ModuleHeader*>(module_header_addr)->signature != rocrt::MODULE_HEADER_SIGNATURE) {
        return false;
    }

    *out_header = reinterpret_cast<const rocrt::ModuleHeader*>(module_header_addr);
    out_version->sdk_major = sdk_version.major;
    out_version->sdk_minor = sdk_version.minor;
    out_version->sdk_patch = sdk_version.patch;
    out_version->rocrt_version = rocrt_version;

    return true;
}

template bool FindModuleHeader<svc::QueryMemory>(const rocrt::ModuleHeader** out_header, rocrt::ModuleVersion* out_version, uintptr_t address);

} // namespace nn::ro::detail