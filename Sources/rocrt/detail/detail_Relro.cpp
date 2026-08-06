#include "diag.hpp"
#include "ro.hpp"
#include "svc.hpp"

namespace nn::rocrt::detail {

void ProtectRelro(const void* relro, const void* relroEnd, const void* fullRelroEnd, const void* pModule, const void* /* pVersion */) noexcept {
    auto module = static_cast<const ro::detail::RoModule*>(pModule);

    if (module->GetUnk20() != 0) {
        return;
    }

    const void* end = module->IsBindNow() ? fullRelroEnd : relroEnd;
    if ((module->IsBindNow() || !module->HasUnresolved()) && end != relro) {
        const auto size = reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(relro);
        if (svc::SetMemoryPermission(relro, size, svc::MemoryPermission_Read)) {
            diag::detail::Abort();
        }

        if (svc::SetMemoryAttribute(relro, size, svc::MemoryAttribute_PermissionLocked, svc::MemoryAttribute_PermissionLocked)) {
            diag::detail::Abort();
        }
    }

    if (!module->IsBindNow()) {
        const void* begin = module->HasUnresolved() ? relro : relroEnd;

        if (begin != fullRelroEnd) {
            const auto size = reinterpret_cast<uintptr_t>(fullRelroEnd) - reinterpret_cast<uintptr_t>(begin);
            if (svc::SetMemoryAttribute(begin, size, svc::MemoryAttribute_PermissionLocked, svc::MemoryAttribute_PermissionLocked)) {
                diag::detail::Abort();
            }
        }
    }
}

void UnknownFunction() { /* ... */ }

} // namespace nn::rocrt::detail