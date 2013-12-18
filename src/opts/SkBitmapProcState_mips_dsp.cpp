/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "SkBitmapProcState.h"
#include "SkBitmapProcState_filter.h"
#include "SkColorPriv.h"
#include "SkFilterProc.h"
#include "SkPaint.h"
#include "SkShader.h"   // for tilemodes
#include "SkUtilsMips.h"

// Required to ensure the table is part of the final binary.
extern const SkBitmapProcState::SampleProc32 gSkBitmapProcStateSample32_mips_dsp[];
extern const SkBitmapProcState::SampleProc16 gSkBitmapProcStateSample16_mips_dsp[];

#define   NAME_WRAP(x)  x ## _mips_dsp
#include "SkBitmapProcState_filter_mips_dsp.h"
#include "SkBitmapProcState_procs.h"

const SkBitmapProcState::SampleProc32 gSkBitmapProcStateSample32_mips_dsp[] = {
    S32_opaque_D32_nofilter_DXDY_mips_dsp,
    S32_alpha_D32_nofilter_DXDY_mips_dsp,
    S32_opaque_D32_nofilter_DX_mips_dsp,
    S32_alpha_D32_nofilter_DX_mips_dsp,
    S32_opaque_D32_filter_DXDY_mips_dsp,
    S32_alpha_D32_filter_DXDY_mips_dsp,
    S32_opaque_D32_filter_DX_mips_dsp,
    S32_alpha_D32_filter_DX_mips_dsp,

    S16_opaque_D32_nofilter_DXDY_mips_dsp,
    S16_alpha_D32_nofilter_DXDY_mips_dsp,
    S16_opaque_D32_nofilter_DX_mips_dsp,
    S16_alpha_D32_nofilter_DX_mips_dsp,
    S16_opaque_D32_filter_DXDY_mips_dsp,
    S16_alpha_D32_filter_DXDY_mips_dsp,
    S16_opaque_D32_filter_DX_mips_dsp,
    S16_alpha_D32_filter_DX_mips_dsp,

    SI8_opaque_D32_nofilter_DXDY_mips_dsp,
    SI8_alpha_D32_nofilter_DXDY_mips_dsp,
    SI8_opaque_D32_nofilter_DX_mips_dsp,
    SI8_alpha_D32_nofilter_DX_mips_dsp,
    SI8_opaque_D32_filter_DXDY_mips_dsp,
    SI8_alpha_D32_filter_DXDY_mips_dsp,
    SI8_opaque_D32_filter_DX_mips_dsp,
    SI8_alpha_D32_filter_DX_mips_dsp,

    S4444_opaque_D32_nofilter_DXDY_mips_dsp,
    S4444_alpha_D32_nofilter_DXDY_mips_dsp,
    S4444_opaque_D32_nofilter_DX_mips_dsp,
    S4444_alpha_D32_nofilter_DX_mips_dsp,
    S4444_opaque_D32_filter_DXDY_mips_dsp,
    S4444_alpha_D32_filter_DXDY_mips_dsp,
    S4444_opaque_D32_filter_DX_mips_dsp,
    S4444_alpha_D32_filter_DX_mips_dsp,

    // A8 treats alpha/opauqe the same (equally efficient)
    SA8_alpha_D32_nofilter_DXDY_mips_dsp,
    SA8_alpha_D32_nofilter_DXDY_mips_dsp,
    SA8_alpha_D32_nofilter_DX_mips_dsp,
    SA8_alpha_D32_nofilter_DX_mips_dsp,
    SA8_alpha_D32_filter_DXDY_mips_dsp,
    SA8_alpha_D32_filter_DXDY_mips_dsp,
    SA8_alpha_D32_filter_DX_mips_dsp,
    SA8_alpha_D32_filter_DX_mips_dsp
};

const SkBitmapProcState::SampleProc16 gSkBitmapProcStateSample16_mips_dsp[] = {
    S32_D16_nofilter_DXDY_mips_dsp,
    S32_D16_nofilter_DX_mips_dsp,
    S32_D16_filter_DXDY_mips_dsp,
    S32_D16_filter_DX_mips_dsp,

    S16_D16_nofilter_DXDY_mips_dsp,
    S16_D16_nofilter_DX_mips_dsp,
    S16_D16_filter_DXDY_mips_dsp,
    S16_D16_filter_DX_mips_dsp,

    SI8_D16_nofilter_DXDY_mips_dsp,
    SI8_D16_nofilter_DX_mips_dsp,
    SI8_D16_filter_DXDY_mips_dsp,
    SI8_D16_filter_DX_mips_dsp,

    // Don't support 4444 -> 565
    NULL, NULL, NULL, NULL,
    // Don't support A8 -> 565
    NULL, NULL, NULL, NULL
};
