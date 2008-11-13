/* libsgl/sgl/SkThaiPreprocessor.h
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

#ifndef SkThaiPreprocessor_DEFINED
#define SkThaiPreprocessor_DEFINED

#include "SkGlyphCache.h"
#include "SkFontHost.h"
#include "SkPaint.h"
#include "SkTemplates.h"

// Lower level SkUnicharacters
#define SARA_U  0xE38
#define SARA_UU  0xE39
#define PHINTHU  0xE3A

// Lower level SkUnicharacters after pullDown
#define SARA_U_DOWN  0xF718
#define SARA_UU_DOWN  0xF719
#define PHINTHU_DOWN  0xF71A

// Upper level 1 SkUnicharacters
#define MAI_HAN_AKAT  0xE31
#define SARA_AM  0xE33
#define SARA_I  0xE34
#define SARA_II  0xE35
#define SARA_UE  0xE36
#define SARA_UEE  0xE37
#define MAI_TAI_KHU  0xE47

// Upper level 1 SkUnicharacters after shift left
#define MAI_HAN_AKAT_LEFT_SHIFT  0xF710
#define SARA_I_LEFT_SHIFT  0xF701
#define SARA_II_LEFT_SHIFT  0xF702
#define SARA_UE_LEFT_SHIFT  0xF703
#define SARA_UEE_LEFT_SHIFT  0xF704
#define MAI_TAI_KHU_LEFT_SHIFT  0xF712

// Upper level 2 SkUnicharacters
#define MAI_EK  0xE48
#define MAI_THO  0xE49
#define MAI_TRI  0xE4A
#define MAI_CHATTAWA  0xE4B
#define THANTHAKHAT  0xE4C
#define NIKHAHIT  0xE4D

// Upper level 2 SkUnicharacters after pull down
#define MAI_EK_DOWN  0xF70A
#define MAI_THO_DOWN  0xF70B
#define MAI_TRI_DOWN  0xF70C
#define MAI_CHATTAWA_DOWN  0xF70D
#define THANTHAKHAT_DOWN  0xF70E

// Upper level 2 SkUnicharacters after pull down and shift left
#define MAI_EK_PULL_DOWN_AND_LEFT_SHIFT  0xF705
#define MAI_THO_PULL_DOWN_AND_LEFT_SHIFT  0xF706
#define MAI_TRI_PULL_DOWN_AND_LEFT_SHIFT  0xF707
#define MAI_CHATTAWA_PULL_DOWN_AND_LEFT_SHIFT  0xF708
#define THANTHAKHAT_PULL_DOWN_AND_LEFT_SHIFT  0xF709

// Upper level 2 SkUnicharacters after shift left
#define MAI_EK_LEFT_SHIFT  0xF713
#define MAI_THO_LEFT_SHIFT  0xF714
#define MAI_TRI_LEFT_SHIFT  0xF715
#define MAI_CHATTAWA_LEFT_SHIFT  0xF716
#define THANTHAKHAT_LEFT_SHIFT  0xF717
#define NIKHAHIT_LEFT_SHIFT  0xF711
#define SARA_AM_LEFT_SHIFT  0xF71F

// Up tail SkUnicharacters
#define PO_PLA  0x0E1B
#define FO_FA  0x0E1D
#define FO_FAN  0x0E1F
#define LO_CHULA  0x0E2C

// Down tail SkUnicharacters
#define THO_THAN  0xE10
#define YO_YING  0xE0D
#define DO_CHADA  0xE0E
#define TO_PATAK  0xE0F
#define RU  0xE24
#define LU  0xE26

// Cut tail SkUnicharacters
#define THO_THAN_CUT_TAIL  0xF700
#define YO_YING_CUT_TAIL  0xF70F

class SkThaiPreprocessor {

private:
    SkUnichar pch0;
    SkUnichar pch;
    SkUnichar ch;
    SkUnichar ch1;

    bool isUpTail(SkUnichar ch) {
        return  ch == PO_PLA ||
            ch == FO_FA ||
            ch == FO_FAN ||
            ch == LO_CHULA;
    }

    bool isDownTail(SkUnichar ch) {
        return  ch == THO_THAN ||
            ch == YO_YING ||
            ch == DO_CHADA  ||
            ch == TO_PATAK ||
            ch == RU ||
            ch == LU;
    }

    bool isUpperLevel1(SkUnichar ch) {
        return  ch == MAI_HAN_AKAT ||
            ch == SARA_I ||
            ch == SARA_II ||
            ch == SARA_UE ||
            ch == SARA_UEE ||
            ch == MAI_TAI_KHU ||
            ch == NIKHAHIT ||
            ch == SARA_AM;
    }

    bool isLeftShiftUpperLevel1(SkUnichar ch) {
        return  ch == MAI_HAN_AKAT_LEFT_SHIFT ||
            ch == SARA_I_LEFT_SHIFT ||
            ch == SARA_II_LEFT_SHIFT ||
            ch == SARA_UE_LEFT_SHIFT ||
            ch == SARA_UEE_LEFT_SHIFT ||
            ch == MAI_TAI_KHU_LEFT_SHIFT ||
            ch == NIKHAHIT_LEFT_SHIFT ||
            ch == SARA_AM_LEFT_SHIFT;
    }

    bool isUpperLevel2(SkUnichar ch) {
        return  ch == MAI_EK ||
            ch == MAI_THO ||
            ch == MAI_TRI ||
            ch == MAI_CHATTAWA ||
            ch == THANTHAKHAT;
    }

    bool isLowerLevel(SkUnichar ch) {
        return  ch == SARA_U ||
            ch == SARA_UU ||
            ch == PHINTHU;
    }

    SkUnichar pullDownAndShiftLeft(SkUnichar ch) {
        switch (ch) {
            case MAI_EK:
                return MAI_EK_PULL_DOWN_AND_LEFT_SHIFT;
            case MAI_THO:
                return MAI_THO_PULL_DOWN_AND_LEFT_SHIFT;
            case MAI_TRI:
                return MAI_TRI_PULL_DOWN_AND_LEFT_SHIFT;
            case MAI_CHATTAWA:
                return MAI_CHATTAWA_PULL_DOWN_AND_LEFT_SHIFT;
            case MAI_HAN_AKAT:
                return MAI_HAN_AKAT_LEFT_SHIFT;
            case THANTHAKHAT:
                return THANTHAKHAT_PULL_DOWN_AND_LEFT_SHIFT;
            default:
                return ch;
        }
    }

    SkUnichar shiftLeft(SkUnichar ch) {
        switch (ch) {
            case MAI_EK:
                return MAI_EK_LEFT_SHIFT;
            case MAI_THO:
                return MAI_THO_LEFT_SHIFT;
            case MAI_TRI:
                return MAI_TRI_LEFT_SHIFT;
            case MAI_CHATTAWA:
                return MAI_CHATTAWA_LEFT_SHIFT;
            case MAI_HAN_AKAT:
                return MAI_HAN_AKAT_LEFT_SHIFT;
            case SARA_I:
                return SARA_I_LEFT_SHIFT;
            case SARA_II:
                return SARA_II_LEFT_SHIFT;
            case SARA_UE:
                return SARA_UE_LEFT_SHIFT;
            case SARA_UEE:
                return SARA_UEE_LEFT_SHIFT;
            case MAI_TAI_KHU:
                return MAI_TAI_KHU_LEFT_SHIFT;
            case NIKHAHIT:
                return NIKHAHIT_LEFT_SHIFT;
            case SARA_AM:
                return SARA_AM_LEFT_SHIFT;
            default:
                return ch;
        }
    }

    SkUnichar pullDown(SkUnichar ch) {
        switch (ch) {
            case MAI_EK:
                return MAI_EK_DOWN;
            case MAI_THO:
                return MAI_THO_DOWN;
            case MAI_TRI:
                return MAI_TRI_DOWN;
            case MAI_CHATTAWA:
                return MAI_CHATTAWA_DOWN;
            case THANTHAKHAT:
                return THANTHAKHAT_DOWN;
            case SARA_U:
                return SARA_U_DOWN;
            case SARA_UU:
                return SARA_UU_DOWN;
            case PHINTHU:
                return PHINTHU_DOWN;
            default:
                return ch;
        }
    }

    bool isCutTail(SkUnichar ch) {
        return ch == THO_THAN_CUT_TAIL || ch == YO_YING_CUT_TAIL;
    }

    SkUnichar cutTail(SkUnichar ch) {
        switch(ch) {
            case THO_THAN:
                return THO_THAN_CUT_TAIL;
            case YO_YING:
                return YO_YING_CUT_TAIL;
            default:
                return ch;
        }
    }

private:
    SkUnichar fixThaiVowel();

public:
    SkUnichar fixThaiVowel8(const char** text);
    SkUnichar fixThaiVowel16(const char** text);

};

#endif
