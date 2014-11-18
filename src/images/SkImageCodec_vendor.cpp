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

#include "SkImageCodec_vendor.h"
#if defined(SK_BUILD_FOR_ANDROID) || defined(SK_BUILD_FOR_UNIX)
#include <dlfcn.h>
#endif

#if defined(SK_BUILD_FOR_ANDROID)
const char * VENDORLIBNAME = "/system/lib/libskiaimagehw.so";
#elif defined(SK_BUILD_FOR_UNIX)
const char * VENDORLIBNAME = "/usr/lib/libskiaimagehw.so";
#endif

SkVendorImageCodec::SkVendorImageCodec()
    : mLibHandle(NULL)
{
#if defined(SK_BUILD_FOR_ANDROID) || defined(SK_BUILD_FOR_UNIX)
    mLibHandle = dlopen(VENDORLIBNAME, RTLD_NOW | RTLD_GLOBAL);
#else
    SkDebugf("SkVendorImageCodec plugin only support Android and Linux now\n");
#endif
}

SkVendorImageCodec::~SkVendorImageCodec()
{
    if (mLibHandle) {
#if defined(SK_BUILD_FOR_ANDROID) || defined(SK_BUILD_FOR_UNIX)
        dlclose(mLibHandle);
#endif
    }
}

SkVendorImageCodec& SkVendorImageCodec::getInstance()
{
    static SkVendorImageCodec inst;
    return inst;
}

const SkImageDecoder_DecodeReg* SkVendorImageCodec::decodeRegHead() const
{
    typedef const SkImageDecoder_DecodeReg* (*pfnGetDecodeRegHead)(void);
    pfnGetDecodeRegHead pfn = NULL;
    if (mLibHandle) {
#if defined(SK_BUILD_FOR_ANDROID) || defined(SK_BUILD_FOR_UNIX)
        pfn = (pfnGetDecodeRegHead)dlsym(mLibHandle, "getDecodeRegHead");
#endif
        if (pfn)
            return (*pfn)();
    }
    return NULL;
}

const SkImageEncoder_EncodeReg* SkVendorImageCodec::encodeRegHead() const
{
    typedef const SkImageEncoder_EncodeReg* (*pfnGetEncodeRegHead)(void);
    pfnGetEncodeRegHead pfn = NULL;
    if (mLibHandle) {
#if defined(SK_BUILD_FOR_ANDROID) || defined(SK_BUILD_FOR_UNIX)
        pfn = (pfnGetEncodeRegHead)dlsym(mLibHandle, "getEncodeRegHead");
#endif
        if (pfn)
            return (*pfn)();
    }
    return NULL;
}
