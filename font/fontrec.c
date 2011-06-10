/* libs/graphics/ports/SkFontHost_android.cpp
**
** Copyright 2011, The Android Open Source Project
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

#include "fontrec.h"

const char* gCustomNames[] = {
    "custom", NULL
};

const char* gSansNames[] = {
    "sans-serif", "arial", "helvetica", "tahoma", "verdana", NULL
};

const char* gSerifNames[] = {
    "serif", "times", "times new roman", "palatino", "georgia", "baskerville",
    "goudy", "fantasy", "cursive", "ITC Stone Serif", NULL
};

const char* gMonoNames[] = {
    "monospace", "courier", "courier new", "monaco", NULL
};

// deliberately empty, but we use the address to identify fallback fonts
const char* gFBNames[] = { NULL };

/*  Fonts must be grouped by family, with the first font in a family having the
    list of names (even if that list is empty), and the following members having
    null for the list. The names list must be NULL-terminated
*/
static const struct FontInitRec gSystemFonts[] = {
    { "/system/fonts/DroidSans.ttf",               gSansNames,     1,      false },
    { "/system/fonts/DroidSans-Bold.ttf",          NULL,           1,      false },
    { "/system/fonts/DroidSerif-Regular.ttf",      gSerifNames,    1,      false },
    { "/system/fonts/DroidSerif-Bold.ttf",         NULL,           1,      false },
    { "/system/fonts/DroidSerif-Italic.ttf",       NULL,           1,      false },
    { "/system/fonts/DroidSerif-BoldItalic.ttf",   NULL,           1,      false },
    { "/system/fonts/DroidSansMono.ttf",           gMonoNames,     1,      true  },
    { "/data/fonts/customfont.ttf",                gCustomNames,   0,      false },
    /*  These are optional, and can be ignored if not found in the file system.
        These are appended to gFallbackFonts[] as they are seen, so we list
        them in the order we want them to be accessed by NextLogicalFont().
     */
    { "/system/fonts/DroidSans.ttf",               gFBNames,       0,      false },
    { "/system/fonts/DroidSansArabic.ttf",         gFBNames,       0,      false },
    { "/system/fonts/DroidSansHebrew.ttf",         gFBNames,       0,      false },
    { "/system/fonts/DroidSansThai.ttf",           gFBNames,       0,      false },
    { "/system/fonts/DroidSansJapanese.ttf",       gFBNames,       0,      false },
    { "/system/fonts/DroidSansFallback.ttf",       gFBNames,       0,      false },
    { "/system/fonts/DroidSansArabic.ttf",         gFBNames,       1,      false },
    { "/system/fonts/DroidSansHebrew.ttf",         gFBNames,       1,      false },
    { "/system/fonts/DroidSansThai.ttf",           gFBNames,       1,      false },
    { "/system/fonts/DroidSansJapanese.ttf",       gFBNames,       1,      false },
    { "/system/fonts/DroidSansFallback.ttf",       gFBNames,       1,      false }
};

/*  This is sized conservatively, assuming that it will never be a size issue.
    It will be initialized in load_system_fonts(), and will be filled with the
    fontIDs that can be used for fallback consideration, in sorted order (sorted
    meaning element[0] should be used first, then element[1], etc. When we hit
    a fontID==0 in the array, the list is done, hence our allocation size is
    +1 the total number of possible system fonts. Also see NextLogicalFont().
 */
#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))
static FallbackIdArray gFallbackFonts[ARRAY_COUNT(gSystemFonts)];


/**
 * Get Font-Init-Recode.
 * @return FontInitRec* Font-Init-Recode
 * @date 2011/05/20
 */
const struct FontInitRec* getFontInitRec() {
    return gSystemFonts;
}


/**
 * Get Fall-Back-Fonts.
 * @return FallbackIdArray* Fall-Back-Fonts
 * @date 2011/05/20
 */
FallbackIdArray* getFallBackFonts() {
    return gFallbackFonts;
}


