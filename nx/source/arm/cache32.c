// armICacheInvalidate for AArch32 (libnx32).
//
// A 32-bit EL0 thread has no cache maintenance instructions (32-bit Linux has
// the cacheflush syscall for this). But the kernel invalidates every core's
// instruction cache (IC IALLUIS and a barrier on each core) whenever a Code or
// AliasCode page gains or loses execute permission. So one spare page --
// mapped with svcMapProcessCodeMemory and never executed -- is flipped R <-> RX
// once per call, after the written range is cleaned from the data cache (the
// instruction cache refills from L2 and memory, not from L1 data).
//
// Needs a real process handle for the one-time mapping
// (envAcquireOwnProcessHandle). Without one, only the data-cache clean
// happens. Hardware-proven in the 32-bit ports (Disney Crossy Road's JIT,
// PvZ Touch's hooks).
#ifndef __ARM_ARCH_ISA_A64
#include <malloc.h>
#include <string.h>
#include "types.h"
#include "result.h"
#include "kernel/svc.h"
#include "kernel/mutex.h"
#include "kernel/virtmem.h"
#include "runtime/env.h"

#define PAGE 0x1000

static Mutex g_icLock;
static uintptr_t g_icPage; // 0: not set up yet, 1: unavailable
static bool g_icExec;

static void _icPageInit(void)
{
    g_icPage = 1;
    Handle self = envAcquireOwnProcessHandle();
    if (self == INVALID_HANDLE)
        return;
    void* src = memalign(PAGE, PAGE); // given to the mapping for good
    if (!src)
        return;
    memset(src, 0, PAGE);
    virtmemLock();
    void* dst = virtmemFindCodeMemory(PAGE, PAGE);
    VirtmemReservation* rv = dst ? virtmemAddReservation(dst, PAGE) : NULL;
    virtmemUnlock();
    if (!rv) {
        free(src);
        return;
    }
    Result rc = svcMapProcessCodeMemory(self, (u64)(uintptr_t)dst, (u64)(uintptr_t)src, PAGE);
    if (R_SUCCEEDED(rc))
        rc = svcSetProcessMemoryPermission(self, (u64)(uintptr_t)dst, PAGE, Perm_R);
    if (R_SUCCEEDED(rc)) {
        g_icPage = (uintptr_t)dst;
        g_icExec = false;
    }
}

void armICacheInvalidate(void* addr, size_t size)
{
    if (size)
        svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u64)(uintptr_t)addr, size);
    mutexLock(&g_icLock);
    if (!g_icPage)
        _icPageInit();
    if (g_icPage > 1) {
        g_icExec = !g_icExec;
        if (R_FAILED(svcSetProcessMemoryPermission(CUR_PROCESS_HANDLE, (u64)g_icPage, PAGE, g_icExec ? Perm_Rx : Perm_R)))
            g_icExec = !g_icExec;
    }
    mutexUnlock(&g_icLock);
}
#endif
