#include "diag.hpp"
#include "ro.hpp"

namespace nn {

namespace rocrt {
extern util::TypedStorage<ro::detail::RoModule> g_RoModule;
} // namespace rocrt

namespace ro::detail {

void RoModule::FixRelativeRel(const Elf64_Rel* /* pRel */, ErrorFunc errorCallback) {
    errorCallback("RoModule::FixRelativeRel is called");
}

void RoModule::FixRelativeRela(const Elf64_Rela* pRel, ErrorFunc /* errorCallback */) {
    if (ELF64_R_TYPE(pRel->r_info) == R_AARCH64_RELATIVE) {
        *reinterpret_cast<uintptr_t*>(m_Base + pRel->r_offset) = m_Base + pRel->r_addend;
    }
}

void RoModule::FixRelativeRelr(const Elf64_Dyn* pDyn, ErrorFunc /* errorCallback */) {
    const Elf64_Relr* pRelr = nullptr;
    Elf64_Xword relrSize = 0;

    for (; pDyn->d_tag != DT_NULL; ++pDyn) {
        switch (pDyn->d_tag) {
            case DT_RELRSZ:
                relrSize = pDyn->d_un.d_val;
                break;
            case DT_RELR:
                pRelr = reinterpret_cast<const Elf64_Relr*>(m_Base + pDyn->d_un.d_ptr);
                break;
            default:
                break;
        }
    }

    if (pRelr != nullptr && relrSize >= sizeof(Elf64_Relr)) {
        uintptr_t* pTarget = nullptr;
        for (size_t i = 0; i < relrSize / sizeof(Elf64_Relr); ++i) {
            auto value = pRelr[i];
            if ((value & 1) == 0) {
                pTarget = reinterpret_cast<uintptr_t*>(m_Base + value);
                *pTarget++ += m_Base;
            } else {
                for (size_t j = 0; (value >>= 1) != 0; ++j) {
                    if (value & 1) {
                        pTarget[j] += m_Base;
                    }
                }
                pTarget += sizeof(void*) * 8 - 1;
            }
        }
    }
}

void RoModule::FixRelativeRelocations(ErrorFunc errorCallback) {
    // fix relocations found in this module
    for (size_t i = 0; i < m_ArchData.relEntryCount; ++i) {
        FixRelativeRel(m_Rel + i, errorCallback);
    }

    for (size_t i = 0; i < m_ArchData.relaEntryCount; ++i) {
        FixRelativeRela(m_Rela + i, errorCallback);
    }

    FixRelativeRelr(m_ArchData.dyn, errorCallback);
}

void Unexpected(const char* msg) {
    diag::detail::Puts(msg);
    diag::detail::Abort();
    __asm__ __volatile__("udf 0x8002" ::: "memory");
    __builtin_unreachable();
}

static Elf64_Word CalcElfHash(const char* name) {
    Elf64_Word hash = 0, high;

    while (*name) {
        hash = (hash << 4) + static_cast<Elf64_Word>(*name++);
        if (high = hash & 0xf0000000; high != 0) {
            hash ^= high >> 0x18;
        }
        hash &= ~high;
    }

    return hash;
}

static Elf64_Word CalcGnuHash(const char* name) {
    Elf64_Word hash = 0x1505;

    while (*name) {
        hash = (hash << 5) + hash + *name++;
    }

    return hash;
}

Elf64_Sym* RoModule::LookupSymbol(const char* name) const {
    if ((m_ArchData.flags & ArchData::Flags_GnuHash) == 0) {
        for (Elf64_Word bucket = m_ArchData.bucket[CalcElfHash(name) % m_ArchData.nbucket]; bucket != 0; bucket = m_ArchData.chain[bucket]) {
            auto pSym = m_ArchData.symTable + bucket;
            if (ELF64_ST_TYPE(pSym->st_info) != STT_FILE
                && pSym->st_shndx != SHN_UNDEF
                && pSym->st_shndx != SHN_COMMON
                && strcmp(name, m_ArchData.strTable + pSym->st_name) == 0
            ) {
                return pSym;
            }
        }
    } else {
        constexpr const Elf64_Word BITS = sizeof(Elf64_Xword) * 8;
        const Elf64_Word hash = CalcGnuHash(name);
        const Elf64_Xword bloomValue = m_ArchData.bloom[(m_ArchData.bloomSize - 1) & (hash / BITS)];
        const Elf64_Xword bloomMask = 1ull << static_cast<Elf64_Xword>((hash >> m_ArchData.bloomShift) % BITS) | 1ull << static_cast<Elf64_Xword>(hash % BITS);

        if ((bloomMask & bloomValue) == bloomMask) {
            const auto nbuckets = m_ArchData.hashTable[0];
            const auto symBase = m_ArchData.hashTable[1];
            const auto buckets = reinterpret_cast<const Elf64_Word*>(m_ArchData.bloom + m_ArchData.bloomSize);
            const auto syms = buckets + nbuckets;

            auto symIndex = buckets[hash % nbuckets];

            if (symIndex >= symBase) {
                while (true) {
                    const auto hash_value = syms[symIndex - symBase];
                    if ((hash | 1) == (hash_value | 1)) {
                        if (strcmp(name, m_ArchData.strTable + m_ArchData.symTable[symIndex].st_name) == 0) {
                            return m_ArchData.symTable + symIndex;
                        }
                    }

                    if (hash_value & 1) {
                        break;
                    }

                    ++symIndex;
                }
            }
        }
    }

    return nullptr;
}

uintptr_t LookupGlobalAuto(const char* name) {
    for (const auto& module : util::GetReference(g_AutoLoadList)) {
        if (auto pSym = module.LookupSymbol(name)) {
            switch (ELF64_ST_BIND(pSym->st_info)) {
                case STB_LOCAL:
                    break;
                default:
                    return module.GetBase() + pSym->st_value;
            }
        }
    }

    return 0;
}

void InitializeModules() {
    for (auto& module : util::GetReference(g_AutoLoadList).reverse()) {
        if (module.GetArchData().dtInit) {
            module.GetArchData().dtInit();
        }
    }
}

void FinalizeModules() {
    for (auto& module : util::GetReference(g_AutoLoadList)) {
        if (module.GetArchData().dtFini) {
            module.GetArchData().dtFini();
        }
    }
}

extern void SetExceptionHandlerReady();

StartCallback* GetSetUserExceptionHandlerReady() {
    return SetExceptionHandlerReady;
}

StartCallback* GetInitializeModules() {
    return InitializeModules;
}

StartCallback* GetFinalizeModules() {
    return FinalizeModules;
}

bool RoModule::TryResolveSymbol(
    uintptr_t* pOutTarget,
    const Elf64_Sym* pSym,
    bool* pOutManual,
    void (* /* jumpSlotResolver */)(),
    uintptr_t (*lookupAuto)(const char*),
    uintptr_t (*lookupManual)(const RoModule*, const char*)
) const {
    const char* name = m_ArchData.strTable + pSym->st_name;

    uintptr_t targetAddr = 0;
    if (ELF64_ST_VISIBILITY(pSym->st_other) == STV_DEFAULT) {
        targetAddr = lookupAuto(name);
        if (lookupManual != nullptr && targetAddr == 0) {
            targetAddr = lookupManual(this, name);
            if (pOutManual) {
                *pOutManual = true;
            }
        } else {
            if (pOutManual) {
                *pOutManual = false;
            }
        }
    } else {
        if (pOutManual) {
            *pOutManual = false;
        }

        if (auto pResolved = LookupSymbol(name)) {
            targetAddr = m_Base + pResolved->st_value;
        }
    }

    *pOutTarget = targetAddr;
    return targetAddr != 0 || ELF64_ST_BIND(pSym->st_info) == STB_WEAK;
}

void RoModule::RelocateRel(
    const Elf64_Rel* /* pRel */,
    void (* /* jumpSlotResolver */)(),
    uintptr_t (* /* lookupAuto */)(const char*),
    uintptr_t (* /* lookupManual */)(const RoModule*, const char*),
    bool /* debug */,
    WarningFunc /* warningCallback */,
    ErrorFunc errorCallback
) {
    errorCallback("RoModule::RelocateRel is called");
}

void RoModule::RelocateRela(
    const Elf64_Rela* pRel,
    void (*jumpSlotResolver)(),
    uintptr_t (*lookupAuto)(const char*),
    uintptr_t (*lookupManual)(const RoModule*, const char*),
    bool debug,
    WarningFunc warningCallback,
    ErrorFunc errorCallback
) {
    switch (ELF64_R_TYPE(pRel->r_info)) {
        case R_AARCH64_ABS16:
        case R_AARCH64_ABS32:
        case R_AARCH64_ABS64:
        case R_AARCH64_GLOB_DAT: {
            auto pSym = m_ArchData.symTable + ELF64_R_SYM(pRel->r_info);
            uintptr_t targetAddr = 0;
            bool manual = false;
            if (TryResolveSymbol(&targetAddr, pSym, &manual, jumpSlotResolver, lookupAuto, lookupManual)) {
                *reinterpret_cast<uintptr_t*>(m_Base + pRel->r_offset) = targetAddr + pRel->r_addend;

                if (targetAddr == 0 || (manual && (targetAddr < m_Base || targetAddr >= m_Base + m_ArchData.moduleSize))) {
                    m_ArchData.flags |= ArchData::Flags_HasUnresolved;
                }
            } else {
                m_ArchData.flags |= ArchData::Flags_HasUnresolved;
                if (debug) {
                    warningCallback("[ro] warning: unresolved symbol = '");
                    warningCallback(m_ArchData.strTable + pSym->st_name);
                    warningCallback("'\n");
                }
                return;
            }
            break;
        }
        case R_AARCH64_COPY:
            errorCallback("R_COPY is not supported");
            break;
    }
}

void RoModule::RelocatePltRel(
    const Elf64_Rel* /* pRel */,
    bool /* lazy */,
    void (* /* jumpSlotResolver */)(),
    uintptr_t (* /* lookupAuto */)(const char*),
    uintptr_t (* /* lookupManual */)(const RoModule*, const char*),
    bool /* debug */,
    WarningFunc /* warningCallback */,
    ErrorFunc errorCallback
) {
    errorCallback("RoModule::RelocatePltRel is called");
}

void RoModule::RelocatePltRela(
    const Elf64_Rela* pRel,
    bool lazy,
    void (*jumpSlotResolver)(),
    uintptr_t (*lookupAuto)(const char*),
    uintptr_t (*lookupManual)(const RoModule*, const char*),
    bool debug,
    WarningFunc warningCallback,
    ErrorFunc errorCallback
) {
    if (ELF64_R_TYPE(pRel->r_info) != R_AARCH64_JUMP_SLOT) {
        return;
    }
    
    uintptr_t* pTarget = reinterpret_cast<uintptr_t*>(m_Base + pRel->r_offset);
    if (m_ArchData.defaultPltGot == 0) {
        m_ArchData.defaultPltGot = *pTarget + m_Base;
    } else if (m_ArchData.defaultPltGot != *pTarget + m_Base) {
        errorCallback("m_ArchData.defaultPltGot != *pTarget + m_Base");
    }

    if (lazy) {
        *pTarget += m_Base;
    } else {
        auto pSym = m_ArchData.symTable + ELF64_R_SYM(pRel->r_info);
        uintptr_t symAddr = 0;
        if (TryResolveSymbol(&symAddr, pSym, nullptr, jumpSlotResolver, lookupAuto, lookupManual)) {
            *pTarget = symAddr + pRel->r_addend;
        } else if (debug) {
            warningCallback("[ro] warning: unresolved symbol = '");
            warningCallback(m_ArchData.strTable + pSym->st_name);
            warningCallback("'\n");
        }
    }
}

void RoModule::Relocate(
    bool lazy,
    void (*jumpSlotResolver)(),
    uintptr_t (*lookupAuto)(const char*),
    uintptr_t (*lookupManual)(const RoModule*, const char*),
    bool debug,
    WarningFunc warningCallback,
    ErrorFunc errorCallback
) {
    // fix relocations for imported symbols
    for (size_t i = m_ArchData.relEntryCount; i < m_ArchData.relSize / sizeof(Elf64_Rel); ++i) {
        RelocateRel(m_Rel + i, jumpSlotResolver, lookupAuto, lookupManual, debug, warningCallback, errorCallback);
    }

    for (size_t i = m_ArchData.relaEntryCount; i < m_ArchData.relaSize / sizeof(Elf64_Rela); ++i) {
        RelocateRela(m_Rela + i, jumpSlotResolver, lookupAuto, lookupManual, debug, warningCallback, errorCallback);
    }

    if ((m_ArchData.flags & ArchData::Flags_PltRela) == 0) {
        for (size_t i = 0; i < m_ArchData.pltRelocSize / sizeof(Elf64_Rel); ++i) {
            RelocatePltRel(m_RelPlt + i, lazy, jumpSlotResolver, lookupAuto, lookupManual, debug, warningCallback, errorCallback);
        }
    } else {
        for (size_t i = 0; i < m_ArchData.pltRelocSize / sizeof(Elf64_Rela); ++i) {
            RelocatePltRela(m_RelaPlt + i, lazy, jumpSlotResolver, lookupAuto, lookupManual, debug, warningCallback, errorCallback);
        }
    }

    if (m_ArchData.pltGot != nullptr) {
        m_ArchData.pltGot[1] = reinterpret_cast<Elf64_Xword>(this);
        m_ArchData.pltGot[2] = reinterpret_cast<Elf64_Xword>(BindEntry);
    }
}

uintptr_t RoModule::BindJumpSlotRel(std::uint32_t /* index */) {
    Unexpected("RoModule::BindJumpSlotRel is called");
    return 0;
}

uintptr_t RoModule::BindJumpSlotRela(std::uint32_t index) {
    const auto pRel = m_RelaPlt + index;
    const auto pSym = m_ArchData.symTable + ELF64_R_SYM(pRel->r_info);

    uintptr_t targetAddr;
    if (!TryResolveSymbol(&targetAddr, pSym, nullptr, BindEntry, LookupGlobalAuto, g_LookupGlobalManualFunctionPointer)) {
        diag::detail::Puts("[rtld] warning: unresolved symbol = '");
        diag::detail::Puts(m_ArchData.strTable + pSym->st_name);
        diag::detail::Puts("'\n");
    }

    const auto address = targetAddr + pRel->r_addend;
    *reinterpret_cast<uintptr_t*>(m_Base + pRel->r_offset) = address;
    return address;
}

// based on the sdk, this is supposed to be nn::ro::detail::Bind and not a member function but whatever
uintptr_t RoModule::BindJumpSlot(std::uint32_t index) {
    if ((m_ArchData.flags & ArchData::Flags_PltRela) == 0) {
        return BindJumpSlotRel(index);
    } else {
        return BindJumpSlotRela(index);
    }
}

void Puts(const char* msg) {
    diag::detail::Puts(msg);
}

void RoModule::InitializeSelfError(std::uint32_t) {
    diag::detail::Abort();
}

void RoModule::InitializeError(std::uint32_t) {
    diag::detail::Abort();
}

} // namespace ro::detail

} // namespace nn