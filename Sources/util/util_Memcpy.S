.extern __memcpy_aarch64

.section ".text","ax"
.global memcpy
.hidden memcpy
.type memcpy, %function
.align 4
.cfi_startproc

memcpy:
    b __memcpy_aarch64

.cfi_endproc