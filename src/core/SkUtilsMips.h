
/*
 * Copyright 2013 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkUtilsMips_DEFINED
#define SkUtilsMips_DEFINED

#include "SkUtils.h"

// Define SK_MIPS_DSP_MODE to one of the following values
// corresponding respectively to:
// - No MIPS DSP support at all  (don't have DSP)
// - Full MIPS DSP support (i.e. assume the CPU always supports it)
// - Optional MIPS DSP support (i.e. probe CPU at runtime)
//
#define SK_MIPS_DSP_MODE_NONE       0
#define SK_MIPS_DSP_MODE_ALWAYS     1
#define SK_MIPS_DSP_MODE_DYNAMIC    2

#if defined(SK_CPU_MIPS) && defined(__MIPS_HAVE_OPTIONAL_DSP_SUPPORT)
#  define SK_MIPS_DSP_MODE  SK_MIPS_DSP_MODE_DYNAMIC
#elif defined(SK_CPU_MIPS) && defined(__MIPS_HAS_DSP)
#  define SK_MIPS_DSP_MODE  SK_MIPS_DSP_MODE_ALWAYS
#else
#  define SK_MIPS_DSP_MODE  SK_MIPS_DSP_MODE_NONE
#endif

// Convenience test macros, always defined as 0 or 1
#define SK_MIPS_DSP_IS_NONE      (SK_MIPS_DSP_MODE == SK_MIPS_DSP_MODE_NONE)
#define SK_MIPS_DSP_IS_ALWAYS    (SK_MIPS_DSP_MODE == SK_MIPS_DSP_MODE_ALWAYS)
#define SK_MIPS_DSP_IS_DYNAMIC   (SK_MIPS_DSP_MODE == SK_MIPS_DSP_MODE_DYNAMIC)

// The sk_cpu_mips_has_dspr2() function returns true iff the target device
// supports dspr2 instructions. In DYNAMIC mode, this actually
// probes the CPU at runtime (and caches the result).

#if SK_MIPS_DSP_IS_NONE
static bool sk_cpu_mips_has_dsp(void) {
    return false;
}
#elif SK_MIPS_DSP_IS_ALWAYS
static inline bool sk_cpu_mips_has_dsp(void) {
    return true;
}
#else // SK_MIPS_DSP_IS_DYNAMIC
extern inline bool sk_cpu_mips_has_dsp(void) SK_PURE_FUNC;
#endif

// Use SK_MIPS_DSP_WRAP(symbol) to map 'symbol' to a DSP-specific symbol
// when applicable. This will transform 'symbol' differently depending on
// the current DSPR2 configuration, i.e.:
//
//    NONE           -> 'symbol'
//    ALWAYS         -> 'symbol_mips_dsp'
//    DYNAMIC        -> 'symbol' or 'symbol_mips_dsp' depending on runtime check.
//
// The goal is to simplify user code, for example:
//
//      return SK_MIPS_DSP_WRAP(do_something)(params);
//
// Replaces the equivalent:
//
//     #if SK_MIPS_DSP_IS_NONE
//       return do_something(params);
//     #elif SK_MIPS_DSP_IS_ALWAYS
//       return do_something_mips_dsp(params);
//     #elif SK_MIPS_DSP_IS_DYNAMIC
//       if (sk_cpu_mips_has_dsp())
//         return do_something_mips_dsp(params);
//       else
//         return do_something(params);
//     #endif
//
#if SK_MIPS_DSP_IS_NONE
#  define SK_MIPS_DSP_WRAP(x)   (x)
#elif SK_MIPS_DSP_IS_ALWAYS
#  define SK_MIPS_DSP_WRAP(x)   (x ## _mips_dsp)
#elif SK_MIPS_DSP_IS_DYNAMIC
#  define SK_MIPS_DSP_WRAP(x)   (sk_cpu_mips_has_dsp() ? x ## _mips_dsp : x)
#endif

#endif // SkUtilsMips_DEFINED
