/* libs/graphics/ports/SkFontHost_android.cpp
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

#include "SkFontHost.h"
#include "SkDescriptor.h"
#include "SkMMapStream.h"
#include "SkPaint.h"
#include "SkString.h"
#include "SkStream.h"
#include "SkThread.h"
#include "SkTSearch.h"
#include <stdio.h>
#include "../../system/core/include/cutils/properties.h"
#include "fontrec.h"

#define FONT_CACHE_MEMORY_BUDGET    (768 * 1024)

#define SK_ANDROID_DEFAULT_FONT_SYSTEM_PROPERTY "persist.sys.font.dfont"

#define DEFAULT_FALLBACK_NO 0

SkTypeface::Style find_name_and_attributes(SkStream* stream, SkString* name,
                                           bool* isFixedWidth);

static SkTypeface* gDefaultFont[4] = { NULL };

static void GetFullPathForSysFonts(SkString* full, const char name[]) {
    full->append(name);
}

///////////////////////////////////////////////////////////////////////////////

struct FamilyRec;

/*  This guy holds a mapping of a name -> family, used for looking up fonts.
    Since it is stored in a stretchy array that doesn't preserve object
    semantics, we don't use constructor/destructors, but just have explicit
    helpers to manage our internal bookkeeping.
*/
struct NameFamilyPair {
    const char* fName;      // we own this
    FamilyRec*  fFamily;    // we don't own this, we just reference it

    void construct(const char name[], FamilyRec* family) {
        fName = strdup(name);
        fFamily = family;   // we don't own this, so just record the referene
    }

    void destruct() {
        free((char*)fName);
        // we don't own family, so just ignore our reference
    }
};

// we use atomic_inc to grow this for each typeface we create
static int32_t gUniqueFontID;

// this is the mutex that protects these globals
static SkMutex gFamilyMutex;
static FamilyRec* gFamilyHead;
static SkTDArray<NameFamilyPair> gNameList;

struct FamilyRec {
    FamilyRec*  fNext;
    SkTypeface* fFaces[4];
    int fUseFallbackFontsEx;

    FamilyRec(const int useFallbackFontsEx)
    {
        fUseFallbackFontsEx = useFallbackFontsEx;

        fNext = gFamilyHead;
        memset(fFaces, 0, sizeof(fFaces));
        gFamilyHead = this;
    }
};

static SkTypeface* find_best_face(const FamilyRec* family,
                                  SkTypeface::Style style) {
    SkTypeface* const* faces = family->fFaces;

    if (faces[style] != NULL) { // exact match
        return faces[style];
    }
    // look for a matching bold
    style = (SkTypeface::Style)(style ^ SkTypeface::kItalic);
    if (faces[style] != NULL) {
        return faces[style];
    }
    // look for the plain
    if (faces[SkTypeface::kNormal] != NULL) {
        return faces[SkTypeface::kNormal];
    }
    // look for anything
    for (int i = 0; i < 4; i++) {
        if (faces[i] != NULL) {
            return faces[i];
        }
    }
    // should never get here, since the faces list should not be empty
    SkASSERT(!"faces list is empty");
    return NULL;
}

static FamilyRec* find_family(const SkTypeface* member) {
    FamilyRec* curr = gFamilyHead;
    while (curr != NULL) {
        for (int i = 0; i < 4; i++) {
            if (curr->fFaces[i] == member) {
                return curr;
            }
        }
        curr = curr->fNext;
    }
    return NULL;
}

/**
 *  setSelectedDefaultFontName()
 *
 *  Returns the matching FamilyRec, or NULL.
 *
 *  @param  uint32_t        (IN) uniqueID
 *  @return FamilyRec*      matching FamilyRec, or NULL
 */
static FamilyRec* find_family_from_uniqueID(uint32_t uniqueID) {
    FamilyRec* curr = gFamilyHead;
    while (curr != NULL) {
        for (int i = 0; i < 4; i++) {
            SkTypeface* face = curr->fFaces[i];
            if (face != NULL && face->uniqueID() == uniqueID) {
                return curr;
            }
        }
        curr = curr->fNext;
    }
    return NULL;
}

/*  Returns the matching typeface, or NULL. If a typeface is found, its refcnt
    is not modified.
 */
static SkTypeface* find_from_uniqueID(uint32_t uniqueID) {
    FamilyRec* curr = gFamilyHead;
    while (curr != NULL) {
        for (int i = 0; i < 4; i++) {
            SkTypeface* face = curr->fFaces[i];
            if (face != NULL && face->uniqueID() == uniqueID) {
                return face;
            }
        }
        curr = curr->fNext;
    }
    return NULL;
}

/*  Remove reference to this face from its family. If the resulting family
    is empty (has no faces), return that family, otherwise return NULL
*/
static FamilyRec* remove_from_family(const SkTypeface* face) {
    FamilyRec* family = find_family(face);
    SkASSERT(family->fFaces[face->style()] == face);
    family->fFaces[face->style()] = NULL;

    for (int i = 0; i < 4; i++) {
        if (family->fFaces[i] != NULL) {    // family is non-empty
            return NULL;
        }
    }
    return family;  // return the empty family
}

// maybe we should make FamilyRec be doubly-linked
static void detach_and_delete_family(FamilyRec* family) {
    FamilyRec* curr = gFamilyHead;
    FamilyRec* prev = NULL;

    while (curr != NULL) {
        FamilyRec* next = curr->fNext;
        if (curr == family) {
            if (prev == NULL) {
                gFamilyHead = next;
            } else {
                prev->fNext = next;
            }
            SkDELETE(family);
            return;
        }
        prev = curr;
        curr = next;
    }
    SkASSERT(!"Yikes, couldn't find family in our list to remove/delete");
}

static SkTypeface* find_typeface(const char name[], SkTypeface::Style style) {
    NameFamilyPair* list = gNameList.begin();
    int             count = gNameList.count();

    int index = SkStrLCSearch(&list[0].fName, count, name, sizeof(list[0]));

    if (index >= 0) {
        return find_best_face(list[index].fFamily, style);
    }
    return NULL;
}

static SkTypeface* find_typeface(const SkTypeface* familyMember,
                                 SkTypeface::Style style) {
    const FamilyRec* family = find_family(familyMember);
    return family ? find_best_face(family, style) : NULL;
}

static void add_name(const char name[], FamilyRec* family) {
    SkAutoAsciiToLC tolc(name);
    name = tolc.lc();

    NameFamilyPair* list = gNameList.begin();
    int             count = gNameList.count();

    int index = SkStrLCSearch(&list[0].fName, count, name, sizeof(list[0]));

    if (index < 0) {
        list = gNameList.insert(~index);
        list->construct(name, family);
    }
}

static void remove_from_names(FamilyRec* emptyFamily)
{
#ifdef SK_DEBUG
    for (int i = 0; i < 4; i++) {
        SkASSERT(emptyFamily->fFaces[i] == NULL);
    }
#endif

    SkTDArray<NameFamilyPair>& list = gNameList;

    // must go backwards when removing
    for (int i = list.count() - 1; i >= 0; --i) {
        NameFamilyPair* pair = &list[i];
        if (pair->fFamily == emptyFamily) {
            pair->destruct();
            list.remove(i);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

class SkTypefaceEx : public SkTypeface {
public:
    SkTypefaceEx(Style style, uint32_t uniqueID, bool sysFont,
                   bool isFixedWidth)
    : SkTypeface(style, uniqueID, isFixedWidth) {
        fIsSysFont = sysFont;
    }

    bool isSysFont() const { return fIsSysFont; }

    virtual SkStream* openStream() = 0;
    virtual const char* getUniqueString() const = 0;
    virtual const char* getFilePath() const = 0;

private:
    bool    fIsSysFont;

    typedef SkTypeface INHERITED;
};

class FamilyTypeface : public SkTypefaceEx {
public:
    FamilyTypeface(Style style, bool sysFont, SkTypeface* familyMember,
         bool isFixedWidth, const int useFallbackFontsEx
         )
    : SkTypefaceEx(style, sk_atomic_inc(&gUniqueFontID) + 1, sysFont, isFixedWidth) {

        SkAutoMutexAcquire  ac(gFamilyMutex);

        FamilyRec* rec = NULL;
        if (familyMember) {
            rec = find_family(familyMember);
            SkASSERT(rec);
        } else {
            rec = SkNEW(FamilyRec(useFallbackFontsEx));
        }
        rec->fFaces[style] = this;
    }

    virtual ~FamilyTypeface() {
        SkAutoMutexAcquire  ac(gFamilyMutex);

        // remove us from our family. If the family is now empty, we return
        // that and then remove that family from the name list
        FamilyRec* family = remove_from_family(this);
        if (NULL != family) {
            remove_from_names(family);
            detach_and_delete_family(family);
        }
    }
};

///////////////////////////////////////////////////////////////////////////////

class StreamTypeface : public FamilyTypeface {
public:
    StreamTypeface(Style style, bool sysFont, SkTypeface* familyMember,
                   SkStream* stream, bool isFixedWidth)
    : INHERITED(style, sysFont, familyMember, DEFAULT_FALLBACK_NO, isFixedWidth) {
        SkASSERT(stream);
        stream->ref();
        fStream = stream;
    }
    virtual ~StreamTypeface() {
        fStream->unref();
    }

    // overrides
    virtual SkStream* openStream() {
        // we just ref our existing stream, since the caller will call unref()
        // when they are through
        fStream->ref();
        // must rewind each time, since the caller assumes a "new" stream
        fStream->rewind();
        return fStream;
    }
    virtual const char* getUniqueString() const { return NULL; }
    virtual const char* getFilePath() const { return NULL; }

private:
    SkStream* fStream;

    typedef FamilyTypeface INHERITED;
};

class FileTypeface : public FamilyTypeface {
public:
    FileTypeface(Style style, bool sysFont, SkTypeface* familyMember,
                 const char path[], bool isFixedWidth, const int useFallbackFontsEx)
    : INHERITED(style, sysFont, familyMember, isFixedWidth, useFallbackFontsEx) {
        SkString fullpath;

        if (sysFont) {
            GetFullPathForSysFonts(&fullpath, path);
            path = fullpath.c_str();
        }
        fPath.set(path);
    }

    // overrides
    virtual SkStream* openStream() {
        SkStream* stream = SkNEW_ARGS(SkMMAPStream, (fPath.c_str()));

        // check for failure
        if (stream->getLength() <= 0) {
            SkDELETE(stream);
            // maybe MMAP isn't supported. try FILE
            stream = SkNEW_ARGS(SkFILEStream, (fPath.c_str()));
            if (stream->getLength() <= 0) {
                SkDELETE(stream);
                stream = NULL;
            }
        }
        return stream;
    }
    virtual const char* getUniqueString() const {
        const char* str = strrchr(fPath.c_str(), '/');
        if (str) {
            str += 1;   // skip the '/'
        }
        return str;
    }
    virtual const char* getFilePath() const {
        return fPath.c_str();
    }

private:
    SkString fPath;

    typedef FamilyTypeface INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

class SkDefaultTypeface : public SkTypefaceEx {
private:
    /** 
     *  @return SkTypeface of the default font.
     */
    SkTypeface* createTargetTypeface() const {
        SkString name;
        SkFontManager::getSelectedDefaultFontName(&name);

        return SkFontHost::CreateDefaultTypeface(name.c_str(), SkTypeface::style());
    }

public:
    /** 
     *  constructor
     */
    SkDefaultTypeface(Style style)
    : SkTypefaceEx(style, 0, true, true) { } // Yasu!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1

    /** 
     *  destructor
     */
    ~SkDefaultTypeface() {
        if (gDefaultFont[SkTypeface::style()] == this) {
            gDefaultFont[SkTypeface::style()] = NULL;
        }
    }

    /** 
     *  @return font style
     */
    Style style() const {
        const SkTypeface* tf = createTargetTypeface();
        return tf->style();
    }

    /** 
     *  @return true - style is Bold / false - style is not Bold
     */
    bool isBold() const {
        SkTypeface* tf = createTargetTypeface();
        return tf->isBold();
    }
    
    /** 
     *  @return true - style is Italic / false - style is not Italic
     */
    bool isItalic() const {
        SkTypeface* tf = createTargetTypeface();
        return tf->isItalic();
    }

    /** 
     *  @return fontID
     */
    uint32_t uniqueID() const {
        SkTypeface* tf = createTargetTypeface();
        return tf->uniqueID();
    }

    virtual SkStream* openStream() { return NULL; }
    virtual const char* getUniqueString() const { return NULL; }
    virtual const char* getFilePath() const { return NULL; }
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static bool get_name_and_style(const char path[], SkString* name,
                               SkTypeface::Style* style,
                               bool* isFixedWidth, bool isExpected) {
    SkString        fullpath;
    GetFullPathForSysFonts(&fullpath, path);

    SkMMAPStream stream(fullpath.c_str());
    if (stream.getLength() > 0) {
        *style = find_name_and_attributes(&stream, name, isFixedWidth);
        return true;
    }
    else {
        SkFILEStream stream(fullpath.c_str());
        if (stream.getLength() > 0) {
            *style = find_name_and_attributes(&stream, name, isFixedWidth);
            return true;
        }
    }

    if (isExpected) {
        SkDebugf("---- failed to open <%s> as a font\n", fullpath.c_str());
    }
    return false;
}

#define DEFAULT_NAMES   gSansNames

// these globals are assigned (once) by load_system_fonts()
static FamilyRec* gDefaultFamily;
static SkTypeface* gDefaultNormal;

static FallbackIdArray* gFallbackFonts;

/*  Called once (ensured by the sentinel check at the beginning of our body).
    Initializes all the globals, and register the system fonts.
 */
static void load_system_fonts() {
    // check if we've already be called
    if (NULL != gDefaultNormal) {
        return;
    }

    gFallbackFonts = getFallBackFonts();

    const FontInitRec* rec = getFontInitRec();
    SkTypeface* firstInFamily = NULL;
    int fallbackCount[INIT_REC_COUNT];
    for (unsigned int i = 0; i < INIT_REC_COUNT; i++) {
        fallbackCount[i] = 0;
    }

    for (size_t i = 0; i < INIT_REC_COUNT; i++) {
        // if we're the first in a new family, clear firstInFamily
        if (rec[i].fNames != NULL) {
            firstInFamily = NULL;
        }

        bool isFixedWidth;
        SkString name;
        SkTypeface::Style style;

        // we expect all the fonts, except the "fallback" fonts
        bool isExpected = (rec[i].fNames != gFBNames);
        if (!get_name_and_style(rec[i].fFileName, &name, &style,
                                &isFixedWidth, isExpected)) {
            continue;
        }

        SkTypeface* tf = SkNEW_ARGS(FileTypeface,
                                    (style,
                                     true,  // system-font (cannot delete)
                                     firstInFamily, // what family to join
                                     rec[i].fFileName,
                                     isFixedWidth, // filename
                                     rec[i].fUseFallbackFontsEx) // use fallback fonts ex
                                    );

        if (rec[i].fNames != NULL) {
            // see if this is one of our fallback fonts
            if (rec[i].fNames == gFBNames) {
            //    SkDebugf("---- adding %s as fallback[%d] fontID %d\n",
            //             rec[i].fFileName, fallbackCount, tf->uniqueID());
                gFallbackFonts[rec[i].fUseFallbackFontsEx][fallbackCount[rec[i].fUseFallbackFontsEx]++] = tf->uniqueID();
                gFallbackFonts[rec[i].fUseFallbackFontsEx][fallbackCount[rec[i].fUseFallbackFontsEx]] = 0;
            }

            firstInFamily = tf;
            FamilyRec* family = find_family(tf);
            const char* const* names = rec[i].fNames;

            // record the default family if this is it
            if (names == DEFAULT_NAMES) {
                gDefaultFamily = family;
            }
            // add the names to map to this family
            while (*names) {
                add_name(*names, family);
                names += 1;
            }
        }
    }

    // do this after all fonts are loaded. This is our default font, and it
    // acts as a sentinel so we only execute load_system_fonts() once
    gDefaultNormal = find_best_face(gDefaultFamily, SkTypeface::kNormal);
    // now terminate our fallback list with the sentinel value
//    gFallbackFonts[fallbackCount] = 0;
}

///////////////////////////////////////////////////////////////////////////////

void SkFontHost::Serialize(const SkTypeface* face, SkWStream* stream) {
    const char* name = ((SkTypefaceEx*)face)->getUniqueString();

    stream->write8((uint8_t)face->style());

    if (NULL == name || 0 == *name) {
        stream->writePackedUInt(0);
//        SkDebugf("--- fonthost serialize null\n");
    } else {
        uint32_t len = strlen(name);
        stream->writePackedUInt(len);
        stream->write(name, len);
//      SkDebugf("--- fonthost serialize <%s> %d\n", name, face->style());
    }
}

SkTypeface* SkFontHost::Deserialize(SkStream* stream) {
    load_system_fonts();

    int style = stream->readU8();

    int len = stream->readPackedUInt();
    if (len > 0) {
        SkString str;
        str.resize(len);
        stream->read(str.writable_str(), len);

        const FontInitRec* rec = getFontInitRec();
        const char* fn;

        for (size_t i = 0; i < INIT_REC_COUNT; i++) {
            fn = strrchr(rec[i].fFileName, '/');
            if (fn) {
                fn += 1;   // skip the '/'
            } else {
                fn = rec[i].fFileName;
            }
            if (strcmp(fn, str.c_str()) == 0) {
                // backup until we hit the fNames
                for (int j = i; j >= 0; --j) {
                    if (rec[j].fNames != NULL) {
                        return SkFontHost::CreateTypeface(NULL,
                                    rec[j].fNames[0], NULL, 0, (SkTypeface::Style)style);
                    }
                }
            }
        }
    }
    return SkFontHost::CreateTypeface(NULL, NULL, NULL, 0, (SkTypeface::Style)style);
}

///////////////////////////////////////////////////////////////////////////////

SkTypeface* SkFontHost::CreateTypeface(const SkTypeface* familyFace,
                                       const char familyName[],
                                       const void* data, size_t bytelength,
                                       SkTypeface::Style style) {
    load_system_fonts();

    SkAutoMutexAcquire  ac(gFamilyMutex);

    // clip to legal style bits
    style = (SkTypeface::Style)(style & SkTypeface::kBoldItalic);

    SkTypeface* tf = NULL;

    if (NULL != familyFace) {
        tf = find_typeface(familyFace, style);
    } else if (NULL != familyName) {
//        SkDebugf("======= familyName <%s>\n", familyName);
        tf = find_typeface(familyName, style);
    }

    if (NULL == tf) {
        if (gDefaultFont[style] == NULL) {
            tf = SkNEW_ARGS(SkDefaultTypeface, (style));
            gDefaultFont[style] = tf;
        } else {
            tf = gDefaultFont[style];
        }
    }

    // we ref(), since the symantic is to return a new instance
    tf->ref();
    return tf;
}

bool SkFontHost::ValidFontID(uint32_t fontID) {
    SkAutoMutexAcquire  ac(gFamilyMutex);

    return find_from_uniqueID(fontID) != NULL;
}

SkStream* SkFontHost::OpenStream(uint32_t fontID) {
    SkAutoMutexAcquire  ac(gFamilyMutex);

    SkTypefaceEx* tf = (SkTypefaceEx*)find_from_uniqueID(fontID);
    SkStream* stream = tf ? tf->openStream() : NULL;

    if (stream && stream->getLength() == 0) {
        stream->unref();
        stream = NULL;
    }
    return stream;
}

size_t SkFontHost::GetFileName(SkFontID fontID, char path[], size_t length,
                               int32_t* index) {
    SkAutoMutexAcquire  ac(gFamilyMutex);

    SkTypefaceEx* tf = (SkTypefaceEx*)find_from_uniqueID(fontID);
    const char* src = tf ? tf->getFilePath() : NULL;

    if (src) {
        size_t size = strlen(src);
        if (path) {
            memcpy(path, src, SkMin32(size, length));
        }
        if (index) {
            *index = 0; // we don't have collections (yet)
        }
        return size;
    } else {
        return 0;
    }
}

uint32_t SkFontHost::NextLogicalFont(uint32_t fontID) {
    load_system_fonts();

    /*  First see if fontID is already one of our fallbacks. If so, return
        its successor. If fontID is not in our list, then return the first one
        in our list. Note: list is zero-terminated, and returning zero means
        we have no more fonts to use for fallbacks.
     */
    uint32_t nextLogicalFontID = gFallbackFonts[0][0];
    uint32_t* list = gFallbackFonts[0];

    FamilyRec* family = find_family_from_uniqueID(fontID);

    if (family != NULL) {
        nextLogicalFontID = gFallbackFonts[family->fUseFallbackFontsEx][0];
        list = gFallbackFonts[family->fUseFallbackFontsEx];
    }

    for (int i = 0; list[i] != 0; i++) {
        if (list[i] == fontID) {
            nextLogicalFontID = list[i+1];
        }
    }

//    SkDebugf("--- fonthost NextLogicalFont fontID = %d nextLogicalFontID = %d\n", fontID, nextLogicalFontID);

    return nextLogicalFontID;
}

///////////////////////////////////////////////////////////////////////////////

SkTypeface* SkFontHost::CreateTypefaceFromStream(SkStream* stream) {
    if (NULL == stream || stream->getLength() <= 0) {
        return NULL;
    }

    bool isFixedWidth;
    SkString name;
    SkTypeface::Style style = find_name_and_attributes(stream, &name, &isFixedWidth);

    if (!name.isEmpty()) {
        return SkNEW_ARGS(StreamTypeface, (style, false, NULL, stream, isFixedWidth));
    } else {
        return NULL;
    }
}

SkTypeface* SkFontHost::CreateTypefaceFromFile(const char path[]) {
    SkStream* stream = SkNEW_ARGS(SkMMAPStream, (path));
    SkTypeface* face = SkFontHost::CreateTypefaceFromStream(stream);
    // since we created the stream, we let go of our ref() here
    stream->unref();
    return face;
}

///////////////////////////////////////////////////////////////////////////////

size_t SkFontHost::ShouldPurgeFontCache(size_t sizeAllocatedSoFar) {
    if (sizeAllocatedSoFar > FONT_CACHE_MEMORY_BUDGET)
        return sizeAllocatedSoFar - FONT_CACHE_MEMORY_BUDGET;
    else
        return 0;   // nothing to do
}

///////////////////////////////////////////////////////////////////////////////

/** 
 *  CreateDefaultTypeface()
 *  
 *  SkTypeface set to default is acquired.
 *  
 *  @param  const char*     (IN) default familyname
 *  @param  style           (IN) font style
 *  @return SkTypeface*     default SkTypeface pointer
 */
SkTypeface* SkFontHost::CreateDefaultTypeface(const char familyName[],
                                              SkTypeface::Style style) {
    load_system_fonts();

    SkAutoMutexAcquire  ac(gFamilyMutex);

    // clip to legal style bits
    style = (SkTypeface::Style)(style & SkTypeface::kBoldItalic);

    SkTypeface* tf = NULL;

    if (NULL != familyName) {
        tf = find_typeface(familyName, style);
    }

    if (NULL == tf) {
        tf = find_best_face(gDefaultFamily, style);
    }

    return tf;
}

///////////////////////////////////////////////////////////////////////////////

/** 
 *  GetFontNameList()
 *  
 *  The font name and the alias that can be used as a default font are acquired.
 *  
 *  @param  SKFONTLIST*     (OUT)font name list
 *  @param  SkString*       (IN) language code
 *  @return -
 */
void SkFontHost::GetFontNameList(SKFONTLIST* names, SkString* language) {
    if (names != NULL) {
        const FontInitRec* rec = getFontInitRec();
        for (size_t i = 0; i < INIT_REC_COUNT; i++) {
            /* sub font files and fallback font files are off the subject. */
            if ((rec[i].fNames == NULL)
            ||  (rec[i].fNames == gFBNames)
            ||  (rec[i].fHide  == true)) {
                continue;
            }

            /* full path of font file. */
            SkString fullpath;
            GetFullPathForSysFonts(&fullpath, rec[i].fFileName);

            SkFontManager::SkFontName* fontname = SkNEW(SkFontManager::SkFontName);

            fontname->displayName.set(NULL);
            if (SkFontHost::getDisplayName(&(fontname->displayName), language, &fullpath)) {
                fontname->name.set(rec[i].fNames[0]);
                /* font name is not set. */
                if (fontname->displayName.isEmpty()) {
                    /* alias is set. */
                    fontname->displayName.set(fontname->name.c_str());
                }
                names->append(1, &fontname);
            }
            else {
                SkDELETE(fontname);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

/** 
 *  getSelectableDefaultFonts()
 *  
 *  Get the selectable fonts infomation.
 *  
 *  @param  SKFONTLIST*     (OUT)font name list
 *  @param  SkString*       (IN) language
 *  @return -
 */
void SkFontManager::getSelectableDefaultFonts(SKFONTLIST* names, SkString* language) {
    if (names != NULL) {
        SkFontHost::GetFontNameList(names, language);
    }
}

/** 
 *  getSelectedDefaultFontName()
 *  
 *  Get the selected font name.
 *  
 *  @param  SkString*       (OUT)default font name
 *  @return -
 */
void SkFontManager::getSelectedDefaultFontName(SkString* name) {
    if (name != NULL) {
        char c_name[PROPERTY_VALUE_MAX];
        property_get(SK_ANDROID_DEFAULT_FONT_SYSTEM_PROPERTY, c_name, "");

        if (!strcmp(c_name, "")) {
            strcpy(c_name, DEFAULT_NAMES[0]);
        }
        name->set(c_name);
    }
}

/** 
 *  setSelectedDefaultFontName()
 *  
 *  Set the selected font name.
 *  
 *  @param  SkString*       (IN) name
 *  @return bool            true - OK / false - NG
 */
bool SkFontManager::setSelectedDefaultFontName(SkString* name) {
    bool ret = false;
    
    if (name != NULL) {
        SKFONTLIST names;
        SkFontHost::GetFontNameList(&names, NULL);

        SkFontManager::SkFontName** list = names.begin();
        int count = names.count();

        for (int i = 0; i < count; i++) {
            if (list[i]->name.equals(name->c_str())) {
                ret = !(static_cast<bool>(property_set(SK_ANDROID_DEFAULT_FONT_SYSTEM_PROPERTY, name->c_str())));
                break;
            }
        }
        names.deleteAll();
        list = NULL;
    }

    return ret;
}

/** 
 *  reset()
 *
 *  Reset.
 *  
 *  @param  -
 *  @return bool            true - OK / false - NG
 */
bool SkFontManager::reset() {
    bool ret = false;
    ret = !(static_cast<bool>(property_set(SK_ANDROID_DEFAULT_FONT_SYSTEM_PROPERTY, "")));

    return ret;
}
