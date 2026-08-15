#include "Config/module.h"

.section ".text", "ax", %progbits
.global memset
.hidden memset
.type memset, %function
.align 4
.cfi_startproc

memset:
    cbz x2, .Lfinish
    add x8, x0, x2
    cmp x2, #0x3
    strb w1, [x0]
    sturb w1, [x8, #-0x1]
    b.cc .Lfinish
    cmp x2, #0x7
    strb w1, [x0, #0x1]
    strb w1, [x0, #0x2]
    sturb w1, [x8, #-0x2]
    sturb w1, [x8, #-0x3]
    b.cc .Lfinish
    cmp x2, #0x9
    strb w1, [x0, #0x3]
    sturb w1, [x8, #-0x4]
    b.cc .Lfinish
    neg w8, w0
    and w9, w1, #0xff
    orr w10, wzr, #0x1010101
    and x8, x8, #0x3
    sub x11, x2, x8
    add x8, x0, x8
    mul w9, w9, w10
    and x11, x11, #-0x4
    add x10, x8, x11
    cmp x11, #0x9
    str w9, [x8]
    stur w9, [x10, #-0x4]
    b.cc .Lfinish
    cmp x11, #0x19
    stp w9, w9, [x8, #0x4]
    stp w9, w9, [x10, #-0xc]
    b.cc .Lfinish
    and x12, x8,#0x4
    stp w9, w9, [x8, #0xc]
    orr x12, x12, #0x18
    stp w9, w9, [x8, #0x14]
    sub x11, x11, x12
    stp w9, w9, [x10, #-0x1c]
    cmp x11, #0x20
    stp w9, w9, [x10, #-0x14]
    b.cc .Lfinish
    orr x9, x9, x9, LSL #0x20
    add x8, x8, x12
.Lprocess32:
    stp x9, x9, [x8]
    sub x11, x11, #0x20
    stp x9, x9, [x8, #0x10]
    add x8, x8, #0x20
    cmp x11, #0x1f
    b.hi .Lprocess32
.Lfinish: 
    ret

.cfi_endproc
SIZE(memset)