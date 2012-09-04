#include "Test.h"
#include "SkBitmapProcState.h"
#include "SkBitmap.h"
#include "SkUtils.h"
#include "SkColorPriv.h"
#include "SkDither.h"
#include "SkRandom.h"
#include "SkCanvas.h"
#include "SkBitmapProcShader.h"
#include "SkBitmapProcState_filter.h"

// returns expanded * 5bits
static inline uint32_t Filter_565_Expanded(unsigned x, unsigned y,
                                           uint32_t a00, uint32_t a01,
                                           uint32_t a10, uint32_t a11) {
    SkASSERT((unsigned)x <= 0xF);
    SkASSERT((unsigned)y <= 0xF);

    a00 = SkExpand_rgb_16(a00);
    a01 = SkExpand_rgb_16(a01);
    a10 = SkExpand_rgb_16(a10);
    a11 = SkExpand_rgb_16(a11);

    int xy = x * y >> 3;
    return  a00 * (32 - 2*y - 2*x + xy) +
            a01 * (2*x - xy) +
            a10 * (2*y - xy) +
            a11 * xy;
}

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst)   Filter_32_opaque(x, y, a, b, c, d, dst)
#define MAKENAME(suffix)        SI8_opaque_D32 ## suffix
#define DSTSIZE                 32
#define SRCTYPE                 uint8_t
#define CHECKSTATE(state)       SkASSERT(state.fBitmap->config() == SkBitmap::kIndex8_Config); \
                                SkASSERT(state.fAlphaScale == 256)
#define PREAMBLE(state)         const SkPMColor* SK_RESTRICT table = state.fBitmap->getColorTable()->lockColors()
#define RETURNDST(src)          table[src]
#define SRC_TO_FILTER(src)      table[src]
#define POSTAMBLE(state)        state.fBitmap->getColorTable()->unlockColors(false)
#include "SkBitmapProcState_sample.h"

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst) \
    do {                                                        \
        uint32_t tmp = Filter_565_Expanded(x, y, a, b, c, d);   \
        *(dst) = SkCompact_rgb_16((tmp) >> 5);                  \
    } while (0)

#define MAKENAME(suffix)        SI8_D16 ## suffix
#define DSTSIZE                 16
#define SRCTYPE                 uint8_t
#define CHECKSTATE(state)       SkASSERT(state.fBitmap->config() == SkBitmap::kIndex8_Config); \
                                SkASSERT(state.fBitmap->isOpaque())
#define PREAMBLE(state)         const uint16_t* SK_RESTRICT table = state.fBitmap->getColorTable()->lock16BitCache()
#define RETURNDST(src)          table[src]
#define SRC_TO_FILTER(src)      table[src]
#define POSTAMBLE(state)        state.fBitmap->getColorTable()->unlock16BitCache()
#include "SkBitmapProcState_sample.h"

#define SIZE    50

/*REFERENT FUNCTIONS*/
void SI8_D16_nofilter_DX_referent(const SkBitmapProcState& s,
                                  const uint32_t* SK_RESTRICT xy,
                                  int count, uint16_t* SK_RESTRICT colors) {
    SkASSERT(count > 0 && colors != NULL);
    SkASSERT(s.fInvType <= (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask));
    SkASSERT(s.fDoFilter == false);

    const uint16_t* SK_RESTRICT table = s.fBitmap->getColorTable()->lock16BitCache();
    const uint8_t* SK_RESTRICT srcAddr = (const uint8_t*)s.fBitmap->getPixels();

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
        const uint16_t* SK_RESTRICT xx = (const uint16_t*)(xy + 1);
        for (i = count; i > 0; --i) {
          src = srcAddr[*xx++];
          *colors++ = table[src];
        }
    }
    s.fBitmap->getColorTable()->unlock16BitCache();
}

void SI8_opaque_D32_nofilter_DX_referent(const SkBitmapProcState& s,
                                    const uint32_t* SK_RESTRICT xy,
                                    int count, SkPMColor* SK_RESTRICT colors) {
    SkASSERT(count > 0 && colors != NULL);
    SkASSERT(s.fInvType <= (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask));
    SkASSERT(s.fDoFilter == false);

    const SkPMColor* SK_RESTRICT table = s.fBitmap->getColorTable()->lockColors();
    const uint8_t* SK_RESTRICT srcAddr = (const uint8_t*)s.fBitmap->getPixels();

    SkASSERT((unsigned)xy[0] < (unsigned)s.fBitmap->height());
    srcAddr = (const uint8_t*)((const char*)srcAddr + xy[0] * s.fBitmap->rowBytes());

    if (1 == s.fBitmap->width()) {
        uint8_t src = srcAddr[0];
        SkPMColor dstValue = table[src];
        sk_memset32(colors, dstValue, count);
    } else {
        int i;
        uint8_t src;
        const uint16_t* xx = (const uint16_t*)(xy + 1);
        for (i = count; i > 0; --i) {
            src = srcAddr[*xx++];
            *colors++ = table[src];
        }
    }
    s.fBitmap->getColorTable()->unlockColors(false);
}

/*TEST FUNCTIONS*/
static void test_SI8_D16_nofilter_DX(skiatest::Reporter* reporter) {
    SkBitmapProcState state;
    SkBitmap bm;
    SkRandom rand;
    int count = SIZE*2;
    uint32_t* xy = (uint32_t*)malloc(sizeof(uint32_t)*count);
    uint16_t* colors = (uint16_t*)malloc(sizeof(uint16_t)*count);
    uint16_t* colors_opt = (uint16_t*)malloc(sizeof(uint16_t)*count);
    int width = SIZE;
    int height = SIZE;

    for (int j = 0; j < count; j++) {
        xy[j] = height - static_cast<unsigned>(rand.nextBits(3));
        //colors[j] = static_cast<uint16_t>(rand.nextU16());
        //colors_opt[j] = colors[j];
      }

    state.fInvType = (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask);
    state.fDoFilter = false;
    SkColorTable* ctable = new SkColorTable(count);
    ctable->setFlags(SkColorTable::kColorsAreOpaque_Flag);
    bm.setConfig(SkBitmap::kIndex8_Config, width, height);
    bm.allocPixels(ctable);
    ctable->unref();

    state.fBitmap = &bm;
    SI8_D16_nofilter_DX_referent(state, xy, count, colors);

#if defined(__mips__) && defined(__mips_dsp)
    state.platformProcs();
    SkBitmapProcState::SampleProc16 proc = state.fSampleProc16;
    proc(state, xy, count, colors_opt);
#else
    SI8_D16_nofilter_DX(state, xy, count, colors_opt);
#endif

    if ( memcmp((void*)colors, (void*)colors_opt, count*sizeof(uint16_t))!=0 ) {
        SkDebugf("test_SI8_D16_nofilter_DX ERROR!\ncolors: 0x%x, colors_opt : 0x%x\n", *colors, *colors_opt);
    }
    free(xy);
    free(colors);
    free(colors_opt);
}

static void test_SI8_opaque_D32_nofilter_DX(skiatest::Reporter* reporter) {
    SkBitmapProcState state;
    SkBitmap bm;
    SkRandom rand;
    int count = SIZE*2;

    uint32_t* xy = (uint32_t*)malloc(sizeof(uint32_t)*count);
    SkPMColor* colors = (SkPMColor*)malloc(sizeof(SkPMColor)*count);
    SkPMColor* colors_opt = (SkPMColor*)malloc(sizeof(SkPMColor)*count);
    int width = SIZE;
    int height = SIZE;

    for (int j = 0; j < count; j++) {
        xy[j] = height - static_cast<unsigned>(rand.nextBits(3));
        colors[j] = static_cast<SkPMColor>(rand.nextU());
        colors_opt[j] = colors[j];
    }

    state.fInvType = (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask);
    state.fDoFilter = false;
    state.fAlphaScale = 256;

    SkColorTable* ctable = new SkColorTable(count);
    SkPMColor* c = (SkPMColor*)ctable->lockColors();
    ctable->setFlags(SkColorTable::kColorsAreOpaque_Flag);
    ctable->unlockColors(true);
    bm.setIsOpaque(true);
    bm.setConfig(SkBitmap::kIndex8_Config, width, height);
    bm.allocPixels(ctable);
    ctable->unref();
    state.fBitmap = &bm;

    SI8_opaque_D32_nofilter_DX_referent(state, xy, count, colors);

#if defined(__mips__) && defined(__mips_dsp)
    state.platformProcs();
    SkBitmapProcState::SampleProc32 proc = state.fSampleProc32;
    proc(state, xy, count, colors_opt);
#else
    SI8_opaque_D32_nofilter_DX(state, xy, count, colors_opt);
#endif

    if ( memcmp((void*)colors, (void*)colors_opt, count)!=0 )
        SkDebugf("test_SI8_D16_nofilter_DX ERROR!\ncolors: 0x%X, colors_opt = 0x%X\n", *colors, *colors_opt);
    free(xy);
    free(colors);
    free(colors_opt);
}

//RUN SOME TESTS:
static void TestBitmapProcState(skiatest::Reporter* reporter) {
    test_SI8_D16_nofilter_DX(reporter);
    test_SI8_opaque_D32_nofilter_DX(reporter);
}

#include "TestClassDef.h"
DEFINE_TESTCLASS("BitmapProcState", TestBitmapProcStateClass, TestBitmapProcState)
