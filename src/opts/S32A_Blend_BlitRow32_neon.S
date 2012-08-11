/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Neon optimized version of S32A_Blend_BlitRow32.
 * Special cases for when alpha is zero or opaque.
 */

#if defined(__ARM_HAVE_NEON) && defined(ENABLE_OPTIMIZED_S32A_BLITTERS)

    .text
    .fpu    neon
    .align

    .global S32A_Blend_BlitRow32_neon
    .func   S32A_Blend_BlitRow32_neon

S32A_Blend_BlitRow32_neon:
    cmp           r2, #8                         // The main code requires at least 8 pixels
    ble           BlitSmall
    /* Setup constants, and do the first 1-8 pixels */
    vld4.8        {d20-d23}, [r1]                // Load eight source RGBA pixels
    vld4.8        {d24-d27}, [r0]                // Load eight destination RGBA pixels
    add           r3, #1                         // Modify global alpha to 0...256 range
    vpush         {q4-q5}
    stmdb         sp!, {r4-r5}
    vmov.i16      q15, #256                      // Set up alpha constant
    vmov.i16      q5, #0xFF00                    // Set up mask constant
    vdup.16       q4, r3                         // Set up global alpha
    pld           [r1, #32]                      // Pre-load next eight source pixels
    pld           [r0, #32]                      // Pre-load next eight destination pixels
    ands          r3, r2, #0x7                   // Should we do a partial first iteration?
    moveq         r3, #8                         // Do full iteration?
    vmovl.u8      q8, d20                        // Expand source red to 16-bit
    vmovl.u8      q9, d21                        // Expand source green to 16-bit
    vmovl.u8      q10, d22                       // Expand source blue to 16-bit
    vmovl.u8      q11, d23                       // Expand source alpha to 16-bit
    vmul.i16      q8, q8, q4                     // Scale source red
    vmul.i16      q11, q11, q4                   // Scale source alpha
    vand          q8, q5                         // Mask low byte in red to avoid overflow in vmla
    vmul.i16      q9, q9, q4                     // Scale source green
    vshr.u16      q0, q11, #8                    // Pre-calculate inverse destination alpha (scale)
    vmul.i16      q10, q10, q4                   // Scale source blue
    vand          q11, q5                        // Mask low byte in green to avoid overflow in vmla
    vand          q9, q5                         // Mask low byte in blue to avoid overflow in vmla
    vand          q10, q5                        // Mask low byte in alpha to avoid overflow in vmla
    vsub.i16      q14, q15, q0                   // Calculate inverse destination alpha (scale)
    vmovl.u8      q2, d24                        // Expand destination red to 16-bit
    vmovl.u8      q3, d25                        // Expand destination green to 16-bit
    vmovl.u8      q12, d26                       // Expand destination blue to 16-bit
    vmovl.u8      q13, d27                       // Expand destination alpha to 16-bit
    vmla.i16      q8, q2, q14                    // Scale destination red, and add to source
    mov           r4, r0                         // Backup destination pointer
    add           r1, r3, lsl #2                 // Increment source pointer
    sub           r2, r2, r3                     // Decrement loop counter
    vmla.i16      q9, q3, q14                    // Scale destination green, and add to source
    add           r0, r3, lsl #2                 // Increment destination pointer
    pld           [r1, #32]                      // Pre-load next eight source pixels
    pld           [r0, #32]                      // Pre-load next eight destination pixels
    mov           r3, r0                         // Backup destination pointer
    vmla.i16      q11, q13, q14                  // Scale destination alpha, and add to source
    vld4.8        {d0-d3}, [r1]!                 // Pre-load next eight source RGBA pixels
    subs          r2, r2, #24                    // Decrement loop counter
    vmla.i16      q10, q12, q14                  // Scale destination blue, and add to source
    vld4.8        {d4-d7}, [r0]!                 // Pre-load next eight destination RGBA pixels
    vshrn.i16     d24, q8, #8                    // Shift and narrow red
    vshrn.i16     d25, q9, #8                    // Shift and narrow green
    vshrn.i16     d26, q10, #8                   // Shift and narrow blue
    vshrn.i16     d27, q11, #8                   // Shift and narrow alpha
    vst4.8        {d24-d27}, [r4]                // Write result to memory
    bmi           PostLoop                       // Do we have enough pixels to enter the main loop?
    /* Main loop, blitting 16 pixels per iteration */
Loop:
    pld           [r1, #32]                      // Pre-load next eight source pixels
    pld           [r0, #32]                      // Pre-load next eight destination pixels
    vmov          r4, r5, d3                     // Move alpha to ARM for test
    orrs          r4, r5                         // Check if source alpha is fully transparent
    beq           AllZero1                       // If so, jump to special case handling
    vmovl.u8      q8, d0                         // Expand source red to 16-bit
    vmovl.u8      q9, d1                         // Expand source green to 16-bit
    vmovl.u8      q0, d2                         // Expand source blue to 16-bit
    vmovl.u8      q1, d3                         // Expand source alpha to 16-bit
    vmul.i16      q8, q8, q4                     // Scale source red
    vmul.i16      q1, q1, q4                     // Scale source alpha
    vand          q8, q5                         // Mask low byte in red to avoid overflow in vmla
    vmul.i16      q9, q9, q4                     // Scale source green
    vshr.u16      q10, q1, #8                    // Pre-calculate inverse destination alpha (scale)
    vmul.i16      q0, q0, q4                     // Scale source blue
    vand          q1, q5                         // Mask low byte in green to avoid overflow in vmla
    vand          q9, q5                         // Mask low byte in blue to avoid overflow in vmla
    vand          q0, q5                         // Mask low byte in alpha to avoid overflow in vmla
    vsub.i16      q14, q15, q10                  // Calculate inverse destination alpha (scale)
    vmovl.u8      q12, d4                        // Expand destination red to 16-bit
    vmovl.u8      q13, d5                        // Expand destination green to 16-bit
    vmovl.u8      q2, d6                         // Expand destination blue to 16-bit
    vmovl.u8      q3, d7                         // Expand destination alpha to 16-bit
    vmla.i16      q8, q12, q14                   // Scale destination red and add to source
    vmla.i16      q9, q13, q14                   // Scale destination green and add to source
    vld4.8        {d20-d23}, [r1]!               // Pre-load next eight source RGBA pixels
    vmla.i16      q1, q3, q14                    // Scale destination alpha and add to source
    vmla.i16      q0, q2, q14                    // Scale destination blue and add to source
    vld4.8        {d24-d27}, [r0]!               // Pre-load next eight destination RGBA pixels
    vshrn.i16     d4, q8, #8                     // Shift and narrow red
    vshrn.i16     d5, q9, #8                     // Shift and narrow green
    vshrn.i16     d6, q0, #8                     // Shift and narrow blue
    vshrn.i16     d7, q1, #8                     // Shift and narrow alpha
    vst4.8        {d4-d7}, [r3]!                 // Write result to memory
GoBack1:
    pld           [r1, #32]                      // Pre-load next eight source pixels
    pld           [r0, #32]                      // Pre-load next eight destination pixels
    vmov          r4, r5, d23                     // Move alpha to ARM for test
    orrs          r4, r5                         // Check if source alpha is fully transparent
    beq           AllZero2                       // If so, jump to special case handling
    vmovl.u8      q8, d20                        // Expand source red to 16-bit
    vmovl.u8      q9, d21                        // Expand source green to 16-bit
    vmovl.u8      q10, d22                       // Expand source blue to 16-bit
    vmovl.u8      q11, d23                       // Expand source alpha to 16-bit
    vmul.i16      q8, q8, q4                     // Scale source red
    subs          r2, r2, #16                    // Decrement loop counter
    vmul.i16      q11, q11, q4                   // Scale source alpha
    vand          q8, q5                         // Mask low byte in red to avoid overflow in vmla
    vmul.i16      q9, q9, q4                     // Scale source green
    vshr.u16      q0, q11, #8                    // Pre-calculate inverse destination alpha (scale)
    vmul.i16      q10, q10, q4                   // Scale source blue
    vand          q11, q5                        // Mask low byte in green to avoid overflow in vmla
    vand          q9, q5                         // Mask low byte in blue to avoid overflow in vmla
    vand          q10, q5                        // Mask low byte in alpha to avoid overflow in vmla
    vsub.i16      q14, q15, q0                   // Calculate inverse destination alpha (scale)
    vmovl.u8      q2, d24                        // Expand destination red to 16-bit
    vmovl.u8      q3, d25                        // Expand destination green to 16-bit
    vmovl.u8      q12, d26                       // Expand destination blue to 16-bit
    vmovl.u8      q13, d27                       // Expand destination alpha to 16-bit
    vmla.i16      q8, q2, q14                    // Scale destination red and add to source
    vmla.i16      q9, q3, q14                    // Scale destination green and add to source
    vmla.i16      q11, q13, q14                  // Scale destination alpha and add to source
    vld4.8        {d0-d3}, [r1]!                 // Pre-load next eight source RGBA pixels
    vmla.i16      q10, q12, q14                  // Scale destination blue, and add to source
    vld4.8        {d4-d7}, [r0]!                 // Pre-load next eight destination RGBA pixels
    vshrn.i16     d24, q8, #8                    // Shift and narrow red
    vshrn.i16     d25, q9, #8                    // Shift and narrow green
    vshrn.i16     d26, q10, #8                   // Shift and narrow blue
    vshrn.i16     d27, q11, #8                   // Shift and narrow alpha
    vst4.8        {d24-d27}, [r3]!               // Write result to memory
    bpl           Loop
PostLoop:
    add           r2, r2, #16
    vmov.i16      q10, q4
    ldmia         sp!, {r4-r5}
    vpop          {q4-q5}
LoopRemaining:
    vmovl.u8      q8, d0                         // Expand source red to 16-bit
    vmovl.u8      q9, d1                         // Expand source green to 16-bit
    vmovl.u8      q0, d2                         // Expand source blue to 16-bit
    vmovl.u8      q1, d3                         // Expand source alpha to 16-bit
    vmul.i16      q8, q8, q10                    // Scale source red
    vmov.i16      q12, #0xFF00                   // Set up mask constant
    vmul.i16      q1, q1, q10                    // Scale source alpha
    vand          q8, q12                        // Mask low byte in red to avoid overflow in vmla
    vmul.i16      q9, q9, q10                    // Scale source green
    vshr.u16      q11, q1, #8                    // Pre-calculate inverse destination alpha (scale)
    vmul.i16      q0, q0, q10                    // Scale source blue
    vand          q1, q12                        // Mask low byte in green to avoid overflow in vmla
    vand          q9, q12                        // Mask low byte in blue to avoid overflow in vmla
    vand          q0, q12                        // Mask low byte in alpha to avoid overflow in vmla
    vsub.i16      q14, q15, q11                  // Calculate inverse destination alpha (scale)
    vmovl.u8      q12, d4                        // Expand destination red to 16-bit
    vmovl.u8      q13, d5                        // Expand destination green to 16-bit
    vmovl.u8      q2, d6                         // Expand destination blue to 16-bit
    vmovl.u8      q3, d7                         // Expand destination alpha to 16-bit
    vmla.i16      q8, q12, q14                   // Scale destination red and add to source
    subs          r2, r2, #8                     // Decrement loop counter
    vmla.i16      q9, q13, q14                   // Scale destination green and add to source
    vmla.i16      q1, q3, q14                    // Scale destination alpha and add to source
    vmla.i16      q0, q2, q14                    // Scale destination blue and add to source
    vshrn.i16     d4, q8, #8                     // Shift and narrow red
    vshrn.i16     d5, q9, #8                     // Shift and narrow green
    vshrn.i16     d6, q0, #8                     // Shift and narrow blue
    vshrn.i16     d7, q1, #8                     // Shift and narrow alpha
    vst4.8        {d4-d7}, [r3]!                 // Write result to memory
    bxmi          lr
    vld4.8        {d0-d3}, [r1]                  // Load eight source RGBA pixels
    vld4.8        {d4-d7}, [r0]                  // Load eight destination RGBA pixels
    b             LoopRemaining
AllZero1:
    vld4.8        {d20-d23}, [r1]!               // Pre-load next eight source RGBA pixels
    vld4.8        {d24-d27}, [r0]!               // Pre-load next eight destination RGBA pixels
    add           r3, r3, #32                    // Advance destination write pointer
    b             GoBack1
AllZero2:
    vld4.8        {d0-d3}, [r1]!                 // Pre-load next eight source RGBA pixels
    vld4.8        {d4-d7}, [r0]!                 // Pre-load next eight destination RGBA pixels
    add           r3, r3, #32                    // Advance destination write pointer
    subs          r2, r2, #16                    // Decrement loop counter
    bpl           Loop
    b             PostLoop

/* Handle small blits, 0-8 pixels */
BlitSmall:
    beq           Blit8
    pld           [r1, #0]                       // Pre-load eight source pixels
    pld           [r0, #0]                       // Pre-load eight destination pixels
    add           r3, #1                         // Modify global alpha to 0...256 range
    vdup.16       q13, r3                        // Set up global alpha
    ldr           r3, =AlphaIndex
    vmov.i16      q15, #256                      // Set up alpha constant
    vld1.8        {d29}, [r3]                    // Set up alpha index table
    vmov.i16      q12, #0xFF00                   // Set up mask constant
    cmp           r2, #1                         // Exit if count is zero
    beq           Blit1
    bxlt          lr                             // Zero pixels left
    /* loop for neon 2-pixel code */
Blit2:
    vld1.32       {d0}, [r1]!                    // Load two source RGBA pixels
    vld1.32       {d1}, [r0]                     // Load two destination RGBA pixels
    sub           r2, r2, #2                     // Decrement width counter
    vmovl.u8      q8, d0                         // Expand source to 16-bit
    vmul.i16      q8, q8, q13                    // Scale source pixels
    vmovl.u8      q3, d1                         // Expand destination to 16-bit
    vtbl.8        d2, {d16, d17}, d29            // Spread out alpha to match pixel format
    vand          q8, q12                        // Mask low byte to avoid overflow in vmla
    vsubw.u8      q2, q15, d2                    // Calculate inverse alpha (scale)
    vmla.i16      q8, q3, q2                     // Scale destination pixels and add to source
    vshrn.i16     d0, q8, #8                     // Shift and narrow result
    vst1.32       {d0}, [r0]!                    // Store two RGBA pixels
    cmp           r2, #1                         // Exit if count is zero
    bhi           Blit2                          // Still two or more pixels left
    bxlt          lr                             // Zero pixels left
	/* code to handle any one last pixel */
Blit1:
    vld1.32       {d0[0]}, [r1]                  // Load one source RGBA pixel
    vld1.32       {d1[0]}, [r0]                  // Load one destination RGBA pixel
    vmovl.u8      q8, d0                         // Expand source to 16-bit
    vmul.i16      d16, d16, d26                  // Scale source pixels
    vmovl.u8      q3, d1                         // Expand destination to 16-bit
    vtbl.8        d2, {d16, d17}, d29            // Spread out alpha to match pixel format
    vand          d16, d24                       // Mask low byte to avoid overflow in vmla
    vsubw.u8      q2, q15, d2                    // Calculate inverse alpha (scale)
    vmla.i16      d16, d6, d4                    // Scale destination pixels and add to source
    vshrn.i16     d0, q8, #8                     // Shift and narrow result
    vst1.32       {d0[0]}, [r0]                  // Store one RGBA pixel
    bx            lr
	/* Handle 8 pixels */
Blit8:
    add           r3, #1                         // Modify global alpha to 0...256 range
    sub           r2, r2, #8                     // Decrement loop counter
    vdup.16       q10, r3                        // Set up global alpha
    mov           r3, r0                         // Backup destination pointer
    vld4.8        {d0-d3}, [r1]                  // Load eight source RGBA pixels
    vld4.8        {d4-d7}, [r0]                  // Load eight destination RGBA pixels
    vmov.i16      q15, #256
    b             LoopRemaining

    .endfunc

    .data
    .align
AlphaIndex:
    .byte 7, 7, 7, 7, 15, 15, 15, 15

#endif
