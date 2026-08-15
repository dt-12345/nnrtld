#include "Config/module.h"

    .section ".text","ax"
	.global _ZN2nn2ro6detail24SetExceptionHandlerReadyEv
    .hidden _ZN2nn2ro6detail24SetExceptionHandlerReadyEv
	.type _ZN2nn2ro6detail24SetExceptionHandlerReadyEv, %function
	.align 2
	.cfi_startproc

_ZN2nn2ro6detail24SetExceptionHandlerReadyEv:
    mov x0, x30
    
    nop

set_ready:
    adr x2, _ZN2nn2ro6detail27g_UserExceptionHandlerReadyE
    mov w1, #0x1
    str w1, [x2]
    mov x30, x0
    ret

    .cfi_endproc
    SIZE(_ZN2nn2ro6detail24SetExceptionHandlerReadyEv)

    .section ".data","a"
    .global _ZN2nn2ro6detail27g_UserExceptionHandlerReadyE
    .hidden _ZN2nn2ro6detail27g_UserExceptionHandlerReadyE
    .align 2
_ZN2nn2ro6detail27g_UserExceptionHandlerReadyE:
    .word 0