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

#include "rocrt.AssemblyOffset.h"
#include "Config/module.h"

    .section ".rocrt.init", "ax", %progbits
    .align  2
    .hidden __bss_start
    .hidden __bss_end
    .hidden __eh_frame_hdr_start
    .hidden __eh_frame_hdr_end
    .global _ZN2nn5rocrt10g_RoModuleE
    .hidden _ZN2nn5rocrt10g_RoModuleE
    .global __rocrt
    .hidden __rocrt
    .global __rocrt_ver
    .hidden __rocrt_ver
    .global __rtld_entrypoint
    .hidden __rtld_entrypoint
    .global __rocrt_init
    .hidden __rocrt_init
    .type __rocrt_init, %function
    .cfi_startproc

__rocrt_init:
1:  b __rtld_entrypoint
    .word   __rocrt - 1b
    .word   __rocrt_ver - 1b
    
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

    .cfi_endproc

    .section ".rocrt.initro", "a"
    .align  2
    .global __rocrt_initro
    .hidden __rocrt_initro
__rocrt_initro:
1:  .word   1
    .word   __rocrt - 1b
    .word   __rocrt_ver - 1b

    // TODO: where is the best place to put this?
    .section ".nx_debuglink", "a"
    .word 0
    .word MODULE_NAME_LENGTH
    .ascii MODULE_NAME

    .section ".rocrt.info", "a"
    .align  2
__rocrt:
    .word   NN_ROCRT_MODULE_HEADER_SIGNATURE
    .word   _DYNAMIC - __rocrt
    .word   __bss_start - __rocrt
    .word   __bss_end - __rocrt
    .word   __eh_frame_hdr_start - __rocrt
    .word   __eh_frame_hdr_end - __rocrt
    .word   _ZN2nn5rocrt10g_RoModuleE - __rocrt
    .word   __relro_start - __rocrt
    .word   __full_relro_end - __rocrt
    .word   __nx_debuglink_start - __rocrt
    .word   __nx_debuglink_end - __rocrt
    .word   __note_gnu_build_id_start - __rocrt
    .word   __note_gnu_build_id_end - __rocrt
__rocrt_ver:
    .word   NN_SDK_VERSION_MAJOR
    .word   NN_SDK_VERSION_MINOR
    .word   NN_SDK_VERSION_MICRO

    .section ".bss"
    .balign  NN_ROCRT_ROMODULE_ALIGN
_ZN2nn5rocrt10g_RoModuleE:
    .skip   NN_ROCRT_ROMODULE_SIZE

    .addrsig
    .addrsig_sym __rocrt_init // アドレスが固定のため
    .addrsig_sym __rocrt
    .addrsig_sym __rocrt_ver
    .addrsig_sym _DYNAMIC
    .addrsig_sym __bss_start
    .addrsig_sym __bss_end
    .addrsig_sym __eh_frame_hdr_start
    .addrsig_sym __eh_frame_hdr_end
    .addrsig_sym _ZN2nn5rocrt10g_RoModuleE
    .addrsig_sym __rtld_entrypoint
    