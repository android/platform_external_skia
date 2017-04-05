/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkJpegUtility_codec_DEFINED
#define SkJpegUtility_codec_DEFINED

#include "SkStream.h"

#include <setjmp.h>
// stdio is needed for jpeglib
#include <stdio.h>

extern "C" {
    #include "jpeglib.h"
    #include "jerror.h"
}

#include "SkUniquePtr.h"

/*
 * Error handling struct
 */
struct skjpeg_error_mgr : jpeg_error_mgr {
    jmp_buf fJmpBuf;
};

/*
 * Error handling function
 */
void skjpeg_err_exit(j_common_ptr cinfo);

/*
 * Source handling struct for that allows libjpeg to use our stream object
 */
struct skjpeg_source_mgr : jpeg_source_mgr {
    skjpeg_source_mgr(SkStream* stream);

    SkStream* fStream; // unowned
    size_t nBufferSize = 0;
    skstd::unique_ptr<char[]> fBuffer;
};

#endif
