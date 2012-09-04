#include "Test.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkGradientShader.h"
#include "SkRect.h"
#include "SkRandom.h"
#include "SkBitmapProcState.h"
#include "SkPerspIter.h"
#include "SkShader.h"
#include "SkUtils.h"
#include "SkRandom.h"

#if defined(__mips__) && defined(__mips_dsp)

extern void decal_nofilter_scale(uint32_t dst[], SkFixed fx, SkFixed dx, int count);
extern void decal_filter_scale(uint32_t dst[], SkFixed fx, SkFixed dx, int count);

extern void ClampX_ClampY_nofilter_persp_mips(const SkBitmapProcState&, uint32_t*, int, int, int);
extern void ClampX_ClampY_filter_persp_mips(const SkBitmapProcState&, uint32_t*, int, int, int);
extern void ClampX_ClampY_nofilter_affine_mips(const SkBitmapProcState&, uint32_t*, int, int, int);
extern void ClampX_ClampY_filter_affine_mips(const SkBitmapProcState&, uint32_t*, int, int, int);
extern void ClampX_ClampY_filter_scale_mips(const SkBitmapProcState&, uint32_t*, int, int, int);
extern void ClampX_ClampY_nofilter_scale_mips(const SkBitmapProcState&, uint32_t*, int, int, int);

extern void RepeatX_RepeatY_nofilter_persp_mips(const SkBitmapProcState&, uint32_t*, int, int, int);
extern void RepeatX_RepeatY_filter_persp_mips(const SkBitmapProcState&, uint32_t*, int, int, int);
extern void RepeatX_RepeatY_filter_affine_mips(const SkBitmapProcState&, uint32_t*, int, int, int);
extern void RepeatX_RepeatY_nofilter_affine_mips(const SkBitmapProcState&, uint32_t*, int, int, int);
extern void RepeatX_RepeatY_filter_scale_mips(const SkBitmapProcState&, uint32_t*, int, int, int);
extern void RepeatX_RepeatY_nofilter_scale_mips(const SkBitmapProcState&, uint32_t*, int, int, int);

static unsigned SK_USHIFT16(unsigned x) {
    return x >> 16;
}
#define MAKENAME(suffix)        ClampX_ClampY ## suffix
#define TILEX_PROCF(fx, max)    SkClampMax((fx) >> 16, max)
#define TILEY_PROCF(fy, max)    SkClampMax((fy) >> 16, max)
#define TILEX_LOW_BITS(fx, max) (((fx) >> 12) & 0xF)
#define TILEY_LOW_BITS(fy, max) (((fy) >> 12) & 0xF)
#include "SkBitmapProcState_matrix.h"

#define MAKENAME(suffix)        RepeatX_RepeatY ## suffix
#define TILEX_PROCF(fx, max)    SK_USHIFT16(((fx) & 0xFFFF) * ((max) + 1))
#define TILEY_PROCF(fy, max)    SK_USHIFT16(((fy) & 0xFFFF) * ((max) + 1))
#define TILEX_LOW_BITS(fx, max) ((((fx) & 0xFFFF) * ((max) + 1) >> 12) & 0xF)
#define TILEY_LOW_BITS(fy, max) ((((fy) & 0xFFFF) * ((max) + 1) >> 12) & 0xF)
#include "SkBitmapProcState_matrix.h"

#define WIDTH 320
#define HEIGHT 240
#define BUF_SIZE WIDTH*HEIGHT

static void test_perspective_nofilter(skiatest::Reporter* reporter) {
    int width = WIDTH;
    int height = WIDTH;
    SkBitmapProcState state;
    SkBitmap src;
    SkRandom rand;
    SkMatrix m;
    SkPMColor* col = (SkPMColor*)malloc(sizeof(SkPMColor)*BUF_SIZE);
    uint32_t* dst_ref = (uint32_t*)malloc(sizeof(uint32_t)*BUF_SIZE);
    uint32_t* dst_opt = (uint32_t*)malloc(sizeof(uint32_t)*BUF_SIZE);

    for (int j = 0; j < BUF_SIZE; j++) {
        col[j] = static_cast<SkPMColor>(rand.nextU());
    }
    src.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    src.allocPixels();
    src.setPixels((void*)col, NULL);
    m.reset();
    SkScalar sinV_r, cosV_r;
    SkScalar deg_r = 30;
    sinV_r = SkScalarSinCos(SkDegreesToRadians(deg_r), &cosV_r);
    SkPoint src_r[4], dst_r[4];

    src_r[0].fX = 0; src_r[0].fY = 0;
    src_r[1].fX = width; src_r[1].fY = 0;
    src_r[2].fX = width; src_r[2].fY = height;
    src_r[3].fX = 0; src_r[3].fY = height;

    dst_r[0].fX = 0; dst_r[0].fY = 0;
    dst_r[1].fX = width*cosV_r; dst_r[1].fY = (sinV_r/5)*height/2;
    dst_r[2].fX = width*cosV_r; dst_r[2].fY = height-sinV_r/5*height/2;
    dst_r[3].fX = 0; dst_r[3].fY = height;
    m.setPolyToPoly(src_r, dst_r, 4);

    state.fInvType = SkMatrix::kPerspective_Mask;
    state.fDoFilter = false;
    state.fBitmap = &src;
    state.fInvMatrix = &m;
    state.fInvProc = m.GetMapXYProc(SkMatrix::kPerspective_Mask);

    ClampX_ClampY_nofilter_persp(state, dst_ref, width, 0, 0);
    ClampX_ClampY_nofilter_persp_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_opt, dst_ref, sizeof(uint32_t)*width)) {
        SkString str;
        str.printf("Perspective_nofilter CLAMP version FAILED!\n");
        reporter->reportFailed(str);
    }

    RepeatX_RepeatY_nofilter_persp(state, dst_ref, width, 0, 0);
    RepeatX_RepeatY_nofilter_persp_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_opt, dst_ref, sizeof(uint32_t)*width)) {
        SkString str;
        str.printf("Perspective_nofilter REPEAT version FAILED!\n");
        reporter->reportFailed(str);
    }
    free(col);
    free(dst_ref);
    free(dst_opt);
}

static void test_rotation_affine_nofilter(skiatest::Reporter* reporter) {
    int width = WIDTH;
    int height = HEIGHT;
    SkBitmapProcState state;
    SkBitmap src;
    SkRandom rand;
    SkMatrix m;
    SkPMColor* col = (SkPMColor*)malloc(sizeof(SkPMColor)*BUF_SIZE);
    uint32_t* dst_ref = (uint32_t*)malloc(sizeof(uint32_t)*BUF_SIZE);
    uint32_t* dst_opt = (uint32_t*)malloc(sizeof(uint32_t)*BUF_SIZE);
    for (int j = 0; j < BUF_SIZE; j++) {
        col[j] = static_cast<SkPMColor>(rand.nextU());
    }
    m.reset();
    m.setRotate(20, 0, 0);
    src.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    src.allocPixels();
    src.setPixels((void*)col, NULL);
    state.fInvType = SkMatrix::kAffine_Mask;
    state.fDoFilter = false;
    state.fBitmap = &src;
    state.fInvMatrix = &m;
    state.fInvProc = m.GetMapXYProc(SkMatrix::kAffine_Mask);

    ClampX_ClampY_nofilter_affine(state, dst_ref, width, 0, 0);
    ClampX_ClampY_nofilter_affine_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_opt, dst_ref, sizeof(uint32_t)*width)) {
        SkString str;
        str.printf("Rotate affine nofilter CLAMP FAILED!\n");
        reporter->reportFailed(str);
    }

    RepeatX_RepeatY_nofilter_affine(state, dst_ref, width, 0, 0);
    RepeatX_RepeatY_nofilter_affine_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_opt, dst_ref, sizeof(uint32_t)*width)) {
        SkString str;
        str.printf("Rotate affine nofilter REPEAT FAILED!\n");
        reporter->reportFailed(str);
    }
    free(col);
    free(dst_ref);
    free(dst_opt);
}

static void test_rotation_affine_filter(skiatest::Reporter* reporter) {
    int width = WIDTH;
    int height = HEIGHT;
    SkBitmapProcState state;
    SkBitmap src;
    SkRandom rand;
    SkMatrix m;
    SkPMColor* col = (SkPMColor*)malloc(sizeof(SkPMColor)*BUF_SIZE);
    uint32_t* dst_ref = (uint32_t*)malloc(sizeof(uint64_t)*BUF_SIZE);
    uint32_t* dst_opt = (uint32_t*)malloc(sizeof(uint64_t)*BUF_SIZE);
    for (int j = 0; j < BUF_SIZE; j++) {
        col[j] = static_cast<SkPMColor>(rand.nextU());
    }
    m.reset();
    m.setRotate(20, 0, 0);
    src.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    src.allocPixels();
    src.setPixels((void*)col, NULL);
    state.fInvType = SkMatrix::kAffine_Mask;
    state.fDoFilter = true;
    state.fBitmap = &src;
    state.fInvMatrix = &m;
    state.fFilterOneX = SK_Fixed1;
    state.fFilterOneY = SK_Fixed1;
    state.fInvProc = m.GetMapXYProc(SkMatrix::kAffine_Mask);

    ClampX_ClampY_filter_affine(state, dst_ref, width, 0, 0);
    ClampX_ClampY_filter_affine_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_opt, dst_ref, sizeof(uint64_t)*width)) {
        SkString str;
        str.printf("Rotate affine filter CLAMP FAILED!\n");
        reporter->reportFailed(str);
    }

    state.fFilterOneX = SK_Fixed1 / width;
    state.fFilterOneY = SK_Fixed1 / width;

    RepeatX_RepeatY_filter_affine(state, dst_ref, width, 0, 0);
    RepeatX_RepeatY_filter_affine_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_opt, dst_ref, sizeof(uint64_t)*width)) {
        SkString str;
        str.printf("Rotate affine filter REPEAT FAILED!\n");
        reporter->reportFailed(str);
    }
    free(col);
    free(dst_ref);
    free(dst_opt);
}

static void test_perspective_filter(skiatest::Reporter* reporter) {
    int width = WIDTH;
    int height = HEIGHT;
    SkBitmapProcState state;
    SkBitmap src;
    SkRandom rand;
    SkMatrix m;
    SkPMColor* col = (SkPMColor*)malloc(sizeof(SkPMColor)*BUF_SIZE);
    uint32_t* dst_ref = (uint32_t*)malloc(sizeof(uint64_t)*BUF_SIZE);
    uint32_t* dst_opt = (uint32_t*)malloc(sizeof(uint64_t)*BUF_SIZE);

    for (int j = 0; j < BUF_SIZE; j++) {
        col[j] = static_cast<SkPMColor>(rand.nextU());
    }
    src.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    src.allocPixels();
    src.setPixels((void*)col, NULL);
    m.reset();
    SkScalar sinV_r, cosV_r;
    SkScalar deg_r = 30;
    sinV_r = SkScalarSinCos(SkDegreesToRadians(deg_r), &cosV_r);
    SkPoint src_r[4], dst_r[4];

    src_r[0].fX = 0; src_r[0].fY = 0;
    src_r[1].fX = width; src_r[1].fY = 0;
    src_r[2].fX = width; src_r[2].fY = height;
    src_r[3].fX = 0; src_r[3].fY = height;

    dst_r[0].fX = 0; dst_r[0].fY = 0;
    dst_r[1].fX = width*cosV_r; dst_r[1].fY = (sinV_r/5)*height/2;
    dst_r[2].fX = width*cosV_r; dst_r[2].fY = height-sinV_r/5*height/2;
    dst_r[3].fX = 0; dst_r[3].fY = height;
    m.setPolyToPoly(src_r, dst_r, 4);

    state.fInvType = SkMatrix::kPerspective_Mask;
    state.fDoFilter = true;
    state.fBitmap = &src;
    state.fInvMatrix = &m;
    state.fFilterOneX = SK_Fixed1;
    state.fFilterOneY = SK_Fixed1;
    state.fInvProc = m.GetMapXYProc(SkMatrix::kPerspective_Mask);

    ClampX_ClampY_filter_persp(state, dst_ref, width, 0, 0);
    ClampX_ClampY_filter_persp_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_opt, dst_ref, sizeof(uint64_t)*width)) {
        SkString str;
        str.printf("Perspective_filter CLAMP version FAILED!\n");
        reporter->reportFailed(str);
    }

    state.fFilterOneX = SK_Fixed1 / width;
    state.fFilterOneY = SK_Fixed1 / width;
    RepeatX_RepeatY_filter_persp(state, dst_ref, width, 0, 0);
    RepeatX_RepeatY_filter_persp_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_opt, dst_ref, sizeof(uint64_t)*width)) {
        SkString str;
        str.printf("Perspective_filter REPEAT version FAILED!\n");
        reporter->reportFailed(str);
    }
    free(col);
    free(dst_ref);
    free(dst_opt);
}

static void test_scale_filter(skiatest::Reporter* reporter) {
    int width = WIDTH;
    int height = HEIGHT;
    SkBitmapProcState state;
    SkBitmap src;
    SkRandom rand;
    SkMatrix m;
    SkPMColor* col = (SkPMColor*)malloc(sizeof(SkPMColor)*BUF_SIZE);
    uint32_t* dst_ref = (uint32_t*)malloc(sizeof(uint32_t)*BUF_SIZE);
    uint32_t* dst_opt = (uint32_t*)malloc(sizeof(uint32_t)*BUF_SIZE);

    for (int j = 0; j < BUF_SIZE; j++) {
        col[j] = static_cast<SkPMColor>(rand.nextU());
    }
    m.reset();
    m.setScale(2,2);
    src.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    src.allocPixels();
    src.setPixels((void*)col, NULL);
    state.fInvType = SkMatrix::kScale_Mask;
    state.fDoFilter = true;
    state.fBitmap = &src;
    state.fInvMatrix = &m;
    state.fInvKy = 0;
    state.fInvProc = m.GetMapXYProc(SkMatrix::kScale_Mask);

    ClampX_ClampY_filter_scale(state, dst_ref, width, 0, 0);
    ClampX_ClampY_filter_scale_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_ref, dst_opt, sizeof(uint32_t)*width)) {
        SkString str;
        str.printf("Scale filter CLAMP FAILED!\n");
        reporter->reportFailed(str);
    }

    RepeatX_RepeatY_filter_scale(state, dst_ref, width, 0, 0);
    RepeatX_RepeatY_filter_scale_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_ref, dst_opt, sizeof(uint32_t)*width)) {
        SkString str;
        str.printf("Scale filter REPEAT FAILED!\n");
        reporter->reportFailed(str);
    }
    free(col);
    free(dst_ref);
    free(dst_opt);
}

static void test_scale_nofilter(skiatest::Reporter* reporter) {
    int width = WIDTH;
    int height = HEIGHT;
    SkBitmapProcState state;
    SkBitmap src;
    SkRandom rand;
    SkMatrix m;
    SkPMColor* col = (SkPMColor*)malloc(sizeof(SkPMColor)*BUF_SIZE);
    uint32_t* dst_ref = (uint32_t*)malloc(sizeof(uint16_t)*BUF_SIZE);
    uint32_t* dst_opt = (uint32_t*)malloc(sizeof(uint16_t)*BUF_SIZE);

    for (int j = 0; j < BUF_SIZE; j++) {
        col[j] = static_cast<SkPMColor>(rand.nextU());
    }
    m.reset();
    m.setScale(2,2);
    src.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    src.allocPixels();
    src.setPixels((void*)col, NULL);
    state.fInvType = SkMatrix::kScale_Mask;
    state.fDoFilter = false;
    state.fBitmap = &src;
    state.fInvMatrix = &m;
    state.fInvKy = 0;
    state.fInvProc = m.GetMapXYProc(SkMatrix::kScale_Mask);

    ClampX_ClampY_nofilter_scale(state, dst_ref, width, 0, 0);
    ClampX_ClampY_nofilter_scale_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_ref, dst_opt, sizeof(uint16_t)*width)) {
        SkString str;
        str.printf("Scale nofilter CLAMP FAILED!\n");
        reporter->reportFailed(str);
    }

    RepeatX_RepeatY_nofilter_scale(state, dst_ref, width, 0, 0);
    RepeatX_RepeatY_nofilter_scale_mips(state, dst_opt, width, 0, 0);

    if (memcmp(dst_ref, dst_opt, sizeof(uint16_t)*width)) {
        SkString str;
        str.printf("Scale nofilter REPEAT FAILED!\n");
        reporter->reportFailed(str);
    }
    free(col);
    free(dst_ref);
    free(dst_opt);
}
static void TestMatrixProcs(skiatest::Reporter* reporter) {
   test_perspective_nofilter(reporter);
   test_perspective_filter(reporter);
   test_rotation_affine_nofilter(reporter);
   test_rotation_affine_filter(reporter);
   test_scale_nofilter(reporter);
   test_scale_filter(reporter);
}

#include "TestClassDef.h"
DEFINE_TESTCLASS("MatrixProcs", TestMatrixProcsClass, TestMatrixProcs)
#endif  // defined(__mips__) && defined(__mips_dsp)
