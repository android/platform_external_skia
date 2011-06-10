/* libs/graphics/ports/SkFontHost_FreeType.cpp
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkDescriptor.h"
#include "SkFDot6.h"
#include "SkFontHost.h"
#include "SkMask.h"
#include "SkAdvancedTypefaceMetrics.h"
#include "SkScalerContext.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTemplates.h"
#include "SkThread.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_SIZES_H
#include FT_TRUETYPE_TABLES_H
#include FT_TYPE1_TABLES_H
#include FT_BITMAP_H
// In the past, FT_GlyphSlot_Own_Bitmap was defined in this header file.
#include FT_SYNTHESIS_H
#include FT_XFREE86_H
#include FT_LCD_FILTER_H

#ifdef   FT_ADVANCES_H
#include FT_ADVANCES_H
#endif

#if 0
// Also include the files by name for build tools which require this.
#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ftsizes.h>
#include <freetype/tttables.h>
#include <freetype/ftadvanc.h>
#include <freetype/ftlcdfil.h>
#include <freetype/ftbitmap.h>
#include <freetype/ftsynth.h>
#endif

#include "freetype/ttnameid.h"
#include "freetype/ftsnames.h"
#include "unicode/ucnv.h"       /* C   Converter API          */
#include "unicode/ustring.h"    /* some more string functions */

//#define ENABLE_GLYPH_SPEW     // for tracing calls
//#define DUMP_STRIKE_CREATION

#ifdef SK_DEBUG
    #define SkASSERT_CONTINUE(pred)                                                         \
        do {                                                                                \
            if (!(pred))                                                                    \
                SkDebugf("file %s:%d: assert failed '" #pred "'\n", __FILE__, __LINE__);    \
        } while (false)
#else
    #define SkASSERT_CONTINUE(pred)
#endif

using namespace skia_advanced_typeface_metrics_utils;

#define SK_ENCODING_SHIFTJIS    "shift_jis"
#define SK_ENCODING_UTF8        "UTF-8"

//////////////////////////////////////////////////////////////////////////

struct SkFaceRec;

static SkMutex      gFTMutex;
static int          gFTCount;
static FT_Library   gFTLibrary;
static SkFaceRec*   gFaceRecHead;
static bool         gLCDSupportValid;  // true iff |gLCDSupport| has been set.
static bool         gLCDSupport;  // true iff LCD is supported by the runtime.

/////////////////////////////////////////////////////////////////////////

// See http://freetype.sourceforge.net/freetype2/docs/reference/ft2-bitmap_handling.html#FT_Bitmap_Embolden
// This value was chosen by eyeballing the result in Firefox and trying to match it.
static const FT_Pos kBitmapEmboldenStrength = 1 << 6;

static bool
InitFreetype() {
    FT_Error err = FT_Init_FreeType(&gFTLibrary);
    if (err) {
        return false;
    }

    // Setup LCD filtering. This reduces colour fringes for LCD rendered
    // glyphs.
    err = FT_Library_SetLcdFilter(gFTLibrary, FT_LCD_FILTER_DEFAULT);
    gLCDSupport = err == 0;
    gLCDSupportValid = true;

    return true;
}

class SkScalerContext_FreeType : public SkScalerContext {
public:
    SkScalerContext_FreeType(const SkDescriptor* desc);
    virtual ~SkScalerContext_FreeType();

    bool success() const {
        return fFaceRec != NULL &&
               fFTSize != NULL &&
               fFace != NULL;
    }

protected:
    virtual unsigned generateGlyphCount();
    virtual uint16_t generateCharToGlyph(SkUnichar uni);
    virtual void generateAdvance(SkGlyph* glyph);
    virtual void generateMetrics(SkGlyph* glyph);
    virtual void generateImage(const SkGlyph& glyph);
    virtual void generatePath(const SkGlyph& glyph, SkPath* path);
    virtual void generateFontMetrics(SkPaint::FontMetrics* mx,
                                     SkPaint::FontMetrics* my);
    virtual SkUnichar generateGlyphToChar(uint16_t glyph);

private:
    SkFaceRec*  fFaceRec;
    FT_Face     fFace;              // reference to shared face in gFaceRecHead
    FT_Size     fFTSize;            // our own copy
    SkFixed     fScaleX, fScaleY;
    FT_Matrix   fMatrix22;
    uint32_t    fLoadGlyphFlags;

    FT_Error setupSize();
    void emboldenOutline(FT_Outline* outline);
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

#include "SkStream.h"

struct SkFaceRec {
    SkFaceRec*      fNext;
    FT_Face         fFace;
    FT_StreamRec    fFTStream;
    SkStream*       fSkStream;
    uint32_t        fRefCnt;
    uint32_t        fFontID;

    // assumes ownership of the stream, will call unref() when its done
    SkFaceRec(SkStream* strm, uint32_t fontID);
    ~SkFaceRec() {
        fSkStream->unref();
    }
};

extern "C" {
    static unsigned long sk_stream_read(FT_Stream       stream,
                                        unsigned long   offset,
                                        unsigned char*  buffer,
                                        unsigned long   count ) {
        SkStream* str = (SkStream*)stream->descriptor.pointer;

        if (count) {
            if (!str->rewind()) {
                return 0;
            } else {
                unsigned long ret;
                if (offset) {
                    ret = str->read(NULL, offset);
                    if (ret != offset) {
                        return 0;
                    }
                }
                ret = str->read(buffer, count);
                if (ret != count) {
                    return 0;
                }
                count = ret;
            }
        }
        return count;
    }

    static void sk_stream_close( FT_Stream stream) {}
}

SkFaceRec::SkFaceRec(SkStream* strm, uint32_t fontID)
        : fSkStream(strm), fFontID(fontID) {
//    SkDEBUGF(("SkFaceRec: opening %s (%p)\n", key.c_str(), strm));

    sk_bzero(&fFTStream, sizeof(fFTStream));
    fFTStream.size = fSkStream->getLength();
    fFTStream.descriptor.pointer = fSkStream;
    fFTStream.read  = sk_stream_read;
    fFTStream.close = sk_stream_close;
}

// Will return 0 on failure
static SkFaceRec* ref_ft_face(uint32_t fontID) {
    SkFaceRec* rec = gFaceRecHead;
    while (rec) {
        if (rec->fFontID == fontID) {
            SkASSERT(rec->fFace);
            rec->fRefCnt += 1;
            return rec;
        }
        rec = rec->fNext;
    }

    SkStream* strm = SkFontHost::OpenStream(fontID);
    if (NULL == strm) {
        SkDEBUGF(("SkFontHost::OpenStream failed opening %x\n", fontID));
        return 0;
    }

    // this passes ownership of strm to the rec
    rec = SkNEW_ARGS(SkFaceRec, (strm, fontID));

    FT_Open_Args    args;
    memset(&args, 0, sizeof(args));
    const void* memoryBase = strm->getMemoryBase();

    if (NULL != memoryBase) {
//printf("mmap(%s)\n", keyString.c_str());
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (const FT_Byte*)memoryBase;
        args.memory_size = strm->getLength();
    } else {
//printf("fopen(%s)\n", keyString.c_str());
        args.flags = FT_OPEN_STREAM;
        args.stream = &rec->fFTStream;
    }

    int face_index;
    int length = SkFontHost::GetFileName(fontID, NULL, 0, &face_index);
    FT_Error err = FT_Open_Face(gFTLibrary, &args, length ? face_index : 0,
                                &rec->fFace);

    if (err) {    // bad filename, try the default font
        fprintf(stderr, "ERROR: unable to open font '%x'\n", fontID);
        SkDELETE(rec);
        return 0;
    } else {
        SkASSERT(rec->fFace);
        //fprintf(stderr, "Opened font '%s'\n", filename.c_str());
        rec->fNext = gFaceRecHead;
        gFaceRecHead = rec;
        rec->fRefCnt = 1;
        return rec;
    }
}

static void unref_ft_face(FT_Face face) {
    SkFaceRec*  rec = gFaceRecHead;
    SkFaceRec*  prev = NULL;
    while (rec) {
        SkFaceRec* next = rec->fNext;
        if (rec->fFace == face) {
            if (--rec->fRefCnt == 0) {
                if (prev) {
                    prev->fNext = next;
                } else {
                    gFaceRecHead = next;
                }
                FT_Done_Face(face);
                SkDELETE(rec);
            }
            return;
        }
        prev = rec;
        rec = next;
    }
    SkASSERT("shouldn't get here, face not in list");
}

///////////////////////////////////////////////////////////////////////////

// Work around for old versions of freetype.
static FT_Error getAdvances(FT_Face face, FT_UInt start, FT_UInt count,
                           FT_Int32 loadFlags, FT_Fixed* advances) {
#ifdef FT_ADVANCES_H
    return FT_Get_Advances(face, start, count, loadFlags, advances);
#else
    if (!face || start >= face->num_glyphs ||
            start + count > face->num_glyphs || loadFlags != FT_LOAD_NO_SCALE) {
        return 6;  // "Invalid argument."
    }
    if (count == 0)
        return 0;

    for (int i = 0; i < count; i++) {
        FT_Error err = FT_Load_Glyph(face, start + i, FT_LOAD_NO_SCALE);
        if (err)
            return err;
        advances[i] = face->glyph->advance.x;
    }

    return 0;
#endif
}

static bool canEmbed(FT_Face face) {
#ifdef FT_FSTYPE_RESTRICTED_LICENSE_EMBEDDING
    FT_UShort fsType = FT_Get_FSType_Flags(face);
    return (fsType & (FT_FSTYPE_RESTRICTED_LICENSE_EMBEDDING |
                      FT_FSTYPE_BITMAP_EMBEDDING_ONLY)) == 0;
#else
    // No embedding is 0x2 and bitmap embedding only is 0x200.
    TT_OS2* os2_table;
    if ((os2_table = (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2)) != NULL) {
        return (os2_table->fsType & 0x202) == 0;
    }
    return false;  // We tried, fail safe.
#endif
}

static bool GetLetterCBox(FT_Face face, char letter, FT_BBox* bbox) {
    const FT_UInt glyph_id = FT_Get_Char_Index(face, letter);
    if (!glyph_id)
        return false;
    FT_Load_Glyph(face, glyph_id, FT_LOAD_NO_SCALE);
    FT_Outline_Get_CBox(&face->glyph->outline, bbox);
    return true;
}

static bool getWidthAdvance(FT_Face face, int gId, int16_t* data) {
    FT_Fixed advance = 0;
    if (getAdvances(face, gId, 1, FT_LOAD_NO_SCALE, &advance)) {
        return false;
    }
    SkASSERT(data);
    *data = advance;
    return true;
}

// static
SkAdvancedTypefaceMetrics* SkFontHost::GetAdvancedTypefaceMetrics(
        uint32_t fontID,
        SkAdvancedTypefaceMetrics::PerGlyphInfo perGlyphInfo) {
#if defined(SK_BUILD_FOR_MAC) || defined(ANDROID)
    return NULL;
#else
    SkAutoMutexAcquire ac(gFTMutex);
    FT_Library libInit = NULL;
    if (gFTCount == 0) {
        if (!InitFreetype())
            sk_throw();
        libInit = gFTLibrary;
    }
    SkAutoTCallIProc<struct FT_LibraryRec_, FT_Done_FreeType> ftLib(libInit);
    SkFaceRec* rec = ref_ft_face(fontID);
    if (NULL == rec)
        return NULL;
    FT_Face face = rec->fFace;

    SkAdvancedTypefaceMetrics* info = new SkAdvancedTypefaceMetrics;
    info->fFontName.set(FT_Get_Postscript_Name(face));
    info->fMultiMaster = FT_HAS_MULTIPLE_MASTERS(face);
    info->fLastGlyphID = face->num_glyphs - 1;
    info->fEmSize = 1000;

    bool cid = false;
    const char* fontType = FT_Get_X11_Font_Format(face);
    if (strcmp(fontType, "Type 1") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kType1_Font;
    } else if (strcmp(fontType, "CID Type 1") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kType1CID_Font;
        cid = true;
    } else if (strcmp(fontType, "CFF") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kCFF_Font;
    } else if (strcmp(fontType, "TrueType") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kTrueType_Font;
        cid = true;
        TT_Header* ttHeader;
        if ((ttHeader = (TT_Header*)FT_Get_Sfnt_Table(face,
                                                      ft_sfnt_head)) != NULL) {
            info->fEmSize = ttHeader->Units_Per_EM;
        }
    }

    info->fStyle = 0;
    if (FT_IS_FIXED_WIDTH(face))
        info->fStyle |= SkAdvancedTypefaceMetrics::kFixedPitch_Style;
    if (face->style_flags & FT_STYLE_FLAG_ITALIC)
        info->fStyle |= SkAdvancedTypefaceMetrics::kItalic_Style;
    // We should set either Symbolic or Nonsymbolic; Nonsymbolic if the font's
    // character set is a subset of 'Adobe standard Latin.'
    info->fStyle |= SkAdvancedTypefaceMetrics::kSymbolic_Style;

    PS_FontInfoRec ps_info;
    TT_Postscript* tt_info;
    if (FT_Get_PS_Font_Info(face, &ps_info) == 0) {
        info->fItalicAngle = ps_info.italic_angle;
    } else if ((tt_info =
                (TT_Postscript*)FT_Get_Sfnt_Table(face,
                                                  ft_sfnt_post)) != NULL) {
        info->fItalicAngle = SkFixedToScalar(tt_info->italicAngle);
    } else {
        info->fItalicAngle = 0;
    }

    info->fAscent = face->ascender;
    info->fDescent = face->descender;

    // Figure out a good guess for StemV - Min width of i, I, !, 1.
    // This probably isn't very good with an italic font.
    int16_t min_width = SHRT_MAX;
    info->fStemV = 0;
    char stem_chars[] = {'i', 'I', '!', '1'};
    for (size_t i = 0; i < SK_ARRAY_COUNT(stem_chars); i++) {
        FT_BBox bbox;
        if (GetLetterCBox(face, stem_chars[i], &bbox)) {
            int16_t width = bbox.xMax - bbox.xMin;
            if (width > 0 && width < min_width) {
                min_width = width;
                info->fStemV = min_width;
            }
        }
    }

    TT_PCLT* pclt_info;
    TT_OS2* os2_table;
    if ((pclt_info = (TT_PCLT*)FT_Get_Sfnt_Table(face, ft_sfnt_pclt)) != NULL) {
        info->fCapHeight = pclt_info->CapHeight;
        uint8_t serif_style = pclt_info->SerifStyle & 0x3F;
        if (serif_style >= 2 && serif_style <= 6)
            info->fStyle |= SkAdvancedTypefaceMetrics::kSerif_Style;
        else if (serif_style >= 9 && serif_style <= 12)
            info->fStyle |= SkAdvancedTypefaceMetrics::kScript_Style;
    } else if ((os2_table =
                (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2)) != NULL) {
        info->fCapHeight = os2_table->sCapHeight;
    } else {
        // Figure out a good guess for CapHeight: average the height of M and X.
        FT_BBox m_bbox, x_bbox;
        bool got_m, got_x;
        got_m = GetLetterCBox(face, 'M', &m_bbox);
        got_x = GetLetterCBox(face, 'X', &x_bbox);
        if (got_m && got_x) {
            info->fCapHeight = (m_bbox.yMax - m_bbox.yMin + x_bbox.yMax -
                    x_bbox.yMin) / 2;
        } else if (got_m && !got_x) {
            info->fCapHeight = m_bbox.yMax - m_bbox.yMin;
        } else if (!got_m && got_x) {
            info->fCapHeight = x_bbox.yMax - x_bbox.yMin;
        }
    }

    info->fBBox = SkIRect::MakeLTRB(face->bbox.xMin, face->bbox.yMax,
                                    face->bbox.xMax, face->bbox.yMin);

    if (!canEmbed(face) || !FT_IS_SCALABLE(face) || 
            info->fType == SkAdvancedTypefaceMetrics::kOther_Font) {
        perGlyphInfo = SkAdvancedTypefaceMetrics::kNo_PerGlyphInfo;
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kHAdvance_PerGlyphInfo) {
        if (FT_IS_FIXED_WIDTH(face)) {
            appendRange(&info->fGlyphWidths, 0);
            int16_t advance = face->max_advance_width;
            info->fGlyphWidths->fAdvance.append(1, &advance);
            finishRange(info->fGlyphWidths.get(), 0,
                        SkAdvancedTypefaceMetrics::WidthRange::kDefault);
        } else if (!cid) {
            appendRange(&info->fGlyphWidths, 0);
            // So as to not blow out the stack, get advances in batches.
            for (int gID = 0; gID < face->num_glyphs; gID += 128) {
                FT_Fixed advances[128];
                int advanceCount = 128;
                if (gID + advanceCount > face->num_glyphs)
                    advanceCount = face->num_glyphs - gID + 1;
                getAdvances(face, gID, advanceCount, FT_LOAD_NO_SCALE,
                            advances);
                for (int i = 0; i < advanceCount; i++) {
                    int16_t advance = advances[gID + i];
                    info->fGlyphWidths->fAdvance.append(1, &advance);
                }
            }
            finishRange(info->fGlyphWidths.get(), face->num_glyphs - 1,
                        SkAdvancedTypefaceMetrics::WidthRange::kRange);
        } else {
            info->fGlyphWidths.reset(
                getAdvanceData(face, face->num_glyphs, &getWidthAdvance));
        }
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kVAdvance_PerGlyphInfo &&
            FT_HAS_VERTICAL(face)) {
        SkASSERT(false);  // Not implemented yet.
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kGlyphNames_PerGlyphInfo &&
            info->fType == SkAdvancedTypefaceMetrics::kType1_Font) {
        // Postscript fonts may contain more than 255 glyphs, so we end up
        // using multiple font descriptions with a glyph ordering.  Record
        // the name of each glyph.
        info->fGlyphNames.reset(
                new SkAutoTArray<SkString>(face->num_glyphs));
        for (int gID = 0; gID < face->num_glyphs; gID++) {
            char glyphName[128];  // PS limit for names is 127 bytes.
            FT_Get_Glyph_Name(face, gID, glyphName, 128);
            info->fGlyphNames->get()[gID].set(glyphName);
        }
    }

    if (!canEmbed(face))
        info->fType = SkAdvancedTypefaceMetrics::kNotEmbeddable_Font;

    unref_ft_face(face);
    return info;
#endif
}
///////////////////////////////////////////////////////////////////////////

void SkFontHost::FilterRec(SkScalerContext::Rec* rec) {
    if (!gLCDSupportValid) {
        InitFreetype();
        FT_Done_FreeType(gFTLibrary);
    }

    if (!gLCDSupport && (rec->isLCD() || SkMask::kLCD16_Format == rec->fMaskFormat)) {
        // If the runtime Freetype library doesn't support LCD mode, we disable
        // it here.
        rec->fMaskFormat = SkMask::kA8_Format;
    }

    SkPaint::Hinting h = rec->getHinting();
    if (SkPaint::kFull_Hinting == h && !rec->isLCD()) {
        // collapse full->normal hinting if we're not doing LCD
        h = SkPaint::kNormal_Hinting;
    } else if ((rec->fFlags & SkScalerContext::kSubpixelPositioning_Flag) &&
               SkPaint::kNo_Hinting != h) {
        // to do subpixel, we must have at most slight hinting
        h = SkPaint::kSlight_Hinting;
    }
    rec->setHinting(h);
}

#ifdef ANDROID
uint32_t SkFontHost::GetUnitsPerEm(SkFontID fontID) {
    SkAutoMutexAcquire ac(gFTMutex);
    SkFaceRec *rec = ref_ft_face(fontID);
    uint16_t unitsPerEm = 0;

    if (rec != NULL && rec->fFace != NULL) {
        unitsPerEm = rec->fFace->units_per_EM;
        unref_ft_face(rec->fFace);
    }

    return (uint32_t)unitsPerEm;
}
#endif

SkScalerContext_FreeType::SkScalerContext_FreeType(const SkDescriptor* desc)
        : SkScalerContext(desc) {
    SkAutoMutexAcquire  ac(gFTMutex);

    if (gFTCount == 0) {
        if (!InitFreetype()) {
            sk_throw();
        }
    }
    ++gFTCount;

    // load the font file
    fFTSize = NULL;
    fFace = NULL;
    fFaceRec = ref_ft_face(fRec.fFontID);
    if (NULL == fFaceRec) {
        return;
    }
    fFace = fFaceRec->fFace;

    // compute our factors from the record

    SkMatrix    m;

    fRec.getSingleMatrix(&m);

#ifdef DUMP_STRIKE_CREATION
    SkString     keyString;
    SkFontHost::GetDescriptorKeyString(desc, &keyString);
    printf("========== strike [%g %g %g] [%g %g %g %g] hints %d format %d %s\n", SkScalarToFloat(fRec.fTextSize),
           SkScalarToFloat(fRec.fPreScaleX), SkScalarToFloat(fRec.fPreSkewX),
           SkScalarToFloat(fRec.fPost2x2[0][0]), SkScalarToFloat(fRec.fPost2x2[0][1]),
           SkScalarToFloat(fRec.fPost2x2[1][0]), SkScalarToFloat(fRec.fPost2x2[1][1]),
           fRec.getHinting(), fRec.fMaskFormat, keyString.c_str());
#endif

    //  now compute our scale factors
    SkScalar    sx = m.getScaleX();
    SkScalar    sy = m.getScaleY();

    if (m.getSkewX() || m.getSkewY() || sx < 0 || sy < 0) {
        // sort of give up on hinting
        sx = SkMaxScalar(SkScalarAbs(sx), SkScalarAbs(m.getSkewX()));
        sy = SkMaxScalar(SkScalarAbs(m.getSkewY()), SkScalarAbs(sy));
        sx = sy = SkScalarAve(sx, sy);

        SkScalar inv = SkScalarInvert(sx);

        // flip the skew elements to go from our Y-down system to FreeType's
        fMatrix22.xx = SkScalarToFixed(SkScalarMul(m.getScaleX(), inv));
        fMatrix22.xy = -SkScalarToFixed(SkScalarMul(m.getSkewX(), inv));
        fMatrix22.yx = -SkScalarToFixed(SkScalarMul(m.getSkewY(), inv));
        fMatrix22.yy = SkScalarToFixed(SkScalarMul(m.getScaleY(), inv));
    } else {
        fMatrix22.xx = fMatrix22.yy = SK_Fixed1;
        fMatrix22.xy = fMatrix22.yx = 0;
    }

    fScaleX = SkScalarToFixed(sx);
    fScaleY = SkScalarToFixed(sy);

    // compute the flags we send to Load_Glyph
    {
        FT_Int32 loadFlags = FT_LOAD_DEFAULT;

        if (SkMask::kBW_Format == fRec.fMaskFormat) {
            // See http://code.google.com/p/chromium/issues/detail?id=43252#c24
            loadFlags = FT_LOAD_TARGET_MONO;
            if (fRec.getHinting() == SkPaint::kNo_Hinting)
                loadFlags = FT_LOAD_NO_HINTING;
        } else {
            switch (fRec.getHinting()) {
            case SkPaint::kNo_Hinting:
                loadFlags = FT_LOAD_NO_HINTING;
                break;
            case SkPaint::kSlight_Hinting:
                loadFlags = FT_LOAD_TARGET_LIGHT;  // This implies FORCE_AUTOHINT
                break;
            case SkPaint::kNormal_Hinting:
                if (fRec.fFlags & SkScalerContext::kAutohinting_Flag)
                    loadFlags = FT_LOAD_FORCE_AUTOHINT;
                else
                    loadFlags = FT_LOAD_NO_AUTOHINT;
                break;
            case SkPaint::kFull_Hinting:
                if (fRec.fFlags & SkScalerContext::kAutohinting_Flag) {
                    loadFlags = FT_LOAD_FORCE_AUTOHINT;
                    break;
                }
                loadFlags = FT_LOAD_TARGET_NORMAL;
                if (SkMask::kHorizontalLCD_Format == fRec.fMaskFormat ||
                        SkMask::kLCD16_Format == fRec.fMaskFormat) {
                    loadFlags = FT_LOAD_TARGET_LCD;
                } else if (SkMask::kVerticalLCD_Format == fRec.fMaskFormat) {
                    loadFlags = FT_LOAD_TARGET_LCD_V;
                }
                break;
            default:
                SkDebugf("---------- UNKNOWN hinting %d\n", fRec.getHinting());
                break;
            }
        }

        if ((fRec.fFlags & SkScalerContext::kEmbeddedBitmapText_Flag) == 0)
            loadFlags |= FT_LOAD_NO_BITMAP;

        fLoadGlyphFlags = loadFlags;
    }

    // now create the FT_Size

    {
        FT_Error    err;

        err = FT_New_Size(fFace, &fFTSize);
        if (err != 0) {
            SkDEBUGF(("SkScalerContext_FreeType::FT_New_Size(%x): FT_Set_Char_Size(0x%x, 0x%x) returned 0x%x\n",
                        fFaceRec->fFontID, fScaleX, fScaleY, err));
            fFace = NULL;
            return;
        }

        err = FT_Activate_Size(fFTSize);
        if (err != 0) {
            SkDEBUGF(("SkScalerContext_FreeType::FT_Activate_Size(%x, 0x%x, 0x%x) returned 0x%x\n",
                        fFaceRec->fFontID, fScaleX, fScaleY, err));
            fFTSize = NULL;
        }

        err = FT_Set_Char_Size( fFace,
                                SkFixedToFDot6(fScaleX), SkFixedToFDot6(fScaleY),
                                72, 72);
        if (err != 0) {
            SkDEBUGF(("SkScalerContext_FreeType::FT_Set_Char_Size(%x, 0x%x, 0x%x) returned 0x%x\n",
                        fFaceRec->fFontID, fScaleX, fScaleY, err));
            fFace = NULL;
            return;
        }

        FT_Set_Transform( fFace, &fMatrix22, NULL);
    }
}

SkScalerContext_FreeType::~SkScalerContext_FreeType() {
    if (fFTSize != NULL) {
        FT_Done_Size(fFTSize);
    }

    SkAutoMutexAcquire  ac(gFTMutex);

    if (fFace != NULL) {
        unref_ft_face(fFace);
    }
    if (--gFTCount == 0) {
//        SkDEBUGF(("FT_Done_FreeType\n"));
        FT_Done_FreeType(gFTLibrary);
        SkDEBUGCODE(gFTLibrary = NULL;)
    }
}

/*  We call this before each use of the fFace, since we may be sharing
    this face with other context (at different sizes).
*/
FT_Error SkScalerContext_FreeType::setupSize() {
    /*  In the off-chance that a font has been removed, we want to error out
        right away, so call resolve just to be sure.

        TODO: perhaps we can skip this, by walking the global font cache and
        killing all of the contexts when we know that a given fontID is going
        away...
     */
    if (!SkFontHost::ValidFontID(fRec.fFontID)) {
        return (FT_Error)-1;
    }

    FT_Error    err = FT_Activate_Size(fFTSize);

    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::FT_Activate_Size(%x, 0x%x, 0x%x) returned 0x%x\n",
                    fFaceRec->fFontID, fScaleX, fScaleY, err));
        fFTSize = NULL;
    } else {
        // seems we need to reset this every time (not sure why, but without it
        // I get random italics from some other fFTSize)
        FT_Set_Transform( fFace, &fMatrix22, NULL);
    }
    return err;
}

void SkScalerContext_FreeType::emboldenOutline(FT_Outline* outline) {
    FT_Pos strength;
    strength = FT_MulFix(fFace->units_per_EM, fFace->size->metrics.y_scale)
               / 24;
    FT_Outline_Embolden(outline, strength);
}

unsigned SkScalerContext_FreeType::generateGlyphCount() {
    return fFace->num_glyphs;
}

uint16_t SkScalerContext_FreeType::generateCharToGlyph(SkUnichar uni) {
    return SkToU16(FT_Get_Char_Index( fFace, uni ));
}

SkUnichar SkScalerContext_FreeType::generateGlyphToChar(uint16_t glyph) {
    // iterate through each cmap entry, looking for matching glyph indices
    FT_UInt glyphIndex;
    SkUnichar charCode = FT_Get_First_Char( fFace, &glyphIndex );

    while (glyphIndex != 0) {
        if (glyphIndex == glyph) {
            return charCode;
        }
        charCode = FT_Get_Next_Char( fFace, charCode, &glyphIndex );
    }

    return 0;
}

static FT_Pixel_Mode compute_pixel_mode(SkMask::Format format) {
    switch (format) {
        case SkMask::kHorizontalLCD_Format:
        case SkMask::kVerticalLCD_Format:
            SkASSERT(!"An LCD format should never be passed here");
            return FT_PIXEL_MODE_GRAY;
        case SkMask::kBW_Format:
            return FT_PIXEL_MODE_MONO;
        case SkMask::kA8_Format:
        default:
            return FT_PIXEL_MODE_GRAY;
    }
}

void SkScalerContext_FreeType::generateAdvance(SkGlyph* glyph) {
#ifdef FT_ADVANCES_H
   /* unhinted and light hinted text have linearly scaled advances
    * which are very cheap to compute with some font formats...
    */
    {
        SkAutoMutexAcquire  ac(gFTMutex);

        if (this->setupSize()) {
            glyph->zeroMetrics();
            return;
        }

        FT_Error    error;
        FT_Fixed    advance;

        error = FT_Get_Advance( fFace, glyph->getGlyphID(fBaseGlyphCount),
                                fLoadGlyphFlags | FT_ADVANCE_FLAG_FAST_ONLY,
                                &advance );
        if (0 == error) {
            glyph->fRsbDelta = 0;
            glyph->fLsbDelta = 0;
            glyph->fAdvanceX = advance;  // advance *2/3; //DEBUG
            glyph->fAdvanceY = 0;
            return;
        }
    }
#endif /* FT_ADVANCES_H */
    /* otherwise, we need to load/hint the glyph, which is slower */
    this->generateMetrics(glyph);
    return;
}

void SkScalerContext_FreeType::generateMetrics(SkGlyph* glyph) {
    SkAutoMutexAcquire  ac(gFTMutex);

    glyph->fRsbDelta = 0;
    glyph->fLsbDelta = 0;

    FT_Error    err;

    if (this->setupSize()) {
        goto ERROR;
    }

    err = FT_Load_Glyph( fFace, glyph->getGlyphID(fBaseGlyphCount), fLoadGlyphFlags );
    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::generateMetrics(%x): FT_Load_Glyph(glyph:%d flags:%d) returned 0x%x\n",
                    fFaceRec->fFontID, glyph->getGlyphID(fBaseGlyphCount), fLoadGlyphFlags, err));
    ERROR:
        glyph->zeroMetrics();
        return;
    }

    switch ( fFace->glyph->format ) {
      case FT_GLYPH_FORMAT_OUTLINE:
        FT_BBox bbox;

        if (fRec.fFlags & kEmbolden_Flag) {
            emboldenOutline(&fFace->glyph->outline);
        }
        FT_Outline_Get_CBox(&fFace->glyph->outline, &bbox);

        if (fRec.fFlags & SkScalerContext::kSubpixelPositioning_Flag) {
            int dx = glyph->getSubXFixed() >> 10;
            int dy = glyph->getSubYFixed() >> 10;
            // negate dy since freetype-y-goes-up and skia-y-goes-down
            bbox.xMin += dx;
            bbox.yMin -= dy;
            bbox.xMax += dx;
            bbox.yMax -= dy;
        }

        bbox.xMin &= ~63;
        bbox.yMin &= ~63;
        bbox.xMax  = (bbox.xMax + 63) & ~63;
        bbox.yMax  = (bbox.yMax + 63) & ~63;

        glyph->fWidth   = SkToU16((bbox.xMax - bbox.xMin) >> 6);
        glyph->fHeight  = SkToU16((bbox.yMax - bbox.yMin) >> 6);
        glyph->fTop     = -SkToS16(bbox.yMax >> 6);
        glyph->fLeft    = SkToS16(bbox.xMin >> 6);
        break;

      case FT_GLYPH_FORMAT_BITMAP:
        if (fRec.fFlags & kEmbolden_Flag) {
            FT_GlyphSlot_Own_Bitmap(fFace->glyph);
            FT_Bitmap_Embolden(gFTLibrary, &fFace->glyph->bitmap, kBitmapEmboldenStrength, 0);
        }
        glyph->fWidth   = SkToU16(fFace->glyph->bitmap.width);
        glyph->fHeight  = SkToU16(fFace->glyph->bitmap.rows);
        glyph->fTop     = -SkToS16(fFace->glyph->bitmap_top);
        glyph->fLeft    = SkToS16(fFace->glyph->bitmap_left);
        break;

      default:
        SkASSERT(!"unknown glyph format");
        goto ERROR;
    }

    if ((fRec.fFlags & SkScalerContext::kSubpixelPositioning_Flag) == 0) {
        glyph->fAdvanceX = SkFDot6ToFixed(fFace->glyph->advance.x);
        glyph->fAdvanceY = -SkFDot6ToFixed(fFace->glyph->advance.y);
        if (fRec.fFlags & kDevKernText_Flag) {
            glyph->fRsbDelta = SkToS8(fFace->glyph->rsb_delta);
            glyph->fLsbDelta = SkToS8(fFace->glyph->lsb_delta);
        }
    } else {
        glyph->fAdvanceX = SkFixedMul(fMatrix22.xx, fFace->glyph->linearHoriAdvance);
        glyph->fAdvanceY = -SkFixedMul(fMatrix22.yx, fFace->glyph->linearHoriAdvance);
    }

#ifdef ENABLE_GLYPH_SPEW
    SkDEBUGF(("FT_Set_Char_Size(this:%p sx:%x sy:%x ", this, fScaleX, fScaleY));
    SkDEBUGF(("Metrics(glyph:%d flags:0x%x) w:%d\n", glyph->getGlyphID(fBaseGlyphCount), fLoadGlyphFlags, glyph->fWidth));
#endif
}

#if defined(SK_SUPPORT_LCDTEXT)
namespace skia_freetype_support {
// extern functions from SkFontHost_FreeType_Subpixel
extern void CopyFreetypeBitmapToLCDMask(const SkGlyph& dest, const FT_Bitmap& source);
extern void CopyFreetypeBitmapToVerticalLCDMask(const SkGlyph& dest, const FT_Bitmap& source);
}

using namespace skia_freetype_support;
#endif

static void copyFT2LCD16(const SkGlyph& glyph, const FT_Bitmap& bitmap) {
    SkASSERT(glyph.fWidth * 3 == bitmap.width - 6);
    SkASSERT(glyph.fHeight == bitmap.rows);

    const uint8_t* src = bitmap.buffer + 3;
    uint16_t* dst = reinterpret_cast<uint16_t*>(glyph.fImage);
    size_t dstRB = glyph.rowBytes();
    int width = glyph.fWidth;

    for (int y = 0; y < glyph.fHeight; y++) {
        const uint8_t* triple = src;
        for (int x = 0; x < width; x++) {
            dst[x] = SkPackRGB16(triple[0] >> 3, triple[1] >> 2, triple[2] >> 3);
            triple += 3;
        }
        src += bitmap.pitch;
        dst = (uint16_t*)((char*)dst + dstRB);
    }
}

void SkScalerContext_FreeType::generateImage(const SkGlyph& glyph) {
    SkAutoMutexAcquire  ac(gFTMutex);

    FT_Error    err;

    if (this->setupSize()) {
        goto ERROR;
    }

    err = FT_Load_Glyph( fFace, glyph.getGlyphID(fBaseGlyphCount), fLoadGlyphFlags);
    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::generateImage: FT_Load_Glyph(glyph:%d width:%d height:%d rb:%d flags:%d) returned 0x%x\n",
                    glyph.getGlyphID(fBaseGlyphCount), glyph.fWidth, glyph.fHeight, glyph.rowBytes(), fLoadGlyphFlags, err));
    ERROR:
        memset(glyph.fImage, 0, glyph.rowBytes() * glyph.fHeight);
        return;
    }

    const bool lcdRenderMode = fRec.fMaskFormat == SkMask::kHorizontalLCD_Format ||
                               fRec.fMaskFormat == SkMask::kVerticalLCD_Format;

    switch ( fFace->glyph->format ) {
        case FT_GLYPH_FORMAT_OUTLINE: {
            FT_Outline* outline = &fFace->glyph->outline;
            FT_BBox     bbox;
            FT_Bitmap   target;

            if (fRec.fFlags & kEmbolden_Flag) {
                emboldenOutline(outline);
            }

            int dx = 0, dy = 0;
            if (fRec.fFlags & SkScalerContext::kSubpixelPositioning_Flag) {
                dx = glyph.getSubXFixed() >> 10;
                dy = glyph.getSubYFixed() >> 10;
                // negate dy since freetype-y-goes-up and skia-y-goes-down
                dy = -dy;
            }
            FT_Outline_Get_CBox(outline, &bbox);
            /*
                what we really want to do for subpixel is
                    offset(dx, dy)
                    compute_bounds
                    offset(bbox & !63)
                but that is two calls to offset, so we do the following, which
                achieves the same thing with only one offset call.
            */
            FT_Outline_Translate(outline, dx - ((bbox.xMin + dx) & ~63),
                                          dy - ((bbox.yMin + dy) & ~63));

#if defined(SK_SUPPORT_LCDTEXT)
            if (lcdRenderMode) {
                // FT_Outline_Get_Bitmap cannot render LCD glyphs. In this case
                // we have to call FT_Render_Glyph and memcpy the image out.
                const bool isVertical = fRec.fMaskFormat == SkMask::kVerticalLCD_Format;
                FT_Render_Mode mode = isVertical ? FT_RENDER_MODE_LCD_V : FT_RENDER_MODE_LCD;
                FT_Render_Glyph(fFace->glyph, mode);

                if (isVertical)
                    CopyFreetypeBitmapToVerticalLCDMask(glyph, fFace->glyph->bitmap);
                else
                    CopyFreetypeBitmapToLCDMask(glyph, fFace->glyph->bitmap);

                break;
            }
#endif

            if (SkMask::kLCD16_Format == glyph.fMaskFormat) {
                FT_Render_Glyph(fFace->glyph, FT_RENDER_MODE_LCD);
                copyFT2LCD16(glyph, fFace->glyph->bitmap);
            } else {
                target.width = glyph.fWidth;
                target.rows = glyph.fHeight;
                target.pitch = glyph.rowBytes();
                target.buffer = reinterpret_cast<uint8_t*>(glyph.fImage);
                target.pixel_mode = compute_pixel_mode(
                                                (SkMask::Format)fRec.fMaskFormat);
                target.num_grays = 256;

                memset(glyph.fImage, 0, glyph.rowBytes() * glyph.fHeight);
                FT_Outline_Get_Bitmap(gFTLibrary, outline, &target);
            }
        } break;

        case FT_GLYPH_FORMAT_BITMAP: {
            if (fRec.fFlags & kEmbolden_Flag) {
                FT_GlyphSlot_Own_Bitmap(fFace->glyph);
                FT_Bitmap_Embolden(gFTLibrary, &fFace->glyph->bitmap, kBitmapEmboldenStrength, 0);
            }
            SkASSERT_CONTINUE(glyph.fWidth == fFace->glyph->bitmap.width);
            SkASSERT_CONTINUE(glyph.fHeight == fFace->glyph->bitmap.rows);
            SkASSERT_CONTINUE(glyph.fTop == -fFace->glyph->bitmap_top);
            SkASSERT_CONTINUE(glyph.fLeft == fFace->glyph->bitmap_left);

            const uint8_t*  src = (const uint8_t*)fFace->glyph->bitmap.buffer;
            uint8_t*        dst = (uint8_t*)glyph.fImage;

            if (fFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY ||
                (fFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO &&
                 glyph.fMaskFormat == SkMask::kBW_Format)) {
                unsigned    srcRowBytes = fFace->glyph->bitmap.pitch;
                unsigned    dstRowBytes = glyph.rowBytes();
                unsigned    minRowBytes = SkMin32(srcRowBytes, dstRowBytes);
                unsigned    extraRowBytes = dstRowBytes - minRowBytes;

                for (int y = fFace->glyph->bitmap.rows - 1; y >= 0; --y) {
                    memcpy(dst, src, minRowBytes);
                    memset(dst + minRowBytes, 0, extraRowBytes);
                    src += srcRowBytes;
                    dst += dstRowBytes;
                }
            } else if (fFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO &&
                       (glyph.fMaskFormat == SkMask::kA8_Format ||
                        glyph.fMaskFormat == SkMask::kHorizontalLCD_Format ||
                        glyph.fMaskFormat == SkMask::kVerticalLCD_Format)) {
                for (int y = 0; y < fFace->glyph->bitmap.rows; ++y) {
                    uint8_t byte = 0;
                    int bits = 0;
                    const uint8_t* src_row = src;
                    uint8_t* dst_row = dst;

                    for (int x = 0; x < fFace->glyph->bitmap.width; ++x) {
                        if (!bits) {
                            byte = *src_row++;
                            bits = 8;
                        }

                        *dst_row++ = byte & 0x80 ? 0xff : 0;
                        bits--;
                        byte <<= 1;
                    }

                    src += fFace->glyph->bitmap.pitch;
                    dst += glyph.rowBytes();
                }
            } else {
              SkASSERT(!"unknown glyph bitmap transform needed");
            }

            if (lcdRenderMode)
                glyph.expandA8ToLCD();

        } break;

    default:
        SkASSERT(!"unknown glyph format");
        goto ERROR;
    }
}

///////////////////////////////////////////////////////////////////////////////

#define ft2sk(x)    SkFixedToScalar((x) << 10)

#if FREETYPE_MAJOR >= 2 && FREETYPE_MINOR >= 2
    #define CONST_PARAM const
#else   // older freetype doesn't use const here
    #define CONST_PARAM
#endif

static int move_proc(CONST_PARAM FT_Vector* pt, void* ctx) {
    SkPath* path = (SkPath*)ctx;
    path->close();  // to close the previous contour (if any)
    path->moveTo(ft2sk(pt->x), -ft2sk(pt->y));
    return 0;
}

static int line_proc(CONST_PARAM FT_Vector* pt, void* ctx) {
    SkPath* path = (SkPath*)ctx;
    path->lineTo(ft2sk(pt->x), -ft2sk(pt->y));
    return 0;
}

static int quad_proc(CONST_PARAM FT_Vector* pt0, CONST_PARAM FT_Vector* pt1,
                     void* ctx) {
    SkPath* path = (SkPath*)ctx;
    path->quadTo(ft2sk(pt0->x), -ft2sk(pt0->y), ft2sk(pt1->x), -ft2sk(pt1->y));
    return 0;
}

static int cubic_proc(CONST_PARAM FT_Vector* pt0, CONST_PARAM FT_Vector* pt1,
                      CONST_PARAM FT_Vector* pt2, void* ctx) {
    SkPath* path = (SkPath*)ctx;
    path->cubicTo(ft2sk(pt0->x), -ft2sk(pt0->y), ft2sk(pt1->x),
                  -ft2sk(pt1->y), ft2sk(pt2->x), -ft2sk(pt2->y));
    return 0;
}

void SkScalerContext_FreeType::generatePath(const SkGlyph& glyph,
                                            SkPath* path) {
    SkAutoMutexAcquire  ac(gFTMutex);

    SkASSERT(&glyph && path);

    if (this->setupSize()) {
        path->reset();
        return;
    }

    uint32_t flags = fLoadGlyphFlags;
    flags |= FT_LOAD_NO_BITMAP; // ignore embedded bitmaps so we're sure to get the outline
    flags &= ~FT_LOAD_RENDER;   // don't scan convert (we just want the outline)

    FT_Error err = FT_Load_Glyph( fFace, glyph.getGlyphID(fBaseGlyphCount), flags);

    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::generatePath: FT_Load_Glyph(glyph:%d flags:%d) returned 0x%x\n",
                    glyph.getGlyphID(fBaseGlyphCount), flags, err));
        path->reset();
        return;
    }

    if (fRec.fFlags & kEmbolden_Flag) {
        emboldenOutline(&fFace->glyph->outline);
    }

    FT_Outline_Funcs    funcs;

    funcs.move_to   = move_proc;
    funcs.line_to   = line_proc;
    funcs.conic_to  = quad_proc;
    funcs.cubic_to  = cubic_proc;
    funcs.shift     = 0;
    funcs.delta     = 0;

    err = FT_Outline_Decompose(&fFace->glyph->outline, &funcs, path);

    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::generatePath: FT_Load_Glyph(glyph:%d flags:%d) returned 0x%x\n",
                    glyph.getGlyphID(fBaseGlyphCount), flags, err));
        path->reset();
        return;
    }

    path->close();
}

void SkScalerContext_FreeType::generateFontMetrics(SkPaint::FontMetrics* mx,
                                                   SkPaint::FontMetrics* my) {
    if (NULL == mx && NULL == my) {
        return;
    }

    SkAutoMutexAcquire  ac(gFTMutex);

    if (this->setupSize()) {
        ERROR:
        if (mx) {
            sk_bzero(mx, sizeof(SkPaint::FontMetrics));
        }
        if (my) {
            sk_bzero(my, sizeof(SkPaint::FontMetrics));
        }
        return;
    }

    FT_Face face = fFace;
    int upem = face->units_per_EM;
    if (upem <= 0) {
        goto ERROR;
    }

    SkPoint pts[6];
    SkFixed ys[6];
    SkFixed scaleY = fScaleY;
    SkFixed mxy = fMatrix22.xy;
    SkFixed myy = fMatrix22.yy;
    SkScalar xmin = SkIntToScalar(face->bbox.xMin) / upem;
    SkScalar xmax = SkIntToScalar(face->bbox.xMax) / upem;

    int leading = face->height - (face->ascender + -face->descender);
    if (leading < 0) {
        leading = 0;
    }

    // Try to get the OS/2 table from the font. This contains the specific
    // average font width metrics which Windows uses.
    TT_OS2* os2 = (TT_OS2*) FT_Get_Sfnt_Table(face, ft_sfnt_os2);

    ys[0] = -face->bbox.yMax;
    ys[1] = -face->ascender;
    ys[2] = -face->descender;
    ys[3] = -face->bbox.yMin;
    ys[4] = leading;
    ys[5] = os2 ? os2->xAvgCharWidth : 0;

    SkScalar x_height;
    if (os2 && os2->sxHeight) {
        x_height = SkFixedToScalar(SkMulDiv(fScaleX, os2->sxHeight, upem));
    } else {
        const FT_UInt x_glyph = FT_Get_Char_Index(fFace, 'x');
        if (x_glyph) {
            FT_BBox bbox;
            FT_Load_Glyph(fFace, x_glyph, fLoadGlyphFlags);
            if (fRec.fFlags & kEmbolden_Flag) {
                emboldenOutline(&fFace->glyph->outline);
            }
            FT_Outline_Get_CBox(&fFace->glyph->outline, &bbox);
            x_height = SkIntToScalar(bbox.yMax) / 64;
        } else {
            x_height = 0;
        }
    }

    // convert upem-y values into scalar points
    for (int i = 0; i < 6; i++) {
        SkFixed y = SkMulDiv(scaleY, ys[i], upem);
        SkFixed x = SkFixedMul(mxy, y);
        y = SkFixedMul(myy, y);
        pts[i].set(SkFixedToScalar(x), SkFixedToScalar(y));
    }

    if (mx) {
        mx->fTop = pts[0].fX;
        mx->fAscent = pts[1].fX;
        mx->fDescent = pts[2].fX;
        mx->fBottom = pts[3].fX;
        mx->fLeading = pts[4].fX;
        mx->fAvgCharWidth = pts[5].fX;
        mx->fXMin = xmin;
        mx->fXMax = xmax;
        mx->fXHeight = x_height;
    }
    if (my) {
        my->fTop = pts[0].fY;
        my->fAscent = pts[1].fY;
        my->fDescent = pts[2].fY;
        my->fBottom = pts[3].fY;
        my->fLeading = pts[4].fY;
        my->fAvgCharWidth = pts[5].fY;
        my->fXMin = xmin;
        my->fXMax = xmax;
        my->fXHeight = x_height;
    }
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

SkScalerContext* SkFontHost::CreateScalerContext(const SkDescriptor* desc) {
    SkScalerContext_FreeType* c = SkNEW_ARGS(SkScalerContext_FreeType, (desc));
    if (!c->success()) {
        SkDELETE(c);
        c = NULL;
    }
    return c;
}

///////////////////////////////////////////////////////////////////////////////

/*  Export this so that other parts of our FonttHost port can make use of our
    ability to extract the name+style from a stream, using FreeType's api.
*/
SkTypeface::Style find_name_and_attributes(SkStream* stream, SkString* name,
                                           bool* isFixedWidth) {
    FT_Library  library;
    if (FT_Init_FreeType(&library)) {
        name->reset();
        return SkTypeface::kNormal;
    }

    FT_Open_Args    args;
    memset(&args, 0, sizeof(args));

    const void* memoryBase = stream->getMemoryBase();
    FT_StreamRec    streamRec;

    if (NULL != memoryBase) {
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (const FT_Byte*)memoryBase;
        args.memory_size = stream->getLength();
    } else {
        memset(&streamRec, 0, sizeof(streamRec));
        streamRec.size = stream->read(NULL, 0);
        streamRec.descriptor.pointer = stream;
        streamRec.read  = sk_stream_read;
        streamRec.close = sk_stream_close;

        args.flags = FT_OPEN_STREAM;
        args.stream = &streamRec;
    }

    FT_Face face;
    if (FT_Open_Face(library, &args, 0, &face)) {
        FT_Done_FreeType(library);
        name->reset();
        return SkTypeface::kNormal;
    }

    name->set(face->family_name);
    int style = SkTypeface::kNormal;

    if (face->style_flags & FT_STYLE_FLAG_BOLD) {
        style |= SkTypeface::kBold;
    }
    if (face->style_flags & FT_STYLE_FLAG_ITALIC) {
        style |= SkTypeface::kItalic;
    }
    if (isFixedWidth) {
        *isFixedWidth = FT_IS_FIXED_WIDTH(face);
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return (SkTypeface::Style)style;
}

///////////////////////////////////////////////////////////////////////////////

/** 
 *  skconvert_open_converter()
 *  
 *  Converter open.
 *  
 *  @param  const char*         (IN) encoding
 *  @return UConverter*         converter's pointer / NULL is failure.
 */
static UConverter* skconvert_open_converter(const char* encoding) {
    UErrorCode  error = U_ZERO_ERROR;
    
    // converter open.
    UConverter* converter = ucnv_open(encoding, &error);
    if (U_FAILURE(error)) {
        converter = NULL;
    }
    
    return converter;
}

/** 
 *  skconvert_close_converter()
 *  
 *  Converter close.
 *  
 *  @param  UConverter*         (IN) converter
 *  @return -
 */
static void skconvert_close_converter(UConverter* converter) {
    ucnv_close(converter);
    converter = NULL;
}

/** 
 *  skconvert_from_unicode_length()
 *  
 *  Get the encoding length.
 *  
 *  @param  size_t              (IN) sourceLength
 *  @param  UConverter*         (IN) converter's pointer
 *  @return size_t              targetLength
 */
static size_t skconvert_from_unicode_length(size_t sourceLength, UConverter* converter) {
    return (sourceLength * ucnv_getMaxCharSize(converter));
}

/** 
 *  skconvert_from_unicode()
 *  
 *  Convert from Unicode.
 *  
 *  @param  const UChar*        (IN) set_value
 *  @param  size_t              (IN) sourceLength
 *  @param  char*               (OUT)ret_value
 *  @param  size_t              (IN) targetLength
 *  @param  UConverter*         (IN) converter's pointer
 *  @return bool                true - OK / false - NG
 */
static bool skconvert_from_unicode(const UChar* set_value, size_t sourceLength,
                                   char* ret_value, size_t targetLength, UConverter* converter) {
    bool        ret = false;
    UErrorCode  error = U_ZERO_ERROR;

    // parameter is set.
    char        *target      = ret_value;
    const char  *targetLimit = ret_value + targetLength;
    const UChar *source      = set_value;
    const UChar *sourceLimit = set_value + sourceLength;

    // convert.
    ucnv_fromUnicode(converter, &target, targetLimit, &source, sourceLimit, 0, TRUE, &error);
    if (!U_FAILURE(error)) {
        ret = true;
    }

    return ret;
}

/** 
 *  skconvert_to_unicode_length()
 *  
 *  Get the Unicode length.
 *  
 *  @param  size_t              (IN) sourceLength
 *  @param  UConverter*         (IN) converter's pointer
 *  @return size_t              targetLength
 */
static size_t skconvert_to_unicode_length(size_t sourceLength, UConverter* converter) {
    return (sourceLength / ucnv_getMinCharSize(converter));
}

/** 
 *  skconvert_to_unicode()
 *  
 *  Convert to Unicode.
 *  
 *  @param  const char*         (IN) set_value
 *  @param  size_t              (IN) sourceLength
 *  @param  UChar*              (OUT)ret_value
 *  @param  size_t              (IN) targetLength
 *  @param  UConverter*         (IN) converter's pointer
 *  @return bool                true - OK / false - NG
 */
static bool skconvert_to_unicode(const char* set_value, size_t sourceLength,
                                 UChar* ret_value, size_t targetLength, UConverter* converter) {
    bool        ret = false;
    UErrorCode  error = U_ZERO_ERROR;

    // parameter is set.
    const char  *source      = set_value;
    const char  *sourceLimit = set_value + sourceLength;
    UChar       *target      = ret_value;
    const UChar *targetLimit = ret_value + targetLength;

    // convert.
    ucnv_toUnicode(converter, &target, targetLimit, &source, sourceLimit, 0, TRUE, &error);
    if (!U_FAILURE(error)) {
        ret = true;
    }

    return ret;
}

/** 
 *  skconvert_to_utf8_length()
 *  
 *  Get the UTF-8 length.
 *  
 *  @param  size_t              (IN) sourceLength
 *  @param  UConverter*         (IN) converter's pointer
 *  @return size_t              targetLength / 0 contains the failure of converter opened.
 */
static size_t skconvert_to_utf8_length(size_t sourceLength, UConverter* converter) {
    size_t  bufLength = 0;
    size_t  targetLength = 0;

    /* get the Unicode length. */
    bufLength = skconvert_to_unicode_length(sourceLength, converter);

    /* UTF-8 converter open. */
    UConverter* utf8_converter = skconvert_open_converter(SK_ENCODING_UTF8);

    if (utf8_converter != NULL) {
        /* get the UTF-8 length. */
        targetLength = skconvert_from_unicode_length(bufLength, utf8_converter);
        /* converter close. */
        skconvert_close_converter(utf8_converter);
    }

    return targetLength;
}

/** 
 *  skconvert_to_utf8()
 *  
 *  Convert To UTF-8.
 *  
 *  @param  const char*         (IN) source
 *  @param  size_t              (IN) sourceLength
 *  @param  char*               (OUT)target
 *  @param  size_t              (IN) targetLength
 *  @param  UConverter*         (IN) converter's pointer
 *  @return bool                true - OK / false - NG
 */
static bool skconvert_to_utf8(const char* source, size_t sourceLength,
                              char* target, size_t targetLength, UConverter* converter) {
    bool    ret = false;

    /* get the Unicode length. */
    const size_t    bufLength = skconvert_to_unicode_length(sourceLength, converter);

    UChar   buf[bufLength];
    /* Convert : encoding -> Unicode */
    if (skconvert_to_unicode(source, sourceLength, buf, bufLength, converter)) {
        /* UTF-8 converter open. */
        UConverter* utf8_converter = skconvert_open_converter(SK_ENCODING_UTF8);

        /* Convert : Unicode -> UTF-8 */
        if (utf8_converter != NULL) {
            if (skconvert_from_unicode(buf, bufLength, target, targetLength, utf8_converter)) {
                ret = true;
            }
            /* UTF-8 converter close. */
            skconvert_close_converter(utf8_converter);
        }
    }

    return ret;
}

///////////////////////////////////////////////////////////////////////////////

/* key structure */
struct Namekey {
    FT_UShort   platform_id;
    FT_UShort   encoding_id;
    FT_UShort   language_id;
    FT_UShort   name_id;
};

/* key management structure */
struct KeyManager{
    const char*     langcode;
    const Namekey*  const   keytable;
    size_t          table_size;
};

/* language : "ja" */
static const Namekey gKey_Japanese[] = {
    {TT_PLATFORM_MACINTOSH,     TT_MAC_ID_ROMAN,        TT_MAC_LANGID_JAPANESE,             TT_NAME_ID_FULL_NAME    },
    {TT_PLATFORM_MACINTOSH,     TT_MAC_ID_ROMAN,        TT_MAC_LANGID_JAPANESE,             TT_NAME_ID_FONT_FAMILY  },
    {TT_PLATFORM_MACINTOSH,     TT_MAC_ID_ROMAN,        TT_MAC_LANGID_ENGLISH,              TT_NAME_ID_FULL_NAME    },
    {TT_PLATFORM_MACINTOSH,     TT_MAC_ID_ROMAN,        TT_MAC_LANGID_ENGLISH,              TT_NAME_ID_FONT_FAMILY  }
};

/* language : "en" */
static const Namekey gKey_English[] = {
    {TT_PLATFORM_MACINTOSH,     TT_MAC_ID_ROMAN,        TT_MAC_LANGID_ENGLISH,              TT_NAME_ID_FULL_NAME    },
    {TT_PLATFORM_MACINTOSH,     TT_MAC_ID_ROMAN,        TT_MAC_LANGID_ENGLISH,              TT_NAME_ID_FONT_FAMILY  }
};

/* key management table */
static const KeyManager gKey_manager[] = {
    /* the head is default. */
    { "en",     gKey_English,    SK_ARRAY_COUNT(gKey_English) },
    { "ja",     gKey_Japanese,   SK_ARRAY_COUNT(gKey_Japanese)}
};

///////////////////////////////////////////////////////////////////////////////

/** 
 *  access_name_table()
 *  
 *  Access the names embedded in TrueType and OpenType files.
 *  set string is UTF-8.
 *  
 *  @param  SkString*           (OUT)sfnt name
 *  @param  FT_Face*            (IN) face
 *  @param  const Namekey*      (IN) key
 *  @return -
 */
static void access_name_table(SkString* aname, FT_Face* face, const Namekey* key) {
    /* set is NULL. */
    aname->set(NULL);

    /* retrieve the number of name strings. */
    FT_UInt get_name_count;
    get_name_count = (FT_UInt)FT_Get_Sfnt_Name_Count(*face);

    /* retrieve a string of the font name. */
    FT_SfntName get_name;
    for (FT_UInt idx = 0; idx < get_name_count; idx++) {
        if (FT_Get_Sfnt_Name(*face, idx, &get_name) != 0) {
            continue;
        }

        if ((get_name.platform_id == key->platform_id)
        &&  (get_name.encoding_id == key->encoding_id)
        &&  (get_name.language_id == key->language_id)
        &&  (get_name.name_id     == key->name_id    )
           ) {

            SkString    value;
            value.set((char*)get_name.string, (size_t)get_name.string_len);
            value.prepend('\0');
            size_t      value_len = (size_t)get_name.string_len + 1;

            if (get_name.platform_id == TT_PLATFORM_MACINTOSH) {
                /* Convert : Shift-JIS -> UTF-8 */
                /* converter open. */
                UConverter* converter = skconvert_open_converter(SK_ENCODING_SHIFTJIS);
                if (converter == NULL) {
                    /* failed. */
                    break;
                }

                /* get the targetLength. */
                const size_t    bufLength = skconvert_to_utf8_length(value_len, converter);
                if (bufLength == 0) {
                    /* failed. */
                    skconvert_close_converter(converter);
                    break;
                }

                /* convert. */
                bool    result = false;
                char    buf[bufLength];
                result = skconvert_to_utf8(value.c_str(), value_len, buf, bufLength, converter);

                /* converter close. */
                skconvert_close_converter(converter);

                if (result) {
                    /* success. */
                    aname->set(buf);
                }
                break;
            }
        }
    }

}

/** 
 *  getDisplayName()
 *  
 *  Get the font display name.
 *  
 *  @param  SkString*           (OUT)display name
 *  @param  SkString*           (IN) language
 *  @param  SkString*           (IN) files fullpath
 *  @return bool                true - OK / false - NG
 */
bool SkFontHost::getDisplayName(SkString* dispname, SkString* language, SkString* fullpath) {
    bool    ret = false;

    if ((dispname != NULL) && (fullpath != NULL)) {
        dispname->set(NULL);

        /* the table used is set. */
        const KeyManager* rec = &gKey_manager[0];
        if (language != NULL) {
            for (size_t i = 0; i < SK_ARRAY_COUNT(gKey_manager); i++) {
                if (language->equals(gKey_manager[i].langcode)) {
                    rec = &gKey_manager[i];
                    break;
                }
            }
        }

        FT_Library  library;
        /* library is opened. */
        if (FT_Init_FreeType(&library) == 0) {
            FT_Face face;
            FT_Long face_index = 0;
            /* face is opened. */
            if (!FT_New_Face(library, (const char*)fullpath->c_str(), face_index, &face)) {
                for (size_t i = 0; i < rec->table_size; i++) {
                    /* get name. */
                    access_name_table(dispname, &face, (const Namekey*)((rec->keytable) + i));
                    if (!dispname->isEmpty()) {
                        /* success. */
                        break;
                    }
                }
                ret = true;

                /* face is closed. */
                FT_Done_Face(face);
            }
            /* library is closed. */
            FT_Done_FreeType(library);
        }
    }

    return ret;
}
