#include "ro.hpp"

extern "C" nn::rocrt::ModuleHeader __rocrt;

namespace nn {

namespace rocrt {
extern util::TypedStorage<ro::detail::RoModule> g_RoModule;
} // namespace rocrt

namespace ro::detail {

bool g_RoDebugFlag;
LookupGlobalManualFunc* g_LookupGlobalManualFunctionPointer;
util::TypedStorage<RoModuleList> g_ManualLoadList;
util::TypedStorage<RoModuleList> g_AutoLoadList;

namespace {
void Error() {
    while (true) { __asm__ __volatile__("" ::: "memory"); }
}
} // anonymouse namespace

void InitializeSelfModule(uintptr_t aslr_base, Elf64_Dyn* dyn) {
    const Elf64_Rela* rela = nullptr;
    const Elf64_Relr* relr = nullptr;
    Elf64_Xword rel_count = 0;
    Elf64_Xword rela_count = 0;
    Elf64_Xword relr_size = 0;

    for (; dyn->d_tag != DT_NULL; ++dyn) {
        switch (dyn->d_tag) {
            case DT_RELA:
                rela = reinterpret_cast<const Elf64_Rela*>(aslr_base + dyn->d_un.d_ptr);
                break;
            case DT_RELAENT:
                if (dyn->d_un.d_val != sizeof(Elf64_Rela)) {
                    Error();
                }
                break;
            case DT_RELENT:
                if (dyn->d_un.d_val != sizeof(Elf64_Rel)) {
                    Error();
                }
                break;
            case DT_RELRSZ:
                relr_size = dyn->d_un.d_val;
                break;
            case DT_RELR:
                relr = reinterpret_cast<const Elf64_Relr*>(aslr_base + dyn->d_un.d_ptr);
                break;
            case DT_RELRENT:
                if (dyn->d_un.d_val != sizeof(Elf64_Relr)) {
                    Error();
                }
                break;
            case DT_RELACOUNT:
                rela_count = dyn->d_un.d_val;
                break;
            case DT_RELCOUNT:
                rel_count = dyn->d_un.d_val;
                break;

            case DT_NULL:
            case DT_NEEDED:
            case DT_PLTRELSZ:
            case DT_PLTGOT:
            case DT_HASH:
            case DT_STRTAB:
            case DT_SYMTAB:
            case DT_RELASZ:
            case DT_SYMENT:
            case DT_INIT:
            case DT_FINI:
            case DT_SONAME:
            case DT_RPATH:
            case DT_SYMBOLIC:
            case DT_REL:
            case DT_RELSZ:
            case DT_PLTREL:
            case DT_DEBUG:
            case DT_TEXTREL:
            case DT_JMPREL:
            case DT_BIND_NOW:
            case DT_INIT_ARRAY:
            case DT_FINI_ARRAY:
            case DT_INIT_ARRAYSZ:
            case DT_FINI_ARRAYSZ:
            case DT_RUNPATH:
            case DT_FLAGS:
            case DT_PREINIT_ARRAY:
            case DT_PREINIT_ARRAYSZ:
            case DT_SYMTAB_SHNDX:
                break;
        }
    }

    for (Elf64_Xword i = 0; i < rela_count; ++i) {
        const auto rel = rela + i;
        if (ELF64_R_TYPE(rel->r_info) == R_AARCH64_RELATIVE) {
            *reinterpret_cast<uintptr_t*>(aslr_base + rel->r_offset) = aslr_base + rel->r_addend;
        }
    }

    if (rel_count) {
        Error();
    }

    if (relr != nullptr && relr_size >= sizeof(Elf64_Relr)) {
        uintptr_t* target = nullptr;
        for (Elf64_Xword i = 0; i < relr_size / sizeof(Elf64_Relr); ++i) {
            auto value = relr[i];
            if ((value & 1) == 0) {
                target = reinterpret_cast<uintptr_t*>(aslr_base + value);
                *target++ += aslr_base;
            } else {
                for (Elf64_Xword j = 0; (value >>= 1) != 0; ++j) {
                    if (value & 1) {
                        target[j] += aslr_base;
                    }
                }
                target += sizeof(void*) * 8 - 1;
            }
        }
    }
}

void Initialize(uintptr_t aslr_base, Elf64_Dyn* dyn) {
    util::ConstructAt(rocrt::g_RoModule);
    util::ConstructAt(g_ManualLoadList);
    util::ConstructAt(g_AutoLoadList);

    while (__rocrt.signature != rocrt::MODULE_HEADER_SIGNATURE) { /* ... */ }
    
    // manually handle this module first
    const auto module_end = util::AlignUp(reinterpret_cast<uintptr_t>(&__rocrt) + __rocrt.bss_end_offset, 0x1000ul);
    util::GetReference(rocrt::g_RoModule).Initialize(
        aslr_base,
        module_end - aslr_base,
        dyn,
        0,
        RoModule::InitializeSelfError
    );
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

            auto module = reinterpret_cast<RoModule*>(reinterpret_cast<uintptr_t>(header) + header->ro_module_offset);
            module->Reset();
            auto end = util::AlignUp(reinterpret_cast<uintptr_t>(header) + header->bss_end_offset, 0x1000ul);
            while (memory_info.size > end - memory_info.address) {
                /* ... */
            }
            module->Initialize(
                memory_info.address,
                end - memory_info.address,
                reinterpret_cast<Elf64_Dyn*>(reinterpret_cast<uintptr_t>(header) + header->dynamic_offset),
                0,
                RoModule::InitializeError
            );
            module->FixRelativeRelocations(Unexpected);
            util::GetReference(g_AutoLoadList).InsertBack(module);
        }

        const auto last_address = current_address;
        current_address = memory_info.address + memory_info.size;
        if (current_address <= last_address) {
            break;
        }
    }

    for (auto& module : util::GetReference(g_AutoLoadList)) {
        module.SetSymbol("_ZN2nn2ro6detail15g_pAutoLoadListE", reinterpret_cast<uintptr_t>(&g_AutoLoadList));
        module.SetSymbol("_ZN2nn2ro6detail17g_pManualLoadListE", reinterpret_cast<uintptr_t>(&g_ManualLoadList));
        module.SetSymbol("_ZN2nn2ro6detail14g_pRoDebugFlagE", reinterpret_cast<uintptr_t>(&g_RoDebugFlag));
        module.SetSymbol("_ZN2nn2ro6detail34g_pLookupGlobalAutoFunctionPointerE", reinterpret_cast<uintptr_t>(nn::ro::detail::LookupGlobalAuto));
        module.SetSymbol("_ZN2nn2ro6detail36g_pLookupGlobalManualFunctionPointerE", reinterpret_cast<uintptr_t>(&g_LookupGlobalManualFunctionPointer));
    }

    for (auto& module : util::GetReference(g_AutoLoadList)) {
        module.Relocate(
            !module.IsBindNow(),
            BindEntry,
            LookupGlobalAuto,
            g_LookupGlobalManualFunctionPointer,
            g_RoDebugFlag,
            Puts,
            Unexpected
        );
    }
}

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

} // namespace ro::detail

} // namespace nn