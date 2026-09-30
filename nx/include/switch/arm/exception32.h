/**
 * @file exception32.h
 * @brief AArch32 user-mode exception handling (libnx32).
 * @copyright libnx Authors
 */
#pragma once
#include "../types.h"

#ifndef __ARM_ARCH_ISA_A64

/// What the kernel saved for the faulting thread. svcReturnFromException(0)
/// resumes it with these values, so a handler may change them (pc, sp, r0-r7...).
typedef struct {
    u32 r[8];        ///< r0-r7
    u32 sp;
    u32 lr;
    u32 pc;
    u32 flags;
    u32 pstate;      ///< CPSR (bit 5: Thumb)
    u32 afsr0;
    u32 afsr1;
    u32 esr;
    u32 far;         ///< Fault address
} ThreadExceptionInfo32;

/// What __libnx_exception_entry saved (the kernel does not): restored before
/// svcReturnFromException, so a handler may change these too.
typedef struct {
    u32 fpscr;
    u32 pad;
    u64 d16_31[16];
    u64 d0_15[16];
    u32 r8_12[5];    ///< r8-r12
    u32 lr_entry;
} ThreadExceptionFrame32;

/// Exception types (r0 at entry).
enum {
    ThreadExceptionType32_InstructionAbort       = 0x100,
    ThreadExceptionType32_DataAbort              = 0x101,
    ThreadExceptionType32_UnalignedInstruction   = 0x102,
    ThreadExceptionType32_UnalignedData          = 0x103,
    ThreadExceptionType32_UndefinedInstruction   = 0x104,
    ThreadExceptionType32_ExceptionInstruction   = 0x105,
    ThreadExceptionType32_MemorySystemError      = 0x106,
    ThreadExceptionType32_FpuException           = 0x200,
};

/**
 * @brief Optional program-defined exception handler, called on a 32 KiB
 * exception stack. Return 0 to resume the thread (with any changes made to
 * \p info and \p frame), or a failure to let the kernel end the process.
 * Without one, every fault ends the process.
 * @note Runs in exception context: avoid locks the faulting thread may hold
 * (newlib's stdio and malloc included).
 */
Result __libnx_exception_handler32(u32 type, ThreadExceptionInfo32* info, ThreadExceptionFrame32* frame);

#endif
