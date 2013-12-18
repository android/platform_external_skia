
/*
 * Copyright 2013 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkUtilsMips.h"

#if SK_MIPS_DSP_IS_DYNAMIC

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

int parse_proc_cpuinfo(const char* search_string)
{
  const char* file_name = "/proc/cpuinfo";
  char cpuinfo_line[256];
  FILE* f = NULL;

  if ((f = fopen(file_name, "r")) != NULL) {
    while (fgets(cpuinfo_line, sizeof(cpuinfo_line), f) != NULL) {
      if (strstr(cpuinfo_line, search_string) != NULL) {
        fclose(f);
        return 1;
      }
    }
    fclose(f);
  }
  /* Did not find string in the proc file, or not Linux ELF. */
  return 0;
}

// A function used to determine at runtime if the target CPU supports
// the MIPS DSP instruction set. This implementation is Linux-specific.
static bool sk_cpu_mips_check_dsp(void) {
    bool result = false;

    // There is no user-accessible CPUID instruction on MIPS that we can use.
    // Instead, we must parse /proc/cpuinfo and look for the 'mips' feature.
    // For example, here's a typical output (for MIPS Malta):
    /*

    system type             : MIPS Malta
    processor               : 0
    cpu model               : MIPS 74Kc V4.9
    BogoMIPS                : 6430.17
    wait instruction        : yes
    microsecond timers      : yes
    tlb_entries             : 32
    extra interrupt vector  : yes
    hardware watchpoint     : yes, count: 4, address/irw mask: [0x0000, 0x0000, 0x0000, 0x0000]
    ASEs implemented        : mips16 dsp
    shadow register sets    : 1
    core                    : 0
    VCED exceptions         : not available
    VCEI exceptions         : not available
    */
#if USE_ANDROID_NDK_CPU_FEATURES

    result = (android_getCpuFeatures() & ANDROID_CPU_MIPS_FEATURE_DSP) != 0;

#else  // USE_ANDROID_NDK_CPU_FEATURES

    bool already_compiling_everything_for_dsp = false;
#if defined(__mips_dsp)
    already_compiling_everything_for_dsp = true;
#endif
    if (already_compiling_everything_for_dsp || parse_proc_cpuinfo ("dsp")) {
        result = true;
    }
#endif  // USE_ANDROID_NDK_CPU_FEATURES
    if (result) {
        SkDebugf("Device supports MIPS DSP instructions!\n");
    } else {
        SkDebugf("Device does NOT support MIPS DSP instructions!\n");
    }
    return result;
}

static pthread_once_t  sOnce;
static bool            sHasMipsDsp;

// called through pthread_once()
void sk_cpu_mips_probe_features(void) {
    sHasMipsDsp = sk_cpu_mips_check_dsp();
}

bool sk_cpu_mips_has_dsp(void) {
    pthread_once(&sOnce, sk_cpu_mips_probe_features);
    return sHasMipsDsp;
}

#endif // SK_MIPS_DSP_IS_DYNAMIC
