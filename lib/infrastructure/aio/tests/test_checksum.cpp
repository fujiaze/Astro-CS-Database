// ============================================================================
// test_checksum.cpp - HISS Checksum 候选注册机制测试 (Phase E, DQ-006)
//
// 依据:
// - hiss_format.h §13.1 ChecksumRegistry (INTERIM_BASELINE_NOT_FROZEN)
// - 02_FROZEN_STAGE1_HISS_SPEC.md §15 (子块目录 checksum_type/checksum 字段)
// - docs/stage1_fix/00_COMMON_CONTRACTS.md §5.1 (测试要求: 零软通过)
//
// 测试范围 (4 个测试用例):
// 01. Writer 启用 CRC32C → 写入 Tile → Reader 读取 → 校验通过 (正常往返)
// 02. Writer 启用 CRC32C → 写入文件 → 手动篡改压缩数据 → Reader 读取 → 返回 -5
// 03. Writer 不启用 checksum (默认 NONE) → Reader 读取 → 正常 (向后兼容)
// 04. ChecksumRegistry 注册/查找/列出 API 测试 + CRC32C 已知向量校验
//
// 编译 (PowerShell + mingw64):
// $env:Path = "C:\msys64\mingw64\bin;$env:Path"
// cd "f:\Astro dev\Astro CS Normalization Database\lib\astro_image_io"
// g++ -std=c++17 -O2 -Iinclude -Isrc `
// tests/test_checksum.cpp `
// src/hiss_codec.cpp src/hiss_common.cpp `
// src/hiss_tile_model.cpp src/hiss_transform.cpp `
// src/hiss_writer.cpp src/hiss_stream_writer.cpp `
// src/hiss_reader.cpp `
// -lm -o tests/test_checksum.exe
//
// 运行:
// ./tests/test_checksum.exe
//
// 测试框架: 自维护通过/失败计数, 任何契约不满足必须真正失败
// (禁止 ASSERT_TRUE(true, "已知问题") 软通过, 依据 spec.md §3 步骤15)
// ============================================================================

#include "hiss_format.h"
#include "hiss_tile_model.h"  // compute_tile_nside

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>

// ============================================================================
// 测试框架: 计数 + 断言宏
// ============================================================================
static int g_pass_count = 0;
static int g_fail_count = 0;

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

// ============================================================================
// 辅助: 构造测试用累加器 (FULL 模式, 全部像素有效)
// ============================================================================
static hiss::DrizzleTileAccumulator make_full_acc(uint32_t tile_nside,
                                                    uint64_t parent_ipix,
                                                    uint32_t n_leaf,
                                                    double pixel_area) {
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = tile_nside;
    acc.parent_ipix = parent_ipix;
    acc.pixel_area = pixel_area;
    acc.pixels.resize(n_leaf);
    for (uint32_t i = 0; i < n_leaf; i++) {
        acc.pixels[i].sum_flux = (double)i * 10.0;
        acc.pixels[i].sum_area = pixel_area;  // 100% 覆盖 → FULL
        acc.pixels[i].n_contrib = 1;
    }
    return acc;
}

// ============================================================================
// 辅助: 构造测试网格和元数据
// ============================================================================
static void make_test_grid_meta(uint32_t nside, hiss::HissGridSpec& grid,
                                  hiss::HissMetadata& meta) {
    grid.nside = nside;
    grid.tile_nside = hiss::compute_tile_nside(nside);
    grid.ordering = 1;  // NESTED
    grid.radesys = 0;   // ICRS
    grid.pixfrac = 1.0;

    meta.nside = grid.nside;
    meta.tile_nside = grid.tile_nside;
    meta.photappl = 1;
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");
}

// ============================================================================
// 测试 01: Writer 启用 CRC32C → 写入 Tile → Reader 读取 → 校验通过
// 验证: set_experiment_checksum(CRC32C) → 写入 → 读取成功, 数据一致
// 覆盖: Writer 端 checksum 计算, Reader 端 checksum 校验, 数据往返一致
// ============================================================================
static void test_01_crc32c_roundtrip() {
    fprintf(stdout, "\n[TEST 01] Writer 启用 CRC32C → 写入 → Reader 读取 → 校验通过\n");

    using namespace hiss;
    const char* path = "test_checksum_crc32c_roundtrip.hiss";

    HissGridSpec grid;
    HissMetadata meta;
    make_test_grid_meta(64, grid, meta);  // nside=64, tile_nside=16, n_leaf=16

    const uint32_t n_leaf = 16;
    const double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);
    DrizzleTileAccumulator acc = make_full_acc(16, 42, n_leaf, A_p);

    // 保存原始 signal 用于后续比较
    std::vector<float> original_signal;
    acc.finalize_signal(original_signal);

    // --- Writer: 对所有子块启用 CRC32C checksum ---
    {
        HissWriter w;
        ASSERT_EQ_INT(w.open(path, grid, meta), 0, "Writer.open 成功");

        // 对 SIGNAL/SUPPORT 子块启用 CRC32C (INTERIM_BASELINE_NOT_FROZEN)
        w.set_experiment_checksum(SubblockType::SIGNAL, ChecksumType::CRC32C);
        w.set_experiment_checksum(SubblockType::SUPPORT, ChecksumType::CRC32C);

        ASSERT_EQ_INT(w.add_tile(42, acc, nullptr, OccupancyMode::FULL), 0,
                      "Writer.add_tile 成功 (含 CRC32C checksum)");
        ASSERT_EQ_INT(w.finalize(), 0, "Writer.finalize 成功");
    }

    // --- Reader: 读取并验证 ---
    {
        HissReader r;
        ASSERT_EQ_INT(r.open(path), 0, "Reader.open 成功");
        ASSERT_EQ_INT(r.tiles().size(), 1u, "Tile 数 = 1");

        // 验证 Tile 目录中 SIGNAL 子块的 checksum_type == CRC32C
        const HissTile& tile = r.tiles()[0];
        const HissSubblockDescriptor* sig_desc = nullptr;
        const HissSubblockDescriptor* sup_desc = nullptr;
        for (const auto& sb : tile.subblocks) {
            if (sb.type == SubblockType::SIGNAL) sig_desc = &sb;
            if (sb.type == SubblockType::SUPPORT) sup_desc = &sb;
        }
        ASSERT_TRUE(sig_desc != nullptr, "SIGNAL 子块存在");
        ASSERT_TRUE(sup_desc != nullptr, "SUPPORT 子块存在");
        if (sig_desc) {
            ASSERT_EQ_INT((int)sig_desc->checksum_type, (int)ChecksumType::CRC32C,
                          "SIGNAL 子块 checksum_type = CRC32C");
            ASSERT_TRUE(sig_desc->checksum != 0, "SIGNAL 子块 checksum 非零 (已计算)");
        }
        if (sup_desc) {
            ASSERT_EQ_INT((int)sup_desc->checksum_type, (int)ChecksumType::CRC32C,
                          "SUPPORT 子块 checksum_type = CRC32C");
            ASSERT_TRUE(sup_desc->checksum != 0, "SUPPORT 子块 checksum 非零 (已计算)");
        }

        // 读取 Tile 数据 (Reader 会校验 checksum, 校验通过返回 0)
        std::vector<float> signal;
        std::vector<uint8_t> support;
        int ret = r.read_tile(42, signal, support);
        ASSERT_EQ_INT(ret, 0, "Reader.read_tile 成功 (CRC32C 校验通过)");

        // 验证 signal 数据一致
        if (ret == 0) {
            ASSERT_EQ_INT(signal.size(), original_signal.size(), "signal 长度一致");
            bool signal_ok = true;
            for (size_t i = 0; i < signal.size() && i < original_signal.size(); i++) {
                if (std::fabs((double)signal[i] - (double)original_signal[i]) > 1e-3) {
                    fprintf(stderr, "  signal mismatch at %zu: got=%g want=%g\n",
                            i, signal[i], original_signal[i]);
                    signal_ok = false;
                    break;
                }
            }
            ASSERT_TRUE(signal_ok, "CRC32C: signal 数据精确一致");
        }

        r.close();
    }

    std::filesystem::remove(path);
}

// ============================================================================
// 测试 02: Writer 启用 CRC32C → 写入文件 → 手动篡改压缩数据 → Reader 返回 -5
// 验证: 篡改压缩数据后, Reader 校验失败返回 -5 (HISS_ERR_FORMAT)
// 覆盖: checksum 损坏检测能力 (核心安全保证)
// ============================================================================
static void test_02_crc32c_tamper_detection() {
    fprintf(stdout, "\n[TEST 02] Writer 启用 CRC32C → 篡改压缩数据 → Reader 返回 -5\n");

    using namespace hiss;
    const char* path = "test_checksum_tamper.hiss";

    HissGridSpec grid;
    HissMetadata meta;
    make_test_grid_meta(64, grid, meta);

    const uint32_t n_leaf = 16;
    const double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);
    DrizzleTileAccumulator acc = make_full_acc(16, 42, n_leaf, A_p);

    // --- Writer: 启用 CRC32C 写入 ---
    {
        HissWriter w;
        ASSERT_EQ_INT(w.open(path, grid, meta), 0, "Writer.open 成功");
        w.set_experiment_checksum(SubblockType::SIGNAL, ChecksumType::CRC32C);
        w.set_experiment_checksum(SubblockType::SUPPORT, ChecksumType::CRC32C);
        ASSERT_EQ_INT(w.add_tile(42, acc, nullptr, OccupancyMode::FULL), 0,
                      "Writer.add_tile 成功");
        ASSERT_EQ_INT(w.finalize(), 0, "Writer.finalize 成功");
    }

    // --- 步骤 1: 读取 Tile 目录, 获取 SIGNAL 子块的 offset 和 compressed_size ---
    uint64_t sig_offset = 0;
    uint64_t sig_comp_size = 0;
    {
        HissReader r;
        ASSERT_EQ_INT(r.open(path), 0, "Reader.open 成功 (篡改前)");
        const HissTile& tile = r.tiles()[0];
        for (const auto& sb : tile.subblocks) {
            if (sb.type == SubblockType::SIGNAL) {
                sig_offset = sb.offset;
                sig_comp_size = sb.compressed_size;
                break;
            }
        }
        ASSERT_TRUE(sig_comp_size > 0, "SIGNAL 子块 compressed_size > 0");
        r.close();
    }

    // --- 步骤 2: 手动篡改文件中 SIGNAL 子块的压缩数据 ---
    // 修改 offset 位置的 1 个字节 (在 compressed 数据范围内)
    // 注意: 不能篡改 Header (Header 在 offset 之前), 只篡改 attachment 数据
    {
        std::fstream fs(path, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.good(), "打开文件用于篡改成功");

        fs.seekg((std::streamoff)sig_offset, std::ios::beg);
        uint8_t orig_byte = 0;
        fs.read((char*)&orig_byte, 1);
        ASSERT_TRUE(fs.good(), "读取原始字节成功");

        // 翻转所有位, 确保篡改后值不同
        uint8_t tampered_byte = (uint8_t)(orig_byte ^ 0xFF);
        // 如果翻转后恰好相同 (理论不可能), 用 0x00
        if (tampered_byte == orig_byte) tampered_byte = 0x00;

        fs.seekp((std::streamoff)sig_offset, std::ios::beg);
        fs.write((const char*)&tampered_byte, 1);
        ASSERT_TRUE(fs.good(), "写入篡改字节成功");
        fs.flush();
        fs.close();

        fprintf(stdout, "  已篡改 SIGNAL 子块 offset=%llu 处字节: 0x%02X → 0x%02X\n",
                (unsigned long long)sig_offset, (unsigned)orig_byte,
                (unsigned)tampered_byte);
    }

    // --- 步骤 3: Reader 读取, 应返回 -5 (checksum 校验失败) ---
    {
        HissReader r;
        ASSERT_EQ_INT(r.open(path), 0, "Reader.open 成功 (篡改后, open 不校验 checksum)");

        std::vector<float> signal;
        std::vector<uint8_t> support;
        int ret = r.read_tile(42, signal, support);
        // 期望返回 -5 (HISS_ERR_FORMAT, checksum 校验失败)
        ASSERT_EQ_INT(ret, -5, "Reader.read_tile 返回 -5 (CRC32C 校验失败, 检测到篡改)");

        r.close();
    }

    std::filesystem::remove(path);
}

// ============================================================================
// 测试 03: Writer 不启用 checksum (默认 NONE) → Reader 读取 → 正常 (向后兼容)
// 验证: 默认 checksum_type=NONE, Reader 不校验, 数据正常读取
// 覆盖: 向后兼容性 (不破坏现有行为)
// ============================================================================
static void test_03_default_none_backward_compatible() {
    fprintf(stdout, "\n[TEST 03] Writer 默认 NONE → Reader 读取 → 正常 (向后兼容)\n");

    using namespace hiss;
    const char* path = "test_checksum_none_default.hiss";

    HissGridSpec grid;
    HissMetadata meta;
    make_test_grid_meta(64, grid, meta);

    const uint32_t n_leaf = 16;
    const double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);
    DrizzleTileAccumulator acc = make_full_acc(16, 42, n_leaf, A_p);

    std::vector<float> original_signal;
    acc.finalize_signal(original_signal);

    // --- Writer: 不设置 checksum (默认 NONE) ---
    {
        HissWriter w;
        ASSERT_EQ_INT(w.open(path, grid, meta), 0, "Writer.open 成功");
        // 不调用 set_experiment_checksum, 默认所有子块 checksum_type=NONE
        ASSERT_EQ_INT(w.add_tile(42, acc, nullptr, OccupancyMode::FULL), 0,
                      "Writer.add_tile 成功 (默认 NONE)");
        ASSERT_EQ_INT(w.finalize(), 0, "Writer.finalize 成功");
    }

    // --- Reader: 读取并验证 ---
    {
        HissReader r;
        ASSERT_EQ_INT(r.open(path), 0, "Reader.open 成功");

        // 验证 Tile 目录中所有子块 checksum_type == NONE
        const HissTile& tile = r.tiles()[0];
        bool all_none = true;
        for (const auto& sb : tile.subblocks) {
            if (sb.checksum_type != ChecksumType::NONE) {
                fprintf(stderr, "  子块 type=%u checksum_type=%u (期望 NONE)\n",
                        (unsigned)sb.type, (unsigned)sb.checksum_type);
                all_none = false;
                break;
            }
            ASSERT_EQ_INT((int)sb.checksum, 0, "默认 NONE 时 checksum 字段 = 0");
        }
        ASSERT_TRUE(all_none, "所有子块 checksum_type = NONE (默认)");

        // 读取 Tile 数据 (NONE 不校验, 应成功)
        std::vector<float> signal;
        std::vector<uint8_t> support;
        int ret = r.read_tile(42, signal, support);
        ASSERT_EQ_INT(ret, 0, "Reader.read_tile 成功 (NONE 不校验)");

        // 验证 signal 数据一致
        if (ret == 0) {
            bool signal_ok = true;
            for (size_t i = 0; i < signal.size() && i < original_signal.size(); i++) {
                if (std::fabs((double)signal[i] - (double)original_signal[i]) > 1e-3) {
                    signal_ok = false;
                    break;
                }
            }
            ASSERT_TRUE(signal_ok, "NONE: signal 数据精确一致 (向后兼容)");
        }

        r.close();
    }

    std::filesystem::remove(path);
}

// ============================================================================
// 测试 04: ChecksumRegistry 注册/查找/列出 API + CRC32C 已知向量校验
// 验证:
// a. CRC32C 内置注册 (find 返回非空)
// b. list 包含 CRC32C
// c. find(NONE) 返回 nullptr (NONE 不在注册表中)
// d. register_checksum(NONE) 返回 <0 (拒绝注册 NONE)
// e. register_checksum 注册自定义 checksum 后能 find 到
// f. CRC32C 计算结果与已知向量一致
// ============================================================================
static void test_04_checksum_registry_api() {
    fprintf(stdout, "\n[TEST 04] ChecksumRegistry 注册/查找/列出 API + CRC32C 向量校验\n");

    using namespace hiss;

    // a. CRC32C 内置注册
    const ChecksumEntry* crc32c_entry = ChecksumRegistry::instance().find(ChecksumType::CRC32C);
    ASSERT_TRUE(crc32c_entry != nullptr, "CRC32C 已内置注册 (find 返回非空)");
    if (crc32c_entry) {
        ASSERT_TRUE(crc32c_entry->name == "CRC32C", "CRC32C 名称正确");
        ASSERT_TRUE(crc32c_entry->compute != nullptr, "CRC32C compute 函数非空");
    }

    // b. list 包含 CRC32C
    std::vector<ChecksumType> list = ChecksumRegistry::instance().list();
    bool has_crc32c = false;
    for (ChecksumType ct : list) {
        if (ct == ChecksumType::CRC32C) has_crc32c = true;
    }
    ASSERT_TRUE(has_crc32c, "ChecksumRegistry::list 包含 CRC32C");
    ASSERT_TRUE(!list.empty(), "ChecksumRegistry::list 非空 (至少含 CRC32C)");

    // c. find(NONE) 返回 nullptr (NONE 不在注册表中, 是基线值)
    const ChecksumEntry* none_entry = ChecksumRegistry::instance().find(ChecksumType::NONE);
    ASSERT_TRUE(none_entry == nullptr, "find(NONE) 返回 nullptr (NONE 不在注册表)");

    // d. register_checksum(NONE) 返回 <0 (拒绝注册 NONE)
    {
        ChecksumEntry bad_entry;
        bad_entry.id = ChecksumType::NONE;
        bad_entry.name = "NONE";
        bad_entry.compute = [](const uint8_t*, size_t) -> uint64_t { return 0; };
        int ret = ChecksumRegistry::instance().register_checksum(bad_entry);
        ASSERT_TRUE(ret < 0, "register_checksum(NONE) 返回 <0 (拒绝)");
    }

    // e. register_checksum 注册自定义 checksum 后能 find 到
    // 使用 XXHASH 的占位实现 (简单异或, 仅用于验证注册机制, 非真正 XXHASH)
    {
        ChecksumEntry custom;
        custom.id = ChecksumType::XXHASH;
        custom.name = "XXHASH_PLACEHOLDER";
        custom.compute = [](const uint8_t* data, size_t size) -> uint64_t {
            uint64_t h = 0;
            for (size_t i = 0; i < size; i++) {
                h = h * 131 + data[i];  // 简单哈希, 仅用于测试注册机制
            }
            return h;
        };
        int ret = ChecksumRegistry::instance().register_checksum(custom);
        ASSERT_EQ_INT(ret, 0, "register_checksum(XXHASH 占位) 返回 0 (成功)");

        const ChecksumEntry* found = ChecksumRegistry::instance().find(ChecksumType::XXHASH);
        ASSERT_TRUE(found != nullptr, "find(XXHASH) 返回非空 (注册后可查找)");
        if (found) {
            ASSERT_TRUE(found->name == "XXHASH_PLACEHOLDER", "XXHASH 名称正确");
            ASSERT_TRUE(found->compute != nullptr, "XXHASH compute 函数非空");

            // 验证 compute 可调用
            uint8_t test_data[] = {1, 2, 3, 4, 5};
            uint64_t h = found->compute(test_data, 5);
            ASSERT_TRUE(h != 0, "XXHASH 占位实现返回非零 (可调用)");
        }

        // 验证 list 现在包含 CRC32C 和 XXHASH
        std::vector<ChecksumType> list2 = ChecksumRegistry::instance().list();
        bool has_xxhash = false;
        bool has_crc32c2 = false;
        for (ChecksumType ct : list2) {
            if (ct == ChecksumType::XXHASH) has_xxhash = true;
            if (ct == ChecksumType::CRC32C) has_crc32c2 = true;
        }
        ASSERT_TRUE(has_xxhash, "list 包含 XXHASH (注册后)");
        ASSERT_TRUE(has_crc32c2, "list 仍包含 CRC32C (内置未丢失)");
    }

    // f. CRC32C 计算结果与已知向量一致
    // Castagnoli CRC32C 标准测试向量:
    // crc32c("") = 0x00000000
    // crc32c("123456789") = 0xE3069283
    if (crc32c_entry) {
        // 空输入
        uint64_t empty_crc = crc32c_entry->compute(nullptr, 0);
        ASSERT_EQ_INT((long)empty_crc, 0L, "CRC32C(\"\") = 0 (空输入)");

        // "123456789" 标准测试向量
        const char* test_str = "123456789";
        uint64_t str_crc = crc32c_entry->compute((const uint8_t*)test_str, 9);
        ASSERT_EQ_INT((long)str_crc, 0xE3069283L,
                      "CRC32C(\"123456789\") = 0xE3069283 (Castagnoli 标准向量)");
    }
}

// ============================================================================
// 主入口: 运行所有测试
// ============================================================================
int main() {
    setvbuf(stdout, NULL, _IONBF, 0);  // 禁用 stdout 缓冲, 确保测试输出立即可见
    fprintf(stdout, "============================================================\n");
    fprintf(stdout, "Phase E: HISS Checksum 候选注册机制测试 (DQ-006)\n");
    fprintf(stdout, "INTERIM_BASELINE_NOT_FROZEN: 候选注册, 默认 NONE\n");
    fprintf(stdout, "============================================================\n");
    fprintf(stdout, "依据: hiss_format.h §13.1, 02_FROZEN §15\n");
    fprintf(stdout, "      00_COMMON_CONTRACTS §5.1 (零软通过)\n");
    fprintf(stdout, "============================================================\n");

    test_01_crc32c_roundtrip();
    test_02_crc32c_tamper_detection();
    test_03_default_none_backward_compatible();
    test_04_checksum_registry_api();

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
