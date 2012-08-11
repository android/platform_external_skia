/*
 * Copyright (C) ST-Ericsson SA 2012
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Neon optimized version of S32A_Opaque_BlitRow32.
 * Special cases for when alpha is zero or opaque.
 */

#if defined(__ARM_HAVE_NEON) && defined(ENABLE_OPTIMIZED_S32A_BLITTERS)

    .text
    .fpu    neon
    .align

    .global S32A_Opaque_BlitRow32_neon
    .func   S32A_Opaque_BlitRow32_neon

S32A_Opaque_BlitRow32_neon:
    stmdb         sp!, {r4-r6}
    cmp           r2, #7                         // The main loop requires at least 8 pixels
    ble           BlitSmall
    /* Setup constants */
    vld4.8        {d0-d3}, [r1]!                 // Load eight source RGBA pixels
    vmov.i16      q15, #256                      // Set up alpha constant
    pld           [r1, #0]                       // Pre-load next eight source pixels
    subs          r2, r2, #24                    // Decrement loop counter
    mov           r3, r0                         // Backup destination pointer
    bmi           PostLoop                       // Do we have enough pixels to enter the main loop?
    pld           [r1, #32]                      // Pre-load next next eight source pixels
    /* Main loop, blitting 16 pixels per iteration */
Loop:
    vmov          r4, r5, d3                     // Move alpha to ARM for test
    pld           [r1, #64]                      // Pre-load next eight source pixels
    and           r6, r4, r5                     // Check if source alpha is opaque
    cmp           r6, #0xFFFFFFFF                //
    bne           NotOpaque1                     // If not opaque, skip code
    vld4.8        {d20-d23}, [r1]!               // Pre-load next eight source RGBA pixels
    vst4.8        {d0-d3}, [r3]!                 // Since it is opaque, just write result to memory
    add           r0, r0, #32                    // Advance destination pointer
    b             GoBack1                        // Skip to next eight pixels
NotOpaque1:
    orrs          r4, r5                         // Check if source alpha is fully transparent
    beq           AllZero1                       // If so, jump to special case handling

    vld4.8        {d4-d7}, [r0]!                 // Pre-load next eight destination RGBA pixels
    vsubw.u8      q14, q15, d3                   // Calculate inverse alpha (scale)
    vmovl.u8      q8, d4                         // Expand destination red to 16-bit
    vmovl.u8      q9, d5                         // Expand destination green to 16-bit
    vmovl.u8      q2, d6                         // Expand destination blue to 16-bit
    vmovl.u8      q3, d7                         // Expand destination alpha to 16-bit
    vmul.i16      q8, q8, q14                    // Scale red
    vmul.i16      q9, q9, q14                    // Scale green
    vld4.8        {d20-d23}, [r1]!               // Pre-load next eight source RGBA pixels
    vmul.i16      q3, q3, q14                    // Scale alpha
    vmul.i16      q2, q2, q14                    // Scale blue
    vshrn.i16     d7, q3, #8                     // Shift and narrow alpha
    vshrn.i16     d6, q2, #8                     // Shift and narrow blue
    vshrn.i16     d5, q9, #8                     // Shift and narrow green
    vshrn.i16     d4, q8, #8                     // Shift and narrow red
    vadd.i8       q3, q1                         // Add source to results
    vadd.i8       q2, q0                         // Add source to results
    vst4.8        {d4-d7}, [r3]!                 // Write result to memory
GoBack1:
    vmov          r4, r5, d23                    // Move alpha to ARM for test
    pld           [r1, #64]                      // Pre-load next eight source pixels
    and           r6, r4, r5                     // Check if source alpha is opaque
    cmp           r6, #0xFFFFFFFF                //
    bne           NotOpaque2                     // If not opaque, skip code
    vld4.8        {d0-d3}, [r1]!                 // Pre-load next eight source RGBA pixels
    vst4.8        {d20-d23}, [r3]!               // Since it is opaque, just write result to memory
    subs          r2, r2, #16                    // Decrement loop counter
    add           r0, r0, #32                    // Advance destination pointer
    bpl           Loop                           // Loop here, instead of jumping to GoBack2
    b             PostLoop
NotOpaque2:
    orrs          r4, r5                         // Check if source alpha is fully transparent
    beq           AllZero2                       // If so, jump to special case handling

    vld4.8        {d24-d27}, [r0]!               // Pre-load next eight destination RGBA pixels
    vsubw.u8      q14, q15, d23                  // Calculate inverse alpha (scale)
    vmovl.u8      q8, d24                        // Expand destination red to 16-bit
    vmovl.u8      q9, d25                        // Expand destination green to 16-bit
    vmovl.u8      q12, d26                       // Expand destination blue to 16-bit
    vmovl.u8      q13, d27                       // Expand destination alpha to 16-bit
    vmul.i16      q8, q8, q14                    // Scale red
    vmul.i16      q9, q9, q14                    // Scale green
    vld4.8        {d0-d3}, [r1]!                 // Pre-load next eight source RGBA pixels
    vmul.i16      q13, q13, q14                  // Scale alpha
    vmul.i16      q12, q12, q14                  // Scale blue
    vshrn.i16     d27, q13, #8                   // Shift and narrow alpha
    vshrn.i16     d26, q12, #8                   // Shift and narrow blue
    vshrn.i16     d25, q9, #8                    // Shift and narrow green
    vshrn.i16     d24, q8, #8                    // Shift and narrow red
    vadd.i8       q13, q11                       // Add source to results
    vadd.i8       q12, q10                       // Add source to results
    vst4.8        {d24-d27}, [r3]!               // Write result to memory
GoBack2:
    subs          r2, r2, #16                    // Decrement loop counter
    bpl           Loop
PostLoop:
    adds          r2, r2, #16
    bmi           Remaining
LoopRemaining:
    vmov          r4, r5, d3                     // Move alpha to ARM for test
    and           r6, r4, r5                     // Check if source alpha is opaque
    cmp           r6, #0xFFFFFFFF                //
    bne           NotOpaque3                     // If not opaque, skip code
    vst4.8        {d0-d3}, [r3]!                 // Since it is opaque, just write result to memory
    add           r0, r0, #32                    // Advance destination pointer
    subs          r2, r2, #8                     // Decrement loop counter
    bmi           Remaining
    vld4.8        {d0-d3}, [r1]!                 // Load eight source RGBA pixels
    b             LoopRemaining
NotOpaque3:
    orrs          r4, r5                         // Check if source alpha is fully transparent
    addeq         r3, r3, #32                    // If so, advance destination write pointer
    addeq         r0, r0, #32                    // ...advance destination read pointer
    beq           GoBack3                        // ...and jump to special case handling

    vld4.8        {d4-d7}, [r0]!                 // Load eight destination RGBA pixels
    vsubw.u8      q14, q15, d3                   // Calculate inverse alpha (scale)
    vmovl.u8      q8, d4                         // Expand destination red to 16-bit
    vmovl.u8      q9, d5                         // Expand destination green to 16-bit
    vmovl.u8      q2, d6                         // Expand destination blue to 16-bit
    vmovl.u8      q3, d7                         // Expand destination alpha to 16-bit
    vmul.i16      q8, q8, q14                    // Scale red
    vmul.i16      q9, q9, q14                    // Scale green
    vmul.i16      q3, q3, q14                    // Scale alpha
    vmul.i16      q2, q2, q14                    // Scale blue
    vshrn.i16     d7, q3, #8                     // Shift and narrow alpha
    vshrn.i16     d6, q2, #8                     // Shift and narrow blue
    vshrn.i16     d5, q9, #8                     // Shift and narrow green
    vshrn.i16     d4, q8, #8                     // Shift and narrow red
    vadd.i8       q3, q1                         // Add source to results
    vadd.i8       q2, q0                         // Add source to results
    vst4.8        {d4-d7}, [r3]!                 // Write result to memory
GoBack3:
    subs          r2, r2, #8                     // Decrement loop counter
    bmi           Remaining
    vld4.8        {d0-d3}, [r1]!                 // Load eight source RGBA pixels
    b             LoopRemaining

AllZero1:
    vld4.8        {d20-d23}, [r1]!               // Pre-load next eight source RGBA pixels
    add           r3, r3, #32                    // Advance destination write pointer
    add           r0, r0, #32                    // Advance destination read pointer
    b             GoBack1
AllZero2:
    vld4.8        {d0-d3}, [r1]!                 // Pre-load next eight source RGBA pixels
    add           r3, r3, #32                    // Advance destination write pointer
    subs          r2, r2, #16                    // Decrement loop counter
    add           r0, r0, #32                    // Advance destination read pointer
    bpl           Loop
    b             PostLoop

/* Handle small blits, 0-8 pixels */
Remaining:
    adds          r2, r2, #8
    ldmeq         sp!, {r4-r6}
    bxeq          lr                             // Zero pixels left
    ldr           r3, =AlphaIndex
    ldr           r6, =0x00FFFFFF                // Set up transparency check constant
    cmp           r2, #1                         // Exit if count is zero
    vld1.8        {d29}, [r3]                    // Set up alpha index table
    bhi           Blit2
    b             Blit1
BlitSmall:
    pld           [r1, #0]                       // Pre-load eight source pixels
    ldr           r3, =AlphaIndex
    ldr           r6, =0x00FFFFFF                // Set up transparency check constant
    vld1.8        {d29}, [r3]                    // Set up alpha index table
    cmp           r2, #1                         // Exit if count is zero
    vmov.i16      q15, #256                      // Set up alpha constant
    beq           Blit1
    ldmlt         sp!, {r4-r6}
    bxlt          lr                             // Zero pixels left
    /* loop for neon 2-pixel code */
Blit2:
    ldmia         r1!, {r4, r5}                  // Load two source RGBA pixels
    sub           r2, r2, #2                     // Decrement loop counter
    and           r3, r4, r5                     // Check if source alpha is opaque
    cmp           r3, #0xFF000000                //
    blo           NotOpaque4                     // If not opaque, skip code
    stmia         r0!, {r4, r5}                  // Store two source RGBA pixels
    cmp           r2, #1                         // Check count
    bhi           Blit2                          // Still two or more pixels left
    ldmlt         sp!, {r4-r6}
    bxlt          lr                             // Zero pixels left
    b             Blit1
NotOpaque4:
    orr           r3, r4, r5                     // Check if source alpha is fully transparent
    cmp           r3, r6                         //
    addls         r0, r0, #8                     // If so, advance destination read pointer
    bls           GoBack4                        // ...and jump to special case handling

    vmov          d0, r4, r5                     // Move pixel to neon
    vld1.32       {d1}, [r0]                     // Load two destination RGBA pixels
    vtbl.8        d2, {d0}, d29                  // Spread out alpha to match pixel format
    vsubw.u8      q2, q15, d2                    // Calculate inverse alpha (scale)
    vmovl.u8      q3, d1                         // Expand destination to 16-bit
    vmul.i16      q3, q3, q2                     // Scale pixels
    vshrn.i16     d1, q3, #8                     // Shift and narrow result
    vadd.i8       d0, d1                         // Add alpha to results
    vst1.32       {d0}, [r0]!                    // Store two RGBA pixels
GoBack4:
    cmp           r2, #1                         // Check count
    bhi           Blit2                          // Still two or more pixels left
    ldmlt         sp!, {r4-r6}
    bxlt          lr                             // Zero pixels left
	/* code to handle any one last pixel */
Blit1:
    ldr           r4, [r1]                       // Load one source RGBA pixel
    cmp           r4, #0xFF000000                // Check if source alpha is opaque
    strhs         r4, [r0]                       // If so, store one RGBA pixel
    ldmhs         sp!, {r4-r6}
    bxhs          lr                             // Zero pixels left

    cmp           r4, r6                         // Check if source alpha is fully transparent
    ldmls         sp!, {r4-r6}
    bxls          lr                             // Zero pixels left

    vmov.32       d0[0], r4                      // Move pixel to neon
    vld1.32       {d1[0]}, [r0]                  // Load one destination RGBA pixel
    vtbl.8        d2, {d0}, d29                  // Spread out alpha to match pixel format
    vsubw.u8      q2, q15, d2                    // Calculate inverse alpha (scale)
    vmovl.u8      q3, d1                         // Expand destination to 16-bit
    vmul.i16      d6, d6, d4                     // Scale pixel
    vshrn.i16     d1, q3, #8                     // Shift and narrow result
    vadd.i8       d0, d1                         // Add alpha to results
    vst1.32       {d0[0]}, [r0]                  // Store one RGBA pixel
    ldmia         sp!, {r4-r6}
    bx            lr

    .endfunc

    .data
    .align
AlphaIndex:
    .byte 3, 3, 3, 3, 7, 7, 7, 7

#endif
