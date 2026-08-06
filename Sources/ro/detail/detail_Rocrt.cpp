#include "ro.hpp"

namespace nn::ro::detail {

std::uint32_t GetRocrtVersion(uintptr_t address) {
    switch (*reinterpret_cast<std::uint32_t*>(address)) {
        // rtld entrypoints
        case 0xea000000: // b #0x8 (arm)
            return 0;
        case 0xea000001: // b #0xc (arm)
            return 1;
        case 0xff000001: // ???
            return 1;
        case 0x14000002: // b #0x8 (aarch64)
            return 0;
        case 0x14000003: // b #0xc (aarch64)
            return 1;

        // application entrypoints
        case 0:
            return 0;
        case 1:
            return 1;
        
        default:
            return 1;
    }
}

} // namespace nn::ro::detail