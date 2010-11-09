
/*
 * Copyright 2009 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "SkBitmapProcState.h"
#include "SkColorPriv.h"
#include "SkUtils.h"

#if defined(__ARM_HAVE_NEON)
#include <arm_neon.h>
#endif


#if __ARM_ARCH__ >= 6 && !defined(SK_CPU_BENDIAN)
void SI8_D16_nofilter_DX_arm(
    const SkBitmapProcState& s,
    const uint32_t* SK_RESTRICT xy,
    int count,
    uint16_t* SK_RESTRICT colors) __attribute__((optimize("O1")));

void SI8_D16_nofilter_DX_arm(const SkBitmapProcState& s,
                             const uint32_t* SK_RESTRICT xy,
                             int count, uint16_t* SK_RESTRICT colors) {
    SkASSERT(count > 0 && colors != NULL);
    SkASSERT(s.fInvType <= (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask));
    SkASSERT(s.fDoFilter == false);
    
    const uint16_t* SK_RESTRICT table = s.fBitmap->getColorTable()->lock16BitCache();
    const uint8_t* SK_RESTRICT srcAddr = (const uint8_t*)s.fBitmap->getPixels();
    
    // buffer is y32, x16, x16, x16, x16, x16
    // bump srcAddr to the proper row, since we're told Y never changes
    SkASSERT((unsigned)xy[0] < (unsigned)s.fBitmap->height());
    srcAddr = (const uint8_t*)((const char*)srcAddr +
                               xy[0] * s.fBitmap->rowBytes());
    
    uint8_t src;
    
    if (1 == s.fBitmap->width()) {
        src = srcAddr[0];
        uint16_t dstValue = table[src];
        sk_memset16(colors, dstValue, count);
    } else {
        int i;
        int count8 = count >> 3;
        const uint16_t* SK_RESTRICT xx = (const uint16_t*)(xy + 1);
        
        asm volatile (
                      "cmp        %[count8], #0                   \n\t"   // compare loop counter with 0
                      "beq        2f                              \n\t"   // if loop counter == 0, exit
                      "1:                                             \n\t"
                      "ldmia      %[xx]!, {r5, r7, r9, r11}       \n\t"   // load ptrs to pixels 0-7
                      "subs       %[count8], %[count8], #1        \n\t"   // decrement loop counter
                      "uxth       r4, r5                          \n\t"   // extract ptr 0
                      "mov        r5, r5, lsr #16                 \n\t"   // extract ptr 1
                      "uxth       r6, r7                          \n\t"   // extract ptr 2
                      "mov        r7, r7, lsr #16                 \n\t"   // extract ptr 3
                      "ldrb       r4, [%[srcAddr], r4]            \n\t"   // load pixel 0 from image
                      "uxth       r8, r9                          \n\t"   // extract ptr 4
                      "ldrb       r5, [%[srcAddr], r5]            \n\t"   // load pixel 1 from image
                      "mov        r9, r9, lsr #16                 \n\t"   // extract ptr 5
                      "ldrb       r6, [%[srcAddr], r6]            \n\t"   // load pixel 2 from image
                      "uxth       r10, r11                        \n\t"   // extract ptr 6
                      "ldrb       r7, [%[srcAddr], r7]            \n\t"   // load pixel 3 from image
                      "mov        r11, r11, lsr #16               \n\t"   // extract ptr 7
                      "ldrb       r8, [%[srcAddr], r8]            \n\t"   // load pixel 4 from image
                      "add        r4, r4, r4                      \n\t"   // double pixel 0 for RGB565 lookup
                      "ldrb       r9, [%[srcAddr], r9]            \n\t"   // load pixel 5 from image
                      "add        r5, r5, r5                      \n\t"   // double pixel 1 for RGB565 lookup
                      "ldrb       r10, [%[srcAddr], r10]          \n\t"   // load pixel 6 from image
                      "add        r6, r6, r6                      \n\t"   // double pixel 2 for RGB565 lookup
                      "ldrb       r11, [%[srcAddr], r11]          \n\t"   // load pixel 7 from image
                      "add        r7, r7, r7                      \n\t"   // double pixel 3 for RGB565 lookup
                      "ldrh       r4, [%[table], r4]              \n\t"   // load pixel 0 RGB565 from colmap
                      "add        r8, r8, r8                      \n\t"   // double pixel 4 for RGB565 lookup
                      "ldrh       r5, [%[table], r5]              \n\t"   // load pixel 1 RGB565 from colmap
                      "add        r9, r9, r9                      \n\t"   // double pixel 5 for RGB565 lookup
                      "ldrh       r6, [%[table], r6]              \n\t"   // load pixel 2 RGB565 from colmap
                      "add        r10, r10, r10                   \n\t"   // double pixel 6 for RGB565 lookup
                      "ldrh       r7, [%[table], r7]              \n\t"   // load pixel 3 RGB565 from colmap
                      "add        r11, r11, r11                   \n\t"   // double pixel 7 for RGB565 lookup
                      "ldrh       r8, [%[table], r8]              \n\t"   // load pixel 4 RGB565 from colmap
                      "ldrh       r9, [%[table], r9]              \n\t"   // load pixel 5 RGB565 from colmap
                      "ldrh       r10, [%[table], r10]            \n\t"   // load pixel 6 RGB565 from colmap
                      "ldrh       r11, [%[table], r11]            \n\t"   // load pixel 7 RGB565 from colmap
                      "pkhbt      r5, r4, r5, lsl #16             \n\t"   // pack pixels 0 and 1
                      "pkhbt      r6, r6, r7, lsl #16             \n\t"   // pack pixels 2 and 3
                      "pkhbt      r8, r8, r9, lsl #16             \n\t"   // pack pixels 4 and 5
                      "pkhbt      r10, r10, r11, lsl #16          \n\t"   // pack pixels 6 and 7
                      "stmia      %[colors]!, {r5, r6, r8, r10}   \n\t"   // store last 8 pixels
                      "bgt        1b                              \n\t"   // loop if counter > 0
                      "2:                                             \n\t"
                      : [xx] "+r" (xx), [count8] "+r" (count8), [colors] "+r" (colors)
                      : [table] "r" (table), [srcAddr] "r" (srcAddr)
                      : "memory", "cc", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11"
                      );
        
        for (i = (count & 7); i > 0; --i) {
            src = srcAddr[*xx++]; *colors++ = table[src];
        }
    }

    s.fBitmap->getColorTable()->unlock16BitCache(); 
}

void SI8_opaque_D32_nofilter_DX_arm(
    const SkBitmapProcState& s,
    const uint32_t* SK_RESTRICT xy,
    int count,
    SkPMColor* SK_RESTRICT colors) __attribute__((optimize("O1")));

void SI8_opaque_D32_nofilter_DX_arm(const SkBitmapProcState& s,
                                    const uint32_t* SK_RESTRICT xy,
                                    int count, SkPMColor* SK_RESTRICT colors) {
    SkASSERT(count > 0 && colors != NULL);
    SkASSERT(s.fInvType <= (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask));
    SkASSERT(s.fDoFilter == false);

    const SkPMColor* SK_RESTRICT table = s.fBitmap->getColorTable()->lockColors();
    const uint8_t* SK_RESTRICT srcAddr = (const uint8_t*)s.fBitmap->getPixels();

    // buffer is y32, x16, x16, x16, x16, x16
    // bump srcAddr to the proper row, since we're told Y never changes
    SkASSERT((unsigned)xy[0] < (unsigned)s.fBitmap->height());
    srcAddr = (const uint8_t*)((const char*)srcAddr + xy[0] * s.fBitmap->rowBytes());

    if (1 == s.fBitmap->width()) {
        uint8_t src = srcAddr[0];
        SkPMColor dstValue = table[src];
        sk_memset32(colors, dstValue, count);
    } else {
        const uint16_t* xx = (const uint16_t*)(xy + 1);

        asm volatile (
                      "subs       %[count], %[count], #8          \n\t"   // decrement count by 8, set flags
                      "blt        2f                              \n\t"   // if count < 0, branch to singles
                      "1:                                             \n\t"   // eights loop
                      "ldmia      %[xx]!, {r5, r7, r9, r11}       \n\t"   // load ptrs to pixels 0-7
                      "uxth       r4, r5                          \n\t"   // extract ptr 0
                      "mov        r5, r5, lsr #16                 \n\t"   // extract ptr 1
                      "uxth       r6, r7                          \n\t"   // extract ptr 2
                      "mov        r7, r7, lsr #16                 \n\t"   // extract ptr 3
                      "ldrb       r4, [%[srcAddr], r4]            \n\t"   // load pixel 0 from image
                      "uxth       r8, r9                          \n\t"   // extract ptr 4
                      "ldrb       r5, [%[srcAddr], r5]            \n\t"   // load pixel 1 from image
                      "mov        r9, r9, lsr #16                 \n\t"   // extract ptr 5
                      "ldrb       r6, [%[srcAddr], r6]            \n\t"   // load pixel 2 from image
                      "uxth       r10, r11                        \n\t"   // extract ptr 6
                      "ldrb       r7, [%[srcAddr], r7]            \n\t"   // load pixel 3 from image
                      "mov        r11, r11, lsr #16               \n\t"   // extract ptr 7
                      "ldrb       r8, [%[srcAddr], r8]            \n\t"   // load pixel 4 from image
                      "ldrb       r9, [%[srcAddr], r9]            \n\t"   // load pixel 5 from image
                      "ldrb       r10, [%[srcAddr], r10]          \n\t"   // load pixel 6 from image
                      "ldrb       r11, [%[srcAddr], r11]          \n\t"   // load pixel 7 from image
                      "ldr        r4, [%[table], r4, lsl #2]      \n\t"   // load pixel 0 SkPMColor from colmap
                      "ldr        r5, [%[table], r5, lsl #2]      \n\t"   // load pixel 1 SkPMColor from colmap
                      "ldr        r6, [%[table], r6, lsl #2]      \n\t"   // load pixel 2 SkPMColor from colmap
                      "ldr        r7, [%[table], r7, lsl #2]      \n\t"   // load pixel 3 SkPMColor from colmap
                      "ldr        r8, [%[table], r8, lsl #2]      \n\t"   // load pixel 4 SkPMColor from colmap
                      "ldr        r9, [%[table], r9, lsl #2]      \n\t"   // load pixel 5 SkPMColor from colmap
                      "ldr        r10, [%[table], r10, lsl #2]    \n\t"   // load pixel 6 SkPMColor from colmap
                      "ldr        r11, [%[table], r11, lsl #2]    \n\t"   // load pixel 7 SkPMColor from colmap
                      "subs       %[count], %[count], #8          \n\t"   // decrement loop counter
                      "stmia      %[colors]!, {r4-r11}            \n\t"   // store 8 pixels
                      "bge        1b                              \n\t"   // loop if counter >= 0
                      "2:                                             \n\t"
                      "adds       %[count], %[count], #8          \n\t"   // fix up counter, set flags
                      "beq        4f                              \n\t"   // if count == 0, branch to exit
                      "3:                                             \n\t"   // singles loop
                      "ldrh       r4, [%[xx]], #2                 \n\t"   // load pixel ptr
                      "subs       %[count], %[count], #1          \n\t"   // decrement loop counter
                      "ldrb       r5, [%[srcAddr], r4]            \n\t"   // load pixel from image
                      "ldr        r6, [%[table], r5, lsl #2]      \n\t"   // load SkPMColor from colmap
                      "str        r6, [%[colors]], #4             \n\t"   // store pixel, update ptr
                      "bne        3b                              \n\t"   // loop if counter != 0
                      "4:                                             \n\t"   // exit
                      : [xx] "+r" (xx), [count] "+r" (count), [colors] "+r" (colors)
                      : [table] "r" (table), [srcAddr] "r" (srcAddr)
                      : "memory", "cc", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11"
                      );
    }

    s.fBitmap->getColorTable()->unlockColors(false);
}
#endif //__ARM_ARCH__ >= 6 && !defined(SK_CPU_BENDIAN)


#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
void S16_opaque_D32_nofilter_DX_neon_asm(const SkBitmapProcState& s,
                            const uint32_t* __restrict__ xy,
                            int count, uint32_t* __restrict__ colors) {

    const uint16_t* __restrict__ srcAddr = (const uint16_t*)s.fBitmap->getPixels();

    uint16_t* index;
    uint16_t src;
    int i;

    srcAddr = (const uint16_t*)((const char*)srcAddr + xy[0] * s.fBitmap->rowBytes());

    const uint16_t* __restrict__ xx = (const uint16_t*)(++xy);

    if (1 == s.fBitmap->width()) {

        src = srcAddr[0];
        uint32_t dstValue = SkPixel16ToPixel32(src);
        sk_memset32(colors, dstValue, count);
    } else if ((xx[count-1] - xx[0]) == (count-1)) {
        // No scaling
        const uint16_t* src_data = (const uint16_t*)(srcAddr + xx[0]);
        asm volatile (
                      "subs       %[count], %[count], #8          \n\t"   // count -= 8, set flag
                      "blt        2f                              \n\t"   // if count < 0, branch to label 2
                      "vmov.u16   q8, #0xFF00                     \n\t"   // Load alpha value into q8 for later use.
                      "1:                                         \n\t"   // 8 loop
                      // Handle 8 pixels in one loop.

                      "vld1.u16   {q0}, [%[src_data]]!            \n\t"   // load eight src 565 pixels

                      "vshl.u16   q2, q0, #5                      \n\t"   // put green in the 6 high bits of q2
                      "vshl.u16   q3, q0, #11                     \n\t"   // put blue in the 5 high bits of q3
                      "vmov.u16   q1, q8                          \n\t"   // copy alpha from q8
                      "vsri.u16   q1, q3, #8                      \n\t"   // put blue below alpha in q1
                      "vsri.u16   q1, q3, #13                     \n\t"   // put 3 MSB blue below blue in q1
                      "vsri.u16   q2, q2, #6                      \n\t"   // put 2 MSB green below green in q2
                      "vsri.u16   q2, q0, #8                      \n\t"   // put red below green in q2
                      "vsri.u16   q2, q0, #13                     \n\t"   // put 3 MSB red below red in q2
                      "vzip.16    q2, q1                          \n\t"   // interleave q1 and q2
                      "vst1.16    {d4, d5}, [%[colors]]!          \n\t"   // store q1 to dst
                      "subs       %[count], %[count], #8          \n\t"   // count -= 8, set flag
                      "vst1.16    {d2, d3}, [%[colors]]!          \n\t"   // store q1 to dst

                      "bge        1b                              \n\t"   // loop if count >= 0
                      "2:                                         \n\t"   // exit of 8 loop

                      "adds       %[count], %[count], #4          \n\t"   // add 4 to count to see if a 4 loop is needed.
                      "blt        3f                              \n\t"   // if count < 0, branch to label 3

                      // Handle 4 pixels at once

                      "vld1.u16   {d0}, [%[src_data]]!            \n\t"   // load eight src 565 pixels

                      "vshl.u16   d2, d0, #5                      \n\t"   // put green in the 6 high bits of d2
                      "vshl.u16   d1, d0, #11                     \n\t"   // put blue in the 5 high bits of d1
                      "vmov.u16   d3, d16                         \n\t"   // copy alpha from d16
                      "vsri.u16   d3, d1, #8                      \n\t"   // put blue below alpha in d3
                      "vsri.u16   d3, d1, #13                     \n\t"   // put 3 MSB blue below blue in d3
                      "vsri.u16   d2, d2, #6                      \n\t"   // put 2 MSB green below green in d2
                      "vsri.u16   d2, d0, #8                      \n\t"   // put red below green in d2
                      "vsri.u16   d2, d0, #13                     \n\t"   // put 3 MSB red below red in d2
                      "vzip.16    d2, d3                          \n\t"   // interleave d2 and d3
                      "vst1.16    {d2, d3}, [%[colors]]!          \n\t"   // store d2 and d3 to dst

                      "3:                                         \n\t"   // end
                      : [src_data] "+r" (src_data), [colors] "+r" (colors), [count] "+r" (count)
                      :
                      : "cc", "memory","d0","d1","d2","d3","d4","d5","d6","d7","d16","d17"
                     );

        for (i = (count & 3); i > 0; --i) {
            *colors++ = SkPixel16ToPixel32(*src_data++);
        }

    } else {
        // Scaling case
        uint16_t data[8];

        asm volatile (
                      "subs       %[count], %[count], #8          \n\t"   // count -= 8, set flag
                      "blt        2f                              \n\t"   // if count < 0, branch to label 2
                      "vmov.u16   q8, #0xFF00                     \n\t"   // Load alpha value into q8 for later use.
                      "1:                                         \n\t"   // 8 loop
                      // Handle 8 pixels in one loop.
                      "ldmia      %[xx]!, {r4, r5, r6, r7}        \n\t"   // load ptrs to pixels 0-7

                      "mov        r4, r4, lsl #1                  \n\t"   // <<1 because of 16 bit pointer
                      "mov        r5, r5, lsl #1                  \n\t"   // <<1 because of 16 bit pointer
                      "mov        r6, r6, lsl #1                  \n\t"   // <<1 because of 16 bit pointer
                      "mov        r7, r7, lsl #1                  \n\t"   // <<1 because of 16 bit pointer

                      "uxth       r8, r4                          \n\t"   // extract ptr 0
                      "mov        r4, r4, lsr #16                 \n\t"   // extract ptr 1
                      "ldrh       r8, [%[srcAddr], r8]            \n\t"   // load pixel 0 from image
                      "ldrh       r4, [%[srcAddr], r4]            \n\t"   // load pixel 1 from image
                      "pkhbt      r4, r8, r4, lsl #16             \n\t"   // combine pixel 0 and 1 in one register

                      "uxth       r8, r5                          \n\t"   // extract ptr 2
                      "mov        r5, r5, lsr #16                 \n\t"   // extract ptr 3
                      "ldrh       r8, [%[srcAddr], r8]            \n\t"   // load pixel 2 from image
                      "ldrh       r5, [%[srcAddr], r5]            \n\t"   // load pixel 3 from image
                      "pkhbt      r5, r8, r5, lsl #16             \n\t"   // combine pixel 2 and 3 in one register

                      "uxth       r8, r6                          \n\t"   // extract ptr 4
                      "mov        r6, r6, lsr #16                 \n\t"   // extract ptr 5
                      "ldrh       r8, [%[srcAddr], r8]            \n\t"   // load pixel 4 from image
                      "ldrh       r6, [%[srcAddr], r6]            \n\t"   // load pixel 5 from image
                      "pkhbt      r6, r8, r6, lsl #16             \n\t"   // combine pixel 4 and 5 in one register

                      "uxth       r8, r7                          \n\t"   // extract ptr 6
                      "mov        r7, r7, lsr #16                 \n\t"   // extract ptr 7
                      "ldrh       r8, [%[srcAddr], r8]            \n\t"   // load pixel 6 from image
                      "ldrh       r7, [%[srcAddr], r7]            \n\t"   // load pixel 7 from image
                      "pkhbt      r7, r8, r7, lsl #16             \n\t"   // combine pixel 6 and 7 in one register

                      "stmia      %[data], {r4, r5, r6, r7}       \n\t"   // store 8 src pixels

                      "vld1.u16   {q0}, [%[data]]                 \n\t"   // load eight src 565 pixels

                      "vshl.u16   q2, q0, #5                      \n\t"   // put green in the 6 high bits of q2
                      "vshl.u16   q3, q0, #11                     \n\t"   // put blue in the 5 high bits of q3
                      "vmov.u16   q1, q8                          \n\t"   // copy alpha from q8
                      "vsri.u16   q1, q3, #8                      \n\t"   // put blue below alpha in q1
                      "vsri.u16   q1, q3, #13                     \n\t"   // put 3 MSB blue below blue in q1
                      "vsri.u16   q2, q2, #6                      \n\t"   // put 2 MSB green below green in q2
                      "vsri.u16   q2, q0, #8                      \n\t"   // put red below green in q2
                      "vsri.u16   q2, q0, #13                     \n\t"   // put 3 MSB red below red in q2
                      "vzip.16    q2, q1                          \n\t"   // interleave q1 and q2
                      "vst1.16    {d4, d5}, [%[colors]]!          \n\t"   // store q1 to dst
                      "subs       %[count], %[count], #8          \n\t"   // count -= 8, set flag
                      "vst1.16    {d2, d3}, [%[colors]]!          \n\t"   // store q2 to dst

                      "bge        1b                              \n\t"   // loop if count >= 0
                      "2:                                         \n\t"   // exit of 8 loop

                      "adds       %[count], %[count], #4          \n\t"   // add 4 to count to see if a 4 loop is needed.
                      "blt        3f                              \n\t"   // if count < 0, branch to label 3

                      // Handle 4 pixels at once
                      "ldmia      %[xx]!, {r4, r5}                \n\t"   // load ptrs to pixels 0-3

                      "mov        r4, r4, lsl #1                  \n\t"   // <<1 because of 16 bit pointer
                      "mov        r5, r5, lsl #1                  \n\t"   // <<1 because of 16 bit pointer

                      "uxth       r8, r4                          \n\t"   // extract ptr 0
                      "mov        r4, r4, lsr #16                 \n\t"   // extract ptr 1
                      "ldrh       r8, [%[srcAddr], r8]            \n\t"   // load pixel 0 from image
                      "ldrh       r4, [%[srcAddr], r4]            \n\t"   // load pixel 1 from image
                      "pkhbt      r4, r8, r4, lsl #16             \n\t"   // combine pixel 0 and 1 in one register

                      "uxth       r8, r5                          \n\t"   // extract ptr 2
                      "mov        r5, r5, lsr #16                 \n\t"   // extract ptr 3
                      "ldrh       r8, [%[srcAddr], r8]            \n\t"   // load pixel 2 from image
                      "ldrh       r5, [%[srcAddr], r5]            \n\t"   // load pixel 3 from image
                      "pkhbt      r5, r8, r5, lsl #16             \n\t"   // combine pixel 2 and 3 in one register

                      "stmia      %[data], {r4, r5}               \n\t"   // store 4 src pixels

                      "vld1.u16   {d0}, [%[data]]                 \n\t"   // load eight src 565 pixels

                      "vshl.u16   d2, d0, #5                      \n\t"   // put green in the 6 high bits of d2
                      "vshl.u16   d1, d0, #11                     \n\t"   // put blue in the 5 high bits of d1
                      "vmov.u16   d3, d16                         \n\t"   // copy alpha from d16
                      "vsri.u16   d3, d1, #8                      \n\t"   // put blue below alpha in d3
                      "vsri.u16   d3, d1, #13                     \n\t"   // put 3 MSB blue below blue in d3
                      "vsri.u16   d2, d2, #6                      \n\t"   // put 2 MSB green below green in d2
                      "vsri.u16   d2, d0, #8                      \n\t"   // put red below green in d2
                      "vsri.u16   d2, d0, #13                     \n\t"   // put 3 MSB red below red in d2
                      "vzip.16    d2, d3                          \n\t"   // interleave d2 and d3
                      "vst1.16    {d2, d3}, [%[colors]]!          \n\t"   // store d2 and d3 to dst

                      "3:                                         \n\t"   // End
                      : [xx] "+r" (xx), [colors] "+r" (colors), [count] "+r" (count)
                      : [data] "r" (data), [srcAddr] "r" (srcAddr)
                      : "cc", "memory","r4","r5","r6","r7","r8","d0","d1","d2","d3","d4","d5","d6","d7","d16","d17"
                     );

        for (i = (count & 3); i > 0; --i) {
            src = srcAddr[*xx++]; *colors++ = SkPixel16ToPixel32(src);
        }
    }
}
#endif


///////////////////////////////////////////////////////////////////////////////

/*  If we replace a sampleproc, then we null-out the associated shaderproc,
    otherwise the shader won't even look at the matrix/sampler
 */


void SkBitmapProcState::platformProcs() {
    bool doFilter = fDoFilter;
    bool isOpaque = 256 == fAlphaScale;
    bool justDx = false;

    if (fInvType <= (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask)) {
        justDx = true;
    }

    switch (fBitmap->config()) {
        case SkBitmap::kIndex8_Config:
#if __ARM_ARCH__ >= 6 && !defined(SK_CPU_BENDIAN)
            if (justDx && !doFilter) {
#if 0   /* crashing on android device */
                fSampleProc16 = SI8_D16_nofilter_DX_arm;
                fShaderProc16 = NULL;
#endif
                if (isOpaque) {
                    // this one is only very slighty faster than the C version
                    fSampleProc32 = SI8_opaque_D32_nofilter_DX_arm;
                    fShaderProc32 = NULL;
                }
            }
#endif
            break;
        case SkBitmap::kRGB_565_Config:
#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
            if (justDx && !doFilter) {
                if (isOpaque) {
                    fSampleProc32 = S16_opaque_D32_nofilter_DX_neon_asm;
                }
            }
#endif
            break;
        default:
            break;
    }
}

