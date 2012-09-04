#include "SkBenchmark.h"
#include "SkBitmap.h"
#include "SkBitmapProcState.h"
#include "SkString.h"
#include "SkCanvas.h"
#include "SkBlitter.h"
#include "SkRandom.h"
#include "SkMatrix.h"
#include "SkPaint.h"
#include "SkColorPriv.h"
#include "SkUtils.h"
#include "SkShader.h"

static void drawIntoBitmap(const SkBitmap& bm) {
    const int w = bm.width();
    const int h = bm.height();

    SkCanvas canvas(bm);
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor(SK_ColorRED);
    canvas.drawCircle(SkIntToScalar(w)/2, SkIntToScalar(h)/2,
                      SkIntToScalar(SkMin32(w, h))*3/8, p);

    SkRect r;
    r.set(0, 0, SkIntToScalar(w), SkIntToScalar(h));
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(SkIntToScalar(4));
    p.setColor(SK_ColorBLUE);
    canvas.drawRect(r, p);
}

static int conv6ToByte(int x) {
    return x * 0xFF / 5;
}

static int convByteTo6(int x) {
    return x * 5 / 255;
}

static uint8_t compute666Index(SkPMColor c) {
    int r = SkGetPackedR32(c);
    int g = SkGetPackedG32(c);
    int b = SkGetPackedB32(c);
    return convByteTo6(r) * 36 + convByteTo6(g) * 6 + convByteTo6(b);
}

static void convertToIndexARGB(const SkBitmap& src, SkBitmap* dst) {
    SkColorTable* ctable = new SkColorTable(216);
    SkPMColor* colors = ctable->lockColors();
    // rrr ggg bbb
    for (int r = 0; r < 6; r++) {
        int rr = conv6ToByte(r);
        for (int g = 0; g < 6; g++) {
            int gg = conv6ToByte(g);
            for (int b = 0; b < 6; b++) {
                int bb = conv6ToByte(b);
                *colors++ = SkPreMultiplyARGB(0xFF, rr, gg, bb);
            }
        }
    }
    ctable->unlockColors(true);
    dst->setConfig(SkBitmap::kIndex8_Config, src.width(), src.height());
    dst->allocPixels(ctable);
    ctable->unref();
    SkAutoLockPixels alps(src);
    SkAutoLockPixels alpd(*dst);

    for (int y = 0; y < src.height(); y++) {
        const SkPMColor* srcP = src.getAddr32(0, y);
        uint8_t* dstP = dst->getAddr8(0, y);
        for (int x = src.width() - 1; x >= 0; --x) {
            *dstP++ = compute666Index(*srcP++);
        }
    }
}
static void convertToIndex565(const SkBitmap& src, SkBitmap* dst) {
    SkColorTable* ctable = new SkColorTable(216);
    uint16_t* colors = (uint16_t*)ctable->lockColors();
    uint8_t rr, gg, bb;
    SkRandom rand;
    for(int i=0; i<216; i++){
        rr = static_cast<uint8_t>(rand.nextBits(5));
        gg = static_cast<uint8_t>(rand.nextBits(6));
        bb = static_cast<uint8_t>(rand.nextBits(5));
        *colors++ = SkPackRGB16(rr, gg, bb);
    }
    ctable->setFlags(SkColorTable::kColorsAreOpaque_Flag);
    ctable->unlockColors(true);
    dst->setConfig(SkBitmap::kIndex8_Config, src.width(), src.height());
    dst->allocPixels(ctable);
    ctable->unref();
    SkAutoLockPixels alps(src);
    SkAutoLockPixels alpd(*dst);

    for (int y = 0; y < src.height(); y++) {
        const uint16_t* srcP = src.getAddr16(0, y);
        uint8_t* dstP = dst->getAddr8(0, y);
        for (int x = src.width() - 1; x >= 0; --x) {
            *dstP++ = compute666Index(*srcP++);
        }
    }
}
class BitmapProcStateBench : public SkBenchmark {
    SkBitmap          fBitmap;
    bool              fIsOpaque;
    SkPaint           fPaint;
    SkString          fName;
    enum { N = 300 };
public:
    BitmapProcStateBench(void* param, SkBitmap::Config c, bool isOpaque)
        : INHERITED(param), fIsOpaque(isOpaque) {
        const int w = 50;
        const int h = 50;
        SkBitmap bm;
        SkBitmap tmp;

        if (isOpaque) {
            bm.setConfig(SkBitmap::kARGB_8888_Config, w, h);
        } else {
            bm.setConfig(SkBitmap::kRGB_565_Config, w, h);
        }
        bm.allocPixels();
        bm.eraseColor(0);

        drawIntoBitmap(bm);

        if (isOpaque) {
            fBitmap.setIsOpaque(isOpaque);
            convertToIndexARGB(bm, &tmp);
            fBitmap = tmp;
            fName.set("SI8_opaque_D32_nofilter_DX");
        } else {
            convertToIndex565(bm, &tmp);
            bm = tmp;
            SkShader* s = SkShader::CreateBitmapShader(bm,
                                                       SkShader::kRepeat_TileMode,
                                                       SkShader::kRepeat_TileMode);
            fPaint.setShader(s)->unref();
            fName.set("SI8_D16_nofilter_DX");
        }
    }

protected:
    virtual const char* onGetName() {
        return fName.c_str();
    }

    virtual void onDraw(SkCanvas* canvas) {
        SkPaint paint(fPaint);
        this->setupPaint(&paint);

        if (fIsOpaque) {
          for (int i = 0; i < N; i++) {
              canvas->drawBitmap(fBitmap, 0, 0, &paint);
          }
        } else {
            SkPaint paint(fPaint);
            this->setupPaint(&paint);
            for (int i = 0; i < N; i++) {
                canvas->drawPaint(paint);
            }
        }
    }
private:
    typedef SkBenchmark INHERITED;
};

class BlitterRGB16Bench : public SkBenchmark {
    SkPaint     fPaint;
    int         fCount;
    SkPoint*    fPos;
    SkString    fText;
    SkString    fName;
    enum { N = 300 };
public:
    BlitterRGB16Bench(void* param)
        : INHERITED(param) {
        fText.set("blitMask");
        fPaint.setAntiAlias(true);
        fPaint.setTextSize(SkIntToScalar(8));
        fPaint.setLinearText(false);
        fCount = 0;
        fPos = NULL;
        fName.set("blitMask");
    }

protected:
    virtual const char* onGetName() {
        return fName.c_str();
    }

    virtual void onDraw(SkCanvas* canvas) {
        const SkIPoint dim = this->getSize();
        SkRandom rand;

        SkPaint paint(fPaint);
        this->setupPaint(&paint);

        const SkScalar x0 = SkIntToScalar(-10);
        const SkScalar y0 = SkIntToScalar(-10);
        const SkColor colors[] = { SK_ColorBLACK, SK_ColorGRAY };

        for (size_t j = 0; j < SK_ARRAY_COUNT(colors); j++) {
            paint.setColor(colors[j]);
            for (int i = 0; i < N; i++) {
                SkScalar x = x0 + rand.nextUScalar1() * dim.fX;
                SkScalar y = y0 + rand.nextUScalar1() * dim.fY;
                if (fPos) {
                    canvas->save(SkCanvas::kMatrix_SaveFlag);
                    canvas->translate(x, y);
                    canvas->drawPosText(fText.c_str(), fText.size(), fPos, paint);
                    canvas->restore();
                } else {
                    canvas->drawText(fText.c_str(), fText.size(), x, y, paint);
                }
            }
        }
    }
private:
    typedef SkBenchmark INHERITED;
};


/*
    BitmapProcState bench:
      - SI8_D16_nofilter_DX
      - SI8_opaque_D32_nofilter_DX
*/
static SkBenchmark* Fact0(void* p) { return new BitmapProcStateBench(p, SkBitmap::kIndex8_Config, false); }
static SkBenchmark* Fact1(void* p) { return new BitmapProcStateBench(p, SkBitmap::kIndex8_Config, true); }

static BenchRegistry pReg0(Fact0);
static BenchRegistry pReg1(Fact1);

/*
    Blitter_RGB16 bench:
      - SkRGB16_Opaque_Blitter::blitMask
*/
static SkBenchmark* bFact0(void* p) { return new BlitterRGB16Bench(p); }

static BenchRegistry bReg0(bFact0);
