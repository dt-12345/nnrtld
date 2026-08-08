.macro SVC_BEGIN name
	.section ".text", "ax", %progbits
	.global \name
	.hidden \name
	.type \name, %function
	.align 2
	.cfi_startproc
\name:
.endm

.macro SVC_END name
	.cfi_endproc
.endm

SVC_BEGIN _ZN2nn3svc7aarch644lp6419SetMemoryPermissionEPKvmj
	svc 0x2
	ret
SVC_END _ZN2nn3svc7aarch644lp6419SetMemoryPermissionEPKvmj

SVC_BEGIN _ZN2nn3svc7aarch644lp6418SetMemoryAttributeEPKvmjj
	svc 0x3
	ret
SVC_END _ZN2nn3svc7aarch644lp6418SetMemoryAttributeEPKvmjj

SVC_BEGIN _ZN2nn3svc7aarch644lp6411QueryMemoryEPNS2_10MemoryInfoEPjm
	str x1, [sp, #-16]!
	svc 0x6
	ldr x2, [sp]
	str w1, [x2]
	add sp, sp, #0x10
	ret
SVC_END _ZN2nn3svc7aarch644lp6411QueryMemoryEPNS2_10MemoryInfoEPjm

SVC_BEGIN _ZN2nn3svc7aarch644lp645BreakEjmm
	svc 0x26
	ret
SVC_END _ZN2nn3svc7aarch644lp645BreakEjmm

SVC_BEGIN _ZN2nn3svc7aarch644lp6417OutputDebugStringEPKcm
	svc 0x27
	ret
SVC_END _ZN2nn3svc7aarch644lp6417OutputDebugStringEPKcm

SVC_BEGIN _ZN2nn3svc7aarch644lp6419ReturnFromExceptionEj
	svc 0x28
	ret
SVC_END _ZN2nn3svc7aarch644lp6419ReturnFromExceptionEj