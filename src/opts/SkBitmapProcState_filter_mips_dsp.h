
/*
 * Copyright 2013 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkColorPriv.h"

/*
    Filter_32_opaque

    There is no hard-n-fast rule that the filtering must produce
    exact results for the color components, but if the 4 incoming colors are
    all opaque, then the output color must also be opaque. Subsequent parts of
    the drawing pipeline may rely on this (e.g. which blitrow proc to use).
 */

static inline void Filter_32_opaque_mips_dsp(unsigned x, unsigned y,
                                         SkPMColor a00, SkPMColor a01,
                                         SkPMColor a10, SkPMColor a11,
                                         SkPMColor* dstColor) {
    register uint32_t t0, t1, t2, t3, t4, t5, t6;
    register uint32_t t8, t9 = 0, t10 = 0;
    __asm__ __volatile__ (
                 "sll               %[t0], %[x], 4          \n\t"  // t0 = 16x
                 "sll               %[t1], %[y], 4          \n\t"  // t1 = 16y
                 "mul               %[t2], %[x], %[y]       \n\t"  // t2 = xy
                 "subu              %[t3], %[t0], %[t2]     \n\t"  // t3 = 16x - xy
                 "subu              %[t4], %[t1], %[t2]     \n\t"  // t4 = 16y - xy
                 "li                %[t8], 256              \n\t"
                 "subu              %[t6], %[t8], %[t0]     \n\t"  // t6 = 256 - 16x
                 "subu              %[t5], %[t6], %[t4]     \n\t"  // t5 = 256 - 16x - 16y + xy

                 "preceu.ph.qbra    %[t0], %[a00]           \n\t"
                 "preceu.ph.qbra    %[t1], %[a01]           \n\t"
                 "preceu.ph.qbra    %[t6], %[a10]           \n\t"
                 "preceu.ph.qbra    %[t8], %[a11]           \n\t"
                 "mult              %[t0], %[t5]            \n\t"
                 "madd              %[t1], %[t3]            \n\t"
                 "madd              %[t6], %[t4]            \n\t"
                 "madd              %[t8], %[t2]            \n\t"
                 "mflo              %[t9]                   \n\t"  // lo

                 "preceu.ph.qbla    %[t0], %[a00]           \n\t"
                 "preceu.ph.qbla    %[t1], %[a01]           \n\t"
                 "preceu.ph.qbla    %[t6], %[a10]           \n\t"
                 "preceu.ph.qbla    %[t8], %[a11]           \n\t"
                 "mult              %[t0], %[t5]            \n\t"
                 "madd              %[t1], %[t3]            \n\t"
                 "madd              %[t6], %[t4]            \n\t"
                 "madd              %[t8], %[t2]            \n\t"
                 "mflo              %[t10]                  \n\t"  // hi

                 "preceu.ph.qbla    %[t0], %[t9]            \n\t"
                 "preceu.ph.qbla    %[t1], %[t10]           \n\t"
                 "sll               %[t1], %[t1], 8         \n\t"
                 "or                %[dst], %[t0], %[t1]    \n\t"
                 : [t0] "=&r" (t0), [t1] "=&r" (t1), [t2] "=&r" (t2), [t3] "=&r" (t3),
                   [t4] "=&r" (t4), [t5] "=&r" (t5), [t6] "=&r" (t6), [t8] "=&r" (t8),
                   [t9] "+r" (t9), [t10] "+r" (t10), [dst] "=r" (*dstColor)
                 : [x] "r" (x), [y] "r" (y), [a00] "r" (a00), [a01] "r" (a01),
                   [a10] "r" (a10), [a11] "r" (a11)
                 : "hi", "lo", "memory"
                 );
}

static inline void Filter_32_alpha_mips_dsp(unsigned x, unsigned y,
                                        SkPMColor a00, SkPMColor a01,
                                        SkPMColor a10, SkPMColor a11,
                                        SkPMColor* dstColor,
                                        unsigned alphaScale) {
    register uint32_t t0, t1, t2, t3, t4, t5, t6;
    register uint32_t t8, t9 = 0, t10 = 0;
    __asm__ __volatile__ (
                 "sll               %[t0], %[x], 4            \n\t"  // t0 = 16x
                 "sll               %[t1], %[y], 4            \n\t"  // t1 = 16y
                 "mul               %[t2], %[x], %[y]         \n\t"  // t2 = xy
                 "subu              %[t3], %[t0], %[t2]       \n\t"  // t3 = 16x - xy
                 "subu              %[t4], %[t1], %[t2]       \n\t"  // t4 = 16y - xy
                 "li                %[t8], 256                \n\t"
                 "subu              %[t6], %[t8], %[t0]       \n\t"  // t6 = 256 - 16x
                 "subu              %[t5], %[t6], %[t4]       \n\t"  // t5 = 256 - 16x - 16y + xy
                 "preceu.ph.qbra    %[t0], %[a00]             \n\t"
                 "preceu.ph.qbra    %[t1], %[a01]             \n\t"
                 "preceu.ph.qbra    %[t6], %[a10]             \n\t"
                 "preceu.ph.qbra    %[t8], %[a11]             \n\t"
                 "mult              %[t0], %[t5]              \n\t"
                 "madd              %[t1], %[t3]              \n\t"
                 "madd              %[t6], %[t4]              \n\t"
                 "madd              %[t8], %[t2]              \n\t"
                 "mflo              %[t9]                     \n\t"  // lo
                 "preceu.ph.qbla    %[t0], %[a00]             \n\t"
                 "preceu.ph.qbla    %[t1], %[a01]             \n\t"
                 "preceu.ph.qbla    %[t6], %[a10]             \n\t"
                 "preceu.ph.qbla    %[t8], %[a11]             \n\t"
                 "mult              %[t0], %[t5]              \n\t"
                 "madd              %[t1], %[t3]              \n\t"
                 "madd              %[t6], %[t4]              \n\t"
                 "madd              %[t8], %[t2]              \n\t"
                 "mflo              %[t10]                    \n\t"  // hi
                 "preceu.ph.qbla    %[t0], %[t9]              \n\t"
                 "preceu.ph.qbla    %[t1], %[t10]             \n\t"
                 "mul               %[t2], %[t0], %[alpha]    \n\t"
                 "mul               %[t3], %[t1], %[alpha]    \n\t"
                 "preceu.ph.qbla    %[t0], %[t2]              \n\t"
                 "preceu.ph.qbla    %[t1], %[t3]              \n\t"
                 "sll               %[t1], %[t1], 8           \n\t"
                 "or                %[dst], %[t0], %[t1]      \n\t"
                 : [t0] "=&r" (t0), [t1] "=&r" (t1), [t2] "=&r" (t2), [t3] "=&r" (t3),
                   [t4] "=&r" (t4), [t5] "=&r" (t5), [t6] "=&r" (t6), [t8] "=&r" (t8),
                   [t9] "=&r" (t9), [t10] "=&r" (t10), [dst] "=r" (*dstColor)
                 : [x] "r" (x), [y] "r" (y), [a00] "r" (a00), [a01] "r" (a01),
                   [a10] "r" (a10), [a11] "r" (a11), [alpha] "r" (alphaScale)
                 : "hi", "lo", "memory"
                 );
}
