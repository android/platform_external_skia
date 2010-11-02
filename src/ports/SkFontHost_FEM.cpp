/* src/ports/SkFontHost_FEM.cpp
**
** Copyright (c) 1989-2010, Bitstream Inc. and others.  All Rights
** Reserved.
**
** THIS SOFTWARE IS PROVIDED BY BITSTREAM INC. "AS IS" AND ANY EXPRESS
** OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DSICLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
** ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXPLEMPLARY OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES, LOSS OF USE, DATA OR PROFITS, OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
** STRICT LIABILITY OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUR OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
** OF SUCH DAMAGE.
*/

#include "SkScalerContext.h"
#include "SkDescriptor.h"
#include "SkFontHost.h"
#include "SkMask.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkThread.h"
#include "SkMatrix.h"

#include <utils/FontEngineManager.h>

/* #define SK_ENABLE_LOG */

#ifdef SK_ENABLE_LOG
/* fprintf, FILE */
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>

static FILE* fplog = NULL;
static int logIndex = 0;

static int fileExist(char *filename)
{
	  struct stat   buffer;
	  return ( stat(filename, &buffer) == 0 );
}

#define SK_STARTLOG fplog = fopen("/data/SKlogplgn.txt", "a");
#define SK_LOG(__s, ...) \
        if ( fileExist("/data/fv.txt") ) { \
		SK_STARTLOG \
		fprintf(fplog, /*"[%d]"*/__s, /*logIndex++,*/ __VA_ARGS__); \
		SK_ENDLOG \
	}
#define SK_SLOG(__s) \
	if ( fileExist("/data/fv.txt") ) { \
		SK_STARTLOG \
		fprintf(fplog, /*"[%d]"*/__s/*, logIndex++*/); \
		SK_ENDLOG \
	}
#define SK_ENDLOG fclose(fplog);
#else
#define SK_STARTLOG
#define SK_LOG(__s, ...)
#define SK_SLOG(__s)
#define SK_ENDLOG
#endif /* SK_ENABLE_LOG */

#ifdef __cplusplus
extern "C" {
#endif
    unsigned long streamRead(void*           stream,
                                 unsigned long   offset,
                                 unsigned char*  buffer,
                                 unsigned long   count )
    {
        SkStream* str = (SkStream*)stream;

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

    void streamClose(void*  stream) {}
#ifdef __cplusplus
}/* end extern "C" */
#endif

class SkStreamRec {
public:
    SkStreamRec*    fNext;
    SkStream*       fSkStream;

    uint8_t*        memoryBase;           /* font file buffer */
    size_t          size;

    char*           pPath;                /* system path to font file */
    size_t          pathSz;               /* font file path length */

    uint32_t        fRefCnt;
    uint32_t        fFontID;

    static SkStreamRec* ref(uint32_t fontID);
    static void unRef(uint32_t fontID);

private:
    /* assumes ownership of the stream, will call unref() when its done */
    SkStreamRec(SkStream* strm, uint32_t fontID)
        : fSkStream(strm), memoryBase(NULL), fFontID(fontID) {
    }

    ~SkStreamRec() {
        if (pPath) {
            free(pPath);
        }

        fSkStream->unref();
    }
};

class SkScalerContextFEM : public SkScalerContext
{
public:
    SkScalerContextFEM(const SkDescriptor* desc, uint32_t fntId, FontScaler * fs);
    virtual ~SkScalerContextFEM();

protected:
    virtual unsigned generateGlyphCount() const;
    virtual uint16_t generateCharToGlyph(SkUnichar uni);
    virtual SkUnichar generateGlyphToChar(uint16_t);
    virtual void generateAdvance(SkGlyph* glyph);
    virtual void generateMetrics(SkGlyph* glyph);
    virtual void generateImage(const SkGlyph& glyph);
    virtual void generatePath(const SkGlyph& glyph, SkPath* path);
    virtual void generateFontMetrics(SkPaint::FontMetrics* mx,
                                     SkPaint::FontMetrics* my);
private:
    FontScaler* pFontScaler;
    uint32_t fontID;
};

static SkMutex       gMutexSkFEM;
static SkStreamRec*  gStreamRecHead = NULL;

/* Returns NULL on failure; a valid instance of SkStreamRec otherwise. */
SkStreamRec* SkStreamRec::ref(uint32_t fontID) {
    SkAutoMutexAcquire  ac(gMutexSkFEM);

    SkStreamRec* rec = gStreamRecHead;
    while (rec) {
        if (rec->fFontID == fontID) {
            rec->fRefCnt += 1;
            break;
        }
        rec = rec->fNext;
    }

    if(NULL == rec) {
        SkStream* strm = SkFontHost::OpenStream(fontID);
        if (NULL == strm) {
            SkDEBUGF(("SkFontHost::OpenStream failed opening %x\n", fontID));
        } else {
            /* this passes ownership of strm to the rec */
            rec = SkNEW_ARGS(SkStreamRec, (strm, fontID));
            rec->memoryBase = (uint8_t*)strm->getMemoryBase();
            if (NULL != rec->memoryBase) {
                SK_SLOG("SkStreamRec::ref, memory based stream\n");
                rec->size = strm->getLength();
            } else {
                SK_SLOG("SkStreamRec::ref, callback based stream\n");
                rec->size = strm->read(NULL, 0);
            }

            char* filePath = NULL;
            size_t filePathSz = 0;
            filePathSz = SkFontHost::GetFileName(fontID, NULL, 0, NULL);
            SK_LOG("SkStreamRec::ref, filePathSz : %d\n", filePathSz);
            if (filePathSz) {
                filePath = (char*)malloc((filePathSz + 1) * sizeof(char));
                SkFontHost::GetFileName(fontID, filePath, filePathSz, NULL);
                filePath[filePathSz] = '\0';
                SK_LOG("SkStreamRec::ref, filePath : %s\n", filePath);
            }
            rec->pPath = filePath;
            rec->pathSz = filePathSz;

            rec->fNext = gStreamRecHead;
            gStreamRecHead = rec;
            rec->fRefCnt = 1;
        }
    }

    return rec;
}

void SkStreamRec::unRef(uint32_t fontID)
{
    SkAutoMutexAcquire  ac(gMutexSkFEM);

    SkStreamRec*  rec = gStreamRecHead;
    SkStreamRec*  prev = NULL;

    while (rec) {
        SkStreamRec* next = rec->fNext;
        if (rec->fFontID == fontID) {
            if (--rec->fRefCnt == 0) {
                if (prev) {
                    prev->fNext = next;
                } else {
                    gStreamRecHead = next;
                }
                SkDELETE(rec);
            }
            return;
        }
        prev = rec;
        rec = next;
    }
    SkASSERT("shouldn't get here, stream not in list");
}

SkScalerContextFEM::SkScalerContextFEM(const SkDescriptor* desc, uint32_t fntId, FontScaler * fs)
    : SkScalerContext(desc), fontID(fntId)
{
    pFontScaler = fs;
}

SkScalerContextFEM::~SkScalerContextFEM()
{
    delete pFontScaler;
    SkStreamRec::unRef(fontID);
}

unsigned SkScalerContextFEM::generateGlyphCount() const
{
    return pFontScaler->getGlyphCount();
}

uint16_t SkScalerContextFEM::generateCharToGlyph(SkUnichar uni)
{
    return pFontScaler->getCharToGlyphID(uni);
}

SkUnichar SkScalerContextFEM::generateGlyphToChar(uint16_t glyphID)
{
    return pFontScaler->getGlyphIDToChar(glyphID);
}

void SkScalerContextFEM::generateAdvance(SkGlyph* glyph)
{
    FEM16Dot16 fracX = 0, fracY = 0;
    GlyphMetrics gm;

    if (fRec.fSubpixelPositioning)
    {
        fracX = glyph->getSubXFixed();
        fracY = glyph->getSubYFixed();
    }/* end if */

    SK_LOG("SkScalerContextFEM::generateAdvance, pFontScaler->getGlyphAdvance for id :%d\n", glyph->getGlyphID(fBaseGlyphCount));

    gm = pFontScaler->getGlyphAdvance(glyph->getGlyphID(fBaseGlyphCount), fracX, fracY);

    glyph->fRsbDelta = (int8_t)gm.rsbDelta;
    glyph->fLsbDelta = (int8_t)gm.lsbDelta;
    glyph->fAdvanceX = (SkFixed)gm.fAdvanceX;
    glyph->fAdvanceY = (SkFixed)gm.fAdvanceY;

    SK_LOG("SkScalerContextFEM::generateAdvance, for id : %d, gm.fAdvanceX : %f, glyph->fAdvanceX :%f\n", glyph->getGlyphID(fBaseGlyphCount), SkFixedToScalar(gm.fAdvanceX), SkFixedToScalar(glyph->fAdvanceX));
}

void SkScalerContextFEM::generateMetrics(SkGlyph* glyph)
{
    FEM16Dot16 fracX = 0, fracY = 0;
    GlyphMetrics gm;

    if (fRec.fSubpixelPositioning)
    {
        fracX = glyph->getSubXFixed();
        fracY = glyph->getSubYFixed();
    }/* end if */

    SK_LOG("SkScalerContextFEM::generateMetrics, pFontScaler->getGlyphMetrics for id :%d\n",
    glyph->getGlyphID(fBaseGlyphCount));

    gm = pFontScaler->getGlyphMetrics(glyph->getGlyphID(fBaseGlyphCount), fracX, fracY);

    glyph->fWidth   = (uint16_t)gm.width;
    glyph->fHeight  = (uint16_t)gm.height;
    glyph->fTop     = SkToS16(gm.top >> 6);
    glyph->fLeft    = SkToS16(gm.left >> 6);

    glyph->fRsbDelta = (int8_t)gm.rsbDelta;
    glyph->fLsbDelta = (int8_t)gm.lsbDelta;
    glyph->fAdvanceX = (SkFixed)gm.fAdvanceX;
    glyph->fAdvanceY = (SkFixed)gm.fAdvanceY;

    SK_LOG("SkScalerContextFEM::generateMetrics, for id : %d, gm.fAdvanceX : %f, glyph->fAdvanceX :%f\n", glyph->getGlyphID(fBaseGlyphCount), SkFixedToScalar(gm.fAdvanceX), SkFixedToScalar(glyph->fAdvanceX));
}

void SkScalerContextFEM::generateImage(const SkGlyph& glyph)
{
    FEM16Dot16 fracX = 0, fracY = 0;

    if (fRec.fSubpixelPositioning)
    {
        fracX = glyph.getSubXFixed();
        fracY = glyph.getSubYFixed();
    }/* end if */

    SK_LOG("SkScalerContextFEM::generateImage, glyph : %d, fracX : %d, fracY : %d, width : %d height : %d rowBytes : %d\n", (uint16_t)glyph.getGlyphID(fBaseGlyphCount), fracX >> 16, fracY >> 16, glyph.fWidth, glyph.fHeight, (uint32_t)glyph.rowBytes());
    pFontScaler->getGlyphImage((uint16_t)glyph.getGlyphID(fBaseGlyphCount), fracX, fracY, (uint32_t)glyph.rowBytes(), glyph.fWidth, glyph.fHeight, reinterpret_cast<uint8_t*>(glyph.fImage));
}

void SkScalerContextFEM::generatePath(const SkGlyph& glyph, SkPath* path)
{
    FEM16Dot16 fracX = 0, fracY = 0;
    GlyphOutline* go = NULL;
    int i = 0;
    FEM26Dot6* x;
    FEM26Dot6* y;
    int16_t* contours;
    uint8_t* onCurve;
    uint8_t*  flags;  /* the points flags */
    FEM26Dot6 startX, startY;
    int16_t contourCount;  /* number of contours in glyph */

    if (fRec.fSubpixelPositioning)
    {
        fracX = glyph.getSubXFixed();
        fracY = glyph.getSubYFixed();
    }/* end if */

    go = pFontScaler->getGlyphOutline(glyph.getGlyphID(fBaseGlyphCount), fracX, fracY);

    x = go->x;
    y = go->y;
    contours = go->contours;
    flags = go->flags;
    contourCount = go->contourCount;

    /* convert the outline to a path */
    for (int j = 0; j < contourCount; ++j) {
    int last_point = contours[j];
    FEM26Dot6 cX[4], cY[4];
    int n = 1;

        startX = x[i];
        startY = -y[i];

        if(! (flags[i] & 1) ) {
            startX = (startX + x[last_point]) >> 1;
            startY = (startY + (-y[last_point]) ) >> 1;
        }

        /* Reach the first point */
        path->moveTo(SkFixedToScalar(startX << 10), SkFixedToScalar(startY << 10));

        cX[0] = startX;
        cY[0] = startY;
        while (i < last_point) {
            ++i;
            cX[n] = x[i];
            cY[n] = -y[i];
            n++;

            switch (flags[i] & 3) {
            case 2:
                /* cubic bezier element */
                if (n < 4)
                    continue;

                cX[3] = (cX[3] + cX[2])/2;
                cY[3] = (cY[3] + cY[2])/2;

                --i;
                break;
            case 0:
                /* quadratic bezier element */
                if (n < 3)
                    continue;

                cX[3] = (cX[1] + cX[2])/2;
                cY[3] = (cY[1] + cY[2])/2;

                cX[2] = (2*cX[1] + cX[3])/3;
                cY[2] = (2*cY[1] + cY[3])/3;

                cX[1] = (2*cX[1] + cX[0])/3;
                cY[1] = (2*cY[1] + cY[0])/3;

                --i;
                break;
            case 1:
            case 3:
                if (n == 2) {
                    path->lineTo(SkFixedToScalar(cX[1] << 10), SkFixedToScalar(cY[1] << 10));

                    cX[0] = cX[1];
                    cY[0] = cY[1];

                    n = 1;
                    continue;
                } else if (n == 3) {
                    cX[3] = cX[2];
                    cY[3] = cY[2];

                    cX[2] = (2*cX[1] + cX[3])/3;
                    cY[2] = (2*cY[1] + cY[3])/3;

                    cX[1] = (2*cX[1] + cX[0])/3;
                    cX[1] = (2*cY[1] + cY[0])/3;
                }/* end else if*/
                break;
            }/* end switch */
            path->cubicTo(SkFixedToScalar(cX[1] << 10), SkFixedToScalar(cY[1] << 10), SkFixedToScalar(cX[2] << 10), SkFixedToScalar(cY[2] << 10), SkFixedToScalar(cX[3] << 10), SkFixedToScalar(cY[3] << 10));
            cX[0] = cX[3];
            cY[0] = cY[3];
            n = 1;
        }/* end while */

        if (n == 1) {
            path->close();
        } else {
            cX[3] = startX;
            cY[3] = startY;
            if (n == 2) {
                cX[2] = (2*cX[1] + cX[3])/3;
                cY[2] = (2*cY[1] + cY[3])/3;

                cX[1] = (2*cX[1] + cX[0])/3;
                cY[1] = (2*cY[1] + cY[0])/3;
            }/* end if */
            path->cubicTo(SkFixedToScalar(cX[1] << 10), SkFixedToScalar(cY[1] << 10), SkFixedToScalar(cX[2] << 10), SkFixedToScalar(cY[2] << 10), SkFixedToScalar(cX[3] << 10), SkFixedToScalar(cY[3] << 10));
        }
        ++i;
    }/* end for */

    if (go)
        delete go;
}

void SkScalerContextFEM::generateFontMetrics(SkPaint::FontMetrics* mx,
                                             SkPaint::FontMetrics* my)
{
    FontMetrics fmX, fmY;

    FEM16Dot16 fracX = 0, fracY = 0;

    if (mx || my) {
        if (mx && my)
            pFontScaler->getFontMetrics(&fmX, &fmY);
        else if(mx)
            pFontScaler->getFontMetrics(&fmX, NULL);
        else
            pFontScaler->getFontMetrics(NULL, &fmY);

        if (mx) {
            mx->fTop = SkFixedToScalar(fmX.fTop);
            mx->fAscent = SkFixedToScalar(fmX.fAscent);
            mx->fDescent = SkFixedToScalar(fmX.fDescent);
            mx->fBottom = SkFixedToScalar(fmX.fBottom);
            mx->fLeading = SkFixedToScalar(fmX.fLeading);

            mx->fAvgCharWidth = SkFixedToScalar(fmX.fAvgCharWidth);
            mx->fXMin = SkFixedToScalar(fmX.fXMin);
            mx->fXMax = SkFixedToScalar(fmX.fXMax);
            mx->fXHeight = SkFixedToScalar(fmX.fXHeight);
        }

        if (my) {
            my->fTop = SkFixedToScalar(fmY.fTop);
            my->fAscent = SkFixedToScalar(fmY.fAscent);
            my->fDescent = SkFixedToScalar(fmY.fDescent);
            my->fBottom = SkFixedToScalar(fmY.fBottom);
            my->fLeading = SkFixedToScalar(fmY.fLeading);

            my->fAvgCharWidth = SkFixedToScalar(fmY.fAvgCharWidth);
            my->fXMin = SkFixedToScalar(fmY.fXMin);
            my->fXMax = SkFixedToScalar(fmY.fXMax);
            my->fXHeight = SkFixedToScalar(fmY.fXHeight);
        }
    }
}

bool find_name_and_style(SkStream* stream, SkString* name, SkTypeface::Style* style)
{
    bool flag = false;
    FontStyle fontStyle = STYLE_NORMAL;
    size_t fontNameLength = 0;

    const void* buffer = stream->getMemoryBase();
    size_t bufferLength = stream->getLength();

    SK_LOG("find_name_and_style, bufferLength : %d\n", bufferLength);

    fontNameLength = FontEngineManager::getInstance().getFontNameAndStyle(buffer, bufferLength, NULL, 0, NULL);
    SK_LOG("find_name_and_style : fontNameLength %d\n", fontNameLength);
    if (fontNameLength) {
        char* fontName = (char*)malloc(fontNameLength * sizeof(char));
        if (fontName) {
            fontNameLength = FontEngineManager::getInstance().getFontNameAndStyle(buffer, bufferLength, fontName, fontNameLength, &fontStyle);
            if (fontNameLength) {
                SK_LOG("find_name_and_style : fontName %s\n", fontName);

                name->set(fontName, fontNameLength);
                SK_LOG("find_name_and_style : name %s\n", name->c_str());

                free(fontName);

                int tempStyle = SkTypeface::kNormal;
                if (fontStyle & STYLE_BOLD) {
                    tempStyle |= SkTypeface::kBold;
                }

                if (fontStyle & STYLE_ITALIC) {
                    tempStyle |= SkTypeface::kItalic;
                }

                *style = (SkTypeface::Style)tempStyle;

                flag = true; /* operation successful */
            }
        }
    }

    SK_SLOG("find_name_and_style, returning...\n");
    return flag;
}/* end method find_name_and_style */


SkScalerContext* SkFontHost::CreateScalerContext(const SkDescriptor* desc)
{
    FontScalerInfo fsInfo;
    FontScaler* fs = NULL;
    SkScalerContext* ctx = NULL;

    SK_SLOG("SkFontHost::CreateScalerContext\n");

    const SkScalerContext::Rec* fRec = (const SkScalerContext::Rec*)desc->findEntry(kRec_SkDescriptorTag, NULL);

    if (fRec) {
    SkMatrix m;

    /* load the font file */
    SkStreamRec* fStreamRec = SkStreamRec::ref(fRec->fFontID);

        if (fStreamRec) {
            fsInfo.fontID = fRec->fFontID;

            fsInfo.pPath = fStreamRec->pPath;
            fsInfo.pathSz = fStreamRec->pathSz;

            fsInfo.pBuffer = fStreamRec->memoryBase;
            fsInfo.size = fStreamRec->size;

            fsInfo.subpixelPositioning = fRec->fSubpixelPositioning;

            if(SkMask::kBW_Format == fRec->fMaskFormat) {
                fsInfo.maskFormat = ALIAS_MONOCHROME;
            } else if (SkMask::kA8_Format == fRec->fMaskFormat) {
                fsInfo.maskFormat = ALIAS_GRAYSCALE;
            } else if (SkMask::kHorizontalLCD_Format == fRec->fMaskFormat) {
                fsInfo.maskFormat = ALIAS_LCD_H;
            } else if (SkMask::kVerticalLCD_Format == fRec->fMaskFormat) {
                fsInfo.maskFormat = ALIAS_LCD_V;
            }

            fRec->getSingleMatrix(&m);
            fsInfo.fScaleX = SkScalarToFixed(m.getScaleX());
            fsInfo.fScaleY = SkScalarToFixed(m.getScaleY());
            fsInfo.fSkewX = SkScalarToFixed(m.getSkewX());
            fsInfo.fSkewY = SkScalarToFixed(m.getSkewY());

            if (fRec->fFlags & SkScalerContext::kEmbolden_Flag) {
                fsInfo.flags |= FEM_Embolden_Flag;
            }

            if (fRec->fFlags & SkScalerContext::kEmbeddedBitmapText_Flag) {
                fsInfo.flags |= FEM_EmbeddedBitmapText_Flag;
            }

            SkPaint::Hinting h = fRec->getHinting();
            if (h) {
                fsInfo.flags |=  ((h << 1) & FEM_Hinting_Flag);
            }

            if (fRec->fFlags & SkScalerContext::kDevKernText_Flag) {
                fsInfo.flags |= FEM_DevKernText_Flag;
            }

            fs = FontEngineManager::getInstance().createFontScalerContext(fsInfo);
            if (fs) {
                SK_SLOG("SkFontHost::CreateScalerContext, font scaler instance created\n");

                /* passing 'fFontID' as to unref it when we are done with scaler context */
                ctx = new SkScalerContextFEM(desc, fStreamRec->fFontID, fs);

                SK_SLOG("SkFontHost::CreateScalerContext, returning SkScalerContextFEM instance\n");
            } else {
              SK_SLOG("SkFontHost::CreateScalerContext, failed to create font scaler instance\n");
              SkStreamRec::unRef(fStreamRec->fFontID);
            }
        }
    }

    return ctx;
}/* end method CreateScalerContext */

uint32_t SkFontHost::GetUnitsPerEm(SkFontID fontID)
{
    uint32_t unitsPerEm = 0;
    SkStream* stream = SkFontHost::OpenStream(fontID);

    if (stream) {
        const void* buffer = stream->getMemoryBase();
        size_t bufferLength = stream->getLength();

        if (buffer && bufferLength) {
            unitsPerEm = FontEngineManager::getInstance().getFontUnitsPerEm(buffer, bufferLength);
        }/* end if */

        stream->unref();
    }

    return unitsPerEm;
}/* end method GetUnitsPerEm */

/* TODO: For now this method is not implemented. This can be implemented from
   the FontManager in two ways
   1/ Use the least common properties of all the font engines in the font manager and
      set rec according to that. May not be effective.
   2/ Use the rec of the current active scaler, this can work good, except the
      case where the font in focus is incorrect.
*/
void SkFontHost::FilterRec(SkScalerContext::Rec* rec)
{
}/* end method FilterRec */

