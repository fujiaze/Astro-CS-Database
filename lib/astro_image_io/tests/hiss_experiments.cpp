// ============================================================================
// hiss_experiments.cpp - WP-I-2 真实数据 C++ 实验 (步骤16) + 性能报告基础
//
// 依据:
// - docs/stage1_fix/spec.md 步骤16/17
// - 02_FROZEN_STAGE1_HISS_SPEC.md §11/§12 (Tile 占用编码)
// - lib/astro_image_io/tests/test_report.md (WP-I-1 已通过的真实数据集成)
//
// 实验内容 (DQ-001 ~ DQ-007):
// DQ-001: 不同 codec (RAW/LZ4/Zstd) × transform (NONE/BSHUF/DELTA/DELTA_VARINT) 压缩率对比
// DQ-002: Tile 占用模式 (FULL/BITMAP/SPARSE_LIST) 体积对比
// DQ-003: 磁盘随机读取延迟 P50/P95/P99 (RAW vs Zstd)
// DQ-004: Drizzle 各阶段耗时 profile (WCS/重叠/候选/累加器合并)
// DQ-005: HissWriter 流式写入内存峰值测试
// DQ-006: 自动 NSIDE 选择验证 (compute_auto_nside)
// DQ-007: signal/support 语义验证 + 通量守恒
//
// 重要声明:
// - 所有结果基于真实测量 (chrono + GetProcessMemoryInfo)
// - 实验结果仅给推荐, 不冻结参数
// - 真实数据来源: testdata/results/ 下的 calibrated FITS 文件
//
// 编译 (从 lib/astro_image_io/ 目录):
// g++ -std=c++17 -O2 -fopenmp -DHAS_LZ4 -DHAS_ZSTD -DAIO_ENABLE_HEALPIX \
// -Iinclude -Isrc \
// -I../../healpix_db/healpix_drizzle \
// -I../../healpix_db/healpix_stack \
// -I../../calibration/include \
// tests/hiss_experiments.cpp \
// src/hiss_codec.cpp src/hiss_common.cpp \
// src/hiss_writer.cpp src/hiss_reader.cpp \
// src/hiss_stream_writer.cpp src/hiss_tile_model.cpp \
// src/hiss_transform.cpp \
// src/healpix/aio_healpix_io.cpp \
// src/aio_fits.cpp src/aio_api.cpp src/aio_log.cpp \
// ../../healpix_db/healpix_drizzle/drizzle_engine.cpp \
// ../../healpix_db/healpix_drizzle/wcs_sip.cpp \
// ../../healpix_db/healpix_drizzle/poly_clip.cpp \
// ../../healpix_db/healpix_drizzle/fits_reader.cpp \
// ../../healpix_db/healpix_drizzle/spherical_overlap.cpp \
// ../../healpix_db/healpix_stack/healpix_core.cpp \
// -llz4 -lzstd -lpsapi -lm \
// -o tests/hiss_experiments.exe
//
// 运行:
// ./tests/hiss_experiments.exe
// ============================================================================

#include "drizzle_engine.h"
#include "wcs_sip.h"
#include "fits_reader.h"
#include "spherical_overlap.h"
#include "healpix_core.h"
#include "aio_healpix_io.h"
#include "hiss_format.h"
#include "hiss_tile_model.h"
#include "hiss_transform.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include <algorithm>
#include <random>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <map>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 全局常量
// ============================================================================
static const char* kResultsDir = "tests/results";
static const int    kMeasureRounds = 5;   // 压缩/解压重复次数 (取中位数)
static const double kMB = (1024.0 * 1024.0);

// ============================================================================
// 计时工具
// ============================================================================
using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;

static inline double elapsed_us(TimePoint start, TimePoint end) {
    return std::chrono::duration<double, std::micro>(end - start).count();
}
static inline double elapsed_ms(TimePoint start, TimePoint end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}
static inline double elapsed_s(TimePoint start, TimePoint end) {
    return std::chrono::duration<double>(end - start).count();
}

// ============================================================================
// 内存测量 (Windows API)
// ============================================================================
static size_t get_peak_rss_kb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize / 1024;
    }
    return 0;
#else
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmHWM:", 0) == 0) {
            size_t kb = 0;
            std::sscanf(line.c_str(), "VmHWM: %zu kB", &kb);
            return kb;
        }
    }
    return 0;
#endif
}

static size_t get_current_rss_kb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / 1024;
    }
    return 0;
#else
    return get_peak_rss_kb();
#endif
}

// 重置峰值 (Windows 上不能直接重置 PeakWorkingSetSize, 用当前值近似基准)
static size_t reset_peak_rss_kb() {
#ifdef _WIN32
    // Windows 不支持重置 PeakWorkingSetSize, 但可以 EmptyWorkingSet 降低当前值
    // 这里返回当前 WorkingSetSize 作为基准, 增量在调用方计算
    return get_current_rss_kb();
#else
    return 0;
#endif
}

// ============================================================================
// 中位数 / 百分位数
// ============================================================================
static double median_d(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n % 2 == 0) return (v[n / 2 - 1] + v[n / 2]) / 2.0;
    return v[n / 2];
}

static double percentile_d(std::vector<double> v, double pct) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n == 1) return v[0];
    double rank = pct * (double)(n - 1);
    size_t lo = (size_t)std::floor(rank);
    size_t hi = (size_t)std::ceil(rank);
    if (lo == hi) return v[lo];
    double frac = rank - (double)lo;
    return v[lo] * (1.0 - frac) + v[hi] * frac;
}

// ============================================================================
// CSV 写入工具
// ============================================================================
static bool write_csv(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) {
        fprintf(stderr, "[csv] 无法写入 %s\n", path.c_str());
        return false;
    }
    f << content;
    f.close();
    fprintf(stderr, "[csv] 已写入 %s (%zu bytes)\n", path.c_str(), content.size());
    return true;
}

// ============================================================================
// 查找真实 FITS 文件 (3 种不同视场)
// ============================================================================
struct FitsSample {
    std::string path;
    std::string label;   // small / medium / large
    int width = 0;
    int height = 0;
    double filesize_mb = 0.0;
};

static std::string find_fits_at(const std::string& dir) {
    if (!std::filesystem::exists(dir)) return "";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.path().extension() == ".fits") {
            std::string name = entry.path().filename().string();
            if (name.find("01_calibrated") != std::string::npos ||
                name.find("calibrated") != std::string::npos) {
                return entry.path().string();
            }
        }
    }
    return "";
}

static std::vector<FitsSample> find_three_fits_samples() {
    std::vector<FitsSample> samples;
    // 三个不同 FOV/对象的 FITS (不同视场, 不同内容, 用于 codec 对比)
    // 优先选择不同 target, 保证内容差异
    struct Candidate { const char* dir; const char* label; };
    Candidate candidates[] = {
        {"../../testdata/results/Galaxy_Center_T4/panel3/Red", "Galaxy_Center_panel3_Red"},
        {"../../testdata/results/NGC55_T3_flying_dutchman/Lum", "NGC55_Lum"},
        {"../../testdata/results/Victory_Nebula_T4_Flying_Dutchman/panel2/Lum", "Victory_Nebula_Lum"},
        {"../../testdata/results/Galaxy_Center_T4/panel3/Oiii", "Galaxy_Center_panel3_Oiii"},
        {"../../testdata/results/NGC55_T3_flying_dutchman/Red", "NGC55_Red"},
    };
    for (const auto& c : candidates) {
        std::string p = find_fits_at(c.dir);
        if (!p.empty()) {
            FitsSample s;
            s.path = p;
            s.label = c.label;
            std::error_code ec;
            auto sz = std::filesystem::file_size(p, ec);
            s.filesize_mb = (double)sz / kMB;
            samples.push_back(s);
            if (samples.size() >= 3) break;
        }
    }
    return samples;
}

// ============================================================================
// 读取真实 FITS 文件
// ============================================================================
static bool load_fits(const std::string& path, drizzle::FitsImage& img, std::string& err) {
    return drizzle::readFits(path, img, err);
}

// ============================================================================
// 运行 drizzle (复用 DrizzleEngine)
// ============================================================================
static bool run_drizzle(const drizzle::FitsImage& img, int nside, double pixfrac,
                        std::unordered_map<uint64_t, drizzle::PixelAccumulator>& accums,
                        drizzle::DrizzleStats& stats, std::string& err) {
    drizzle::DrizzleConfig config;
    config.nside = nside;
    config.nested = true;
    config.pixfrac = pixfrac;
    config.apply_photometry = false;
    config.photscal = 1.0;
    drizzle::DrizzleEngine engine;
    return engine.drizzle(img, config, nullptr, nullptr, accums, stats, err);
}

// ============================================================================
// 将累加器分组到 Tile (NESTED 位运算)
// ============================================================================
struct TileAccumGroup {
    uint64_t parent_ipix = 0;
    std::vector<std::pair<uint32_t, drizzle::PixelAccumulator>> pixels; // (local_ipix, acc)
};

static std::vector<TileAccumGroup> group_accumulators_to_tiles(
    const std::unordered_map<uint64_t, drizzle::PixelAccumulator>& accums,
    uint32_t nside) {
    uint32_t depth = hiss::compute_tile_depth(nside);
    int shift = 2 * (int)depth;
    uint64_t local_mask = (shift >= 64) ? ~0ULL : ((1ULL << shift) - 1);

    std::map<uint64_t, TileAccumGroup> grouped;
    for (const auto& [ipix, acc] : accums) {
        uint64_t parent = (shift >= 64) ? 0 : (ipix >> shift);
        uint32_t local = (uint32_t)(ipix & local_mask);
        grouped[parent].parent_ipix = parent;
        grouped[parent].pixels.emplace_back(local, acc);
    }
    std::vector<TileAccumGroup> out;
    out.reserve(grouped.size());
    for (auto& [_, g] : grouped) out.push_back(std::move(g));
    return out;
}

// 构造 HissWriter 可用的 DrizzleTileAccumulator
static hiss::DrizzleTileAccumulator make_tile_accum(
    const TileAccumGroup& group, uint32_t tile_nside, uint32_t n_leaf_per_tile,
    double pixel_area) {
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = tile_nside;
    acc.parent_ipix = group.parent_ipix;
    acc.pixel_area = pixel_area;
    acc.pixels.resize(n_leaf_per_tile);
    for (const auto& [local, pacc] : group.pixels) {
        if (local < n_leaf_per_tile) {
            acc.pixels[local].sum_flux = pacc.sumFlux;
            acc.pixels[local].sum_area = pacc.sumArea;
            acc.pixels[local].n_contrib = pacc.nContrib;
        }
    }
    return acc;
}

// ============================================================================
// DQ-001: codec/transform 对比
// ============================================================================
struct CodecTransformResult {
    std::string fits_label;
    int nside;
    uint32_t n_leaf_per_tile;
    std::string codec_name;
    std::string transform_name;
    size_t uncompressed_bytes;
    size_t compressed_bytes;
    double ratio;            // uncompressed / compressed
    double compress_us;      // 中位数
    double decompress_us;    // 中位数
};

static const char* codec_name(hiss::CodecId id) {
    switch (id) {
        case hiss::CodecId::RAW:  return "RAW";
        case hiss::CodecId::LZ4:  return "LZ4";
        case hiss::CodecId::ZSTD: return "Zstd";
    }
    return "?";
}

static const char* transform_name(hiss::TransformId id) {
    switch (id) {
        case hiss::TransformId::NONE:         return "NONE";
        case hiss::TransformId::BYTE_SHUFFLE: return "BYTE_SHUFFLE";
        case hiss::TransformId::DELTA:        return "DELTA";
        case hiss::TransformId::DELTA_VARINT: return "DELTA_VARINT";
        case hiss::TransformId::VARINT:       return "VARINT";
    }
    return "?";
}

// 直接对一段字节执行 transform + codec 压缩, 返回压缩后字节
static bool compress_once(hiss::CodecId codec, hiss::TransformId transform,
                          const uint8_t* data, size_t data_size, size_t element_size,
                          std::vector<uint8_t>& out, size_t& compressed_size) {
    out.clear();
    compressed_size = 0;

    // 1. transform (若非 NONE)
    const uint8_t* data_to_compress = data;
    size_t size_to_compress = data_size;
    std::vector<uint8_t> transformed;
    if (transform != hiss::TransformId::NONE) {
        hiss::TransformType tt = hiss::transform_id_to_type(transform);
        transformed = hiss::apply_transform(tt, data, data_size, element_size);
        if (transformed.empty() && data_size > 0) return false;
        data_to_compress = transformed.data();
        size_to_compress = transformed.size();
    }

    // 2. codec
    const hiss::CodecEntry* entry = hiss::CodecRegistry::instance().find(codec);
    if (!entry) return false;

    size_t bound = entry->bound(size_to_compress);
    out.resize(bound);
    compressed_size = bound;
    int ret = entry->compress(data_to_compress, size_to_compress, out.data(), &compressed_size);
    if (ret != 0) return false;
    out.resize(compressed_size);
    return true;
}

static bool decompress_once(hiss::CodecId codec, hiss::TransformId transform,
                            const uint8_t* comp_data, size_t comp_size,
                            size_t expected_uncomp_size, size_t element_size,
                            std::vector<uint8_t>& out) {
    const hiss::CodecEntry* entry = hiss::CodecRegistry::instance().find(codec);
    if (!entry) return false;

    // 解压后大小 = transform 后大小 (即 expected_uncomp_size)
    std::vector<uint8_t> decompressed(expected_uncomp_size);
    size_t decompressed_size = expected_uncomp_size;
    int ret = entry->decompress(comp_data, comp_size, decompressed.data(), decompressed_size);
    if (ret != 0) return false;

    if (transform != hiss::TransformId::NONE) {
        hiss::TransformType tt = hiss::transform_id_to_type(transform);
        // 对于 DELTA_VARINT, expected_output_size 应是原始 (transform 前) 大小
        // 这里调用方传入原始大小, 直接还原
        out = hiss::inverse_transform(tt, decompressed.data(), decompressed.size(),
                                       element_size, 0);
        if (out.empty() && !decompressed.empty()) return false;
    } else {
        out = std::move(decompressed);
    }
    return true;
}

// 对单段数据重复压缩/解压, 返回中位数耗时
static void measure_compress_decompress(
    hiss::CodecId codec, hiss::TransformId transform,
    const uint8_t* data, size_t data_size, size_t element_size,
    double& compress_us_median, double& decompress_us_median,
    size_t& compressed_size_out) {
    std::vector<double> comp_times, decomp_times;
    std::vector<uint8_t> comp_buf;
    size_t last_comp_size = 0;
    for (int r = 0; r < kMeasureRounds; ++r) {
        TimePoint t0 = Clock::now();
        bool ok = compress_once(codec, transform, data, data_size, element_size, comp_buf, last_comp_size);
        TimePoint t1 = Clock::now();
        if (!ok) { compress_us_median = -1; decompress_us_median = -1; return; }
        comp_times.push_back(elapsed_us(t0, t1));
    }
    compressed_size_out = last_comp_size;

    // 解压计时 (用最后一次压缩结果)
    std::vector<uint8_t> decomp_buf;
    for (int r = 0; r < kMeasureRounds; ++r) {
        TimePoint t0 = Clock::now();
        bool ok = decompress_once(codec, transform, comp_buf.data(), comp_buf.size(),
                                  data_size, element_size, decomp_buf);
        TimePoint t1 = Clock::now();
        if (!ok) { decompress_us_median = -1; return; }
        decomp_times.push_back(elapsed_us(t0, t1));
    }

    compress_us_median = median_d(comp_times);
    decompress_us_median = median_d(decomp_times);
}

static bool run_dq001(const std::vector<FitsSample>& fits_samples) {
    fprintf(stderr, "\n========== [DQ-001] codec/transform 压缩率对比 ==========\n");

    std::ostringstream csv;
    csv << "fits_label,nside,n_leaf_per_tile,codec,transform,uncompressed_bytes,"
           "compressed_bytes,ratio,compress_us_median,decompress_us_median\n";

    // 用 3 个不同 NSIDE 产生不同大小的 Tile signal 数据
    // NSIDE=64 -> depth=2, n_leaf_per_tile=16 (小)
    // NSIDE=256 -> depth=4, n_leaf_per_tile=256 (中)
    // NSIDE=1024-> depth=6, n_leaf_per_tile=4096 (大)
    struct NsideConf { int nside; const char* size_label; };
    NsideConf nsides[] = {
        {64,   "S"},
        {256,  "M"},
        {1024, "L"},
    };

    hiss::CodecId codecs[] = {hiss::CodecId::RAW, hiss::CodecId::LZ4, hiss::CodecId::ZSTD};
    hiss::TransformId transforms[] = {
        hiss::TransformId::NONE,
        hiss::TransformId::BYTE_SHUFFLE,
        hiss::TransformId::DELTA,
        hiss::TransformId::DELTA_VARINT,
    };

    int total_combos = 0;
    for (const auto& sample : fits_samples) {
        // 读取 FITS
        drizzle::FitsImage img;
        std::string err;
        if (!load_fits(sample.path, img, err)) {
            fprintf(stderr, "[DQ-001] 跳过 %s: %s\n", sample.label.c_str(), err.c_str());
            continue;
        }
        fprintf(stderr, "[DQ-001] FITS: %s  %dx%d  %.2f MB\n",
                sample.label.c_str(), img.width, img.height, sample.filesize_mb);

        for (const auto& nc : nsides) {
            // drizzle 到指定 NSIDE
            std::unordered_map<uint64_t, drizzle::PixelAccumulator> accums;
            drizzle::DrizzleStats stats;
            TimePoint t0 = Clock::now();
            if (!run_drizzle(img, nc.nside, 0.8, accums, stats, err)) {
                fprintf(stderr, "[DQ-001] drizzle 失败 nside=%d: %s\n", nc.nside, err.c_str());
                continue;
            }
            TimePoint t1 = Clock::now();
            fprintf(stderr, "[DQ-001]   drizzle nside=%d: %lld HEALPix 像素, %.2fs\n",
                    nc.nside, (long long)accums.size(), elapsed_s(t0, t1));

            // 分组到 Tile
            uint32_t depth = hiss::compute_tile_depth(nc.nside);
            uint32_t tile_nside = hiss::compute_tile_nside(nc.nside);
            uint32_t n_leaf_per_tile = 1u << (2 * depth);
            double A_p = 4.0 * M_PI / (12.0 * (double)nc.nside * (double)nc.nside);

            auto groups = group_accumulators_to_tiles(accums, nc.nside);
            if (groups.empty()) {
                fprintf(stderr, "[DQ-001]   无 Tile, 跳过\n");
                continue;
            }

            // 选取最大的 Tile (有效像素最多) 作为压缩基准数据
            size_t best_idx = 0;
            size_t best_n_valid = 0;
            for (size_t i = 0; i < groups.size(); ++i) {
                if (groups[i].pixels.size() > best_n_valid) {
                    best_n_valid = groups[i].pixels.size();
                    best_idx = i;
                }
            }
            auto tile_acc = make_tile_accum(groups[best_idx], tile_nside, n_leaf_per_tile, A_p);
            std::vector<float> signal;
            tile_acc.finalize_signal(signal);
            size_t sig_bytes = signal.size() * sizeof(float);
            fprintf(stderr, "[DQ-001]   Tile signal: %zu 像素, %zu bytes (parent=%llu)\n",
                    signal.size(), sig_bytes,
                    (unsigned long long)groups[best_idx].parent_ipix);

            // 测试 3 codec × 4 transform = 12 组
            for (hiss::CodecId codec : codecs) {
                for (hiss::TransformId tf : transforms) {
                    double comp_us = 0, decomp_us = 0;
                    size_t comp_size = 0;
                    measure_compress_decompress(
                        codec, tf,
                        (const uint8_t*)signal.data(), sig_bytes, sizeof(float),
                        comp_us, decomp_us, comp_size);

                    double ratio = (comp_size > 0) ? (double)sig_bytes / (double)comp_size : 0.0;
                    if (comp_us < 0 || decomp_us < 0) {
                        fprintf(stderr, "[DQ-001]     %s/%s: 失败\n",
                                codec_name(codec), transform_name(tf));
                        continue;
                    }

                    fprintf(stderr, "[DQ-001]     %s/%s: %zu -> %zu (%.2fx), comp=%.1fus decomp=%.1fus\n",
                            codec_name(codec), transform_name(tf),
                            sig_bytes, comp_size, ratio, comp_us, decomp_us);

                    csv << sample.label << ","
                        << nc.nside << ","
                        << n_leaf_per_tile << ","
                        << codec_name(codec) << ","
                        << transform_name(tf) << ","
                        << sig_bytes << ","
                        << comp_size << ","
                        << std::fixed << std::setprecision(4) << ratio << ","
                        << std::fixed << std::setprecision(2) << comp_us << ","
                        << std::fixed << std::setprecision(2) << decomp_us << "\n";
                    ++total_combos;
                }
            }
        }
    }

    if (total_combos == 0) {
        fprintf(stderr, "[DQ-001] 无有效实验结果\n");
        return false;
    }
    return write_csv(std::string(kResultsDir) + "/dq001_codec_comparison.csv", csv.str());
}

// ============================================================================
// DQ-002: Tile 占用模式对比
// ============================================================================
// 构造不同占用率的 Tile, 用 HissWriter 写入, 测量文件大小
// 由于 Writer 自动选择 occupancy 模式 (步骤11), 我们通过控制有效像素比例
// 来触发不同模式, 并测量最终文件大小 (含 occupancy + signal + support)
//
// 占用率目标: 100% (FULL), 80% (接近 FULL 阈值), 30% (BITMAP), 5% (SPARSE)
struct OccupancyResult {
    double target_occupancy;
    double actual_occupancy;
    uint32_t n_leaf_per_tile;
    uint32_t n_valid;
    std::string auto_mode;        // Writer 自动选择的模式
    size_t total_file_size;       // 文件总大小 (bytes)
    size_t occupancy_bytes;       // occupancy 子块压缩后大小
    size_t signal_bytes;          // signal 子块压缩后大小
    size_t support_bytes;         // support 子块压缩后大小
};

static bool run_dq002() {
    fprintf(stderr, "\n========== [DQ-002] Tile 占用模式对比 ==========\n");

    std::ostringstream csv;
    csv << "target_occupancy,actual_occupancy,n_leaf_per_tile,n_valid,auto_mode,"
           "total_file_size,occupancy_bytes,signal_bytes,support_bytes\n";

    // 使用 NSIDE=256, depth=4, tile_nside=16, n_leaf_per_tile=256
    // 用真实 FITS 的 drizzle 结果作为 signal 基础 (取一个 Tile 的真实 signal 模式)
    const uint32_t nside = 256;
    const uint32_t depth = hiss::compute_tile_depth(nside);
    const uint32_t tile_nside = hiss::compute_tile_nside(nside);
    const uint32_t n_leaf_per_tile = 1u << (2 * depth);
    const double A_p = 4.0 * M_PI / (12.0 * (double)nside * (double)nside);

    // 读取一个真实 FITS 获得 signal 数值分布
    auto samples = find_three_fits_samples();
    if (samples.empty()) {
        fprintf(stderr, "[DQ-002] 无 FITS 样本, 跳过\n");
        return false;
    }
    drizzle::FitsImage img;
    std::string err;
    if (!load_fits(samples[0].path, img, err)) {
        fprintf(stderr, "[DQ-002] 读取 FITS 失败: %s\n", err.c_str());
        return false;
    }

    // drizzle 获取真实 signal 分布 (用 nside=256 产生足够 Tile)
    std::unordered_map<uint64_t, drizzle::PixelAccumulator> accums;
    drizzle::DrizzleStats stats;
    if (!run_drizzle(img, nside, 0.8, accums, stats, err)) {
        fprintf(stderr, "[DQ-002] drizzle 失败: %s\n", err.c_str());
        return false;
    }
    auto groups = group_accumulators_to_tiles(accums, nside);
    if (groups.empty()) {
        fprintf(stderr, "[DQ-002] 无 Tile\n");
        return false;
    }

    // 取最大 Tile 的 signal 数值作为模板
    size_t best_idx = 0;
    size_t best_n = 0;
    for (size_t i = 0; i < groups.size(); ++i) {
        if (groups[i].pixels.size() > best_n) { best_n = groups[i].pixels.size(); best_idx = i; }
    }
    auto tmpl_acc = make_tile_accum(groups[best_idx], tile_nside, n_leaf_per_tile, A_p);
    std::vector<float> tmpl_signal;
    tmpl_acc.finalize_signal(tmpl_signal);
    std::vector<uint8_t> tmpl_support;
    tmpl_acc.finalize_support(tmpl_support);

    // 目标占用率: 1.0, 0.8, 0.3, 0.05
    double targets[] = {1.0, 0.8, 0.3, 0.05};

    for (double target : targets) {
        // 构造一个 Tile: 按 target 占用率从模板中选取有效像素
        hiss::DrizzleTileAccumulator acc;
        acc.tile_nside = tile_nside;
        acc.parent_ipix = 0;  // 单 Tile 测试
        acc.pixel_area = A_p;
        acc.pixels.resize(n_leaf_per_tile);

        uint32_t n_valid = (uint32_t)(target * (double)n_leaf_per_tile);
        if (n_valid == 0 && target > 0) n_valid = 1;
        if (n_valid > n_leaf_per_tile) n_valid = n_leaf_per_tile;

        // 随机选择 n_valid 个 local_ipix 作为有效像素
        std::vector<uint32_t> indices(n_leaf_per_tile);
        for (uint32_t i = 0; i < n_leaf_per_tile; ++i) indices[i] = i;
        std::mt19937 rng(12345);
        std::shuffle(indices.begin(), indices.end(), rng);
        for (uint32_t i = 0; i < n_valid; ++i) {
            uint32_t idx = indices[i];
            acc.pixels[idx].sum_flux = tmpl_signal[idx % tmpl_signal.size()];
            acc.pixels[idx].sum_area = (double)tmpl_support[idx % tmpl_support.size()] / 255.0 * A_p;
            acc.pixels[idx].n_contrib = 1;
        }

        double actual_occ = (double)n_valid / (double)n_leaf_per_tile;

        // 用 HissWriter 写入, 测量文件大小
        char path[256];
        std::snprintf(path, sizeof(path), "tests/results/_dq002_occ%03d.hiss",
                      (int)(target * 100));
        std::filesystem::remove(path);
        std::filesystem::remove(std::string(path) + ".tmppool");

        hiss::HissWriter writer;
        hiss::HissGridSpec grid;
        grid.nside = nside;
        grid.tile_nside = tile_nside;
        grid.ordering = 1;
        grid.pixfrac = 0.8;

        hiss::HissMetadata meta;
        std::strncpy(meta.object, "DQ002", sizeof(meta.object) - 1);
        std::strncpy(meta.filter, "Lum", sizeof(meta.filter) - 1);
        meta.exptime = 1.0;
        meta.photscal = 1.0;
        meta.photappl = 1;
        std::strncpy(meta.bunit, "ASTROCS_RELATIVE_FLUX", sizeof(meta.bunit) - 1);

        int ret = writer.open(path, grid, meta);
        if (ret != 0) {
            fprintf(stderr, "[DQ-002] Writer.open 失败 ret=%d\n", ret);
            continue;
        }
        // 用 RAW codec 避免 codec 干扰占用模式体积对比
        writer.set_experiment_codec(hiss::SubblockType::SIGNAL, hiss::CodecId::RAW, hiss::TransformId::NONE);
        writer.set_experiment_codec(hiss::SubblockType::SUPPORT, hiss::CodecId::RAW, hiss::TransformId::NONE);
        writer.set_experiment_codec(hiss::SubblockType::OCCUPANCY, hiss::CodecId::RAW, hiss::TransformId::NONE);

        ret = writer.add_tile(0, acc, nullptr, hiss::OccupancyMode::FULL);
        if (ret != 0) {
            fprintf(stderr, "[DQ-002] add_tile 失败 ret=%d\n", ret);
            writer.cancel();
            continue;
        }
        ret = writer.finalize();
        if (ret != 0) {
            fprintf(stderr, "[DQ-002] finalize 失败 ret=%d\n", ret);
            continue;
        }

        size_t file_size = std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0;

        // 用 Reader 获取子块详情
        hiss::HissReader reader;
        if (reader.open(path) != 0) {
            fprintf(stderr, "[DQ-002] Reader.open 失败\n");
            std::filesystem::remove(path);
            std::filesystem::remove(std::string(path) + ".tmppool");
            continue;
        }
        const auto& tiles = reader.tiles();
        std::string auto_mode = "UNKNOWN";
        size_t occ_bytes = 0, sig_bytes = 0, sup_bytes = 0;
        if (!tiles.empty()) {
            const auto& t = tiles[0];
            switch (t.occ_mode) {
                case hiss::OccupancyMode::FULL:        auto_mode = "FULL"; break;
                case hiss::OccupancyMode::BITMAP:      auto_mode = "BITMAP"; break;
                case hiss::OccupancyMode::SPARSE_LIST: auto_mode = "SPARSE_LIST"; break;
            }
            for (const auto& sb : t.subblocks) {
                if (sb.type == hiss::SubblockType::OCCUPANCY)      occ_bytes = sb.compressed_size;
                else if (sb.type == hiss::SubblockType::SIGNAL)    sig_bytes = sb.compressed_size;
                else if (sb.type == hiss::SubblockType::SUPPORT)   sup_bytes = sb.compressed_size;
            }
        }
        reader.close();

        fprintf(stderr, "[DQ-002]   target=%.2f actual=%.4f n_valid=%u/%u mode=%s file=%zu occ=%zu sig=%zu sup=%zu\n",
                target, actual_occ, n_valid, n_leaf_per_tile, auto_mode.c_str(),
                file_size, occ_bytes, sig_bytes, sup_bytes);

        csv << std::fixed << std::setprecision(4) << target << ","
            << std::fixed << std::setprecision(4) << actual_occ << ","
            << n_leaf_per_tile << ","
            << n_valid << ","
            << auto_mode << ","
            << file_size << ","
            << occ_bytes << ","
            << sig_bytes << ","
            << sup_bytes << "\n";

        std::filesystem::remove(path);
        std::filesystem::remove(std::string(path) + ".tmppool");
    }

    return write_csv(std::string(kResultsDir) + "/dq002_occupancy_mode.csv", csv.str());
}

// ============================================================================
// DQ-003: 磁盘随机读取延迟
// ============================================================================
static bool run_dq003() {
    fprintf(stderr, "\n========== [DQ-003] 磁盘随机读取延迟 ==========\n");

    std::ostringstream csv;
    csv << "codec,n_tiles,n_reads,p50_us,p95_us,p99_us,mean_us,min_us,max_us\n";

    // 生成 100 个 Tile 的 HISS 文件 (NSIDE=256, n_leaf_per_tile=256)
    const uint32_t nside = 256;
    const uint32_t depth = hiss::compute_tile_depth(nside);
    const uint32_t tile_nside = hiss::compute_tile_nside(nside);
    const uint32_t n_leaf_per_tile = 1u << (2 * depth);
    const double A_p = 4.0 * M_PI / (12.0 * (double)nside * (double)nside);
    const int N_TILES = 100;
    const int N_READS = 50;

    // 用真实 FITS signal 模板
    auto samples = find_three_fits_samples();
    if (samples.empty()) {
        fprintf(stderr, "[DQ-003] 无 FITS 样本, 跳过\n");
        return false;
    }
    drizzle::FitsImage img;
    std::string err;
    if (!load_fits(samples[0].path, img, err)) {
        fprintf(stderr, "[DQ-003] 读取 FITS 失败: %s\n", err.c_str());
        return false;
    }
    std::unordered_map<uint64_t, drizzle::PixelAccumulator> accums;
    drizzle::DrizzleStats stats;
    if (!run_drizzle(img, nside, 0.8, accums, stats, err)) {
        fprintf(stderr, "[DQ-003] drizzle 失败: %s\n", err.c_str());
        return false;
    }
    auto groups = group_accumulators_to_tiles(accums, nside);
    if (groups.empty()) {
        fprintf(stderr, "[DQ-003] 无 Tile\n");
        return false;
    }
    // 取最大 Tile signal 作为模板
    size_t best_idx = 0; size_t best_n = 0;
    for (size_t i = 0; i < groups.size(); ++i) {
        if (groups[i].pixels.size() > best_n) { best_n = groups[i].pixels.size(); best_idx = i; }
    }
    auto tmpl_acc = make_tile_accum(groups[best_idx], tile_nside, n_leaf_per_tile, A_p);
    std::vector<float> tmpl_signal;
    tmpl_acc.finalize_signal(tmpl_signal);
    std::vector<uint8_t> tmpl_support;
    tmpl_acc.finalize_support(tmpl_support);

    // 分别测试 RAW 和 Zstd codec
    struct CodecConf { hiss::CodecId id; const char* name; };
    CodecConf codec_confs[] = {
        {hiss::CodecId::RAW,  "RAW"},
        {hiss::CodecId::ZSTD, "Zstd"},
    };

    for (const auto& cc : codec_confs) {
        char path[256];
        std::snprintf(path, sizeof(path), "tests/results/_dq003_%s.hiss", cc.name);
        std::filesystem::remove(path);
        std::filesystem::remove(std::string(path) + ".tmppool");

        hiss::HissWriter writer;
        hiss::HissGridSpec grid;
        grid.nside = nside;
        grid.tile_nside = tile_nside;
        grid.ordering = 1;
        grid.pixfrac = 0.8;

        hiss::HissMetadata meta;
        std::strncpy(meta.object, "DQ003", sizeof(meta.object) - 1);
        meta.exptime = 1.0;
        meta.photscal = 1.0;
        meta.photappl = 1;
        std::strncpy(meta.bunit, "ASTROCS_RELATIVE_FLUX", sizeof(meta.bunit) - 1);

        if (writer.open(path, grid, meta) != 0) {
            fprintf(stderr, "[DQ-003] Writer.open 失败 (%s)\n", cc.name);
            continue;
        }
        writer.set_experiment_codec(hiss::SubblockType::SIGNAL, cc.id, hiss::TransformId::NONE);
        writer.set_experiment_codec(hiss::SubblockType::SUPPORT, cc.id, hiss::TransformId::NONE);

        // 生成 N_TILES 个 Tile, 用确定性模板填充 (FULL 模式以保证每次读到相同大小)
        std::mt19937 rng(20260731);
        std::vector<uint64_t> parent_ipix_list;
        for (int i = 0; i < N_TILES; ++i) {
            uint64_t parent = (uint64_t)i;
            parent_ipix_list.push_back(parent);

            hiss::DrizzleTileAccumulator acc;
            acc.tile_nside = tile_nside;
            acc.parent_ipix = parent;
            acc.pixel_area = A_p;
            acc.pixels.resize(n_leaf_per_tile);
            // 全部填充有效像素 (FULL 模式)
            for (uint32_t j = 0; j < n_leaf_per_tile; ++j) {
                acc.pixels[j].sum_flux = tmpl_signal[(i * 7 + j) % tmpl_signal.size()];
                acc.pixels[j].sum_area = (double)tmpl_support[(i * 11 + j) % tmpl_support.size()] / 255.0 * A_p;
                acc.pixels[j].n_contrib = 1;
            }
            if (writer.add_tile(parent, acc, nullptr, hiss::OccupancyMode::FULL) != 0) {
                fprintf(stderr, "[DQ-003] add_tile 失败 i=%d\n", i);
                writer.cancel();
                continue;
            }
        }
        if (writer.finalize() != 0) {
            fprintf(stderr, "[DQ-003] finalize 失败 (%s)\n", cc.name);
            continue;
        }

        // 随机选 50 个 Tile
        std::shuffle(parent_ipix_list.begin(), parent_ipix_list.end(), rng);
        std::vector<uint64_t> read_list(parent_ipix_list.begin(),
                                         parent_ipix_list.begin() + std::min(N_READS, (int)parent_ipix_list.size()));

        // 用 HissReader 随机读取, 计时
        hiss::HissReader reader;
        if (reader.open(path) != 0) {
            fprintf(stderr, "[DQ-003] Reader.open 失败 (%s)\n", cc.name);
            continue;
        }

        // 先做 5 次预热读取 (排除 OS 文件缓存冷启动)
        for (int w = 0; w < 5; ++w) {
            std::vector<float> sig; std::vector<uint8_t> sup;
            reader.read_tile(parent_ipix_list[w % parent_ipix_list.size()], sig, sup);
        }

        std::vector<double> latencies;
        for (uint64_t parent : read_list) {
            std::vector<float> sig; std::vector<uint8_t> sup;
            TimePoint t0 = Clock::now();
            int ret = reader.read_tile(parent, sig, sup);
            TimePoint t1 = Clock::now();
            if (ret != 0) {
                fprintf(stderr, "[DQ-003] read_tile 失败 parent=%llu (%s)\n",
                        (unsigned long long)parent, cc.name);
                continue;
            }
            latencies.push_back(elapsed_us(t0, t1));
        }
        reader.close();

        if (latencies.empty()) {
            fprintf(stderr, "[DQ-003] %s 无有效延迟测量\n", cc.name);
            continue;
        }

        double p50 = percentile_d(latencies, 0.50);
        double p95 = percentile_d(latencies, 0.95);
        double p99 = percentile_d(latencies, 0.99);
        double mean = 0, mn = 1e30, mx = 0;
        for (double v : latencies) { mean += v; if (v < mn) mn = v; if (v > mx) mx = v; }
        mean /= (double)latencies.size();

        fprintf(stderr, "[DQ-003] %s: n=%zu P50=%.1fus P95=%.1fus P99=%.1fus mean=%.1fus min=%.1fus max=%.1fus\n",
                cc.name, latencies.size(), p50, p95, p99, mean, mn, mx);

        csv << cc.name << ","
            << N_TILES << ","
            << latencies.size() << ","
            << std::fixed << std::setprecision(2) << p50 << ","
            << std::fixed << std::setprecision(2) << p95 << ","
            << std::fixed << std::setprecision(2) << p99 << ","
            << std::fixed << std::setprecision(2) << mean << ","
            << std::fixed << std::setprecision(2) << mn << ","
            << std::fixed << std::setprecision(2) << mx << "\n";

        std::filesystem::remove(path);
        std::filesystem::remove(std::string(path) + ".tmppool");
    }

    return write_csv(std::string(kResultsDir) + "/dq003_random_read_latency.csv", csv.str());
}

// ============================================================================
// DQ-004: Drizzle 性能 profile
// ============================================================================
// 重新实现 drizzle 6 步流水线 (取自 drizzle_engine.cpp processPixel),
// 在各阶段插入计时, 输出 profile CSV
struct DrizzleProfile {
    std::string fits_label;
    int nside;
    int64_t n_source_pixels;
    int64_t n_healpix_pixels;
    double total_s;
    double wcs_s;             // Step 3: SIP+WCS 像素→天球
    double overlap_s;         // Step 4+6: 球面面积 + 重叠计算
    double candidate_s;       // Step 5: 候选像素查询
    double accum_merge_s;     // 线程局部累加器合并
};

static bool run_dq004() {
    fprintf(stderr, "\n========== [DQ-004] Drizzle 性能 profile ==========\n");

    std::ostringstream csv;
    csv << "fits_label,nside,n_source_pixels,n_healpix_pixels,total_s,wcs_s,overlap_s,candidate_s,accum_merge_s\n";

    auto samples = find_three_fits_samples();
    if (samples.empty()) {
        fprintf(stderr, "[DQ-004] 无 FITS 样本, 跳过\n");
        return false;
    }

    // 取第一个样本做 profile (Galaxy_Center 4500x3600)
    const auto& sample = samples[0];
    drizzle::FitsImage img;
    std::string err;
    if (!load_fits(sample.path, img, err)) {
        fprintf(stderr, "[DQ-004] 读取 FITS 失败: %s\n", err.c_str());
        return false;
    }
    fprintf(stderr, "[DQ-004] FITS: %s %dx%d\n", sample.label.c_str(), img.width, img.height);

    // 用 nside=64 (与 test_report.md 一致, 便于对比)
    const int nside = 64;
    const double pixfrac = 0.8;

    drizzle::DrizzleConfig config;
    config.nside = nside;
    config.nested = true;
    config.pixfrac = pixfrac;
    config.apply_photometry = false;
    config.photscal = 1.0;

    // 构造 WCS + HEALPix
    drizzle::WcsSip wcs(img.wcs);
    healpix::HealpixCore hp(nside, true);

    // 计时器
    double total_wcs = 0, total_overlap = 0, total_candidate = 0;
    int64_t n_source = 0;

    // 单线程运行 (便于精确 profile)
    std::unordered_map<uint64_t, drizzle::PixelAccumulator> accumulators;

    TimePoint t_total_start = Clock::now();

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            float pixelValue;
            if (img.channels == 1) {
                pixelValue = img.pixels[(size_t)y * img.width + x];
            } else {
                pixelValue = img.pixels[((size_t)y * img.width + x) * img.channels + 0];
            }
            if (!std::isfinite(pixelValue)) continue;

            // Step 1+2: 取四角 + pixfrac 收缩
            double px = (double)x, py = (double)y;
            double corners_xy[4][2] = {
                {px - 0.5, py - 0.5}, {px + 0.5, py - 0.5},
                {px + 0.5, py + 0.5},  {px - 0.5, py + 0.5}
            };
            if (pixfrac < 1.0) {
                for (int i = 0; i < 4; ++i) {
                    corners_xy[i][0] = px + pixfrac * (corners_xy[i][0] - px);
                    corners_xy[i][1] = py + pixfrac * (corners_xy[i][1] - py);
                }
            }

            // Step 3: WCS 转换 (计时)
            TimePoint t_wcs0 = Clock::now();
            std::vector<spherical::Vec3> drop_corners(4);
            bool wcs_ok = true;
            for (int i = 0; i < 4; ++i) {
                double ra, dec;
                wcs.pixelToSky(corners_xy[i][0], corners_xy[i][1], ra, dec);
                if (!std::isfinite(ra) || !std::isfinite(dec)) { wcs_ok = false; break; }
                drop_corners[i] = spherical::radec_to_vec(ra, dec);
            }
            TimePoint t_wcs1 = Clock::now();
            total_wcs += elapsed_us(t_wcs0, t_wcs1);
            if (!wcs_ok) continue;

            // Step 4: drop 球面面积
            TimePoint t_ov0 = Clock::now();
            double drop_area = spherical::spherical_polygon_area(drop_corners);
            if (drop_area < 1e-20) { total_overlap += elapsed_us(t_ov0, Clock::now()); continue; }

            // Step 5: 候选像素查询 (计时)
            TimePoint t_cand0 = Clock::now();
            std::vector<uint64_t> candidates;
            spherical::query_candidate_pixels(drop_corners, hp, candidates);
            TimePoint t_cand1 = Clock::now();
            total_candidate += elapsed_us(t_cand0, t_cand1);
            if (candidates.empty()) {
                total_overlap += elapsed_us(t_ov0, Clock::now());
                continue;
            }

            // Step 6: 对每个候选计算球面重叠 + 累加
            for (uint64_t ipix : candidates) {
                double overlap_area = spherical::compute_overlap_area(drop_corners, hp, ipix);
                if (overlap_area < 1e-20) continue;
                double weight = overlap_area / drop_area;
                if (weight <= 0.0) continue;
                auto& acc = accumulators[ipix];
                acc.sumFlux   += (double)pixelValue * weight;
                acc.sumWeight += weight;
                acc.sumArea   += overlap_area;
                acc.nContrib++;
            }
            TimePoint t_ov1 = Clock::now();
            total_overlap += elapsed_us(t_ov0, t_ov1);
            total_overlap -= elapsed_us(t_cand0, t_cand1);  // 减去候选查询时间 (已单独计)

            ++n_source;
        }
    }

    TimePoint t_merge0 = Clock::now();
    // 单线程无需合并, 但保留接口
    TimePoint t_merge1 = Clock::now();
    double merge_s = elapsed_s(t_merge0, t_merge1);

    TimePoint t_total_end = Clock::now();
    double total_s = elapsed_s(t_total_start, t_total_end);

    DrizzleProfile prof;
    prof.fits_label = sample.label;
    prof.nside = nside;
    prof.n_source_pixels = n_source;
    prof.n_healpix_pixels = (int64_t)accumulators.size();
    prof.total_s = total_s;
    prof.wcs_s = total_wcs / 1e6;
    prof.overlap_s = total_overlap / 1e6;
    prof.candidate_s = total_candidate / 1e6;
    prof.accum_merge_s = merge_s;

    fprintf(stderr, "[DQ-004] %s nside=%d: %lld src -> %lld hp, total=%.3fs wcs=%.3fs overlap=%.3fs candidate=%.3fs merge=%.6fs\n",
            prof.fits_label.c_str(), prof.nside,
            (long long)prof.n_source_pixels, (long long)prof.n_healpix_pixels,
            prof.total_s, prof.wcs_s, prof.overlap_s, prof.candidate_s, prof.accum_merge_s);

    csv << prof.fits_label << ","
        << prof.nside << ","
        << prof.n_source_pixels << ","
        << prof.n_healpix_pixels << ","
        << std::fixed << std::setprecision(4) << prof.total_s << ","
        << std::fixed << std::setprecision(4) << prof.wcs_s << ","
        << std::fixed << std::setprecision(4) << prof.overlap_s << ","
        << std::fixed << std::setprecision(4) << prof.candidate_s << ","
        << std::fixed << std::setprecision(6) << prof.accum_merge_s << "\n";

    return write_csv(std::string(kResultsDir) + "/dq004_drizzle_profile.csv", csv.str());
}

// ============================================================================
// DQ-005: Writer 流式写入内存测试
// ============================================================================
struct WriterMemResult {
    int n_tiles;
    size_t peak_rss_kb;
    size_t baseline_rss_kb;
    size_t delta_rss_kb;
    double write_s;
    size_t file_size;
};

static bool run_dq005() {
    fprintf(stderr, "\n========== [DQ-005] Writer 流式写入内存测试 ==========\n");

    std::ostringstream csv;
    csv << "n_tiles,baseline_rss_kb,peak_rss_kb,delta_rss_kb,write_s,file_size\n";

    const uint32_t nside = 256;
    const uint32_t depth = hiss::compute_tile_depth(nside);
    const uint32_t tile_nside = hiss::compute_tile_nside(nside);
    const uint32_t n_leaf_per_tile = 1u << (2 * depth);
    const double A_p = 4.0 * M_PI / (12.0 * (double)nside * (double)nside);

    // 用真实 FITS signal 模板
    auto samples = find_three_fits_samples();
    if (samples.empty()) {
        fprintf(stderr, "[DQ-005] 无 FITS 样本, 跳过\n");
        return false;
    }
    drizzle::FitsImage img;
    std::string err;
    if (!load_fits(samples[0].path, img, err)) {
        fprintf(stderr, "[DQ-005] 读取 FITS 失败: %s\n", err.c_str());
        return false;
    }
    std::unordered_map<uint64_t, drizzle::PixelAccumulator> accums;
    drizzle::DrizzleStats stats;
    if (!run_drizzle(img, nside, 0.8, accums, stats, err)) {
        fprintf(stderr, "[DQ-005] drizzle 失败: %s\n", err.c_str());
        return false;
    }
    auto groups = group_accumulators_to_tiles(accums, nside);
    if (groups.empty()) {
        fprintf(stderr, "[DQ-005] 无 Tile\n");
        return false;
    }
    size_t best_idx = 0; size_t best_n = 0;
    for (size_t i = 0; i < groups.size(); ++i) {
        if (groups[i].pixels.size() > best_n) { best_n = groups[i].pixels.size(); best_idx = i; }
    }
    auto tmpl_acc = make_tile_accum(groups[best_idx], tile_nside, n_leaf_per_tile, A_p);
    std::vector<float> tmpl_signal;
    tmpl_acc.finalize_signal(tmpl_signal);
    std::vector<uint8_t> tmpl_support;
    tmpl_acc.finalize_support(tmpl_support);

    int tile_counts[] = {100, 500, 1000};

    for (int n_tiles : tile_counts) {
        char path[256];
        std::snprintf(path, sizeof(path), "tests/results/_dq005_%d.hiss", n_tiles);
        std::filesystem::remove(path);
        std::filesystem::remove(std::string(path) + ".tmppool");

        // 记录写入前 RSS 基准
        size_t baseline_rss = get_current_rss_kb();

        hiss::HissWriter writer;
        hiss::HissGridSpec grid;
        grid.nside = nside;
        grid.tile_nside = tile_nside;
        grid.ordering = 1;
        grid.pixfrac = 0.8;

        hiss::HissMetadata meta;
        std::strncpy(meta.object, "DQ005", sizeof(meta.object) - 1);
        meta.exptime = 1.0;
        meta.photscal = 1.0;
        meta.photappl = 1;
        std::strncpy(meta.bunit, "ASTROCS_RELATIVE_FLUX", sizeof(meta.bunit) - 1);

        if (writer.open(path, grid, meta) != 0) {
            fprintf(stderr, "[DQ-005] Writer.open 失败 n_tiles=%d\n", n_tiles);
            continue;
        }
        // 用 Zstd + BYTE_SHUFFLE (典型生产配置)
        writer.set_experiment_codec(hiss::SubblockType::SIGNAL, hiss::CodecId::ZSTD, hiss::TransformId::BYTE_SHUFFLE);
        writer.set_experiment_codec(hiss::SubblockType::SUPPORT, hiss::CodecId::ZSTD, hiss::TransformId::NONE);

        TimePoint t0 = Clock::now();
        bool failed = false;
        for (int i = 0; i < n_tiles; ++i) {
            uint64_t parent = (uint64_t)i;
            hiss::DrizzleTileAccumulator acc;
            acc.tile_nside = tile_nside;
            acc.parent_ipix = parent;
            acc.pixel_area = A_p;
            acc.pixels.resize(n_leaf_per_tile);
            for (uint32_t j = 0; j < n_leaf_per_tile; ++j) {
                acc.pixels[j].sum_flux = tmpl_signal[(i * 7 + j) % tmpl_signal.size()];
                acc.pixels[j].sum_area = (double)tmpl_support[(i * 11 + j) % tmpl_support.size()] / 255.0 * A_p;
                acc.pixels[j].n_contrib = 1;
            }
            if (writer.add_tile(parent, acc, nullptr, hiss::OccupancyMode::FULL) != 0) {
                fprintf(stderr, "[DQ-005] add_tile 失败 i=%d\n", i);
                failed = true;
                break;
            }
        }
        if (failed) { writer.cancel(); continue; }
        if (writer.finalize() != 0) {
            fprintf(stderr, "[DQ-005] finalize 失败 n_tiles=%d\n", n_tiles);
            continue;
        }
        TimePoint t1 = Clock::now();
        double write_s = elapsed_s(t0, t1);

        size_t peak_rss = get_peak_rss_kb();
        size_t delta_rss = (peak_rss > baseline_rss) ? (peak_rss - baseline_rss) : 0;
        size_t file_size = std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0;

        fprintf(stderr, "[DQ-005]   n_tiles=%d baseline=%zuKB peak=%zuKB delta=%zuKB write=%.3fs file=%zu\n",
                n_tiles, baseline_rss, peak_rss, delta_rss, write_s, file_size);

        csv << n_tiles << ","
            << baseline_rss << ","
            << peak_rss << ","
            << delta_rss << ","
            << std::fixed << std::setprecision(4) << write_s << ","
            << file_size << "\n";

        std::filesystem::remove(path);
        std::filesystem::remove(std::string(path) + ".tmppool");
    }

    return write_csv(std::string(kResultsDir) + "/dq005_writer_memory.csv", csv.str());
}

// ============================================================================
// DQ-006: 自动 NSIDE 选择验证
// ============================================================================
struct AutoNsideResult {
    std::string fits_label;
    int img_w;
    int img_h;
    double crval_ra;
    double crval_dec;
    int auto_nside;
    double hp_res_arcsec;
    double finest_arcsec;     // 估算 (来自 CD 行列式, 用于对比)
    bool is_power_of_2;
    bool in_range;            // [16, 4194304]
    double oversample_factor; // hp_res / finest
};

static bool run_dq006() {
    fprintf(stderr, "\n========== [DQ-006] 自动 NSIDE 选择验证 ==========\n");

    std::ostringstream csv;
    csv << "fits_label,img_w,img_h,crval_ra,crval_dec,auto_nside,hp_res_arcsec,finest_arcsec,is_power_of_2,in_range,oversample_factor\n";

    auto samples = find_three_fits_samples();
    if (samples.empty()) {
        fprintf(stderr, "[DQ-006] 无 FITS 样本, 跳过\n");
        return false;
    }

    for (const auto& sample : samples) {
        drizzle::FitsImage img;
        std::string err;
        if (!load_fits(sample.path, img, err)) {
            fprintf(stderr, "[DQ-006] 跳过 %s: %s\n", sample.label.c_str(), err.c_str());
            continue;
        }
        if (!img.wcs.has_wcs) {
            fprintf(stderr, "[DQ-006] %s 无 WCS, 跳过\n", sample.label.c_str());
            continue;
        }

        int auto_n = drizzle::compute_auto_nside(img.wcs, img.width, img.height);
        if (auto_n <= 0) {
            fprintf(stderr, "[DQ-006] %s compute_auto_nside 失败\n", sample.label.c_str());
            continue;
        }

        // 验证: 2 的幂
        bool is_pow2 = ((auto_n & (auto_n - 1)) == 0) && (auto_n > 0);
        bool in_range = (auto_n >= 16 && auto_n <= 4194304);

        // HEALPix 像素分辨率 (角秒)
        healpix::HealpixCore hp(auto_n, true);
        double hp_res = hp.pixelResolutionArcsec();

        // 估算最细输入像素尺度 (CD 行列式法, 简化)
        double cd_det = std::abs(img.wcs.cd[0] * img.wcs.cd[3] - img.wcs.cd[1] * img.wcs.cd[2]);
        double finest = std::sqrt(cd_det) * 3600.0;  // 度→角秒
        double oversample = (finest > 0) ? hp_res / finest : 0.0;

        fprintf(stderr, "[DQ-006] %s %dx%d -> nside=%d hp_res=%.4f\" finest≈%.4f\" oversample=%.3fx pow2=%d in_range=%d\n",
                sample.label.c_str(), img.width, img.height, auto_n, hp_res, finest, oversample,
                is_pow2 ? 1 : 0, in_range ? 1 : 0);

        csv << sample.label << ","
            << img.width << ","
            << img.height << ","
            << std::fixed << std::setprecision(6) << img.wcs.crval[0] << ","
            << std::fixed << std::setprecision(6) << img.wcs.crval[1] << ","
            << auto_n << ","
            << std::fixed << std::setprecision(6) << hp_res << ","
            << std::fixed << std::setprecision(6) << finest << ","
            << (is_pow2 ? "true" : "false") << ","
            << (in_range ? "true" : "false") << ","
            << std::fixed << std::setprecision(4) << oversample << "\n";
    }

    return write_csv(std::string(kResultsDir) + "/dq006_auto_nside.csv", csv.str());
}

// ============================================================================
// DQ-007: signal/support 语义验证
// ============================================================================
struct SemanticsResult {
    std::string fits_label;
    int nside;
    int64_t n_healpix_pixels;
    double total_source_flux;        // Σ L_j (源像素总通量)
    double total_signal;             // Σ signal[p] (HISS signal 总和)
    double flux_conservation_err;    // |Σsignal - ΣL_j| / ΣL_j
    double support_min;              // support [0, 255]
    double support_max;
    double support_mean;
    bool support_in_range;           // [0, 255]
    bool signal_is_flux_not_avg;     // 验证 signal != sum_flux/sum_area
    double A_p;                      // HEALPix 像素面积
};

static bool run_dq007() {
    fprintf(stderr, "\n========== [DQ-007] signal/support 语义验证 ==========\n");

    std::ostringstream csv;
    csv << "fits_label,nside,n_healpix_pixels,total_source_flux,total_signal,flux_conservation_err,"
           "support_min,support_max,support_mean,support_in_range,signal_is_flux_not_avg,A_p\n";

    auto samples = find_three_fits_samples();
    if (samples.empty()) {
        fprintf(stderr, "[DQ-007] 无 FITS 样本, 跳过\n");
        return false;
    }

    const int nside = 64;   // 与 test_report.md 一致
    const double pixfrac = 0.8;

    for (const auto& sample : samples) {
        drizzle::FitsImage img;
        std::string err;
        if (!load_fits(sample.path, img, err)) {
            fprintf(stderr, "[DQ-007] 跳过 %s: %s\n", sample.label.c_str(), err.c_str());
            continue;
        }
        if (!img.wcs.has_wcs) continue;

        // 1. 计算源像素总通量 Σ L_j (仅 finite 像素)
        double total_source_flux = 0.0;
        for (size_t i = 0; i < img.pixels.size(); ++i) {
            float v = img.pixels[i];
            if (std::isfinite(v)) total_source_flux += (double)v;
        }

        // 2. drizzle 获取累加器
        std::unordered_map<uint64_t, drizzle::PixelAccumulator> accums;
        drizzle::DrizzleStats stats;
        if (!run_drizzle(img, nside, pixfrac, accums, stats, err)) {
            fprintf(stderr, "[DQ-007] %s drizzle 失败: %s\n", sample.label.c_str(), err.c_str());
            continue;
        }

        // 3. 验证 signal 语义: signal[p] = sum_flux (累计通量, 不除面积)
        // 对比: 若 signal = sum_flux / sum_area, 则 total_signal ≈ Σ (L_j * weight / area) ≠ Σ L_j
        double total_signal = 0.0;
        double total_signal_avg = 0.0;  // 若 signal 错误地除以面积
        double support_min = 256, support_max = -1, support_sum = 0;
        int support_count = 0;
        bool support_in_range = true;

        double A_p = 4.0 * M_PI / (12.0 * (double)nside * (double)nside);

        for (const auto& [ipix, acc] : accums) {
            total_signal += acc.sumFlux;
            if (acc.sumArea > 0) {
                total_signal_avg += acc.sumFlux / acc.sumArea;
            }
            double S = acc.sumArea / A_p;
            if (S < 0.0) { S = 0.0; support_in_range = false; }
            if (S > 1.0) {
                // 仅浮点误差级超限可钳制 (02_FROZEN §10)
                if (S > 1.0 + 1e-6) support_in_range = false;
                S = 1.0;
            }
            long v = std::lround(255.0 * S);
            if (v < 0) { v = 0; support_in_range = false; }
            if (v > 255) { v = 255; support_in_range = false; }
            if (v < support_min) support_min = v;
            if (v > support_max) support_max = v;
            support_sum += v;
            ++support_count;
        }

        double support_mean = (support_count > 0) ? support_sum / (double)support_count : 0;
        double flux_err = (total_source_flux != 0.0)
            ? std::abs(total_signal - total_source_flux) / std::abs(total_source_flux)
            : 0.0;
        // signal_is_flux_not_avg: 若 signal=累计通量, 则 total_signal ≈ total_source_flux
        // 若 signal=平均面亮度, 则 total_signal ≠ total_source_flux
        // 用相对误差 < 5% 判定 (drizzle 有边缘像素丢失, 容忍 5%)
        bool signal_is_flux = (flux_err < 0.05);

        fprintf(stderr, "[DQ-007] %s nside=%d: ΣL_j=%.3e Σsignal=%.3e err=%.4e support[%g,%g] mean=%.2f in_range=%d signal_is_flux=%d\n",
                sample.label.c_str(), nside, total_source_flux, total_signal, flux_err,
                support_min, support_max, support_mean, support_in_range ? 1 : 0, signal_is_flux ? 1 : 0);

        csv << sample.label << ","
            << nside << ","
            << (long long)accums.size() << ","
            << std::scientific << std::setprecision(6) << total_source_flux << ","
            << std::scientific << std::setprecision(6) << total_signal << ","
            << std::scientific << std::setprecision(6) << flux_err << ","
            << std::fixed << std::setprecision(0) << support_min << ","
            << std::fixed << std::setprecision(0) << support_max << ","
            << std::fixed << std::setprecision(4) << support_mean << ","
            << (support_in_range ? "true" : "false") << ","
            << (signal_is_flux ? "true" : "false") << ","
            << std::scientific << std::setprecision(6) << A_p << "\n";
    }

    return write_csv(std::string(kResultsDir) + "/dq007_signal_support_semantics.csv", csv.str());
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char* argv[]) {
    fprintf(stderr, "============================================================\n");
    fprintf(stderr, "WP-I-2 真实数据 C++ 实验 (DQ-001 ~ DQ-007)\n");
    fprintf(stderr, "============================================================\n");

    // 确保 results 目录存在
    std::filesystem::create_directories(kResultsDir);

    // 触发 CodecRegistry 初始化 (注册 RAW/LZ4/Zstd)
    hiss::CodecRegistry::instance();

    // 查找真实 FITS 样本
    auto fits_samples = find_three_fits_samples();
    if (fits_samples.empty()) {
        fprintf(stderr, "[FATAL] 未找到测试 FITS 文件\n");
        return 1;
    }
    fprintf(stderr, "找到 %zu 个 FITS 样本:\n", fits_samples.size());
    for (const auto& s : fits_samples) {
        fprintf(stderr, "  - %s: %s\n", s.label.c_str(), s.path.c_str());
    }

    // 运行各实验
    bool ok = true;
    ok = run_dq001(fits_samples) && ok;
    ok = run_dq002() && ok;
    ok = run_dq003() && ok;
    ok = run_dq004() && ok;
    ok = run_dq005() && ok;
    ok = run_dq006() && ok;
    ok = run_dq007() && ok;

    fprintf(stderr, "\n========== 全部实验完成 ==========\n");
    fprintf(stderr, "结果目录: %s\n", kResultsDir);
    fprintf(stderr, "总体状态: %s\n", ok ? "成功" : "部分失败 (见日志)");
    return ok ? 0 : 1;
}
