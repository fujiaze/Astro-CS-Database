// ============================================================================
// test_transform.cpp - WP-G 步骤12 Transform 正式路径单元测试
//
// 依据:
// - docs/stage1_fix/spec.md 步骤12 (transform 正式路径)
// - docs/stage1_fix/00_COMMON_CONTRACTS.md §5.1 (测试要求)
// - 02_FROZEN_STAGE1_HISS_SPEC.md §15 (子块目录 transform_id 字段)
//
// 测试范围:
// 1. NONE 变换: 零开销, 数据不变
// 2. BYTE_SHUFFLE 往返: 随机 float32 数组, shuffle → inverse 后数据一致
// 3. DELTA 往返: 随机 int32 数组, delta → inverse 后数据一致
// 4. DELTA_VARINT 往返: 随机 int32 数组, delta_varint → inverse 后数据一致
// 5. ZSTD+shuffle 组合: 先 shuffle 再 ZSTD 压缩, 解压后 inverse shuffle 还原
// 6. 空数据: transform 处理空输入不崩溃
// 7. 不同 element_size: float32(4B)/float64(8B)/uint8(1B) 都能正确处理
// 8. Writer/Reader 集成: Writer 设置 transform, Reader 逆向还原
// 9. 枚举互转: TransformType <-> name, TransformType <-> TransformId
// 10. DELTA 边界: INT32_MIN/INT32_MAX 环绕运算正确
// 11. DELTA_VARINT 递增序列: 小 delta 的 varint 压缩效果
//
// 编译 (PowerShell + mingw64):
// # 基础编译 (仅 transform 单元测试, 无 ZSTD/Writer/Reader):
// $env:Path = "C:\msys64\mingw64\bin;$env:Path"
// cd "f:\Astro dev\Astro CS Normalization Database\lib\astro_image_io"
// g++ -std=c++17 -O2 -Iinclude -Isrc tests/test_transform.cpp src/hiss_transform.cpp -o tests/test_transform.exe
//
// # 完整编译 (含 ZSTD + Writer/Reader 集成测试):
// g++ -std=c++17 -O2 -DHAS_ZSTD -DHAS_LZ4 -Iinclude -Isrc `
// tests/test_transform.cpp `
// src/hiss_transform.cpp src/hiss_codec.cpp src/hiss_common.cpp `
// src/hiss_tile_model.cpp src/hiss_writer.cpp src/hiss_stream_writer.cpp `
// src/hiss_reader.cpp `
// -lzstd -llz4 `
// -o tests/test_transform.exe
//
// 运行:
// ./tests/test_transform.exe
//
// 测试框架: 自维护通过/失败计数, 任何契约不满足必须真正失败
// (禁止 ASSERT_TRUE(true, "已知问题") 软通过, 依据 spec.md §3 步骤15)
// ============================================================================

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <random>
#include <algorithm>

// 条件包含: ZSTD + Writer/Reader 集成测试需要这些头文件和宏
#ifdef HAS_ZSTD
#include "hiss_format.h"     // HissWriter, HissReader, CodecRegistry
#include "hiss_tile_model.h" // compute_tile_nside
#endif

#include "hiss_transform.h"  // TransformType, apply_transform, inverse_transform

// ============================================================================
// 测试框架: 计数 + 断言宏
// ============================================================================
static int g_pass_count = 0;
static int g_fail_count = 0;
static int g_skip_count = 0;

// ASSERT_TRUE(cond, msg): cond 为假时记失败并打印
// 注意: 不支持 "true, 已知问题" 软通过 — 任何 false 都是真失败
#define ASSERT_TRUE(cond, msg) do { \
    if (cond) { \
        g_pass_count++; \
        fprintf(stdout, "  OK: %s\n", msg); \
    } else { \
        g_fail_count++; \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
    } \
} while (0)

// ASSERT_EQ_INT(a, b, msg): 整数相等
#define ASSERT_EQ_INT(a, b, msg) do { \
    long _a = (long)(a); \
    long _b = (long)(b); \
    if (_a == _b) { \
        g_pass_count++; \
        fprintf(stdout, "  OK: %s (=%ld)\n", msg, _a); \
    } else { \
        g_fail_count++; \
        fprintf(stderr, "  FAIL: %s (got=%ld, want=%ld, line %d)\n", \
                msg, _a, _b, __LINE__); \
    } \
} while (0)

// ASSERT_EQ_FLT(a, b, tol, msg): 浮点数近似相等
#define ASSERT_EQ_FLT(a, b, tol, msg) do { \
    double _a = (double)(a); \
    double _b = (double)(b); \
    double _d = _a - _b; \
    if (_d < 0) _d = -_d; \
    if (_d <= (double)(tol)) { \
        g_pass_count++; \
        fprintf(stdout, "  OK: %s (a=%g, b=%g)\n", msg, _a, _b); \
    } else { \
        g_fail_count++; \
        fprintf(stderr, "  FAIL: %s (got=%g, want=%g, tol=%g, line %d)\n", \
                msg, _a, _b, (double)(tol), __LINE__); \
    } \
} while (0)

// SKIP(msg): 跳过测试 (不计数为 pass/fail, 因条件不满足如缺少 ZSTD)
#define SKIP(msg) do { \
    g_skip_count++; \
    fprintf(stdout, "  SKIP: %s\n", msg); \
} while (0)

// ============================================================================
// 辅助: 比较两个字节数组是否完全一致
// ============================================================================
static bool bytes_equal(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return false;
    if (a.empty()) return true;
    return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

// 辅助: 将数据转为 vector<uint8_t>
template<typename T>
static std::vector<uint8_t> to_bytes(const std::vector<T>& v) {
    std::vector<uint8_t> bytes(v.size() * sizeof(T));
    if (!v.empty()) {
        std::memcpy(bytes.data(), v.data(), bytes.size());
    }
    return bytes;
}

// 辅助: 将 vector<uint8_t> 转为 vector<T>
template<typename T>
static std::vector<T> from_bytes(const std::vector<uint8_t>& bytes) {
    std::vector<T> v(bytes.size() / sizeof(T));
    if (!v.empty()) {
        std::memcpy(v.data(), bytes.data(), v.size() * sizeof(T));
    }
    return v;
}

// ============================================================================
// 测试 1: NONE 变换 — 零开销, 数据不变
// ============================================================================
static void test_none_transform() {
    fprintf(stdout, "[TEST] NONE 变换: 零开销, 数据不变\n");

    // 构造随机 float32 数据
    std::mt19937 rng(42);
    std::vector<float> data(256);
    for (auto& v : data) {
        v = (float)(rng() % 10000) / 100.0f;
    }
    auto bytes = to_bytes(data);

    // apply_transform(NONE)
    auto transformed = hiss::apply_transform(hiss::TransformType::NONE,
                                               bytes.data(), bytes.size(), sizeof(float));
    ASSERT_TRUE(bytes_equal(transformed, bytes), "NONE forward: 输出 == 输入");

    // inverse_transform(NONE)
    auto restored = hiss::inverse_transform(hiss::TransformType::NONE,
                                              transformed.data(), transformed.size(),
                                              sizeof(float), transformed.size());
    ASSERT_TRUE(bytes_equal(restored, bytes), "NONE inverse: 还原 == 原始");

    // 验证数据值一致
    auto data_back = from_bytes<float>(restored);
    ASSERT_EQ_INT(data_back.size(), data.size(), "NONE: 元素数量一致");
    bool values_ok = true;
    for (size_t i = 0; i < data.size() && i < data_back.size(); i++) {
        if (data[i] != data_back[i]) { values_ok = false; break; }
    }
    ASSERT_TRUE(values_ok, "NONE: 所有 float 值精确一致");
}

// ============================================================================
// 测试 2: BYTE_SHUFFLE 往返 — 随机 float32 数组
// ============================================================================
static void test_byte_shuffle_float32() {
    fprintf(stdout, "[TEST] BYTE_SHUFFLE 往返: 随机 float32 数组\n");

    std::mt19937 rng(42);
    std::vector<float> data(256);
    for (auto& v : data) {
        v = (float)(rng() % 10000) / 100.0f;
    }
    auto bytes = to_bytes(data);

    // forward
    auto shuffled = hiss::apply_transform(hiss::TransformType::BYTE_SHUFFLE,
                                            bytes.data(), bytes.size(), sizeof(float));
    ASSERT_EQ_INT(shuffled.size(), bytes.size(), "BYTE_SHUFFLE: 输出大小 == 输入大小");

    // shuffled 不应等于原始 (除非数据极端特殊)
    ASSERT_TRUE(!bytes_equal(shuffled, bytes), "BYTE_SHUFFLE: 重排后数据 != 原始 (验证变换生效)");

    // inverse
    auto restored = hiss::inverse_transform(hiss::TransformType::BYTE_SHUFFLE,
                                              shuffled.data(), shuffled.size(),
                                              sizeof(float), shuffled.size());
    ASSERT_TRUE(bytes_equal(restored, bytes), "BYTE_SHUFFLE: 往返后数据一致");

    // 验证 float 值
    auto data_back = from_bytes<float>(restored);
    bool values_ok = true;
    for (size_t i = 0; i < data.size() && i < data_back.size(); i++) {
        if (data[i] != data_back[i]) { values_ok = false; break; }
    }
    ASSERT_TRUE(values_ok, "BYTE_SHUFFLE: 所有 float 值精确一致");
}

// ============================================================================
// 测试 3: DELTA 往返 — 随机 int32 数组
// ============================================================================
static void test_delta_int32() {
    fprintf(stdout, "[TEST] DELTA 往返: 随机 int32 数组\n");

    std::mt19937 rng(42);
    std::vector<int32_t> data(256);
    for (auto& v : data) {
        v = (int32_t)rng();
    }
    auto bytes = to_bytes(data);

    // forward
    auto delta = hiss::apply_transform(hiss::TransformType::DELTA,
                                         bytes.data(), bytes.size(), sizeof(int32_t));
    ASSERT_EQ_INT(delta.size(), bytes.size(), "DELTA: 输出大小 == 输入大小");

    // inverse
    auto restored = hiss::inverse_transform(hiss::TransformType::DELTA,
                                              delta.data(), delta.size(),
                                              sizeof(int32_t), delta.size());
    ASSERT_TRUE(bytes_equal(restored, bytes), "DELTA: 往返后数据一致");

    // 验证 int32 值
    auto data_back = from_bytes<int32_t>(restored);
    bool values_ok = true;
    for (size_t i = 0; i < data.size() && i < data_back.size(); i++) {
        if (data[i] != data_back[i]) { values_ok = false; break; }
    }
    ASSERT_TRUE(values_ok, "DELTA: 所有 int32 值精确一致 (含环绕)");
}

// ============================================================================
// 测试 4: DELTA_VARINT 往返 — 随机 int32 数组
// ============================================================================
static void test_delta_varint_int32() {
    fprintf(stdout, "[TEST] DELTA_VARINT 往返: 随机 int32 数组\n");

    std::mt19937 rng(42);
    std::vector<int32_t> data(256);
    for (auto& v : data) {
        v = (int32_t)rng();
    }
    auto bytes = to_bytes(data);

    // forward
    auto encoded = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                           bytes.data(), bytes.size(), sizeof(int32_t));
    // DELTA_VARINT 输出含 4 字节 n_elements 前缀 + varint 数据, 大小可变
    ASSERT_TRUE(encoded.size() >= 4, "DELTA_VARINT: 输出 >= 4 字节 (n_elements 前缀)");

    // 验证 n_elements 前缀
    uint32_t n_elements = (uint32_t)encoded[0] | ((uint32_t)encoded[1] << 8) |
                          ((uint32_t)encoded[2] << 16) | ((uint32_t)encoded[3] << 24);
    ASSERT_EQ_INT(n_elements, 256u, "DELTA_VARINT: n_elements 前缀 == 256");

    // inverse (expected_output_size=0 自动确定)
    auto restored = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                              encoded.data(), encoded.size(),
                                              sizeof(int32_t), 0);
    ASSERT_TRUE(bytes_equal(restored, bytes), "DELTA_VARINT: 往返后数据一致");

    // 验证 int32 值
    auto data_back = from_bytes<int32_t>(restored);
    bool values_ok = true;
    for (size_t i = 0; i < data.size() && i < data_back.size(); i++) {
        if (data[i] != data_back[i]) { values_ok = false; break; }
    }
    ASSERT_TRUE(values_ok, "DELTA_VARINT: 所有 int32 值精确一致 (含环绕)");
}

// ============================================================================
// 测试 5: ZSTD + shuffle 组合
// 先 shuffle 再 ZSTD 压缩, 解压后 inverse shuffle 还原
// ============================================================================
#ifdef HAS_ZSTD
static void test_zstd_shuffle_combo() {
    fprintf(stdout, "[TEST] ZSTD + shuffle 组合: shuffle → ZSTD → decompress → inverse shuffle\n");

    // 构造浮点数据 (相似值, shuffle+ZSTD 应有压缩效果)
    std::mt19937 rng(42);
    std::vector<float> data(1024);
    for (size_t i = 0; i < data.size(); i++) {
        // 相似浮点值: 基线 + 小扰动 (shuffle 后高位字节相似, ZSTD 压缩率高)
        data[i] = 100.0f + (float)(rng() % 100) / 100.0f;
    }
    auto bytes = to_bytes(data);

    // 1. BYTE_SHUFFLE forward
    auto shuffled = hiss::apply_transform(hiss::TransformType::BYTE_SHUFFLE,
                                            bytes.data(), bytes.size(), sizeof(float));
    ASSERT_EQ_INT(shuffled.size(), bytes.size(), "ZSTD+shuffle: shuffle 后大小一致");

    // 2. ZSTD 压缩 shuffled 数据
    const hiss::CodecEntry* zstd_codec = hiss::CodecRegistry::instance().find(hiss::CodecId::ZSTD);
    ASSERT_TRUE(zstd_codec != nullptr, "ZSTD codec 已注册");

    size_t bound = zstd_codec->bound(shuffled.size());
    std::vector<uint8_t> compressed(bound);
    size_t compressed_size = bound;
    int ret = zstd_codec->compress(shuffled.data(), shuffled.size(),
                                     compressed.data(), &compressed_size);
    ASSERT_EQ_INT(ret, 0, "ZSTD 压缩成功");
    ASSERT_TRUE(compressed_size < shuffled.size(), "ZSTD+shuffle: 压缩后 < 原始 (有压缩效果)");

    fprintf(stdout, "  原始=%zu, shuffle后=%zu, ZSTD压缩后=%zu (%.1f%%)\n",
            bytes.size(), shuffled.size(), compressed_size,
            100.0 * compressed_size / bytes.size());

    // 3. ZSTD 解压
    std::vector<uint8_t> decompressed(shuffled.size());
    size_t decomp_size = decompressed.size();
    ret = zstd_codec->decompress(compressed.data(), compressed_size,
                                   decompressed.data(), decomp_size);
    ASSERT_EQ_INT(ret, 0, "ZSTD 解压成功");
    ASSERT_EQ_INT(decomp_size, shuffled.size(), "ZSTD 解压后大小 == shuffle 后大小");

    // 4. inverse BYTE_SHUFFLE
    auto restored = hiss::inverse_transform(hiss::TransformType::BYTE_SHUFFLE,
                                              decompressed.data(), decompressed.size(),
                                              sizeof(float), decompressed.size());
    ASSERT_TRUE(bytes_equal(restored, bytes), "ZSTD+shuffle: 往返后数据一致");

    // 验证 float 值
    auto data_back = from_bytes<float>(restored);
    bool values_ok = true;
    for (size_t i = 0; i < data.size() && i < data_back.size(); i++) {
        if (data[i] != data_back[i]) { values_ok = false; break; }
    }
    ASSERT_TRUE(values_ok, "ZSTD+shuffle: 所有 float 值精确一致");

    // 额外: 对比无 shuffle 的 ZSTD 压缩 (纯诊断信息, 不计入 pass/fail)
    // 注: shuffle 对压缩率的提升是数据相关的启发式特性, 非正确性保证。
    // 对于小数据集或值已高度相似的浮点数据, ZSTD 单独可能已压缩很好,
    // shuffle 后字节重排可能打破局部相似性, 反而略微降低压缩率。
    // transform 的正确性由上方往返测试保证, 压缩效率不属于正确性范畴。
    size_t bound2 = zstd_codec->bound(bytes.size());
    std::vector<uint8_t> compressed_noshuffle(bound2);
    size_t compressed_size_noshuffle = bound2;
    ret = zstd_codec->compress(bytes.data(), bytes.size(),
                                 compressed_noshuffle.data(), &compressed_size_noshuffle);
    ASSERT_EQ_INT(ret, 0, "ZSTD (无shuffle) 压缩成功");
    fprintf(stdout, "  [诊断] 无shuffle ZSTD=%zu, 有shuffle ZSTD=%zu (差异 %+.1f%%)\n",
            compressed_size_noshuffle, compressed_size,
            100.0 * (double)compressed_size / compressed_size_noshuffle - 100.0);
}
#endif // HAS_ZSTD

// ============================================================================
// 测试 6: 空数据 — transform 处理空输入不崩溃
// ============================================================================
static void test_empty_data() {
    fprintf(stdout, "[TEST] 空数据: transform 处理空输入不崩溃\n");

    std::vector<uint8_t> empty;

    // NONE
    auto r1 = hiss::apply_transform(hiss::TransformType::NONE,
                                      empty.data(), 0, 4);
    ASSERT_TRUE(r1.empty(), "NONE forward: 空输入 → 空输出");
    auto r1i = hiss::inverse_transform(hiss::TransformType::NONE,
                                         empty.data(), 0, 4, 0);
    ASSERT_TRUE(r1i.empty(), "NONE inverse: 空输入 → 空输出");

    // BYTE_SHUFFLE
    auto r2 = hiss::apply_transform(hiss::TransformType::BYTE_SHUFFLE,
                                      empty.data(), 0, 4);
    ASSERT_TRUE(r2.empty(), "BYTE_SHUFFLE forward: 空输入 → 空输出");
    auto r2i = hiss::inverse_transform(hiss::TransformType::BYTE_SHUFFLE,
                                         empty.data(), 0, 4, 0);
    ASSERT_TRUE(r2i.empty(), "BYTE_SHUFFLE inverse: 空输入 → 空输出");

    // DELTA
    auto r3 = hiss::apply_transform(hiss::TransformType::DELTA,
                                      empty.data(), 0, 4);
    ASSERT_TRUE(r3.empty(), "DELTA forward: 空输入 → 空输出");
    auto r3i = hiss::inverse_transform(hiss::TransformType::DELTA,
                                         empty.data(), 0, 4, 0);
    ASSERT_TRUE(r3i.empty(), "DELTA inverse: 空输入 → 空输出");

    // DELTA_VARINT: 空输入 → [0,0,0,0] (n_elements=0 前缀)
    auto r4 = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                      empty.data(), 0, 4);
    ASSERT_EQ_INT(r4.size(), 4u, "DELTA_VARINT forward: 空输入 → 4 字节 (n_elements=0)");
    if (r4.size() >= 4) {
        uint32_t n = (uint32_t)r4[0] | ((uint32_t)r4[1] << 8) |
                     ((uint32_t)r4[2] << 16) | ((uint32_t)r4[3] << 24);
        ASSERT_EQ_INT(n, 0u, "DELTA_VARINT: 空输入的 n_elements=0");
    }
    // inverse: [0,0,0,0] → 空输出
    auto r4i = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                         r4.data(), r4.size(), 4, 0);
    ASSERT_TRUE(r4i.empty(), "DELTA_VARINT inverse: n_elements=0 → 空输出");
}

// ============================================================================
// 测试 7: 不同 element_size — float32(4B)/float64(8B)/uint8(1B)
// ============================================================================
static void test_different_element_sizes() {
    fprintf(stdout, "[TEST] 不同 element_size: float32(4B)/float64(8B)/uint8(1B)\n");

    std::mt19937 rng(42);

    // --- float32 (4B) ---
    {
        std::vector<float> data(128);
        for (auto& v : data) v = (float)(rng() % 10000) / 100.0f;
        auto bytes = to_bytes(data);

        // BYTE_SHUFFLE
        auto sh = hiss::apply_transform(hiss::TransformType::BYTE_SHUFFLE,
                                          bytes.data(), bytes.size(), 4);
        auto rs = hiss::inverse_transform(hiss::TransformType::BYTE_SHUFFLE,
                                            sh.data(), sh.size(), 4, sh.size());
        ASSERT_TRUE(bytes_equal(rs, bytes), "float32 BYTE_SHUFFLE 往返一致");

        // DELTA
        auto dl = hiss::apply_transform(hiss::TransformType::DELTA,
                                          bytes.data(), bytes.size(), 4);
        auto rd = hiss::inverse_transform(hiss::TransformType::DELTA,
                                            dl.data(), dl.size(), 4, dl.size());
        ASSERT_TRUE(bytes_equal(rd, bytes), "float32 DELTA 往返一致");

        // DELTA_VARINT
        auto dv = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                          bytes.data(), bytes.size(), 4);
        auto rv = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                            dv.data(), dv.size(), 4, 0);
        ASSERT_TRUE(bytes_equal(rv, bytes), "float32 DELTA_VARINT 往返一致");
    }

    // --- float64 (8B) ---
    {
        std::vector<double> data(128);
        for (auto& v : data) v = (double)(rng() % 1000000) / 1000.0;
        auto bytes = to_bytes(data);

        // BYTE_SHUFFLE
        auto sh = hiss::apply_transform(hiss::TransformType::BYTE_SHUFFLE,
                                          bytes.data(), bytes.size(), 8);
        auto rs = hiss::inverse_transform(hiss::TransformType::BYTE_SHUFFLE,
                                            sh.data(), sh.size(), 8, sh.size());
        ASSERT_TRUE(bytes_equal(rs, bytes), "float64 BYTE_SHUFFLE 往返一致");

        // DELTA
        auto dl = hiss::apply_transform(hiss::TransformType::DELTA,
                                          bytes.data(), bytes.size(), 8);
        auto rd = hiss::inverse_transform(hiss::TransformType::DELTA,
                                            dl.data(), dl.size(), 8, dl.size());
        ASSERT_TRUE(bytes_equal(rd, bytes), "float64 DELTA 往返一致");

        // DELTA_VARINT
        auto dv = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                          bytes.data(), bytes.size(), 8);
        auto rv = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                            dv.data(), dv.size(), 8, 0);
        ASSERT_TRUE(bytes_equal(rv, bytes), "float64 DELTA_VARINT 往返一致");
    }

    // --- uint8 (1B) ---
    {
        std::vector<uint8_t> data(256);
        for (auto& v : data) v = (uint8_t)(rng() % 256);
        auto bytes = data;  // uint8 数据本身就是字节

        // BYTE_SHUFFLE (element_size=1: no-op, 但不应崩溃)
        auto sh = hiss::apply_transform(hiss::TransformType::BYTE_SHUFFLE,
                                          bytes.data(), bytes.size(), 1);
        ASSERT_TRUE(bytes_equal(sh, bytes), "uint8 BYTE_SHUFFLE: no-op (element_size=1)");
        auto rs = hiss::inverse_transform(hiss::TransformType::BYTE_SHUFFLE,
                                            sh.data(), sh.size(), 1, sh.size());
        ASSERT_TRUE(bytes_equal(rs, bytes), "uint8 BYTE_SHUFFLE 往返一致");

        // DELTA (element_size=1: 按字节差分)
        auto dl = hiss::apply_transform(hiss::TransformType::DELTA,
                                          bytes.data(), bytes.size(), 1);
        auto rd = hiss::inverse_transform(hiss::TransformType::DELTA,
                                            dl.data(), dl.size(), 1, dl.size());
        ASSERT_TRUE(bytes_equal(rd, bytes), "uint8 DELTA 往返一致");

        // DELTA_VARINT (element_size=1: 按字节差分+varint)
        auto dv = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                          bytes.data(), bytes.size(), 1);
        auto rv = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                            dv.data(), dv.size(), 1, 0);
        ASSERT_TRUE(bytes_equal(rv, bytes), "uint8 DELTA_VARINT 往返一致");
    }

    // --- uint16 (2B) ---
    {
        std::vector<uint16_t> data(128);
        for (auto& v : data) v = (uint16_t)(rng() % 65536);
        auto bytes = to_bytes(data);

        auto dl = hiss::apply_transform(hiss::TransformType::DELTA,
                                          bytes.data(), bytes.size(), 2);
        auto rd = hiss::inverse_transform(hiss::TransformType::DELTA,
                                            dl.data(), dl.size(), 2, dl.size());
        ASSERT_TRUE(bytes_equal(rd, bytes), "uint16 DELTA 往返一致");

        auto dv = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                          bytes.data(), bytes.size(), 2);
        auto rv = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                            dv.data(), dv.size(), 2, 0);
        ASSERT_TRUE(bytes_equal(rv, bytes), "uint16 DELTA_VARINT 往返一致");
    }

    // --- int64 (8B) ---
    {
        std::vector<int64_t> data(64);
        for (auto& v : data) v = (int64_t)rng() * (int64_t)rng();
        auto bytes = to_bytes(data);

        auto dl = hiss::apply_transform(hiss::TransformType::DELTA,
                                          bytes.data(), bytes.size(), 8);
        auto rd = hiss::inverse_transform(hiss::TransformType::DELTA,
                                            dl.data(), dl.size(), 8, dl.size());
        ASSERT_TRUE(bytes_equal(rd, bytes), "int64 DELTA 往返一致");

        auto dv = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                          bytes.data(), bytes.size(), 8);
        auto rv = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                            dv.data(), dv.size(), 8, 0);
        ASSERT_TRUE(bytes_equal(rv, bytes), "int64 DELTA_VARINT 往返一致");
    }
}

// ============================================================================
// 测试 8: 枚举互转 — TransformType <-> name, TransformType <-> TransformId
// ============================================================================
static void test_enum_conversion() {
    fprintf(stdout, "[TEST] 枚举互转: TransformType <-> name, TransformType <-> TransformId\n");

    // name 往返
    ASSERT_TRUE(std::string(hiss::transform_type_name(hiss::TransformType::NONE)) == "NONE",
                "name(NONE) == \"NONE\"");
    ASSERT_TRUE(std::string(hiss::transform_type_name(hiss::TransformType::BYTE_SHUFFLE)) == "BYTE_SHUFFLE",
                "name(BYTE_SHUFFLE) == \"BYTE_SHUFFLE\"");
    ASSERT_TRUE(std::string(hiss::transform_type_name(hiss::TransformType::DELTA)) == "DELTA",
                "name(DELTA) == \"DELTA\"");
    ASSERT_TRUE(std::string(hiss::transform_type_name(hiss::TransformType::DELTA_VARINT)) == "DELTA_VARINT",
                "name(DELTA_VARINT) == \"DELTA_VARINT\"");

    // name -> enum
    ASSERT_EQ_INT((int)hiss::transform_type_from_name("NONE"), (int)hiss::TransformType::NONE,
                  "from_name(\"NONE\") == NONE");
    ASSERT_EQ_INT((int)hiss::transform_type_from_name("BYTE_SHUFFLE"), (int)hiss::TransformType::BYTE_SHUFFLE,
                  "from_name(\"BYTE_SHUFFLE\") == BYTE_SHUFFLE");
    ASSERT_EQ_INT((int)hiss::transform_type_from_name("DELTA"), (int)hiss::TransformType::DELTA,
                  "from_name(\"DELTA\") == DELTA");
    ASSERT_EQ_INT((int)hiss::transform_type_from_name("DELTA_VARINT"), (int)hiss::TransformType::DELTA_VARINT,
                  "from_name(\"DELTA_VARINT\") == DELTA_VARINT");

    // TransformId <-> TransformType
    ASSERT_EQ_INT((int)hiss::transform_id_to_type(hiss::TransformId::NONE),
                  (int)hiss::TransformType::NONE,
                  "id_to_type(NONE) == NONE");
    ASSERT_EQ_INT((int)hiss::transform_id_to_type(hiss::TransformId::BYTE_SHUFFLE),
                  (int)hiss::TransformType::BYTE_SHUFFLE,
                  "id_to_type(BYTE_SHUFFLE) == BYTE_SHUFFLE");
    ASSERT_EQ_INT((int)hiss::transform_id_to_type(hiss::TransformId::DELTA),
                  (int)hiss::TransformType::DELTA,
                  "id_to_type(DELTA) == DELTA");
    ASSERT_EQ_INT((int)hiss::transform_id_to_type(hiss::TransformId::DELTA_VARINT),
                  (int)hiss::TransformType::DELTA_VARINT,
                  "id_to_type(DELTA_VARINT) == DELTA_VARINT");

    // 向后兼容: VARINT(旧值=3) → DELTA_VARINT
    ASSERT_EQ_INT((int)hiss::transform_id_to_type(hiss::TransformId::VARINT),
                  (int)hiss::TransformType::DELTA_VARINT,
                  "id_to_type(VARINT) == DELTA_VARINT (向后兼容)");

    // type -> id
    ASSERT_EQ_INT((int)hiss::transform_type_to_id(hiss::TransformType::NONE),
                  (int)hiss::TransformId::NONE,
                  "type_to_id(NONE) == NONE");
    ASSERT_EQ_INT((int)hiss::transform_type_to_id(hiss::TransformType::BYTE_SHUFFLE),
                  (int)hiss::TransformId::BYTE_SHUFFLE,
                  "type_to_id(BYTE_SHUFFLE) == BYTE_SHUFFLE");
    ASSERT_EQ_INT((int)hiss::transform_type_to_id(hiss::TransformType::DELTA),
                  (int)hiss::TransformId::DELTA,
                  "type_to_id(DELTA) == DELTA");
    ASSERT_EQ_INT((int)hiss::transform_type_to_id(hiss::TransformType::DELTA_VARINT),
                  (int)hiss::TransformId::DELTA_VARINT,
                  "type_to_id(DELTA_VARINT) == DELTA_VARINT");
}

// ============================================================================
// 测试 9: DELTA 边界 — INT32_MIN/INT32_MAX 环绕运算
// ============================================================================
static void test_delta_boundary() {
    fprintf(stdout, "[TEST] DELTA 边界: INT32_MIN/INT32_MAX 环绕运算\n");

    // 构造极端值序列: INT32_MAX → INT32_MIN → 0 → -1 → INT32_MAX
    std::vector<int32_t> data = {
        0,
        INT32_MAX,
        INT32_MIN,   // delta = INT32_MIN - INT32_MAX = 环绕
        0,           // delta = 0 - INT32_MIN = 环绕
        -1,
        INT32_MAX,
        1,
        -1
    };
    auto bytes = to_bytes(data);

    // forward + inverse
    auto delta = hiss::apply_transform(hiss::TransformType::DELTA,
                                         bytes.data(), bytes.size(), sizeof(int32_t));
    auto restored = hiss::inverse_transform(hiss::TransformType::DELTA,
                                              delta.data(), delta.size(),
                                              sizeof(int32_t), delta.size());
    ASSERT_TRUE(bytes_equal(restored, bytes), "DELTA 边界: INT32_MIN/MAX 环绕往返一致");

    auto data_back = from_bytes<int32_t>(restored);
    bool values_ok = true;
    for (size_t i = 0; i < data.size(); i++) {
        if (data[i] != data_back[i]) {
            fprintf(stderr, "  DELTA 边界 mismatch at %zu: got=%d want=%d\n",
                    i, data_back[i], data[i]);
            values_ok = false;
            break;
        }
    }
    ASSERT_TRUE(values_ok, "DELTA 边界: 所有 int32 值精确一致 (含 INT32_MIN/MAX 环绕)");

    // 同样测试 DELTA_VARINT
    auto dv = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                      bytes.data(), bytes.size(), sizeof(int32_t));
    auto rv = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                        dv.data(), dv.size(), sizeof(int32_t), 0);
    ASSERT_TRUE(bytes_equal(rv, bytes), "DELTA_VARINT 边界: INT32_MIN/MAX 环绕往返一致");

    auto data_back2 = from_bytes<int32_t>(rv);
    bool values_ok2 = true;
    for (size_t i = 0; i < data.size(); i++) {
        if (data[i] != data_back2[i]) {
            fprintf(stderr, "  DELTA_VARINT 边界 mismatch at %zu: got=%d want=%d\n",
                    i, data_back2[i], data[i]);
            values_ok2 = false;
            break;
        }
    }
    ASSERT_TRUE(values_ok2, "DELTA_VARINT 边界: 所有 int32 值精确一致 (含 INT32_MIN/MAX 环绕)");
}

// ============================================================================
// 测试 10: DELTA_VARINT 递增序列 — 小 delta 的 varint 压缩效果
// ============================================================================
static void test_delta_varint_incremental() {
    fprintf(stdout, "[TEST] DELTA_VARINT 递增序列: 小 delta 的 varint 压缩效果\n");

    // 递增序列: 0, 1, 2, 3, ..., 999 (delta 全为 1, varint 仅 1 字节)
    std::vector<int32_t> data(1000);
    for (int i = 0; i < 1000; i++) {
        data[i] = i;
    }
    auto bytes = to_bytes(data);

    // forward: delta 全为 1, zig-zag(1)=2, varint(2)=1 字节
    // 输出 = 4 (n_elements) + 1000 * 1 (varint) = 1004 字节
    // 原始 = 1000 * 4 = 4000 字节
    auto encoded = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                           bytes.data(), bytes.size(), sizeof(int32_t));
    ASSERT_TRUE(encoded.size() < bytes.size(),
                "DELTA_VARINT 递增序列: 编码后 < 原始 (有压缩效果)");
    fprintf(stdout, "  递增序列: 原始=%zu, DELTA_VARINT编码=%zu (%.1f%%)\n",
            bytes.size(), encoded.size(), 100.0 * encoded.size() / bytes.size());

    // 验证 n_elements 前缀
    uint32_t n = (uint32_t)encoded[0] | ((uint32_t)encoded[1] << 8) |
                 ((uint32_t)encoded[2] << 16) | ((uint32_t)encoded[3] << 24);
    ASSERT_EQ_INT(n, 1000u, "DELTA_VARINT: n_elements=1000");

    // inverse
    auto restored = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                              encoded.data(), encoded.size(),
                                              sizeof(int32_t), 0);
    ASSERT_TRUE(bytes_equal(restored, bytes), "DELTA_VARINT 递增序列: 往返一致");

    auto data_back = from_bytes<int32_t>(restored);
    bool values_ok = true;
    for (int i = 0; i < 1000; i++) {
        if (data[i] != data_back[i]) { values_ok = false; break; }
    }
    ASSERT_TRUE(values_ok, "DELTA_VARINT 递增序列: 所有值精确一致");

    // 测试负递减序列: 0, -1, -2, -3, ... (delta 全为 -1)
    std::vector<int32_t> neg_data(1000);
    for (int i = 0; i < 1000; i++) {
        neg_data[i] = -i;
    }
    auto neg_bytes = to_bytes(neg_data);

    auto neg_encoded = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                                neg_bytes.data(), neg_bytes.size(), sizeof(int32_t));
    ASSERT_TRUE(neg_encoded.size() < neg_bytes.size(),
                "DELTA_VARINT 负递减序列: 编码后 < 原始");

    auto neg_restored = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                                  neg_encoded.data(), neg_encoded.size(),
                                                  sizeof(int32_t), 0);
    ASSERT_TRUE(bytes_equal(neg_restored, neg_bytes), "DELTA_VARINT 负递减序列: 往返一致");
}

// ============================================================================
// 测试 11: DELTA_VARINT expected_output_size 校验
// ============================================================================
static void test_delta_varint_expected_size() {
    fprintf(stdout, "[TEST] DELTA_VARINT expected_output_size 校验\n");

    std::vector<int32_t> data(100);
    for (int i = 0; i < 100; i++) data[i] = i * 7;
    auto bytes = to_bytes(data);

    auto encoded = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                           bytes.data(), bytes.size(), sizeof(int32_t));

    // 正确的 expected_output_size
    auto r1 = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                        encoded.data(), encoded.size(),
                                        sizeof(int32_t), bytes.size());
    ASSERT_TRUE(!r1.empty(), "DELTA_VARINT: 正确 expected_output_size → 成功");

    // 错误的 expected_output_size
    auto r2 = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                        encoded.data(), encoded.size(),
                                        sizeof(int32_t), bytes.size() + 100);
    ASSERT_TRUE(r2.empty(), "DELTA_VARINT: 错误 expected_output_size → 返回空 (校验生效)");

    // expected_output_size=0 (自动确定)
    auto r3 = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                        encoded.data(), encoded.size(),
                                        sizeof(int32_t), 0);
    ASSERT_TRUE(!r3.empty(), "DELTA_VARINT: expected=0 → 自动确定成功");
    ASSERT_TRUE(bytes_equal(r3, bytes), "DELTA_VARINT: 自动确定大小往返一致");
}

// ============================================================================
// 测试 12: 单元素数据 — n_elements=1 的边界情况
// ============================================================================
static void test_single_element() {
    fprintf(stdout, "[TEST] 单元素数据: n_elements=1\n");

    // BYTE_SHUFFLE 单元素: no-op (只有一个元素, 无可重排)
    {
        float val = 3.14f;
        std::vector<uint8_t> bytes((uint8_t*)&val, (uint8_t*)&val + sizeof(float));
        auto sh = hiss::apply_transform(hiss::TransformType::BYTE_SHUFFLE,
                                          bytes.data(), bytes.size(), sizeof(float));
        ASSERT_TRUE(bytes_equal(sh, bytes), "BYTE_SHUFFLE 单元素: no-op");
        auto rs = hiss::inverse_transform(hiss::TransformType::BYTE_SHUFFLE,
                                            sh.data(), sh.size(), sizeof(float), sh.size());
        ASSERT_TRUE(bytes_equal(rs, bytes), "BYTE_SHUFFLE 单元素: 往返一致");
    }

    // DELTA 单元素: delta[0] = input[0] (无前驱, delta = input - 0 = input)
    {
        int32_t val = 42;
        std::vector<uint8_t> bytes((uint8_t*)&val, (uint8_t*)&val + sizeof(int32_t));
        auto dl = hiss::apply_transform(hiss::TransformType::DELTA,
                                          bytes.data(), bytes.size(), sizeof(int32_t));
        ASSERT_TRUE(bytes_equal(dl, bytes), "DELTA 单元素: delta == 原始 (第一个元素不变)");
        auto rd = hiss::inverse_transform(hiss::TransformType::DELTA,
                                            dl.data(), dl.size(), sizeof(int32_t), dl.size());
        ASSERT_TRUE(bytes_equal(rd, bytes), "DELTA 单元素: 往返一致");
    }

    // DELTA_VARINT 单元素
    {
        int32_t val = 42;
        std::vector<uint8_t> bytes((uint8_t*)&val, (uint8_t*)&val + sizeof(int32_t));
        auto dv = hiss::apply_transform(hiss::TransformType::DELTA_VARINT,
                                          bytes.data(), bytes.size(), sizeof(int32_t));
        // 输出: 4 (n_elements=1) + varint(zig_zag(42))
        ASSERT_EQ_INT(dv.size(), 5u, "DELTA_VARINT 单元素: 4 (前缀) + 1 (varint of 42)");

        auto rv = hiss::inverse_transform(hiss::TransformType::DELTA_VARINT,
                                            dv.data(), dv.size(), sizeof(int32_t), 0);
        ASSERT_TRUE(bytes_equal(rv, bytes), "DELTA_VARINT 单元素: 往返一致");
    }
}

// ============================================================================
// 测试 13: Writer/Reader 集成 — transform 端到端
// Writer 设置 transform, Reader 逆向还原, 验证数据一致
// ============================================================================
#ifdef HAS_ZSTD
static void test_writer_reader_transform() {
    fprintf(stdout, "[TEST] Writer/Reader 集成: transform 端到端\n");

    // 构造 Tile 数据
    const uint32_t nside = 64;
    const uint32_t tile_nside = hiss::compute_tile_nside(nside);
    const size_t n_leaf = 16;  // 4^2 (depth=2)

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = tile_nside;
    acc.parent_ipix = 7;
    acc.pixel_area = 1.0;
    acc.pixels.resize(n_leaf);
    std::mt19937 rng(42);
    for (size_t i = 0; i < n_leaf; i++) {
        acc.pixels[i].sum_flux = (double)(rng() % 10000) / 100.0;
        acc.pixels[i].sum_area = 1.0;  // 全部有效 (FULL 模式)
        acc.pixels[i].n_contrib = 1;
    }

    // 保存原始 signal 用于后续比较
    std::vector<float> original_signal;
    acc.finalize_signal(original_signal);

    hiss::HissGridSpec grid;
    grid.nside = nside;
    grid.tile_nside = tile_nside;
    grid.ordering = 1;  // NESTED
    grid.radesys = 0;   // ICRS
    grid.pixfrac = 1.0;

    hiss::HissMetadata meta;
    meta.nside = nside;
    meta.tile_nside = tile_nside;
    std::strncpy(meta.object, "TransformTest", sizeof(meta.object) - 1);
    meta.exptime = 60.0;
    meta.photappl = 1;
    meta.photscal = 1.0;

    std::string path = "test_transform_writer_reader.hiss";

    // --- Writer: 设置 ZSTD + BYTE_SHUFFLE ---
    {
        hiss::HissWriter w;
        ASSERT_EQ_INT(w.open(path, grid, meta), 0, "Writer.open 成功");

        // 设置 signal 子块使用 ZSTD + BYTE_SHUFFLE
        w.set_experiment_codec(hiss::SubblockType::SIGNAL,
                                hiss::CodecId::ZSTD, hiss::TransformId::BYTE_SHUFFLE);
        // 设置 support 子块使用 ZSTD + DELTA (uint8, DELTA 有效)
        w.set_experiment_codec(hiss::SubblockType::SUPPORT,
                                hiss::CodecId::ZSTD, hiss::TransformId::DELTA);

        ASSERT_EQ_INT(w.add_tile(acc.parent_ipix, acc, nullptr, hiss::OccupancyMode::FULL), 0,
                      "Writer.add_tile 成功 (含 transform)");
        ASSERT_EQ_INT(w.finalize(), 0, "Writer.finalize 成功");
    }

    // --- Reader: 读取并验证 ---
    {
        hiss::HissReader r;
        ASSERT_EQ_INT(r.open(path), 0, "Reader.open 成功");

        std::vector<float> signal;
        std::vector<uint8_t> support;
        int ret = r.read_tile(7, signal, support);
        ASSERT_EQ_INT(ret, 0, "Reader.read_tile 成功 (含 inverse transform)");

        // 验证 signal 数据一致
        ASSERT_EQ_INT(signal.size(), original_signal.size(), "signal 数组长度一致");
        bool signal_ok = true;
        for (size_t i = 0; i < signal.size() && i < original_signal.size(); i++) {
            if (signal[i] != original_signal[i]) {
                fprintf(stderr, "  signal mismatch at %zu: got=%g want=%g\n",
                        i, signal[i], original_signal[i]);
                signal_ok = false;
                break;
            }
        }
        ASSERT_TRUE(signal_ok, "ZSTD+BYTE_SHUFFLE: signal 数据精确一致");

        // 验证 support 数据一致
        std::vector<uint8_t> expected_support;
        acc.finalize_support(expected_support);
        ASSERT_EQ_INT(support.size(), expected_support.size(), "support 数组长度一致");
        bool support_ok = true;
        for (size_t i = 0; i < support.size() && i < expected_support.size(); i++) {
            if (support[i] != expected_support[i]) {
                fprintf(stderr, "  support mismatch at %zu: got=%u want=%u\n",
                        i, (unsigned)support[i], (unsigned)expected_support[i]);
                support_ok = false;
                break;
            }
        }
        ASSERT_TRUE(support_ok, "ZSTD+DELTA: support 数据精确一致");

        r.close();
    }

    // 清理
    std::remove(path.c_str());

    // --- 测试 set_experiment_transform (仅设 transform, 保留 RAW codec) ---
    {
        hiss::HissWriter w;
        ASSERT_EQ_INT(w.open(path, grid, meta), 0, "Writer.open 成功 (set_experiment_transform)");

        // 仅设置 transform, codec 默认 RAW
        w.set_experiment_transform(hiss::SubblockType::SIGNAL, hiss::TransformId::BYTE_SHUFFLE);
        w.set_experiment_transform(hiss::SubblockType::SUPPORT, hiss::TransformId::DELTA_VARINT);

        ASSERT_EQ_INT(w.add_tile(acc.parent_ipix, acc, nullptr, hiss::OccupancyMode::FULL), 0,
                      "Writer.add_tile 成功 (RAW + transform)");
        ASSERT_EQ_INT(w.finalize(), 0, "Writer.finalize 成功");
    }

    {
        hiss::HissReader r;
        ASSERT_EQ_INT(r.open(path), 0, "Reader.open 成功 (RAW + transform)");

        std::vector<float> signal;
        std::vector<uint8_t> support;
        int ret = r.read_tile(7, signal, support);
        ASSERT_EQ_INT(ret, 0, "Reader.read_tile 成功 (RAW + inverse transform)");

        // 验证 signal (RAW + BYTE_SHUFFLE)
        bool signal_ok = true;
        for (size_t i = 0; i < signal.size() && i < original_signal.size(); i++) {
            if (signal[i] != original_signal[i]) { signal_ok = false; break; }
        }
        ASSERT_TRUE(signal_ok, "RAW+BYTE_SHUFFLE: signal 数据精确一致");

        // 验证 support (RAW + DELTA_VARINT, uint8)
        std::vector<uint8_t> expected_support;
        acc.finalize_support(expected_support);
        bool support_ok = true;
        for (size_t i = 0; i < support.size() && i < expected_support.size(); i++) {
            if (support[i] != expected_support[i]) { support_ok = false; break; }
        }
        ASSERT_TRUE(support_ok, "RAW+DELTA_VARINT: support 数据精确一致");

        r.close();
    }

    std::remove(path.c_str());
}
#endif // HAS_ZSTD

// ============================================================================
// 主入口: 运行所有测试
// ============================================================================
int main() {
    setvbuf(stdout, NULL, _IONBF, 0);  // 禁用 stdout 缓冲, 确保测试输出立即可见
    fprintf(stdout, "============================================================\n");
    fprintf(stdout, "WP-G 步骤12: Transform 正式路径单元测试\n");
    fprintf(stdout, "============================================================\n");
    fprintf(stdout, "依据: spec.md 步骤12, 02_FROZEN §15\n");
    fprintf(stdout, "      00_COMMON_CONTRACTS §5.1\n");
    fprintf(stdout, "============================================================\n\n");

    // 基础 transform 测试 (不需要 ZSTD/Writer/Reader)
    test_none_transform();
    test_byte_shuffle_float32();
    test_delta_int32();
    test_delta_varint_int32();
    test_empty_data();
    test_different_element_sizes();
    test_enum_conversion();
    test_delta_boundary();
    test_delta_varint_incremental();
    test_delta_varint_expected_size();
    test_single_element();

    // ZSTD + Writer/Reader 集成测试 (需要 HAS_ZSTD)
#ifdef HAS_ZSTD
    test_zstd_shuffle_combo();
    test_writer_reader_transform();
#else
    SKIP("ZSTD+shuffle 组合测试 (需 -DHAS_ZSTD 编译)");
    SKIP("Writer/Reader 集成测试 (需 -DHAS_ZSTD 编译)");
#endif

    fprintf(stdout, "\n------------------------------------------------------------\n");
    fprintf(stdout, "总计: PASS=%d, FAIL=%d, SKIP=%d\n",
            g_pass_count, g_fail_count, g_skip_count);
    fprintf(stdout, "------------------------------------------------------------\n");

    if (g_fail_count == 0) {
        fprintf(stdout, "[RESULT] 全部通过 (跳过 %d 项因缺少可选依赖)\n", g_skip_count);
        return 0;
    } else {
        fprintf(stderr, "[RESULT] 有失败用例\n");
        return 1;
    }
}
