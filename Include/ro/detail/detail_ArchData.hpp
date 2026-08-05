#pragma once

#include "types.hpp"

#include "elf.h"

namespace nn::ro::detail {

#if defined(__aarch64__)

namespace aarch64 {

struct ArchData {
    Elf64_Dyn* dyn;
    Elf64_Xword pltRelocSize;
    void (*dtInit)(void);
    void (*dtFini)(void);
    union {
        Elf64_Word* bucket;     // normal hash
        Elf64_Xword* bloom;     // GNU hash
    };
    union {
        Elf64_Word* chain;      // normal hash
        Elf64_Word bloomSize;   // GNU hash
    };
    char* strTable;
    Elf64_Sym* symTable;
    Elf64_Xword strTableSize;
    Elf64_Xword* pltGot;
    Elf64_Xword relaSize;
    Elf64_Xword relSize;
    Elf64_Xword relEntryCount;
    Elf64_Xword relaEntryCount;
    union {
        Elf64_Xword nchain;     // normal hash
        Elf64_Xword bloomShift; // GNU hash
    };
    union {
        Elf64_Xword nbucket;    // normal hash
        Elf64_Word* hashTable;  // GNU hash
    };
    Elf64_Xword sharedObjectNameOffset;
    uintptr_t moduleSize;
    uint8_t _90; // 0x14
    char _91[2];
    uint8_t flags;
    uintptr_t defaultPltGot;

    enum Flags {
        Flags_PltRela       = 1 << 1, // plt uses DT_RELA
        Flags_BindNow       = 1 << 2, // DT_BIND_NOW
        Flags_HasUnresolved = 1 << 3, // unresolvable symbol
        Flags_GnuHash       = 1 << 4, // DT_GNU_HASH
    };
};

} // namespace aarc64

using ArchData = aarch64::ArchData;

#else
#error "Unsupported architecture"
#endif

} // namespace nn::ro::detail