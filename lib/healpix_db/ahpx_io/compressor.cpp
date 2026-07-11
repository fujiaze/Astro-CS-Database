#include "compressor.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// 条件编译: 检测是否有 zstd / lz4 库
// 编译时通过 -DHAS_ZSTD / -DHAS_LZ4 启用, 或编译器自动检测头文件
// ============================================================================
#ifdef HAS_ZSTD
#include <zstd.h>
#endif

#ifdef HAS_LZ4
#include <lz4.h>
#endif

namespace ahpx {

// ============================================================================
// Zstd 压缩/解压实现
// ============================================================================

bool hasZstdSupport() {
#ifdef HAS_ZSTD
    return true;
#else
    return false;
#endif
}

size_t compressBoundZstd(size_t srcSize) {
#ifdef HAS_ZSTD
    return ZSTD_compressBound(srcSize);
#else
    // 无库时返回 srcSize (不压缩)
    return srcSize;
#endif
}

size_t compressZstd(const void* src, size_t srcSize,
                    void* dst, size_t dstCapacity, int level) {
    if (!src || !dst || srcSize == 0) {
        fprintf(stderr, "[ahpx][compressor] compressZstd: 无效参数 (src=%p dst=%p srcSize=%zu)\n",
                src, dst, srcSize);
        return 0;
    }

#ifdef HAS_ZSTD
    // 级别范围校验
    if (level < 1) level = 1;
    if (level > 22) level = 22;

    size_t outSize = ZSTD_compress(dst, dstCapacity, src, srcSize, level);
    if (ZSTD_isError(outSize)) {
        fprintf(stderr, "[ahpx][compressor] ZSTD_compress 失败: %s\n",
                ZSTD_getErrorName(outSize));
        return 0;
    }
    return outSize;
#else
    // 无 zstd 库: fallback 到不压缩 (直接拷贝)
    fprintf(stderr, "[ahpx][compressor] 警告: 未编译 zstd 支持, fallback 到不压缩 (srcSize=%zu)\n",
            srcSize);
    if (dstCapacity < srcSize) {
        fprintf(stderr, "[ahpx][compressor] 缓冲区不足: dstCapacity=%zu srcSize=%zu\n",
                dstCapacity, srcSize);
        return 0;
    }
    std::memcpy(dst, src, srcSize);
    return srcSize;
#endif
}

size_t decompressZstd(const void* src, size_t srcSize,
                      void* dst, size_t dstCapacity) {
    if (!src || !dst || srcSize == 0) {
        fprintf(stderr, "[ahpx][compressor] decompressZstd: 无效参数 (src=%p dst=%p srcSize=%zu)\n",
                src, dst, srcSize);
        return 0;
    }

#ifdef HAS_ZSTD
    size_t outSize = ZSTD_decompress(dst, dstCapacity, src, srcSize);
    if (ZSTD_isError(outSize)) {
        fprintf(stderr, "[ahpx][compressor] ZSTD_decompress 失败: %s\n",
                ZSTD_getErrorName(outSize));
        return 0;
    }
    return outSize;
#else
    // 无 zstd 库: 数据未压缩, 直接拷贝
    if (dstCapacity < srcSize) {
        fprintf(stderr, "[ahpx][compressor] 解压缓冲区不足: dstCapacity=%zu srcSize=%zu\n",
                dstCapacity, srcSize);
        return 0;
    }
    std::memcpy(dst, src, srcSize);
    return srcSize;
#endif
}

// ============================================================================
// LZ4 压缩/解压实现
// ============================================================================

bool hasLz4Support() {
#ifdef HAS_LZ4
    return true;
#else
    return false;
#endif
}

size_t compressBoundLz4(size_t srcSize) {
#ifdef HAS_LZ4
    return (size_t)LZ4_compressBound((int)srcSize);
#else
    return srcSize;
#endif
}

size_t compressLz4(const void* src, size_t srcSize,
                   void* dst, size_t dstCapacity) {
    if (!src || !dst || srcSize == 0) {
        fprintf(stderr, "[ahpx][compressor] compressLz4: 无效参数 (src=%p dst=%p srcSize=%zu)\n",
                src, dst, srcSize);
        return 0;
    }

#ifdef HAS_LZ4
    // LZ4_compress_default 返回压缩后字节数, 失败返回 0
    int outSize = LZ4_compress_default(
        (const char*)src, (char*)dst, (int)srcSize, (int)dstCapacity);
    if (outSize <= 0) {
        fprintf(stderr, "[ahpx][compressor] LZ4_compress_default 失败 (返回 %d)\n", outSize);
        return 0;
    }
    return (size_t)outSize;
#else
    // 无 lz4 库: fallback 到不压缩
    fprintf(stderr, "[ahpx][compressor] 警告: 未编译 lz4 支持, fallback 到不压缩 (srcSize=%zu)\n",
            srcSize);
    if (dstCapacity < srcSize) {
        fprintf(stderr, "[ahpx][compressor] 缓冲区不足: dstCapacity=%zu srcSize=%zu\n",
                dstCapacity, srcSize);
        return 0;
    }
    std::memcpy(dst, src, srcSize);
    return srcSize;
#endif
}

size_t decompressLz4(const void* src, size_t srcSize,
                     void* dst, size_t dstCapacity) {
    if (!src || !dst || srcSize == 0) {
        fprintf(stderr, "[ahpx][compressor] decompressLz4: 无效参数 (src=%p dst=%p srcSize=%zu)\n",
                src, dst, srcSize);
        return 0;
    }

#ifdef HAS_LZ4
    // LZ4_decompress_safe 返回解压后字节数, 失败返回负值
    int outSize = LZ4_decompress_safe(
        (const char*)src, (char*)dst, (int)srcSize, (int)dstCapacity);
    if (outSize < 0) {
        fprintf(stderr, "[ahpx][compressor] LZ4_decompress_safe 失败 (返回 %d)\n", outSize);
        return 0;
    }
    return (size_t)outSize;
#else
    // 无 lz4 库: 数据未压缩, 直接拷贝
    if (dstCapacity < srcSize) {
        fprintf(stderr, "[ahpx][compressor] 解压缓冲区不足: dstCapacity=%zu srcSize=%zu\n",
                dstCapacity, srcSize);
        return 0;
    }
    std::memcpy(dst, src, srcSize);
    return srcSize;
#endif
}

} // namespace ahpx
