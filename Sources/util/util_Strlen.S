.section ".text", "ax", %progbits
.global strlen
.hidden strlen
.type strlen, %function
.align 4
.cfi_startproc

strlen:
    mov x8, x0
    tst x0, #0x7
    b.eq process8_init
    ldrb w8, [x0]
    cbz w8, zero_length
    and x8, x0, #-0x8
    add x9, x0, #0x1
    add x8, x8, #0x8
    tst x9, #0x7
    b.eq process8_init
    ldrb w10, [x9]
    cbz w10, finish
    add x9, x0, #0x2
    tst x9, #0x7
    b.eq process8_init
    ldrb w10, [x9]
    cbz w10, finish
    add x9, x0, #0x3
    tst x9, #0x7
    b.eq process8_init
    ldrb w10, [x9]
    cbz w10, finish
    add x9, x0, #0x4
    tst x9, #0x7
    b.eq process8_init
    ldrb w10, [x9]
    cbz w10, finish
    add x9, x0, #0x5
    tst x9, #0x7
    b.eq process8_init
    ldrb w10, [x9]
    cbz w10, finish
    add x9, x0, #0x6
    tst x9, #0x7
    b.eq process8_init
    ldrb w10, [x9]
    cbz w10, finish
    add x9, x0, #0x7
    tst x9, #0x7
    b.eq process8_init
    ldrb w10, [x9]
    cbz w10, finish
    add x9, x0, #0x8
    tst x9, #0x7
    b.ne finish
process8_init:
    orr x10, xzr, #-0x101010101010102
    sub x11, x8,#0x1
    movk x10, #0xfeff
process8: 
    ldr x12, [x8], #0x8
    mov x9, x11
    add x11, x12, x10
    bic x12, x11, x12
    add x11, x9, #0x8
    tst x12, #-0x7f7f7f7f7f7f7f80
    b.eq process8
    mov x10, #-0x8
process1:
    ldrb w11, [x8, x10]
    add x9, x9, #0x1
    add x10, x10, #0x1
    cbnz w11, process1
finish:
    sub x0, x9, x0
    ret
zero_length:
    sub x0, x0, x0
    ret

.cfi_endproc