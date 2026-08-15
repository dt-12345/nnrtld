#include "Config/module.h"

	.section ".text","ax"
	.global _ZN2nn2ro9BindEntryEv
    .hidden _ZN2nn2ro9BindEntryEv
	.type _ZN2nn2ro9BindEntryEv, %function
	.align 2
	.cfi_startproc

_ZN2nn2ro9BindEntryEv:
    // x16 is a pointer to the resolver slot (.got.plt + 0x10)
    // sp[0] contains a pointer to the .got.plt slot to overwrite
    // sp[1] contains x30 (lr)

    // load .got.plt slot
    ldr x17, [sp]
    // back up registers (we can replace sp[0] since we just loaded it)
    str x29, [sp]
    stp x8, xzr, [sp, #-0x10]!
    stp x6, x7 , [sp, #-0x10]!
    stp x4, x5 , [sp, #-0x10]!
    stp x2, x3 , [sp, #-0x10]!
    stp x0, x1 , [sp, #-0x10]!
    stp q6, q7 , [sp, #-0x20]!
    stp q4, q5 , [sp, #-0x20]!
    stp q2, q3 , [sp, #-0x20]!
    stp q0, q1 , [sp, #-0x20]!
    // update frame pointer
    mov x29, sp
    // calculate .got.plt index
    sub x1, x17, x16
    sub x1, x1, #0x8
    lsr x1, x1, #0x3
    // load module pointer (rtld writes it to .got.plt + 8)
    ldur x0, [x16, #-0x8]
    // resolve symbol
    // this also overwrites the .got.plt entry so we don't need to resolve it again the next time
    bl _ZN2nn2ro6detail8RoModule12BindJumpSlotEj
    // x0 holds the symbol address
    mov x16, x0
    // restore registers
    ldp q0 , q1 , [sp], #0x20
    ldp q2 , q3 , [sp], #0x20
    ldp q4 , q5 , [sp], #0x20
    ldp q6 , q7 , [sp], #0x20
    ldp x0 , x1 , [sp], #0x10
    ldp x2 , x3 , [sp], #0x10
    ldp x4 , x5 , [sp], #0x10
    ldp x6 , x7 , [sp], #0x10
    ldp x8 , xzr, [sp], #0x10
    ldp x29, x30, [sp], #0x10
    // call resolved function
    br x16

    .cfi_endproc
    SIZE(_ZN2nn2ro9BindEntryEv)