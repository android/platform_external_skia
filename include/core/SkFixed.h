
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkFixed_DEFINED
#define SkFixed_DEFINED

#include "SkTypes.h"

/** \file SkFixed.h

    Types and macros for 16.16 fixed point
*/

/** 32 bit signed integer used to represent fractions values with 16 bits to the right of the decimal point
*/
typedef int32_t             SkFixed;
#define SK_Fixed1           (1 << 16)
#define SK_FixedHalf        (1 << 15)
#define SK_FixedMax         (0x7FFFFFFF)
#define SK_FixedMin         (-SK_FixedMax)
#define SK_FixedNaN         ((int) 0x80000000)
#define SK_FixedPI          (0x3243F)
#define SK_FixedSqrt2       (92682)
#define SK_FixedTanPIOver8  (0x6A0A)
#define SK_FixedRoot2Over2  (0xB505)

#ifdef SK_CAN_USE_FLOAT
    #define SkFixedToFloat(x)   ((x) * 1.5258789e-5f)
#if 1
    #define SkFloatToFixed(x)   ((SkFixed)((x) * SK_Fixed1))
#else
    // pins over/under flows to max/min int32 (slower than just a cast)
    static inline SkFixed SkFloatToFixed(float x) {
        int64_t n = x * SK_Fixed1;
        return (SkFixed)n;
    }
#endif

    #define SkFixedToDouble(x)  ((x) * 1.5258789e-5)
    #define SkDoubleToFixed(x)  ((SkFixed)((x) * SK_Fixed1))
#endif

/** 32 bit signed integer used to represent fractions values with 30 bits to the right of the decimal point
*/
typedef int32_t             SkFract;
#define SK_Fract1           (1 << 30)
#define Sk_FracHalf         (1 << 29)
#define SK_FractPIOver180   (0x11DF46A)

#ifdef SK_CAN_USE_FLOAT
    #define SkFractToFloat(x)   ((float)(x) * 0.00000000093132257f)
    #define SkFloatToFract(x)   ((SkFract)((x) * SK_Fract1))
#endif

/** Converts an integer to a SkFixed, asserting that the result does not overflow
    a 32 bit signed integer
*/
#ifdef SK_DEBUG
    inline SkFixed SkIntToFixed(int n)
    {
        SkASSERT(n >= -32768 && n <= 32767);
        return n << 16;
    }
#else
    //  force the cast to SkFixed to ensure that the answer is signed (like the debug version)
    #define SkIntToFixed(n)     (SkFixed)((n) << 16)
#endif

/** Converts a SkFixed to a SkFract, asserting that the result does not overflow
    a 32 bit signed integer
*/
#ifdef SK_DEBUG
    inline SkFract SkFixedToFract(SkFixed x)
    {
        SkASSERT(x >= (-2 << 16) && x <= (2 << 16) - 1);
        return x << 14;
    }
#else
    #define SkFixedToFract(x)   ((x) << 14)
#endif

/** Returns the signed fraction of a SkFixed
*/
inline SkFixed SkFixedFraction(SkFixed x)
{
    SkFixed mask = x >> 31 << 16;
    return (x & 0xFFFF) | mask;
}

/** Converts a SkFract to a SkFixed
*/
#define SkFractToFixed(x)   ((x) >> 14)

#define SkFixedRoundToInt(x)    (((x) + SK_FixedHalf) >> 16)
#define SkFixedCeilToInt(x)     (((x) + SK_Fixed1 - 1) >> 16)
#define SkFixedFloorToInt(x)    ((x) >> 16)

#define SkFixedRoundToFixed(x)  (((x) + SK_FixedHalf) & 0xFFFF0000)
#define SkFixedCeilToFixed(x)   (((x) + SK_Fixed1 - 1) & 0xFFFF0000)
#define SkFixedFloorToFixed(x)  ((x) & 0xFFFF0000)

// DEPRECATED
#define SkFixedFloor(x)     SkFixedFloorToInt(x)
#define SkFixedCeil(x)      SkFixedCeilToInt(x)
#define SkFixedRound(x)     SkFixedRoundToInt(x)

#define SkFixedAbs(x)       SkAbs32(x)
#define SkFixedAve(a, b)    (((a) + (b)) >> 1)

SkFixed SkFixedMul_portable(SkFixed, SkFixed);
SkFract SkFractMul_portable(SkFract, SkFract);
inline SkFixed SkFixedSquare_portable(SkFixed value)
{
    uint32_t a = SkAbs32(value);
    uint32_t ah = a >> 16;
    uint32_t al = a & 0xFFFF;
    SkFixed result = ah * a + al * ah + (al * al >> 16);
    if (result >= 0)
        return result;
    else // Overflow.
        return SK_FixedMax;
}

#define SkFixedDiv(numer, denom)    SkDivBits(numer, denom, 16)
SkFixed SkFixedDivInt(int32_t numer, int32_t denom);
SkFixed SkFixedMod(SkFixed numer, SkFixed denom);
#define SkFixedInvert(n)            SkDivBits(SK_Fixed1, n, 16)
SkFixed SkFixedFastInvert(SkFixed n);
#define SkFixedSqrt(n)              SkSqrtBits(n, 23)
SkFixed SkFixedMean(SkFixed a, SkFixed b);  //*< returns sqrt(x*y)
int SkFixedMulCommon(SkFixed, int , int bias);  // internal used by SkFixedMulFloor, SkFixedMulCeil, SkFixedMulRound

#define SkFractDiv(numer, denom)    SkDivBits(numer, denom, 30)
#define SkFractSqrt(n)              SkSqrtBits(n, 30)

SkFixed SkFixedSinCos(SkFixed radians, SkFixed* cosValueOrNull);
#define SkFixedSin(radians)         SkFixedSinCos(radians, NULL)
inline SkFixed SkFixedCos(SkFixed radians)
{
    SkFixed cosValue;
    (void)SkFixedSinCos(radians, &cosValue);
    return cosValue;
}
SkFixed SkFixedTan(SkFixed radians);
SkFixed SkFixedASin(SkFixed);
SkFixed SkFixedACos(SkFixed);
SkFixed SkFixedATan2(SkFixed y, SkFixed x);
SkFixed SkFixedExp(SkFixed);
SkFixed SkFixedLog(SkFixed);

#define SK_FixedNearlyZero          (SK_Fixed1 >> 12)

inline bool SkFixedNearlyZero(SkFixed x, SkFixed tolerance = SK_FixedNearlyZero)
{
    SkASSERT(tolerance > 0);
    return SkAbs32(x) < tolerance;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Now look for ASM overrides for our portable versions (should consider putting this in its own file)

#ifdef SkLONGLONG
    inline SkFixed SkFixedMul_longlong(SkFixed a, SkFixed b)
    {
        return (SkFixed)((SkLONGLONG)a * b >> 16);
    }
    inline SkFract SkFractMul_longlong(SkFract a, SkFract b)
    {
        return (SkFixed)((SkLONGLONG)a * b >> 30);
    }
    inline SkFixed SkFixedSquare_longlong(SkFixed value)
    {
        return (SkFixed)((SkLONGLONG)value * value >> 16);
    }
    #define SkFixedMul(a,b)     SkFixedMul_longlong(a,b)
    #define SkFractMul(a,b)     SkFractMul_longlong(a,b)
    #define SkFixedSquare(a)    SkFixedSquare_longlong(a)
#endif

#if defined(__arm__) && !defined(__thumb__)
    /* This guy does not handle NaN or other obscurities, but is faster than
       than (int)(x*65536) when we only have software floats
    */
    inline SkFixed SkFloatToFixed_arm(float x)
    {
        register int32_t y, z;
        asm("movs    %1, %3, lsl #1         \n"
            "mov     %2, #0x8E              \n"
            "sub     %1, %2, %1, lsr #24    \n"
            "mov     %2, %3, lsl #8         \n"
            "orr     %2, %2, #0x80000000    \n"
            "mov     %1, %2, lsr %1         \n"
            "rsbcs   %1, %1, #0             \n"
            : "=r"(x), "=&r"(y), "=&r"(z)
            : "r"(x)
            : "cc"
            );
        return y;
    }
    inline SkFixed SkFixedMul_arm(SkFixed x, SkFixed y)
    {
        register int32_t t;
        asm("smull  %0, %2, %1, %3          \n"
            "mov    %0, %0, lsr #16         \n"
            "orr    %0, %0, %2, lsl #16     \n"
            : "=r"(x), "=&r"(y), "=r"(t)
            : "r"(x), "1"(y)
            :
            );
        return x;
    }
    inline SkFixed SkFixedMulAdd_arm(SkFixed x, SkFixed y, SkFixed a)
    {
        register int32_t t;
        asm("smull  %0, %3, %1, %4          \n"
            "add    %0, %2, %0, lsr #16     \n"
            "add    %0, %0, %3, lsl #16     \n"
            : "=r"(x), "=&r"(y), "=&r"(a), "=r"(t)
            : "%r"(x), "1"(y), "2"(a)
            :
            );
        return x;
    }
    inline SkFixed SkFractMul_arm(SkFixed x, SkFixed y)
    {
        register int32_t t;
        asm("smull  %0, %2, %1, %3          \n"
            "mov    %0, %0, lsr #30         \n"
            "orr    %0, %0, %2, lsl #2      \n"
            : "=r"(x), "=&r"(y), "=r"(t)
            : "r"(x), "1"(y)
            :
            );
        return x;
    }
    #undef SkFixedMul
    #undef SkFractMul
    #define SkFixedMul(x, y)        SkFixedMul_arm(x, y)
    #define SkFractMul(x, y)        SkFractMul_arm(x, y)
    #define SkFixedMulAdd(x, y, a)  SkFixedMulAdd_arm(x, y, a)

    #undef SkFloatToFixed
    #define SkFloatToFixed(x)  SkFloatToFixed_arm(x)
#endif

#if defined (__mips__)
    /* This guy does not handle NaN or other obscurities, but is faster than
       than (int)(x*65536) when we only have software floats
    */
    inline SkFixed SkFloatToFixed_mips(float x) {
        register int32_t t0,t1,t2,t3;
        SkFixed res;

        asm("srl    %[t0], %[x], 31       \n\t"  // t0 <- sign bit
#if (__mips==32) && (__mips_isa_rev>=2)
            "ext    %[t1], %[x], 23, 8    \n\t"  // get exponent
#else
            "srl    %[t1], %[x], 23       \n\t"
            "andi   %[t1], %[t1], 0xff    \n\t"
#endif
            "li     %[t2], 0x8e           \n\t"
            "subu   %[t1], %[t2], %[t1]   \n\t"  // t1=127+15-exponent
            "sll    %[t2], %[x], 8        \n\t"  // mantissa<<8
            "lui    %[t3], 0x8000         \n\t"
            "or     %[t2], %[t2], %[t3]   \n\t"  // add the missing 1
            "srl    %[res], %[t2], %[t1]  \n\t"  // scale to 16.16
            "subu   %[t2], $zero, %[res]  \n\t"
            "movn   %[res], %[t2], %[t0]  \n\t"  // if negative?
            "sltiu  %[t3], %[t1], 32      \n\t"  // if t1>=32 the float value is too small
            "movz   %[res], $zero, %[t3]  \n\t"  // so res=0
            : [res]"=&r"(res),[t0]"=&r"(t0),[t1]"=&r"(t1),[t2]"=&r"(t2),[t3]"=&r"(t3)
            : [x] "r" (x)
            );
        return res;
    }
    inline SkFixed SkFixedMul_mips(SkFixed x, SkFixed y) {
        SkFixed res;
        int32_t t0;
        asm("mult  %[x], %[y]             \n\t"
            "mflo  %[res]                 \n\t"
            "mfhi  %[t0]                  \n\t"
            "srl   %[res], %[res], 16     \n\t"
            "sll   %[t0], %[t0], 16       \n\t"
            "or    %[res], %[res], %[t0]  \n\t"
            : [res]"=&r"(res),[t0]"=&r"(t0)
            : [x] "r" (x), [y] "r" (y)
            : "hi", "lo"
            );
        return res;
    }
    inline SkFixed SkFixedMulAdd_mips(SkFixed x, SkFixed y, SkFixed a) {
        SkFixed res;
        int32_t t0;
        asm("mult  %[x], %[y]             \n\t"
            "mflo  %[res]                 \n\t"
            "mfhi  %[t0]                  \n\t"
            "srl   %[res], %[res], 16     \n\t"
            "sll   %[t0], %[t0], 16       \n\t"
            "or    %[res], %[res], %[t0]  \n\t"
            "add   %[res], %[res], %[a]   \n\t"
            : [res]"=&r"(res),[t0]"=&r"(t0)
            : [x] "r" (x), [y] "r" (y), [a] "r" (a)
            : "hi", "lo"
            );
        return res;
    }
    inline SkFixed SkFractMul_mips(SkFixed x, SkFixed y) {
        SkFixed res;
        int32_t t0;
        asm("mult  %[x], %[y]             \n\t"
            "mfhi  %[t0]                  \n\t"
            "mflo  %[res]                 \n\t"
            "srl   %[res], %[res], 30     \n\t"
            "sll   %[t0], %[t0], 2        \n\t"
            "or    %[res], %[res], %[t0]  \n\t"
            : [res]"=&r"(res),[t0]"=&r"(t0)
            : [x] "r" (x), [y] "r" (y)
            : "hi", "lo"
            );
        return res;
    }

    #undef SkFixedMul
    #undef SkFractMul
    #define SkFixedMul(x, y)        SkFixedMul_mips(x, y)
    #define SkFractMul(x, y)        SkFractMul_mips(x, y)
    #define SkFixedMulAdd(x, y, a)  SkFixedMulAdd_mips(x, y, a)

    #undef SkFloatToFixed
    #define SkFloatToFixed(x)  SkFloatToFixed_mips(x)

#endif // defined(__mips__)

/////////////////////// Now define our macros to the portable versions if they weren't overridden

#ifndef SkFixedSquare
    #define SkFixedSquare(x)    SkFixedSquare_portable(x)
#endif
#ifndef SkFixedMul
    #define SkFixedMul(x, y)    SkFixedMul_portable(x, y)
#endif
#ifndef SkFractMul
    #define SkFractMul(x, y)    SkFractMul_portable(x, y)
#endif
#ifndef SkFixedMulAdd
    #define SkFixedMulAdd(x, y, a)  (SkFixedMul(x, y) + (a))
#endif

#endif
