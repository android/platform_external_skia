#include "Test.h"
#include "SkRandom.h"
#include "SkBitmapProcState_filter.h"

#define TEST_DUR 10

/*REFERENT FUNCTIONS*/
static inline void Filter_32_alpha_referent(unsigned x, unsigned y,
                                            SkPMColor a00, SkPMColor a01,
                                            SkPMColor a10, SkPMColor a11,
                                            SkPMColor* dstColor,
                                            unsigned alphaScale) {
    SkASSERT((unsigned)x <= 0xF);
    SkASSERT((unsigned)y <= 0xF);
    SkASSERT(alphaScale <= 256);

    int xy = x * y;
    uint32_t mask = gMask_00FF00FF; //0xFF00FF;

    int scale = 256 - 16*y - 16*x + xy;
    uint32_t lo = (a00 & mask) * scale;
    uint32_t hi = ((a00 >> 8) & mask) * scale;

    scale = 16*x - xy;
    lo += (a01 & mask) * scale;
    hi += ((a01 >> 8) & mask) * scale;

    scale = 16*y - xy;
    lo += (a10 & mask) * scale;
    hi += ((a10 >> 8) & mask) * scale;

    lo += (a11 & mask) * xy;
    hi += ((a11 >> 8) & mask) * xy;

    lo = ((lo >> 8) & mask) * alphaScale;
    hi = ((hi >> 8) & mask) * alphaScale;

    *dstColor = ((lo >> 8) & mask) | (hi & ~mask);
}

static inline void Filter_32_opaque_referent(unsigned x, unsigned y,
                                             SkPMColor a00, SkPMColor a01,
                                             SkPMColor a10, SkPMColor a11,
                                             SkPMColor* dstColor) {
    SkASSERT((unsigned)x <= 0xF);
    SkASSERT((unsigned)y <= 0xF);

    int xy = x * y;
    uint32_t mask = gMask_00FF00FF; //0xFF00FF;

    int scale = 256 - 16*y - 16*x + xy;
    uint32_t lo = (a00 & mask) * scale;
    uint32_t hi = ((a00 >> 8) & mask) * scale;

    scale = 16*x - xy;
    lo += (a01 & mask) * scale;
    hi += ((a01 >> 8) & mask) * scale;

    scale = 16*y - xy;
    lo += (a10 & mask) * scale;
    hi += ((a10 >> 8) & mask) * scale;

    lo += (a11 & mask) * xy;
    hi += ((a11 >> 8) & mask) * xy;

    *dstColor = ((lo >> 8) & mask) | (hi & ~mask);
}

/*TEST FUNCTIONS*/
static void test_Filter_32_alpha(skiatest::Reporter* reporter) {
    uint32_t a00, a01, a10, a11;
    unsigned x, y;
    unsigned alphaScale[TEST_DUR];
    SkPMColor dst[TEST_DUR];
    SkPMColor dst_opt[TEST_DUR];

    SkPMColor dst_one;
    SkPMColor dst_opt_one;

    int r;
    SkRandom rand;
    SkString str;

    for (int i = 0; i < TEST_DUR; i++ ) {
        a00 = static_cast<SkPMColor>(rand.nextU());
        a01 = static_cast<SkPMColor>(rand.nextU());
        a10 = static_cast<SkPMColor>(rand.nextU());
        a11 = static_cast<SkPMColor>(rand.nextU());
        x = static_cast<unsigned>(rand.nextBits(4));
        y = static_cast<unsigned>(rand.nextBits(4));
        alphaScale[i] = static_cast<unsigned>(rand.nextBits(8));

        Filter_32_alpha_referent(x, y, a00, a01, a10, a11, &(dst_one), alphaScale[i]);
        Filter_32_alpha(x, y, a00, a01, a10, a11, &(dst_opt_one), alphaScale[i]);

        if (dst_one != dst_opt_one) {
            str.printf("test_Filter_32_alpha ERROR!x: %d, y: %d, a00: %d, a01: %d, a10: %d, a11: %d, alpha: %d\n"
                       ,x, y, a00, a01, a10, a11, alphaScale);
            reporter->reportFailed(str);
    }
  }
}

static void test_Filter_32_opaque(skiatest::Reporter* reporter) {
    uint32_t a00, a01, a10, a11;
    unsigned x, y;
    SkPMColor dst_one;
    SkPMColor dst_opt_one;

    int r;
    SkRandom rand;
    SkString str;

    for (int i = 0; i < TEST_DUR; i++ ) {
        a00 = static_cast<SkPMColor>(rand.nextU());
        a01 = static_cast<SkPMColor>(rand.nextU());
        a10 = static_cast<SkPMColor>(rand.nextU());
        a11 = static_cast<SkPMColor>(rand.nextU());
        x = static_cast<unsigned>(rand.nextBits(4));
        y = static_cast<unsigned>(rand.nextBits(4));

        Filter_32_opaque_referent(x, y, a00, a01, a10, a11, &(dst_one));
        Filter_32_opaque(x, y, a00, a01, a10, a11, &(dst_opt_one));

        if (dst_one != dst_opt_one) {
            str.printf("test_Filter_32_opaque ERROR!x: %d, y: %d, a00: %d, a01: %d, a10: %d, a11: %d\n",x, y, a00, a01, a10, a11);
            reporter->reportFailed(str);
        }
    }
}

//RUN SOME TESTS:
static void test_Filter32(skiatest::Reporter* reporter) {
    test_Filter_32_alpha(reporter);
    test_Filter_32_opaque(reporter);
}

#include "TestClassDef.h"
DEFINE_TESTCLASS("Filter_32", Filter32Class, test_Filter32)

