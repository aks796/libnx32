// envAcquireOwnProcessHandle: a real handle to the running process.
//
// svcMapProcessCodeMemory, svcMapProcessMemory and svcUnmapProcessCodeMemory
// refuse CUR_PROCESS_HANDLE (the kernel looks those handles up without pseudo-
// handles). A program started by hbloader is given a real one
// (envGetOwnProcessHandle); one started as an NSO or an ExeFS override -- the
// only way to start a 32-bit program -- is not. So one is made the way
// hbloader makes its own: CUR_PROCESS_HANDLE is sent as a copy handle over a
// session to ourselves, IPC handle translation resolves the pseudo-handle, and
// the receiving side ends up holding a real handle to this process. The
// receiver closes the session without replying, which ends the send.
#include "types.h"
#include "result.h"
#include "arm/tls.h"
#include "kernel/svc.h"
#include "kernel/thread.h"
#include "kernel/mutex.h"
#include "sf/hipc.h"
#include "runtime/env.h"

static Mutex g_selfLock;
static Handle g_selfHandle = INVALID_HANDLE;

static void _envSelfReceive(void* arg)
{
    Handle session = (Handle)(uintptr_t)arg;
    void* tls = armGetTls();
    hipcMakeRequestInline(tls);
    s32 idx = 0;
    Result rc = svcReplyAndReceive(&idx, &session, 1, INVALID_HANDLE, UINT64_MAX);
    if (R_SUCCEEDED(rc)) {
        HipcParsedRequest r = hipcParseRequest(tls);
        if (r.meta.num_copy_handles == 1)
            g_selfHandle = r.data.copy_handles[0];
    }
    svcCloseHandle(session);
}

Handle envAcquireOwnProcessHandle(void)
{
    Handle given = envGetOwnProcessHandle(); // from hbloader, when it started us
    if (given != INVALID_HANDLE && given != 0)
        return given;

    mutexLock(&g_selfLock);
    if (g_selfHandle == INVALID_HANDLE) {
        Handle server = INVALID_HANDLE, client = INVALID_HANDLE;
        if (R_SUCCEEDED(svcCreateSession(&server, &client, 0, 0))) {
            Thread t;
            Result rc = threadCreate(&t, _envSelfReceive, (void*)(uintptr_t)server, NULL, 0x4000, 0x20, -2);
            if (R_SUCCEEDED(rc)) {
                rc = threadStart(&t);
                if (R_FAILED(rc))
                    threadClose(&t);
            }
            if (R_SUCCEEDED(rc)) {
                hipcMakeRequestInline(armGetTls(), .num_copy_handles = 1).copy_handles[0] = CUR_PROCESS_HANDLE;
                svcSendSyncRequest(client); // returns once the receiver has closed the session
                svcCloseHandle(client);
                threadWaitForExit(&t);
                threadClose(&t);
            } else {
                svcCloseHandle(server);
                svcCloseHandle(client);
            }
        }
    }
    Handle h = g_selfHandle;
    mutexUnlock(&g_selfLock);
    return h;
}
