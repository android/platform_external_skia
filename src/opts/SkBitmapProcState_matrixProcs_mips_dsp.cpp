/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmapProcState.h"
#include "SkPerspIter.h"
#include "SkShader.h"
#include "SkUtilsMips.h"

extern const SkBitmapProcState::MatrixProc ClampX_ClampY_Procs_mips_dsp[];
extern const SkBitmapProcState::MatrixProc RepeatX_RepeatY_Procs_mips_dsp[];

static void decal_nofilter_scale_mips_dsp(uint32_t dst[], SkFixed fx, SkFixed dx, int count);
static void decal_filter_scale_mips_dsp(uint32_t dst[], SkFixed fx, SkFixed dx, int count);

static unsigned SK_USHIFT16(unsigned x) {
    return x >> 16;
}
#define MAKENAME(suffix)        ClampX_ClampY ## suffix ## _mips_dsp
#define TILEX_PROCF(fx, max)    SkClampMax((fx) >> 16, max)
#define TILEY_PROCF(fy, max)    SkClampMax((fy) >> 16, max)
#define TILEX_LOW_BITS(fx, max) (((fx) >> 12) & 0xF)
#define TILEY_LOW_BITS(fy, max) (((fy) >> 12) & 0xF)
#define CHECK_FOR_DECAL
#include "SkBitmapProcState_matrix_clamp_mips_dsp.h"

#define MAKENAME(suffix)        RepeatX_RepeatY ## suffix ## _mips_dsp
#define TILEX_PROCF(fx, max)    SK_USHIFT16(((fx) & 0xFFFF) * ((max) + 1))
#define TILEY_PROCF(fy, max)    SK_USHIFT16(((fy) & 0xFFFF) * ((max) + 1))
#define TILEX_LOW_BITS(fx, max) ((((fx) & 0xFFFF) * ((max) + 1) >> 12) & 0xF)
#define TILEY_LOW_BITS(fy, max) ((((fy) & 0xFFFF) * ((max) + 1) >> 12) & 0xF)
#include "SkBitmapProcState_matrix_repeat_mips_dsp.h"


void decal_nofilter_scale_mips_dsp(uint32_t dst[], SkFixed fx, SkFixed dx, int count)
{
    register int t1, t2, t3, t4;
    __asm__ __volatile__ (
        "addu        %[t1], %[fx], %[dx]          \n\t"         // fx+dx
        "addu        %[t2], %[dx], %[dx]          \n\t"         //
        "srl         %[t3], %[count], 1           \n\t"         //
        "blez        %[t3], 2f                    \n\t"         // check for count <= 0
        "1:                                       \n\t"
        "pref           1,  64(%[dst])            \n\t"
#ifdef SK_CPU_BENDIAN
        "precrq.ph.w %[t4], %[fx], %[t1]          \n\t"         // fx || (fx+dx)
#else // SK_CPU_LENDIAN
        "precrq.ph.w %[t4], %[t1], %[fx]          \n\t"         // (fx+dx) || fx
#endif // SK_CPU_BENDIAN
        "sw          %[t4], 0(%[dst])             \n\t"         // store to dst[]
        "addu        %[fx], %[fx], %[t2]          \n\t"         // fx += 2*dx
        "addu        %[t1], %[t1], %[t2]          \n\t"         // (fx+dx) += 2*dx
        "subu        %[t3], %[t3], 1              \n\t"         //
        "addu        %[dst], %[dst], 4            \n\t"         // dst += 4
        "bnez        %[t3], 1b                    \n\t"         //
        "2:                                       \n\t"
        "and         %[t4], %[count], 1           \n\t"         //
        "beqz        %[t4], 3f                    \n\t"         //
        "sra         %[fx], %[fx], 16             \n\t"         //
        "sh          %[fx], 0(%[dst])             \n\t"         //
        "3:                                       \n\t"
        : [t1] "=&r" (t1), [t2] "=&r" (t2), [t3] "=&r" (t3), [t4] "=&r" (t4),
          [fx] "+r" (fx), [dst] "+r" (dst)
        : [count] "r" (count), [dx] "r" (dx)
        : "memory"
    );
}

void decal_filter_scale_mips_dsp(uint32_t dst[], SkFixed fx, SkFixed dx, int count)
{
    register int t1, t2, t3;
    __asm__ __volatile__ (
        "sll    %[t1], %[count], 2        \n\t"         // count*sizeof(int)
        "blez   %[count], 2f              \n\t"         // check for count <= 0
        "addu   %[t1], %[dst], %[t1]      \n\t"         // final address of dst
        "1:                               \n\t"
        "sra    %[t2], %[fx], 12          \n\t"         //
        "sll    %[t2], %[t2], 14          \n\t"         //
        "sra    %[t3], %[fx], 16          \n\t"         //
        "addu   %[t3], %[t3], 1           \n\t"         //
        "addu   %[fx], %[fx], %[dx]       \n\t"         // fx += dx
        "addu   %[dst], %[dst], 4         \n\t"         // dst += 4
        "or     %[t3], %[t2], %[t3]       \n\t"         //
        "sw     %[t3], -4(%[dst])         \n\t"         // store to dst[]
        "bne    %[dst], %[t1], 1b         \n\t"         //
        "2:                               \n\t"
        : [t1] "=&r" (t1), [t2] "=&r" (t2), [t3] "=&r" (t3),
          [fx] "+r" (fx), [dst] "+r" (dst)
        : [count] "r" (count), [dx] "r" (dx)
        : "memory"
    );
}
