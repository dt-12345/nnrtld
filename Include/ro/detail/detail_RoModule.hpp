#pragma once

#include "diag.hpp"
#include "ro/detail/detail_ArchData.hpp"
#include "util.hpp"

#include "elf.h"

namespace nn::ro::detail {

using AbortFunc = void (const char*);

class RoModule {
public:
    RoModule() = default;

    void Initialize(uintptr_t start, uintptr_t end, Elf64_Dyn* dyn);

    void FixRelativeRelocations(AbortFunc abort_func);
    void InitRoSymbols();
    void Relocate(AbortFunc abort_func);

    uintptr_t BindJumpSlot(std::uint32_t);

    Elf64_Sym* GetSymbolByName(const char* name) const;

    Elf64_Sym* GetNonLocalSymbol(const char* name) const {
        if (auto sym = GetSymbolByName(name); sym != nullptr && ELF64_ST_BIND(sym->st_info) != STB_LOCAL) {
            return sym;
        }

        return nullptr;
    }

    constexpr uint64_t GetUnk20() const {
        return _20;
    }

    constexpr bool IsPltRela() const {
        return (m_ArchData.flags & ArchData::Flags_PltRela) != 0;
    }

    constexpr bool IsBindNow() const {
        return (m_ArchData.flags & ArchData::Flags_BindNow) != 0;
    }

    constexpr bool HasUnresolved() const {
        return (m_ArchData.flags & ArchData::Flags_HasUnresolved) != 0;
    }

    constexpr bool IsGnuHash() const {
        return (m_ArchData.flags & ArchData::Flags_GnuHash) != 0;
    }

    static constexpr size_t GetListNodeOffset() {
        return 0;
    }

    constexpr uintptr_t GetBase() const {
        return m_Base;
    }

    constexpr const ArchData& GetArchData() const {
        return m_ArchData;
    }

    constexpr const char* GetName() const {
        return m_ArchData.strTable + m_ArchData.sharedObjectNameOffset;
    }

private:
    bool TryResolveSymbol(uintptr_t* target, Elf64_Sym* sym, bool* is_manual) const;
    void LogUnresolvedSymbol(const Elf64_Sym* sym) const;
    void RtldLogUnresolvedSymbol(const Elf64_Sym* sym) const;

    void SetSymbol(const char* name, uintptr_t address);

    void FixRelativeRel(const Elf64_Rel* rel, AbortFunc abort_func);
    void FixRelativeRela(const Elf64_Rela* rel, AbortFunc abort_func);
    void FixRelativeRelr(const Elf64_Dyn* dyn, AbortFunc abort_func);
    
    void RelocateRel(const Elf64_Rel* rel, AbortFunc abort_func);
    void RelocateRela(const Elf64_Rela* rel, AbortFunc abort_func);
    void RelocatePltRel(const Elf64_Rel* rel, AbortFunc abort_func);
    void RelocatePltRela(const Elf64_Rela* rel, AbortFunc abort_func);

    uintptr_t BindJumpSlotRel(std::uint32_t index);
    uintptr_t BindJumpSlotRela(std::uint32_t index);

    util::IntrusiveListNode m_ListNode;
    union {
        Elf64_Rela* m_RelaPlt;
        Elf64_Rel* m_RelPlt;
    };
    union {
        Elf64_Rela* m_Rela;
        Elf64_Rel* m_Rel;
    };
    uint64_t _20; // if non-zero, ProtectRelro returns early
    uintptr_t m_Base;
    ArchData m_ArchData{};
};
static_assert(sizeof(RoModule) == 0xd0);

} // namespace nn::ro::detail