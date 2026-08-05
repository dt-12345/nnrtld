#include "rocrt.hpp"

namespace nn::rocrt::detail {

namespace {
void Error() { while (true) { /* ... */ } }
} // anonymous namespace

void Initialize(uintptr_t aslr_base, Elf64_Dyn* dyn) {
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

} // namespace nn::rocrt::detail