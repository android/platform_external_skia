/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrCCCoverageProcessor.h"

#include "GrMesh.h"
#include "glsl/GrGLSLVertexGeoBuilder.h"

using InputType = GrGLSLGeometryBuilder::InputType;
using OutputType = GrGLSLGeometryBuilder::OutputType;

/**
 * This class and its subclasses implement the coverage processor with geometry shaders.
 */
class GrCCCoverageProcessor::GSImpl : public GrGLSLGeometryProcessor {
protected:
    GSImpl(std::unique_ptr<Shader> shader) : fShader(std::move(shader)) {}

    virtual bool hasCoverage() const { return false; }
    virtual bool hasAttenuatedCoverage() const { return false; }

    void setData(const GrGLSLProgramDataManager& pdman, const GrPrimitiveProcessor&,
                 FPCoordTransformIter&& transformIter) final {
        this->setTransformDataHelper(SkMatrix::I(), pdman, &transformIter);
    }

    void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) final {
        const GrCCCoverageProcessor& proc = args.fGP.cast<GrCCCoverageProcessor>();

        // The vertex shader simply forwards transposed x or y values to the geometry shader.
        SkASSERT(1 == proc.numAttribs());
        gpArgs->fPositionVar.set(GrVertexAttribTypeToSLType(proc.getAttrib(0).fType),
                                 proc.getAttrib(0).fName);

        // Geometry shader.
        GrGLSLVaryingHandler* varyingHandler = args.fVaryingHandler;
        this->emitGeometryShader(proc, varyingHandler, args.fGeomBuilder, args.fRTAdjustName);
        varyingHandler->emitAttributes(proc);
        varyingHandler->setNoPerspective();
        SkASSERT(!args.fFPCoordTransformHandler->nextCoordTransform());

        // Fragment shader.
        fShader->emitFragmentCode(proc, args.fFragBuilder, args.fOutputColor, args.fOutputCoverage);
    }

    void emitGeometryShader(const GrCCCoverageProcessor& proc,
                            GrGLSLVaryingHandler* varyingHandler, GrGLSLGeometryBuilder* g,
                            const char* rtAdjust) const {
        int numInputPoints = proc.numInputPoints();
        SkASSERT(3 == numInputPoints || 4 == numInputPoints);

        const char* posValues = (4 == numInputPoints) ? "sk_Position" : "sk_Position.xyz";
        g->codeAppendf("float%ix2 pts = transpose(float2x%i(sk_in[0].%s, sk_in[1].%s));",
                       numInputPoints, numInputPoints, posValues, posValues);

        GrShaderVar wind("wind", kHalf_GrSLType);
        g->declareGlobal(wind);
        if (WindMethod::kCrossProduct == proc.fWindMethod) {
            g->codeAppend ("float area_x2 = determinant(float2x2(pts[0] - pts[1], "
                                                                "pts[0] - pts[2]));");
            if (4 == numInputPoints) {
                g->codeAppend ("area_x2 += determinant(float2x2(pts[0] - pts[2], "
                                                               "pts[0] - pts[3]));");
            }
            g->codeAppendf("%s = sign(area_x2);", wind.c_str());
        } else {
            SkASSERT(WindMethod::kInstanceData == proc.fWindMethod);
            SkASSERT(3 == numInputPoints);
            SkASSERT(kFloat4_GrVertexAttribType == proc.getAttrib(0).fType);
            g->codeAppendf("%s = sk_in[0].sk_Position.w;", wind.c_str());
        }

        SkString emitVertexFn;
        SkSTArray<2, GrShaderVar> emitArgs;
        const char* position = emitArgs.emplace_back("position", kFloat2_GrSLType).c_str();
        const char* coverage = nullptr;
        if (this->hasCoverage()) {
            coverage = emitArgs.emplace_back("coverage", kHalf_GrSLType).c_str();
        }
        const char* attenuatedCoverage = nullptr;
        if (this->hasAttenuatedCoverage()) {
            attenuatedCoverage = emitArgs.emplace_back("attenuated_coverage",
                                                       kHalf2_GrSLType).c_str();
        }
        g->emitFunction(kVoid_GrSLType, "emitVertex", emitArgs.count(), emitArgs.begin(), [&]() {
            SkString fnBody;
            if (coverage) {
                fnBody.appendf("%s *= %s;", coverage, wind.c_str());
            }
            if (attenuatedCoverage) {
                fnBody.appendf("%s.x *= %s;", attenuatedCoverage, wind.c_str());
            }
            fShader->emitVaryings(varyingHandler, GrGLSLVarying::Scope::kGeoToFrag, &fnBody,
                                  position, coverage ? coverage : wind.c_str(), attenuatedCoverage);
            g->emitVertex(&fnBody, position, rtAdjust);
            return fnBody;
        }().c_str(), &emitVertexFn);

        float bloat = kAABloatRadius;
#ifdef SK_DEBUG
        if (proc.debugBloatEnabled()) {
            bloat *= proc.debugBloat();
        }
#endif
        g->defineConstant("bloat", bloat);

        this->onEmitGeometryShader(proc, g, wind, emitVertexFn.c_str());
    }

    virtual void onEmitGeometryShader(const GrCCCoverageProcessor&, GrGLSLGeometryBuilder*,
                                      const GrShaderVar& wind, const char* emitVertexFn) const = 0;

    virtual ~GSImpl() {}

    const std::unique_ptr<Shader> fShader;

    typedef GrGLSLGeometryProcessor INHERITED;
};

/**
 * Generates conservative rasters around a triangle and its edges, and calculates coverage ramps.
 *
 * Triangle rough outlines are drawn in two steps: (1) draw a conservative raster of the entire
 * triangle, with a coverage of +1, and (2) draw conservative rasters around each edge, with a
 * coverage ramp from -1 to 0. These edge coverage values convert jagged conservative raster edges
 * into smooth, antialiased ones.
 *
 * The final corners get touched up in a later step by GSTriangleCornerImpl.
 */
class GrCCCoverageProcessor::GSTriangleHullImpl : public GrCCCoverageProcessor::GSImpl {
public:
    GSTriangleHullImpl(std::unique_ptr<Shader> shader) : GSImpl(std::move(shader)) {}

    bool hasCoverage() const override { return true; }

    void onEmitGeometryShader(const GrCCCoverageProcessor&, GrGLSLGeometryBuilder* g,
                              const GrShaderVar& wind, const char* emitVertexFn) const override {
        fShader->emitSetupCode(g, "pts", wind.c_str());

        // Visualize the input triangle as upright and equilateral, with a flat base. Paying special
        // attention to wind, we can identify the points as top, bottom-left, and bottom-right.
        //
        // NOTE: We generate the rasters in 5 independent invocations, so each invocation designates
        // the corner it will begin with as the top.
        g->codeAppendf("int i = (%s > 0 ? sk_InvocationID : 4 - sk_InvocationID) %% 3;",
                       wind.c_str());
        g->codeAppend ("float2 top = pts[i];");
        g->codeAppendf("float2 right = pts[(i + (%s > 0 ? 1 : 2)) %% 3];", wind.c_str());
        g->codeAppendf("float2 left = pts[(i + (%s > 0 ? 2 : 1)) %% 3];", wind.c_str());

        // Determine which direction to outset the conservative raster from each of the three edges.
        g->codeAppend ("float2 leftbloat = sign(top - left);");
        g->codeAppend ("leftbloat = float2(0 != leftbloat.y ? leftbloat.y : leftbloat.x, "
                                          "0 != leftbloat.x ? -leftbloat.x : -leftbloat.y);");

        g->codeAppend ("float2 rightbloat = sign(right - top);");
        g->codeAppend ("rightbloat = float2(0 != rightbloat.y ? rightbloat.y : rightbloat.x, "
                                           "0 != rightbloat.x ? -rightbloat.x : -rightbloat.y);");

        g->codeAppend ("float2 downbloat = sign(left - right);");
        g->codeAppend ("downbloat = float2(0 != downbloat.y ? downbloat.y : downbloat.x, "
                                           "0 != downbloat.x ? -downbloat.x : -downbloat.y);");

        // The triangle's conservative raster has a coverage of +1 all around.
        g->codeAppend ("half4 coverages = half4(+1);");

        // Edges have coverage ramps.
        g->codeAppend ("if (sk_InvocationID >= 2) {"); // Are we an edge?
        Shader::CalcEdgeCoverageAtBloatVertex(g, "top", "right",
                                              "float2(+rightbloat.y, -rightbloat.x)",
                                              "coverages[0]");
        g->codeAppend (    "coverages.yzw = half3(-1, 0, -1 - coverages[0]);");
        // Reassign bloats to characterize a conservative raster around a single edge, rather than
        // the entire triangle.
        g->codeAppend (    "leftbloat = downbloat = -rightbloat;");
        g->codeAppend ("}");

        // These can't be scaled until after we calculate coverage.
        g->codeAppend ("leftbloat *= bloat;");
        g->codeAppend ("rightbloat *= bloat;");
        g->codeAppend ("downbloat *= bloat;");

        // Here we generate the conservative raster geometry. The triangle's conservative raster is
        // the convex hull of 3 pixel-size boxes centered on the input points. This translates to a
        // convex polygon with either one, two, or three vertices at each input point (depending on
        // how sharp the corner is) that we split between two invocations. Edge conservative rasters
        // are convex hulls of 2 pixel-size boxes, one at each endpoint. For more details on
        // conservative raster, see:
        // https://developer.nvidia.com/gpugems/GPUGems2/gpugems2_chapter42.html
        g->codeAppendf("bool2 left_right_notequal = notEqual(leftbloat, rightbloat);");
        g->codeAppend ("if (all(left_right_notequal)) {");
                           // The top corner will have three conservative raster vertices. Emit the
                           // middle one first to the triangle strip.
        g->codeAppendf(    "%s(top + float2(-leftbloat.y, +leftbloat.x), coverages[0]);",
                           emitVertexFn);
        g->codeAppend ("}");
        g->codeAppend ("if (any(left_right_notequal)) {");
                           // Second conservative raster vertex for the top corner.
        g->codeAppendf(    "%s(top + rightbloat, coverages[1]);", emitVertexFn);
        g->codeAppend ("}");

        // Main interior body.
        g->codeAppendf("%s(top + leftbloat, coverages[2]);", emitVertexFn);
        g->codeAppendf("%s(right + rightbloat, coverages[1]);", emitVertexFn);

        // Here the invocations diverge slightly. We can't symmetrically divide three triangle
        // points between two invocations, so each does the following:
        //
        // sk_InvocationID=0: Finishes the main interior body of the triangle hull.
        // sk_InvocationID=1: Remaining two conservative raster vertices for the third hull corner.
        // sk_InvocationID=2..4: Finish the opposite endpoint of their corresponding edge.
        g->codeAppendf("bool2 right_down_notequal = notEqual(rightbloat, downbloat);");
        g->codeAppend ("if (any(right_down_notequal) || 0 == sk_InvocationID) {");
        g->codeAppendf(    "%s(0 == sk_InvocationID ? left + leftbloat : right + downbloat, "
                              "coverages[2]);", emitVertexFn);
        g->codeAppend ("}");
        g->codeAppend ("if (all(right_down_notequal) && 0 != sk_InvocationID) {");
        g->codeAppendf(    "%s(right + float2(-rightbloat.y, +rightbloat.x), coverages[3]);",
                           emitVertexFn);
        g->codeAppend ("}");

        // 5 invocations: 2 triangle hull invocations and 3 edges.
        g->configure(InputType::kLines, OutputType::kTriangleStrip, 6, 5);
    }
};

/**
 * Generates conservative rasters around corners (aka pixel-size boxes) and calculates
 * coverage and attenuation ramps to fix up the coverage values written by the hulls.
 */
class GrCCCoverageProcessor::GSCornerImpl : public GrCCCoverageProcessor::GSImpl {
public:
    GSCornerImpl(std::unique_ptr<Shader> shader) : GSImpl(std::move(shader)) {}

    bool hasCoverage() const override { return true; }
    bool hasAttenuatedCoverage() const override { return true; }

    void onEmitGeometryShader(const GrCCCoverageProcessor& proc, GrGLSLGeometryBuilder* g,
                              const GrShaderVar& wind, const char* emitVertexFn) const override {
        fShader->emitSetupCode(g, "pts", wind.c_str());

        bool isTriangle = RenderPass::kTriangleCorners == proc.fRenderPass;
        g->codeAppendf("int corneridx = sk_InvocationID;");
        if (!isTriangle) {
            g->codeAppendf("corneridx *= %i;", proc.numInputPoints() - 1);
        }

        g->codeAppendf("float2 corner = pts[corneridx];");
        g->codeAppendf("float2 left = pts[(corneridx + (%s > 0 ? %i : 1)) %% %i];",
                       wind.c_str(), proc.numInputPoints() - 1, proc.numInputPoints());
        g->codeAppendf("float2 right = pts[(corneridx + (%s > 0 ? 1 : %i)) %% %i];",
                       wind.c_str(), proc.numInputPoints() - 1, proc.numInputPoints());

        g->codeAppend ("float2 leftdir = corner - left;");
        g->codeAppend ("leftdir = (float2(0) != leftdir) ? normalize(leftdir) : float2(1, 0);");

        g->codeAppend ("float2 rightdir = right - corner;");
        g->codeAppend ("rightdir = (float2(0) != rightdir) ? normalize(rightdir) : float2(1, 0);");

        // Find "outbloat" and "crossbloat" at our corner. The outbloat points diagonally out of the
        // triangle, in the direction that should ramp to zero coverage with attenuation. The
        // crossbloat runs perpindicular to outbloat.
        g->codeAppend ("float2 outbloat = float2(leftdir.x > rightdir.x ? +1 : -1, "
                                                "leftdir.y > rightdir.y ? +1 : -1);");
        g->codeAppend ("float2 crossbloat = float2(-outbloat.y, +outbloat.x);");

        g->codeAppend ("half2 left_coverages; {");
        Shader::CalcEdgeCoveragesAtBloatVertices(g, "left", "corner", "-outbloat", "-crossbloat",
                                                 "left_coverages");
        g->codeAppend ("}");

        g->codeAppend ("half2 right_coverages; {");
        Shader::CalcEdgeCoveragesAtBloatVertices(g, "corner", "right", "-outbloat", "crossbloat",
                                                 "right_coverages");
        g->codeAppend ("}");

        g->codeAppend ("half attenuation; {");
        Shader::CalcCornerCoverageAttenuation(g, "leftdir", "rightdir", "attenuation");
        g->codeAppend ("}");

        // Emit a corner box. The first coverage argument erases the values that were written
        // previously by the hull and edge geometry. The second pair are multiplied together by the
        // fragment shader. They ramp to 0 with attenuation in the direction of outbloat, and
        // linearly from left-edge coverage to right-edge coverage in the direction of crossbloat.
        //
        // NOTE: Since this is not a linear mapping, it is important that the box's diagonal shared
        // edge points in the direction of outbloat.
        g->codeAppendf("%s(corner - crossbloat * bloat, %s, half2(1 + left_coverages[1], 1));",
                       emitVertexFn,
                       // Erase what the hull wrote previously.
                       isTriangle ? "right_coverages[1] - left_coverages[1]" : "-1");

        g->codeAppendf("%s(corner + outbloat * bloat, %s, half2(0, attenuation));",
                       emitVertexFn,
                       // Erase what the hull wrote previously.
                       isTriangle ? "1 + left_coverages[0] + right_coverages[0]" : "-1");

        g->codeAppendf("%s(corner - outbloat * bloat, %s, "
                          "half2(1 + left_coverages[0] + right_coverages[0], 1));",
                       emitVertexFn,
                       // Erase what the hull wrote previously.
                       isTriangle ? "-1 - left_coverages[0] - right_coverages[0]" : "-1");

        g->codeAppendf("%s(corner + crossbloat * bloat, %s, half2(1 + right_coverages[1], 1));",
                       emitVertexFn,
                       // Erase what the hull wrote previously.
                       isTriangle ? "left_coverages[1] - right_coverages[1]" : "-1");

        g->configure(InputType::kLines, OutputType::kTriangleStrip, 4, isTriangle ? 3 : 2);
    }
};

/**
 * Generates a conservative raster around a convex quadrilateral that encloses a cubic or quadratic.
 */
class GrCCCoverageProcessor::GSCurveHullImpl : public GrCCCoverageProcessor::GSImpl {
public:
    GSCurveHullImpl(std::unique_ptr<Shader> shader) : GSImpl(std::move(shader)) {}

    void onEmitGeometryShader(const GrCCCoverageProcessor&, GrGLSLGeometryBuilder* g,
                              const GrShaderVar& wind, const char* emitVertexFn) const override {
        const char* hullPts = "pts";
        fShader->emitSetupCode(g, "pts", wind.c_str(), &hullPts);

        // Visualize the input (convex) quadrilateral as a square. Paying special attention to wind,
        // we can identify the points by their corresponding corner.
        //
        // NOTE: We split the square down the diagonal from top-right to bottom-left, and generate
        // the hull in two independent invocations. Each invocation designates the corner it will
        // begin with as top-left.
        g->codeAppend ("int i = sk_InvocationID * 2;");
        g->codeAppendf("float2 topleft = %s[i];", hullPts);
        g->codeAppendf("float2 topright = %s[%s > 0 ? i + 1 : 3 - i];", hullPts, wind.c_str());
        g->codeAppendf("float2 bottomleft = %s[%s > 0 ? 3 - i : i + 1];", hullPts, wind.c_str());
        g->codeAppendf("float2 bottomright = %s[2 - i];", hullPts);

        // Determine how much to outset the conservative raster hull from the relevant edges.
        g->codeAppend ("float2 leftbloat = float2(topleft.y > bottomleft.y ? +bloat : -bloat, "
                                                 "topleft.x > bottomleft.x ? -bloat : bloat);");
        g->codeAppend ("float2 upbloat = float2(topright.y > topleft.y ? +bloat : -bloat, "
                                               "topright.x > topleft.x ? -bloat : +bloat);");
        g->codeAppend ("float2 rightbloat = float2(bottomright.y > topright.y ? +bloat : -bloat, "
                                                  "bottomright.x > topright.x ? -bloat : +bloat);");

        // Here we generate the conservative raster geometry. It is the convex hull of 4 pixel-size
        // boxes centered on the input points, split evenly between two invocations. This translates
        // to a polygon with either one, two, or three vertices at each input point, depending on
        // how sharp the corner is. For more details on conservative raster, see:
        // https://developer.nvidia.com/gpugems/GPUGems2/gpugems2_chapter42.html
        g->codeAppendf("bool2 left_up_notequal = notEqual(leftbloat, upbloat);");
        g->codeAppend ("if (all(left_up_notequal)) {");
                           // The top-left corner will have three conservative raster vertices.
                           // Emit the middle one first to the triangle strip.
        g->codeAppendf(    "%s(topleft + float2(-leftbloat.y, leftbloat.x));", emitVertexFn);
        g->codeAppend ("}");
        g->codeAppend ("if (any(left_up_notequal)) {");
                           // Second conservative raster vertex for the top-left corner.
        g->codeAppendf(    "%s(topleft + leftbloat);", emitVertexFn);
        g->codeAppend ("}");

        // Main interior body of this invocation's half of the hull.
        g->codeAppendf("%s(topleft + upbloat);", emitVertexFn);
        g->codeAppendf("%s(bottomleft + leftbloat);", emitVertexFn);
        g->codeAppendf("%s(topright + upbloat);", emitVertexFn);

        // Remaining two conservative raster vertices for the top-right corner.
        g->codeAppendf("bool2 up_right_notequal = notEqual(upbloat, rightbloat);");
        g->codeAppend ("if (any(up_right_notequal)) {");
        g->codeAppendf(    "%s(topright + rightbloat);", emitVertexFn);
        g->codeAppend ("}");
        g->codeAppend ("if (all(up_right_notequal)) {");
        g->codeAppendf(    "%s(topright + float2(-upbloat.y, upbloat.x));", emitVertexFn);
        g->codeAppend ("}");

        g->configure(InputType::kLines, OutputType::kTriangleStrip, 7, 2);
    }
};

void GrCCCoverageProcessor::initGS() {
    SkASSERT(Impl::kGeometryShader == fImpl);
    if (RenderPassIsCubic(fRenderPass) || WindMethod::kInstanceData == fWindMethod) {
        SkASSERT(WindMethod::kCrossProduct == fWindMethod || 3 == this->numInputPoints());
        this->addVertexAttrib("x_or_y_values", kFloat4_GrVertexAttribType);
        SkASSERT(sizeof(QuadPointInstance) == this->getVertexStride() * 2);
        SkASSERT(offsetof(QuadPointInstance, fY) == this->getVertexStride());
    } else {
        this->addVertexAttrib("x_or_y_values", kFloat3_GrVertexAttribType);
        SkASSERT(sizeof(TriPointInstance) == this->getVertexStride() * 2);
        SkASSERT(offsetof(TriPointInstance, fY) == this->getVertexStride());
    }
    this->setWillUseGeoShader();
}

void GrCCCoverageProcessor::appendGSMesh(GrBuffer* instanceBuffer, int instanceCount,
                                         int baseInstance, SkTArray<GrMesh>* out) const {
    // GSImpl doesn't actually make instanced draw calls. Instead, we feed transposed x,y point
    // values to the GPU in a regular vertex array and draw kLines (see initGS). Then, each vertex
    // invocation receives either the shape's x or y values as inputs, which it forwards to the
    // geometry shader.
    SkASSERT(Impl::kGeometryShader == fImpl);
    GrMesh& mesh = out->emplace_back(GrPrimitiveType::kLines);
    mesh.setNonIndexedNonInstanced(instanceCount * 2);
    mesh.setVertexData(instanceBuffer, baseInstance * 2);
}

GrGLSLPrimitiveProcessor* GrCCCoverageProcessor::createGSImpl(std::unique_ptr<Shader> shadr) const {
    switch (fRenderPass) {
        case RenderPass::kTriangles:
            return new GSTriangleHullImpl(std::move(shadr));
        case RenderPass::kQuadratics:
        case RenderPass::kCubics:
            return new GSCurveHullImpl(std::move(shadr));
        case RenderPass::kTriangleCorners:
        case RenderPass::kQuadraticCorners:
        case RenderPass::kCubicCorners:
            return new GSCornerImpl(std::move(shadr));
    }
    SK_ABORT("Invalid RenderPass");
    return nullptr;
}
