#include "Test.h"
#include "SkBlitRow.h"
#include "SkBitmap.h"
#include "SkColorPriv.h"
#include "SkDither.h"
#include "SkRandom.h"
#include "SkMask.h"
#include "SkCoreBlitters.h"
#include "SkUtils.h"
#include "SkShader.h"
#include "SkTemplatesPriv.h"
#include "SkXfermode.h"
#include "SkDevice.h"

#define solid_8_pixels(mask, dst, color)    \
    do {                                    \
        if (mask & 0x80) dst[0] = color;    \
        if (mask & 0x40) dst[1] = color;    \
        if (mask & 0x20) dst[2] = color;    \
        if (mask & 0x10) dst[3] = color;    \
        if (mask & 0x08) dst[4] = color;    \
        if (mask & 0x04) dst[5] = color;    \
        if (mask & 0x02) dst[6] = color;    \
        if (mask & 0x01) dst[7] = color;    \
    } while (0)
#define SK_BLITBWMASK_NAME                  SkRGB16_BlitBW
#define SK_BLITBWMASK_ARGS                  , uint16_t color
#define SK_BLITBWMASK_BLIT8(mask, dst)      solid_8_pixels(mask, dst, color)
#define SK_BLITBWMASK_GETADDR               getAddr16
#define SK_BLITBWMASK_DEVTYPE               uint16_t
#include "SkBlitBWMaskTemplate.h"

int32_t ref_png [1024];
SkBitmap fDeviceR;
SkBitmap fDevice;
uint16_t fColor16;
uint32_t fExpandedRaw16;

static U16CPU blend_compact(uint32_t src32, uint32_t dst32, unsigned scale5) {
    return SkCompact_rgb_16(dst32 + ((src32 - dst32) * scale5 >> 5));
}

/*REFERENT FUNCTIONS*/

void SkRGB16_Opaque_Blitter_blitMask_referent(const SkMask& SK_RESTRICT mask,
                                      const SkIRect& SK_RESTRICT clip) {

    if (mask.fFormat == SkMask::kBW_Format) {
        SkRGB16_BlitBW(fDeviceR, mask, clip, fColor16);
        return;
    }
    uint16_t* SK_RESTRICT device = fDeviceR.getAddr16(clip.fLeft, clip.fTop);

    const uint8_t* SK_RESTRICT alpha = (uint8_t*)mask.getAddr(clip.fLeft, clip.fTop);
    int width = clip.width();
    int height = clip.height();
    int i;

    unsigned    deviceRB = fDeviceR.rowBytes() - (width << 1);
    unsigned    maskRB = mask.fRowBytes - width;
    uint32_t    expanded32 = fExpandedRaw16;

    do {
        int w = width;
        do {
            *device = blend_compact(expanded32, SkExpand_rgb_16(*device),
                                    SkAlpha255To256(*alpha++) >> 3);
            device += 1;

        } while (--w != 0);
        device = (uint16_t*)((char*)device + deviceRB);
        alpha += maskRB;
    } while (--height != 0);
}

/*TEST FUNCTIONS*/

static void test_SkRGB16_Opaque_Blitter_blitMask(skiatest::Reporter* reporter) {
    SkBlitter*  blitter = NULL;
    int width = 10;
    int height = 10;
    int dev = 0x2;
    SkRandom rand;
    fColor16 = rand.nextU16();
    int i;
    SkMask mask;
    SkIRect clip;
    SkPaint paint;
    uint8_t r, g, b;
    for (int i = 0; i < width*height; i++) {
        r = rand.nextU();
        g = rand.nextU();
        b = rand.nextU();
        ref_png[i] = SkColorSetRGB(r, g, b);
    }
    fDevice.setConfig(SkBitmap::kRGB_565_Config, width + dev, height);
    fDevice.allocPixels();
    fDevice.setPixels((void*)ref_png, NULL);
    fDeviceR.setConfig(SkBitmap::kRGB_565_Config, width + dev, height);
    fDeviceR.allocPixels();
    fDeviceR.setPixels((void*)ref_png, NULL);

    mask.fFormat = SkMask::kA8_Format;
    mask.fImage = (uint8_t*)ref_png;
    mask.fRowBytes = (width)*height*4;
    mask.fBounds.set(0, 0, width, height);
    clip.set(0, 0, width, height);
    fExpandedRaw16 = rand.nextU16();

    paint.setColor(SK_ColorMAGENTA);

    SkRGB16_Opaque_Blitter_blitMask_referent(mask, clip);

    blitter = SkBlitter_ChooseD565(fDevice, paint, ref_png, 4*width*height);
    blitter->blitMask(mask,clip);

    uint16_t* device_ref = fDeviceR.getAddr16(clip.fLeft, clip.fTop);
    uint16_t* device_opt = fDevice.getAddr16(clip.fLeft, clip.fTop);

    if ( memcmp((void*)device_ref, (void*)device_opt, fDeviceR.getSize())!=0 ) {
        SkDebugf("test_SkRGB16_Opaque_Blitter_blitMask ERROR!\ndevice_ref: %X, device_opt: %X\n", device_ref, device_opt);
    }
}
//RUN SOME TESTS:
static void TestBlitter_RGB16(skiatest::Reporter* reporter) {
    test_SkRGB16_Opaque_Blitter_blitMask(reporter);
}

#include "TestClassDef.h"
DEFINE_TESTCLASS("Blitter_RGB16", TestBlitter_RGB16Class, TestBlitter_RGB16)
