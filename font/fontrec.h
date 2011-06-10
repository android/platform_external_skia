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

#include <stdio.h>
#include <stdbool.h>

// used to record our notion of the pre-existing fonts
struct FontInitRec {
    const char*         fFileName;
    const char* const*  fNames;     // null-terminated list
    const int           fUseFallbackFontsEx;
    const bool          fHide;
};

#define INIT_REC_COUNT 19

typedef uint32_t FallbackIdArray[INIT_REC_COUNT + 1];

#ifdef __cplusplus
extern "C"
{
#endif
extern const char* gSample1Names[];
extern const char* gSample2Names[];
extern const char* gCustomNames[];
extern const char* gSansNames[];
extern const char* gSerifNames[];
extern const char* gMonoNames[];
extern const char* gFBNames[];

extern const struct FontInitRec* getFontInitRec();
extern FallbackIdArray* getFallBackFonts();
#ifdef __cplusplus
}
#endif

