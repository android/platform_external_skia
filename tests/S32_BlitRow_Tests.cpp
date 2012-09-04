#include "Test.h"
#include "SkBlitRow.h"
#include "SkBitmap.h"
#include "SkColorPriv.h"
#include "SkDither.h"
#include "SkRandom.h"

#define TEST_DUR    101

/*REFERENT FUNCTIONS*/
static void S32_D565_Blend_referent(uint16_t* SK_RESTRICT dst,
                             const SkPMColor* SK_RESTRICT src, int count,
                             U8CPU alpha, int /*x*/, int /*y*/) {
    SkASSERT(255 > alpha);

    if (count > 0) {
        int scale = SkAlpha255To256(alpha);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            SkASSERT(SkGetPackedA32(c) == 255);
            uint16_t d = *dst;
            *dst++ = SkPackRGB16(
                    SkAlphaBlend(SkPacked32ToR16(c), SkGetPackedR16(d), scale),
                    SkAlphaBlend(SkPacked32ToG16(c), SkGetPackedG16(d), scale),
                    SkAlphaBlend(SkPacked32ToB16(c), SkGetPackedB16(d), scale));
        } while (--count != 0);
    }
}
static void S32A_D565_Opaque_Dither_referent(uint16_t* SK_RESTRICT dst,
                                      const SkPMColor* SK_RESTRICT src,
                                      int count, U8CPU alpha, int x, int y) {
    SkASSERT(255 == alpha);

    if (count > 0) {
        DITHER_565_SCAN(y);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            if (c) {
                unsigned a = SkGetPackedA32(c);

                int d = SkAlphaMul(DITHER_VALUE(x), SkAlpha255To256(a));

                unsigned sr = SkGetPackedR32(c);
                unsigned sg = SkGetPackedG32(c);
                unsigned sb = SkGetPackedB32(c);
                sr = SkDITHER_R32_FOR_565(sr, d);
                sg = SkDITHER_G32_FOR_565(sg, d);
                sb = SkDITHER_B32_FOR_565(sb, d);

                uint32_t src_expanded = (sg << 24) | (sr << 13) | (sb << 2);
                uint32_t dst_expanded = SkExpand_rgb_16(*dst);
                dst_expanded = dst_expanded * (SkAlpha255To256(255 - a) >> 3);
                // now src and dst expanded are in g:11 r:10 x:1 b:10
                *dst = SkCompact_rgb_16((src_expanded + dst_expanded) >> 5);
            }
            dst += 1;
            DITHER_INC_X(x);
        } while (--count != 0);
    }
}

static void S32_D565_Opaque_Dither_referent(uint16_t* __restrict__ dst,
                                     const SkPMColor* __restrict__ src,
                                     int count, U8CPU alpha, int x, int y) {
    SkASSERT(255 == alpha);

    if (count > 0) {
        DITHER_565_SCAN(y);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            SkASSERT(SkGetPackedA32(c) == 255);

            unsigned dither = DITHER_VALUE(x);
            *dst++ = SkDitherRGB32To565(c, dither);
            DITHER_INC_X(x);
        } while (--count != 0);
    }
}

static void S32A_D565_Opaque_referent(uint16_t* SK_RESTRICT dst,
                               const SkPMColor* SK_RESTRICT src, int count,
                               U8CPU alpha, int /*x*/, int /*y*/) {
    SkASSERT(255 == alpha);

    if (count > 0) {
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            if (c) {
                *dst = SkSrcOver32To16(c, *dst);
            }
            dst += 1;
        } while (--count != 0);
    }
}

static void S32_D565_Blend_Dither_referent(uint16_t* SK_RESTRICT dst,
                                    const SkPMColor* SK_RESTRICT src,
                                    int count, U8CPU alpha, int x, int y) {
    SkASSERT(255 > alpha);

    if (count > 0) {
        int scale = SkAlpha255To256(alpha);
        DITHER_565_SCAN(y);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            SkASSERT(SkGetPackedA32(c) == 255);

            int dither = DITHER_VALUE(x);
            int sr = SkGetPackedR32(c);
            int sg = SkGetPackedG32(c);
            int sb = SkGetPackedB32(c);
            sr = SkDITHER_R32To565(sr, dither);
            sg = SkDITHER_G32To565(sg, dither);
            sb = SkDITHER_B32To565(sb, dither);

            uint16_t d = *dst;
            *dst++ = SkPackRGB16(SkAlphaBlend(sr, SkGetPackedR16(d), scale),
                                 SkAlphaBlend(sg, SkGetPackedG16(d), scale),
                                 SkAlphaBlend(sb, SkGetPackedB16(d), scale));
            DITHER_INC_X(x);
        } while (--count != 0);
    }
}

static void S32A_D565_Blend_referent(uint16_t* SK_RESTRICT dst,
                              const SkPMColor* SK_RESTRICT src, int count,
                               U8CPU alpha, int /*x*/, int /*y*/) {
    SkASSERT(255 > alpha);

    if (count > 0) {
        do {
            SkPMColor sc = *src++;
            SkPMColorAssert(sc);
            if (sc) {
                uint16_t dc = *dst;
                unsigned dst_scale = 255 - SkMulDiv255Round(SkGetPackedA32(sc), alpha);
                unsigned dr = SkMulS16(SkPacked32ToR16(sc), alpha) + SkMulS16(SkGetPackedR16(dc), dst_scale);
                unsigned dg = SkMulS16(SkPacked32ToG16(sc), alpha) + SkMulS16(SkGetPackedG16(dc), dst_scale);
                unsigned db = SkMulS16(SkPacked32ToB16(sc), alpha) + SkMulS16(SkGetPackedB16(dc), dst_scale);
                *dst = SkPackRGB16(SkDiv255Round(dr), SkDiv255Round(dg), SkDiv255Round(db));
            }
            dst += 1;
        } while (--count != 0);
    }
}

static void S32_Blend_BlitRow32_referent(SkPMColor* SK_RESTRICT dst,
                                const SkPMColor* SK_RESTRICT src,
                                int count, U8CPU alpha) {
    SkASSERT(alpha <= 255);
    if (count > 0) {
        unsigned src_scale = SkAlpha255To256(alpha);
        unsigned dst_scale = 256 - src_scale;
        do {
            *dst = SkAlphaMulQ(*src, src_scale) + SkAlphaMulQ(*dst, dst_scale);
            src += 1;
            dst += 1;
        } while (--count > 0);
    }
}

/*TEST FUNCTIONS*/
static void test_S32_D565_Blend(skiatest::Reporter* reporter) {

SkBlitRow::Proc proc = NULL;
proc = SkBlitRow::Factory(1, SkBitmap::kRGB_565_Config); //were calling S32_D565_Blend_PROC

    int count_xy = 10;
    uint16_t dst[TEST_DUR];
    uint16_t dst_opt[TEST_DUR];
    SkPMColor src[TEST_DUR];
    U8CPU alpha;
    alpha = 253;
    int x, y;
    uint8_t r, g, b;
    SkRandom rand;
    SkString str;

    for (int j = 0; j < count_xy; j++) {
        for (int i = 0; i < TEST_DUR; i++) {
            r = rand.nextU();
            g = rand.nextU();
            b = rand.nextU();
            src[i] = SkColorSetRGB(r, g, b);
        }
        x = static_cast<int>(rand.nextU());
        y = static_cast<int>(rand.nextU());
        memset(dst, 0, TEST_DUR * 2);
        memset(dst_opt, 0, TEST_DUR * 2);

        proc(dst_opt, src, TEST_DUR, alpha, x, y);
        S32_D565_Blend_referent(dst, src, TEST_DUR, alpha, x, y);

        if ( memcmp((void*)dst_opt, (void*)dst, TEST_DUR * 2)!=0 ) {
            str.printf("test_S32_D565_Blend ERROR!\nalpha: %X\n", alpha);
            reporter->reportFailed(str);
        }
    }
}
static void test_S32A_D565_Opaque_Dither(skiatest::Reporter* reporter) {
    SkBlitRow::Proc proc = NULL;
    proc = SkBlitRow::Factory(6, SkBitmap::kRGB_565_Config); //were calling S32A_D565_Opaque_Dither_PROC

    int count_xy = 10;
    uint16_t dst[TEST_DUR];
    uint16_t dst_opt[TEST_DUR];
    SkPMColor src[TEST_DUR];
    U8CPU alpha;
    alpha = 255;
    int x, y;
    uint8_t r, g, b;
    SkRandom rand;
    SkString str;

    for (int j = 0; j < count_xy; j++) {
        for (int i = 0; i < TEST_DUR; i++) {
            r = rand.nextU();
            g = rand.nextU();
            b = rand.nextU();
            src[i] = SkColorSetRGB(r, g, b);
        }
        x = static_cast<int>(rand.nextU());
        y = static_cast<int>(rand.nextU());
        memset(dst, 0, TEST_DUR * 2);
        memset(dst_opt, 0, TEST_DUR * 2);

        S32A_D565_Opaque_Dither_referent(dst, src, TEST_DUR, alpha, x, y);
        proc(dst_opt, src, TEST_DUR, alpha, x, y);

        if ( memcmp((void*)dst, (void*)dst_opt, TEST_DUR * 2)!=0 ) {
            str.printf("test_S32A_D565_Opaque_Dither ERROR!\nx: %X, y: %X, alpha: %X\n", x, y, alpha);
            reporter->reportFailed(str);
        }
    }
}
static void test_S32_D565_Opaque_Dither(skiatest::Reporter* reporter) {
    SkBlitRow::Proc proc = NULL;
    proc = SkBlitRow::Factory(4, SkBitmap::kRGB_565_Config); //were calling S32_D565_Opaque_Dither_PROC

    int count_xy = 10;
    uint16_t dst[TEST_DUR];
    uint16_t dst_opt[TEST_DUR];
    SkPMColor src[TEST_DUR];
    U8CPU alpha;
    alpha = 255;
    int x, y;
    uint8_t r, g, b;
    SkRandom rand;
    SkString str;

    for (int j = 0; j < count_xy; j++) {
        for (int i = 0; i < TEST_DUR; i++) {
            r = rand.nextU();
            g = rand.nextU();
            b = rand.nextU();
            src[i] = SkColorSetRGB(r, g, b);
        }
        x = static_cast<int>(rand.nextU());
        y = static_cast<int>(rand.nextU());

        proc(dst_opt, src, TEST_DUR, alpha, x, y);
        S32_D565_Opaque_Dither_referent(dst, src, TEST_DUR, alpha, x, y);

        if ( memcmp((void*)dst, (void*)dst_opt, TEST_DUR)!=0 ) {
            str.printf("test_S32_D565_Opaque_Dither ERROR!\nx: %X, y: %X, alpha: %X\n", x, y, alpha);
            reporter->reportFailed(str);
        }
    }
}

static void test_S32A_D565_Opaque(skiatest::Reporter* reporter) {
    SkBlitRow::Proc proc_opt = NULL;
    proc_opt = SkBlitRow::Factory(2, SkBitmap::kRGB_565_Config); //were calling S32A_D565_Opaque_PROC

    int count_xy = 10;
    uint16_t dst[TEST_DUR];
    uint16_t dst_opt[TEST_DUR];
    SkPMColor src[TEST_DUR];
    U8CPU alpha;
    int x, y;
    uint8_t r, g, b;
    SkRandom rand;
    SkString str;
    x = 0;
    y = 0;
    alpha = 255;

    for (int j = 0; j < count_xy; j++) {
        for (int i = 0; i < TEST_DUR; i++) {
            r = rand.nextU();
            g = rand.nextU();
            b = rand.nextU();
            src[i] = SkColorSetRGB(r, g, b);
        }

        proc_opt(dst_opt, src, TEST_DUR, alpha, x, y);
        S32A_D565_Opaque_referent(dst, src, TEST_DUR, alpha, x, y);

        if ( memcmp((void*)dst, (void*)dst_opt, TEST_DUR)!=0 ) {
            str.printf("test_S32A_D565_Opaque ERROR!\nx: %X, y: %X, alpha: %X\n", x, y, alpha);
            reporter->reportFailed(str);
        }
    }
}

static void test_S32_D565_Blend_Dither(skiatest::Reporter* reporter) {
    SkBlitRow::Proc proc = NULL;
    proc = SkBlitRow::Factory(5, SkBitmap::kRGB_565_Config); //were calling S32_D565_Blend_Dither_PROC

    int count1 = 5;
    uint16_t dst[TEST_DUR];
    uint16_t dst_opt[TEST_DUR];
    SkPMColor src[TEST_DUR];
    U8CPU alpha;
    alpha = 155;
    int x, y;
    int r;
    SkRandom rand;
    SkString str;

    for (int j = 0; j < count1; j++) {
      x = static_cast<int>(rand.nextU());
      y = static_cast<int>(rand.nextU());
      alpha = static_cast<U8CPU>(rand.nextBits(8));
      if (alpha == 255) alpha = 1;
      for (int i = 0; i < TEST_DUR; i++ ) {
        src[i] = static_cast<SkPMColor>(rand.nextU());
        dst[i] = dst_opt[i];
      }

        proc(dst_opt, src, TEST_DUR, alpha, x, y);
        S32_D565_Blend_Dither_referent(dst, src, TEST_DUR, alpha, x, y);

        if ( memcmp((void*)dst, (void*)dst_opt, TEST_DUR)!=0 ) {
            str.printf("test_S32_D565_Blend_Dither ERROR!\nx: %X, y: %X, alpha: %X\n", x, y, alpha);
            reporter->reportFailed(str);
        }
    }
}

static void test_S32A_D565_Blend(skiatest::Reporter* reporter) {
    SkBlitRow::Proc proc = NULL;
    proc = SkBlitRow::Factory(3, SkBitmap::kRGB_565_Config); //were calling S32_D565_Blend_PROC

    int count_a = 10;
    uint16_t dst[TEST_DUR];
    uint16_t dst_opt[TEST_DUR];
    SkPMColor src[TEST_DUR];
    U8CPU alpha;
    alpha = 155;
    int x, y;
    uint8_t r, g, b;
    SkRandom rand;
    x = 0;
    y = 0;

    for (int j = 0; j < count_a; j++) {
        for (int i = 0; i < TEST_DUR; i++) {
            r = rand.nextU();
            g = rand.nextU();
            b = rand.nextU();
            src[i] = SkColorSetRGB(r, g, b);
        }
        alpha = static_cast<U8CPU>(rand.nextBits(8));
        if (alpha == 0) alpha = 123;

        proc(dst_opt, src, TEST_DUR, alpha, x, y);
        S32A_D565_Blend_referent(dst, src, TEST_DUR, alpha, x, y);

        if ( memcmp((void*)dst, (void*)dst_opt, TEST_DUR)!=0 ) {
            SkString str;
            str.printf("test_S32_D565_Blend ERROR!\nx: %X, y: %X, alpha: %X\n", x, y, alpha);
            reporter->reportFailed(str);
        }
    }
}

static void test_S32_Blend_BlitRow32(skiatest::Reporter* reporter) {
    SkBlitRow::Proc32 proc32 = NULL;
    proc32 = SkBlitRow::Factory32(1); //were calling S32_Blend_BlitRow32_PROC
    int count_a = 10;
    SkPMColor dst[TEST_DUR];
    SkPMColor dst_opt[TEST_DUR];
    SkPMColor src[TEST_DUR];
    U8CPU alpha;
    SkRandom rand;
    SkString str;
    uint8_t r, g, b;

    for(int j = 0; j < count_a; j++) {
        for (int i = 0; i < TEST_DUR; i++) {
            r = rand.nextU();
            g = rand.nextU();
            b = rand.nextU();
            src[i] = SkColorSetRGB(r, g, b);
            dst[i] = static_cast<SkPMColor>(rand.nextU());
            dst_opt[i] = dst[i];
        }
        alpha = static_cast<U8CPU>(rand.nextBits(8));
        if (alpha == 0) alpha = 223;

        proc32(dst_opt, src, TEST_DUR, alpha);
        S32_Blend_BlitRow32_referent(dst, src, TEST_DUR, alpha);

        if ( memcmp((void*)dst, (void*)dst_opt, TEST_DUR)!=0 ) {
            str.printf("test_S32_Blend_BlitRow32 ERROR!\nalpha: %X\n", alpha);
            reporter->reportFailed(str);
        }
    }
}

//RUN SOME TESTS:
static void test_S32(skiatest::Reporter* reporter) {
    test_S32_D565_Blend(reporter);
    test_S32A_D565_Opaque_Dither(reporter);
    test_S32_D565_Opaque_Dither(reporter);
    test_S32A_D565_Opaque(reporter);
    test_S32_D565_Blend_Dither(reporter);
    test_S32A_D565_Blend(reporter);
    test_S32_Blend_BlitRow32(reporter);
}

#include "TestClassDef.h"
DEFINE_TESTCLASS("S32_BlitRow", TestS32_BlitRowClass, test_S32)

