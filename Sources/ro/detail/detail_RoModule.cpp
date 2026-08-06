#include "diag.hpp"
#include "ro.hpp"

namespace nn {

namespace rocrt {
extern util::TypedStorage<ro::detail::RoModule> g_RoModule;
} // namespace rocrt

namespace ro::detail {

void RoModule::Initialize(uintptr_t start, uintptr_t size, Elf64_Dyn* dyn, std::uint8_t flags, void (*error_callback)(std::uint32_t code)) {
    m_ArchData.moduleSize = size;
    m_Base = start;
    m_ArchData.dyn = dyn;
    m_RelaPlt = nullptr;
    m_ArchData.dtFini = nullptr;
    m_ArchData.dtInit = nullptr;
    m_ArchData.pltRelocSize = 0;
    memset(&m_ArchData.bucket, 0, 0x10);
    m_Rela = nullptr;
    m_ArchData.strTable = nullptr;
    m_ArchData.symTable = nullptr;
    m_ArchData.strTableSize = 0;
    m_ArchData.pltGot = nullptr;
    m_ArchData.relaSize = 0;
    m_ArchData.relSize = 0;
    m_ArchData.relEntryCount = 0;
    m_ArchData.relaEntryCount = 0;
    memset(&m_ArchData.nchain, 0, 0x10);
    m_ArchData.flags = flags;
    m_ArchData.sharedObjectNameOffset = 0;
    _20 = 0;
    m_ArchData.sdkVersion = rocrt::NN_SDK_VERSION_MAJOR;

    Elf64_Dyn* hash = nullptr;
    Elf64_Dyn* gnu_hash = nullptr;
    for (; dyn->d_tag != DT_NULL; ++dyn) {
        switch (dyn->d_tag) {
            case DT_PLTRELSZ:
                m_ArchData.pltRelocSize = dyn->d_un.d_val;
                break;
            case DT_PLTGOT:
                m_ArchData.pltGot = reinterpret_cast<Elf64_Xword*>(m_Base + dyn->d_un.d_ptr);
                break;
            case DT_HASH:
                hash = dyn;
                break;
            case DT_STRTAB:
                m_ArchData.strTable = reinterpret_cast<char*>(m_Base + dyn->d_un.d_ptr);
                break;
            case DT_SYMTAB:
                m_ArchData.symTable = reinterpret_cast<Elf64_Sym*>(m_Base + dyn->d_un.d_ptr);
                break;
            case DT_RELA:
                m_Rela = reinterpret_cast<Elf64_Rela*>(m_Base + dyn->d_un.d_ptr);
                break;
            case DT_REL:
                m_Rel = reinterpret_cast<Elf64_Rel*>(m_Base + dyn->d_un.d_ptr);
                break;
            case DT_RELASZ:
                m_ArchData.relaSize = dyn->d_un.d_val;
                break;
            case DT_RELAENT:
                if (dyn->d_un.d_val != sizeof(Elf64_Rela)) {
                    error_callback(DT_RELAENT);
                }
                break;
            case DT_SYMENT:
                if (dyn->d_un.d_val != sizeof(Elf64_Sym)) {
                    error_callback(DT_SYMENT);
                }
                break;
            case DT_STRSZ:
                m_ArchData.strTableSize = dyn->d_un.d_val;
                break;
            case DT_INIT:
                m_ArchData.dtInit = reinterpret_cast<void(*)()>(m_Base + dyn->d_un.d_ptr);
                break;
            case DT_FINI:
                m_ArchData.dtFini = reinterpret_cast<void(*)()>(m_Base + dyn->d_un.d_ptr);
                break;
            case DT_SONAME:
                m_ArchData.sharedObjectNameOffset = dyn->d_un.d_val;
                break;
            case DT_RELSZ:
                m_ArchData.relSize = dyn->d_un.d_val;
                break;
            case DT_RELENT:
                if (dyn->d_un.d_val != sizeof(Elf64_Rel)) {
                    error_callback(DT_RELENT);
                }
                break;
            case DT_PLTREL:
                if (dyn->d_un.d_val == DT_RELA) {
                    m_ArchData.flags |= ArchData::Flags_PltRela;
                } else {
                    m_ArchData.flags &= ~ArchData::Flags_PltRela;
                }
                if (dyn->d_un.d_val != DT_RELA && dyn->d_un.d_val != DT_REL) {
                    error_callback(DT_PLTREL);
                }
                break;
            case DT_JMPREL:
                m_RelaPlt = reinterpret_cast<Elf64_Rela*>(m_Base + dyn->d_un.d_ptr);
                break;
            case DT_BIND_NOW:
                m_ArchData.flags |= ArchData::Flags_BindNow;
                break;
            case DT_FLAGS:
                if (dyn->d_un.d_val & DF_BIND_NOW) {
                    m_ArchData.flags |= ArchData::Flags_BindNow;
                }
                break;
            case DT_RELRENT:
                if (dyn->d_un.d_val != sizeof(Elf64_Relr)) {
                    error_callback(DT_RELRENT);
                }
                break;
            case DT_GNU_HASH:
                gnu_hash = dyn;
                break;
            case DT_RELACOUNT:
                m_ArchData.relaEntryCount = dyn->d_un.d_val;
                break;
            case DT_RELCOUNT:
                m_ArchData.relEntryCount = dyn->d_un.d_val;
                break;
            case DT_FLAGS_1:
                if (dyn->d_un.d_val & DF_1_NOW) {
                    m_ArchData.flags |= ArchData::Flags_BindNow;
                }
                break;
        }
    }

    if (gnu_hash != nullptr) {
        m_ArchData.flags |= ArchData::Flags_GnuHash;
        auto hash_table = reinterpret_cast<Elf64_Word*>(m_Base + gnu_hash->d_un.d_ptr);
        m_ArchData.hashTable = hash_table;
        m_ArchData.bloom = reinterpret_cast<Elf64_Xword*>(hash_table + 4);
        m_ArchData.bloomSize = hash_table[2];
        m_ArchData.bloomShift = hash_table[3];
    } else if (hash != nullptr) {
        m_ArchData.flags &= ~ArchData::Flags_GnuHash;
        auto hash_table = reinterpret_cast<Elf64_Word*>(m_Base + hash->d_un.d_ptr);
        m_ArchData.nbucket = hash_table[0];
        m_ArchData.nchain = hash_table[1];
        m_ArchData.bucket = hash_table + 2;
        m_ArchData.chain = m_ArchData.bucket + m_ArchData.nbucket;
    }

    m_ArchData.defaultPltGot = 0;
}

bool RoModule::TryResolveSymbol(
    uintptr_t* target,
    Elf64_Sym* sym,
    bool* is_manual,
    void (* /* jump_slot_resolver */)(),
    uintptr_t (*lookup_auto)(const char*),
    uintptr_t (*lookup_manual)(const RoModule*, const char*)
) const {
    const char* name = m_ArchData.strTable + sym->st_name;

    uintptr_t target_addr = 0;
    if (ELF64_ST_VISIBILITY(sym->st_other) == STV_DEFAULT) {
        target_addr = lookup_auto(name);
        if (lookup_manual != nullptr && target_addr == 0) {
            target_addr = lookup_manual(this, name);
            if (is_manual) {
                *is_manual = true;
            }
        } else {
            if (is_manual) {
                *is_manual = false;
            }
        }
    } else {
        if (is_manual) {
            *is_manual = false;
        }

        if (auto resolved_sym = LookupSymbol(name)) {
            target_addr = m_Base + resolved_sym->st_value;
        }
    }

    *target = target_addr;
    return target_addr != 0 || ELF64_ST_BIND(sym->st_info) == STB_WEAK;
}

void RoModule::RtldLogUnresolvedSymbol(const Elf64_Sym* sym) const {
    diag::detail::Puts("[rtld] warning: unresolved symbol = '");
    diag::detail::Puts(m_ArchData.strTable + sym->st_name);
    diag::detail::Puts("'\n");
}

void RoModule::FixRelativeRel(const Elf64_Rel* /* rel */, LogFunc error_callback) {
    error_callback("RoModule::FixRelativeRel is called");
}

void RoModule::FixRelativeRela(const Elf64_Rela* rel, LogFunc /* error_callback */) {
    if (ELF64_R_TYPE(rel->r_info) == R_AARCH64_RELATIVE) {
        *reinterpret_cast<uintptr_t*>(m_Base + rel->r_offset) = m_Base + rel->r_addend;
    }
}

void RoModule::FixRelativeRelr(const Elf64_Dyn* dyn, LogFunc /* error_callback */) {
    const Elf64_Relr* relr = nullptr;
    Elf64_Xword relr_size = 0;

    for (; dyn->d_tag != DT_NULL; ++dyn) {
        switch (dyn->d_tag) {
            case DT_RELRSZ:
                relr_size = dyn->d_un.d_val;
                break;
            case DT_RELR:
                relr = reinterpret_cast<const Elf64_Relr*>(m_Base + dyn->d_un.d_ptr);
                break;
            default:
                break;
        }
    }

    if (relr != nullptr && relr_size >= sizeof(Elf64_Relr)) {
        uintptr_t* target = nullptr;
        for (size_t i = 0; i < relr_size / sizeof(Elf64_Relr); ++i) {
            auto value = relr[i];
            if ((value & 1) == 0) {
                target = reinterpret_cast<uintptr_t*>(m_Base + value);
                *target++ += m_Base;
            } else {
                for (size_t j = 0; (value >>= 1) != 0; ++j) {
                    if (value & 1) {
                        target[j] += m_Base;
                    }
                }
                target += sizeof(void*) * 8 - 1;
            }
        }
    }
}

void RoModule::FixRelativeRelocations(LogFunc error_callback) {
    // fix relocations found in this module
    for (size_t i = 0; i < m_ArchData.relEntryCount; ++i) {
        FixRelativeRel(m_Rel + i, error_callback);
    }

    for (size_t i = 0; i < m_ArchData.relaEntryCount; ++i) {
        FixRelativeRela(m_Rela + i, error_callback);
    }

    FixRelativeRelr(m_ArchData.dyn, error_callback);
}

void Unexpected(const char* msg) {
    diag::detail::Puts(msg);
    diag::detail::Abort();
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
            auto sym = m_ArchData.symTable + bucket;
            if (ELF64_ST_TYPE(sym->st_info) != STT_FILE
                && sym->st_shndx != SHN_UNDEF
                && sym->st_shndx != SHN_COMMON
                && strcmp(name, m_ArchData.strTable + sym->st_name) == 0
            ) {
                return sym;
            }
        }
    } else {
        constexpr const Elf64_Word BITS = sizeof(Elf64_Xword) * 8;
        const Elf64_Word hash = CalcGnuHash(name);
        const Elf64_Xword bloom_value = m_ArchData.bloom[(m_ArchData.bloomSize - 1) & (hash / BITS)];
        const Elf64_Xword bloom_mask = 1ull << static_cast<Elf64_Xword>((hash >> m_ArchData.bloomShift) % BITS) | 1ull << static_cast<Elf64_Xword>(hash % BITS);

        if ((bloom_mask & bloom_value) == bloom_mask) {
            const auto nbuckets = m_ArchData.hashTable[0];
            const auto sym_offset = m_ArchData.hashTable[1];
            const auto buckets = reinterpret_cast<const Elf64_Word*>(m_ArchData.bloom + m_ArchData.bloomSize);
            const auto syms = buckets + nbuckets;

            auto sym_index = buckets[hash % nbuckets];

            if (sym_index >= sym_offset) {
                while (true) {
                    const auto hash_value = syms[sym_index - sym_offset];
                    if ((hash | 1) == (hash_value | 1)) {
                        if (strcmp(name, m_ArchData.strTable + m_ArchData.symTable[sym_index].st_name) == 0) {
                            return m_ArchData.symTable + sym_index;
                        }
                    }

                    if (hash_value & 1) {
                        break;
                    }

                    ++sym_index;
                }
            }
        }
    }

    return nullptr;
}

uintptr_t LookupGlobalAuto(const char* name) {
    for (const auto& module : util::GetReference(g_AutoLoadList)) {
        if (auto sym = module.GetNonLocalSymbol(name)) {
            return module.GetBase() + sym->st_value;
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

void RoModule::RelocateRel(
    const Elf64_Rel* /* rel */,
    void (* /* jump_slot_resolver */)(),
    uintptr_t (* /* lookup_auto */)(const char*),
    uintptr_t (* /* lookup_manual */)(const RoModule*, const char*),
    bool /* debug */,
    LogFunc /* warning_callback */,
    LogFunc error_callback
) {
    error_callback("RoModule::RelocateRel is called");
}

void RoModule::RelocateRela(
    const Elf64_Rela* rel,
    void (*jump_slot_resolver)(),
    uintptr_t (*lookup_auto)(const char*),
    uintptr_t (*lookup_manual)(const RoModule*, const char*),
    bool debug,
    LogFunc warning_callback,
    LogFunc error_callback
) {
    switch (ELF64_R_TYPE(rel->r_info)) {
        case R_AARCH64_ABS16:
        case R_AARCH64_ABS32:
        case R_AARCH64_ABS64:
        case R_AARCH64_GLOB_DAT: {
            auto sym = m_ArchData.symTable + ELF64_R_SYM(rel->r_info);
            uintptr_t target_addr = 0;
            bool manual = false;
            if (TryResolveSymbol(&target_addr, sym, &manual, jump_slot_resolver, lookup_auto, lookup_manual)) {
                *reinterpret_cast<uintptr_t*>(m_Base + rel->r_offset) = target_addr + rel->r_addend;

                if (target_addr == 0 || (manual && (target_addr < m_Base || target_addr >= m_Base + m_ArchData.moduleSize))) {
                    m_ArchData.flags |= ArchData::Flags_HasUnresolved;
                }
            } else {
                m_ArchData.flags |= ArchData::Flags_HasUnresolved;
                if (debug) {
                    warning_callback("[ro] warning: unresolved symbol = '");
                    warning_callback(m_ArchData.strTable + sym->st_name);
                    warning_callback("'\n");
                }
                return;
            }
            break;
        }
        case R_AARCH64_COPY:
            error_callback("R_COPY is not supported");
    }
}

void RoModule::RelocatePltRel(
    const Elf64_Rel* /* rel */,
    bool /* lazy */,
    void (* /* jump_slot_resolver */)(),
    uintptr_t (* /* lookup_auto */)(const char*),
    uintptr_t (* /* lookup_manual */)(const RoModule*, const char*),
    bool /* debug */,
    LogFunc /* warning_callback */,
    LogFunc error_callback
) {
    error_callback("RoModule::RelocatePltRel is called");
}

void RoModule::RelocatePltRela(
    const Elf64_Rela* rel,
    bool lazy,
    void (*jump_slot_resolver)(),
    uintptr_t (*lookup_auto)(const char*),
    uintptr_t (*lookup_manual)(const RoModule*, const char*),
    bool debug,
    LogFunc warning_callback,
    LogFunc error_callback
) {
    if (ELF64_R_TYPE(rel->r_info) != R_AARCH64_JUMP_SLOT) {
        return;
    }
    
    uintptr_t* pTarget = reinterpret_cast<uintptr_t*>(m_Base + rel->r_offset);
    if (m_ArchData.defaultPltGot == 0) {
        m_ArchData.defaultPltGot = *pTarget + m_Base;
    } else if (m_ArchData.defaultPltGot != *pTarget + m_Base) {
        error_callback("m_ArchData.defaultPltGot != *pTarget + m_Base");
    }

    if (lazy) {
        *pTarget += m_Base;
    } else {
        auto sym = m_ArchData.symTable + ELF64_R_SYM(rel->r_info);
        uintptr_t sym_addr = 0;
        if (TryResolveSymbol(&sym_addr, sym, nullptr, jump_slot_resolver, lookup_auto, lookup_manual)) {
            *pTarget = sym_addr + rel->r_addend;
        } else if (debug) {
            warning_callback("[ro] warning: unresolved symbol = '");
            warning_callback(m_ArchData.strTable + sym->st_name);
            warning_callback("'\n");
        }
    }
}

void RoModule::Relocate(
    bool lazy,
    void (*jump_slot_resolver)(),
    uintptr_t (*lookup_auto)(const char*),
    uintptr_t (*lookup_manual)(const RoModule*, const char*),
    bool debug,
    LogFunc warning_callback,
    LogFunc error_callback
) {
    // fix relocations for imported symbols
    for (size_t i = m_ArchData.relEntryCount; i < m_ArchData.relSize / sizeof(Elf64_Rel); ++i) {
        RelocateRel(m_Rel + i, jump_slot_resolver, lookup_auto, lookup_manual, debug, warning_callback, error_callback);
    }

    for (size_t i = m_ArchData.relaEntryCount; i < m_ArchData.relaSize / sizeof(Elf64_Rela); ++i) {
        RelocateRela(m_Rela + i, jump_slot_resolver, lookup_auto, lookup_manual, debug, warning_callback, error_callback);
    }

    if ((m_ArchData.flags & ArchData::Flags_PltRela) == 0) {
        for (size_t i = 0; i < m_ArchData.pltRelocSize / sizeof(Elf64_Rel); ++i) {
            RelocatePltRel(m_RelPlt + i, lazy, jump_slot_resolver, lookup_auto, lookup_manual, debug, warning_callback, error_callback);
        }
    } else {
        for (size_t i = 0; i < m_ArchData.pltRelocSize / sizeof(Elf64_Rela); ++i) {
            RelocatePltRela(m_RelaPlt + i, lazy, jump_slot_resolver, lookup_auto, lookup_manual, debug, warning_callback, error_callback);
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
    const auto rel = m_RelaPlt + index;
    const auto sym = m_ArchData.symTable + ELF64_R_SYM(rel->r_info);

    uintptr_t target_addr;
    if (!TryResolveSymbol(&target_addr, sym, nullptr, BindEntry, LookupGlobalAuto, g_LookupGlobalManualFunctionPointer)) {
        diag::detail::Puts("[rtld] warning: unresolved symbol = '");
        diag::detail::Puts(m_ArchData.strTable + sym->st_name);
        diag::detail::Puts("'\n");
    }

    const auto address = target_addr + rel->r_addend;
    *reinterpret_cast<uintptr_t*>(m_Base + rel->r_offset) = address;
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