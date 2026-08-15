#include "Config/module.h"

.section ".text", "ax", %progbits
.global strcmp
.hidden strcmp
.type strcmp, %function
.align 4
.cfi_startproc

strcmp:
    eor w8, w1, w0
    and x8, x8, #0x7
    cbz x8, .Lprocess8
    ldrb w8, [x0]
    ldrb w9, [x1]
    cmp w8, #0x0
    ccmp w8, w9, #0x0, ne
    b.ne .Lfinish
.Lprocess1:
    add x0, x0, #0x1
    add x1, x1, #0x1
    ldrb w8, [x0]
    ldrb w9, [x1]
    cmp w8, #0x0
    ccmp w8, w9, #0x0, ne
    b.eq .Lprocess1
.Lfinish: 
    sub w0, w8, w9
    ret
.Ladvance: 
    ldrb w8, [x0], #0x1
    ldrb w9, [x1], #0x1
    cmp w8, #0x0
    ccmp w8, w9, #0x0, ne
    b.ne .Lfinish
.Lprocess8:
    and x8, x0, #0x7
    cbnz x8, .Ladvance
    ldrb w8, [x0]
    ldrb w9, [x1]
    cmp w8, #0x0
    ccmp w8, w9, #0x0, ne
    b.ne .Lfinish
    orr x10, xzr, #-0x101010101010102
    mov w8, wzr
    movk x10, #0xfeff
.Lcheck8:
    ldr x8, [x0], #0x8
    add x11, x8, x10
    ldr x9, [x1], #0x8
    eor x12, x9, x8
    bic x11, x11, x8
    and x11, x11, #-0x7f7f7f7f7f7f7f80
    orr x11, x11, x12
    cbz x11, .Lcheck8
    cbnz x12, .Lcalc_diff
    mov w0, wzr
    ret
.Lcalc_diff:
    rbit x10, x11
    clz x10, x10
    and x10, x10, #0x78
    lsr x8, x8, x10
    and w8, w8, #0xff
    lsr x9, x9, x10
    sub w0, w8, w9, UXTB 
    ret

.cfi_endproc
SIZE(strcmp)