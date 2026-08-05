/*--------------------------------------------------------------------------------*
  Copyright (C)Nintendo. All rights reserved.

  These coded instructions, statements, and computer programs contain
  information of Nintendo and/or its licensed developers and are protected
  by national and international copyright laws.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *--------------------------------------------------------------------------------*/

/*
  Nintendo puts __attribute__((visibility("hidden"))) on of the linker-provided symbols, but that seems to prevent them from generating a GOT entry
  I've removed those for now, but I will need to see later if there's a way to get them to generate anyways
*/

extern "C" {
    struct AtExitEntry
    {
        void (*func) (void*);
        void* pObject;
        void* pDsoHandle;
    };

    extern AtExitEntry __atexit_start[];
    extern AtExitEntry __atexit_end[];
}

namespace {
    unsigned long g_AtExitEntryCount = 0;

    AtExitEntry* AllocEntry() noexcept
    {
        const unsigned long numAtExitEntry = __atexit_end - __atexit_start;
        if (numAtExitEntry <= g_AtExitEntryCount)
        {
            return nullptr;
        }
        unsigned long index = __sync_fetch_and_add(&g_AtExitEntryCount, 1);
        if (numAtExitEntry <= index)
        {
            return nullptr;
        }
        else
        {
            return &__atexit_start[index];
        }
    }

    void CallFinalize(
        void *pDsoHandle, unsigned long begin, unsigned long end) noexcept
    {
        unsigned long numAtExitEntry = __atexit_end - __atexit_start;
        if (numAtExitEntry >= end)
        {
            numAtExitEntry = end;
        }

        if (numAtExitEntry < begin)
        {
            return;
        }

        for (unsigned long index = numAtExitEntry; index > begin; index--)
        {
            unsigned long newEnd = g_AtExitEntryCount;
            unsigned long newBegin = newEnd;

            AtExitEntry* entry = &__atexit_start[index - 1];

            if (pDsoHandle == entry->pDsoHandle)
            {
                entry->func(entry->pObject);
                newEnd = g_AtExitEntryCount;
            }

            CallFinalize(pDsoHandle, newBegin, newEnd);
        }
    }

    inline int CxaAtExitImpl(
        void (*pDestroyer)(void*), void* pObject, void* pDsoHandle, AtExitEntry* entry) noexcept
    {
        if (entry)
        {
            entry->func = pDestroyer;
            entry->pObject = pObject;
            entry->pDsoHandle = pDsoHandle;
            return 0;
        }
        else
        {
            return -1;
        }
    }

    inline void CxaFinalizeImpl(void* pDsoHandle, unsigned long count) noexcept
    {
        CallFinalize(pDsoHandle, 0, count);
    }

#if __has_feature(ptrauth_init_fini)
    #define INIT_FINI_PTRAUTH_QUALIFIER __ptrauth(0, __has_feature(ptrauth_init_fini_address_discrimination), __builtin_ptrauth_string_discriminator("init_fini"))
#else
    #define INIT_FINI_PTRAUTH_QUALIFIER
#endif

    using InitFiniPointerT = void(*INIT_FINI_PTRAUTH_QUALIFIER)();

// #if defined(__aarch64__) && defined(__LP64__) && !(__has_feature(ptrauth_calls) && __has_feature(ptrauth_init_fini))
//     template<InitFiniPointerT pBegin[], InitFiniPointerT pEnd[]>
//     [[gnu::naked]] void CallFunction(InitFiniPointerT* ppFunc) noexcept
//     {
//         asm volatile(
//             "    ldr  x16, [x0];"
//             "    adrp x8, %[BeginSym];"
//             "    add  x8, x8, :lo12:%[BeginSym];"
//             "    cmp  x0, x8;"
//             "    b.lo 1f;"
//             "    adrp x8, %[EndSym];"
//             "    add  x8, x8, :lo12:%[EndSym];"
//             "    cmp  x0, x8;"
//             "    b.hs 1f;"
//             "    tst  x0, #0x7;"
//             "    b.ne 1f;"
//             "    br   x16;"
//             "1:  udf #0x8003;"::
//             [BeginSym]"S"(pBegin), [EndSym]"S"(pEnd));
//         }
// #else
    template<InitFiniPointerT pBegin[], InitFiniPointerT pEnd[]>
    [[gnu::always_inline]] void CallFunction(InitFiniPointerT* ppFunc) noexcept
    {
        (*ppFunc)();
    }
// #endif
}

namespace nn { namespace rocrt {
    extern unsigned char g_RoModule[];
    
    namespace detail {
        void ProtectRelro(const void* relro, const void* relroEnd, const void* fullRelroEnd, const void* pModule, const void* pVersion) noexcept;
    }

}}

extern "C"
{
    extern InitFiniPointerT __init_array_start[];
    extern InitFiniPointerT __init_array_end[];
    extern InitFiniPointerT __fini_array_start[];
    extern InitFiniPointerT __fini_array_end[];
    extern unsigned char __relro_start[];
    extern unsigned char __relro_end[];
    extern unsigned char __full_relro_end[];
    extern unsigned char __rocrt_ver[];

    extern unsigned char            __EX_start[];
    extern unsigned char            __EX_end[];
    extern unsigned char            __tdata_start[];
    extern unsigned char            __tdata_end[];
    extern unsigned char            __tdata_align_abs[];
    extern unsigned char            __tdata_align_rel[];
    extern unsigned char            __tbss_start[];
    extern unsigned char            __tbss_end[];
    extern unsigned char            __tbss_align_abs[];
    extern unsigned char            __tbss_align_rel[];
    extern unsigned char            __rela_dyn_start[];
    extern unsigned char            __rela_dyn_end[];
    extern unsigned char            __rel_dyn_start[];
    extern unsigned char            __rel_dyn_end[];
    extern unsigned char            __rela_plt_start[];
    extern unsigned char            __rela_plt_end[];
    extern unsigned char            __rel_plt_start[];
    extern unsigned char            __rel_plt_end[];
    extern unsigned char            __got_plt_start[];
    extern unsigned char            __got_plt_end[];
    extern unsigned char            _DYNAMIC[];
    int __nnmusl_init_dso(unsigned char *EX_start, unsigned char *EX_end,
                            unsigned char *tdata_start, unsigned char *tdata_end,
                            unsigned char *tdata_align_abs, unsigned char *tdata_align_rel,
                            unsigned char *tbss_start, unsigned char *tbss_end,
                            unsigned char *tbss_align_abs, unsigned char *tbss_align_rel,
                            unsigned char *got_plt_start, unsigned char *got_plt_end,
                            unsigned char *rela_dyn_start, unsigned char *rela_dyn_end,
                            unsigned char *rel_dyn_start, unsigned char *rel_dyn_end,
                            unsigned char *rela_plt_start, unsigned char *rela_plt_end,
                            unsigned char *rel_plt_start, unsigned char *rel_plt_end,
                            unsigned char *DYNAMIC);
    void __nnmusl_fini_dso(unsigned char *EX_start, unsigned char *EX_end,
                            unsigned char *tdata_start, unsigned char *tdata_end,
                            unsigned char *tbss_start, unsigned char *tbss_end);

    extern void* __dso_handle __attribute__ ((section(".data.rel.ro.__dso_handle")));
    void* __dso_handle __attribute__ ((section(".data.rel.ro.__dso_handle"))) = &__dso_handle;
    int __aeabi_atexit(void* object, void (*destroyer)(void*), void* dso_handle);
    int __cxa_atexit(void (*destroyer)(void*), void* pObject, void* dso_handle);
    int __cxa_finalize(void* pDsoHandle);
    void _init();
    void _fini();
    static volatile int nnmuslTlsInitializationPhase __attribute__((section(".data._ZL28nnmuslTlsInitializationPhase"))) = 0;

    void _init()
    {
        if (nnmuslTlsInitializationPhase == 0)
        {
            nnmuslTlsInitializationPhase = __nnmusl_init_dso( __EX_start, __EX_end,
                                            __tdata_start, __tdata_end,
                                            __tdata_align_abs, __tdata_align_rel,
                                            __tbss_start, __tbss_end,
                                            __tbss_align_abs, __tbss_align_rel,
                                            __got_plt_start, __got_plt_end,
                                            __rela_dyn_start, __rela_dyn_end,
                                            __rel_dyn_start, __rel_dyn_end,
                                            __rela_plt_start, __rela_plt_end,
                                            __rel_plt_start, __rel_plt_end,
                                            static_cast<unsigned char *>(_DYNAMIC) );
            if (nnmuslTlsInitializationPhase == 1)
            {
                return;
            }
        }

        nn::rocrt::detail::ProtectRelro(__relro_start, __relro_end, __full_relro_end, nn::rocrt::g_RoModule, __rocrt_ver);
        for (InitFiniPointerT* f = __init_array_start; f < __init_array_end; ++f)
        {
            CallFunction<__init_array_start, __init_array_end>(f);
        }
    }

    void _fini()
    {
        __cxa_finalize(static_cast<void*>(&__dso_handle));

        for (InitFiniPointerT* f = __fini_array_end; f > __fini_array_start; --f)
        {
            CallFunction<__fini_array_start, __fini_array_end>(f - 1);
        }

        // __nnmusl_fini_dso( __EX_start, __EX_end,
        //                     __tdata_start, __tdata_end,
        //                     __tbss_start, __tbss_end );
    }

    int __aeabi_atexit(void* object, void (*destroyer)(void*), void* dso_handle)
    {
        return __cxa_atexit(destroyer, object, dso_handle);
    }

    int __cxa_atexit(void (*pDestroyer)(void*), void* pObject, void* pDsoHandle)
    {
        return CxaAtExitImpl(pDestroyer, pObject, pDsoHandle, pDsoHandle ? AllocEntry() : nullptr);
    }

    int __cxa_finalize(void* pDsoHandle)
    {
        CxaFinalizeImpl(pDsoHandle, g_AtExitEntryCount);
        return 0;
    }

}

