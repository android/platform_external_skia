/*
* Copyright (c) 2014 Intel Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef SKIMAGECODEC_VENDOR_H
#define SKIMAGECODEC_VENDOR_H

#include "SkImageDecoder.h"
#include "SkImageEncoder.h"
#include "SkTRegistry.h"

class SkVendorImageCodec
{
public:
	static SkVendorImageCodec& getInstance();
    virtual ~SkVendorImageCodec();
    const SkImageDecoder_DecodeReg* decodeRegHead() const;
    const SkImageEncoder_EncodeReg* encodeRegHead() const;
private:
    SkVendorImageCodec();
    SkVendorImageCodec(const SkVendorImageCodec& other);
    SkVendorImageCodec& operator=(const SkVendorImageCodec& other);
    void * mLibHandle;
};
#endif
