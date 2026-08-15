#include "Config/module.h"

.section ".text", "ax", %progbits
.global strlen
.hidden strlen
.type strlen, %function
.align 4
.cfi_startproc

strlen:
    mov x8, x0
    tst x0, #0x7
    b.eq .Lprocess8_init
    ldrb w8, [x0]
    cbz w8, .Lzero_length
    and x8, x0, #-0x8
    add x9, x0, #0x1
    add x8, x8, #0x8
    tst x9, #0x7
    b.eq .Lprocess8_init
    ldrb w10, [x9]
    cbz w10, .Lfinish
    add x9, x0, #0x2
    tst x9, #0x7
    b.eq .Lprocess8_init
    ldrb w10, [x9]
    cbz w10, .Lfinish
    add x9, x0, #0x3
    tst x9, #0x7
    b.eq .Lprocess8_init
    ldrb w10, [x9]
    cbz w10, .Lfinish
    add x9, x0, #0x4
    tst x9, #0x7
    b.eq .Lprocess8_init
    ldrb w10, [x9]
    cbz w10, .Lfinish
    add x9, x0, #0x5
    tst x9, #0x7
    b.eq .Lprocess8_init
    ldrb w10, [x9]
    cbz w10, .Lfinish
    add x9, x0, #0x6
    tst x9, #0x7
    b.eq .Lprocess8_init
    ldrb w10, [x9]
    cbz w10, .Lfinish
    add x9, x0, #0x7
    tst x9, #0x7
    b.eq .Lprocess8_init
    ldrb w10, [x9]
    cbz w10, .Lfinish
    add x9, x0, #0x8
    tst x9, #0x7
    b.ne .Lfinish
.Lprocess8_init:
    orr x10, xzr, #0x101010101010101
    sub x12, x8,#0x1
    movk x10, #0x100
    mov x11, #0x8080808080808080
.Lprocess8: 
    ldr x13, [x8], #0x8
    mov x9, x12
    sub x12, x10, x13
    orr x13, x12, x13
    add x12, x9, #0x8
    bics xzr, x11, x13
    b.eq .Lprocess8
    mov x10, #-0x8
.Lprocess1:
    ldrb w11, [x8, x10]
    add x9, x9, #0x1
    add x10, x10, #0x1
    cbnz w11, .Lprocess1
.Lfinish:
    sub x0, x9, x0
    ret
.Lzero_length:
    sub x0, x0, x0
    ret

.cfi_endproc
SIZE(strlen)