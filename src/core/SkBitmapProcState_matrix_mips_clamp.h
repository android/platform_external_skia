
#define SCALE_NOFILTER_NAME     MAKENAME(_nofilter_scale_mips)
#define SCALE_FILTER_NAME       MAKENAME(_filter_scale_mips)
#define AFFINE_NOFILTER_NAME    MAKENAME(_nofilter_affine_mips)
#define AFFINE_FILTER_NAME      MAKENAME(_filter_affine_mips)
#define PERSP_NOFILTER_NAME     MAKENAME(_nofilter_persp_mips)
#define PERSP_FILTER_NAME       MAKENAME(_filter_persp_mips)

#define PACK_FILTER_X_NAME  MAKENAME(_pack_filter_x)
#define PACK_FILTER_Y_NAME  MAKENAME(_pack_filter_y)

#ifndef PREAMBLE
    #define PREAMBLE(state)
    #define PREAMBLE_PARAM_X
    #define PREAMBLE_PARAM_Y
    #define PREAMBLE_ARG_X
    #define PREAMBLE_ARG_Y
#endif

void SCALE_NOFILTER_NAME(const SkBitmapProcState& s,
                                uint32_t xy[], int count, int x, int y) {
    SkASSERT((s.fInvType & ~(SkMatrix::kTranslate_Mask |
                             SkMatrix::kScale_Mask)) == 0);

    PREAMBLE(s);
    // we store y, x, x, x, x, x

    const unsigned maxX = s.fBitmap->width() - 1;
    SkFixed fx;
    {
        SkPoint pt;
        s.fInvProc(*s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                                  SkIntToScalar(y) + SK_ScalarHalf, &pt);
        fx = SkScalarToFixed(pt.fY);
        const unsigned maxY = s.fBitmap->height() - 1;
        *xy++ = TILEY_PROCF(fx, maxY);
        fx = SkScalarToFixed(pt.fX);
    }

    if (0 == maxX) {
        // all of the following X values must be 0
        memset(xy, 0, count * sizeof(uint16_t));
        return;
    }

    const SkFixed dx = s.fInvSx;

#ifdef CHECK_FOR_DECAL
    // test if we don't need to apply the tile proc
    if ((unsigned)(fx >> 16) <= maxX &&
        (unsigned)((fx + dx * (count - 1)) >> 16) <= maxX) {
        decal_nofilter_scale(xy, fx, dx, count);
    } else
#endif
    {
#if defined ( __mips_dsp)
        register int t1, t2, t3, t4, t5;
        __asm__ __volatile__ (
            "addu        %[t1], %[fx], %[dx]          \n\t"         // fx + dx
            "addu        %[t2], %[dx], %[dx]          \n\t"         //
            "replv.ph    %[t3], %[maxX]               \n\t"         // maxX || maxX
            "srl         %[t4], %[count], 1           \n\t"         //
            "blez        %[t4], 2f                    \n\t"         // check for count <= 0
            "1:                                       \n\t"
#ifdef SK_CPU_BENDIAN
            "precrq.ph.w %[t5], %[fx], %[t1]          \n\t"         // fx || (fx+dx)
#else
            "precrq.ph.w %[t5], %[t1], %[fx]          \n\t"         // (fx+dx) || fx
#endif
            "cmp.lt.ph   %[t5], $zero                 \n\t"         //
            "pick.ph     %[t5], $zero, %[t5]          \n\t"         // clamp 0
            "cmp.lt.ph   %[t3], %[t5]                 \n\t"         //
            "pick.ph     %[t5], %[t3], %[t5]          \n\t"         // clamp max
            "sw          %[t5], 0(%[xy])              \n\t"         // store to xy[]
            "addu        %[fx], %[fx], %[t2]          \n\t"         // fx += 2*dx
            "addu        %[t1], %[t1], %[t2]          \n\t"         // (fx+dx) += 2*dx
            "subu        %[t4], %[t4], 1              \n\t"         //
            "addu        %[xy], %[xy], 4              \n\t"         // xy += 4
            "bnez        %[t4], 1b                    \n\t"         //
            "2:                                       \n\t"
            "and         %[t4], %[count], 1           \n\t"         //
            "beqz        %[t4], 3f                    \n\t"         //
            "sra         %[t4], %[fx], 16             \n\t"         //
            "slt         %[t5], %[t4], $zero          \n\t"         //
            "movn        %[t4], $zero, %[t5]          \n\t"         // clamp 0
            "sgt         %[t5], %[t4], %[maxX]        \n\t"         //
            "movn        %[t4], %[maxX], %[t5]        \n\t"         // clamp maxX
            "sh          %[t4], 0(%[xy])              \n\t"         //
            "3:                                       \n\t"
            : [t1] "=&r" (t1), [t2] "=&r" (t2), [t3] "=&r" (t3), [t4] "=&r" (t4), [t5] "=&r" (t5)
            : [fx] "r" (fx), [xy] "r" (xy), [count] "r" (count), [dx] "r" (dx), [maxX] "r" (maxX)
            : "memory"
        );
#else
        int i;
        for (i = (count >> 2); i > 0; --i) {
            unsigned a, b;
            a = TILEX_PROCF(fx, maxX); fx += dx;
            b = TILEX_PROCF(fx, maxX); fx += dx;
#ifdef SK_CPU_BENDIAN
            *xy++ = (a << 16) | b;
#else
            *xy++ = (b << 16) | a;
#endif
            a = TILEX_PROCF(fx, maxX); fx += dx;
            b = TILEX_PROCF(fx, maxX); fx += dx;
#ifdef SK_CPU_BENDIAN
            *xy++ = (a << 16) | b;
#else
            *xy++ = (b << 16) | a;
#endif
        }
        uint16_t* xx = (uint16_t*)xy;
        for (i = (count & 3); i > 0; --i) {
            *xx++ = TILEX_PROCF(fx, maxX); fx += dx;
        }
#endif // __mips_dsp
    }
}

// note: we could special-case on a matrix which is skewed in X but not Y.
// this would require a more general setup thatn SCALE does, but could use
// SCALE's inner loop that only looks at dx

void AFFINE_NOFILTER_NAME(const SkBitmapProcState& s,
                                 uint32_t xy[], int count, int x, int y) {
    SkASSERT(s.fInvType & SkMatrix::kAffine_Mask);
    SkASSERT((s.fInvType & ~(SkMatrix::kTranslate_Mask |
                             SkMatrix::kScale_Mask |
                             SkMatrix::kAffine_Mask)) == 0);

    PREAMBLE(s);
    SkPoint srcPt;
    s.fInvProc(*s.fInvMatrix,
               SkIntToScalar(x) + SK_ScalarHalf,
               SkIntToScalar(y) + SK_ScalarHalf, &srcPt);

    SkFixed fx = SkScalarToFixed(srcPt.fX);
    SkFixed fy = SkScalarToFixed(srcPt.fY);
    SkFixed dx = s.fInvSx;
    SkFixed dy = s.fInvKy;
    int maxX = s.fBitmap->width() - 1;
    int maxY = s.fBitmap->height() - 1;

#if defined ( __mips_dsp)
    register int t1, t2, t3;
    __asm__ __volatile__ (
        "sll %[t1], %[maxX], 16                   \n\t"         //
        "blez %[count], 2f                        \n\t"         // check for count <= 0
        "packrl.ph %[t1], %[maxY], %[t1]          \n\t"         // maxY || maxX
        "sll %[t2], %[count], 2                   \n\t"         // count*sizeof(int)
        "addu %[t2], %[xy], %[t2]                 \n\t"         // final address of xy
        "1:                                       \n\t"
        "precrq.ph.w %[t3], %[fy], %[fx]          \n\t"         // fy || fx
        "cmp.lt.ph %[t3], $zero                   \n\t"         //
        "pick.ph %[t3], $zero, %[t3]              \n\t"         // clamp 0
        "cmp.lt.ph %[t1], %[t3]                   \n\t"         //
        "pick.ph %[t3], %[t1], %[t3]              \n\t"         // clamp max
        "sw %[t3], 0(%[xy])                       \n\t"         // store to xy[]
        "addu %[fx], %[fx], %[dx]                 \n\t"         // fx += dx
        "addu %[fy], %[fy], %[dy]                 \n\t"         // fy += dy
        "addu %[xy], %[xy], 4                     \n\t"         // xy += 4
        "bne %[xy], %[t2], 1b                     \n\t"         //
        "2:                                       \n\t"
        : [fx] "+r" (fx), [fy] "+r" (fy), [xy] "+r" (xy), [t1] "=&r" (t1), [t2] "=&r" (t2), [t3] "=&r" (t3)
        : [count] "r" (count), [maxX] "r" (maxX), [maxY] "r" (maxY),
          [dx] "r" (dx), [dy] "r" (dy)
        : "memory"
    );
#else
    for (int i = count; i > 0; --i) {
        *xy++ = (TILEY_PROCF(fy, maxY) << 16) | TILEX_PROCF(fx, maxX);
        fx += dx; fy += dy;
    }
#endif
}

void PERSP_NOFILTER_NAME(const SkBitmapProcState& s,
                                uint32_t* SK_RESTRICT xy,
                                int count, int x, int y) {
    SkASSERT(s.fInvType & SkMatrix::kPerspective_Mask);

    PREAMBLE(s);
    int maxX = s.fBitmap->width() - 1;
    int maxY = s.fBitmap->height() - 1;
#if defined( __mips_dsp)
    register int max32, min32;
    max32 = (maxY << 16) | maxX;
    min32 = 0;
    register int t0, t1;
#endif
    SkPerspIter   iter(*s.fInvMatrix,
                       SkIntToScalar(x) + SK_ScalarHalf,
                       SkIntToScalar(y) + SK_ScalarHalf, count);

    while ((count = iter.next()) != 0) {
        const SkFixed* SK_RESTRICT srcXY = iter.getXY();
#if defined( __mips_dsp)
        __asm__ __volatile__ (
                "1:                                      \n\t"
                "addiu       %[count], %[count], -1      \n\t"
                "bltz        %[count], 2f                \n\t"
                "lw          %[t1], 4(%[srcXY])          \n\t"
                "lw          %[t0], 0(%[srcXY])          \n\t"
                "addiu       %[srcXY], %[srcXY], 8       \n\t"
                "precrq.ph.w %[t0], %[t1], %[t0]         \n\t"
                "cmp.le.ph   %[t0], %[max32]             \n\t"
                "pick.ph     %[t1], %[t0], %[max32]      \n\t"
                "cmp.le.ph   %[t1], %[min32]             \n\t"
                "pick.ph     %[t0], %[min32], %[t1]      \n\t"
                "sw          %[t0], 0(%[xy])             \n\t"
                "addiu       %[xy], %[xy], 4             \n\t"
                "b           1b                          \n\t"
                "2:                                      \n\t"
                : [t0] "=&r" (t0), [t1] "=&r" (t1), [srcXY] "+r" (srcXY), [count] "+r" (count)
                : [max32] "r" (max32), [xy] "r" (xy), [min32] "r" (min32)
                : "memory"
        );
#else
        while (--count >= 0) {
            *xy++ = (TILEY_PROCF(srcXY[1], maxY) << 16) |
                     TILEX_PROCF(srcXY[0], maxX);
            srcXY += 2;
        }
#endif
    }
}

//////////////////////////////////////////////////////////////////////////////

static inline uint32_t PACK_FILTER_Y_NAME(SkFixed f, unsigned max,
                                          SkFixed one PREAMBLE_PARAM_Y) {
    unsigned i = TILEY_PROCF(f, max);
    i = (i << 4) | TILEY_LOW_BITS(f, max);
    return (i << 14) | (TILEY_PROCF((f + one), max));
}

static inline uint32_t PACK_FILTER_X_NAME(SkFixed f, unsigned max,
                                          SkFixed one PREAMBLE_PARAM_X) {
    unsigned i = TILEX_PROCF(f, max);
    i = (i << 4) | TILEX_LOW_BITS(f, max);
    return (i << 14) | (TILEX_PROCF((f + one), max));
}

void SCALE_FILTER_NAME(const SkBitmapProcState& s,
                              uint32_t xy[], int count, int x, int y) {
    SkASSERT((s.fInvType & ~(SkMatrix::kTranslate_Mask |
                             SkMatrix::kScale_Mask)) == 0);
    SkASSERT(s.fInvKy == 0);

    PREAMBLE(s);

    const unsigned maxX = s.fBitmap->width() - 1;
    const SkFixed one = s.fFilterOneX;
    const SkFixed dx = s.fInvSx;
    SkFixed fx;

    {
        SkPoint pt;
        s.fInvProc(*s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                                  SkIntToScalar(y) + SK_ScalarHalf, &pt);
        const SkFixed fy = SkScalarToFixed(pt.fY) - (s.fFilterOneY >> 1);
        const unsigned maxY = s.fBitmap->height() - 1;
        // compute our two Y values up front
        *xy++ = PACK_FILTER_Y_NAME(fy, maxY, s.fFilterOneY PREAMBLE_ARG_Y);
        // now initialize fx
        fx = SkScalarToFixed(pt.fX) - (one >> 1);
    }

#ifdef CHECK_FOR_DECAL
    // test if we don't need to apply the tile proc
    if (dx > 0 &&
            (unsigned)(fx >> 16) <= maxX &&
            (unsigned)((fx + dx * (count - 1)) >> 16) < maxX) {
        decal_filter_scale(xy, fx, dx, count);
    } else
#endif
    {
#if defined ( __mips_dsp)
        register int t1, t2, t3, t4, t5;
        __asm__ __volatile__ (
            "sll    %[t1], %[count], 2                \n\t"         // count*sizeof(int)
            "addu   %[t1], %[xy], %[t1]               \n\t"         // final address of xy
            "1:                                       \n\t"
            "sra    %[t2], %[fx], 16                  \n\t"         // fx[31..16]
            "slt    %[t3], %[t2], $zero               \n\t"         //
            "movn   %[t2], $zero, %[t3]               \n\t"         // clamp 0
            "sgt    %[t3], %[t2], %[maxX]             \n\t"         //
            "movn   %[t2], %[maxX], %[t3]             \n\t"         // clamp maxX
            "ext    %[t4], %[fx], 12, 4               \n\t"         // fx[15..12]
#ifdef __mips_dspr2
            "append %[t2], %[t4], 4                   \n\t"         //
#else
            "sll    %[t2], %[t2], 4                   \n\t"         //
            "or     %[t2], %[t2], %[t4]               \n\t"         //
#endif
            "addu   %[t4], %[fx], %[one]              \n\t"         // fx + oneX
            "sra    %[t5], %[t4], 16                  \n\t"         // (fx + oneX)[31..16]
            "slt    %[t3], %[t5], $zero               \n\t"         //
            "movn   %[t5], $zero, %[t3]               \n\t"         // clamp 0
            "sgt    %[t3], %[t5], %[maxX]             \n\t"         //
            "movn   %[t5], %[maxX], %[t3]             \n\t"         // clamp maxX
            "sll    %[t2], %[t2], 14                  \n\t"         //
            "or     %[t2], %[t2], %[t5]               \n\t"         //
            "sw     %[t2], 0(%[xy])                   \n\t"         // store to xy[]

            "addu   %[xy], %[xy], 4                   \n\t"         // xy += 4
            "addu   %[fx], %[fx], %[dx]               \n\t"         // fx += dx
            "bne    %[xy], %[t1], 1b                  \n\t"         //
            "2:                                       \n\t"
            : [t1] "=&r" (t1), [t2] "=&r" (t2), [t3] "=&r" (t3), [t4] "=&r" (t4), [t5] "=&r" (t5)
            : [fx] "r" (fx), [xy] "r" (xy), [count] "r" (count), [maxX] "r" (maxX), [dx] "r" (dx), [one] "r" (one)
            : "memory"
        );
#else
        do {
            *xy++ = PACK_FILTER_X_NAME(fx, maxX, one PREAMBLE_ARG_X);
            fx += dx;
        } while (--count != 0);
#endif // __mips_dsp
    }
}

void AFFINE_FILTER_NAME(const SkBitmapProcState& s,
                               uint32_t xy[], int count, int x, int y) {
    SkASSERT(s.fInvType & SkMatrix::kAffine_Mask);
    SkASSERT((s.fInvType & ~(SkMatrix::kTranslate_Mask |
                             SkMatrix::kScale_Mask |
                             SkMatrix::kAffine_Mask)) == 0);

    PREAMBLE(s);
    SkPoint srcPt;
    s.fInvProc(*s.fInvMatrix,
               SkIntToScalar(x) + SK_ScalarHalf,
               SkIntToScalar(y) + SK_ScalarHalf, &srcPt);

    SkFixed oneX = s.fFilterOneX;
    SkFixed oneY = s.fFilterOneY;
    SkFixed fx = SkScalarToFixed(srcPt.fX) - (oneX >> 1);
    SkFixed fy = SkScalarToFixed(srcPt.fY) - (oneY >> 1);
    SkFixed dx = s.fInvSx;
    SkFixed dy = s.fInvKy;
    unsigned maxX = s.fBitmap->width() - 1;
    unsigned maxY = s.fBitmap->height() - 1;

#if defined( __mips__)
    register int t1, t2, t3, t4, t5;
    __asm__ __volatile__ (
        "sll %[t1], %[count], 3                   \n\t"         // count*2*sizeof(int)
        "addu %[t1], %[xy], %[t1]                 \n\t"         // final address of xy
        "1:                                       \n\t"
        "sra %[t2], %[fy], 16                     \n\t"         // fy[31..16]
        "slt %[t3], %[t2], $zero                  \n\t"         //
        "movn %[t2], $zero, %[t3]                 \n\t"         // clamp 0
        "sgt %[t3], %[t2], %[maxY]                \n\t"         //
        "movn %[t2], %[maxY], %[t3]               \n\t"         // clamp maxY
        "ext %[t4], %[fy], 12, 4                  \n\t"         // fy[15..12]
#ifdef __mips_dspr2
        "append %[t2], %[t4], 4                   \n\t"         //
#else
        "sll %[t2], %[t2], 4                      \n\t"         //
        "or %[t2], %[t2], %[t4]                   \n\t"         //
#endif
        "addu %[t4], %[fy], %[oneY]               \n\t"         // fy + oneY
        "sra %[t5], %[t4], 16                     \n\t"         // (fy + oneY)[31..16]
        "slt %[t3], %[t5], $zero                  \n\t"         //
        "movn %[t5], $zero, %[t3]                 \n\t"         // clamp 0
        "sgt %[t3], %[t5], %[maxY]                \n\t"         //
        "movn %[t5], %[maxY], %[t3]               \n\t"         // clamp maxY
        "sll %[t2], %[t2], 14                     \n\t"         //
        "or %[t2], %[t2], %[t5]                   \n\t"         //
        "sw %[t2], 0(%[xy])                       \n\t"         // store to xy[]

        "sra %[t2], %[fx], 16                     \n\t"         // fx[31..16]
        "slt %[t3], %[t2], $zero                  \n\t"         //
        "movn %[t2], $zero, %[t3]                 \n\t"         // clamp 0
        "sgt %[t3], %[t2], %[maxX]                \n\t"         //
        "movn %[t2], %[maxX], %[t3]               \n\t"         // clamp maxX
        "ext %[t4], %[fx], 12, 4                  \n\t"         // fx[15..12]
#ifdef __mips_dspr2
        "append %[t2], %[t4], 4                   \n\t"         //
#else
        "sll %[t2], %[t2], 4                      \n\t"         //
        "or %[t2], %[t2], %[t4]                   \n\t"         //
#endif
        "addu %[t4], %[fx], %[oneX]               \n\t"         // fx + oneX
        "sra %[t5], %[t4], 16                     \n\t"         // (fx + oneX)[31..16]
        "slt %[t3], %[t5], $zero                  \n\t"         //
        "movn %[t5], $zero, %[t3]                 \n\t"         // clamp 0
        "sgt %[t3], %[t5], %[maxX]                \n\t"         //
        "movn %[t5], %[maxX], %[t3]               \n\t"         // clamp maxX
        "sll %[t2], %[t2], 14                     \n\t"         //
        "or %[t2], %[t2], %[t5]                   \n\t"         //
        "sw %[t2], 4(%[xy])                       \n\t"         // store to xy[]

        "addu %[xy], %[xy], 8                     \n\t"         // xy += 8
        "addu %[fx], %[fx], %[dx]                 \n\t"         // fx += dx
        "addu %[fy], %[fy], %[dy]                 \n\t"         // fy += dy
        "bne %[xy], %[t1], 1b                     \n\t"         //
        "2:                                       \n\t"
        : [fx] "+r" (fx), [fy] "+r" (fy), [xy] "+r" (xy), [t1] "=&r" (t1), [t2] "=&r" (t2),
          [t3] "=&r" (t3), [t4] "=&r" (t4), [t5] "=&r" (t5)
        : [count] "r" (count), [maxX] "r" (maxX), [maxY] "r" (maxY), [dx] "r" (dx), [dy] "r" (dy),
          [oneX] "r" (oneX), [oneY] "r" (oneY)
        : "memory"
    );
#else
    do {
        *xy++ = PACK_FILTER_Y_NAME(fy, maxY, oneY PREAMBLE_ARG_Y);
        fy += dy;
        *xy++ = PACK_FILTER_X_NAME(fx, maxX, oneX PREAMBLE_ARG_X);
        fx += dx;
    } while (--count != 0);
#endif
}

void PERSP_FILTER_NAME(const SkBitmapProcState& s,
                              uint32_t* SK_RESTRICT xy, int count,
                              int x, int y) {
    SkASSERT(s.fInvType & SkMatrix::kPerspective_Mask);

    PREAMBLE(s);
    unsigned maxX = s.fBitmap->width() - 1;
    unsigned maxY = s.fBitmap->height() - 1;
    SkFixed oneX = s.fFilterOneX;
    SkFixed oneY = s.fFilterOneY;

    SkPerspIter   iter(*s.fInvMatrix,
                       SkIntToScalar(x) + SK_ScalarHalf,
                       SkIntToScalar(y) + SK_ScalarHalf, count);

    while ((count = iter.next()) != 0) {
        const SkFixed* SK_RESTRICT srcXY = iter.getXY();
#if defined( __mips_dsp)
    register int max32, min32;
    max32 = (maxY << 16) | maxX;
    min32 = 0;
    register int t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10;
    __asm__ __volatile__ (
        "sra         %[t3], %[oneY], 1           \n\t"
        "sra         %[t10], %[oneX], 1          \n\t"
        "1:                                      \n\t"
        "addiu       %[count], %[count], -1      \n\t"
        "bltz        %[count], 2f                \n\t"
        "lw          %[t1], 4(%[srcXY])          \n\t"
        "lw          %[t0], 0(%[srcXY])          \n\t"
        "addiu       %[srcXY], %[srcXY], 8       \n\t"
        "subu        %[t5], %[t1], %[t3]         \n\t"  // t5: y1 prepared for clamping
        "subu        %[t4], %[t0], %[t10]        \n\t"  // t4: x1 prepared for clamping
        "precrq.ph.w %[t0], %[t5], %[t4]         \n\t"
        "cmp.le.ph   %[t0], %[max32]             \n\t"
        "pick.ph     %[t1], %[t0], %[max32]      \n\t"
        "cmp.le.ph   %[t1], %[min32]             \n\t"
        "pick.ph     %[t0], %[min32], %[t1]      \n\t"  // t0: clamp_y1 || clamp_x1
        "addu        %[t7], %[t5], %[oneY]       \n\t"  // t7: y2 prepared for clamping
        "addu        %[t6], %[t4], %[oneX]       \n\t"  // t6: x2 prepared for clamping
        "precrq.ph.w %[t1], %[t7], %[t6]         \n\t"
        "cmp.le.ph   %[t1], %[max32]             \n\t"
        "pick.ph     %[t2], %[t1], %[max32]      \n\t"
        "cmp.le.ph   %[t2], %[min32]             \n\t"
        "pick.ph     %[t1], %[min32], %[t2]      \n\t"  // t1: clamp_y2 || clamp_x2

        "sra         %[t2], %[t5], 12            \n\t"
        "andi        %[t2], %[t2], 0xF           \n\t"
        "sra         %[t8], %[t0], 16            \n\t"  // t8: [0..0] || clamp_y1
        "sra         %[t9], %[t1], 16            \n\t"  // t9: [0..0] || clamp_y2
#ifdef __mips_dspr2
        "append      %[t8], %[t2], 4                  \n\t"
#else
        "sll         %[t8], %[t8], 4             \n\t"
        "or          %[t8], %[t8], %[t2]         \n\t"
#endif
        "sll         %[t8], %[t8], 14            \n\t"
        "or          %[t6], %[t8], %[t9]         \n\t"
        "sw          %[t6], 0(%[xy])             \n\t"

        "sra         %[t2], %[t4], 12            \n\t"
        "andi        %[t2], %[t2], 0xF           \n\t"
        "andi        %[t8], %[t0], 0xFFFF        \n\t"  // t8: [0..0] || clamp_x1
        "andi        %[t9], %[t1], 0xFFFF        \n\t"  // t9: [0..0] || clamp_x2
#ifdef __mips_dspr2
        "append      %[t8], %[t2], 4                  \n\t"
#else
        "sll         %[t8], %[t8], 4             \n\t"
        "or          %[t8], %[t8], %[t2]         \n\t"
#endif
        "sll         %[t8], %[t8], 14            \n\t"
        "or          %[t6], %[t8], %[t9]         \n\t"
        "sw          %[t6], 4(%[xy])             \n\t"
        "addiu       %[xy], %[xy], 8             \n\t"

        "b           1b                          \n\t"
        "2:                                      \n\t"
        : [t0] "+r" (t0), [t1] "+r" (t1), [t2] "+r" (t2), [t3] "+r" (t3),
          [t4] "+r" (t4), [t5] "+r" (t5), [t6] "+r" (t6), [t7] "+r" (t7), [t8] "+r" (t8),
          [t9] "+r" (t9), [t10] "=&r" (t10), [srcXY] "+r" (srcXY), [count] "+r" (count)
        : [xy] "r" (xy), [max32] "r" (max32), [min32] "r" (min32), [oneY] "r" (oneY),
          [oneX] "r" (oneX)
        : "memory"
    );
#else
        do {
            *xy++ = PACK_FILTER_Y_NAME(srcXY[1] - (oneY >> 1), maxY,
                                       oneY PREAMBLE_ARG_Y);
            *xy++ = PACK_FILTER_X_NAME(srcXY[0] - (oneX >> 1), maxX,
                                       oneX PREAMBLE_ARG_X);
            srcXY += 2;
        } while (--count != 0);
#endif // __mips_dsp
    }
}

static SkBitmapProcState::MatrixProc MAKENAME(_Procs)[] = {
    SCALE_NOFILTER_NAME,
    SCALE_FILTER_NAME,
    AFFINE_NOFILTER_NAME,
    AFFINE_FILTER_NAME,
    PERSP_NOFILTER_NAME,
    PERSP_FILTER_NAME
};

#undef MAKENAME
#undef TILEX_PROCF
#undef TILEY_PROCF
#ifdef CHECK_FOR_DECAL
    #undef CHECK_FOR_DECAL
#endif

#undef SCALE_NOFILTER_NAME
#undef SCALE_FILTER_NAME
#undef AFFINE_NOFILTER_NAME
#undef AFFINE_FILTER_NAME
#undef PERSP_NOFILTER_NAME
#undef PERSP_FILTER_NAME

#undef PREAMBLE
#undef PREAMBLE_PARAM_X
#undef PREAMBLE_PARAM_Y
#undef PREAMBLE_ARG_X
#undef PREAMBLE_ARG_Y

#undef TILEX_LOW_BITS
#undef TILEY_LOW_BITS
