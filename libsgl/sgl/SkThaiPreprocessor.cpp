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

SkUnichar SkThaiPreprocessor::fixThaiVowel16(const char** text) {
    ch = SkUTF16_NextUnichar((const uint16_t**)text);

    // Guard condition detecting 'ch' in Thai character range; for improving speed
    if((ch & THAI_CHAR_RANGE_MASK) != THAI_CHAR_RANGE_MASK) return ch;

    ch1 = 0;
    if(isDownTail(ch) || isUpperLevel2(ch)) { 
        ch1 = SkUTF16_NextUnichar((const uint16_t**)text); 
        SkUTF16_PrevUnichar((const uint16_t**)text); 
    }
    SkUnichar result = fixThaiVowel();
    pch0 = pch;
    pch = ch;
    return result;
}

SkUnichar SkThaiPreprocessor::fixThaiVowel8(const char** text) {
    ch = SkUTF8_NextUnichar(text);

    // Guard condition detecting 'ch' in Thai character range; for improving speed
    if((ch & THAI_CHAR_RANGE_MASK) != THAI_CHAR_RANGE_MASK) return ch;

    ch1 = 0;
    if(isDownTail(ch) || isUpperLevel2(ch)) {
        ch1 = SkUTF8_NextUnichar(text);
        SkUTF8_PrevUnichar(text);
    }
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
        SkUnichar temp_r_pch = r_pch;
        
        if (isLowerLevel(pch)) {
            temp_pch = pch0;
            temp_r_pch = r_pch0;
        }

        if (ch1 == SARA_AM) {
            if (isUpTail(temp_pch))
                result = shiftLeft(ch);
            else
                result = ch;
        } else if (isUpTail(temp_pch)) {
            result = pullDownAndShiftLeft(ch);
        } else if (isLeftShiftUpperLevel1(temp_r_pch)) {
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
    } else if (isLowerLevel(ch) && !(isCutTail(r_pch))) {
        result = pullDown(ch);
    }

    r_pch0 = r_pch;
    r_pch = result;
    return result;
}
