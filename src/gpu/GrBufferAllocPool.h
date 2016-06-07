/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrBufferAllocPool_DEFINED
#define GrBufferAllocPool_DEFINED

#include "SkTArray.h"
#include "SkTDArray.h"
#include "SkTypes.h"

class GrGeometryBuffer;
class GrGpu;

/**
 * A pool of geometry buffers tied to a GrGpu.
 *
 * The pool allows a client to make space for geometry and then put back excess
 * space if it over allocated. When a client is ready to draw from the pool
 * it calls unmap on the pool ensure buffers are ready for drawing. The pool
 * can be reset after drawing is completed to recycle space.
 *
 * At creation time a minimum per-buffer size can be specified. Additionally,
 * a number of buffers to preallocate can be specified. These will
 * be allocated at the min size and kept around until the pool is destroyed.
 */
class GrBufferAllocPool : SkNoncopyable {
public:
    /**
     * Ensures all buffers are unmapped and have all data written to them.
     * Call before drawing using buffers from the pool.
     */
    void unmap();

    /**
     *  Invalidates all the data in the pool, unrefs non-preallocated buffers.
     */
    void reset();

    /**
     * Frees data from makeSpaces in LIFO order.
     */
    void putBack(size_t bytes);

protected:
    /**
     * Used to determine what type of buffers to create. We could make the
     * createBuffer a virtual except that we want to use it in the cons for
     * pre-allocated buffers.
     */
    enum BufferType {
        kVertex_BufferType,
        kIndex_BufferType,
    };

    /**
     * Constructor
     *
     * @param gpu                   The GrGpu used to create the buffers.
     * @param bufferType            The type of buffers to create.
     * @param bufferSize            The minimum size of created buffers.
     *                              This value will be clamped to some
     *                              reasonable minimum.
     * @param preallocBufferCnt     The pool will allocate this number of
     *                              buffers at bufferSize and keep them until it
     *                              is destroyed.
     */
     GrBufferAllocPool(GrGpu* gpu,
                       BufferType bufferType,
                       size_t   bufferSize = 0,
                       int preallocBufferCnt = 0);

    virtual ~GrBufferAllocPool();

    /**
     * Returns a block of memory to hold data. A buffer designated to hold the
     * data is given to the caller. The buffer may or may not be locked. The
     * returned ptr remains valid until any of the following:
     *      *makeSpace is called again.
     *      *unmap is called.
     *      *reset is called.
     *      *this object is destroyed.
     *
     * Once unmap on the pool is called the data is guaranteed to be in the
     * buffer at the offset indicated by offset. Until that time it may be
     * in temporary storage and/or the buffer may be locked.
     *
     * @param size         the amount of data to make space for
     * @param alignment    alignment constraint from start of buffer
     * @param buffer       returns the buffer that will hold the data.
     * @param offset       returns the offset into buffer of the data.
     * @return pointer to where the client should write the data.
     */
    void* makeSpace(size_t size,
                    size_t alignment,
                    const GrGeometryBuffer** buffer,
                    size_t* offset);

    GrGeometryBuffer* createBuffer(size_t size);

private:
    struct BufferBlock {
        size_t              fBytesFree;
        GrGeometryBuffer*   fBuffer;
    };

    bool createBlock(size_t requestSize);
    void destroyBlock();
    void flushCpuData(const BufferBlock& block, size_t flushSize);
#ifdef SK_DEBUG
    void validate(bool unusedBlockAllowed = false) const;
#endif

    size_t                          fBytesInUse;

    GrGpu*                          fGpu;
    SkTDArray<GrGeometryBuffer*>    fPreallocBuffers;
    size_t                          fMinBlockSize;
    BufferType                      fBufferType;

    SkTArray<BufferBlock>           fBlocks;
    int                             fPreallocBuffersInUse;
    // We attempt to cycle through the preallocated buffers rather than
    // always starting from the first.
    int                             fPreallocBufferStartIdx;
    SkAutoMalloc                    fCpuData;
    void*                           fBufferPtr;
};

class GrVertexBuffer;

/**
 * A GrBufferAllocPool of vertex buffers
 */
class GrVertexBufferAllocPool : public GrBufferAllocPool {
public:
    /**
     * Constructor
     *
     * @param gpu                   The GrGpu used to create the vertex buffers.
     * @param bufferSize            The minimum size of created VBs. This value
     *                              will be clamped to some reasonable minimum.
     * @param preallocBufferCnt     The pool will allocate this number of VBs at
     *                              bufferSize and keep them until it is
     *                              destroyed.
     */
    explicit GrVertexBufferAllocPool(GrGpu* gpu, size_t bufferSize = 0, int preallocBufferCnt = 0);

    /**
     * Returns a block of memory to hold vertices. A buffer designated to hold
     * the vertices given to the caller. The buffer may or may not be locked.
     * The returned ptr remains valid until any of the following:
     *      *makeSpace is called again.
     *      *unmap is called.
     *      *reset is called.
     *      *this object is destroyed.
     *
     * Once unmap on the pool is called the vertices are guaranteed to be in
     * the buffer at the offset indicated by startVertex. Until that time they
     * may be in temporary storage and/or the buffer may be locked.
     *
     * @param vertexSize   specifies size of a vertex to allocate space for
     * @param vertexCount  number of vertices to allocate space for
     * @param buffer       returns the vertex buffer that will hold the
     *                     vertices.
     * @param startVertex  returns the offset into buffer of the first vertex.
     *                     In units of the size of a vertex from layout param.
     * @return pointer to first vertex.
     */
    void* makeSpace(size_t vertexSize,
                    int vertexCount,
                    const GrVertexBuffer** buffer,
                    int* startVertex);

private:
    typedef GrBufferAllocPool INHERITED;
};

class GrIndexBuffer;

/**
 * A GrBufferAllocPool of index buffers
 */
class GrIndexBufferAllocPool : public GrBufferAllocPool {
public:
    /**
     * Constructor
     *
     * @param gpu                   The GrGpu used to create the index buffers.
     * @param bufferSize            The minimum size of created IBs. This value
     *                              will be clamped to some reasonable minimum.
     * @param preallocBufferCnt     The pool will allocate this number of VBs at
     *                              bufferSize and keep them until it is
     *                              destroyed.
     */
    explicit GrIndexBufferAllocPool(GrGpu* gpu,
                           size_t bufferSize = 0,
                           int preallocBufferCnt = 0);

    /**
     * Returns a block of memory to hold indices. A buffer designated to hold
     * the indices is given to the caller. The buffer may or may not be locked.
     * The returned ptr remains valid until any of the following:
     *      *makeSpace is called again.
     *      *unmap is called.
     *      *reset is called.
     *      *this object is destroyed.
     *
     * Once unmap on the pool is called the indices are guaranteed to be in the
     * buffer at the offset indicated by startIndex. Until that time they may be
     * in temporary storage and/or the buffer may be locked.
     *
     * @param indexCount   number of indices to allocate space for
     * @param buffer       returns the index buffer that will hold the indices.
     * @param startIndex   returns the offset into buffer of the first index.
     * @return pointer to first index.
     */
    void* makeSpace(int indexCount,
                    const GrIndexBuffer** buffer,
                    int* startIndex);

private:
    typedef GrBufferAllocPool INHERITED;
};

#endif
