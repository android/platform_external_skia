/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkLinearBitmapPipeline_DEFINED
#define SkLinearBitmapPipeline_DEFINED


#include "SkColor.h"
#include "SkImageInfo.h"
#include "SkMatrix.h"
#include "SkNx.h"
#include "SkShader.h"

class SkLinearBitmapPipeline {
public:
    SkLinearBitmapPipeline(
        const SkMatrix& inverse,
        SkFilterQuality filterQuality,
        SkShader::TileMode xTile, SkShader::TileMode yTile,
        float postAlpha,
        const SkPixmap& srcPixmap);
    ~SkLinearBitmapPipeline();

    void shadeSpan4f(int x, int y, SkPM4f* dst, int count);

    template<typename Base, size_t kSize>
    class PolymorphicUnion {
    public:
        PolymorphicUnion() : fIsInitialized{false} {}

        ~PolymorphicUnion() {
            if (fIsInitialized) {
                this->get()->~Base();
            }
        }

        template<typename Variant, typename... Args>
        void Initialize(Args&&... args) {
            SkASSERTF(sizeof(Variant) <= sizeof(fSpace),
                      "Size Variant: %d, Space: %d", sizeof(Variant), sizeof(fSpace));

            new(&fSpace) Variant(std::forward<Args>(args)...);
            fIsInitialized = true;
        };

        Base* get() const { return reinterpret_cast<Base*>(&fSpace); }
        Base* operator->() const { return this->get(); }
        Base& operator*() const { return *(this->get()); }

    private:
        struct SK_STRUCT_ALIGN(16) Space {
            char space[kSize];
        };
        bool fIsInitialized;
        mutable Space fSpace;
    };

    class PointProcessorInterface;
    class BilerpProcessorInterface;
    class PixelPlacerInterface;

    // These values were generated by the assert above in PolymorphicUnion.
    using MatrixStage = PolymorphicUnion<PointProcessorInterface, 160>;
    using TileStage   = PolymorphicUnion<PointProcessorInterface, 160>;
    using SampleStage = PolymorphicUnion<BilerpProcessorInterface,100>;
    using PixelStage  = PolymorphicUnion<PixelPlacerInterface,     80>;

private:
    PointProcessorInterface* fFirstStage;
    MatrixStage fMatrixStage;
    TileStage   fTiler;
    SampleStage fSampleStage;
    PixelStage  fPixelStage;
};

#endif  // SkLinearBitmapPipeline_DEFINED
