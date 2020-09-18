#pragma once

#include <malloc.h>
#include <stdlib.h>
#include <sys/cdefs.h>

// FIXME - get from external/scudo
#ifndef M_THREAD_DISABLE_MEM_INIT
#define M_THREAD_DISABLE_MEM_INIT -103
#endif

#ifdef __BIONIC__
// FIXME - move to library
// FIXME - get current value?
struct WithoutHeapZeroing {
    WithoutHeapZeroing() {
        int ret = mallopt(M_THREAD_DISABLE_MEM_INIT, 1);
        if (ret != 1) {
            abort();
        }
    };
    ~WithoutHeapZeroing() {
        int ret = mallopt(M_THREAD_DISABLE_MEM_INIT, 0);
        if (ret != 1) {
            abort();
        }
    };
};

#define SCOPE_WITHOUT_HEAP_ZEROING auto __no_heap_init_##__LINE__ = WithoutHeapZeroing()
#else
#define SCOPE_WITHOUT_HEAP_ZEROING
#endif
