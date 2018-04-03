/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSurfaceCharacterization_DEFINED
#define SkSurfaceCharacterization_DEFINED

#include "GrTypes.h"

#include "SkColorSpace.h"
#include "SkRefCnt.h"
#include "SkSurfaceProps.h"

class SkColorSpace;

#if SK_SUPPORT_GPU
#include "GrContext.h"

/** \class SkSurfaceCharacterization
    A surface characterization contains all the information Ganesh requires to makes its internal
    rendering decisions. When passed into a SkDeferredDisplayListRecorder it will copy the
    data and pass it on to the SkDeferredDisplayList if/when it is created. Note that both of
    those objects (the Recorder and the DisplayList) will take a ref on the
    GrContextThreadSafeProxy and SkColorSpace objects.
*/
class SkSurfaceCharacterization {
public:
    enum class Textureable : bool { kNo = false, kYes = true };
    enum class MipMapped : bool { kNo = false, kYes = true };

    SkSurfaceCharacterization()
            : fCacheMaxResourceBytes(0)
            , fOrigin(kBottomLeft_GrSurfaceOrigin)
            , fConfig(kUnknown_GrPixelConfig)
            , fFSAAType(GrFSAAType::kNone)
            , fStencilCnt(0)
            , fIsTextureable(Textureable::kYes)
            , fIsMipMapped(MipMapped::kYes)
            , fSurfaceProps(0, kUnknown_SkPixelGeometry) {
    }

    SkSurfaceCharacterization(SkSurfaceCharacterization&&) = default;
    SkSurfaceCharacterization& operator=(SkSurfaceCharacterization&&) = default;

    SkSurfaceCharacterization(const SkSurfaceCharacterization&) = default;
    SkSurfaceCharacterization& operator=(const SkSurfaceCharacterization& other) = default;

    SkSurfaceCharacterization createResized(int width, int height) const {
        const GrCaps* caps = fContextInfo->caps();
        if (!caps) {
            return SkSurfaceCharacterization();
        }

        if (width <= 0 || height <= 0 ||
            width > caps->maxRenderTargetSize() || height > caps->maxRenderTargetSize()) {
            return SkSurfaceCharacterization();
        }

        return SkSurfaceCharacterization(fContextInfo,
                                         fCacheMaxResourceBytes,
                                         fImageInfo.makeWH(width, height),
                                         fOrigin, fConfig, fFSAAType, fStencilCnt,
                                         fIsTextureable, fIsMipMapped,
                                         fSurfaceProps);
    }

    GrContextThreadSafeProxy* contextInfo() const { return fContextInfo.get(); }
    sk_sp<GrContextThreadSafeProxy> refContextInfo() const { return fContextInfo; }
    size_t cacheMaxResourceBytes() const { return fCacheMaxResourceBytes; }

    bool isValid() const { return kUnknown_SkColorType != fImageInfo.colorType(); }

    const SkImageInfo& imageInfo() const { return fImageInfo; }
    GrSurfaceOrigin origin() const { return fOrigin; }
    int width() const { return fImageInfo.width(); }
    int height() const { return fImageInfo.height(); }
    SkColorType colorType() const { return fImageInfo.colorType(); }
    GrFSAAType fsaaType() const { return fFSAAType; }
    int stencilCount() const { return fStencilCnt; }
    bool isTextureable() const { return Textureable::kYes == fIsTextureable; }
    bool isMipMapped() const { return MipMapped::kYes == fIsMipMapped; }
    SkColorSpace* colorSpace() const { return fImageInfo.colorSpace(); }
    sk_sp<SkColorSpace> refColorSpace() const { return fImageInfo.refColorSpace(); }
    const SkSurfaceProps& surfaceProps()const { return fSurfaceProps; }

private:
    friend class SkSurface_Gpu; // for 'set' & 'config'
    friend class GrContextThreadSafeProxy; // for private ctor
    friend class SkDeferredDisplayListRecorder; // for 'config'
    friend class SkSurface; // for 'config'

    GrPixelConfig config() const { return fConfig; }

    SkSurfaceCharacterization(sk_sp<GrContextThreadSafeProxy> contextInfo,
                              size_t cacheMaxResourceBytes,
                              const SkImageInfo& ii,
                              GrSurfaceOrigin origin,
                              GrPixelConfig config,
                              GrFSAAType FSAAType, int stencilCnt,
                              Textureable isTextureable, MipMapped isMipMapped,
                              const SkSurfaceProps& surfaceProps)
            : fContextInfo(std::move(contextInfo))
            , fCacheMaxResourceBytes(cacheMaxResourceBytes)
            , fImageInfo(ii)
            , fOrigin(origin)
            , fConfig(config)
            , fFSAAType(FSAAType)
            , fStencilCnt(stencilCnt)
            , fIsTextureable(isTextureable)
            , fIsMipMapped(isMipMapped)
            , fSurfaceProps(surfaceProps) {
    }

    void set(sk_sp<GrContextThreadSafeProxy> contextInfo,
             size_t cacheMaxResourceBytes,
             const SkImageInfo& ii,
             GrSurfaceOrigin origin,
             GrPixelConfig config,
             GrFSAAType fsaaType,
             int stencilCnt,
             Textureable isTextureable,
             MipMapped isMipMapped,
             const SkSurfaceProps& surfaceProps) {
        SkASSERT(MipMapped::kNo == isMipMapped || Textureable::kYes == isTextureable);

        fContextInfo = contextInfo;
        fCacheMaxResourceBytes = cacheMaxResourceBytes;

        fImageInfo = ii;
        fOrigin = origin;
        fConfig = config;
        fFSAAType = fsaaType;
        fStencilCnt = stencilCnt;
        fIsTextureable = isTextureable;
        fIsMipMapped = isMipMapped;
        fSurfaceProps = surfaceProps;
    }

    sk_sp<GrContextThreadSafeProxy> fContextInfo;
    size_t                          fCacheMaxResourceBytes;

    SkImageInfo                     fImageInfo;
    GrSurfaceOrigin                 fOrigin;
    GrPixelConfig                   fConfig;
    GrFSAAType                      fFSAAType;
    int                             fStencilCnt;
    Textureable                     fIsTextureable;
    MipMapped                       fIsMipMapped;
    SkSurfaceProps                  fSurfaceProps;
};

#else// !SK_SUPPORT_GPU

class SkSurfaceCharacterization {
public:
    SkSurfaceCharacterization() : fSurfaceProps(0, kUnknown_SkPixelGeometry) { }

    SkSurfaceCharacterization createResized(int width, int height) const {
        return *this;
    }

    size_t cacheMaxResourceBytes() const { return 0; }

    bool isValid() const { return false; }

    int width() const { return 0; }
    int height() const { return 0; }
    int stencilCount() const { return 0; }
    bool isTextureable() const { return false; }
    bool isMipMapped() const { return false; }
    SkColorSpace* colorSpace() const { return nullptr; }
    sk_sp<SkColorSpace> refColorSpace() const { return nullptr; }
    const SkSurfaceProps& surfaceProps()const { return fSurfaceProps; }

private:
    SkSurfaceProps fSurfaceProps;
};

#endif

#endif
