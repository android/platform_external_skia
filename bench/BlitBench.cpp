#include "SkBenchmark.h"
#include "SkBitmap.h"
#include "SkPaint.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkRandom.h"
#include "SkString.h"
#include "SkShader.h"

class BlitBench : public SkBenchmark {
    SkBitmap    fBitmap;
    SkPaint     fPaint;
    bool        fIsOpaque;
    unsigned    fAlpha;
    bool        fIsDither;
    SkString    fName;
    enum { N = 50 };
public:
    BlitBench(void* param, SkBitmap::Config c, bool isOpaque,
                unsigned alpha, bool isDither)
        : INHERITED(param), fIsOpaque(isOpaque), fAlpha(alpha), fIsDither(isDither) {
        const int w = 128;
        const int h = 128;
        SkBitmap bm;

        bm.setConfig(c, w, h);
        bm.allocPixels();
        bm.eraseColor(0xffeeaa77);

        fBitmap = bm;
        fBitmap.setIsOpaque(isOpaque);
        fPaint.setAlpha(fAlpha);
        fPaint.setDither(fIsDither);

        if ( (fIsOpaque == true) && (fAlpha < 255) && (fIsDither == true) )
            fName.set("S32_D565_Blend_Dither");
        if ( (fIsOpaque == false) && (fAlpha < 255) && (fIsDither == false) )
            fName.set("S32A_D565_Blend");
        if ( (fIsOpaque == false) && (fAlpha == 255) && (fIsDither == true) )
            fName.set("S32A_D565_Opaque_Dither");
        if ( (fIsOpaque == true) && (fAlpha == 255) && (fIsDither == true) )
            fName.set("S32_D565_Opaque_Dither");
        if ( (fIsOpaque == false) && (fAlpha == 255) && (fIsDither == false) )
            fName.set("S32A_D565_Opaque");
    }

protected:
    virtual const char* onGetName() {
        return fName.c_str();
    }

    virtual void onDraw(SkCanvas* canvas) {
        for (int i = 0; i < N; i++) {
            canvas->drawBitmap(fBitmap, 0, 0, &fPaint);
        }
    }

private:
    typedef SkBenchmark INHERITED;
};

int32_t col[1024];

class MatrixScaleRotatePerspBench : public SkBenchmark {
    SkBitmap    fBitmap;
    SkPaint     fPaint;
    bool        fIsOpaque;
    SkShader::TileMode fTileMode;
    bool        fIsScale;
    bool        fIsRotate;
    bool        fIsPersp;
    bool        fIsFilter;
    SkString    fName;
    enum { N = 50 };
public:
    MatrixScaleRotatePerspBench(void* param, bool isOpaque, SkBitmap::Config c,
                SkShader::TileMode tileMode, bool isScale, bool isRotate = false,
                bool isPersp = false, bool isFilter = false)
        : INHERITED(param), fIsOpaque(isOpaque), fTileMode(tileMode), fIsScale(isScale), fIsRotate(isRotate),
          fIsPersp(isPersp), fIsFilter(isFilter) {
        const int w = 32;
        const int h = 32;
        SkBitmap bm;
        SkRandom rand;
        uint8_t r, g, b;
        for (int j = 0; j < w*h ; j++) {
            r = rand.nextU();
            g = rand.nextU();
            b = rand.nextU();
            col[j] = SkColorSetRGB(r, g, b);
        }
        bm.setConfig(c, w, h);
        bm.allocPixels();
        bm.setPixels((void*)col, NULL);
        fBitmap = bm;
        fBitmap.setIsOpaque(isOpaque);
        const char* type_matrix [] = {"scale", "rotate", "persp"};
        const char* tile_mode[] = {"clamp", "repeat"};
        int i, j;
        if (fIsScale) i = 0;
        if (fIsRotate) i = 1;
        if (fIsPersp) i = 2;
        j = 0;
        if (tileMode == SkShader::kRepeat_TileMode) j = 1;
        fName.printf("matrix_%s_%s_%s", type_matrix[i], tile_mode[j], fIsFilter ? "filter" : "nofilter");
    }

protected:
    virtual const char* onGetName() {
        return fName.c_str();
    }

    virtual void onDraw(SkCanvas* canvas) {
        SkIPoint dim = this->getSize();

        SkPaint paint(fPaint);
        const SkScalar w = SkIntToScalar(fBitmap.width());
        const SkScalar h = SkIntToScalar(fBitmap.height());

        SkPoint fPts[4];
        fPts[0].set(0, 0);
        fPts[1].set(w, 0);
        fPts[2].set(w, h);
        fPts[3].set(0, h);
        SkShader* s;
        if (fTileMode == SkShader::kClamp_TileMode) {
            s = SkShader::CreateBitmapShader(fBitmap, SkShader::kClamp_TileMode, SkShader::kClamp_TileMode);
        } else {
            s = SkShader::CreateBitmapShader(fBitmap, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);
        }
        if (fIsFilter) {
            paint.setFilterBitmap(true);
        }
        else {
            paint.setFilterBitmap(false);
        }
        paint.setShader(s)->unref();

        if (fIsScale) canvas->scale(2,2);
        if (fIsRotate) canvas->rotate(30);

        if (fIsPersp) {
            SkMatrix mat;
            mat.reset();
            SkScalar sinV, cosV;
            SkScalar deg = 30;
            sinV = SkScalarSinCos(SkDegreesToRadians(deg), &cosV);
            SkPoint src[4], dst[4];

            src[0].fX = 0; src[0].fY = 0;
            src[1].fX = w; src[1].fY = 0;
            src[2].fX = w; src[2].fY = h;
            src[3].fX = 0; src[3].fY = h;

            dst[0].fX = 0; dst[0].fY = 0;
            dst[1].fX = w*cosV; dst[1].fY = (sinV/5)*h/2;
            dst[2].fX = w*cosV; dst[2].fY = h-sinV/5*h/2;
            dst[3].fX = 0; dst[3].fY = h;
            mat.setPolyToPoly(src, dst, 4);
            canvas->resetMatrix();
            canvas->concat(mat);
        }
        for (int i = 0; i < N; i++) {
            canvas->drawVertices(SkCanvas::kTriangleFan_VertexMode, 4, fPts, fPts,
                                  NULL, NULL, NULL, 0, paint);
        }
    }
private:
    typedef SkBenchmark INHERITED;
};

class BitmapFilter32Bench : public SkBenchmark {
    SkBitmap    fBitmap;
    SkPaint     fPaint;
    bool        fIsOpaque;
    SkString    fName;
    bool        fAlpha;
    enum { N = 50 };
public:
    BitmapFilter32Bench(void* param, bool isOpaque, SkBitmap::Config c,
                bool a)
        : INHERITED(param), fIsOpaque(isOpaque), fAlpha(a) {
        const int w = 640;
        const int h = 480;
        SkBitmap bm;

        bm.setConfig(c, w, h);
        bm.allocPixels();
        bm.eraseColor(isOpaque ? SK_ColorBLACK : 0);
        fBitmap = bm;
        fBitmap.setIsOpaque(isOpaque);
        fName.printf("filter_32_%s", fAlpha ? "opaque" : "alpha");
    }

protected:
    virtual const char* onGetName() {
        return fName.c_str();
    }

    virtual void onDraw(SkCanvas* canvas) {
        SkIPoint dim = this->getSize();
        SkRandom rand;

        SkPaint paint(fPaint);
        paint.setDither(true);

        const SkBitmap& bitmap = fBitmap;
        const SkScalar x0 = SkIntToScalar(-bitmap.width() / 2);
        const SkScalar y0 = SkIntToScalar(-bitmap.height() / 2);

        for (int i = 0; i < N; i++) {
            SkScalar x = x0 + rand.nextUScalar1() * dim.fX;
            SkScalar y = y0 + rand.nextUScalar1() * dim.fY;
            canvas->scale(SkIntToScalar(2), SkIntToScalar(2));
            paint.setFilterBitmap(true);
            if (!fAlpha) paint.setAlpha(151);
            canvas->drawBitmap(bitmap, x, y, &paint);
        }
    }
private:
    typedef SkBenchmark INHERITED;
};

/*
    BlitRow bench:

      - S32_D565_Blend_Dither
      - S32A_D565_Blend
      - S32A_D565_Opaque_Dither
      - S32_D565_Opaque_Dither
      - S32A_D565_Opaque
 */
static SkBenchmark* Fact0(void* p) { return new BlitBench(p, SkBitmap::kARGB_8888_Config, true, 155, true ); }
static SkBenchmark* Fact1(void* p) { return new BlitBench(p, SkBitmap::kARGB_8888_Config, false, 155, false ); }
static SkBenchmark* Fact2(void* p) { return new BlitBench(p, SkBitmap::kARGB_8888_Config, false, 255, true ); }
static SkBenchmark* Fact3(void* p) { return new BlitBench(p, SkBitmap::kARGB_8888_Config, true, 255, false ); }
static SkBenchmark* Fact4(void* p) { return new BlitBench(p, SkBitmap::kARGB_8888_Config, true, 255, true ); }
static SkBenchmark* Fact5(void* p) { return new BlitBench(p, SkBitmap::kARGB_8888_Config, false, 255, false ); }

static BenchRegistry gReg0(Fact0);
static BenchRegistry gReg1(Fact1);
static BenchRegistry gReg2(Fact2);
static BenchRegistry gReg3(Fact3);
static BenchRegistry gReg4(Fact4);
static BenchRegistry gReg5(Fact5);

/*
   Matrix SCALE and ROTATE bench:

      - SCALE_NOFILTER
      - ROTATE_NOFILTER
      - SCALE_FILTER
      - ROTATE_FILTER
      - PERSP_NOFILTER
      - PERSP_FILTER
 */
static SkBenchmark* mFact0(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kClamp_TileMode, true, false, false, false); }
static SkBenchmark* mFact1(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kClamp_TileMode, false, true, false, false); }
static SkBenchmark* mFact2(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kClamp_TileMode, true, false, false, true); }
static SkBenchmark* mFact3(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kClamp_TileMode, false, true, false, true); }
static SkBenchmark* mFact4(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kRepeat_TileMode, true, false, false, false); }
static SkBenchmark* mFact5(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kRepeat_TileMode, false, true, false, false); }
static SkBenchmark* mFact6(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kRepeat_TileMode, true, false, false, true); }
static SkBenchmark* mFact7(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kRepeat_TileMode, false, true, false, true); }
static SkBenchmark* mFact8(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kClamp_TileMode, false, false, true, false); }
static SkBenchmark* mFact9(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kClamp_TileMode, false, false, true, true); }
static SkBenchmark* mFact10(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kRepeat_TileMode, false, false, true, false); }
static SkBenchmark* mFact11(void* p) { return new MatrixScaleRotatePerspBench(p, false, SkBitmap::kARGB_8888_Config, SkShader::kRepeat_TileMode, false, false, true, true); }

static BenchRegistry mReg0(mFact0);
static BenchRegistry mReg1(mFact1);
static BenchRegistry mReg2(mFact2);
static BenchRegistry mReg3(mFact3);
static BenchRegistry mReg4(mFact4);
static BenchRegistry mReg5(mFact5);
static BenchRegistry mReg6(mFact6);
static BenchRegistry mReg7(mFact7);
static BenchRegistry mReg8(mFact8);
static BenchRegistry mReg9(mFact9);
static BenchRegistry mReg10(mFact10);
static BenchRegistry mReg11(mFact11);

/*
   Filter_32 bench:

      - Filter_32_alpha routine
      - Filter_32_opaque routine
 */
static SkBenchmark* fFact0(void* p) { return new BitmapFilter32Bench(p, false, SkBitmap::kARGB_8888_Config, false); }
static SkBenchmark* fFact1(void* p) { return new BitmapFilter32Bench(p, false, SkBitmap::kARGB_8888_Config, true); }

static BenchRegistry fReg0(fFact0);
static BenchRegistry fReg1(fFact1);
