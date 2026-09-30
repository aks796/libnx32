// The C half of the AArch32 exception entry (exception32.s). Not named
// exception32.c: its object would overwrite the entry's exception32.o.
#ifndef __ARM_ARCH_ISA_A64
#include "types.h"
#include "result.h"
#include "arm/exception32.h"

_Static_assert(sizeof(ThreadExceptionFrame32) == 288, "must match exception32.s");
_Static_assert(sizeof(ThreadExceptionInfo32) == 0x44, "the kernel's 32-bit ExceptionInfo");

Result __attribute__((weak)) __libnx_exception_handler32(u32 type, ThreadExceptionInfo32* info, ThreadExceptionFrame32* frame);

Result __libnx_exception_dispatch32(u32 type, ThreadExceptionInfo32* info, ThreadExceptionFrame32* frame)
{
    if (__libnx_exception_handler32)
        return __libnx_exception_handler32(type, info, frame);
    // No handler: the kernel ends the process and Atmosphère reports the fault.
    return 0xF801;
}
#endif
