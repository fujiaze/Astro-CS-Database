// ============================================================================
// hiss_experiment_suite.cpp - AstroCS HISS Phase C++ 实验 (DQ-001 ~ DQ-007)
//
// 依据:
// - 任务: codec/transform/occupancy/checksum/alignment/browser I/O 实验
// - 02_FROZEN §11-§17 (HISS 格式)
// - hiss_format.h (CodecRegistry/ChecksumRegistry/TransformId/SubblockType)
//
// 核心原则:
// 1. 必须使用正式 HissWriter/HissReader、CodecRegistry、ChecksumRegistry、
// transform 注册表 (apply_transform/inverse_transform)
// 2. 不在 benchmark 里另写一套编码器 (不自己实现 byte_shuffle/CRC32C 等)
// 3. C++ 实验输出原始 CSV, Python 只允许做结果汇总
// 4. 不冻结任何参数, 所有推荐标注 "INTERIM_BASELINE_NOT_FROZEN"
// 5. Decision Queue 状态为 WAITING_FOR_USER_DECISION
//
// 实验内容:
// - codec_signal: signal (float32) codec/transform 对比
// - codec_support: support (uint8) codec 对比
// - codec_bitmap: BITMAP codec 对比
// - codec_sparse: SPARSE_LIST codec/transform 对比
// - codec_snr: SNR 控制点 codec 对比
// - occupancy_threshold: BITMAP vs SPARSE_LIST 切换阈值
// - checksum: NONE vs CRC32C 吞吐损失与篡改检测
// - alignment: 1/4/8/4096 字节对齐对读取性能影响
// - browser_io: HissReader 冷/热缓存性能
//
// 编译 (从 lib/astro_image_io/tests/ 目录):
// g++ -std=c++17 -O2 -fopenmp -DHAS_LZ4 -DHAS_ZSTD -DAIO_ENABLE_HEALPIX \
// -I../include -I../src hiss_experiment_suite.cpp \
// ../src/hiss_codec.cpp ../src/hiss_common.cpp ../src/hiss_tile_model.cpp \
// ../src/hiss_transform.cpp ../src/hiss_writer.cpp ../src/hiss_stream_writer.cpp \
// ../src/hiss_reader.cpp -llz4 -lzstd -lm -o hiss_experiment_suite.exe
//
// 运行:
// ./hiss_experiment_suite.exe [output_root_dir]
// 默认输出根目录: ../../../run/experiments
// ============================================================================

#include "hiss_format.h"
#include "hiss_tile_model.h"
#include "hiss_transform.h"

// Windows 头文件中 OPTIONAL/REQUIRED 宏与 hiss::SubblockFlags 冲突, 取消定义
#ifdef OPTIONAL
#undef OPTIONAL
#endif
#ifdef REQUIRED
#undef REQUIRED
#endif

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
#include <map>
#include <numeric>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

#ifdef HAS_ZSTD
#include <zstd.h>
#endif
#ifdef HAS_LZ4
#include <lz4.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 全局常量
// ============================================================================
static const int    kWarmupRounds  = 3;
static const int    kMeasureRounds = 7;
static const double kMB = (1024.0 * 1024.0);
static const double kBytesPerMB = 1.0 / kMB;

static std::string g_output_root;   // 输出根目录 (run/experiments)
static std::string g_raw_dir;       // 原始 CSV 目录 (run/experiments/raw)

// ============================================================================
// 计时工具
// ============================================================================
using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;

static inline double elapsed_us(TimePoint s, TimePoint e) {
    return std::chrono::duration<double, std::micro>(e - s).count();
}
static inline double elapsed_ms(TimePoint s, TimePoint e) {
    return std::chrono::duration<double, std::milli>(e - s).count();
}
static inline double elapsed_s(TimePoint s, TimePoint e) {
    return std::chrono::duration<double>(e - s).count();
}

// ============================================================================
// 内存测量
// ============================================================================
static size_t get_peak_rss_kb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.PeakWorkingSetSize / 1024;
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

// ============================================================================
// 中位数 / 百分位数
// ============================================================================
static double median_d(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n % 2 == 0) return (v[n/2 - 1] + v[n/2]) / 2.0;
    return v[n/2];
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
// CSV / 文本写入工具
// ============================================================================
static bool write_file(const std::string& path, const std::string& content) {
    // 确保父目录存在
    std::filesystem::path p(path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());
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
// Zstd 压缩级别变体注册
// 生产路径 hiss_codec.cpp 中 ZSTD 固定 level=3 (kZstdDefaultLevel)。
// 实验需要测试 level 1/6/9, 通过 CodecRegistry 注册变体:
// CodecId(100) = Zstd level 1
// CodecId(101) = Zstd level 6
// CodecId(102) = Zstd level 9
// compress 函数调用同一 ZSTD_compress 库函数, 仅 level 参数不同,
// 不算 "另写一套编码器"。decompress/bound 复用 ZSTD 标准实现。
// ============================================================================
#ifdef HAS_ZSTD
static int register_zstd_variant(int level, hiss::CodecId id, const std::string& name) {
    hiss::CodecEntry e;
    e.id = id;
    e.name = name;
    // lambda 捕获 level, 调用 ZSTD_compress
    e.compress = [level](const uint8_t* in, size_t in_size,
                          uint8_t* out, size_t* out_size) -> int {
        if (!out || !out_size) return -1;
        if (in_size == 0) { *out_size = 0; return 0; }
        if (!in) return -1;
        size_t r = ZSTD_compress(out, *out_size, in, in_size, level);
        if (ZSTD_isError(r)) return -3;
        *out_size = r;
        return 0;
    };
    e.decompress = [](const uint8_t* in, size_t in_size,
                       uint8_t* out, size_t out_size) -> int {
        if (!out) return -1;
        if (in_size == 0) return 0;
        if (!in) return -1;
        size_t r = ZSTD_decompress(out, out_size, in, in_size);
        if (ZSTD_isError(r)) return -3;
        return 0;
    };
    e.bound = [](size_t n) { return ZSTD_compressBound(n); };
    return hiss::CodecRegistry::instance().register_codec(e);
}

static void register_all_zstd_variants() {
    register_zstd_variant(1, (hiss::CodecId)100, "Zstd1");
    register_zstd_variant(6, (hiss::CodecId)101, "Zstd6");
    register_zstd_variant(9, (hiss::CodecId)102, "Zstd9");
}
#endif

// ============================================================================
// CodecId / TransformId 名称
// ============================================================================
static std::string codec_name(hiss::CodecId id) {
    switch (id) {
        case hiss::CodecId::RAW:  return "RAW";
        case hiss::CodecId::LZ4:  return "LZ4";
        case hiss::CodecId::ZSTD: return "Zstd3";
        default: break;
    }
    // 实验变体
    if ((uint16_t)id == 100) return "Zstd1";
    if ((uint16_t)id == 101) return "Zstd6";
    if ((uint16_t)id == 102) return "Zstd9";
    return "UNKNOWN(" + std::to_string((uint16_t)id) + ")";
}

static std::string transform_name(hiss::TransformId id) {
    switch (id) {
        case hiss::TransformId::NONE:         return "NONE";
        case hiss::TransformId::BYTE_SHUFFLE: return "BYTE_SHUFFLE";
        case hiss::TransformId::DELTA:        return "DELTA";
        case hiss::TransformId::VARINT:       return "DELTA_VARINT";
        case hiss::TransformId::DELTA_VARINT: return "DELTA_VARINT";
    }
    return "UNKNOWN";
}

static bool codec_available(hiss::CodecId id) {
    return hiss::CodecRegistry::instance().find(id) != nullptr;
}

// ============================================================================
// 数据集结构
// ============================================================================
struct Dataset {
    std::string name;
    uint32_t n_leaf = 0;          // 潜在叶像素总数
    double occupancy = 0.0;       // 实际占用率
    std::vector<float>    signal;     // 紧凑: 仅有效像素的 signal (float32)
    std::vector<uint8_t>  support;    // 紧凑: 仅有效像素的 support (uint8)
    std::vector<uint8_t>  bitmap;     // 位图: 1 bit/潜在叶像素
    std::vector<uint32_t> sparse_list; // 升序局部索引列表
};

// 生成数据集: 按占用率随机选择有效像素, signal/support 按指定分布生成
// pattern: "gaussian" / "gradient" / "holes" / "sparse" / "full"
static Dataset generate_dataset(const std::string& name, uint32_t n_leaf,
                                 double occupancy, uint32_t seed,
                                 const std::string& pattern) {
    Dataset ds;
    ds.name = name;
    ds.n_leaf = n_leaf;
    ds.occupancy = occupancy;

    std::mt19937 rng(seed);
    uint32_t n_valid = (uint32_t)(occupancy * (double)n_leaf);
    if (n_valid == 0 && occupancy > 0) n_valid = 1;
    if (n_valid > n_leaf) n_valid = n_leaf;
    ds.occupancy = (double)n_valid / (double)n_leaf;

    // 选择有效像素索引
    std::vector<uint32_t> indices(n_leaf);
    std::iota(indices.begin(), indices.end(), 0u);
    std::shuffle(indices.begin(), indices.end(), rng);
    std::vector<uint32_t> occ_idx(indices.begin(), indices.begin() + n_valid);
    std::sort(occ_idx.begin(), occ_idx.end());
    ds.sparse_list = occ_idx;

    // 位图
    ds.bitmap.assign((n_leaf + 7) / 8, 0);
    for (uint32_t idx : occ_idx)
        ds.bitmap[idx / 8] |= (uint8_t)(1u << (idx % 8));

    // signal / support
    ds.signal.resize(n_valid);
    ds.support.resize(n_valid);
    std::normal_distribution<float> gauss(100.0f, 15.0f);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    for (uint32_t i = 0; i < n_valid; i++) {
        double x = (n_valid > 1) ? (double)i / (double)(n_valid - 1) : 0.0;
        if (pattern == "gaussian") {
            ds.signal[i] = gauss(rng);
        } else if (pattern == "gradient") {
            // 边缘 Tile: 线性梯度 + 正弦波 + 噪声
            ds.signal[i] = 200.0f * (float)x + 50.0f * std::sin(2.0 * M_PI * x * 3.0)
                          + gauss(rng) * 0.1f;
        } else if (pattern == "holes") {
            // pixfrac 孔洞: 双正弦 + 噪声
            ds.signal[i] = 100.0f + 80.0f * std::sin(2.0 * M_PI * x * 5.0)
                          + 30.0f * std::cos(2.0 * M_PI * x * 7.0) + gauss(rng) * 0.2f;
        } else if (pattern == "sparse") {
            ds.signal[i] = 50.0f + gauss(rng) * 0.5f;
        } else {
            // full / default: 天文梯度模拟
            ds.signal[i] = 100.0f * std::sin(2.0 * M_PI * x * 2.0) + 50.0f * (float)x
                          + gauss(rng) * 0.3f;
        }
        // support: 0.4-1.0 映射到 102-255
        double s = 0.4 + 0.6 * uniform(rng);
        ds.support[i] = (uint8_t)std::lround(255.0 * s);
    }

    fprintf(stderr, "[data] %-20s n_leaf=%u occ=%.2f%% n_valid=%u pattern=%s\n",
            name.c_str(), n_leaf, ds.occupancy * 100, n_valid, pattern.c_str());
    return ds;
}

// 生成 SNR 控制点字节数据 (冻结布局: [n_points:u32][points: n*8B])
static std::vector<uint8_t> generate_snr_bytes(uint32_t n_points, uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> snr_dist(20.0f, 5.0f);
    std::vector<uint8_t> data(4 + (size_t)n_points * 8);
    std::memcpy(data.data(), &n_points, 4);
    for (uint32_t i = 0; i < n_points; i++) {
        size_t off = 4 + (size_t)i * 8;
        uint32_t local_ipix = rng() % 262144;  // 模拟局部索引
        float snr = snr_dist(rng);
        std::memcpy(data.data() + off, &local_ipix, 4);
        std::memcpy(data.data() + off + 4, &snr, 4);
    }
    return data;
}

// ============================================================================
// Codec + Transform 组合测量
// 使用正式路径: hiss::apply_transform / inverse_transform + CodecRegistry
// 返回压缩/解压性能指标
// ============================================================================
struct ComboResult {
    size_t orig_size = 0;
    size_t comp_size = 0;
    double ratio = 0;            // orig / comp
    double comp_us = 0;          // 中位数
    double decomp_us = 0;        // 中位数
    double comp_mbs = 0;         // 吞吐
    double decomp_mbs = 0;       // 吞吐
    bool verified = false;
    std::string note;
};

static ComboResult measure_combo(hiss::CodecId codec, hiss::TransformId transform,
                                   const uint8_t* data, size_t data_size,
                                   size_t element_size) {
    ComboResult r;
    r.orig_size = data_size;

    const hiss::CodecEntry* entry = hiss::CodecRegistry::instance().find(codec);
    if (!entry) {
        r.note = "codec 未注册";
        return r;
    }

    // 1. 正向变换 (若 transform != NONE), 使用正式 apply_transform
    std::vector<uint8_t> transformed;
    const uint8_t* data_to_compress = data;
    size_t size_to_compress = data_size;
    if (transform != hiss::TransformId::NONE) {
        hiss::TransformType tt = hiss::transform_id_to_type(transform);
        transformed = hiss::apply_transform(tt, data, data_size, element_size);
        if (transformed.empty() && data_size > 0) {
            r.note = "apply_transform 失败";
            return r;
        }
        data_to_compress = transformed.data();
        size_to_compress = transformed.size();
    }

    // 2. 首次压缩获取 comp_size
    std::vector<uint8_t> comp(entry->bound(size_to_compress));
    size_t comp_size = comp.size();
    int ret = entry->compress(data_to_compress, size_to_compress, comp.data(), &comp_size);
    if (ret != 0) {
        r.note = "compress 失败 ret=" + std::to_string(ret);
        return r;
    }
    comp.resize(comp_size);
    r.comp_size = comp_size;
    r.ratio = (comp_size > 0) ? (double)data_size / (double)comp_size : 0.0;

    // 3. 预热
    for (int i = 0; i < kWarmupRounds; i++) {
        std::vector<uint8_t> tmp(entry->bound(size_to_compress));
        size_t ts = tmp.size();
        entry->compress(data_to_compress, size_to_compress, tmp.data(), &ts);
        std::vector<uint8_t> dec(size_to_compress);
        entry->decompress(tmp.data(), ts, dec.data(), dec.size());
    }

    // 4. 测量压缩耗时 (中位数)
    std::vector<double> comp_times, decomp_times;
    bool ok = true;
    for (int i = 0; i < kMeasureRounds; i++) {
        std::vector<uint8_t> c(entry->bound(size_to_compress));
        size_t cs = c.size();
        TimePoint t0 = Clock::now();
        ret = entry->compress(data_to_compress, size_to_compress, c.data(), &cs);
        TimePoint t1 = Clock::now();
        if (ret != 0) { ok = false; break; }
        comp_times.push_back(elapsed_us(t0, t1));

        // 解压
        std::vector<uint8_t> d(size_to_compress);
        TimePoint t2 = Clock::now();
        ret = entry->decompress(c.data(), cs, d.data(), d.size());
        TimePoint t3 = Clock::now();
        if (ret != 0) { ok = false; break; }
        decomp_times.push_back(elapsed_us(t2, t3));

        // 逆向变换 + 验证
        if (transform != hiss::TransformId::NONE) {
            hiss::TransformType tt = hiss::transform_id_to_type(transform);
            std::vector<uint8_t> restored = hiss::inverse_transform(
                tt, d.data(), d.size(), element_size, 0);
            if (restored.size() != data_size) { ok = false; break; }
            if (std::memcmp(restored.data(), data, data_size) != 0) { ok = false; break; }
        } else {
            if (d.size() != data_size || std::memcmp(d.data(), data, data_size) != 0) {
                ok = false; break;
            }
        }
    }

    r.comp_us = median_d(comp_times);
    r.decomp_us = median_d(decomp_times);
    r.verified = ok;
    double orig_mb = (double)data_size * kBytesPerMB;
    if (r.comp_us > 0)   r.comp_mbs = orig_mb / (r.comp_us / 1e6);
    if (r.decomp_us > 0) r.decomp_mbs = orig_mb / (r.decomp_us / 1e6);
    if (!ok) r.note = "verify failed";
    return r;
}

// ============================================================================
// 实验 1: Codec/transform 对比 (按子块类型)
// ============================================================================

// signal (float32) 候选
using Combo = std::pair<hiss::CodecId, hiss::TransformId>;
static const Combo kSignalCombos[] = {
    {hiss::CodecId::RAW,  hiss::TransformId::NONE},
    {hiss::CodecId::LZ4,  hiss::TransformId::NONE},
    {hiss::CodecId::ZSTD, hiss::TransformId::NONE},        // Zstd3
    {(hiss::CodecId)100,  hiss::TransformId::NONE},        // Zstd1
    {(hiss::CodecId)101,  hiss::TransformId::NONE},        // Zstd6
    {(hiss::CodecId)102,  hiss::TransformId::NONE},        // Zstd9
    {hiss::CodecId::LZ4,  hiss::TransformId::BYTE_SHUFFLE},
    {hiss::CodecId::ZSTD, hiss::TransformId::BYTE_SHUFFLE},
    {hiss::CodecId::LZ4,  hiss::TransformId::DELTA},
    {hiss::CodecId::ZSTD, hiss::TransformId::DELTA},
};

// support (uint8) 候选
static const Combo kSupportCombos[] = {
    {hiss::CodecId::RAW,  hiss::TransformId::NONE},
    {hiss::CodecId::LZ4,  hiss::TransformId::NONE},
    {hiss::CodecId::ZSTD, hiss::TransformId::NONE},        // Zstd3
    {(hiss::CodecId)100,  hiss::TransformId::NONE},        // Zstd1
    {(hiss::CodecId)101,  hiss::TransformId::NONE},        // Zstd6
};

// BITMAP 候选
static const Combo kBitmapCombos[] = {
    {hiss::CodecId::RAW,  hiss::TransformId::NONE},
    {hiss::CodecId::LZ4,  hiss::TransformId::NONE},
    {hiss::CodecId::ZSTD, hiss::TransformId::NONE},        // Zstd3
};

// SPARSE_LIST 候选 (element_size=4, uint32 索引)
static const Combo kSparseCombos[] = {
    {hiss::CodecId::RAW,  hiss::TransformId::NONE},
    {hiss::CodecId::LZ4,  hiss::TransformId::DELTA},
    {hiss::CodecId::ZSTD, hiss::TransformId::DELTA},
    {hiss::CodecId::LZ4,  hiss::TransformId::DELTA_VARINT},
    {hiss::CodecId::ZSTD, hiss::TransformId::DELTA_VARINT},
};

// SNR 候选 (混合布局, element_size=1)
static const Combo kSnrCombos[] = {
    {hiss::CodecId::RAW,  hiss::TransformId::NONE},
    {hiss::CodecId::LZ4,  hiss::TransformId::NONE},
    {hiss::CodecId::ZSTD, hiss::TransformId::NONE},        // Zstd3
};

static bool run_codec_experiment(const std::string& csv_name,
                                   const std::vector<Dataset>& datasets,
                                   const std::string& subblock_label,
                                   size_t element_size,
                                   const std::vector<std::pair<hiss::CodecId, hiss::TransformId>>* combos_ptr) {
    fprintf(stderr, "\n========== [codec] %s ==========\n", subblock_label.c_str());

    std::ostringstream csv;
    csv << "dataset,codec,transform,orig_size,comp_size,ratio,"
           "comp_time_us,decomp_time_us,comp_throughput_mbs,decomp_throughput_mbs\n";

    int n_rows = 0;
    for (const auto& ds : datasets) {
        // 选择数据源
        std::vector<uint8_t> raw_bytes;
        if (subblock_label == "signal") {
            raw_bytes.assign((const uint8_t*)ds.signal.data(),
                             (const uint8_t*)ds.signal.data() + ds.signal.size() * sizeof(float));
        } else if (subblock_label == "support") {
            raw_bytes.assign(ds.support.begin(), ds.support.end());
        } else if (subblock_label == "bitmap") {
            raw_bytes = ds.bitmap;
        } else if (subblock_label == "sparse") {
            raw_bytes.assign((const uint8_t*)ds.sparse_list.data(),
                             (const uint8_t*)ds.sparse_list.data() + ds.sparse_list.size() * sizeof(uint32_t));
        } else {
            continue;
        }
        if (raw_bytes.empty()) continue;

        for (const auto& [codec, transform] : *combos_ptr) {
            if (!codec_available(codec)) continue;

            ComboResult r = measure_combo(codec, transform,
                                           raw_bytes.data(), raw_bytes.size(), element_size);

            fprintf(stderr, "[codec] %-20s %-8s + %-14s orig=%zu comp=%zu ratio=%.2fx "
                    "comp=%.1fus decomp=%.1fus %s\n",
                    ds.name.c_str(), codec_name(codec).c_str(),
                    transform_name(transform).c_str(),
                    r.orig_size, r.comp_size, r.ratio,
                    r.comp_us, r.decomp_us,
                    r.verified ? "OK" : ("FAIL:" + r.note).c_str());

            csv << ds.name << ","
                << codec_name(codec) << ","
                << transform_name(transform) << ","
                << r.orig_size << ","
                << r.comp_size << ","
                << std::fixed << std::setprecision(4) << r.ratio << ","
                << std::fixed << std::setprecision(2) << r.comp_us << ","
                << std::fixed << std::setprecision(2) << r.decomp_us << ","
                << std::fixed << std::setprecision(2) << r.comp_mbs << ","
                << std::fixed << std::setprecision(2) << r.decomp_mbs << "\n";
            ++n_rows;
        }
    }

    if (n_rows == 0) {
        fprintf(stderr, "[codec] %s: 无有效结果\n", subblock_label.c_str());
        return false;
    }
    return write_file(g_raw_dir + "/" + csv_name, csv.str());
}

// SNR codec 实验 (独立, 因为 SNR 数据不从 Dataset 取)
static bool run_codec_snr_experiment() {
    fprintf(stderr, "\n========== [codec] SNR ==========\n");
    std::ostringstream csv;
    csv << "dataset,codec,transform,orig_size,comp_size,ratio,"
           "comp_time_us,decomp_time_us,comp_throughput_mbs,decomp_throughput_mbs\n";

    // 生成 SNR 数据: 不同规模
    struct SnrSet { std::string name; uint32_t n_points; uint32_t seed; };
    SnrSet sets[] = {
        {"snr_small",  100,   42},
        {"snr_medium", 1000,  100},
        {"snr_large",  10000, 200},
    };

    std::vector<std::pair<hiss::CodecId, hiss::TransformId>> combos(
        kSnrCombos, kSnrCombos + sizeof(kSnrCombos)/sizeof(kSnrCombos[0]));

    int n_rows = 0;
    for (const auto& s : sets) {
        std::vector<uint8_t> data = generate_snr_bytes(s.n_points, s.seed);
        for (const auto& [codec, transform] : combos) {
            if (!codec_available(codec)) continue;
            ComboResult r = measure_combo(codec, transform,
                                           data.data(), data.size(), 1);
            fprintf(stderr, "[codec] %-20s %-8s + %-14s orig=%zu comp=%zu ratio=%.2fx %s\n",
                    s.name.c_str(), codec_name(codec).c_str(),
                    transform_name(transform).c_str(),
                    r.orig_size, r.comp_size, r.ratio,
                    r.verified ? "OK" : "FAIL");
            csv << s.name << ","
                << codec_name(codec) << ","
                << transform_name(transform) << ","
                << r.orig_size << ","
                << r.comp_size << ","
                << std::fixed << std::setprecision(4) << r.ratio << ","
                << std::fixed << std::setprecision(2) << r.comp_us << ","
                << std::fixed << std::setprecision(2) << r.decomp_us << ","
                << std::fixed << std::setprecision(2) << r.comp_mbs << ","
                << std::fixed << std::setprecision(2) << r.decomp_mbs << "\n";
            ++n_rows;
        }
    }
    if (n_rows == 0) return false;
    return write_file(g_raw_dir + "/codec_snr.csv", csv.str());
}

// ============================================================================
// 实验 2: Occupancy 阈值 (BITMAP vs SPARSE_LIST)
// 测试不同占用率下两种模式的总编码大小
// 覆盖: 随机分布 / 簇状稀疏 / 细线状覆盖
// ============================================================================
static bool run_occupancy_threshold_experiment() {
    fprintf(stderr, "\n========== [occupancy] BITMAP vs SPARSE_LIST 阈值 ==========\n");

    std::ostringstream csv;
    csv << "occupancy,pattern,bitmap_total_size,sparse_total_size,recommended\n";

    const uint32_t n_leaf = 65536;   // 标准 Tile 大小
    const double A_p = 4.0 * M_PI / (12.0 * 256.0 * 256.0);  // NSIDE=256 像素面积
    double occ_targets[] = {0.01, 0.05, 0.10, 0.15, 0.20, 0.30, 0.50, 0.70, 0.90};
    const char* patterns[] = {"random", "clustered", "streak"};
    const uint32_t seeds[] = {42, 100, 200};

    // 目录开销: 每子块描述符 40 字节 (02_FROZEN §15)
    const size_t kSubblockDescSize = 40;
    const size_t kTileHeaderOverhead = 15;  // Tile 头

    for (int pi = 0; pi < 3; pi++) {
        const char* pattern = patterns[pi];
        uint32_t seed = seeds[pi];

        for (double occ : occ_targets) {
            uint32_t n_valid = (uint32_t)(occ * (double)n_leaf);
            if (n_valid == 0 && occ > 0) n_valid = 1;

            // 生成有效像素索引 (按 pattern)
            std::vector<uint32_t> occ_idx;
            std::mt19937 rng(seed ^ (uint32_t)(occ * 100000));
            if (std::string(pattern) == "random") {
                std::vector<uint32_t> all(n_leaf);
                std::iota(all.begin(), all.end(), 0u);
                std::shuffle(all.begin(), all.end(), rng);
                occ_idx.assign(all.begin(), all.begin() + n_valid);
                std::sort(occ_idx.begin(), occ_idx.end());
            } else if (std::string(pattern) == "clustered") {
                // 簇状: 选择几个簇中心, 每簇周围聚集
                uint32_t n_clusters = std::max(1u, n_valid / 100);
                for (uint32_t c = 0; c < n_clusters && occ_idx.size() < n_valid; c++) {
                    uint32_t center = rng() % n_leaf;
                    for (int d = -50; d <= 50 && occ_idx.size() < n_valid; d++) {
                        uint32_t idx = (center + d) % n_leaf;
                        occ_idx.push_back(idx);
                    }
                }
                std::sort(occ_idx.begin(), occ_idx.end());
                occ_idx.erase(std::unique(occ_idx.begin(), occ_idx.end()), occ_idx.end());
                if (occ_idx.size() > n_valid) occ_idx.resize(n_valid);
            } else {
                // streak: 细线状, 沿行方向覆盖
                uint32_t row_width = std::max(1u, (uint32_t)(std::sqrt((double)n_leaf)));
                uint32_t n_rows = std::max(1u, n_valid / row_width);
                for (uint32_t r = 0; r < n_rows && occ_idx.size() < n_valid; r++) {
                    uint32_t row = rng() % row_width;
                    for (uint32_t c = 0; c < row_width && occ_idx.size() < n_valid; c++) {
                        occ_idx.push_back(row * row_width + c);
                    }
                }
                std::sort(occ_idx.begin(), occ_idx.end());
                occ_idx.erase(std::unique(occ_idx.begin(), occ_idx.end()), occ_idx.end());
            }
            n_valid = (uint32_t)occ_idx.size();

            // 紧凑 signal/support 大小 (RAW, 仅有效像素)
            size_t compact_signal = n_valid * sizeof(float);
            size_t compact_support = n_valid * sizeof(uint8_t);

            // BITMAP 总大小 = bitmap_bytes + compact_signal + compact_support + 目录
            size_t bitmap_bytes = (n_leaf + 7) / 8;
            size_t bitmap_total = bitmap_bytes + compact_signal + compact_support
                                 + kSubblockDescSize * 3 + kTileHeaderOverhead;

            // SPARSE_LIST 总大小 = sparse_list_bytes + compact_signal + compact_support + 目录
            size_t sparse_list_bytes = n_valid * sizeof(uint32_t);
            size_t sparse_total = sparse_list_bytes + compact_signal + compact_support
                                 + kSubblockDescSize * 3 + kTileHeaderOverhead;

            std::string recommended = (bitmap_total <= sparse_total) ? "BITMAP" : "SPARSE_LIST";

            fprintf(stderr, "[occ] pattern=%-10s occ=%.2f%% n_valid=%u "
                    "bitmap=%zu sparse=%zu -> %s\n",
                    pattern, occ * 100, n_valid, bitmap_total, sparse_total,
                    recommended.c_str());

            csv << std::fixed << std::setprecision(4) << occ << ","
                << pattern << ","
                << bitmap_total << ","
                << sparse_total << ","
                << recommended << "\n";
        }
    }
    return write_file(g_raw_dir + "/occupancy_threshold.csv", csv.str());
}

// ============================================================================
// 实验 4: Checksum 性能 (DQ-006)
// - NONE vs CRC32C 吞吐对比 (使用正式 ChecksumRegistry)
// - Writer+Reader 端到端开销 (set_experiment_checksum)
// - 篡改检测验证 (CRC32C 必须检测到压缩数据篡改)
//
// 原则: 复用 ChecksumRegistry::find(CRC32C), 不另写 CRC32C 实现
// ============================================================================
static bool run_checksum_experiment() {
    fprintf(stderr, "\n========== [checksum] NONE vs CRC32C ==========\n");

    std::ostringstream csv;
    csv << "data_size_mb,checksum_type,checksum_us,checksum_throughput_mbs,"
           "end_to_end_write_us,end_to_end_read_us,overhead_pct\n";

    // 数据规模
    struct Sz { size_t bytes; const char* label; };
    Sz sizes[] = {
        {64 * 1024,        "0.0625"},
        {256 * 1024,       "0.25"},
        {1024 * 1024,      "1.0"},
        {4 * 1024 * 1024,  "4.0"},
    };

    const hiss::ChecksumEntry* crc32c = hiss::ChecksumRegistry::instance().find(hiss::ChecksumType::CRC32C);
    if (!crc32c) {
        fprintf(stderr, "[checksum] CRC32C 未注册, 跳过\n");
        return false;
    }

    // 4.1 纯 checksum 计算吞吐 (使用正式 ChecksumRegistry)
    for (const auto& sz : sizes) {
        std::vector<uint8_t> data(sz.bytes);
        std::mt19937 rng(42);
        for (size_t i = 0; i < sz.bytes; i++) data[i] = (uint8_t)(rng() & 0xFF);

        // 预热
        for (int w = 0; w < kWarmupRounds; w++) crc32c->compute(data.data(), data.size());

        std::vector<double> times;
        for (int i = 0; i < kMeasureRounds; i++) {
            TimePoint t0 = Clock::now();
            volatile uint64_t cs = crc32c->compute(data.data(), data.size());
            (void)cs;
            TimePoint t1 = Clock::now();
            times.push_back(elapsed_us(t0, t1));
        }
        double cs_us = median_d(times);
        double cs_mbs = (cs_us > 0) ? ((double)sz.bytes * kBytesPerMB) / (cs_us / 1e6) : 0.0;
        fprintf(stderr, "[checksum] CRC32C %zuB: %.2f us %.0f MB/s\n",
                sz.bytes, cs_us, cs_mbs);
        csv << sz.label << ",CRC32C,"
            << std::fixed << std::setprecision(2) << cs_us << ","
            << std::fixed << std::setprecision(2) << cs_mbs << ","
            << "0,0,0\n";
    }

    // 4.2 端到端 Writer/Reader 开销 (NONE vs CRC32C)
    // 使用正式 HissWriter/HissReader, 对 SIGNAL/SUPPORT 启用 CRC32C
    const uint32_t nside = 256;
    const uint32_t depth = hiss::compute_tile_depth(nside);
    const uint32_t tile_nside = hiss::compute_tile_nside(nside);
    const uint32_t n_leaf = 1u << (2 * depth);
    const double A_p = 4.0 * M_PI / (12.0 * (double)nside * (double)nside);

    hiss::HissGridSpec grid;
    grid.nside = nside;
    grid.tile_nside = tile_nside;
    grid.ordering = 1;
    grid.radesys = 0;
    grid.pixfrac = 1.0;

    hiss::HissMetadata meta;
    meta.nside = nside;
    meta.tile_nside = tile_nside;
    meta.photappl = 1;
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");
    std::snprintf(meta.object, sizeof(meta.object), "ChecksumBench");

    struct CkConf { hiss::ChecksumType ct; const char* name; };
    CkConf cks[] = {
        {hiss::ChecksumType::NONE,   "NONE"},
        {hiss::ChecksumType::CRC32C, "CRC32C"},
    };

    for (const auto& ck : cks) {
        std::string path = g_raw_dir + "/_checksum_" + std::string(ck.name) + ".hiss";
        std::filesystem::remove(path);
        std::filesystem::remove(path + ".partial");

        // 构造累加器 (FULL 模式, 全部有效)
        hiss::DrizzleTileAccumulator acc;
        acc.tile_nside = tile_nside;
        acc.parent_ipix = 1;
        acc.pixel_area = A_p;
        acc.pixels.resize(n_leaf);
        std::mt19937 rng(20260731);
        for (uint32_t i = 0; i < n_leaf; i++) {
            acc.pixels[i].sum_flux = 100.0 + (double)(rng() % 1000) * 0.1;
            acc.pixels[i].sum_area = A_p;
            acc.pixels[i].n_contrib = 1;
        }

        std::vector<double> write_times, read_times;
        bool e2e_ok = true;
        for (int round = 0; round < kMeasureRounds; round++) {
            // 写入
            hiss::HissWriter w;
            if (w.open(path, grid, meta) != 0) { e2e_ok = false; break; }
            w.set_experiment_checksum(hiss::SubblockType::SIGNAL, ck.ct);
            w.set_experiment_checksum(hiss::SubblockType::SUPPORT, ck.ct);
            TimePoint t0 = Clock::now();
            if (w.add_tile(1, acc, nullptr, hiss::OccupancyMode::FULL) != 0) { e2e_ok = false; w.cancel(); break; }
            if (w.finalize() != 0) { e2e_ok = false; break; }
            TimePoint t1 = Clock::now();
            write_times.push_back(elapsed_us(t0, t1));

            // 读取
            hiss::HissReader r;
            if (r.open(path) != 0) { e2e_ok = false; break; }
            std::vector<float> sig; std::vector<uint8_t> sup;
            TimePoint t2 = Clock::now();
            int rret = r.read_tile(1, sig, sup);
            TimePoint t3 = Clock::now();
            r.close();
            if (rret != 0) { e2e_ok = false; break; }
            read_times.push_back(elapsed_us(t2, t3));

            std::filesystem::remove(path);
        }

        if (!e2e_ok || write_times.empty() || read_times.empty()) {
            fprintf(stderr, "[checksum] 端到端 %s 失败\n", ck.name);
            continue;
        }
        double w_us = median_d(write_times);
        double r_us = median_d(read_times);
        fprintf(stderr, "[checksum] 端到端 %s: write=%.1f us read=%.1f us\n",
                ck.name, w_us, r_us);
        csv << "1.0," << ck.name << ",0,0,"
            << std::fixed << std::setprecision(2) << w_us << ","
            << std::fixed << std::setprecision(2) << r_us << ",0\n";
    }

    // 4.3 篡改检测验证 (CRC32C 必须检测到压缩数据篡改)
    {
        fprintf(stderr, "[checksum] 篡改检测验证...\n");
        std::string path = g_raw_dir + "/_checksum_tamper.hiss";
        std::filesystem::remove(path);
        std::filesystem::remove(path + ".partial");

        hiss::DrizzleTileAccumulator acc;
        acc.tile_nside = tile_nside;
        acc.parent_ipix = 42;
        acc.pixel_area = A_p;
        acc.pixels.resize(n_leaf);
        for (uint32_t i = 0; i < n_leaf; i++) {
            acc.pixels[i].sum_flux = (double)i * 1.5;
            acc.pixels[i].sum_area = A_p;
            acc.pixels[i].n_contrib = 1;
        }

        hiss::HissWriter w;
        if (w.open(path, grid, meta) != 0) {
            fprintf(stderr, "[checksum] 篡改: Writer.open 失败\n");
        } else {
            w.set_experiment_checksum(hiss::SubblockType::SIGNAL, hiss::ChecksumType::CRC32C);
            if (w.add_tile(42, acc, nullptr, hiss::OccupancyMode::FULL) == 0 && w.finalize() == 0) {
                // 读取子块 offset, 篡改压缩数据中的 1 字节
                hiss::HissReader r;
                if (r.open(path) == 0 && !r.tiles().empty()) {
                    const auto& tile = r.tiles()[0];
                    uint64_t sig_offset = 0; size_t sig_size = 0; bool found = false;
                    for (const auto& sb : tile.subblocks) {
                        if (sb.type == hiss::SubblockType::SIGNAL) {
                            sig_offset = sb.offset; sig_size = sb.compressed_size; found = true;
                        }
                    }
                    r.close();
                    if (found && sig_size > 0) {
                        // 篡改: 翻转 SIGNAL 子块中间一字节
                        FILE* fp = std::fopen(path.c_str(), "r+b");
                        if (fp) {
                            long pos = (long)sig_offset + (long)(sig_size / 2);
                            std::fseek(fp, pos, SEEK_SET);
                            int ch = std::fgetc(fp);
                            std::fseek(fp, pos, SEEK_SET);
                            std::fputc(ch ^ 0xFF, fp);
                            std::fclose(fp);

                            // 读取应失败 (checksum 错误)
                            hiss::HissReader r2;
                            int open_ret = r2.open(path);
                            std::vector<float> sig; std::vector<uint8_t> sup;
                            int read_ret = r2.read_tile(42, sig, sup);
                            r2.close();
                            fprintf(stderr, "[checksum] 篡改后 read_tile=%d (期望 <0)\n", read_ret);
                            csv << "0.0625,CRC32C_TAMPER,0,0,0,0,"
                                << (read_ret < 0 ? 100 : 0) << "\n";
                        }
                    }
                }
            }
        }
        std::filesystem::remove(path);
    }

    return write_file(g_raw_dir + "/checksum.csv", csv.str());
}

// ============================================================================
// 实验 5: Attachment 对齐影响 (DQ-007a)
// - Writer 当前不 padding (顺序写入), 测量子块 offset 的自然对齐分布
// - 按 1/4/8/4096 字节对齐分桶, 测量 read_tile 延迟
// - 注: 因 Writer 未暴露对齐配置, 本实验测量 "自然对齐" 下的延迟分布
// ============================================================================

// 辅助: 获取 offset 的最大对齐 (1/4/8/4096)
static const char* alignment_bucket(uint64_t offset) {
    if (offset % 4096 == 0) return "4096";
    if (offset % 8 == 0)    return "8";
    if (offset % 4 == 0)    return "4";
    return "1";
}

static bool run_alignment_experiment() {
    fprintf(stderr, "\n========== [alignment] attachment 对齐影响 ==========\n");

    std::ostringstream csv;
    csv << "alignment_bucket,n_subblocks,p50_read_us,p95_read_us,mean_read_us\n";

    // 写入多个不同规模的 Tile (触发不同 offset)
    const uint32_t nside = 1024;
    const uint32_t depth = hiss::compute_tile_depth(nside);
    const uint32_t tile_nside = hiss::compute_tile_nside(nside);
    const uint32_t n_leaf = 1u << (2 * depth);
    const double A_p = 4.0 * M_PI / (12.0 * (double)nside * (double)nside);
    const int N_TILES = 60;

    hiss::HissGridSpec grid;
    grid.nside = nside;
    grid.tile_nside = tile_nside;
    grid.ordering = 1;
    grid.radesys = 0;
    grid.pixfrac = 1.0;

    hiss::HissMetadata meta;
    meta.nside = nside;
    meta.tile_nside = tile_nside;
    meta.photappl = 1;
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");
    std::snprintf(meta.object, sizeof(meta.object), "AlignmentBench");

    std::string path = g_raw_dir + "/_alignment.hiss";
    std::filesystem::remove(path);
    std::filesystem::remove(path + ".partial");

    std::vector<uint64_t> parents;
    {
        hiss::HissWriter w;
        if (w.open(path, grid, meta) != 0) {
            fprintf(stderr, "[alignment] Writer.open 失败\n");
            return false;
        }
        // 使用 Zstd (压缩后子块大小可变, offset 分布更广)
        w.set_experiment_codec(hiss::SubblockType::SIGNAL, hiss::CodecId::ZSTD, hiss::TransformId::NONE);
        w.set_experiment_codec(hiss::SubblockType::SUPPORT, hiss::CodecId::ZSTD, hiss::TransformId::NONE);

        std::mt19937 rng(20260731);
        for (int i = 0; i < N_TILES; i++) {
            uint64_t parent = (uint64_t)(i + 1);
            parents.push_back(parent);
            hiss::DrizzleTileAccumulator acc;
            acc.tile_nside = tile_nside;
            acc.parent_ipix = parent;
            acc.pixel_area = A_p;
            acc.pixels.resize(n_leaf);
            // 生成可变内容 (不同压缩率 → 不同 offset)
            for (uint32_t j = 0; j < n_leaf; j++) {
                acc.pixels[j].sum_flux = 100.0 + (double)(rng() % 5000) * 0.01;
                acc.pixels[j].sum_area = A_p;
                acc.pixels[j].n_contrib = 1;
            }
            if (w.add_tile(parent, acc, nullptr, hiss::OccupancyMode::FULL) != 0) {
                fprintf(stderr, "[alignment] add_tile 失败 i=%d\n", i);
                w.cancel();
                return false;
            }
        }
        if (w.finalize() != 0) {
            fprintf(stderr, "[alignment] finalize 失败\n");
            return false;
        }
    }

    // 读取子块 offset, 按对齐分桶
    hiss::HissReader reader;
    if (reader.open(path) != 0) {
        fprintf(stderr, "[alignment] Reader.open 失败\n");
        return false;
    }

    // 收集每个 Tile 的 SIGNAL 子块对齐 + 延迟
    std::map<std::string, std::vector<double>> bucket_latencies;
    std::map<std::string, int> bucket_counts;

    // 预热
    for (size_t i = 0; i < 5 && i < parents.size(); i++) {
        std::vector<float> sig; std::vector<uint8_t> sup;
        reader.read_tile(parents[i], sig, sup);
    }

    for (uint64_t parent : parents) {
        const hiss::HissTile* tile = nullptr;
        for (const auto& t : reader.tiles()) {
            if (t.parent_ipix == parent) { tile = &t; break; }
        }
        if (!tile) continue;

        // 记录 SIGNAL 子块对齐
        for (const auto& sb : tile->subblocks) {
            if (sb.type == hiss::SubblockType::SIGNAL) {
                std::string bucket = alignment_bucket(sb.offset);
                bucket_counts[bucket]++;
            }
        }

        // 测量 read_tile 延迟
        std::vector<float> sig; std::vector<uint8_t> sup;
        TimePoint t0 = Clock::now();
        int ret = reader.read_tile(parent, sig, sup);
        TimePoint t1 = Clock::now();
        if (ret != 0) continue;
        double us = elapsed_us(t0, t1);

        // 取该 Tile 的 SIGNAL 对齐作为分桶
        for (const auto& sb : tile->subblocks) {
            if (sb.type == hiss::SubblockType::SIGNAL) {
                std::string bucket = alignment_bucket(sb.offset);
                bucket_latencies[bucket].push_back(us);
            }
        }
    }
    reader.close();

    // 输出每个桶的统计
    const char* buckets[] = {"1", "4", "8", "4096"};
    for (const char* b : buckets) {
        int cnt = bucket_counts[b];
        const auto& lats = bucket_latencies[b];
        if (lats.empty()) {
            csv << b << "," << cnt << ",0,0,0\n";
            fprintf(stderr, "[alignment] bucket=%s n=%d (无延迟样本)\n", b, cnt);
        } else {
            double p50 = percentile_d(lats, 0.50);
            double p95 = percentile_d(lats, 0.95);
            double mean = 0;
            for (double v : lats) mean += v;
            mean /= (double)lats.size();
            csv << b << "," << cnt << ","
                << std::fixed << std::setprecision(2) << p50 << ","
                << std::fixed << std::setprecision(2) << p95 << ","
                << std::fixed << std::setprecision(2) << mean << "\n";
            fprintf(stderr, "[alignment] bucket=%s n=%d p50=%.1f p95=%.1f mean=%.1f us\n",
                    b, cnt, p50, p95, mean);
        }
    }

    std::filesystem::remove(path);
    return write_file(g_raw_dir + "/alignment.csv", csv.str());
}

// ============================================================================
// 实验 6: Browser I/O (DQ-007b)
// - HissReader 冷缓存 (首次打开) vs 热缓存 (已打开后重复读) 性能
// - 顺序访问 vs 随机访问
// - 使用正式 HissWriter/HissReader
// ============================================================================

// 辅助: 构造并写入一个多 Tile 的 HISS 文件
static bool build_browser_io_file(const std::string& path, int n_tiles,
                                    uint32_t nside, std::vector<uint64_t>& parents_out) {
    const uint32_t depth = hiss::compute_tile_depth(nside);
    const uint32_t tile_nside = hiss::compute_tile_nside(nside);
    const uint32_t n_leaf = 1u << (2 * depth);
    const double A_p = 4.0 * M_PI / (12.0 * (double)nside * (double)nside);

    hiss::HissGridSpec grid;
    grid.nside = nside;
    grid.tile_nside = tile_nside;
    grid.ordering = 1;
    grid.radesys = 0;
    grid.pixfrac = 1.0;

    hiss::HissMetadata meta;
    meta.nside = nside;
    meta.tile_nside = tile_nside;
    meta.photappl = 1;
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");
    std::snprintf(meta.object, sizeof(meta.object), "BrowserIOBench");

    std::filesystem::remove(path);
    std::filesystem::remove(path + ".partial");

    hiss::HissWriter w;
    if (w.open(path, grid, meta) != 0) return false;
    w.set_experiment_codec(hiss::SubblockType::SIGNAL, hiss::CodecId::ZSTD, hiss::TransformId::NONE);
    w.set_experiment_codec(hiss::SubblockType::SUPPORT, hiss::CodecId::ZSTD, hiss::TransformId::NONE);

    std::mt19937 rng(20260731);
    parents_out.clear();
    for (int i = 0; i < n_tiles; i++) {
        uint64_t parent = (uint64_t)(i + 1);
        parents_out.push_back(parent);
        hiss::DrizzleTileAccumulator acc;
        acc.tile_nside = tile_nside;
        acc.parent_ipix = parent;
        acc.pixel_area = A_p;
        acc.pixels.resize(n_leaf);
        for (uint32_t j = 0; j < n_leaf; j++) {
            acc.pixels[j].sum_flux = 100.0 + (double)(rng() % 5000) * 0.01;
            acc.pixels[j].sum_area = A_p;
            acc.pixels[j].n_contrib = 1;
        }
        if (w.add_tile(parent, acc, nullptr, hiss::OccupancyMode::FULL) != 0) {
            w.cancel();
            return false;
        }
    }
    return w.finalize() == 0;
}

static bool run_browser_io_experiment() {
    fprintf(stderr, "\n========== [browser_io] 冷/热缓存 + 顺序/随机 ==========\n");

    std::ostringstream csv;
    csv << "nside,n_tiles,access_mode,cache_mode,total_us,p50_us,p95_us,throughput_tiles_s\n";

    struct NsideConf { uint32_t nside; int n_tiles; };
    NsideConf confs[] = {
        {256,  100},
        {1024, 60},
    };

    for (const auto& conf : confs) {
        std::string path = g_raw_dir + "/_browser_io.hiss";
        std::vector<uint64_t> parents;
        if (!build_browser_io_file(path, conf.n_tiles, conf.nside, parents)) {
            fprintf(stderr, "[browser_io] 构建文件失败 nside=%u\n", conf.nside);
            continue;
        }
        size_t file_size = std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0;
        fprintf(stderr, "[browser_io] nside=%u n_tiles=%d file=%zu bytes\n",
                conf.nside, conf.n_tiles, file_size);

        // 4.1 冷缓存顺序读: 每次新建 Reader (清 OS 缓存影响 - 注: 仅清应用缓存)
        {
            std::vector<double> lats;
            for (int i = 0; i < conf.n_tiles; i++) {
                hiss::HissReader r;
                if (r.open(path) != 0) continue;
                std::vector<float> sig; std::vector<uint8_t> sup;
                TimePoint t0 = Clock::now();
                int ret = r.read_tile(parents[i], sig, sup);
                TimePoint t1 = Clock::now();
                r.close();
                if (ret == 0) lats.push_back(elapsed_us(t0, t1));
            }
            if (!lats.empty()) {
                double total = 0; for (double v : lats) total += v;
                double p50 = percentile_d(lats, 0.50);
                double p95 = percentile_d(lats, 0.95);
                double tput = (total > 0) ? (double)lats.size() / (total / 1e6) : 0;
                fprintf(stderr, "[browser_io] nside=%u cold+seq: p50=%.1f p95=%.1f tput=%.0f t/s\n",
                        conf.nside, p50, p95, tput);
                csv << conf.nside << "," << conf.n_tiles << ",sequential,cold,"
                    << std::fixed << std::setprecision(2) << total << ","
                    << std::fixed << std::setprecision(2) << p50 << ","
                    << std::fixed << std::setprecision(2) << p95 << ","
                    << std::fixed << std::setprecision(2) << tput << "\n";
            }
        }

        // 4.2 热缓存顺序读: 同一 Reader 读全部 Tile
        {
            hiss::HissReader r;
            if (r.open(path) != 0) continue;
            // 预热
            for (int i = 0; i < 3; i++) {
                std::vector<float> sig; std::vector<uint8_t> sup;
                r.read_tile(parents[i % parents.size()], sig, sup);
            }
            std::vector<double> lats;
            for (int i = 0; i < conf.n_tiles; i++) {
                std::vector<float> sig; std::vector<uint8_t> sup;
                TimePoint t0 = Clock::now();
                int ret = r.read_tile(parents[i], sig, sup);
                TimePoint t1 = Clock::now();
                if (ret == 0) lats.push_back(elapsed_us(t0, t1));
            }
            r.close();
            if (!lats.empty()) {
                double total = 0; for (double v : lats) total += v;
                double p50 = percentile_d(lats, 0.50);
                double p95 = percentile_d(lats, 0.95);
                double tput = (total > 0) ? (double)lats.size() / (total / 1e6) : 0;
                fprintf(stderr, "[browser_io] nside=%u hot+seq: p50=%.1f p95=%.1f tput=%.0f t/s\n",
                        conf.nside, p50, p95, tput);
                csv << conf.nside << "," << conf.n_tiles << ",sequential,hot,"
                    << std::fixed << std::setprecision(2) << total << ","
                    << std::fixed << std::setprecision(2) << p50 << ","
                    << std::fixed << std::setprecision(2) << p95 << ","
                    << std::fixed << std::setprecision(2) << tput << "\n";
            }
        }

        // 4.3 热缓存随机读: 同一 Reader, 乱序读
        {
            hiss::HissReader r;
            if (r.open(path) != 0) continue;
            // 预热
            for (int i = 0; i < 5; i++) {
                std::vector<float> sig; std::vector<uint8_t> sup;
                r.read_tile(parents[i % parents.size()], sig, sup);
            }
            std::vector<uint64_t> rand_order = parents;
            std::mt19937 rng(12345);
            std::shuffle(rand_order.begin(), rand_order.end(), rng);

            std::vector<double> lats;
            for (uint64_t parent : rand_order) {
                std::vector<float> sig; std::vector<uint8_t> sup;
                TimePoint t0 = Clock::now();
                int ret = r.read_tile(parent, sig, sup);
                TimePoint t1 = Clock::now();
                if (ret == 0) lats.push_back(elapsed_us(t0, t1));
            }
            r.close();
            if (!lats.empty()) {
                double total = 0; for (double v : lats) total += v;
                double p50 = percentile_d(lats, 0.50);
                double p95 = percentile_d(lats, 0.95);
                double tput = (total > 0) ? (double)lats.size() / (total / 1e6) : 0;
                fprintf(stderr, "[browser_io] nside=%u hot+rand: p50=%.1f p95=%.1f tput=%.0f t/s\n",
                        conf.nside, p50, p95, tput);
                csv << conf.nside << "," << conf.n_tiles << ",random,hot,"
                    << std::fixed << std::setprecision(2) << total << ","
                    << std::fixed << std::setprecision(2) << p50 << ","
                    << std::fixed << std::setprecision(2) << p95 << ","
                    << std::fixed << std::setprecision(2) << tput << "\n";
            }
        }

        std::filesystem::remove(path);
    }

    return write_file(g_raw_dir + "/browser_io.csv", csv.str());
}

// ============================================================================
// 主函数: 注册 codec, 生成数据集, 运行全部实验
// ============================================================================
int main(int argc, char** argv) {
    fprintf(stderr, "============================================================\n");
    fprintf(stderr, "  AstroCS HISS Phase C++ Experiment Suite\n");
    fprintf(stderr, "  DQ-001 ~ DQ-007 (INTERIM_BASELINE_NOT_FROZEN)\n");
    fprintf(stderr, "============================================================\n");

    // 输出根目录 (从 lib/astro_image_io/ 运行时: ../../run/experiments)
    g_output_root = (argc >= 2) ? argv[1] : "../../run/experiments";
    g_raw_dir = g_output_root + "/raw";
    std::filesystem::create_directories(g_raw_dir);
    fprintf(stderr, "[main] 输出根目录: %s\n", g_output_root.c_str());
    fprintf(stderr, "[main] 原始 CSV 目录: %s\n", g_raw_dir.c_str());

    // 注册 Zstd 级别变体 (1/6/9, 生产路径固定 level=3)
    #ifdef HAS_ZSTD
    register_all_zstd_variants();
    fprintf(stderr, "[main] 已注册 Zstd 变体: Zstd1/Zstd6/Zstd9\n");
    #else
    fprintf(stderr, "[main] 警告: 未启用 HAS_ZSTD, 跳过 Zstd 变体\n");
    #endif

    // 列出已注册 codec / checksum
    fprintf(stderr, "[main] 已注册 codec:");
    for (auto id : hiss::CodecRegistry::instance().list())
        fprintf(stderr, " %s", codec_name(id).c_str());
    fprintf(stderr, "\n[main] 已注册 checksum:");
    for (auto id : hiss::ChecksumRegistry::instance().list()) {
        const char* nm = (id == hiss::ChecksumType::CRC32C) ? "CRC32C" : "UNKNOWN";
        fprintf(stderr, " %s", nm);
    }
    fprintf(stderr, "\n");

    int n_ok = 0, n_fail = 0;
    auto run = [&](const char* name, auto fn) {
        fprintf(stderr, "\n>>>>>>>>>> 开始: %s <<<<<<<<<<\n", name);
        bool ok = fn();
        fprintf(stderr, ">>>>>>>>>> 结束: %s -> %s <<<<<<<<<<\n",
                name, ok ? "OK" : "FAIL");
        if (ok) n_ok++; else n_fail++;
    };

    // ----- 1. 数据集生成 -----
    // 不同 NSIDE 对应不同 n_leaf_per_tile, 覆盖小/中/大规模
    // nside=64: depth=2, tile_nside=16, n_leaf=16
    // nside=256: depth=4, tile_nside=16, n_leaf=256
    // nside=1024: depth=6, tile_nside=16, n_leaf=4096
    std::vector<Dataset> sig_datasets;
    std::vector<Dataset> sup_datasets;
    std::vector<Dataset> bmp_datasets;
    std::vector<Dataset> spr_datasets;

    struct DsConf { const char* name; uint32_t n_leaf; double occ; uint32_t seed; const char* pattern; };
    DsConf dscs[] = {
        {"sig_small_gauss",   16,    1.0,  42,  "gaussian"},
        {"sig_med_gradient",  256,   1.0,  100, "gradient"},
        {"sig_med_holes",     256,   0.7,  150, "holes"},
        {"sig_large_full",    4096,  1.0,  200, "full"},
        {"sig_large_holes",   4096,  0.5,  250, "holes"},
    };
    for (const auto& c : dscs) {
        Dataset ds = generate_dataset(c.name, c.n_leaf, c.occ, c.seed, c.pattern);
        sig_datasets.push_back(ds);
        sup_datasets.push_back(ds);   // support 来自同一数据集
        bmp_datasets.push_back(ds);   // bitmap 来自同一数据集
        spr_datasets.push_back(ds);   // sparse_list 来自同一数据集
    }

    // ----- 2. Codec/transform 对比实验 -----
    {
        std::vector<std::pair<hiss::CodecId, hiss::TransformId>> sig_combos(
            kSignalCombos, kSignalCombos + sizeof(kSignalCombos)/sizeof(kSignalCombos[0]));
        run("codec_signal", [&] {
            return run_codec_experiment("codec_signal.csv", sig_datasets, "signal",
                                         sizeof(float), &sig_combos);
        });
    }
    {
        std::vector<std::pair<hiss::CodecId, hiss::TransformId>> sup_combos(
            kSupportCombos, kSupportCombos + sizeof(kSupportCombos)/sizeof(kSupportCombos[0]));
        run("codec_support", [&] {
            return run_codec_experiment("codec_support.csv", sup_datasets, "support",
                                         sizeof(uint8_t), &sup_combos);
        });
    }
    {
        std::vector<std::pair<hiss::CodecId, hiss::TransformId>> bmp_combos(
            kBitmapCombos, kBitmapCombos + sizeof(kBitmapCombos)/sizeof(kBitmapCombos[0]));
        run("codec_bitmap", [&] {
            return run_codec_experiment("codec_bitmap.csv", bmp_datasets, "bitmap",
                                         sizeof(uint8_t), &bmp_combos);
        });
    }
    {
        std::vector<std::pair<hiss::CodecId, hiss::TransformId>> spr_combos(
            kSparseCombos, kSparseCombos + sizeof(kSparseCombos)/sizeof(kSparseCombos[0]));
        run("codec_sparse", [&] {
            return run_codec_experiment("codec_sparse.csv", spr_datasets, "sparse",
                                         sizeof(uint32_t), &spr_combos);
        });
    }
    run("codec_snr", run_codec_snr_experiment);

    // ----- 3. Occupancy 阈值实验 -----
    run("occupancy_threshold", run_occupancy_threshold_experiment);

    // ----- 4. Checksum 实验 -----
    run("checksum", run_checksum_experiment);

    // ----- 5. Alignment 实验 -----
    run("alignment", run_alignment_experiment);

    // ----- 6. Browser I/O 实验 -----
    run("browser_io", run_browser_io_experiment);

    // ----- 汇总 -----
    fprintf(stderr, "\n============================================================\n");
    fprintf(stderr, "  实验汇总: %d OK, %d FAIL\n", n_ok, n_fail);
    fprintf(stderr, "  输出目录: %s\n", g_raw_dir.c_str());
    fprintf(stderr, "  状态: INTERIM_BASELINE_NOT_FROZEN\n");
    fprintf(stderr, "  Decision Queue: WAITING_FOR_USER_DECISION\n");
    fprintf(stderr, "============================================================\n");

    return (n_fail == 0) ? 0 : 1;
}
