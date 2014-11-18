
/*
 * Copyright 2009 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkImageEncoder.h"
#include "SkImageCodec_vendor.h"

template SkImageEncoder_EncodeReg* SkImageEncoder_EncodeReg::gHead;

SkImageEncoder* SkImageEncoder::Create(Type t) {
    SkImageEncoder* codec = NULL;
    bool end_of_vendor = false;
    const SkImageEncoder_EncodeReg* curr = SkVendorImageCodec::getInstance().encodeRegHead();
    while (curr || !end_of_vendor) {
        if (!curr && !end_of_vendor) {
            curr = SkImageEncoder_EncodeReg::Head();
            end_of_vendor = true;
            continue;
        }
        if ((codec = curr->factory()(t)) != NULL) {
            return codec;
        }
        curr = curr->next();
    }
    return NULL;
}
