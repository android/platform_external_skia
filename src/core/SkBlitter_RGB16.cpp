
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "SkBlitRow.h"
#include "SkCoreBlitters.h"
#include "SkColorPriv.h"
#include "SkDither.h"
#include "SkShader.h"
#include "SkUtils.h"
#include "SkUtilsArm.h"
#include "SkXfermode.h"

#if SK_ARM_NEON_IS_ALWAYS && defined(SK_CPU_LENDIAN)
    #include <arm_neon.h>
#else
    // if we don't have neon, then our black blitter is worth the extra code
    #define USE_BLACK_BLITTER
#endif

void sk_dither_memset16(uint16_t dst[], uint16_t value, uint16_t other,
                        int count) {
    if (count > 0) {
        // see if we need to write one short before we can cast to an 4byte ptr
        // (we do this subtract rather than (unsigned)dst so we don't get warnings
        //  on 64bit machines)
        if (((char*)dst - (char*)0) & 2) {
            *dst++ = value;
            count -= 1;
            SkTSwap(value, other);
        }

        // fast way to set [value,other] pairs
#ifdef SK_CPU_BENDIAN
        sk_memset32((uint32_t*)dst, (value << 16) | other, count >> 1);
#else
        sk_memset32((uint32_t*)dst, (other << 16) | value, count >> 1);
#endif

        if (count & 1) {
            dst[count - 1] = value;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

class SkRGB16_Blitter : public SkRasterBlitter {
public:
    SkRGB16_Blitter(const SkBitmap& device, const SkPaint& paint);
    virtual void blitH(int x, int y, int width);
    virtual void blitAntiH(int x, int y, const SkAlpha* antialias,
                           const int16_t* runs);
    virtual void blitV(int x, int y, int height, SkAlpha alpha);
    virtual void blitRect(int x, int y, int width, int height);
    virtual void blitMask(const SkMask&,
                          const SkIRect&);
    virtual const SkBitmap* justAnOpaqueColor(uint32_t*);

protected:
    SkPMColor   fSrcColor32;
    uint32_t    fExpandedRaw16;
    unsigned    fScale;
    uint16_t    fColor16;       // already scaled by fScale
    uint16_t    fRawColor16;    // unscaled
    uint16_t    fRawDither16;   // unscaled
    SkBool8     fDoDither;

    // illegal
    SkRGB16_Blitter& operator=(const SkRGB16_Blitter&);

    typedef SkRasterBlitter INHERITED;
};

class SkRGB16_Opaque_Blitter : public SkRGB16_Blitter {
public:
    SkRGB16_Opaque_Blitter(const SkBitmap& device, const SkPaint& paint);
    virtual void blitH(int x, int y, int width);
    virtual void blitAntiH(int x, int y, const SkAlpha* antialias,
                           const int16_t* runs);
    virtual void blitV(int x, int y, int height, SkAlpha alpha);
    virtual void blitRect(int x, int y, int width, int height);
    virtual void blitMask(const SkMask&,
                          const SkIRect&);

private:
    typedef SkRGB16_Blitter INHERITED;
};

#ifdef USE_BLACK_BLITTER
class SkRGB16_Black_Blitter : public SkRGB16_Opaque_Blitter {
public:
    SkRGB16_Black_Blitter(const SkBitmap& device, const SkPaint& paint);
    virtual void blitMask(const SkMask&, const SkIRect&);
    virtual void blitAntiH(int x, int y, const SkAlpha* antialias,
                           const int16_t* runs);

private:
    typedef SkRGB16_Opaque_Blitter INHERITED;
};
#endif

class SkRGB16_Shader_Blitter : public SkShaderBlitter {
public:
    SkRGB16_Shader_Blitter(const SkBitmap& device, const SkPaint& paint,
                           SkShader::Context* shaderContext);
    virtual ~SkRGB16_Shader_Blitter();
    virtual void blitH(int x, int y, int width);
    virtual void blitAntiH(int x, int y, const SkAlpha* antialias,
                           const int16_t* runs);
    virtual void blitRect(int x, int y, int width, int height);

protected:
    SkPMColor*      fBuffer;
    SkBlitRow::Proc fOpaqueProc;
    SkBlitRow::Proc fAlphaProc;

private:
    // illegal
    SkRGB16_Shader_Blitter& operator=(const SkRGB16_Shader_Blitter&);

    typedef SkShaderBlitter INHERITED;
};

// used only if the shader can perform shadSpan16
class SkRGB16_Shader16_Blitter : public SkRGB16_Shader_Blitter {
public:
    SkRGB16_Shader16_Blitter(const SkBitmap& device, const SkPaint& paint,
                             SkShader::Context* shaderContext);
    virtual void blitH(int x, int y, int width);
    virtual void blitAntiH(int x, int y, const SkAlpha* antialias,
                           const int16_t* runs);
    virtual void blitRect(int x, int y, int width, int height);

private:
    typedef SkRGB16_Shader_Blitter INHERITED;
};

class SkRGB16_Shader_Xfermode_Blitter : public SkShaderBlitter {
public:
    SkRGB16_Shader_Xfermode_Blitter(const SkBitmap& device, const SkPaint& paint,
                                    SkShader::Context* shaderContext);
    virtual ~SkRGB16_Shader_Xfermode_Blitter();
    virtual void blitH(int x, int y, int width);
    virtual void blitAntiH(int x, int y, const SkAlpha* antialias,
                           const int16_t* runs);

private:
    SkXfermode* fXfermode;
    SkPMColor*  fBuffer;
    uint8_t*    fAAExpand;

    // illegal
    SkRGB16_Shader_Xfermode_Blitter& operator=(const SkRGB16_Shader_Xfermode_Blitter&);

    typedef SkShaderBlitter INHERITED;
};

///////////////////////////////////////////////////////////////////////////////
#ifdef USE_BLACK_BLITTER
SkRGB16_Black_Blitter::SkRGB16_Black_Blitter(const SkBitmap& device, const SkPaint& paint)
    : INHERITED(device, paint) {
    SkASSERT(paint.getShader() == NULL);
    SkASSERT(paint.getColorFilter() == NULL);
    SkASSERT(paint.getXfermode() == NULL);
    SkASSERT(paint.getColor() == SK_ColorBLACK);
}

#if 1
#define black_8_pixels(mask, dst)       \
    do {                                \
        if (mask & 0x80) dst[0] = 0;    \
        if (mask & 0x40) dst[1] = 0;    \
        if (mask & 0x20) dst[2] = 0;    \
        if (mask & 0x10) dst[3] = 0;    \
        if (mask & 0x08) dst[4] = 0;    \
        if (mask & 0x04) dst[5] = 0;    \
        if (mask & 0x02) dst[6] = 0;    \
        if (mask & 0x01) dst[7] = 0;    \
    } while (0)
#else
static inline black_8_pixels(U8CPU mask, uint16_t dst[])
{
    if (mask & 0x80) dst[0] = 0;
    if (mask & 0x40) dst[1] = 0;
    if (mask & 0x20) dst[2] = 0;
    if (mask & 0x10) dst[3] = 0;
    if (mask & 0x08) dst[4] = 0;
    if (mask & 0x04) dst[5] = 0;
    if (mask & 0x02) dst[6] = 0;
    if (mask & 0x01) dst[7] = 0;
}
#endif

#define SK_BLITBWMASK_NAME                  SkRGB16_Black_BlitBW
#define SK_BLITBWMASK_ARGS
#define SK_BLITBWMASK_BLIT8(mask, dst)      black_8_pixels(mask, dst)
#define SK_BLITBWMASK_GETADDR               getAddr16
#define SK_BLITBWMASK_DEVTYPE               uint16_t
#include "SkBlitBWMaskTemplate.h"

void SkRGB16_Black_Blitter::blitMask(const SkMask& mask,
                                     const SkIRect& clip) {
    if (mask.fFormat == SkMask::kBW_Format) {
        SkRGB16_Black_BlitBW(fDevice, mask, clip);
    } else {
        uint16_t* SK_RESTRICT device = fDevice.getAddr16(clip.fLeft, clip.fTop);
        const uint8_t* SK_RESTRICT alpha = mask.getAddr8(clip.fLeft, clip.fTop);
        unsigned width = clip.width();
        unsigned height = clip.height();
        size_t deviceRB = fDevice.rowBytes() - (width << 1);
        unsigned maskRB = mask.fRowBytes - width;

        SkASSERT((int)height > 0);
        SkASSERT((int)width > 0);
        SkASSERT((int)deviceRB >= 0);
        SkASSERT((int)maskRB >= 0);

        do {
            unsigned w = width;
            do {
                unsigned aa = *alpha++;
                *device = SkAlphaMulRGB16(*device, SkAlpha255To256(255 - aa));
                device += 1;
            } while (--w != 0);
            device = (uint16_t*)((char*)device + deviceRB);
            alpha += maskRB;
        } while (--height != 0);
    }
}

void SkRGB16_Black_Blitter::blitAntiH(int x, int y,
                                      const SkAlpha* SK_RESTRICT antialias,
                                      const int16_t* SK_RESTRICT runs) {
    uint16_t* SK_RESTRICT device = fDevice.getAddr16(x, y);

    for (;;) {
        int count = runs[0];
        SkASSERT(count >= 0);
        if (count <= 0) {
            return;
        }
        runs += count;

        unsigned aa = antialias[0];
        antialias += count;
        if (aa) {
            if (aa == 255) {
                memset(device, 0, count << 1);
            } else {
                aa = SkAlpha255To256(255 - aa);
                do {
                    *device = SkAlphaMulRGB16(*device, aa);
                    device += 1;
                } while (--count != 0);
                continue;
            }
        }
        device += count;
    }
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

SkRGB16_Opaque_Blitter::SkRGB16_Opaque_Blitter(const SkBitmap& device,
                                               const SkPaint& paint)
: INHERITED(device, paint) {}

void SkRGB16_Opaque_Blitter::blitH(int x, int y, int width) {
    SkASSERT(width > 0);
    SkASSERT(x + width <= fDevice.width());
    uint16_t* SK_RESTRICT device = fDevice.getAddr16(x, y);
    uint16_t srcColor = fColor16;

    SkASSERT(fRawColor16 == srcColor);
    if (fDoDither) {
        uint16_t ditherColor = fRawDither16;
        if ((x ^ y) & 1) {
            SkTSwap(ditherColor, srcColor);
        }
        sk_dither_memset16(device, srcColor, ditherColor, width);
    } else {
        sk_memset16(device, srcColor, width);
    }
}

// return 1 or 0 from a bool
static inline int Bool2Int(int value) {
    return !!value;
}

void SkRGB16_Opaque_Blitter::blitAntiH(int x, int y,
                                       const SkAlpha* SK_RESTRICT antialias,
                                       const int16_t* SK_RESTRICT runs) {
    uint16_t* SK_RESTRICT device = fDevice.getAddr16(x, y);
    uint16_t    srcColor = fRawColor16;
    uint32_t    srcExpanded = fExpandedRaw16;
    int         ditherInt = Bool2Int(fDoDither);
    uint16_t    ditherColor = fRawDither16;
    // if we have no dithering, this will always fail
    if ((x ^ y) & ditherInt) {
        SkTSwap(ditherColor, srcColor);
    }
    for (;;) {
        int count = runs[0];
        SkASSERT(count >= 0);
        if (count <= 0) {
            return;
        }
        runs += count;

        unsigned aa = antialias[0];
        antialias += count;
        if (aa) {
            if (aa == 255) {
                if (ditherInt) {
                    sk_dither_memset16(device, srcColor,
                                       ditherColor, count);
                } else {
                    sk_memset16(device, srcColor, count);
                }
            } else {
                // TODO: respect fDoDither
                unsigned scale5 = SkAlpha255To256(aa) >> 3;
                uint32_t src32 = srcExpanded * scale5;
                scale5 = 32 - scale5; // now we can use it on the device
                int n = count;
                do {
                    uint32_t dst32 = SkExpand_rgb_16(*device) * scale5;
                    *device++ = SkCompact_rgb_16((src32 + dst32) >> 5);
                } while (--n != 0);
                goto DONE;
            }
        }
        device += count;

    DONE:
        // if we have no dithering, this will always fail
        if (count & ditherInt) {
            SkTSwap(ditherColor, srcColor);
        }
    }
}

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

static U16CPU blend_compact(uint32_t src32, uint32_t dst32, unsigned scale5) {
    return SkCompact_rgb_16(dst32 + ((src32 - dst32) * scale5 >> 5));
}

void SkRGB16_Opaque_Blitter::blitMask(const SkMask& mask,
                                      const SkIRect& clip) {
    if (mask.fFormat == SkMask::kBW_Format) {
        SkRGB16_BlitBW(fDevice, mask, clip, fColor16);
        return;
    }

    uint16_t* SK_RESTRICT device = fDevice.getAddr16(clip.fLeft, clip.fTop);
    const uint8_t* SK_RESTRICT alpha = mask.getAddr8(clip.fLeft, clip.fTop);
    int width = clip.width();
    int height = clip.height();
    size_t      deviceRB = fDevice.rowBytes() - (width << 1);
    unsigned    maskRB = mask.fRowBytes - width;
    uint32_t    expanded32 = fExpandedRaw16;

#if SK_ARM_NEON_IS_ALWAYS && defined(SK_CPU_LENDIAN)
#define    UNROLL    8
    do {
        int w = width;
        if (w >= UNROLL) {
            uint32x4_t color, dev_lo, dev_hi;
            uint32x4_t wn1, wn2, tmp;
            uint32x4_t vmask_g16, vmask_ng16;
            uint16x8_t valpha, vdev;
            uint16x4_t odev_lo, odev_hi, valpha_lo, valpha_hi;

            // prepare constants
            vmask_g16 = vdupq_n_u32(SK_G16_MASK_IN_PLACE);
            vmask_ng16 = vdupq_n_u32(~SK_G16_MASK_IN_PLACE);
            color = vdupq_n_u32(expanded32);

            do {
                // alpha is 8x8, widen and split to get a pair of 16x4
                valpha = vaddw_u8(vdupq_n_u16(1), vld1_u8(alpha));
                valpha = vshrq_n_u16(valpha, 3);
                valpha_lo = vget_low_u16(valpha);
                valpha_hi = vget_high_u16(valpha);

                // load pixels
                vdev = vld1q_u16(device);
                dev_lo = vmovl_u16(vget_low_u16(vdev));
                dev_hi = vmovl_u16(vget_high_u16(vdev));

                // unpack them in 32 bits
                dev_lo = (dev_lo & vmask_ng16) | vshlq_n_u32(dev_lo & vmask_g16, 16);
                dev_hi = (dev_hi & vmask_ng16) | vshlq_n_u32(dev_hi & vmask_g16, 16);

                // blend with color
                tmp = (color - dev_lo) * vmovl_u16(valpha_lo);
                tmp = vshrq_n_u32(tmp, 5);
                dev_lo += tmp;

                tmp = vmulq_u32(color - dev_hi, vmovl_u16(valpha_hi));
                tmp = vshrq_n_u32(tmp, 5);
                dev_hi += tmp;

                // re-compact
                wn1 = dev_lo & vmask_ng16;
                wn2 = vshrq_n_u32(dev_lo, 16) & vmask_g16;
                odev_lo = vmovn_u32(wn1 | wn2);

                wn1 = dev_hi & vmask_ng16;
                wn2 = vshrq_n_u32(dev_hi, 16) & vmask_g16;
                odev_hi = vmovn_u32(wn1 | wn2);

                // store
                vst1q_u16(device, vcombine_u16(odev_lo, odev_hi));

                device += UNROLL;
                alpha += UNROLL;
                w -= UNROLL;
            } while (w >= UNROLL);
        }

        // residuals
        while (w > 0) {
            *device = blend_compact(expanded32, SkExpand_rgb_16(*device),
                                    SkAlpha255To256(*alpha++) >> 3);
            device += 1;
            --w;
        }
        device = (uint16_t*)((char*)device + deviceRB);
        alpha += maskRB;
    } while (--height != 0);
#undef    UNROLL
#else   // non-neon code
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
#endif
}

void SkRGB16_Opaque_Blitter::blitV(int x, int y, int height, SkAlpha alpha) {
    uint16_t* SK_RESTRICT device = fDevice.getAddr16(x, y);
    size_t    deviceRB = fDevice.rowBytes();

    // TODO: respect fDoDither
    unsigned scale5 = SkAlpha255To256(alpha) >> 3;
    uint32_t src32 =  fExpandedRaw16 * scale5;
    scale5 = 32 - scale5;
    do {
        uint32_t dst32 = SkExpand_rgb_16(*device) * scale5;
        *device = SkCompact_rgb_16((src32 + dst32) >> 5);
        device = (uint16_t*)((char*)device + deviceRB);
    } while (--height != 0);
}

void SkRGB16_Opaque_Blitter::blitRect(int x, int y, int width, int height) {
    SkASSERT(x + width <= fDevice.width() && y + height <= fDevice.height());
    uint16_t* SK_RESTRICT device = fDevice.getAddr16(x, y);
    size_t      deviceRB = fDevice.rowBytes();
    uint16_t    color16 = fColor16;

    if (fDoDither) {
        uint16_t ditherColor = fRawDither16;
        if ((x ^ y) & 1) {
            SkTSwap(ditherColor, color16);
        }
        while (--height >= 0) {
            sk_dither_memset16(device, color16, ditherColor, width);
            SkTSwap(ditherColor, color16);
            device = (uint16_t*)((char*)device + deviceRB);
        }
    } else {  // no dither
        while (--height >= 0) {
            sk_memset16(device, color16, width);
            device = (uint16_t*)((char*)device + deviceRB);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

SkRGB16_Blitter::SkRGB16_Blitter(const SkBitmap& device, const SkPaint& paint)
    : INHERITED(device) {
    SkColor color = paint.getColor();

    fSrcColor32 = SkPreMultiplyColor(color);
    fScale = SkAlpha255To256(SkColorGetA(color));

    int r = SkColorGetR(color);
    int g = SkColorGetG(color);
    int b = SkColorGetB(color);

    fRawColor16 = fRawDither16 = SkPack888ToRGB16(r, g, b);
    // if we're dithered, use fRawDither16 to hold that.
    if ((fDoDither = paint.isDither()) != false) {
        fRawDither16 = SkDitherPack888ToRGB16(r, g, b);
    }

    fExpandedRaw16 = SkExpand_rgb_16(fRawColor16);

    fColor16 = SkPackRGB16( SkAlphaMul(r, fScale) >> (8 - SK_R16_BITS),
                            SkAlphaMul(g, fScale) >> (8 - SK_G16_BITS),
                            SkAlphaMul(b, fScale) >> (8 - SK_B16_BITS));
}

const SkBitmap* SkRGB16_Blitter::justAnOpaqueColor(uint32_t* value) {
    if (!fDoDither && 256 == fScale) {
        *value = fRawColor16;
        return &fDevice;
    }
    return NULL;
}

static uint32_t pmcolor_to_expand16(SkPMColor c) {
    unsigned r = SkGetPackedR32(c);
    unsigned g = SkGetPackedG32(c);
    unsigned b = SkGetPackedB32(c);
    return (g << 24) | (r << 13) | (b << 2);
}

#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)

/* a prefetch distance of 4 cache-lines works best experimentally */

static void blend32_16_row_neon(const SkPMColor* SK_RESTRICT src,
                                 uint16_t* SK_RESTRICT dst, int count) {
    uint32_t src_expand;
    unsigned scale;

    if (count <= 0) return;
    SkASSERT(((size_t)dst & 0x01) == 0);

    /*
     * This preamble code is in order to make dst aligned to 8 bytes
     * in the next mutiple bytes read & write access.
     */
    src_expand = pmcolor_to_expand16(*src);
    scale = SkAlpha255To256(0xFF - SkGetPackedA32(*src)) >> 3;

#define DST_ALIGN 8

    /*
     * preamble_size is in byte, meantime, this blend32_16_row_neon updates 2 bytes at a time.
     */
    int preamble_size = (DST_ALIGN - (size_t)dst) & (DST_ALIGN - 1);

    for (int i = 0; i < preamble_size; i+=2, dst++) {
        uint32_t dst_expand = SkExpand_rgb_16(*dst) * scale;
        *dst = SkCompact_rgb_16((src_expand + dst_expand) >> 5);
        if (--count == 0)
            break;
    }

    asm volatile (
    /* This code refers to a Neon version of S32A_D565_Blend and is modified in order to make it work
     * as blend32_16_row. For detail, see S32A_D565_Blend_neon.
     */
                  "movs       r4, %[count], lsr #4                \n\t"   // calc. count>>4
                  "beq        2f                                  \n\t"   // if count16 == 0, exit
                  "vmov.u16   q15, #0x1f                          \n\t"   // set up blue mask
                  "vld4.u8    {d24, d25, d26, d27}, [%[src]]      \n\t"   // load eight src ABGR32 pixels

                  "vmov   r5, r6, d24                             \n\t"   // save src red in r5, r6
                  "vmov   r7, r8, d25                             \n\t"   // save src green in r7, r8
                  "vmov   r9, r10, d26                            \n\t"   // save src blue in r9, r10
                  "vmov   r11, r12, d27                           \n\t"   // save src alpha in r11, r12


                  "1:                                             \n\t"
                  "vld1.u16   {d0, d1, d2, d3}, [%[dst]]          \n\t"   // load sixteen dst RGB565 pixels
                  //set PREFETCH_DISTANCE to 128
                  "pld        [%[dst], #128]                      \n\t"

                  "subs       r4, r4, #1                          \n\t"   // decrement loop counter

                  "vmov   d24, r5, r6                             \n\t"   // src red to d24
                  "vmov   d25, r7, r8                             \n\t"   // src green to d25
                  "vmov   d26, r9, r10                            \n\t"   // src blue to d26
                  "vmov   d27, r11, r12                           \n\t"   // src alpha to d27

                  "vmov.u16   q3, #256                            \n\t"   // set up constant
                  "vmovl.u8   q14, d27                            \n\t"   // widen alpha to 16 bits
                  // dst_scale = q14
                  "vsub.u16   q14, q3, q14                        \n\t"   // 256 - sa
                  "vshr.u16   q14, q14, #3                        \n\t"   // (256 - sa) >> 3


                  // dst_0_rgb = {q8, q9, q10}
                  "vshl.u16   q9, q0, #5                          \n\t"   // shift green to top of lanes
                  "vand       q10, q0, q15                        \n\t"   // extract blue
                  "vshr.u16   q8, q0, #11                         \n\t"   // extract red
                  "vshr.u16   q9, q9, #10                         \n\t"   // extract green

                  //use q3 for dst_1 green. In the next loop, needs to set q3 to 256 again.
                  // dst_1_rgb = {q2, q3, q7}
                  "vshl.u16   q3, q1, #5                          \n\t"   // shift green to top of lanes
                  "vand       q7, q1, q15                         \n\t"   // extract blue
                  "vshr.u16   q2, q1, #11                         \n\t"   // extract red
                  "vshr.u16   q3, q3, #10                         \n\t"   // extract green

                  // srcrgba = {q4, q5, q6, q14}, alpha calculation is done already in above.
                  // q4, q5, q6 will have each channel's result of dst_1_rgb.
                  "vmovl.u8   q4, d24                             \n\t"   // widen red to 16 bits
                  "vmovl.u8   q5, d25                             \n\t"   // widen green to 16 bits
                  "vmovl.u8   q6, d26                             \n\t"   // widen blue to 16 bits

                  // srcrgba = {q11, q12, q13, q14}, alpha calculation is done already in above.
                  // q11, q12, q13 will have each channel's result of dst_0_rgb.
                  "vmovl.u8   q11, d24                            \n\t"   // widen red to 16 bits
                  "vmovl.u8   q12, d25                            \n\t"   // widen green to 16 bits
                  "vmovl.u8   q13, d26                            \n\t"   // widen blue to 16 bits

                  "vshl.u16   q11, q11, #2                        \n\t"   // dst 0 red result = src_red << 2 (later will do >> 5 to make 5 bit red)
                  "vshl.u16   q12, q12, #3                        \n\t"   // dst 0 grn result = src_grn << 3 (later will do >> 5 to make 6 bit grn)
                  "vshl.u16   q13, q13, #2                        \n\t"   // dst 0 blu result = src_blu << 2 (later will do >> 5 to make 5 bit blu)

                  "vshl.u16   q4, q4, #2                          \n\t"   // dst 1 red result = src_red << 2 (later will do >> 5 to make 5 bit red)
                  "vshl.u16   q5, q5, #3                          \n\t"   // dst 1 grn result = src_grn << 3 (later will do >> 5 to make 6 bit grn)
                  "vshl.u16   q6, q6, #2                          \n\t"   // dst 1 blu result = src_blu << 2 (later will do >> 5 to make 5 bit blu)

                  "vmla.u16   q11, q8, q14                        \n\t"   // dst 0 red result += dst_red * dst_scale
                  "vmla.u16   q12, q9, q14                        \n\t"   // dst 0 grn result += dst_grn * dst_scale
                  "vmla.u16   q13, q10, q14                       \n\t"   // dst 0 blu result += dst_blu * dst_scale

                  "vmla.u16   q4, q2, q14                         \n\t"   // dst 1 red result += dst_red * dst_scale
                  "vmla.u16   q5, q3, q14                         \n\t"   // dst 1 grn result += dst_grn * dst_scale
                  "vmla.u16   q6, q7, q14                         \n\t"   // dst 1 blu result += dst_blu * dst_scale

                  "vshr.u16   q11, q11, #5                        \n\t"   // dst 0 red result >> 5
                  "vshr.u16   q12, q12, #5                        \n\t"   // dst 0 grn result >> 5
                  "vshr.u16   q13, q13, #5                        \n\t"   // dst 0 blu result >> 5

                  "vshr.u16   q4, q4, #5                          \n\t"   // dst 1 red result >> 5
                  "vshr.u16   q5, q5, #5                          \n\t"   // dst 1 grn result >> 5
                  "vshr.u16   q14, q6, #5                         \n\t"   // dst 1 blu result >> 5

                  "vsli.u16   q13, q12, #5                        \n\t"   // dst 0 insert green into blue
                  "vsli.u16   q13, q11, #11                       \n\t"   // dst 0 insert red into green/blue

                  "vsli.u16   q14, q5, #5                         \n\t"   // dst 1 insert green into blue
                  "vsli.u16   q14, q4, #11                        \n\t"   // dst 1 insert red into green/blue

                  "vst1.16    {d26, d27, d28, d29}, [%[dst]]!     \n\t"   // write pixel back to dst 0 and dst 1, update ptr

                  "bne        1b                                  \n\t"   // if counter != 0, loop
                  "2:                                             \n\t"   // exit

                  : [src] "+r" (src), [dst] "+r" (dst), [count] "+r" (count)
                  :
                  : "cc", "memory", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12",
                                          "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15", "d16",
                                          "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31"
                  );

    count &= 0xF;
    if (count > 0) {
        do {
            uint32_t dst_expand = SkExpand_rgb_16(*dst) * scale;
            *dst = SkCompact_rgb_16((src_expand + dst_expand) >> 5);
            dst += 1;
        } while (--count != 0);
    }
}

#endif
static inline void blend32_16_row(SkPMColor src, uint16_t dst[], int count) {
    SkASSERT(count > 0);
    uint32_t src_expand = pmcolor_to_expand16(src);
    unsigned scale = SkAlpha255To256(0xFF - SkGetPackedA32(src)) >> 3;
    do {
        uint32_t dst_expand = SkExpand_rgb_16(*dst) * scale;
        *dst = SkCompact_rgb_16((src_expand + dst_expand) >> 5);
        dst += 1;
    } while (--count != 0);
}

void SkRGB16_Blitter::blitH(int x, int y, int width) {
    SkASSERT(width > 0);
    SkASSERT(x + width <= fDevice.width());
    uint16_t* SK_RESTRICT device = fDevice.getAddr16(x, y);

    // TODO: respect fDoDither
#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
    SkPMColor src32s[8] = { fSrcColor32, fSrcColor32, fSrcColor32, fSrcColor32,
                            fSrcColor32, fSrcColor32, fSrcColor32, fSrcColor32 };
    if (((size_t)device & 0x01) == 0)
        blend32_16_row_neon(src32s, device, width);
    else
        blend32_16_row(fSrcColor32, device, width);
#else
    blend32_16_row(fSrcColor32, device, width);
#endif
}

void SkRGB16_Blitter::blitAntiH(int x, int y,
                                const SkAlpha* SK_RESTRICT antialias,
                                const int16_t* SK_RESTRICT runs) {
    uint16_t* SK_RESTRICT device = fDevice.getAddr16(x, y);
    uint32_t    srcExpanded = fExpandedRaw16;
    unsigned    scale = fScale;

    // TODO: respect fDoDither
    for (;;) {
        int count = runs[0];
        SkASSERT(count >= 0);
        if (count <= 0) {
            return;
        }
        runs += count;

        unsigned aa = antialias[0];
        antialias += count;
        if (aa) {
            unsigned scale5 = SkAlpha255To256(aa) * scale >> (8 + 3);
            uint32_t src32 =  srcExpanded * scale5;
            scale5 = 32 - scale5;
            do {
                uint32_t dst32 = SkExpand_rgb_16(*device) * scale5;
                *device++ = SkCompact_rgb_16((src32 + dst32) >> 5);
            } while (--count != 0);
            continue;
        }
        device += count;
    }
}

static inline void blend_8_pixels(U8CPU bw, uint16_t dst[], unsigned dst_scale,
                                  U16CPU srcColor) {
    if (bw & 0x80) dst[0] = srcColor + SkAlphaMulRGB16(dst[0], dst_scale);
    if (bw & 0x40) dst[1] = srcColor + SkAlphaMulRGB16(dst[1], dst_scale);
    if (bw & 0x20) dst[2] = srcColor + SkAlphaMulRGB16(dst[2], dst_scale);
    if (bw & 0x10) dst[3] = srcColor + SkAlphaMulRGB16(dst[3], dst_scale);
    if (bw & 0x08) dst[4] = srcColor + SkAlphaMulRGB16(dst[4], dst_scale);
    if (bw & 0x04) dst[5] = srcColor + SkAlphaMulRGB16(dst[5], dst_scale);
    if (bw & 0x02) dst[6] = srcColor + SkAlphaMulRGB16(dst[6], dst_scale);
    if (bw & 0x01) dst[7] = srcColor + SkAlphaMulRGB16(dst[7], dst_scale);
}

#define SK_BLITBWMASK_NAME                  SkRGB16_BlendBW
#define SK_BLITBWMASK_ARGS                  , unsigned dst_scale, U16CPU src_color
#define SK_BLITBWMASK_BLIT8(mask, dst)      blend_8_pixels(mask, dst, dst_scale, src_color)
#define SK_BLITBWMASK_GETADDR               getAddr16
#define SK_BLITBWMASK_DEVTYPE               uint16_t
#include "SkBlitBWMaskTemplate.h"

void SkRGB16_Blitter::blitMask(const SkMask& mask,
                               const SkIRect& clip) {
    if (mask.fFormat == SkMask::kBW_Format) {
        SkRGB16_BlendBW(fDevice, mask, clip, 256 - fScale, fColor16);
        return;
    }

    uint16_t* SK_RESTRICT device = fDevice.getAddr16(clip.fLeft, clip.fTop);
    const uint8_t* SK_RESTRICT alpha = mask.getAddr8(clip.fLeft, clip.fTop);
    int width = clip.width();
    int height = clip.height();
    size_t      deviceRB = fDevice.rowBytes() - (width << 1);
    unsigned    maskRB = mask.fRowBytes - width;
    uint32_t    color32 = fExpandedRaw16;

    unsigned scale256 = fScale;
    do {
        int w = width;
        do {
            unsigned aa = *alpha++;
            unsigned scale = SkAlpha255To256(aa) * scale256 >> (8 + 3);
            uint32_t src32 = color32 * scale;
            uint32_t dst32 = SkExpand_rgb_16(*device) * (32 - scale);
            *device++ = SkCompact_rgb_16((src32 + dst32) >> 5);
        } while (--w != 0);
        device = (uint16_t*)((char*)device + deviceRB);
        alpha += maskRB;
    } while (--height != 0);
}

void SkRGB16_Blitter::blitV(int x, int y, int height, SkAlpha alpha) {
    uint16_t* SK_RESTRICT device = fDevice.getAddr16(x, y);
    size_t    deviceRB = fDevice.rowBytes();

    // TODO: respect fDoDither
    unsigned scale5 = SkAlpha255To256(alpha) * fScale >> (8 + 3);
    uint32_t src32 =  fExpandedRaw16 * scale5;
    scale5 = 32 - scale5;
    do {
        uint32_t dst32 = SkExpand_rgb_16(*device) * scale5;
        *device = SkCompact_rgb_16((src32 + dst32) >> 5);
        device = (uint16_t*)((char*)device + deviceRB);
    } while (--height != 0);
}

void SkRGB16_Blitter::blitRect(int x, int y, int width, int height) {
    SkASSERT(x + width <= fDevice.width() && y + height <= fDevice.height());
    uint16_t* SK_RESTRICT device = fDevice.getAddr16(x, y);
    size_t    deviceRB = fDevice.rowBytes();
    SkPMColor src32 = fSrcColor32;
#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
    SkPMColor src32s[8] = { src32, src32, src32, src32,
                                 src32, src32, src32, src32 };
#endif

    while (--height >= 0) {
#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
    if (((size_t)device & 0x01) == 0)
        blend32_16_row_neon(src32s, device, width);
    else
        blend32_16_row(src32, device, width);
#else
        blend32_16_row(src32, device, width);
#endif
        device = (uint16_t*)((char*)device + deviceRB);
    }
}

///////////////////////////////////////////////////////////////////////////////

SkRGB16_Shader16_Blitter::SkRGB16_Shader16_Blitter(const SkBitmap& device,
                                                   const SkPaint& paint,
                                                   SkShader::Context* shaderContext)
    : SkRGB16_Shader_Blitter(device, paint, shaderContext) {
    SkASSERT(SkShader::CanCallShadeSpan16(fShaderFlags));
}

void SkRGB16_Shader16_Blitter::blitH(int x, int y, int width) {
    SkASSERT(x + width <= fDevice.width());

    uint16_t* SK_RESTRICT device = fDevice.getAddr16(x, y);
    SkShader::Context*    shaderContext = fShaderContext;

    int alpha = shaderContext->getSpan16Alpha();
    if (0xFF == alpha) {
        shaderContext->shadeSpan16(x, y, device, width);
    } else {
        uint16_t* span16 = (uint16_t*)fBuffer;
        shaderContext->shadeSpan16(x, y, span16, width);
        SkBlendRGB16(span16, device, SkAlpha255To256(alpha), width);
    }
}

void SkRGB16_Shader16_Blitter::blitRect(int x, int y, int width, int height) {
    SkShader::Context* shaderContext = fShaderContext;
    uint16_t*          dst = fDevice.getAddr16(x, y);
    size_t             dstRB = fDevice.rowBytes();
    int                alpha = shaderContext->getSpan16Alpha();

    if (0xFF == alpha) {
        if (fShaderFlags & SkShader::kConstInY16_Flag) {
            // have the shader blit directly into the device the first time
            shaderContext->shadeSpan16(x, y, dst, width);
            // and now just memcpy that line on the subsequent lines
            if (--height > 0) {
                const uint16_t* orig = dst;
                do {
                    dst = (uint16_t*)((char*)dst + dstRB);
                    memcpy(dst, orig, width << 1);
                } while (--height);
            }
        } else {    // need to call shadeSpan16 for every line
            do {
                shaderContext->shadeSpan16(x, y, dst, width);
                y += 1;
                dst = (uint16_t*)((char*)dst + dstRB);
            } while (--height);
        }
    } else {
        int scale = SkAlpha255To256(alpha);
        uint16_t* span16 = (uint16_t*)fBuffer;
        if (fShaderFlags & SkShader::kConstInY16_Flag) {
            shaderContext->shadeSpan16(x, y, span16, width);
            do {
                SkBlendRGB16(span16, dst, scale, width);
                dst = (uint16_t*)((char*)dst + dstRB);
            } while (--height);
        } else {
            do {
                shaderContext->shadeSpan16(x, y, span16, width);
                SkBlendRGB16(span16, dst, scale, width);
                y += 1;
                dst = (uint16_t*)((char*)dst + dstRB);
            } while (--height);
        }
    }
}

void SkRGB16_Shader16_Blitter::blitAntiH(int x, int y,
                                         const SkAlpha* SK_RESTRICT antialias,
                                         const int16_t* SK_RESTRICT runs) {
    SkShader::Context*     shaderContext = fShaderContext;
    SkPMColor* SK_RESTRICT span = fBuffer;
    uint16_t* SK_RESTRICT  device = fDevice.getAddr16(x, y);

    int alpha = shaderContext->getSpan16Alpha();
    uint16_t* span16 = (uint16_t*)span;

    if (0xFF == alpha) {
        for (;;) {
            int count = *runs;
            if (count <= 0) {
                break;
            }
            SkASSERT(count <= fDevice.width()); // don't overrun fBuffer

            int aa = *antialias;
            if (aa == 255) {
                // go direct to the device!
                shaderContext->shadeSpan16(x, y, device, count);
            } else if (aa) {
                shaderContext->shadeSpan16(x, y, span16, count);
                SkBlendRGB16(span16, device, SkAlpha255To256(aa), count);
            }
            device += count;
            runs += count;
            antialias += count;
            x += count;
        }
    } else {  // span alpha is < 255
        alpha = SkAlpha255To256(alpha);
        for (;;) {
            int count = *runs;
            if (count <= 0) {
                break;
            }
            SkASSERT(count <= fDevice.width()); // don't overrun fBuffer

            int aa = SkAlphaMul(*antialias, alpha);
            if (aa) {
                shaderContext->shadeSpan16(x, y, span16, count);
                SkBlendRGB16(span16, device, SkAlpha255To256(aa), count);
            }

            device += count;
            runs += count;
            antialias += count;
            x += count;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

SkRGB16_Shader_Blitter::SkRGB16_Shader_Blitter(const SkBitmap& device,
                                               const SkPaint& paint,
                                               SkShader::Context* shaderContext)
: INHERITED(device, paint, shaderContext) {
    SkASSERT(paint.getXfermode() == NULL);

    fBuffer = (SkPMColor*)sk_malloc_throw(device.width() * sizeof(SkPMColor));

    // compute SkBlitRow::Procs
    unsigned flags = 0;

    uint32_t shaderFlags = fShaderFlags;
    // shaders take care of global alpha, so we never set it in SkBlitRow
    if (!(shaderFlags & SkShader::kOpaqueAlpha_Flag)) {
        flags |= SkBlitRow::kSrcPixelAlpha_Flag;
    }
    // don't dither if the shader is really 16bit
    if (paint.isDither() && !(shaderFlags & SkShader::kIntrinsicly16_Flag)) {
        flags |= SkBlitRow::kDither_Flag;
    }
    // used when we know our global alpha is 0xFF
    fOpaqueProc = SkBlitRow::Factory(flags, kRGB_565_SkColorType);
    // used when we know our global alpha is < 0xFF
    fAlphaProc  = SkBlitRow::Factory(flags | SkBlitRow::kGlobalAlpha_Flag,
                                     kRGB_565_SkColorType);
}

SkRGB16_Shader_Blitter::~SkRGB16_Shader_Blitter() {
    sk_free(fBuffer);
}

void SkRGB16_Shader_Blitter::blitH(int x, int y, int width) {
    SkASSERT(x + width <= fDevice.width());

    fShaderContext->shadeSpan(x, y, fBuffer, width);
    // shaders take care of global alpha, so we pass 0xFF (should be ignored)
    fOpaqueProc(fDevice.getAddr16(x, y), fBuffer, width, 0xFF, x, y);
}

void SkRGB16_Shader_Blitter::blitRect(int x, int y, int width, int height) {
    SkShader::Context* shaderContext = fShaderContext;
    SkBlitRow::Proc    proc = fOpaqueProc;
    SkPMColor*         buffer = fBuffer;
    uint16_t*          dst = fDevice.getAddr16(x, y);
    size_t             dstRB = fDevice.rowBytes();

    if (fShaderFlags & SkShader::kConstInY32_Flag) {
        shaderContext->shadeSpan(x, y, buffer, width);
        do {
            proc(dst, buffer, width, 0xFF, x, y);
            y += 1;
            dst = (uint16_t*)((char*)dst + dstRB);
        } while (--height);
    } else {
        do {
            shaderContext->shadeSpan(x, y, buffer, width);
            proc(dst, buffer, width, 0xFF, x, y);
            y += 1;
            dst = (uint16_t*)((char*)dst + dstRB);
        } while (--height);
    }
}

static inline int count_nonzero_span(const int16_t runs[], const SkAlpha aa[]) {
    int count = 0;
    for (;;) {
        int n = *runs;
        if (n == 0 || *aa == 0) {
            break;
        }
        runs += n;
        aa += n;
        count += n;
    }
    return count;
}

void SkRGB16_Shader_Blitter::blitAntiH(int x, int y,
                                       const SkAlpha* SK_RESTRICT antialias,
                                       const int16_t* SK_RESTRICT runs) {
    SkShader::Context*     shaderContext = fShaderContext;
    SkPMColor* SK_RESTRICT span = fBuffer;
    uint16_t* SK_RESTRICT  device = fDevice.getAddr16(x, y);

    for (;;) {
        int count = *runs;
        if (count <= 0) {
            break;
        }
        int aa = *antialias;
        if (0 == aa) {
            device += count;
            runs += count;
            antialias += count;
            x += count;
            continue;
        }

        int nonZeroCount = count + count_nonzero_span(runs + count, antialias + count);

        SkASSERT(nonZeroCount <= fDevice.width()); // don't overrun fBuffer
        shaderContext->shadeSpan(x, y, span, nonZeroCount);

        SkPMColor* localSpan = span;
        for (;;) {
            SkBlitRow::Proc proc = (aa == 0xFF) ? fOpaqueProc : fAlphaProc;
            proc(device, localSpan, count, aa, x, y);

            x += count;
            device += count;
            runs += count;
            antialias += count;
            nonZeroCount -= count;
            if (nonZeroCount == 0) {
                break;
            }
            localSpan += count;
            SkASSERT(nonZeroCount > 0);
            count = *runs;
            SkASSERT(count > 0);
            aa = *antialias;
        }
    }
}

///////////////////////////////////////////////////////////////////////

SkRGB16_Shader_Xfermode_Blitter::SkRGB16_Shader_Xfermode_Blitter(
                                const SkBitmap& device, const SkPaint& paint,
                                SkShader::Context* shaderContext)
: INHERITED(device, paint, shaderContext) {
    fXfermode = paint.getXfermode();
    SkASSERT(fXfermode);
    fXfermode->ref();

    int width = device.width();
    fBuffer = (SkPMColor*)sk_malloc_throw((width + (SkAlign4(width) >> 2)) * sizeof(SkPMColor));
    fAAExpand = (uint8_t*)(fBuffer + width);
}

SkRGB16_Shader_Xfermode_Blitter::~SkRGB16_Shader_Xfermode_Blitter() {
    fXfermode->unref();
    sk_free(fBuffer);
}

void SkRGB16_Shader_Xfermode_Blitter::blitH(int x, int y, int width) {
    SkASSERT(x + width <= fDevice.width());

    uint16_t*   device = fDevice.getAddr16(x, y);
    SkPMColor*  span = fBuffer;

    fShaderContext->shadeSpan(x, y, span, width);
    fXfermode->xfer16(device, span, width, NULL);
}

void SkRGB16_Shader_Xfermode_Blitter::blitAntiH(int x, int y,
                                const SkAlpha* SK_RESTRICT antialias,
                                const int16_t* SK_RESTRICT runs) {
    SkShader::Context*     shaderContext = fShaderContext;
    SkXfermode*            mode = fXfermode;
    SkPMColor* SK_RESTRICT span = fBuffer;
    uint8_t* SK_RESTRICT   aaExpand = fAAExpand;
    uint16_t* SK_RESTRICT  device = fDevice.getAddr16(x, y);

    for (;;) {
        int count = *runs;
        if (count <= 0) {
            break;
        }
        int aa = *antialias;
        if (0 == aa) {
            device += count;
            runs += count;
            antialias += count;
            x += count;
            continue;
        }

        int nonZeroCount = count + count_nonzero_span(runs + count,
                                                      antialias + count);

        SkASSERT(nonZeroCount <= fDevice.width()); // don't overrun fBuffer
        shaderContext->shadeSpan(x, y, span, nonZeroCount);

        x += nonZeroCount;
        SkPMColor* localSpan = span;
        for (;;) {
            if (aa == 0xFF) {
                mode->xfer16(device, localSpan, count, NULL);
            } else {
                SkASSERT(aa);
                memset(aaExpand, aa, count);
                mode->xfer16(device, localSpan, count, aaExpand);
            }
            device += count;
            runs += count;
            antialias += count;
            nonZeroCount -= count;
            if (nonZeroCount == 0) {
                break;
            }
            localSpan += count;
            SkASSERT(nonZeroCount > 0);
            count = *runs;
            SkASSERT(count > 0);
            aa = *antialias;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

SkBlitter* SkBlitter_ChooseD565(const SkBitmap& device, const SkPaint& paint,
        SkShader::Context* shaderContext,
        SkTBlitterAllocator* allocator) {
    SkASSERT(allocator != NULL);

    SkBlitter* blitter;
    SkShader* shader = paint.getShader();
    SkXfermode* mode = paint.getXfermode();

    // we require a shader if there is an xfermode, handled by our caller
    SkASSERT(NULL == mode || NULL != shader);

    if (shader) {
        SkASSERT(shaderContext != NULL);
        if (mode) {
            blitter = allocator->createT<SkRGB16_Shader_Xfermode_Blitter>(device, paint,
                                                                          shaderContext);
        } else if (shaderContext->canCallShadeSpan16()) {
            blitter = allocator->createT<SkRGB16_Shader16_Blitter>(device, paint, shaderContext);
        } else {
            blitter = allocator->createT<SkRGB16_Shader_Blitter>(device, paint, shaderContext);
        }
    } else {
        // no shader, no xfermode, (and we always ignore colorfilter)
        SkColor color = paint.getColor();
        if (0 == SkColorGetA(color)) {
            blitter = allocator->createT<SkNullBlitter>();
#ifdef USE_BLACK_BLITTER
        } else if (SK_ColorBLACK == color) {
            blitter = allocator->createT<SkRGB16_Black_Blitter>(device, paint);
#endif
        } else if (0xFF == SkColorGetA(color)) {
            blitter = allocator->createT<SkRGB16_Opaque_Blitter>(device, paint);
        } else {
            blitter = allocator->createT<SkRGB16_Blitter>(device, paint);
        }
    }

    return blitter;
}
