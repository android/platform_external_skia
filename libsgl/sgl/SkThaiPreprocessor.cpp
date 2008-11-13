/* libsgl/sgl/SkThaiPreprocessor.cpp
**
** Copyright 2008, The Android Open Source Project
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

#include "SkThaiPreprocessor.h"
#include "SkUtils.h"

#define KO_KAI 0x0E01 //The first Thai letter in the unicode table
#define FIRST_THAI_LETTER KO_KAI
#define KHOMUT 0x0EB5 //The last Thai letter in the unicode table
#define LAST_THAI_LETTER KHOMUT
#define isNotInRange(a, b, c) ((a<b)||(a>c)) 
#define isNotThaiLetter(ch) isNotInRange(ch, FIRST_THAI_LETTER, LAST_THAI_LETTER)

SkUnichar SkThaiPreprocessor::fixThaiVowel16(const char** text) {
    ch = SkUTF16_NextUnichar((const uint16_t**)text);

    // Guard condition detecting 'ch' in Thai character range; for improving speed
    if(isNotThaiLetter(ch)) return ch;

    ch1 = 0;
    if(isDownTail(ch) || isUpperLevel2(ch)) { ch1 = SkUTF16_NextUnichar((const uint16_t**)text); SkUTF16_PrevUnichar((const uint16_t**)text); }
    SkUnichar result = fixThaiVowel();
    pch0 = pch;
    pch = ch;
    return result;
}

SkUnichar SkThaiPreprocessor::fixThaiVowel8(const char** text) {
    ch = SkUTF8_NextUnichar(text);

    // Guard condition detecting 'ch' in Thai character range; for improving speed
    if(isNotThaiLetter(ch)) return ch;

    ch1 = 0;
    if(isDownTail(ch) || isUpperLevel2(ch)) { ch1 = SkUTF8_NextUnichar(text); SkUTF8_PrevUnichar(text); }
    SkUnichar result = fixThaiVowel();
    pch0 = pch;
    pch = ch;
    return result;
}

SkUnichar SkThaiPreprocessor::fixThaiVowel() {
    SkUnichar result = ch;
    if (isUpperLevel1(ch)) {
        SkUnichar temp_pch = pch;
        if (isUpperLevel2(pch)) {
            temp_pch = pch0;
        }
        if (isUpTail(temp_pch)) {
            // Level 1 and up-tail
            result = shiftLeft(ch);
        }
    } else if (isUpperLevel2(ch)) {
        // Level 2
        SkUnichar temp_pch = pch;
        if (isLowerLevel(pch)) {
            temp_pch = pch0;
        }

        if (ch1 == SARA_AM) {
            if (isUpTail(temp_pch))
                result = shiftLeft(ch);
            else
                result = ch;
        } else if (isUpTail(temp_pch)) {
            result = pullDownAndShiftLeft(ch);
        } else if (isLeftShiftUpperLevel1(temp_pch)) {
            result = shiftLeft(ch);
        } else if (!isUpperLevel1(temp_pch)) {
            result = pullDown(ch);
        }

    } else if (isDownTail(ch)) { // Lower level and down-tail
        if(isLowerLevel(ch1)) {
            SkUnichar cutch = cutTail(ch);
            if (ch != cutch) {
                result = cutch;
            }
        }
    } else if (isLowerLevel(ch) && !(isCutTail(pch))) {
        result = pullDown(ch);
    }
    return result;
}
