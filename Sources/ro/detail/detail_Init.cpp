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

#define NN_ASSERT(EXPR) do { if (!(EXPR)) { while (true) { __asm__ __volatile__("" ::: "memory"); } } } while (0)

void InitializeSelfModule(uintptr_t moduleBase, Elf64_Dyn* pDyn) {
    const Elf64_Rela* pRela = nullptr;
    Elf64_Xword relCount = 0;
    Elf64_Xword relrCount = 0;
    Elf64_Xword relaCount = 0;
    const Elf64_Relr* pRelr = nullptr;

    for (; pDyn->d_tag != DT_NULL; ++pDyn) {
        switch (pDyn->d_tag) {
            case DT_RELA:
                pRela = reinterpret_cast<const Elf64_Rela*>(moduleBase + pDyn->d_un.d_ptr);
                break;
            case DT_RELAENT:
                NN_ASSERT(pDyn->d_un.d_val == sizeof(Elf64_Rela));
                break;
            case DT_RELENT:
                NN_ASSERT(pDyn->d_un.d_val == sizeof(Elf64_Rel));
                break;
            case DT_RELRSZ:
                relrCount = pDyn->d_un.d_val;
                break;
            case DT_RELR:
                pRelr = reinterpret_cast<const Elf64_Relr*>(moduleBase + pDyn->d_un.d_ptr);
                break;
            case DT_RELRENT:
                NN_ASSERT(pDyn->d_un.d_val == sizeof(Elf64_Relr));
                break;
            case DT_RELACOUNT:
                relaCount = pDyn->d_un.d_val;
                break;
            case DT_RELCOUNT:
                relCount = pDyn->d_un.d_val;
                break;
        }
    }

    for (Elf64_Xword i = 0; i < relaCount; ++i) {
        const auto rel = pRela + i;
        if (ELF64_R_TYPE(rel->r_info) == R_AARCH64_RELATIVE) {
            *reinterpret_cast<uintptr_t*>(moduleBase + rel->r_offset) = moduleBase + rel->r_addend;
        }
    }

    NN_ASSERT(relCount == 0);

    if (pRelr != nullptr && relrCount >= sizeof(Elf64_Relr)) {
        uintptr_t* pTarget = nullptr;
        for (Elf64_Xword i = 0; i < relrCount / sizeof(Elf64_Relr); ++i) {
            auto value = pRelr[i];
            if ((value & 1) == 0) {
                pTarget = reinterpret_cast<uintptr_t*>(moduleBase + value);
                *pTarget++ += moduleBase;
            } else {
                for (Elf64_Xword j = 0; (value >>= 1) != 0; ++j) {
                    if (value & 1) {
                        pTarget[j] += moduleBase;
                    }
                }
                pTarget += sizeof(void*) * 8 - 1;
            }
        }
    }
}

template <QueryMemoryFunction QueryFunc>
static svc::MemoryInfo GetNextRegion(uintptr_t baseAddr) {
    std::uint32_t pageInfo = 0u;
    svc::MemoryInfo memoryInfo{};
    NN_ASSERT(QueryFunc(&memoryInfo, &pageInfo, baseAddr) == 0);
    return memoryInfo;
}

template <QueryMemoryFunction QueryFunc, typename CallbackT>
static void ForEachRegion(uintptr_t addr, CallbackT func) {
    svc::MemoryInfo memoryInfo = GetNextRegion<QueryFunc>(addr);
    while (true) {
        func(memoryInfo);

        const auto lastAddr = addr;
        addr = memoryInfo.address + memoryInfo.size;

        if (addr <= lastAddr) {
            break;
        }

        memoryInfo = GetNextRegion<QueryFunc>(addr);
    }
}

void Initialize(uintptr_t moduleBase, Elf64_Dyn* pDyn) {
    util::ConstructAt(rocrt::g_RoModule);
    util::ConstructAt(g_ManualLoadList);
    util::ConstructAt(g_AutoLoadList);

    NN_ASSERT(__rocrt.signature == rocrt::MODULE_HEADER_SIGNATURE);
    
    // manually handle this module first
    const auto moduleEnd = util::AlignUp(reinterpret_cast<uintptr_t>(&__rocrt) + __rocrt.bssEndOffset, 0x1000ul);
    util::GetReference(rocrt::g_RoModule).Initialize(
        moduleBase,
        moduleEnd - moduleBase,
        pDyn,
        0,
        RoModule::InitializeSelfError
    );
    util::GetReference(g_AutoLoadList).InsertBack(util::GetPointer(rocrt::g_RoModule));

    // search for other modules
    ForEachRegion<svc::QueryMemory>(0, [&](const svc::MemoryInfo& memoryInfo) -> void {
        if ((memoryInfo.permission & svc::MemoryPermission_Execute) != 0 && memoryInfo.state == svc::MemoryState_Code && memoryInfo.address != moduleBase) {
            const rocrt::ModuleHeader* header = nullptr;
            rocrt::ModuleVersion version{};
            NN_ASSERT(FindModuleHeader<svc::QueryMemory>(&header, &version, memoryInfo.address));

            if (header->bssStartOffset != header->bssEndOffset) {
                void* bss = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(header) + header->bssStartOffset);
                memset(bss, 0, header->bssEndOffset - header->bssStartOffset);
            }

            auto pModule = reinterpret_cast<RoModule*>(reinterpret_cast<uintptr_t>(header) + header->roMduleOffset);
            pModule->Reset();
            
            const auto moduleSize = util::AlignUp(reinterpret_cast<uintptr_t>(header) + header->bssEndOffset, 0x1000ul) - memoryInfo.address;
            NN_ASSERT(moduleSize >= memoryInfo.size);

            pModule->Initialize(
                memoryInfo.address,
                moduleSize,
                reinterpret_cast<Elf64_Dyn*>(reinterpret_cast<uintptr_t>(header) + header->dynamicOffset),
                0,
                RoModule::InitializeError
            );

            pModule->FixRelativeRelocations(Unexpected);

            util::GetReference(g_AutoLoadList).InsertBack(pModule);
        }
    });

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

void RoModule::Initialize(uintptr_t start, uintptr_t size, Elf64_Dyn* pDyn, std::uint8_t flags, void (*errorCallback)(Elf64_Sxword tag, Elf64_Xword val)) {
    m_ArchData.moduleSize = size;
    m_Base = start;
    m_ArchData.dyn = pDyn;
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

    void* pPltRel = nullptr;
    Elf64_Dyn* pHash = nullptr;
    Elf64_Dyn* pGnuHash = nullptr;
    for (; pDyn->d_tag != DT_NULL; ++pDyn) {
        switch (pDyn->d_tag) {
            case DT_PLTRELSZ:
                m_ArchData.pltRelocSize = pDyn->d_un.d_val;
                break;
            case DT_PLTGOT:
                m_ArchData.pltGot = reinterpret_cast<Elf64_Xword*>(m_Base + pDyn->d_un.d_ptr);
                break;
            case DT_HASH:
                pHash = pDyn;
                break;
            case DT_STRTAB:
                m_ArchData.strTable = reinterpret_cast<char*>(m_Base + pDyn->d_un.d_ptr);
                break;
            case DT_SYMTAB:
                m_ArchData.symTable = reinterpret_cast<Elf64_Sym*>(m_Base + pDyn->d_un.d_ptr);
                break;
            case DT_RELA:
                m_Rela = reinterpret_cast<Elf64_Rela*>(m_Base + pDyn->d_un.d_ptr);
                break;
            case DT_RELASZ:
                m_ArchData.relaSize = pDyn->d_un.d_val;
                break;
            case DT_RELAENT:
                if (pDyn->d_un.d_val != sizeof(Elf64_Rela)) {
                    errorCallback(DT_RELAENT, pDyn->d_un.d_val);
                }
                break;
            case DT_STRSZ:
                m_ArchData.strTableSize = pDyn->d_un.d_val;
                break;
            case DT_SYMENT:
                if (pDyn->d_un.d_val != sizeof(Elf64_Sym)) {
                    errorCallback(DT_SYMENT, pDyn->d_un.d_val);
                }
                break;
            case DT_INIT:
                m_ArchData.dtInit = reinterpret_cast<void(*)()>(m_Base + pDyn->d_un.d_ptr);
                break;
            case DT_FINI:
                m_ArchData.dtFini = reinterpret_cast<void(*)()>(m_Base + pDyn->d_un.d_ptr);
                break;
            case DT_SONAME:
                m_ArchData.sharedObjectNameOffset = pDyn->d_un.d_val;
                break;
            case DT_REL:
                m_Rel = reinterpret_cast<Elf64_Rel*>(m_Base + pDyn->d_un.d_ptr);
                break;
            case DT_RELSZ:
                m_ArchData.relSize = pDyn->d_un.d_val;
                break;
            case DT_RELENT:
                if (pDyn->d_un.d_val != sizeof(Elf64_Rel)) {
                    errorCallback(DT_RELENT, pDyn->d_un.d_val);
                }
                break;
            case DT_PLTREL:
                if (pDyn->d_un.d_val == DT_RELA) {
                    m_ArchData.flags |= ArchData::Flags_PltRela;
                } else {
                    m_ArchData.flags &= ~ArchData::Flags_PltRela;
                }
                if (pDyn->d_un.d_val != DT_RELA && pDyn->d_un.d_val != DT_REL) {
                    errorCallback(DT_PLTREL, pDyn->d_un.d_val);
                }
                break;
            case DT_JMPREL:
                pPltRel = reinterpret_cast<void*>(m_Base + pDyn->d_un.d_ptr);
                break;
            case DT_BIND_NOW:
                m_ArchData.flags |= ArchData::Flags_BindNow;
                break;
            case DT_FLAGS:
                if (pDyn->d_un.d_val & DF_BIND_NOW) {
                    m_ArchData.flags |= ArchData::Flags_BindNow;
                }
                break;
            case DT_RELRENT:
                if (pDyn->d_un.d_val != sizeof(Elf64_Relr)) {
                    errorCallback(DT_RELRENT, pDyn->d_un.d_val);
                }
                break;
            case DT_GNU_HASH:
                pGnuHash = pDyn;
                break;
            case DT_RELACOUNT:
                m_ArchData.relaEntryCount = pDyn->d_un.d_val;
                break;
            case DT_RELCOUNT:
                m_ArchData.relEntryCount = pDyn->d_un.d_val;
                break;
            case DT_FLAGS_1:
                if (pDyn->d_un.d_val & DF_1_NOW) {
                    m_ArchData.flags |= ArchData::Flags_BindNow;
                }
                break;
            case DT_AARCH64_PAC_PLT:
                errorCallback(DT_AARCH64_PAC_PLT, pDyn->d_un.d_val);
                break;

            case DT_NULL:
            case DT_NEEDED:
            case DT_RPATH:
            case DT_SYMBOLIC:
            case DT_DEBUG:
            case DT_TEXTREL:
            case DT_INIT_ARRAY:
            case DT_FINI_ARRAY:
            case DT_INIT_ARRAYSZ:
            case DT_FINI_ARRAYSZ:
            case DT_RUNPATH:
            case DT_ENCODING:
            case DT_PREINIT_ARRAYSZ:
            case DT_SYMTAB_SHNDX:
            case DT_RELRSZ:
            case DT_RELR:
            // case DT_AARCH64_BTI_PLT:
            // case DT_AARCH64_VARIANT_PCS:
                break;
        }
    }

    m_RelaPlt = static_cast<Elf64_Rela*>(pPltRel);
    if (pGnuHash == nullptr && pHash == nullptr) {
        m_ArchData.defaultPltGot = 0;
        return;
    }
    
    auto flag = m_ArchData.flags;
    if (pGnuHash != nullptr) {
        flag |= ArchData::Flags_GnuHash;
        auto pHashTable = reinterpret_cast<Elf64_Word*>(m_Base + pGnuHash->d_un.d_ptr);
        m_ArchData.bloom = reinterpret_cast<Elf64_Xword*>(pHashTable + 4);
        m_ArchData.bloomSize = pHashTable[2];
        m_ArchData.bloomShift = pHashTable[3];
        m_ArchData.hashTable = pHashTable;
    } else if (pHash != nullptr) {
        flag &= ~ArchData::Flags_GnuHash;
        auto pHashTable = reinterpret_cast<Elf64_Word*>(m_Base + pHash->d_un.d_ptr);
        m_ArchData.nbucket = pHashTable[0];
        m_ArchData.nchain = pHashTable[1];
        m_ArchData.bucket = pHashTable + 2;
        m_ArchData.chain = m_ArchData.bucket + m_ArchData.nbucket;
    }
    m_ArchData.flags = flag;

    m_ArchData.defaultPltGot = 0;
}

} // namespace ro::detail

} // namespace nn