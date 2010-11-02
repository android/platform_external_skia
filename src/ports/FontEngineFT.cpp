/* fontengines/freetype/FontEngineFT.cpp
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
**
** This file uses much of the code from SkFontHost_FreeType.cpp
*/
#include <assert.h>
#include <utils/threads.h>
#include <utils/FontEngineManager.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_SIZES_H
#include FT_TRUETYPE_TABLES_H

#if defined(SUPPORT_LCDTEXT)
#include FT_LCD_FILTER_H
#endif

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
#endif

//#define ENABLE_GLYPH_SPEW     // for tracing calls

/* If the following macro is enabled; then list of font instance will be
   maintained by every font object. While creating an font instance this list
   will be searched. If similar font instance found while searching than that
   instance will be retured; a new instance will be created and returned
   otherwise.
*/
#define ENABLE_FONTINSTLIST

//#define FT_ENABLE_LOG

#ifdef FT_ENABLE_LOG
// fprintf, FILE
#include <stdio.h>

static FILE * fplog = NULL;
static int logIndex = 0;

#define FT_STARTLOG fplog = fopen("/data/FTlogplgn.txt", "a");
#define FT_LOG(__s, ...) \
        FT_STARTLOG \
        fprintf(fplog, /*"[%d]"*/__s, /*logIndex++,*/ __VA_ARGS__); \
        FT_ENDLOG
#define FT_SLOG(__s) \
        FT_STARTLOG \
        fprintf(fplog, /*"[%d]"*/__s/*, logIndex++*/); \
        FT_ENDLOG
#define FT_ENDLOG fclose(fplog);
#else
#define FT_STARTLOG
#define FT_LOG(__s, ...)
#define FT_SLOG(__s)
#define FT_ENDLOG
#endif /* FT_ENABLE_LOG */

#ifdef FT_ENABLE_LOG
    #define FT_ASSERT_CONTINUE(pred)                                                      \
        do {                                                                              \
            if (!(pred))                                                                  \
                FT_LOG("file %s:%d: assert failed '" #pred "'\n", __FILE__, __LINE__);    \
        } while (false)
#else
    #define FT_ASSERT_CONTINUE(pred)
#endif

////////////////////////////////////////////////////////////////////////

/* Following are adopted from Skia */

#define FEM16Dot16ToFEM26Dot6(x)   ((x) >> 10)
#define FEM26Dot6ToFEM16Dot16(x)   ((x) << 10)

#define FEM16Dot16Avg(a, b)    (((a) + (b)) >> 1)

/** Returns -1 if n < 0, else returns 0
*/
#define FEM16Dot16ExtractSign(n)    ((int32_t)(n) >> 31)

#define FEM16Dot16Invert(n)         DivBits(FEMOne16Dot16, n, 16)

#define MaxS32   0x7FFFFFFF

#if defined(__arm__) && !defined(__thumb__)
    #define CLZ(x)    __builtin_clz(x)
#endif

#ifndef CLZ
    #define CLZ(x)    CLZ_portable(x)
#endif

static int CLZ_portable(uint32_t x) {
    if (x == 0) {
        return 32;
    }

    int zeros = ((x >> 16) - 1) >> 31 << 4;
    x <<= zeros;

    int nonzero = ((x >> 24) - 1) >> 31 << 3;
    zeros += nonzero;
    x <<= nonzero;

    nonzero = ((x >> 28) - 1) >> 31 << 2;
    zeros += nonzero;
    x <<= nonzero;

    nonzero = ((x >> 30) - 1) >> 31 << 1;
    zeros += nonzero;
    x <<= nonzero;

    zeros += (~x) >> 31;

    return zeros;
}

static int32_t FEM16Dot16Abs(int32_t value);

/** If sign == -1, returns -n, else sign must be 0, and returns n.
    Typically used in conjunction with FEM16Dot16ExtractSign().
*/
static int32_t FEM16Dot16ApplySign(int32_t n, int32_t sign);

/** Computes (numer1 << shift) / denom in full 64 intermediate precision.
    It is an error for denom to be 0. There is no special handling if
    the result overflows 32bits.
*/
static int32_t DivBits(int32_t numer, int32_t denom, int shift);

int32_t FEM16Dot16Abs(int32_t value)
{
    int32_t  mask = value >> 31;
    return (value ^ mask) - mask;
}

int32_t FEM16Dot16ApplySign(int32_t n, int32_t sign)
{
    assert(sign == 0 || sign == -1);
    return (n ^ sign) - sign;
}


#define DIVBITS_ITER(n)                                 \
    case n:                                             \
        if ((numer = (numer << 1) - denom) >= 0)        \
            result |= 1 << (n - 1); else numer += denom

int32_t DivBits(int32_t numer, int32_t denom, int shift_bias) {
    assert(denom != 0);
    if (numer == 0) {
        return 0;
    }

    // make numer and denom positive, and sign hold the resulting sign
    int32_t sign = FEM16Dot16ExtractSign(numer ^ denom);
    numer = FEM16Dot16Abs(numer);
    denom = FEM16Dot16Abs(denom);

    int nbits = CLZ(numer) - 1;
    int dbits = CLZ(denom) - 1;
    int bits = shift_bias - nbits + dbits;

    if (bits < 0) {  // answer will underflow
        return 0;
    }
    if (bits > 31) {  // answer will overflow
        return FEM16Dot16ApplySign(MaxS32, sign);
    }

    denom <<= dbits;
    numer <<= nbits;

    FEM16Dot16 result = 0;

    // do the first one
    if ((numer -= denom) >= 0) {
        result = 1;
    } else {
        numer += denom;
    }

    // Now fall into our switch statement if there are more bits to compute
    if (bits > 0) {
        // make room for the rest of the answer bits
        result <<= bits;
        switch (bits) {
            DIVBITS_ITER(31); DIVBITS_ITER(30); DIVBITS_ITER(29);
            DIVBITS_ITER(28); DIVBITS_ITER(27); DIVBITS_ITER(26);
            DIVBITS_ITER(25); DIVBITS_ITER(24); DIVBITS_ITER(23);
            DIVBITS_ITER(22); DIVBITS_ITER(21); DIVBITS_ITER(20);
            DIVBITS_ITER(19); DIVBITS_ITER(18); DIVBITS_ITER(17);
            DIVBITS_ITER(16); DIVBITS_ITER(15); DIVBITS_ITER(14);
            DIVBITS_ITER(13); DIVBITS_ITER(12); DIVBITS_ITER(11);
            DIVBITS_ITER(10); DIVBITS_ITER( 9); DIVBITS_ITER( 8);
            DIVBITS_ITER( 7); DIVBITS_ITER( 6); DIVBITS_ITER( 5);
            DIVBITS_ITER( 4); DIVBITS_ITER( 3); DIVBITS_ITER( 2);
            // we merge these last two together, makes GCC make better ARM
            default:
            DIVBITS_ITER( 1);
        }
    }

    if (result < 0) {
        result = MaxS32;
    }
    return FEM16Dot16ApplySign(result, sign);
}
////////////////////////////////////////////////////////////////////////

#ifdef FT_ENABLE_LOG
#define FTEnginePrintList FT_PrintList((BasicNodePtr) gFontEngineInstFT->getList())
#else /* ! FT_ENABLE_LOG */
#define FTEnginePrintList
#endif /* ! FT_ENABLE_LOG */

#define FontFTAddAtHead(__l, __r) FT_AddAtHead((BasicNodePtr*)__l, (BasicNodePtr)__r)

//////////////////////////////////////////////////////////////////////////

class FontEngineFT;
class FontFT;
class FontInstFT;
class FontScalerFT;

static android::Mutex  gMutexFT;
static int             gCountFontFT;
static FT_Library      gLibraryFT;
static bool            gLCDSupportValid;  /* true iff |gLCDSupport| has been set. */
static bool            gLCDSupport;  /* true iff LCD is supported by the runtime. */

static FontEngineFT*   gFontEngineInstFT = NULL;  /* global plugin engine instance */

typedef struct BasicNode_t  BasicNode;
typedef BasicNode*          BasicNodePtr;

struct BasicNode_t
{
    BasicNodePtr  next;
};

typedef struct FontNode_t  FontNode;
typedef FontNode*          FontNodePtr;

/* font list */
struct FontNode_t
{
    FontNodePtr  next;
    FontFT*      font;
};/* end struct FontNode_t */

class FontEngineFT : public FontEngine
{
public:
    FontEngineFT()
        : name("freetype"), pFontList(NULL)
    {
        FT_LOG("FontEngineFT::FontEngineFT(), %s engine created\n", name);
    }

    ~FontEngineFT() {}

    /* Return Font engine name */
    const char* getName() const { return name; }

    /* Return font engine capabilities */
    EngineCapability getCapabilities(FontScalerInfo& desc) const;

    /* Given system path to font file; return font's name and style */
    size_t getFontNameAndStyle(const char path[], char name[], size_t length, FontStyle* style);

    /* Given buffer to font file and buffer length; return font's name and style */
    size_t getFontNameAndStyle(const void* buffer, const uint32_t bufferLength, char name[], size_t length, FontStyle*  style);

    /* Given system path to font file; return 'true' if the font format
     * supported; 'false' otherwise.
     */
    bool isFontSupported(const char path[], bool isLoad);

    /* Given a buffer to font file; return 'true' if the font format supported;
     * 'false' otherwise.
     */
    bool isFontSupported(const void* buffer, const uint32_t bufferLength);

    /* Create and return font scaler */
    FontScaler* createFontScalerContext(const FontScalerInfo& desc);

    /** Given system path of the font file; returns the number of font units
       per em.
       @param path    The system path to font file.
       @return the number of font units per em or 0 on error.
    */
    uint32_t getFontUnitsPerEm(const char path[]);

    /** Given font data in buffer; returns the number of font units per em.
        @param buffer          The font file buffer.
        @param bufferLength    Length of the buffer.
        @return the number of font units per em or 0 on error.
    */
    uint32_t getFontUnitsPerEm(const void* buffer, const uint32_t bufferLength);

private:
    FontScaler* getFontScaler(const FontScalerInfo& desc);
    FontFT* getFont(const FontScalerInfo& desc);
    FontNodePtr getList() { return this->pFontList; }

    const char* name;
    FontNodePtr  pFontList;

    /* Array of supported font formats. */
    static const char* const formats[];

    friend class FontFT;
    friend class FontScalerFT;
};/* end class FontEngineFT */

const char* const FontEngineFT::formats[] = { "ttf", NULL };

#ifdef ENABLE_FONTINSTLIST
typedef struct FontInstNode_t  FontInstNode;
typedef FontInstNode*          FontInstNodePtr;

/* font instances list */
struct FontInstNode_t
{
    FontInstNodePtr  next;
    FontInstFT*      inst;
};/* end struct FontInstNode_t */
#endif /* ENABLE_FONTINSTLIST */

class FontFT
{
public:
    FontFT(const FontScalerInfo& desc);
    ~FontFT();

    FontScaler* getFontScaler(const FontScalerInfo& desc);
    bool success() { return bInitialized; }

private:
    FontFT(FontFT&);
    FontFT& operator = (FontFT&);

#ifdef ENABLE_FONTINSTLIST
    FontInstNodePtr searchFontInst(const FontScalerInfo& desc);
    FontInstNodePtr getList() { return this->pFontInstList; }
#endif /* ENABLE_FONTINSTLIST */

    void getTransMatrix(const FontScalerInfo& desc, FT_Matrix& ftMatrix22, FEM16Dot16& fScaleX, FEM16Dot16& fScaleY, uint32_t& loadGlyphFlags);

    FT_StreamRec      streamRecFT;
    FT_Face           pFace;   /* we own this */

    uint32_t          fontID;
    const char*       pPath;   /* we own this */
    const uint8_t*    pBuffer; /* font file buffer */

    bool              bInitialized;
    uint16_t          refCnt;

#ifdef ENABLE_FONTINSTLIST
    FontInstNodePtr   pFontInstList;
#endif /* ENABLE_FONTINSTLIST */

    friend class FontScalerFT;
    friend class FontEngineFT;
    friend class FontInstFT;
};

class FontInstFT
{
public :
    FontInstFT(const FontScalerInfo& desc, FontFT* font);
    ~FontInstFT();

    FT_Error setupSize();

    bool success() { return bInitialized; }

private:
    bool       kerning;
    bool       subpixelPositioning;
    FEM16Dot16   fScaleX, fScaleY;
    FT_Matrix  ftMatrix22;
    FT_Size    ftSize;
    uint32_t   loadGlyphFlags;
    AliasMode  maskFormat;           /* mono, gray, lcd */

    FontFT*    pFont;
    uint16_t   refCnt;

    bool       bInitialized;

    friend class FontFT;
    friend class FontScalerFT;
};/* end class FontInstFT */

class FontScalerFT : public FontScaler
{
public:
    FontScalerFT(FontInstFT* fontInst);
    ~FontScalerFT();

    bool success() { return bInitialized; }

    uint32_t getGlyphCount() const;
    uint16_t getCharToGlyphID(int32_t charUniCode);
    int32_t getGlyphIDToChar(uint16_t glyphID);
    GlyphMetrics getGlyphAdvance(uint16_t glyphID, FEM16Dot16 fracX, FEM16Dot16 fracY);
    GlyphMetrics getGlyphMetrics(uint16_t glyphID, FEM16Dot16 fracX, FEM16Dot16 fracY);
    void getGlyphImage(uint16_t glyphID, FEM16Dot16 fracX, FEM16Dot16 fracY, uint32_t rowBytes, uint16_t width, uint16_t height, uint8_t *buffer);
    void getFontMetrics(FontMetrics* mX, FontMetrics* mY);
    GlyphOutline* getGlyphOutline(uint16_t glyphID, FEM16Dot16 fracX, FEM16Dot16 fracY);

private:
    void emboldenOutline(FT_Outline* outline);

    FontInstFT*  pFontInst;
    FT_Face      ftFace;        /* convinence pointer */

    bool         bInitialized;

    friend class FontFT;
    friend class FontInstFT;
};/* end class FontScalerFT */

/**
 * Global Methods.
 */
#ifdef FT_ENABLE_LOG
static void FT_PrintList(BasicNodePtr head)
{
    while (head != NULL) {
        FT_LOG("%x->", head);
        head = head->next;
    }/* end while */

    FT_SLOG("NULL\n");
}/* end method FT_PrintList */
#endif /* FT_ENABLE_LOG */

static void FT_AddAtHead(BasicNodePtr *list,  BasicNodePtr r)
{
    r->next = *list;
    *list = r;
}/* end method FT_AddAtHead */

static bool InitFreetype()
{
    FT_Error err = FT_Init_FreeType(&gLibraryFT);
    if (err)
        return false;

#if defined(SUPPORT_LCDTEXT)
    /* Setup LCD filtering. This reduces colour fringes for LCD rendered glyphs. */
    err = FT_Library_SetLcdFilter(gLibraryFT, FT_LCD_FILTER_DEFAULT);
    gLCDSupport = err == 0;
#endif
    gLCDSupportValid = true;

    return true;
}

/**
 * FontEngineFT
 */
EngineCapability FontEngineFT::getCapabilities(FontScalerInfo& desc) const
{
    FEM_UNUSED(desc);
    return EngineCapability(CAN_RENDER_MONO | CAN_RENDER_GRAY);
}/* end method getCapabilities */

size_t FontEngineFT::getFontNameAndStyle(const char path[], char name[], size_t length, FontStyle* style)
{
    FT_Library  library;
    size_t count = 0;

    FT_SLOG("FontEngineFT::getFontNameAndStyle\n");

    assert(path);

    if (path) {
        FT_Face face;

        if (FT_Init_FreeType(&library)) {
            return 0;
        }

        if (FT_New_Face(library, path, 0, &face)) {
            FT_Done_FreeType(library);
            return 0;
        }

        const char* s = face->family_name;
        while (s[count] != '\0')
            count++;

        if (name && length && style) {
            if (length < count) {
                count = 0;
                while(length) {
                    name[count] = s[count];
                    count++; length--;
                }

                int fntStyle = STYLE_NORMAL;
                if (face->style_flags & FT_STYLE_FLAG_BOLD) {
                    fntStyle |= STYLE_BOLD;
                }/* end if */

                if (face->style_flags & FT_STYLE_FLAG_ITALIC) {
                    fntStyle |= STYLE_ITALIC;
                }/* end if */

                *style = (FontStyle)fntStyle;
            } else {
                count = 0;  // avoiding buffer over flow
            }/* end else if */
        }

        FT_Done_Face(face);
        FT_Done_FreeType(library);
    }

    return count;
}

size_t FontEngineFT::getFontNameAndStyle(const void* buffer, const uint32_t bufferLength, char name[], size_t length, FontStyle*  style)
{
    FT_Library  library;
    size_t count = 0;

    FT_SLOG("FontEngineFT::getFontNameAndStyle\n");

    assert(buffer && bufferLength);

    if (buffer && bufferLength) {
        FT_Open_Args  args;
        FT_Face  face;

        if (FT_Init_FreeType(&library)) {
            return 0;
        }

        memset(&args, 0, sizeof(args));

        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (const FT_Byte*)buffer;
        args.memory_size = bufferLength;

        if (FT_Open_Face(library, &args, 0, &face)) {
            FT_Done_FreeType(library);
            return 0;
        }

        const char* s = face->family_name;
        FT_LOG("FontEngineFT::getFontNameAndStyle, face->family_name : %s\n", face->family_name);

        while (s[count] != '\0')
            count++;

        if (name && length && style) {
            int fntStyle = STYLE_NORMAL;

            FT_LOG("FontEngineFT::getFontNameAndStyle, length : %d\n", length);
            memset(name, 0, length);

            if (length < count) {
                count = 0;  /* avoiding buffer over flow */
                while(length) {
                    name[count] = s[count];
                    count++; length--;
                }
            } else {
                length = 0;  /* avoiding buffer over flow */
                while(count) {
                    name[length] = s[length];
                    length++; count--;
                }

                count = length;
            }/* end else if */

            FT_LOG("FontEngineFT::getFontNameAndStyle, name : %s\n", name);
            if (face->style_flags & FT_STYLE_FLAG_BOLD) {
                fntStyle |= STYLE_BOLD;
            }/* end if */

            if (face->style_flags & FT_STYLE_FLAG_ITALIC) {
                fntStyle |= STYLE_ITALIC;
            }/* end if */

            *style = (FontStyle)fntStyle;
            FT_LOG("FontEngineFT::getFontNameAndStyle, style : %d\n", *style);
        }

        FT_Done_Face(face);
        FT_Done_FreeType(library);
    }/* end if */

    FT_LOG("FontEngineFT::getFontNameAndStyle, length : %d\n", count);
    return count;
}/* end method getFontNameAndStyle */

/* Given system path to font file; return 'true' if the font format
 * supported; 'false' otherwise.
 *
 * If 'isLoad' is set to 'true' then; font file will be loaded i.e. font
 * objects (sfnt etc.) will be created to determine the font file support.
 * If 'isLoad' is set to 'false' then; the file extension will be checked
 * against the static font format list of the Font Engine.
 */
bool FontEngineFT::isFontSupported(const char path[], bool isLoad)
{
    FT_Library  library;
    bool        retVal = false;

    assert(path);

    if (path) {
        if (FT_Init_FreeType(&library)) {
            FT_SLOG("FontEngineFT::isFontSupported, failed to initalized FreeType\n");
            goto RETURN;
        }

        if (isLoad) {
            FT_Face   face;
            FT_Error  error;

            error = FT_New_Face(library, path, 0, &face);
            if (error == FT_Err_Unknown_File_Format) {
                FT_Done_FreeType(library);
                FT_SLOG("FontEngineFT::isFontSupported, unsupported font format\n");
                goto RETURN;
            } else if (error) {
                FT_Done_FreeType(library);
                FT_SLOG("FontEngineFT::isFontSupported, failed to create FT_Face\n");
                goto RETURN;
            }

            retVal = true;

            FT_Done_Face(face);
            FT_Done_FreeType(library);
        } else {
            unsigned int len = strlen(path);
            unsigned int idx = len - 3;
            const char* fileExtension = &path[idx];

            if (idx > 0) {
                size_t  formatCount = sizeof(formats) / sizeof(formats[0]);

                for (size_t i = 0; i < formatCount; i++) {
                    if (! strcmp(formats[i], fileExtension)) {
                        retVal = true;
                        break;
                    }
                }
            }
        }
    }

RETURN:
    return retVal;
}

/* Given a buffer to font file; return 'true' if the font format supported;
 * 'false' otherwise.
 *
 * Font file will be loaded i.e. font object (sfnt etc.) will be created to
 * determine the font file support.
 */
bool FontEngineFT::isFontSupported(const void* buffer, const uint32_t bufferLength)
{
    FT_Library    library;
    bool          retVal = false;

    assert(buffer && bufferLength);

    if (buffer && bufferLength) {
        FT_Open_Args  args;
        FT_Face       face;
        FT_Error      error;

        if (FT_Init_FreeType(&library)) {
            FT_SLOG("FontEngineFT::isFontSupported, failed to initalized FreeType\n");
            goto RETURN;
        }

        memset(&args, 0, sizeof(args));

        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (const FT_Byte*)buffer;
        args.memory_size = bufferLength;

        error = FT_Open_Face(library, &args, 0, &face);
        if (error == FT_Err_Unknown_File_Format) {
            FT_Done_FreeType(library);
            FT_SLOG("FontEngineFT::isFontSupported, unsupported font format\n");
            goto RETURN;
        } else if (error) {
            FT_Done_FreeType(library);
            FT_SLOG("FontEngineFT::isFontSupported, failed to create FT_Face\n");
            goto RETURN;
        }

        retVal = true;

        FT_Done_Face(face);
        FT_Done_FreeType(library);
    }

RETURN:
    return retVal;
}

/** Given system path of the font file; returns the number of font units
    per em.
    @param path    The system path to font file.
    @return the number of font units per em or 0 on error.
*/
uint32_t FontEngineFT::getFontUnitsPerEm(const char path[])
{
    FT_Library  library;
    uint32_t    unitsPerEm = 0;

    assert(path);

    if (path) {
        FT_Face  face;

        if (FT_Init_FreeType(&library)) {
            FT_SLOG("FontEngineFT::getFontUnitsPerEm, failed to initalized FreeType\n");
            goto RETURN;
        }

        if (FT_New_Face(library, path, 0, &face)) {
            FT_SLOG("FontEngineFT::getFontUnitsPerEm, failed to create FT_Face\n");
            FT_Done_FreeType(library);
            goto RETURN;
        }

        unitsPerEm = face->units_per_EM;

        FT_Done_Face(face);
        FT_Done_FreeType(library);
    }/* end if */

RETURN:
    return unitsPerEm;
}

/** Given font data in buffer; returns the number of font units per em.
    @param buffer          The font file buffer.
    @param bufferLength    Length of the buffer.
    @return the number of font units per em or 0 on error.
*/
uint32_t FontEngineFT::getFontUnitsPerEm(const void* buffer, const uint32_t bufferLength)
{
    FT_Library  library;
    uint32_t    unitsPerEm = 0;

    assert(buffer && bufferLength);

    if (buffer && bufferLength) {
        FT_Open_Args  args;
        FT_Face       face;

        if (FT_Init_FreeType(&library)) {
            FT_SLOG("FontEngineFT::getFontUnitsPerEm, failed to initalized FreeType\n");
            goto RETURN;
        }

        memset(&args, 0, sizeof(args));

        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (const FT_Byte*)buffer;
        args.memory_size = bufferLength;

        if (FT_Open_Face(library, &args, 0, &face)) {
            FT_SLOG("FontEngineFT::getFontUnitsPerEm, failed to open FT_Face\n");
            FT_Done_FreeType(library);
            goto RETURN;
        }

        unitsPerEm = face->units_per_EM;

        FT_Done_Face(face);
        FT_Done_FreeType(library);
    }

RETURN:
    return unitsPerEm;
}

FontScaler* FontEngineFT::createFontScalerContext(const FontScalerInfo& desc)
{
    android::Mutex::Autolock ac(gMutexFT);
    FT_SLOG("FontEngineFT::createFontScalerContext\n");
    return this->getFontScaler(desc);
}/* end method createFontScalerContext */

FontScaler* FontEngineFT::getFontScaler(const FontScalerInfo& desc)
{
    FontScaler*  pFontScaler = NULL;

    FT_SLOG("FontEngineFT::getFontScaler\n");

    FontFT*  pFont = this->getFont(desc);
    if (pFont == NULL) {
        pFont = new FontFT(desc);
        if (! pFont->success()) {
            delete pFont;

            if (gCountFontFT == 0) {
                /* required as font was not initialized */
                FT_SLOG("FT_Done_FreeType\n");
                FT_Done_FreeType(gLibraryFT);

                assert(gLibraryFT = NULL);
            }/* end if */

            return NULL;
        }

        pFontScaler = pFont->getFontScaler(desc);
        if (pFontScaler == NULL) {
            delete pFont;

            return NULL;
        }

        FontNodePtr fontNode = (FontNodePtr)malloc(sizeof(FontNode));
        fontNode->next = NULL;
        fontNode->font = pFont;
        FontFTAddAtHead(&this->pFontList, fontNode);
    } else {
        pFontScaler = pFont->getFontScaler(desc);
    }

    return pFontScaler;
}/* end method getFontScaler */

FontFT* FontEngineFT::getFont(const FontScalerInfo& desc)
{
    FT_SLOG("FontEngineFT::getFont\n");
    FontNodePtr node = this->pFontList;
    FontFT* pFontFT = NULL;

    FT_LOG("desc.pBuffer : %x\n", desc.pBuffer);
    while (node != NULL) {
        if (desc.pBuffer && node->font->pBuffer) {
            FT_LOG("[%x:%x]->", node, node->font->pBuffer);
            if ( desc.pBuffer ==  node->font->pBuffer ) {
                pFontFT = node->font;
                break;
            }/* end if */
        } else {
            FT_LOG("[%s:%x]->", node, node->font->pPath);
            if ( ! strcmp(desc.pPath, node->font->pPath) ) {
                pFontFT = node->font;
                break;
            }/* end if */
        }
        node = node->next;
    }/* end while */

    FT_SLOG("FontEngineFT::getFont, exiting...\n");
    return pFontFT;
}/* end method getFont */

#ifdef __cplusplus
extern "C" {
#endif
    static unsigned long ft_stream_read(FT_Stream       stream,
                                        unsigned long   offset,
                                        unsigned char*  buffer,
                                        unsigned long   count )
    {
        return streamRead((void*)stream->descriptor.pointer, offset, buffer, count);
    }/* end method ft_stream_read */

    static void ft_stream_close(FT_Stream  stream) { FEM_UNUSED(stream); }/* end method ft_stream_close */
#ifdef __cplusplus
}/* end extern "C" */
#endif

/**
 * FontFT.
 */
FontFT::FontFT(const FontScalerInfo& desc)
    : pPath(NULL), bInitialized(false), refCnt(0)
{
    FT_Error    err;
    int flag = 0;

    FT_SLOG("FontFT Constructor\n");

#ifdef ENABLE_FONTINSTLIST
    pFontInstList = NULL;
#endif /* ENABLE_FONTINSTLIST */

    if (gCountFontFT == 0) {
        if (! InitFreetype()) {
            assert(0);
        }
    }

    memset(&streamRecFT, 0, sizeof(FT_StreamRec));
    streamRecFT.size = desc.size;
    streamRecFT.descriptor.pointer = desc.pStream;
    streamRecFT.read  = ft_stream_read;
    streamRecFT.close = ft_stream_close;

    FT_Open_Args  args;
    memset(&args, 0, sizeof(args));

    fontID = desc.fontID;

    pBuffer = desc.pBuffer;
    if (pBuffer) {
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (const FT_Byte*)pBuffer;
        args.memory_size = desc.size;

        flag = 1;
        FT_LOG("FontFT::FontFT, flag : %d\n", flag);
    } else if (desc.pStream) {
        args.flags = FT_OPEN_STREAM;
        args.stream = &streamRecFT;

        flag = 2;
        FT_LOG("FontFT::FontFT, flag : %d\n", flag);
    }

    if (flag) {
        err = FT_Open_Face(gLibraryFT, &args, 0, &pFace);
    } else {
        err = FT_New_Face(gLibraryFT, desc.pPath, 0, &pFace);
    }

    if (err) {
        FT_LOG("ERROR: unable to open font '%d', error num : '%d' \n", fontID, err);
        return;
    } else {
        if (desc.pPath) {
            pPath = strdup(desc.pPath);
        }

        ++gCountFontFT;
        bInitialized = true;
    }
    FT_SLOG("FontFT::FontFT, exiting...\n");
}/* end constructor */

FontScaler* FontFT::getFontScaler(const FontScalerInfo& desc)
{
    FontScaler* pFontScaler = NULL;
    FontInstFT* fontInst = NULL;

#ifdef ENABLE_FONTINSTLIST
    FontInstNodePtr fontInstNode = NULL;
#endif /* ENABLE_FONTINSTLIST */

    FT_SLOG("FontFT::getFontScaler\n");
    FTEnginePrintList;

#ifndef ENABLE_FONTINSTLIST
    fontInst = new FontInstFT(desc, this);
    if (! fontInst->success()) {
        delete fontInst;
        return NULL;
    }
#else
    fontInstNode = searchFontInst(desc);
    if (NULL == fontInstNode)
    {
        fontInst = new FontInstFT(desc, this);
        if (! fontInst->success()) {
            delete fontInst;
            return NULL;
        }

        fontInstNode = (FontInstNodePtr)malloc(sizeof(FontInstNode));
        if (fontInstNode == NULL) {
            FT_SLOG("malloc failed for FontInstNode\n");
            delete fontInst;
            return NULL;
        }

        fontInstNode->inst = fontInst;
        fontInstNode->next = NULL;
        FontFTAddAtHead(&this->pFontInstList, fontInstNode);
        FT_LOG("Font Inst: 0x%x, Font Inst Node: 0x%x\n", fontInst, fontInstNode);
    } else {
        fontInst = fontInstNode->inst;
    }
#endif /* ENABLE_FONTINSTLIST */

    pFontScaler = new FontScalerFT(fontInst);
    FT_LOG("Inst: 0x%x\n", pFontScaler);

    return pFontScaler;
}/* end method getFontScaler */

#ifdef ENABLE_FONTINSTLIST
FontInstNodePtr FontFT::searchFontInst(const FontScalerInfo& desc)
{
  FT_Matrix  ftMatrix22;
  FT_Matrix  ftMatrix22Temp;
  FEM16Dot16 fScaleX;
  FEM16Dot16 fScaleY;
  uint32_t   loadGlyphFlags;

  FontInstNodePtr node = this->pFontInstList;
  FontInstFT* inst;

  FT_SLOG("FontFT::searchFontInst\n");
  this->getTransMatrix(desc, ftMatrix22, fScaleX, fScaleY, loadGlyphFlags);

  while(node)
  {
    inst = node->inst;
    ftMatrix22Temp = inst->ftMatrix22;
    if( (ftMatrix22Temp.xx == ftMatrix22.xx) &&
        (ftMatrix22Temp.xy == ftMatrix22.xy) &&
        (ftMatrix22Temp.yx == ftMatrix22.yx) &&
        (ftMatrix22Temp.yy == ftMatrix22.yy) &&
        (inst->fScaleX == fScaleX) &&
        (inst->fScaleY == fScaleY) &&
        (inst->loadGlyphFlags == loadGlyphFlags) )
    {
      FT_SLOG("FontFT::searchFontInst, Found the Inst Node!!\n");
      return node;
    }/* end if */

    node = node->next;
  }/* end while */

  FT_SLOG("FontFT::searchFontInst, existing\n");
  return NULL;
}
#endif /* ENABLE_FONTINSTLIST */

void FontFT::getTransMatrix(const FontScalerInfo& desc, FT_Matrix& ftMatrix22, FEM16Dot16& fScaleX, FEM16Dot16& fScaleY, uint32_t& loadGlyphFlags)
{
    /* compute our scale factors */
    FEM16Dot16 sx = desc.fScaleX;
    FEM16Dot16 sy = desc.fScaleY;

    if (desc.fSkewX || desc.fSkewY || sx < 0 || sy < 0) {
        /* sort of give up on hinting */
        sx = FEM16Dot16Abs(sx) > FEM16Dot16Abs(desc.fSkewX) ? FEM16Dot16Abs(sx): FEM16Dot16Abs(desc.fSkewX);
        sy = FEM16Dot16Abs(desc.fSkewY) > FEM16Dot16Abs(sy) ? FEM16Dot16Abs(desc.fSkewY) : FEM16Dot16Abs(sy);
        sx = sy = FEM16Dot16Avg(sx, sy);

        FEM16Dot16 inv = FEM16Dot16Invert(sx);

        /* flip the skew elements to go from our Y-down system to FreeType's */
        ftMatrix22.xx = FT_MulFix(desc.fScaleX, inv);
        ftMatrix22.xy = -FT_MulFix(desc.fSkewX, inv);
        ftMatrix22.yx = -FT_MulFix(desc.fSkewY, inv);
        ftMatrix22.yy = FT_MulFix(desc.fScaleY, inv);
    } else {
        ftMatrix22.xx = ftMatrix22.yy = FEMOne16Dot16;
        ftMatrix22.xy = ftMatrix22.yx = 0;
    }

    fScaleX = sx;
    fScaleY = sy;

    /* compute the flags we send to FT_Load_Glyph */
    {
        FT_Int32 loadFlags = FT_LOAD_DEFAULT;
        Hinting h = static_cast<Hinting>(desc.flags >> 1);

        if (desc.subpixelPositioning) {
            switch (h) {
            case HINTING_NONE:
                loadFlags = FT_LOAD_NO_HINTING;
                FT_SLOG("Subpixel, No Hinting\n");
                break;
            case HINTING_FULL:
                loadFlags = FT_LOAD_TARGET_NORMAL;
                FT_SLOG("Full Hinting\n");
                if ( gLCDSupport ) {
                    if (ALIAS_LCD_H == desc.maskFormat)
                        loadFlags = FT_LOAD_TARGET_LCD;
                    else if (ALIAS_LCD_V == desc.maskFormat)
                        loadFlags = FT_LOAD_TARGET_LCD_V;
                }
                break;
            default :
                /* HINTING_LIGHT or HINTING_NORMAL */
                loadFlags = FT_LOAD_TARGET_LIGHT;  // This implies FORCE_AUTOHINT
                FT_SLOG("Subpixel, Light Hinting\n");
            }
        } else {
            switch (h) {
            case HINTING_NONE:
                loadFlags = FT_LOAD_NO_HINTING;
                FT_SLOG("No Hinting\n");
                break;
            case HINTING_NORMAL:
            case HINTING_FULL:
                loadFlags = FT_LOAD_TARGET_NORMAL;
                FT_SLOG("Full Hinting\n");
                if ( gLCDSupport ) {
                    if (ALIAS_LCD_H == desc.maskFormat)
                        loadFlags = FT_LOAD_TARGET_LCD;
                    else if (ALIAS_LCD_V == desc.maskFormat)
                        loadFlags = FT_LOAD_TARGET_LCD_V;
                }
                break;
            default :
                /* HINTING_LIGHT */
                loadFlags = FT_LOAD_TARGET_LIGHT;  // This implies FORCE_AUTOHINT
                FT_SLOG("Light Hinting\n");
            }
        }

        if (desc.maskFormat != ALIAS_MONOCHROME) {
            /* If the user requested anti-aliasing then we don't use bitmap
             * strikes in the font. The consensus among our Japanese users is
             * that this results in the best quality. */
            loadFlags |= FT_LOAD_NO_BITMAP;
        }

        loadGlyphFlags = loadFlags;
    }
}/* end method getTransMatrix */

FontFT::~FontFT()
{
    FT_SLOG("FontFT Destructor\n");

    if (this->bInitialized) {
        FontNodePtr curr = gFontEngineInstFT->pFontList;
        FontNodePtr prev = NULL;
        FontNodePtr next = NULL;

        while (curr) {
            next = curr->next;
            if (curr->font == this) {
                if (-- curr->font->refCnt == 0) {
                    FT_LOG("Deleted font node corresponding to font : %s from font list\n", this->pPath);
                    if (prev) {
                        prev->next = next;
                    } else {
                        gFontEngineInstFT->pFontList = next;
                    }/* end else if */

                    if ( pPath ) {
                        free((char*)pPath);
                    }

                    FT_Done_Face(pFace);
                    pFace = NULL;

                    free(curr);

                    if (--gCountFontFT == 0) {
                        FT_SLOG("FT_Done_FreeType\n");
                        FT_Done_FreeType(gLibraryFT);
                        assert(gLibraryFT = NULL);
                    }/* end if */
                }/* end if */

                goto RET;
            }/* end if */
            prev = curr;
            curr = next;
        }/* end while */

        assert("shouldn't get here, font not in list");
    }/* end if */

RET:
    ;  /* Do nothing */
}/* end destructor */

FontInstFT::FontInstFT(const FontScalerInfo& desc, FontFT* font)
    : ftSize( NULL), pFont(font), refCnt(0), bInitialized(false)
{
    FT_SLOG("FontInstFT::FontInstFT\n");

    pFont->getTransMatrix(desc, ftMatrix22, fScaleX, fScaleY, loadGlyphFlags);
    FT_LOG("FontInstFT::getTransMatrix returned, ftMatrix22.xx  : %d, ftMatrix22.xy : %d, ftMatrix22.yx : %d, ftMatrix22.yy : %d, fScaleX : %d, fScaleY : %d\n", ftMatrix22.xx >> 16, ftMatrix22.xy >> 16, ftMatrix22.yx >> 16, ftMatrix22.yy >> 16, fScaleX >> 16, fScaleY >> 16);

    subpixelPositioning = desc.subpixelPositioning;
    kerning = desc.flags & 0x01;
    maskFormat = desc.maskFormat;

    /* now create the FT_Size */
    {
        FT_Error    err;

        err = FT_New_Size(pFont->pFace, &ftSize);
        if (err != 0) {
            FT_LOG("FontInstFT::FT_New_Size(%d): FT_Set_Char_Size(0x%x, 0x%x) returned 0x%x\n",
                      desc.fontID, fScaleX, fScaleY, err);
            return;
        }

        err = FT_Activate_Size(ftSize);
        if (err != 0) {
            FT_LOG("FontInstFT::FT_Activate_Size(%d, 0x%x, 0x%x) returned 0x%x\n",
                      desc.fontID, fScaleX, fScaleY, err);

            FT_Done_Size(ftSize);
            ftSize = NULL;
        }

        err = FT_Set_Char_Size(pFont->pFace,
                                FEM16Dot16ToFEM26Dot6(fScaleX), FEM16Dot16ToFEM26Dot6(fScaleY),
                                72, 72);
        if (err != 0) {
            FT_LOG("FontInstFT::FT_Set_Char_Size(%d, 0x%x, 0x%x) returned 0x%x\n",
                      desc.fontID, fScaleX, fScaleY, err);

            FT_Done_Size(ftSize);
            ftSize = NULL;

            return;
        }

        FT_Set_Transform(pFont->pFace, &ftMatrix22, NULL);
    }

    bInitialized = true;
    this->pFont->refCnt++;
}

FontInstFT::~FontInstFT()
{
    FT_SLOG("FontInstFT Destructor\n");

    if (bInitialized) {
#ifndef ENABLE_FONTINSTLIST
        FT_Done_Size(ftSize);
        ftSize = NULL;

        if( (-- this->pFont->refCnt) == 0 ) {
            delete this->pFont;
        }/* end if */
#else
        FontInstNodePtr curr = this->pFont->pFontInstList;
        FontInstNodePtr prev = NULL;
        FontInstNodePtr next = NULL;

        while (curr) {
            next = curr->next;
            if (curr->inst == this) {
                if (-- curr->inst->refCnt == 0) {
                    if (prev) {
                        prev->next = next;
                    } else {
                        this->pFont->pFontInstList = next;
                    }/* end else if */

                    FT_Done_Size(ftSize);
                    ftSize = NULL;

                    free(curr);

                    if( (-- this->pFont->refCnt) == 0 ) {
                        delete this->pFont;
                    }/* end if */
                }/* end if */

                goto RET;
            }/* end if */
            prev = curr;
            curr = next;
        }/* end while */

        assert("shouldn't get here, face instance not in list");
#endif /* ENABLE_FONTINSTLIST */
    }/* end if */

RET:
    ;  /* Do nothing */
}

/*  We call this before each use of the fFace, since we may be sharing
    this face with other context (at different sizes).

    Return : 0 on success; non zero value otherwise.
*/
FT_Error FontInstFT::setupSize()
{
    FT_Error    err = 0;

    assert(bInitialized);

    FT_LOG("FTScalerContext::setupSize, this : 0x%x, ftMatrix22.xx  : %d, ftMatrix22.xy : %d, ftMatrix22.yx : %d, ftMatrix22.yy : %d, fScaleX : %d, fScaleY : %d\n", this, ftMatrix22.xx >> 16, ftMatrix22.xy >> 16, ftMatrix22.yx >> 16, ftMatrix22.yy >> 16, fScaleX >> 16, fScaleY >> 16);

    err = FT_Activate_Size(ftSize);
    if (err != 0) {
        FT_LOG("FTScalerContext::FT_Activate_Size(%s, 0x%x, 0x%x) returned 0x%x\n",
                  pFont->pPath, fScaleX, fScaleY, err);
    } else {
        /* seems we need to reset this every time (not sure why, but without it
         * I get random italics from some other fFTSize) */
        FT_Set_Transform(pFont->pFace, &ftMatrix22, NULL);
    }

    return err;
}/* end method setupSize */

/**
 * FontScalerFT.
 */
FontScalerFT::FontScalerFT(FontInstFT* fontInst) : pFontInst(fontInst), bInitialized(false)
{
    FT_SLOG("FontInstFT Constructor\n");

    assert(pFontInst);

    ftFace = fontInst->pFont->pFace;
    bInitialized = true;
    this->pFontInst->refCnt++;
}/* end method constructor */

FontScalerFT::~FontScalerFT()
{
    android::Mutex::Autolock ac(gMutexFT);

    FT_SLOG("FontInstFT Destructor\n");

    if (bInitialized) {
        if (ftFace)
            ftFace = NULL;

        if( (-- this->pFontInst->refCnt) == 0 ) {
            /* delete font */
            delete this->pFontInst;
        }
    }
}/* end method destructor */

uint16_t FontScalerFT::getCharToGlyphID(int32_t charUniCode)
{
    return uint16_t(FT_Get_Char_Index(ftFace, (FT_ULong)charUniCode));
}/* end method getCharToGlyphID */

int32_t FontScalerFT::getGlyphIDToChar(uint16_t glyphID)
{
    /* iterate through each cmap entry, looking for matching glyph indices */
    FT_UInt glyphIndex;
    int32_t charCode = FT_Get_First_Char(ftFace, &glyphIndex);

    while (glyphIndex != 0) {
        if (glyphIndex == glyphID) {
            return charCode;
        }
        charCode = FT_Get_Next_Char(ftFace, charCode, &glyphIndex);
    }

    return 0;
}/* end method getCharToGlyphID */

uint32_t FontScalerFT::getGlyphCount() const
{
    return ftFace->num_glyphs;
}/* end method getGlyphCount */

GlyphMetrics FontScalerFT::getGlyphAdvance(uint16_t glyphID, FEM16Dot16 fracX, FEM16Dot16 fracY)
{
    GlyphMetrics  gm;
#ifdef FT_ADVANCES_H
    /* unhinted and light hinted text have linearly scaled advances
     * which are very cheap to compute with some font formats...
     */
    {
        android::Mutex::Autolock ac(gMutexFT);

        FEM_UNUSED(fracX);
        FEM_UNUSED(fracY);

        if (this->pFontInst->setupSize()) {
            return gm;
        }

        FT_Error  error;
        FT_Fixed  advance;

        error = FT_Get_Advance(ftFace, glyphID,
                                this->pFontInst->loadGlyphFlags | FT_ADVANCE_FLAG_FAST_ONLY,
                                &advance);
        if (0 == error) {
            gm.rsbDelta = 0;
            gm.lsbDelta = 0;
            gm.fAdvanceX = advance;  // advance *2/3; //DEBUG
            gm.fAdvanceY = 0;
        }
    }
#else
    /* otherwise, we need to load/hint the glyph, which is slower */
    gm  = this->getGlyphMetrics(glyphID, fracX, fracY);
#endif/* FT_ADVANCES_H */

    FT_LOG("FontScalerFT::getGlyphAdvance, glyph : %d, advanceX : %d, advanceY : %d\n", glyphID, gm.fAdvanceX >> 16, gm.fAdvanceY >> 16);

    return gm;
}/* end method getGlyphAdvance */

GlyphMetrics FontScalerFT::getGlyphMetrics(uint16_t glyphID, FEM16Dot16 fracX, FEM16Dot16 fracY)
{
    android::Mutex::Autolock ac(gMutexFT);
    GlyphMetrics  gm;

    FT_Error    err;

    if (this->pFontInst->setupSize()) {
        goto ERROR;
    }

    err = FT_Load_Glyph(ftFace, glyphID, this->pFontInst->loadGlyphFlags);
    if (err != 0) {
        FT_LOG("FontScalerFT::getGlyphMetrics(%s): FT_Load_Glyph(glyph:%d flags:%d) returned 0x%x\n",
                 this->pFontInst->pFont->pPath, glyphID, this->pFontInst->loadGlyphFlags, err);
    ERROR:
        gm.clear(); /* or memset(&gm, 0, sizeof(GlyphMetrics)); */
        return gm;
    }

    switch ( ftFace->glyph->format ) {
      case FT_GLYPH_FORMAT_OUTLINE:
        FT_BBox bbox;

        FT_Outline_Get_CBox(&ftFace->glyph->outline, &bbox);

        if (this->pFontInst->subpixelPositioning) {
            int dx = fracX >> 10;
            int dy = fracY >> 10;
            /* negate dy since freetype-y-goes-up and skia-y-goes-down */
            bbox.xMin += dx;
            bbox.yMin -= dy;
            bbox.xMax += dx;
            bbox.yMax -= dy;
        }

        bbox.xMin &= ~63;
        bbox.yMin &= ~63;
        bbox.xMax  = (bbox.xMax + 63) & ~63;
        bbox.yMax  = (bbox.yMax + 63) & ~63;

        gm.width   = uint16_t((bbox.xMax - bbox.xMin) >> 6);
        gm.height  = uint16_t((bbox.yMax - bbox.yMin) >> 6);
        gm.top     = -FEM26Dot6(bbox.yMax);
        gm.left    = FEM26Dot6(bbox.xMin);
        break;

      case FT_GLYPH_FORMAT_BITMAP:
        gm.width   = uint16_t(ftFace->glyph->bitmap.width);
        gm.height  = uint16_t(ftFace->glyph->bitmap.rows);
        gm.top     = -FEM26Dot6(ftFace->glyph->bitmap_top << 6);
        gm.left    = FEM26Dot6(ftFace->glyph->bitmap_left << 6);
        break;

      default:
        assert(!"unknown glyph format");
        goto ERROR;
    }

    if (!this->pFontInst->subpixelPositioning) {
        gm.fAdvanceX = FEM26Dot6ToFEM16Dot16(ftFace->glyph->advance.x);
        gm.fAdvanceY = -FEM26Dot6ToFEM16Dot16(ftFace->glyph->advance.y);
        if (this->pFontInst->kerning) {
            gm.rsbDelta = int8_t(ftFace->glyph->rsb_delta);
            gm.lsbDelta = int8_t(ftFace->glyph->lsb_delta);
        }
    } else {
        gm.fAdvanceX = FT_MulFix(this->pFontInst->ftMatrix22.xx, ftFace->glyph->linearHoriAdvance);
        gm.fAdvanceY = -FT_MulFix(this->pFontInst->ftMatrix22.yx, ftFace->glyph->linearHoriAdvance);
    }

#ifdef ENABLE_GLYPH_SPEW
    FT_LOG("FT_Set_Char_Size(this:%p sx:%x sy:%x ", this, fracX, fracY);

    FT_LOG("Metrics(glyph:%d flags:0x%x) w:%d\n", glyphID, this->pFontInst->loadGlyphFlags, gm.width);
#endif

    FT_LOG("FontScalerFT::getGlyphMetrics, glyph : %d, width : %d, height : %d, top : %d, left : %d, advanceX : %d, advanceY : %d, rsbdelta : %d, lsbdelta : %d\n", glyphID, gm.width, gm.height, gm.top >> 6, gm.left >> 6, gm.fAdvanceX >> 16, gm.fAdvanceY >> 16, gm.rsbDelta, gm.lsbDelta);

    return gm;
}/* end method getGlyphMetrics */

GlyphOutline* FontScalerFT::getGlyphOutline(uint16_t glyphID, FEM16Dot16 fracX, FEM16Dot16 fracY)
{
    android::Mutex::Autolock ac(gMutexFT);
    GlyphOutline* pGO = NULL;

    FEM_UNUSED(fracX);
    FEM_UNUSED(fracY);

    if (this->pFontInst->setupSize()) {
        return pGO;
    }

    uint32_t flags = this->pFontInst->loadGlyphFlags;
    flags |= FT_LOAD_NO_BITMAP; // ignore embedded bitmaps so we're sure to get the outline
    flags &= ~FT_LOAD_RENDER;   // don't scan convert (we just want the outline)

    FT_Error err = FT_Load_Glyph(ftFace, glyphID, flags);
    if (err != 0) {
        FT_LOG("FontScalerFT::getGlyphOutline: FT_Load_Glyph(glyph:%d flags:%d) returned 0x%x\n",
                    glyphID, flags, err);
        return pGO;
    }

    FT_GlyphSlot ftGS = ftFace->glyph;
    if (ftGS)
    {
        FEM26Dot6* x;
        FEM26Dot6* y;
        int16_t* contours;
        uint8_t* tags;

        FT_Outline& ftOtln = ftGS->outline;
        int nOtlnPts = ftOtln.n_points;
        int nContours = ftOtln.n_contours;
        FT_Vector* sOtlnPts = ftOtln.points;
        char* sOtlnTags = ftOtln.tags;
        int16_t* sCntrEndPts = ftOtln.contours;

        pGO = new GlyphOutline(nOtlnPts, nContours);
        if (NULL == pGO)
            return pGO;

        x = pGO->x;
        y = pGO->y;
        tags = pGO->flags;
        contours = pGO->contours;

        for (int i = 0; i < nOtlnPts; ++i) {
            x[i] = sOtlnPts[i].x;
            y[i] = sOtlnPts[i].y;
            tags[i] = sOtlnTags[i] & 0x03;
        }

        for (int i = 0; i < nContours; ++i) {
            contours[i] = sCntrEndPts[i];
        }
    }

    return pGO;
}/* end method getGlyphOutline */

void FontScalerFT::getFontMetrics(FontMetrics* mX, FontMetrics* mY)
{
    if (NULL == mX && NULL == mY) {
        return;
    }

    android::Mutex::Autolock ac(gMutexFT);

    if (this->pFontInst->setupSize()) {
        ERROR:
        if (mX) {
            memset(mX, 0, sizeof(FontMetrics));
        }
        if (mY) {
            memset(mY, 0, sizeof(FontMetrics));
        }
        return;
    }

    int upem = ftFace->units_per_EM;
    if (upem <= 0) {
        goto ERROR;
    }

    FEM16Dot16 ptsX[6];
    FEM16Dot16 ptsY[6];
    FEM16Dot16 ys[6];
    FEM16Dot16 scaleY = this->pFontInst->fScaleY;
    FEM16Dot16 mxy = this->pFontInst->ftMatrix22.xy;
    FEM16Dot16 myy = this->pFontInst->ftMatrix22.yy;
    FEM16Dot16 xmin = (ftFace->bbox.xMin << 16) / upem;
    FEM16Dot16 xmax = (ftFace->bbox.xMax << 16) / upem;

    int leading = ftFace->height - (ftFace->ascender + -ftFace->descender);
    if (leading < 0) {
        leading = 0;
    }

    /* Try to get the OS/2 table from the font. This contains the specific
     * average font width metrics which Windows uses. */
    TT_OS2* os2 = (TT_OS2*) FT_Get_Sfnt_Table(ftFace, ft_sfnt_os2);

    ys[0] = -ftFace->bbox.yMax;
    ys[1] = -ftFace->ascender;
    ys[2] = -ftFace->descender;
    ys[3] = -ftFace->bbox.yMin;
    ys[4] = leading;
    ys[5] = os2 ? os2->xAvgCharWidth : 0;

    FEM16Dot16 x_height;
    if (os2 && os2->sxHeight) {
        x_height = FT_MulDiv(this->pFontInst->fScaleX, os2->sxHeight, upem);
    } else {
        const FT_UInt x_glyph = FT_Get_Char_Index(ftFace, 'x');
        if (x_glyph) {
            FT_BBox bbox;
            FT_Load_Glyph(ftFace, x_glyph, this->pFontInst->loadGlyphFlags);
            FT_Outline_Get_CBox(&ftFace->glyph->outline, &bbox);
            x_height = (bbox.yMax << 16) / 64;
        } else {
            x_height = 0;
        }
    }

    /* convert upem-y values into scalar points */
    for (int i = 0; i < 6; i++) {
        FEM16Dot16 y = FT_MulDiv(scaleY, ys[i], upem);
        FEM16Dot16 x = FT_MulFix(mxy, y);
        y = FT_MulFix(myy, y);
        ptsX[i] = x;
        ptsY[i] = y;
    }

    if (mX) {
        mX->fTop = ptsX[0];
        mX->fAscent = ptsX[1];
        mX->fDescent = ptsX[2];
        mX->fBottom = ptsX[3];
        mX->fLeading = ptsX[4];
        mX->fAvgCharWidth = ptsX[5];
        mX->fXMin = xmin;
        mX->fXMax = xmax;
        mX->fXHeight = x_height;

        FT_LOG("FontScalerFT::getFontMetrics, top : %d, ascent : %d, descent : %d, bottom : %d, leading : %d, avgCharWidth : %d, xmin : %d, xmax : %d, xheight : %d\n", (mX->fTop >> 16), (mX->fAscent >> 16), (mX->fDescent >> 16), (mX->fBottom >> 16), (mX->fLeading >> 16), (mX->fAvgCharWidth >> 16), (mX->fXMin >> 16), (mX->fXMax) >> 16, (mX->fXHeight >> 16));
    }
    if (mY) {
        mY->fTop = ptsY[0];
        mY->fAscent = ptsY[1];
        mY->fDescent = ptsY[2];
        mY->fBottom = ptsY[3];
        mY->fLeading = ptsY[4];
        mY->fAvgCharWidth = ptsY[5];
        mY->fXMin = xmin;
        mY->fXMax = xmax;
        mY->fXHeight = x_height;

        FT_LOG("FontScalerFT::getFontMetrics, top : %d, ascent : %d, descent : %d, bottom : %d, leading : %d, avgCharWidth : %d, xmin : %d, xmax : %d, xheight : %d\n", (mY->fTop >> 16), (mY->fAscent >> 16), (mY->fDescent >> 16), (mY->fBottom >> 16), (mY->fLeading >> 16), (mY->fAvgCharWidth >> 16), (mY->fXMin >> 16), (mY->fXMax) >> 16, (mY->fXHeight >> 16));
    }
}/* end method getFontMetrics */

static FT_Pixel_Mode compute_pixel_mode(AliasMode format) {
    switch (format) {
        case ALIAS_LCD_H:
        case ALIAS_LCD_V:
            assert(!"An LCD format should never be passed here");
            return FT_PIXEL_MODE_GRAY;
        case ALIAS_MONOCHROME:
            return FT_PIXEL_MODE_MONO;
        case ALIAS_GRAYSCALE:
        default:
            return FT_PIXEL_MODE_GRAY;
    }
}/* end method compute_pixel_mode */

void FontScalerFT::getGlyphImage(uint16_t glyphID, FEM16Dot16 fracX, FEM16Dot16 fracY, uint32_t rowBytes, uint16_t width, uint16_t height, uint8_t *buffer)
{
    android::Mutex::Autolock ac(gMutexFT);

    FT_Error    err;

    if (this->pFontInst->setupSize()) {
        goto ERROR;
    }

    FT_LOG("FontScalerFT::getGlyphImage, glyph : %d width : %d height : %d rowBytes : %d\n", glyphID, width, height, rowBytes);

    err = FT_Load_Glyph(ftFace, glyphID, this->pFontInst->loadGlyphFlags);
    if (err != 0) {
        FT_LOG("FontScalerFT::getGlyphImage: FT_Load_Glyph(glyph:%d width:%d height:%d rb:%d flags:%d) returned 0x%x\n",
                  glyphID, width, height, rowBytes, this->pFontInst->loadGlyphFlags, err);
    ERROR:
        memset(buffer, 0, rowBytes * height);
        return;
    }

    const bool lcdRenderMode = this->pFontInst->maskFormat == ALIAS_LCD_H ||
                               this->pFontInst->maskFormat == ALIAS_LCD_V;

    switch ( ftFace->glyph->format ) {
        case FT_GLYPH_FORMAT_OUTLINE: {
            FT_Outline* outline = &ftFace->glyph->outline;
            FT_BBox     bbox;
            FT_Bitmap   target;

            int dx = 0, dy = 0;
            if (this->pFontInst->subpixelPositioning) {
                dx = fracX >> 10;
                dy = fracY >> 10;
                /* negate dy since freetype-y-goes-up and skia-y-goes-down */
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

#if defined(SUPPORT_LCDTEXT)
            if (lcdRenderMode) {
                /* FT_Outline_Get_Bitmap cannot render LCD glyphs. In this case
                 * we have to call FT_Render_Glyph and memcpy the image out. */
                const bool isVertical = this->pFontInst->maskFormat == ALIAS_LCD_V;
                FT_Render_Mode mode = isVertical ? FT_RENDER_MODE_LCD_V : FT_RENDER_MODE_LCD;

                /* TODO:
                 * FT_Render_Glyph(ftFace->glyph, mode);
                 *
                 * if (isVertical)
                 *     CopyFreetypeBitmapToVerticalLCDMask(glyph, ftFace->glyph->bitmap);
                 * else
                 *     CopyFreetypeBitmapToLCDMask(glyph, ftFace->glyph->bitmap);
                 */
                break;
            }
#endif

            target.width = width;
            target.rows = height;
            target.pitch = rowBytes;
            target.buffer = buffer;
            target.pixel_mode = compute_pixel_mode(this->pFontInst->maskFormat);
            target.num_grays = 256;

            memset(buffer, 0, rowBytes * height);
            FT_Outline_Get_Bitmap(gLibraryFT, outline, &target);
        } break;

        case FT_GLYPH_FORMAT_BITMAP: {
            FT_ASSERT_CONTINUE(width == ftFace->glyph->bitmap.width);
            FT_ASSERT_CONTINUE(height == ftFace->glyph->bitmap.rows);

            const uint8_t*  src = (const uint8_t*)ftFace->glyph->bitmap.buffer;
            uint8_t*        dst = buffer;

            if (ftFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY ||
                (ftFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO &&
                  this->pFontInst->maskFormat == ALIAS_MONOCHROME)) {
                unsigned srcRowBytes = ftFace->glyph->bitmap.pitch;
                unsigned dstRowBytes = rowBytes;
                unsigned minRowBytes = srcRowBytes < dstRowBytes ? srcRowBytes : dstRowBytes;
                unsigned extraRowBytes = dstRowBytes - minRowBytes;

                for (int y = ftFace->glyph->bitmap.rows - 1; y >= 0; --y) {
                    memcpy(dst, src, minRowBytes);
                    memset(dst + minRowBytes, 0, extraRowBytes);
                    src += srcRowBytes;
                    dst += dstRowBytes;
                }
            } else if (ftFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO &&
                        this->pFontInst->maskFormat == ALIAS_GRAYSCALE) {
                for (int y = 0; y < ftFace->glyph->bitmap.rows; ++y) {
                    uint8_t byte = 0;
                    int bits = 0;
                    const uint8_t* src_row = src;
                    uint8_t* dst_row = dst;

                    for (int x = 0; x < ftFace->glyph->bitmap.width; ++x) {
                        if (!bits) {
                            byte = *src_row++;
                            bits = 8;
                        }

                        *dst_row++ = byte & 0x80 ? 0xff : 0;
                        bits--;
                        byte <<= 1;
                    }

                    src += ftFace->glyph->bitmap.pitch;
                    dst += rowBytes;
                }
            } else {
              assert(!"unknown glyph bitmap transform needed");
            }

            if (lcdRenderMode) {
                /* TODO: glyph.expandA8ToLCD(); */
            }
        } break;
        default:
            assert(!"unknown glyph format");
            goto ERROR;
    }
}/* end method generateImage */

void FontScalerFT::emboldenOutline(FT_Outline* outline) {
    FT_Pos strength;
    strength = FT_MulFix(ftFace->units_per_EM, ftFace->size->metrics.y_scale) / 24;
    FT_Outline_Embolden(outline, strength);
}/* end method emboldenOutline */

//////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif
    FontEngine* getFontEngineInstance()
    {
        FT_SLOG("getFontEngineInstance\n");
        gFontEngineInstFT = new FontEngineFT();
        return (FontEngine*)gFontEngineInstFT;
    }/* getFontEngineInstance() */
#ifdef __cplusplus
}/* end extern "C" */
#endif

