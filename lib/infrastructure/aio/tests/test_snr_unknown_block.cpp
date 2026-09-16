// ============================================================================
// test_snr_unknown_block.cpp - WP-F 步骤13: SNR 子块布局与未知必需子块拒绝测试
//
// 依据:
// - docs/stage1_fix/spec.md 步骤13
// - 02_FROZEN_STAGE1_HISS_SPEC.md §17 (SNR 控制点), §13 (独立子块)
// - docs/stage1_fix/00_COMMON_CONTRACTS.md §2.5, §3.3
//
// 测试范围:
// 1. SNR 往返: 10 个控制点 → n_points 一致, 每点 local_ipix 和 snr 一致
// 2. SNR 往返: 0 个控制点 → n_points=0
// 3. SNR 往返: 1000 个控制点 → 全部一致
// 4. 未知必需子块: Reader.open 返回 HISS_ERR_UNKNOWN_REQUIRED (-7)
// 5. 未知可选子块: Reader.open 成功, 跳过该子块
// 6. SNR 布局验证: SNR 子块 uncompressed_size = 12 + n_points*8
// (: estimator_id(4) + sampling_scale(4) + n_points(4) + points(n*8))
//
// 编译 (PowerShell + mingw64):
// $env:Path = "C:\msys64\mingw64\bin;$env:Path"
// cd "f:\Astro dev\Astro CS Normalization Database\lib\astro_image_io\tests"
// g++ -std=c++17 -O2 -DHAS_LZ4 -DHAS_ZSTD `
// -I../include -I../src `
// test_snr_unknown_block.cpp `
// ../src/hiss_writer.cpp ../src/hiss_stream_writer.cpp ../src/hiss_reader.cpp `
// ../src/hiss_codec.cpp ../src/hiss_common.cpp `
// ../src/hiss_tile_model.cpp ../src/hiss_transform.cpp `
// -llz4 -lzstd -lm `
// -o test_snr_unknown_block.exe
// ./test_snr_unknown_block.exe
//
// 测试框架: 自维护通过/失败计数, 任何契约不满足必须真正失败
// (禁止 ASSERT_TRUE(true, "已知问题") 软通过, 依据 spec.md §3 步骤15)
// ============================================================================

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <random>
#include <algorithm>

#include "hiss_format.h"     // HissWriter, HissReader, HissSnrBlock 等
#include "hiss_tile_model.h" // compute_tile_nside

// ============================================================================
// 测试框架: 计数 + 断言宏
// ============================================================================
static int g_pass_count = 0;
static int g_fail_count = 0;

// ASSERT_TRUE(cond, msg): cond 为假时记失败并打印, 并从当前函数返回
// 注意: 不支持 "true, 已知问题" 软通过 — 任何 false 都是真失败
#define ASSERT_TRUE(cond, msg) do { \
    if (cond) { \
        g_pass_count++; \
        fprintf(stdout, "  OK: %s\n", msg); \
    } else { \
        g_fail_count++; \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
        return; \
    } \
} while (0)

// ASSERT_EQ_INT(a, b, msg): 整数相等, 不等时返回
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
        return; \
    } \
} while (0)

// ASSERT_EQ_FLT(a, b, tol, msg): 浮点数近似相等, 不等时返回
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
        return; \
    } \
} while (0)

// ============================================================================
// 辅助: 构造简单的 DrizzleTileAccumulator (用于 Writer.add_tile)
// tile_nside=16, n_leaf 个叶像素, 每个像素 sum_flux=50, sum_area=1.0
// ============================================================================
static hiss::DrizzleTileAccumulator make_simple_accumulator(uint64_t parent_ipix,
                                                              size_t n_leaf) {
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside  = 16;
    acc.parent_ipix = parent_ipix;
    acc.pixel_area  = 1.0;
    acc.pixels.resize(n_leaf);
    for (size_t i = 0; i < n_leaf; i++) {
        acc.pixels[i].sum_flux  = 50.0;
        acc.pixels[i].sum_area  = 1.0;
        acc.pixels[i].n_contrib = 1;
    }
    return acc;
}

// ============================================================================
// 辅助: 模拟 Writer 的 SNR 排序去重行为, 生成预期结果
// 按 local_ipix 升序排序, 重复点保留首次出现
// ============================================================================
static std::vector<hiss::HissSnrControlPoint> expected_after_writer(
    const std::vector<hiss::HissSnrControlPoint>& input) {
    std::vector<hiss::HissSnrControlPoint> sorted = input;
    std::sort(sorted.begin(), sorted.end(),
              [](const hiss::HissSnrControlPoint& a, const hiss::HissSnrControlPoint& b) {
                  return a.local_ipix < b.local_ipix;
              });
    auto last = std::unique(sorted.begin(), sorted.end(),
                            [](const hiss::HissSnrControlPoint& a, const hiss::HissSnrControlPoint& b) {
                                return a.local_ipix == b.local_ipix;
                            });
    sorted.erase(last, sorted.end());
    return sorted;
}

// ============================================================================
// 辅助: 解析新格式 HISS 文件的 TLV Header,
// 定位 TILE_DIRECTORY TLV value 的文件偏移
// 签名块(16B): magic[8] + header_length(u32 LE) + feature_flags(u32 LE)
// TLV Header 从 offset 16 开始: tag(u16)+flags(u8)+length(u32)+value
// 返回 TILE_DIRECTORY value 起始偏移, 失败返回 SIZE_MAX
// ============================================================================
static size_t find_tile_directory_value_pos(const std::vector<uint8_t>& file_data) {
    if (file_data.size() < 16) return SIZE_MAX;
    uint32_t header_length;
    std::memcpy(&header_length, file_data.data() + 8, 4);
    size_t pos = 16;  // TLV Header 起始
    size_t header_end = 16 + header_length;
    while (pos + 7 <= header_end) {
        uint16_t tag;
        std::memcpy(&tag, file_data.data() + pos, 2);
        uint8_t  flags = file_data[pos + 2];
        uint32_t length;
        std::memcpy(&length, file_data.data() + pos + 3, 4);
        pos += 7;  // 跳过 TLV 头
        if (tag == HISS_TLV_TILE_DIRECTORY) {
            return pos;  // value 起始位置
        }
        pos += length;  // 跳过 value
    }
    return SIZE_MAX;  // 未找到
}

// ============================================================================
// 辅助: 写入测试 HISS 文件 (含可选 SNR)
// 返回文件路径, 失败返回空字符串
// ============================================================================
static std::string write_test_hiss(const std::string& base_path,
                                    const hiss::DrizzleTileAccumulator& acc,
                                    const hiss::HissSnrBlock* snr) {
    std::string path = base_path + ".hiss";
    hiss::HissGridSpec grid;
    grid.nside      = 64;
    grid.tile_nside = hiss::compute_tile_nside(64);
    grid.ordering   = 1;  // NESTED
    grid.radesys    = 0;  // ICRS
    grid.pixfrac    = 1.0;

    hiss::HissMetadata meta;
    meta.nside      = grid.nside;
    meta.tile_nside = grid.tile_nside;
    std::strncpy(meta.object, "SnrUnknownBlockTest", sizeof(meta.object) - 1);
    meta.exptime = 60.0;
    // BUNIT=ASTROCS_RELATIVE_FLUX 要求 PHOTAPPL=TRUE (WP-C 校验)
    meta.photappl = 1;
    meta.photscal = 1.0;

    hiss::HissWriter w;
    if (w.open(path, grid, meta) != 0) return "";
    if (w.add_tile(acc.parent_ipix, acc, snr, hiss::OccupancyMode::FULL) != 0) return "";
    if (w.finalize() != 0) return "";
    return path;
}

// ============================================================================
// 测试 1: SNR 往返 — 10 个控制点
// Writer 写入 10 个控制点 → Reader 读取 → n_points 一致,
// 每点 local_ipix 和 snr 一致
// ============================================================================
static void test_snr_roundtrip_10_points() {
    fprintf(stdout, "[TEST] SNR 往返: 10 个控制点\n");

    // 构造 SNR 控制点 (确定性随机数)
    hiss::HissSnrBlock snr;
    std::mt19937 rng(42);
    for (int i = 0; i < 10; i++) {
        hiss::HissSnrControlPoint p;
        p.local_ipix = (uint32_t)(rng() % 1000);
        p.snr        = (float)(rng() % 10000) / 100.0f;
        snr.points.push_back(p);
    }

    // Writer 按升序排序并去重, 生成预期结果
    auto expected = expected_after_writer(snr.points);

    // 写入 HISS 文件
    hiss::DrizzleTileAccumulator acc = make_simple_accumulator(7, 256);
    std::string path = write_test_hiss("test_snr_10pts", acc, &snr);
    ASSERT_TRUE(!path.empty(), "写入含 10 个 SNR 控制点的 HISS 文件");

    // 读取并验证
    hiss::HissReader r;
    ASSERT_EQ_INT(r.open(path), 0, "Reader.open 成功");

    hiss::HissSnrBlock snr_read;
    int ret = r.read_tile_snr(7, snr_read);
    ASSERT_EQ_INT(ret, 0, "read_tile_snr 返回 0");

    ASSERT_EQ_INT(snr_read.points.size(), expected.size(), "n_points 一致 (排序去重后)");

    // 验证每个控制点的 local_ipix 和 snr (按 Writer 排序后的顺序)
    int mismatches = 0;
    for (size_t i = 0; i < expected.size() && i < snr_read.points.size(); i++) {
        if (snr_read.points[i].local_ipix != expected[i].local_ipix) {
            mismatches++;
            fprintf(stderr, "  点 %zu: local_ipix 不匹配 (got=%u, want=%u)\n",
                    i, snr_read.points[i].local_ipix, expected[i].local_ipix);
        }
        if (std::fabs(snr_read.points[i].snr - expected[i].snr) > 1e-6f) {
            mismatches++;
            fprintf(stderr, "  点 %zu: snr 不匹配 (got=%g, want=%g)\n",
                    i, snr_read.points[i].snr, expected[i].snr);
        }
    }
    ASSERT_EQ_INT(mismatches, 0, "所有控制点 local_ipix 和 snr 一致 (排序后)");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 2: SNR 往返 — 0 个控制点
// Writer 写入 0 个控制点 (HissSnrBlock 含空 points) → Reader 读取 → n_points=0
// ============================================================================
static void test_snr_roundtrip_0_points() {
    fprintf(stdout, "[TEST] SNR 往返: 0 个控制点\n");

    hiss::HissSnrBlock snr;  // points 为空
    ASSERT_EQ_INT(snr.points.size(), 0u, "构造时 n_points=0");

    hiss::DrizzleTileAccumulator acc = make_simple_accumulator(11, 256);
    std::string path = write_test_hiss("test_snr_0pts", acc, &snr);
    ASSERT_TRUE(!path.empty(), "写入含 0 个 SNR 控制点的 HISS 文件");

    hiss::HissReader r;
    ASSERT_EQ_INT(r.open(path), 0, "Reader.open 成功");

    hiss::HissSnrBlock snr_read;
    int ret = r.read_tile_snr(11, snr_read);
    ASSERT_EQ_INT(ret, 0, "read_tile_snr 返回 0");

    ASSERT_EQ_INT(snr_read.points.size(), 0u, "读取 n_points=0");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 3: SNR 往返 — 1000 个控制点
// Writer 写入 1000 个控制点 → Reader 读取 → 全部一致
// ============================================================================
static void test_snr_roundtrip_1000_points() {
    fprintf(stdout, "[TEST] SNR 往返: 1000 个控制点\n");

    hiss::HissSnrBlock snr;
    std::mt19937 rng(123);
    for (int i = 0; i < 1000; i++) {
        hiss::HissSnrControlPoint p;
        p.local_ipix = (uint32_t)(rng() % 100000);
        p.snr        = (float)(rng() % 100000) / 100.0f;
        snr.points.push_back(p);
    }

    // Writer 按升序排序并去重, 生成预期结果
    // 1000 个点中 local_ipix = rng % 100000, 可能有重复, 去重后 n_points < 1000
    auto expected = expected_after_writer(snr.points);

    hiss::DrizzleTileAccumulator acc = make_simple_accumulator(13, 256);
    std::string path = write_test_hiss("test_snr_1000pts", acc, &snr);
    ASSERT_TRUE(!path.empty(), "写入含 1000 个 SNR 控制点的 HISS 文件");

    hiss::HissReader r;
    ASSERT_EQ_INT(r.open(path), 0, "Reader.open 成功");

    hiss::HissSnrBlock snr_read;
    int ret = r.read_tile_snr(13, snr_read);
    ASSERT_EQ_INT(ret, 0, "read_tile_snr 返回 0");

    ASSERT_EQ_INT(snr_read.points.size(), expected.size(), "n_points 一致 (排序去重后)");

    // 验证全部一致 (按 Writer 排序后的顺序)
    int mismatches = 0;
    for (size_t i = 0; i < expected.size() && i < snr_read.points.size(); i++) {
        if (snr_read.points[i].local_ipix != expected[i].local_ipix) mismatches++;
        if (std::fabs(snr_read.points[i].snr - expected[i].snr) > 1e-6f) mismatches++;
    }
    ASSERT_EQ_INT(mismatches, 0, "控制点全部一致 (排序去重后)");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 4: 未知必需子块拒绝
// 构造含未知 required 子块的 HISS 文件 → Reader.open 返回 HISS_ERR_UNKNOWN_REQUIRED (-7)
//
// 实现方式: 直接修改现有 SUPPORT 子块描述符的 type 字节为未知值 (201),
// 保留其 REQUIRED flags。无需插入新描述符, 避免子块数据偏移问题。
//
// 新格式签名块(16B) = magic[8]+"HISS0100" + header_length(u32 LE) + feature_flags(u32 LE)
// Header 为 TLV 二进制结构 (tag+flags+length+value)
// 子块描述符 42B (含 ext_type_id)
// ============================================================================
static void test_unknown_required_reject() {
    fprintf(stdout, "[TEST] 未知必需子块拒绝\n");
    fflush(stdout);

    // 1. 写入正常 HISS 文件
    hiss::DrizzleTileAccumulator acc = make_simple_accumulator(5, 256);
    std::string path = write_test_hiss("test_unknown_req", acc, nullptr);
    ASSERT_TRUE(!path.empty(), "写入基础 HISS 文件");

    // 2. 读取文件内容, 修改 SUPPORT 子块描述符的 type 为未知值
    std::ifstream fin(path, std::ios::binary);
    ASSERT_TRUE(fin.is_open(), "打开文件进行修改");
    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(fin)),
                                    std::istreambuf_iterator<char>());
    fin.close();

    // 解析 TLV Header, 定位 TILE_DIRECTORY value
    size_t tile_dir_value_pos = find_tile_directory_value_pos(file_data);
    ASSERT_TRUE(tile_dir_value_pos != SIZE_MAX, "找到 TILE_DIRECTORY TLV");

    // TILE_DIRECTORY value 布局:
    // n_tiles(4) + [parent_ipix(8) + tile_nside(4) + occ_mode(1) + n_subblocks(2) + subblocks(42*n)]
    // 第一个 Tile 的 n_subblocks 在 tile_dir_value_pos + 4 + 13
    size_t n_subblocks_pos = tile_dir_value_pos + 4 + 13;
    uint16_t n_subblocks;
    std::memcpy(&n_subblocks, file_data.data() + n_subblocks_pos, 2);
    ASSERT_TRUE(n_subblocks >= 2, "原始 Tile 至少有 signal+support 子块");

    // 遍历子块描述符 (: 42B), 找到 SUPPORT (type=2) 并改为未知 type=201
    // 描述符布局(42B): type(1)+ext_type_id(2)+flags(2)+offset(8)+comp(8)+uncomp(8)+codec(2)+transform(2)+cksum_type(1)+checksum(8)
    size_t desc_start = tile_dir_value_pos + 4 + 15;  // 跳过 n_tiles + tile 头
    bool found_support = false;
    for (uint16_t i = 0; i < n_subblocks; i++) {
        size_t desc_off = desc_start + (size_t)i * 42;
        uint8_t type = file_data[desc_off];
        if (type == (uint8_t)hiss::SubblockType::SUPPORT) {
            file_data[desc_off] = 201;  // 改为未知 type, flags 保持 REQUIRED
            found_support = true;
            fprintf(stderr, "  修改 Tile 子块 %u: type SUPPORT(2) → 201(未知), flags 保持 REQUIRED\n", i);
            break;
        }
    }
    ASSERT_TRUE(found_support, "找到 SUPPORT 子块并修改为未知 type");

    // 写回文件
    std::ofstream fout(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(fout.is_open(), "打开文件进行写入");
    fout.write((const char*)file_data.data(), file_data.size());
    fout.close();

    // 3. Reader.open 应返回 HISS_ERR_UNKNOWN_REQUIRED (-7)
    hiss::HissReader r;
    int open_ret = r.open(path);
    fprintf(stderr, "  Reader.open 返回 %d (期望 %d)\n", open_ret, HISS_ERR_UNKNOWN_REQUIRED);
    ASSERT_EQ_INT(open_ret, HISS_ERR_UNKNOWN_REQUIRED,
                  "未知必需子块 → Reader.open 返回 HISS_ERR_UNKNOWN_REQUIRED (-7)");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 5: 未知可选子块跳过
// 构造含未知 optional 子块的 HISS 文件 → Reader.open 成功, 跳过该子块
// 读取 signal/support 仍正常工作
//
// 新格式 TLV Header + 42B 子块描述符
// ============================================================================
static void test_unknown_optional_skip() {
    fprintf(stdout, "[TEST] 未知可选子块跳过\n");
    fflush(stdout);

    // 1. 写入正常 HISS 文件
    hiss::DrizzleTileAccumulator acc = make_simple_accumulator(9, 256);
    std::string path = write_test_hiss("test_unknown_opt", acc, nullptr);
    ASSERT_TRUE(!path.empty(), "写入基础 HISS 文件");

    // 2. 读取文件内容, 追加一个未知可选子块描述符 (42B)
    std::ifstream fin(path, std::ios::binary);
    ASSERT_TRUE(fin.is_open(), "打开文件进行修改");
    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(fin)),
                                    std::istreambuf_iterator<char>());
    fin.close();

    // 解析 TLV Header, 定位 TILE_DIRECTORY value
    size_t tile_dir_value_pos = find_tile_directory_value_pos(file_data);
    ASSERT_TRUE(tile_dir_value_pos != SIZE_MAX, "找到 TILE_DIRECTORY TLV");

    // TILE_DIRECTORY value: n_tiles(4) + tile_prefix(15) + subblocks(42*n)
    size_t n_subblocks_pos = tile_dir_value_pos + 4 + 13;
    uint16_t n_subblocks;
    std::memcpy(&n_subblocks, file_data.data() + n_subblocks_pos, 2);
    ASSERT_TRUE(n_subblocks >= 2, "原始 Tile 至少有 signal+support 子块");

    size_t desc_start = tile_dir_value_pos + 4 + 15;  // 第一个描述符位置
    size_t subblocks_end = desc_start + (size_t)n_subblocks * 42;

    // 插入前, 更新现有子块描述符的 offset (+42)
    // 原因: 插入 42B 描述符后 Header 增长 42B, 子块数据区整体后移 42B
    // 描述符布局(42B): type(1)+ext_type_id(2)+flags(2)+offset(8)+...
    // offset 字段在描述符内偏移 5, 长度 8
    for (uint16_t i = 0; i < n_subblocks; i++) {
        size_t off_pos = desc_start + (size_t)i * 42 + 5;
        uint64_t old_off;
        std::memcpy(&old_off, file_data.data() + off_pos, 8);
        uint64_t new_off = old_off + 42;
        std::memcpy(file_data.data() + off_pos, &new_off, 8);
    }

    // 构造未知可选子块描述符 (: 42 字节)
    // type(1)+ext_type_id(2)+flags(2)+offset(8)+comp(8)+uncomp(8)+codec(2)+transform(2)+cksum_type(1)+checksum(8)
    uint8_t unknown_opt[42] = {0};
    unknown_opt[0] = 200;  // 未知 type
    // ext_type_id = 0 (offset 1-2, 已为 0)
    uint16_t opt_flags = (uint16_t)hiss::SubblockFlags::OPTIONAL;
    std::memcpy(unknown_opt + 3, &opt_flags, 2);  // flags (offset 3-4)
    // offset 指向文件末尾 (compressed_size=0, Reader 跳过越界检查)
    uint64_t fake_offset = file_data.size() + 42;  // 插入后的文件末尾
    std::memcpy(unknown_opt + 5, &fake_offset, 8);  // offset (offset 5-12)
    uint64_t fake_size = 0;
    std::memcpy(unknown_opt + 13, &fake_size, 8);   // compressed_size=0 (offset 13-20)
    std::memcpy(unknown_opt + 21, &fake_size, 8);   // uncompressed_size=0 (offset 21-28)

    // 插入到子块描述符末尾 (Header 和数据区交界处)
    file_data.insert(file_data.begin() + subblocks_end, unknown_opt, unknown_opt + 42);

    // 更新 n_subblocks (tile_dir_value_pos + 4 + 13)
    uint16_t new_n = (uint16_t)(n_subblocks + 1);
    std::memcpy(file_data.data() + n_subblocks_pos, &new_n, 2);

    // 更新 TILE_DIRECTORY TLV 的 length (+42)
    // TLV 头: tag(2) + flags(1) + length(4), value 在 tile_dir_value_pos
    // length 字段在 tile_dir_value_pos - 4
    size_t tlv_length_pos = tile_dir_value_pos - 4;
    uint32_t old_tlv_len;
    std::memcpy(&old_tlv_len, file_data.data() + tlv_length_pos, 4);
    uint32_t new_tlv_len = old_tlv_len + 42;
    std::memcpy(file_data.data() + tlv_length_pos, &new_tlv_len, 4);

    // 更新签名块中的 header_length (+42)
    // 签名块: magic[8] + header_length(u32 LE) @ offset 8 + feature_flags(u32 LE) @ offset 12
    uint32_t old_hlen;
    std::memcpy(&old_hlen, file_data.data() + 8, 4);
    uint32_t new_hlen = old_hlen + 42;
    std::memcpy(file_data.data() + 8, &new_hlen, 4);

    // 写回文件
    std::ofstream fout(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(fout.is_open(), "打开文件进行写入");
    fout.write((const char*)file_data.data(), file_data.size());
    fout.close();

    // 3. Reader.open 应成功 (跳过未知可选子块)
    hiss::HissReader r;
    int open_ret = r.open(path);
    ASSERT_EQ_INT(open_ret, 0, "未知可选子块 → Reader.open 成功 (跳过)");

    // 4. 读取 signal/support 仍正常工作
    std::vector<float> sig;
    std::vector<uint8_t> sup;
    int read_ret = r.read_tile(9, sig, sup);
    ASSERT_EQ_INT(read_ret, 0, "未知可选子块不影响 signal/support 读取");

    // 验证 signal 数据正确
    ASSERT_EQ_INT(sig.size(), 256u, "signal 数组长度正确 (256)");
    if (!sig.empty()) {
        ASSERT_EQ_FLT(sig[0], 50.0f, 1e-4f, "signal[0]=50.0 (数据正确)");
    }

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 6: SNR 布局验证
// 写入后检查文件中 SNR 子块的 uncompressed_size = 12 + n_points*8
// 冻结布局: estimator_id(4) + sampling_scale(4) + n_points(4) + points(n*8)
// 验证 SNR 子块不包含 snr_phot/median_snr/idw_power (旧布局会多 24 字节)
// ============================================================================
static void test_snr_layout_bytes() {
    fprintf(stdout, "[TEST] SNR 布局验证: uncompressed_size = 12 + n_points*8\n");

    // 构造 10 个控制点的 SNR
    hiss::HissSnrBlock snr;
    for (int i = 0; i < 10; i++) {
        hiss::HissSnrControlPoint p;
        p.local_ipix = (uint32_t)(i * 7);
        p.snr        = (float)(i * 1.5 + 10.0);
        snr.points.push_back(p);
    }

    hiss::DrizzleTileAccumulator acc = make_simple_accumulator(15, 256);
    std::string path = write_test_hiss("test_snr_layout", acc, &snr);
    ASSERT_TRUE(!path.empty(), "写入含 SNR 的 HISS 文件");

    hiss::HissReader r;
    ASSERT_EQ_INT(r.open(path), 0, "Reader.open 成功");

    // 查找 SNR 子块描述符
    const auto& tiles = r.tiles();
    ASSERT_EQ_INT(tiles.size(), 1u, "Tile 数量 == 1");

    const hiss::HissSubblockDescriptor* snr_desc = nullptr;
    for (const auto& sb : tiles[0].subblocks) {
        if (sb.type == hiss::SubblockType::SNR) {
            snr_desc = &sb;
            break;
        }
    }
    ASSERT_TRUE(snr_desc != nullptr, "找到 SNR 子块描述符");

    // 验证 uncompressed_size = 12 + n_points * 8 = 12 + 10*8 = 92
    // 冻结布局 (02_FROZEN §17): estimator_id(4) + sampling_scale(4) + n_points(4) + points(n*8)
    // 旧错误布局 (含 snr_phot/median_snr/idw_power) 会是 92 + 24 = 116
    uint64_t expected_size = 12 + (uint64_t)10 * 8;  // 92
    ASSERT_EQ_INT(snr_desc->uncompressed_size, (long)expected_size,
                  "SNR uncompressed_size = 12 + 10*8 = 92 (eid+scale+np+points)");

    // 明确验证: 不等于旧错误布局 (92 + 24 = 116)
    ASSERT_TRUE(snr_desc->uncompressed_size != 116,
                "SNR uncompressed_size != 116 (旧错误布局含 snr_phot/median_snr/idw_power)");

    fprintf(stderr, "  SNR uncompressed_size=%llu (期望 %llu, 旧错误布局会是 116)\n",
            (unsigned long long)snr_desc->uncompressed_size,
            (unsigned long long)expected_size);

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 主入口: 运行所有测试, 返回 0=全部通过, 非 0=有失败
// ============================================================================
int main() {
    setvbuf(stdout, NULL, _IONBF, 0);  // 禁用 stdout 缓冲, 确保测试输出立即可见
    fprintf(stdout, "============================================================\n");
    fprintf(stdout, "WP-F 步骤13: SNR 子块布局与未知必需子块拒绝测试\n");
    fprintf(stdout, "============================================================\n");
    fprintf(stdout, "依据: 02_FROZEN §17 (SNR), §13 (独立子块)\n");
    fprintf(stdout, "      00_COMMON_CONTRACTS §2.5, §3.3\n");
    fprintf(stdout, "      spec.md 步骤13\n");
    fprintf(stdout, "============================================================\n\n");

    test_snr_roundtrip_10_points();
    test_snr_roundtrip_0_points();
    test_snr_roundtrip_1000_points();
    test_unknown_required_reject();
    test_unknown_optional_skip();
    test_snr_layout_bytes();

    fprintf(stdout, "\n------------------------------------------------------------\n");
    fprintf(stdout, "总计: PASS=%d, FAIL=%d\n", g_pass_count, g_fail_count);
    fprintf(stdout, "------------------------------------------------------------\n");

    if (g_fail_count == 0) {
        fprintf(stdout, "[RESULT] 全部通过\n");
        return 0;
    } else {
        fprintf(stderr, "[RESULT] 有失败用例\n");
        return 1;
    }
}
