#pragma once

#include "diag.hpp"
#include "ro/detail/detail_ArchData.hpp"
#include "util.hpp"

#include "elf.h"

namespace nn::ro::detail {

class RoModule {
public:
    using WarningFunc = void (const char*);
    using ErrorFunc = __attribute__((__noreturn__)) void (const char*);

    RoModule() = default;

    void Reset() {
        m_ListNode = {};
    }

    void Initialize(uintptr_t start, uintptr_t size, Elf64_Dyn* pDyn, std::uint8_t flags, void (*errorCallback)(std::uint32_t code));

    void FixRelativeRelocations(ErrorFunc errorCallback);
    void Relocate(
        bool lazy,
        void (*jumpSlotResolver)(),
        uintptr_t (*lookupAuto)(const char*),
        uintptr_t (*lookupManual)(const RoModule*, const char*),
        bool debug,
        WarningFunc warningCallback,
        ErrorFunc errorCallback
    );

    uintptr_t BindJumpSlot(std::uint32_t);

    Elf64_Sym* LookupSymbol(const char* name) const;

    Elf64_Sym* GetNonLocalSymbol(const char* name) const {
        if (auto sym = LookupSymbol(name); sym != nullptr && ELF64_ST_BIND(sym->st_info) != STB_LOCAL) {
            return sym;
        }

        return nullptr;
    }

    [[nodiscard]] constexpr uint64_t GetUnk20() const { return _20; }

    [[nodiscard]] constexpr bool IsPltRela() const { return (m_ArchData.flags & ArchData::Flags_PltRela) != 0; }
    [[nodiscard]] constexpr bool IsBindNow() const { return (m_ArchData.flags & ArchData::Flags_BindNow) != 0; }
    [[nodiscard]] constexpr bool HasUnresolved() const { return (m_ArchData.flags & ArchData::Flags_HasUnresolved) != 0; }
    [[nodiscard]] constexpr bool IsGnuHash() const { return (m_ArchData.flags & ArchData::Flags_GnuHash) != 0; }

    [[nodiscard]] constexpr uintptr_t GetBase() const { return m_Base; }
    [[nodiscard]] constexpr const ArchData& GetArchData() const { return m_ArchData; }
    [[nodiscard]] constexpr const char* GetName() const { return m_ArchData.strTable + m_ArchData.sharedObjectNameOffset; }

    void SetSymbol(const char* name, uintptr_t address) {
        if (auto sym = GetNonLocalSymbol(name)) {
            *reinterpret_cast<uintptr_t*>(m_Base + sym->st_value) = address;
        }
    }

    static void InitializeSelfError(std::uint32_t);
    static void InitializeError(std::uint32_t);

    [[nodiscard]] static constexpr size_t GetListNodeOffset() { return 0; }

private:
    bool TryResolveSymbol(
        uintptr_t* pOutTarget,
        const Elf64_Sym* pSym,
        bool* pOutManual,
        void (*jumpSlotResolver)(),
        uintptr_t (*lookupAuto)(const char*),
        uintptr_t (*lookupManual)(const RoModule*, const char*)
    ) const;
    void LogUnresolvedSymbol(const Elf64_Sym* sym) const;
    void RtldLogUnresolvedSymbol(const Elf64_Sym* sym) const;

    void FixRelativeRel(const Elf64_Rel* pRel, ErrorFunc errorCallback);
    void FixRelativeRela(const Elf64_Rela* pRel, ErrorFunc errorCallback);
    void FixRelativeRelr(const Elf64_Dyn* pDyn, ErrorFunc errorCallback);
    
    void RelocateRel(
        const Elf64_Rel* pRel,
        void (*jumpSlotResolver)(),
        uintptr_t (*lookupAuto)(const char*),
        uintptr_t (*lookupManual)(const RoModule*, const char*),
        bool debug,
        WarningFunc warningCallback,
        ErrorFunc errorCallback
    );
    void RelocateRela(
        const Elf64_Rela* pRel,
        void (*jumpSlotResolver)(),
        uintptr_t (*lookupAuto)(const char*),
        uintptr_t (*lookupManual)(const RoModule*, const char*),
        bool debug,
        WarningFunc warningCallback,
        ErrorFunc errorCallback
    );
    void RelocatePltRel(
        const Elf64_Rel* pRel,
        bool lazy,
        void (*jumpSlotResolver)(),
        uintptr_t (*lookupAuto)(const char*),
        uintptr_t (*lookupManual)(const RoModule*, const char*),
        bool debug,
        WarningFunc warningCallback,
        ErrorFunc errorCallback
    );
    void RelocatePltRela(
        const Elf64_Rela* rel,
        bool lazy,
        void (*jumpSlotResolver)(),
        uintptr_t (*lookupAuto)(const char*),
        uintptr_t (*lookupManual)(const RoModule*, const char*),
        bool debug,
        WarningFunc warningCallback,
        ErrorFunc errorCallback
    );

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