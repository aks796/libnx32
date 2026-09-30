@ exception32.s -- the AArch32 user-mode exception entry.
@
@ On a fault the kernel (Mesosphere, 32-bit processes) saves r0-r7, sp, lr, pc,
@ pstate, esr and far in the thread's ExceptionInfo, and re-enters the program
@ at its entry point with r0 = exception type, r1 = &ExceptionInfo and a stack
@ of under 448 bytes (the tail of the process-local region). r8-r12 and the
@ VFP/NEON registers still hold the faulting code's values, and
@ svcReturnFromException restores only what the kernel saved. crt0 branches
@ here when r0 != 0 and r1 != -1.
@
@ This entry moves to its own stack, saves r8-r12, d0-d31 and FPSCR, and calls
@ __libnx_exception_dispatch32(type, info, frame) (exception32.c), which runs
@ the program's __libnx_exception_handler32 if it has one. Everything is then
@ restored and svcReturnFromException(result) ends the exception: 0 resumes
@ the thread with the (possibly changed) ExceptionInfo, anything else lets the
@ kernel end the process (Atmosphère writes its crash report). The kernel runs
@ one user exception per process at a time, so one static stack is enough.
@
@ Frame layout (ThreadExceptionFrame32, arm/exception32.h):
@   +0 fpscr, pad   +8 d16-d31   +136 d0-d15   +264 r8-r12, lr
@
@ Hardware-proven in the 32-bit ports (Disney Crossy Road, PvZ Touch, ...).

    .syntax unified
    .arm
    .fpu neon-fp-armv8

    .section .text.__libnx_exception_entry, "ax", %progbits
    .weak __libnx_exception_entry
    .type __libnx_exception_entry, %function
    .align 2
__libnx_exception_entry:
    ldr     r2, 1f
0:  add     r2, pc, r2              @ r2 = __libnx_exception_stack_end (pc-relative)
    mov     sp, r2
    push    {r8-r12, lr}
    vpush   {d0-d15}
    vpush   {d16-d31}
    vmrs    r2, fpscr
    push    {r2, r3}
    mov     r2, sp                  @ r0 = type, r1 = info, r2 = frame
    bl      __libnx_exception_dispatch32
    mov     r4, r0                  @ the result; the kernel restores r4 from info
    pop     {r2, r3}
    vmsr    fpscr, r2
    vpop    {d16-d31}
    vpop    {d0-d15}
    pop     {r8-r12, lr}
    mov     r0, r4
    svc     0x28                    @ svcReturnFromException(result): does not return
    b       .
    .align 2
1:  .word   __libnx_exception_stack_end - (0b + 8)
    .size __libnx_exception_entry, . - __libnx_exception_entry

    .section .bss.__libnx_exception_stack, "aw", %nobits
    .align 4
__libnx_exception_stack:
    .space  0x8000
__libnx_exception_stack_end:
