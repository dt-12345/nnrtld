#include "Config/module.h"

.section ".text.crt0", "ax", %progbits
.extern memset
.extern _ZN2nn2ro6detail20InitializeSelfModuleEmP9Elf64_Dyn
.extern _ZN2nn2ro6detail10InitializeEmP9Elf64_Dyn
.extern _ZN2nn2ro6detail31GetSetUserExceptionHandlerReadyEv
.extern _ZN2nn2ro6detail20GetInitializeModulesEv
.extern _ZN2nn2ro6detail18GetFinalizeModulesEv
.extern _ZN2nn4init5StartEmmPFvvES2_S2_
.extern _ZN2nn2ro6detail27g_UserExceptionHandlerReadyE
.extern _ZN2nn2os6detail20UserExceptionHandlerEv
.extern _ZN2nn3svc7aarch644lp6419ReturnFromExceptionEj

.global __rtld_entrypoint
.hidden __rtld_entrypoint

__rtld_entrypoint:
    cmp x0, #0x0    // x0 is zero on NSO entry
    b.ne __rtld_exception
    mov w19, w1     // w1 is the main thread handle

    nop

__rtld_load_bss:
    adr x0, __bss_start

    nop

__rtld_init_bss:
    adr x2, __bss_end
    sub x2, x2, x0
    mov w1, #0x0
    bl memset
    adr x0, __rocrt_init

    nop

__rtld_initialize:
    adr x1, __dynamic_start
    mov x20, x0
    mov x21, x1
    bl _ZN2nn2ro6detail20InitializeSelfModuleEmP9Elf64_Dyn
    mov x0, x20
    mov x1, x21
    bl _ZN2nn2ro6detail10InitializeEmP9Elf64_Dyn
    bl _ZN2nn2ro6detail31GetSetUserExceptionHandlerReadyEv
    mov x20, x0
    bl _ZN2nn2ro6detail20GetInitializeModulesEv
    mov x21, x0
    bl _ZN2nn2ro6detail18GetFinalizeModulesEv
    mov x22, x0
    mov w0, w19

    nop

__rtld_start:
    adr x1, __rtld_arguments
    mov x2, x20
    mov x3, x21
    mov x4, x22
    bl _ZN2nn4init5StartEmmPFvvES2_S2_
    b .

__rtld_exception:
    nop

__rtld_exception_handler:
    adr x2, _ZN2nn2ro6detail27g_UserExceptionHandlerReadyE
    ldr w2, [x2]
    cbz w2, __rtld_throw_exception
    adrp x2, :got:_ZN2nn2os6detail20UserExceptionHandlerEv
    ldr x2, [x2, :got_lo12:_ZN2nn2os6detail20UserExceptionHandlerEv]
    cbz x2, __rtld_throw_exception
    br x2

__rtld_throw_exception:
    mov x0, #0xf801

    nop

__rtld_return_from_exception:
    adr x1, _ZN2nn3svc7aarch644lp6419ReturnFromExceptionEj
    blr x1
    b .     // unreachable infinite loop

    .addrsig_sym __rtld_entrypoint

// TODO: where is the best place to put this?
.section ".nx_debuglink", "a"
    .word 0
    .word MODULE_NAME_LENGTH
    .ascii MODULE_NAME