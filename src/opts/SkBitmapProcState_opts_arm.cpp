
/*
 * Copyright 2009 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "SkBitmapProcState.h"
#include "SkColorPriv.h"
#include "SkUtils.h"
#include "SkShader.h"

#if defined(__ARM_HAVE_NEON)
#include "SkBitmapProcState_filter.h"
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
void Clamp_S32_opaque_D32_filter_DX_shaderproc(const SkBitmapProcState& s, int x, int y,
                                               SkPMColor* SK_RESTRICT colors, int count) {
    SkASSERT((s.fInvType & ~(SkMatrix::kTranslate_Mask |
                             SkMatrix::kScale_Mask)) == 0);
    SkASSERT(s.fInvKy == 0);
    SkASSERT(count > 0 && colors != NULL);
    SkASSERT(s.fDoFilter);
    SkDEBUGCODE(SkASSERT(s.fBitmap->config() == SkBitmap::kARGB_8888_Config); SkASSERT(s.fAlphaScale == 256);)

    const unsigned maxX = s.fBitmap->width() - 1;
    const SkFixed oneX = s.fFilterOneX;
    const SkFixed dx = s.fInvSx;
    SkFixed fx;
    const SkPMColor* SK_RESTRICT row0;
    const SkPMColor* SK_RESTRICT row1;
    unsigned subY;

    {
        SkPoint pt;
        s.fInvProc(*s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                   SkIntToScalar(y) + SK_ScalarHalf, &pt);
        SkFixed fy = SkScalarToFixed(pt.fY) - (s.fFilterOneY >> 1);
        const unsigned maxY = s.fBitmap->height() - 1;
        // compute our two Y values up front
        subY = (((fy) >> 12) & 0xF);
        int y0 = SkClampMax((fy) >> 16, maxY);
        int y1 = SkClampMax((fy + s.fFilterOneY) >> 16, maxY);

        const char* SK_RESTRICT srcAddr = (const char*)s.fBitmap->getPixels();
        unsigned rb = s.fBitmap->rowBytes();
        row0 = (const SkPMColor*)(srcAddr + y0 * rb);
        row1 = (const SkPMColor*)(srcAddr + y1 * rb);
        // now initialize fx
        fx = SkScalarToFixed(pt.fX) - (oneX >> 1);
    }

    do {
        // Check if we can do the next four pixels using ARM NEON asm
        if ((count >= 4) &&
            (((dx >= 0) && (fx >= 0) && (((fx + 3 * dx) >> 16) < (const signed)maxX)) ||
             ((dx < 0) && ((fx >> 16) < (const signed)maxX) && (((fx + 3 * dx) >> 16) >= 0)))) {
            int asm_count;

            // How many iterations can we do while still clamped?
            if (dx >= 0) {
                asm_count = (((((const signed)maxX - 1) << 16) - fx) / dx) >> 2;
            } else {
                asm_count = ((0 - fx) / dx) >> 2;
            }

            if (asm_count <= 0) {
                asm_count = 1;
            } else if ((asm_count << 2) > count) {
                asm_count = count >> 2;
            }

            count -= asm_count << 2;

            // We know that oneX is 1.0 since we are running clamped.
            // This means that we can load both x0 and x1 pixels in one go.
            asm volatile (
                // Setup constants
                "rsb            r8, %[subY], #16                \n\t"   // 16 - subY
                "vdup.8         d30, %[subY]                    \n\t"   // Create constant for subY
                "vdup.8         d31, r8                         \n\t"   // Create constant for 16 - subY
                "vmov.u16       d29, #16                        \n\t"   // Create constant for 16
                "1:                                             \n\t"   // Loop start
                // Pre-load pixel #1
                "asr            r7, %[fx], #16                  \n\t"   // Calculate offset fx >> 16
                "lsl            r7, r7, #2                      \n\t"   // Adjust offset for 32-bit RGBA values
                "add            r8, %[row0], r7                 \n\t"   // Calculate address for row0
                "vld1.32        {d0}, [r8]                      \n\t"   // Load two RGBA pixels from row0
                "add            r7, %[row1], r7                 \n\t"   // Calculate address for row1
                "vld1.32        {d1}, [r7]                      \n\t"   // Load two RGBA pixels from row1
                "subs           %[cnt], %[cnt], #1              \n\t"   // Decrement loop counter
                // Calculate pixel #1 and pre-load #2
                "lsr            r8, %[fx], #12                  \n\t"   // Calculate subX = ((fx >> 12) & 0xF)
                "and            r8, r8, #0xF                    \n\t"   //
                "add            %[fx], %[fx], %[dx]             \n\t"   // Move fx to next position
                "vdup.16        d28, r8                         \n\t"   // subX
                "vmull.u8       q1, d0, d31                     \n\t"   // q1 = [a00|a01] * (16 - y)
                "vmull.u8       q2, d1, d30                     \n\t"   // q2 = [a10|a11] * y
                "asr            r7, %[fx], #16                  \n\t"   // Calculate offset fx >> 16
                "lsl            r7, r7, #2                      \n\t"   // Adjust offset for 32-bit RGBA values
                "add            r8, %[row0], r7                 \n\t"   // Calculate next address for row0
                "add            r7, %[row1], r7                 \n\t"   // Calculate next address for row1
                "vld1.32        {d0}, [r8]                      \n\t"   // Load next two RGBA pixels from row0
                "vmul.i16       d16, d3, d28                    \n\t"   // d16  = a01 * x
                "vmla.i16       d16, d5, d28                    \n\t"   // d16 += a11 * x
                "vld1.32        {d1}, [r7]                      \n\t"   // Load next two RGBA pixels from row1
                "vsub.i16       d27, d29, d28                   \n\t"   // 16 - subX
                "vmla.i16       d16, d2, d27                    \n\t"   // d16 += a00 * (16 - x)
                "vmla.i16       d16, d4, d27                    \n\t"   // d16 += a10 * (16 - x)
                // Calculate pixel #2 and pre-load #3
                "lsr            r8, %[fx], #12                  \n\t"   // Calculate subX = ((fx >> 12) & 0xF)
                "and            r8, r8, #0xF                    \n\t"   //
                "add            %[fx], %[fx], %[dx]             \n\t"   // Move fx to next position
                "vdup.16        d28, r8                         \n\t"   // subX
                "vmull.u8       q1, d0, d31                     \n\t"   // q1 = [a00|a01] * (16 - y)
                "vmull.u8       q2, d1, d30                     \n\t"   // q2 = [a10|a11] * y
                "asr            r7, %[fx], #16                  \n\t"   // Calculate offset fx >> 16
                "lsl            r7, r7, #2                      \n\t"   // Adjust offset for 32-bit RGBA values
                "add            r8, %[row0], r7                 \n\t"   // Calculate next address for row0
                "add            r7, %[row1], r7                 \n\t"   // Calculate next address for row1
                "vld1.32        {d0}, [r8]                      \n\t"   // Load next two RGBA pixels from row0
                "vmul.i16       d17, d3, d28                    \n\t"   // d17  = a01 * x
                "vmla.i16       d17, d5, d28                    \n\t"   // d17 += a11 * x
                "vld1.32        {d1}, [r7]                      \n\t"   // Load next two RGBA pixels from row1
                "vsub.i16       d27, d29, d28                   \n\t"   // 16 - subX
                "vmla.i16       d17, d2, d27                    \n\t"   // d17 += a00 * (16 - x)
                "vmla.i16       d17, d4, d27                    \n\t"   // d17 += a10 * (16 - x)
                // Calculate pixel #3 and pre-load #4
                "lsr            r8, %[fx], #12                  \n\t"   // Calculate subX = ((fx >> 12) & 0xF)
                "and            r8, r8, #0xF                    \n\t"   //
                "add            %[fx], %[fx], %[dx]             \n\t"   // Move fx to next position
                "vdup.16        d28, r8                         \n\t"   // subX
                "vmull.u8       q1, d0, d31                     \n\t"   // q1 = [a00|a01] * (16 - y)
                "vmull.u8       q2, d1, d30                     \n\t"   // q2 = [a10|a11] * y
                "vshrn.i16      d16, q8, #8                     \n\t"   // shift down result by 8
                "asr            r7, %[fx], #16                  \n\t"   // Calculate offset fx >> 16
                "lsl            r7, r7, #2                      \n\t"   // Adjust offset for 32-bit RGBA values
                "add            r8, %[row0], r7                 \n\t"   // Calculate next address for row0
                "add            r7, %[row1], r7                 \n\t"   // Calculate next address for row1
                "vld1.32        {d0}, [r8]                      \n\t"   // Load next two RGBA pixels from row0
                "vmul.i16       d18, d3, d28                    \n\t"   // d18  = a01 * x
                "vmla.i16       d18, d5, d28                    \n\t"   // d18 += a11 * x
                "vld1.32        {d1}, [r7]                      \n\t"   // Load next two RGBA pixels from row1
                "vsub.i16       d27, d29, d28                   \n\t"   // 16 - subX
                "vmla.i16       d18, d2, d27                    \n\t"   // d18 += a00 * (16 - x)
                "vmla.i16       d18, d4, d27                    \n\t"   // d18 += a10 * (16 - x)
                // Calculate pixel #4
                "vmull.u8       q1, d0, d31                     \n\t"   // q1 = [a00|a01] * (16 - y)
                "vmull.u8       q2, d1, d30                     \n\t"   // q2 = [a10|a11] * y
                "lsr            r8, %[fx], #12                  \n\t"   // Calculate subX = ((fx >> 12) & 0xF)
                "and            r8, r8, #0xF                    \n\t"   //
                "add            %[fx], %[fx], %[dx]             \n\t"   // Move fx to next position
                "vdup.16        d28, r8                         \n\t"   // subX
                "vmul.i16       d19, d3, d28                    \n\t"   // d19  = a01 * x
                "vmla.i16       d19, d5, d28                    \n\t"   // d19 += a11 * x
                "vsub.i16       d27, d29, d28                   \n\t"   // 16 - subX
                "vmla.i16       d19, d2, d27                    \n\t"   // d19 += a00 * (16 - x)
                "vmla.i16       d19, d4, d27                    \n\t"   // d19 += a10 * (16 - x)
                "vshrn.i16      d17, q9, #8                     \n\t"   // shift down result by 8
                "vst1.32        {d16-d17}, [%[colors]]!         \n\t"   // Write result to memory
                "bne            1b                              \n\t"
                : [fx] "+r" (fx), [colors] "+r" (colors), [cnt] "+r" (asm_count)
                : [row0] "r" (row0), [row1] "r" (row1), [subY] "r" (subY), [dx] "r" (dx)
                : "cc", "memory", "r7", "r8", "d0", "d1", "d2", "d3", "d4", "d5", "d16", "d17", "d18", "d19", "d27", "d28", "d29", "d30", "d31"
                );
        } else {
            unsigned subX = (((fx) >> 12) & 0xF);
            unsigned x0 = SkClampMax((fx) >> 16, maxX);
            unsigned x1 = SkClampMax((fx + oneX) >> 16, maxX);

            Filter_32_opaque(subX, subY,
                            row0[x0],
                            row0[x1],
                            row1[x0],
                            row1[x1],
                            colors);
            colors += 1;
            fx += dx;
            count--;
        }
    } while (count != 0);
}
#endif


///////////////////////////////////////////////////////////////////////////////

/*  If we replace a sampleproc, then we null-out the associated shaderproc,
    otherwise the shader won't even look at the matrix/sampler
 */
void SkBitmapProcState::platformProcs() {
    bool doFilter = fDoFilter;
    bool isOpaque = 256 == fAlphaScale;
    bool justDx = (fInvType <= (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask));
    bool clamp_clamp = ((SkShader::kClamp_TileMode == fTileModeX) &&
                       (SkShader::kClamp_TileMode == fTileModeY));

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
        case SkBitmap::kARGB_8888_Config:
#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
            if (S32_opaque_D32_filter_DX == fSampleProc32 && clamp_clamp) {
                fShaderProc32 = Clamp_S32_opaque_D32_filter_DX_shaderproc;
            }
#endif
            break;
        default:
            break;
    }
}

