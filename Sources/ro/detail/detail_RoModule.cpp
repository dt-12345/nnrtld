#include "diag.hpp"
#include "ro.hpp"
#include "util.hpp"

extern "C" nn::rocrt::ModuleHeader __rocrt;
extern "C" void __ro_runtime_resolve();

namespace nn {

namespace rocrt {
extern util::TypedStorage<ro::detail::RoModule> g_RoModule;
} // namespace rocrt

namespace ro::detail {

extern void SetExceptionHandlerReady();
extern bool g_UserExceptionHandlerReady;

bool g_RoDebugFlag;
LookupGlobalManualFunc* g_LookupGlobalManualFunctionPointer;
util::TypedStorage<RoModuleList> g_ManualLoadList;
util::TypedStorage<RoModuleList> g_AutoLoadList;

namespace {

void Error() {
    while (true) { /* ... */ }
}

} // anonymous namespace

void Initialize(uintptr_t aslr_base, Elf64_Dyn* dyn) {
    util::ConstructAt(rocrt::g_RoModule);
    util::ConstructAt(g_ManualLoadList);
    util::ConstructAt(g_AutoLoadList);

    while (__rocrt.signature != rocrt::MODULE_HEADER_SIGNATURE) { /* ... */ }
    
    // manually handle this module first
    const auto module_end = util::AlignUp(reinterpret_cast<uintptr_t>(&__rocrt) + __rocrt.bss_end_offset, 0x1000ul);
    util::GetReference(rocrt::g_RoModule).Initialize(aslr_base, module_end, dyn);
    util::GetReference(g_AutoLoadList).InsertBack(util::GetPointer(rocrt::g_RoModule));

    // search for other modules
    uintptr_t current_address = 0;
    while (true) {
        std::uint32_t page_info;
        svc::MemoryInfo memory_info{};
        if (svc::QueryMemory(&memory_info, &page_info, current_address)) {
            Error();
        }

        // a valid module should start with executable code (also skip over this module because we just handled it above)
        if ((memory_info.permission & svc::MemoryPermission_Execute) != 0 && memory_info.state == svc::MemoryState_Code && memory_info.address != aslr_base) {
            const rocrt::ModuleHeader* header = nullptr;
            rocrt::ModuleVersion version{};
            if (!FindModuleHeader<svc::QueryMemory>(&header, &version, memory_info.address)) {
                Error();
            }

            if (header->bss_start_offset != header->bss_end_offset) {
                void* bss = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(header) + header->bss_start_offset);
                memset(bss, 0, header->bss_end_offset - header->bss_start_offset);
            }

            auto module = reinterpret_cast<util::TypedStorage<RoModule>*>(reinterpret_cast<uintptr_t>(header) + header->ro_module_offset);
            util::ConstructAt(*module);
            auto end = util::AlignUp(reinterpret_cast<uintptr_t>(header) + header->bss_end_offset, 0x1000ul);
            while (memory_info.size > end - memory_info.address) {
                /* ... */
            }
            util::GetReference(*module).Initialize(memory_info.address, end, reinterpret_cast<Elf64_Dyn*>(reinterpret_cast<uintptr_t>(header) + header->dynamic_offset));
            util::GetReference(*module).FixRelativeRelocations(diag::detail::RtldAbort);
            util::GetReference(g_AutoLoadList).InsertBack(util::GetPointer(*module));
        }

        const auto last_address = current_address;
        current_address = memory_info.address + memory_info.size;
        if (current_address <= last_address) {
            break;
        }
    }

    for (auto& module : util::GetReference(g_AutoLoadList)) {
        module.InitRoSymbols();
    }

    for (auto& module : util::GetReference(g_AutoLoadList)) {
        module.Relocate(diag::detail::RtldAbort);
    }
}

template <QueryMemoryFunction QueryFunc>
[[gnu::noinline]] bool FindModuleHeader(const rocrt::ModuleHeader** out_header, rocrt::ModuleVersion* out_version, uintptr_t address) {
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

bool RoModule::TryResolveSymbol(uintptr_t* target, Elf64_Sym* sym, bool* is_manual) const {
    const char* name = m_ArchData.strTable + sym->st_name;

    uintptr_t target_addr = 0;
    if (ELF64_ST_VISIBILITY(sym->st_other) == STV_DEFAULT) {
        for (const auto& module : util::GetReference(g_AutoLoadList)) {
            if (auto resolved_sym = module.GetNonLocalSymbol(name)) {
                target_addr = module.GetBase() + resolved_sym->st_value;
                break;
            }
        }

        if (g_LookupGlobalManualFunctionPointer != nullptr && target_addr == 0) {
            target_addr = g_LookupGlobalManualFunctionPointer(this, name);
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

        if (auto resolved_sym = GetNonLocalSymbol(name)) {
            target_addr = m_Base + resolved_sym->st_value;
        }
    }

    *target = target_addr;
    return target_addr != 0 || ELF64_ST_BIND(sym->st_info) == STB_WEAK;
}

void RoModule::LogUnresolvedSymbol(const Elf64_Sym* sym) const {
    diag::detail::Puts("[ro] warning: unresolved symbol = '");
    diag::detail::Puts(m_ArchData.strTable + sym->st_name);
    diag::detail::Puts("'\n");
}

void RoModule::RtldLogUnresolvedSymbol(const Elf64_Sym* sym) const {
    diag::detail::Puts("[rtld] warning: unresolved symbol = '");
    diag::detail::Puts(m_ArchData.strTable + sym->st_name);
    diag::detail::Puts("'\n");
}

void RoModule::SetSymbol(const char* name, uintptr_t address) {
    if (auto sym = GetNonLocalSymbol(name)) {
        *reinterpret_cast<uintptr_t*>(m_Base + sym->st_value) = address;
    }
}

void RoModule::FixRelativeRel(const Elf64_Rel* rel, AbortFunc abort_func) {
    abort_func("RoModule::FixRelativeRel is called");
}

void RoModule::FixRelativeRela(const Elf64_Rela* rel, AbortFunc abort_func) {
    if (ELF64_R_TYPE(rel->r_info) == R_AARCH64_RELATIVE) {
        *reinterpret_cast<uintptr_t*>(m_Base + rel->r_offset) = m_Base + rel->r_addend;
    }
}

void RoModule::FixRelativeRelr(const Elf64_Dyn* dyn, AbortFunc abort_func) {
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

void RoModule::RelocateRel(const Elf64_Rel* rel, AbortFunc abort_func) {
    abort_func("RoModule::RelocateRel is called");
}

void RoModule::RelocateRela(const Elf64_Rela* rel, AbortFunc abort_func) {
    switch (ELF64_R_TYPE(rel->r_info)) {
        case R_AARCH64_ABS16:
        case R_AARCH64_ABS32:
        case R_AARCH64_ABS64:
        case R_AARCH64_GLOB_DAT: {
            auto sym = m_ArchData.symTable + ELF64_R_SYM(rel->r_info);
            uintptr_t target_addr = 0;
            bool manual = false;
            if (TryResolveSymbol(&target_addr, sym, &manual)) {
                *reinterpret_cast<uintptr_t*>(m_Base + rel->r_offset) = target_addr + rel->r_addend;

                if (target_addr == 0 || (manual && target_addr < m_Base || target_addr >= m_Base + m_ArchData.moduleSize)) {
                    m_ArchData.flags |= ArchData::Flags_HasUnresolved;
                }
            } else {
                m_ArchData.flags |= ArchData::Flags_HasUnresolved;
                if (g_RoDebugFlag) {
                    LogUnresolvedSymbol(sym);
                }
                return;
            }
            break;
        }
        case R_AARCH64_COPY:
            abort_func("R_COPY is not supported");
    }
}

void RoModule::RelocatePltRel(const Elf64_Rel* rel, AbortFunc abort_func) {
    abort_func("RoModule::RelocatePltRel is called");
}

void RoModule::RelocatePltRela(const Elf64_Rela* rel, AbortFunc abort_func) {
    if (ELF64_R_TYPE(rel->r_info) != R_AARCH64_JUMP_SLOT) {
        return;
    }
    
    uintptr_t* pTarget = reinterpret_cast<uintptr_t*>(m_Base + rel->r_offset);
    if (m_ArchData.defaultPltGot == 0) {
        m_ArchData.defaultPltGot = *pTarget + m_Base;
    } else if (m_ArchData.defaultPltGot != *pTarget + m_Base) {
        abort_func("m_ArchData.defaultPltGot != *pTarget + m_Base");
    }

    if (IsBindNow()) {
        auto sym = m_ArchData.symTable + ELF64_R_SYM(rel->r_info);
        uintptr_t sym_addr = 0;
        if (TryResolveSymbol(&sym_addr, sym, nullptr)) {
            *pTarget = sym_addr + rel->r_addend;
        } else if (g_RoDebugFlag) {
            LogUnresolvedSymbol(sym);
        }
    } else {
        *pTarget += m_Base;
    }
}

uintptr_t RoModule::BindJumpSlotRel(std::uint32_t index) {
    diag::detail::RtldAbort("RoModule::BindJumpSlotRel is called");
    return 0;
}

uintptr_t RoModule::BindJumpSlotRela(std::uint32_t index) {
    const auto rel = m_RelaPlt + index;
    const auto sym = m_ArchData.symTable + ELF64_R_SYM(rel->r_info);

    uintptr_t target_addr;
    if (!TryResolveSymbol(&target_addr, sym, nullptr)) {
        RtldLogUnresolvedSymbol(sym);
    }

    const auto address = target_addr + rel->r_addend;
    *reinterpret_cast<uintptr_t*>(m_Base + rel->r_offset) = address;
    return address;
}

void RoModule::Initialize(uintptr_t start, uintptr_t end, Elf64_Dyn* dyn) {
    m_RelaPlt = nullptr;
    m_Rela = nullptr;
    _20 = 0;
    m_Base = start;
    m_ArchData = {
        .dyn = dyn,
        .dtInit = nullptr,
        .dtFini = nullptr,
        .bucket = nullptr,
        .chain = nullptr,
        .strTable = nullptr,
        .symTable = nullptr,
        .strTableSize = 0,
        .pltGot = nullptr,
        .relaSize = 0,
        .relSize = 0,
        .relEntryCount = 0,
        .relaEntryCount = 0,
        .nchain = 0,
        .nbucket = 0,
        .sharedObjectNameOffset = 0,
        .moduleSize = end - start,
        ._90 = 0x14,
        .flags = 0,
        .defaultPltGot = 0,
    };

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
                    diag::detail::RtldAbort();
                }
                break;
            case DT_SYMENT:
                if (dyn->d_un.d_val != sizeof(Elf64_Sym)) {
                    diag::detail::RtldAbort();
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
                    diag::detail::RtldAbort();
                }
                break;
            case DT_PLTREL:
                if (dyn->d_un.d_val == DT_RELA) {
                    m_ArchData.flags |= ArchData::Flags_PltRela;
                } else {
                    m_ArchData.flags &= ~ArchData::Flags_PltRela;
                }
                if (dyn->d_un.d_val != DT_RELA && dyn->d_un.d_val != DT_REL) {
                    diag::detail::RtldAbort();
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
                    diag::detail::RtldAbort();
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

void RoModule::FixRelativeRelocations(AbortFunc abort_func) {
    // fix relocations found in this module
    for (size_t i = 0; i < m_ArchData.relEntryCount; ++i) {
        FixRelativeRel(m_Rel + i, abort_func);
    }

    for (size_t i = 0; i < m_ArchData.relaEntryCount; ++i) {
        FixRelativeRela(m_Rela + i, abort_func);
    }

    FixRelativeRelr(m_ArchData.dyn, abort_func);
}

void RoModule::InitRoSymbols() {
    SetSymbol("_ZN2nn2ro6detail15g_pAutoLoadListE", reinterpret_cast<uintptr_t>(&g_AutoLoadList));
    SetSymbol("_ZN2nn2ro6detail17g_pManualLoadListE", reinterpret_cast<uintptr_t>(&g_ManualLoadList));
    SetSymbol("_ZN2nn2ro6detail14g_pRoDebugFlagE", reinterpret_cast<uintptr_t>(&g_RoDebugFlag));
    SetSymbol("_ZN2nn2ro6detail34g_pLookupGlobalAutoFunctionPointerE", reinterpret_cast<uintptr_t>(::nn::ro::detail::GetSymbolByName));
    SetSymbol("_ZN2nn2ro6detail36g_pLookupGlobalManualFunctionPointerE", reinterpret_cast<uintptr_t>(&g_LookupGlobalManualFunctionPointer));
}

void RoModule::Relocate(AbortFunc abort_func) {
    // fix relocations for imported symbols
    for (size_t i = m_ArchData.relEntryCount; i < m_ArchData.relSize / sizeof(Elf64_Rel); ++i) {
        RelocateRel(m_Rel + i, abort_func);
    }

    for (size_t i = m_ArchData.relaEntryCount; i < m_ArchData.relaSize / sizeof(Elf64_Rela); ++i) {
        RelocateRela(m_Rela + i, abort_func);
    }

    if ((m_ArchData.flags & ArchData::Flags_PltRela) == 0) {
        for (size_t i = 0; i < m_ArchData.pltRelocSize / sizeof(Elf64_Rel); ++i) {
            RelocatePltRel(m_RelPlt + i, abort_func);
        }
    } else {
        for (size_t i = 0; i < m_ArchData.pltRelocSize / sizeof(Elf64_Rela); ++i) {
            RelocatePltRela(m_RelaPlt + i, abort_func);
        }
    }

    if (m_ArchData.pltGot != nullptr) {
        m_ArchData.pltGot[1] = reinterpret_cast<Elf64_Xword>(this);
        m_ArchData.pltGot[2] = reinterpret_cast<Elf64_Xword>(__ro_runtime_resolve);
    }
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

Elf64_Sym* RoModule::GetSymbolByName(const char* name) const {
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

uintptr_t GetSymbolByName(const char* name) {
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

StartCallback* GetSetUserExceptionHandlerReady() {
    return SetExceptionHandlerReady;
}

StartCallback* GetInitializeModules() {
    return InitializeModules;
}

StartCallback* GetFinalizeModules() {
    return FinalizeModules;
}

uintptr_t RoModule::BindJumpSlot(std::uint32_t index) {
    if ((m_ArchData.flags & ArchData::Flags_PltRela) == 0) {
        return BindJumpSlotRel(index);
    } else {
        return BindJumpSlotRela(index);
    }
}

// NON-MATCHING: mov w9, #0xff000001 instead of orr w9, wzr, #0xff000001
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

} // namespace ro::detail

} // namespace nn