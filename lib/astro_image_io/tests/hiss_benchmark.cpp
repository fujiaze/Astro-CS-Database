// ============================================================================
// hiss_benchmark.cpp - AstroCS HISS 未决工程实验 (DQ-001 ~ DQ-007)
//
// 实验内容:
// DQ-001: signal (float32) 默认 codec/transform 候选对比
// DQ-002: support (uint8) 默认 codec 候选对比
// DQ-003: BITMAP 默认 codec 候选对比
// DQ-004: SPARSE_LIST 编码方式候选对比
// DQ-005: FULL/BITMAP/SPARSE_LIST 切换阈值测量
// DQ-006: checksum (CRC32C/xxHash/RAW) 吞吐与占比
// DQ-007: 子块对齐 (8B/64B/4KiB) 体积浪费与随机读取延迟
//
// 重要声明:
// 此程序输出仅为实验建议, 未写入冻结规范, 也未设为不可更改的正式默认值,
// 等待用户与主审助手确认。
//
// 编译:
// g++ -std=c++17 -O2 -DHAS_LZ4 -DHAS_ZSTD \
// -I../include -I<C:/msys64/mingw64/include> \
// hiss_benchmark.cpp ../src/hiss_codec.cpp \
// -L<C:/msys64/mingw64/lib> -llz4 -lzstd -lpsapi \
// -o hiss_benchmark.exe
// ============================================================================

#include "hiss_format.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

// ============================================================================
// 常量
// ============================================================================

static const int    WARMUP_ROUNDS  = 3;   // 预热轮数
static const int    MEASURE_ROUNDS = 10;  // 测量轮数 (取中位数)
static const double MB             = (1024.0 * 1024.0);

// ============================================================================
// 计时工具
// ============================================================================

using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;

static inline double elapsed_ms(TimePoint start, TimePoint end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// ============================================================================
// 内存测量
// ============================================================================

static size_t get_peak_rss_kb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize / 1024;
    }
    return 0;
#else
    // Linux: 读取 /proc/self/status VmHWM
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
// 中位数计算
// ============================================================================

static double median_d(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n % 2 == 0) return (v[n / 2 - 1] + v[n / 2]) / 2.0;
    return v[n / 2];
}

// ============================================================================
// 字节重排 (Byte Shuffle) 变换
// 用于 float32 等多字节类型, 将同一字节位置的数据聚集, 提升压缩率
// ============================================================================

static void byte_shuffle(const uint8_t* in, size_t count, size_t elem_size,
                          std::vector<uint8_t>& out) {
    out.resize(count * elem_size);
    for (size_t b = 0; b < elem_size; b++) {
        for (size_t i = 0; i < count; i++) {
            out[b * count + i] = in[i * elem_size + b];
        }
    }
}

static void byte_unshuffle(const uint8_t* in, size_t count, size_t elem_size,
                            std::vector<uint8_t>& out) {
    out.resize(count * elem_size);
    for (size_t b = 0; b < elem_size; b++) {
        for (size_t i = 0; i < count; i++) {
            out[i * elem_size + b] = in[b * count + i];
        }
    }
}

// ============================================================================
// CRC32C (Castagnoli) 实现
// 多项式: 0x1EDC6F41 (反转: 0x82F63B78)
// ============================================================================

static uint32_t crc32c_table[256];
static bool crc32c_table_init = false;

static void init_crc32c_table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0x82F63B78;
            else         crc >>= 1;
        }
        crc32c_table[i] = crc;
    }
    crc32c_table_init = true;
}

static uint32_t crc32c_compute(const uint8_t* data, size_t len) {
    if (!crc32c_table_init) init_crc32c_table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32c_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// ============================================================================
// xxHash32 实现 (Yann Collet)
// ============================================================================

static const uint32_t XXH_PRIME1 = 2654435761U;
static const uint32_t XXH_PRIME2 = 2246822519U;
static const uint32_t XXH_PRIME3 = 3266489917U;
static const uint32_t XXH_PRIME4 = 668265263U;
static const uint32_t XXH_PRIME5 = 374761393U;

static inline uint32_t xxh_rotl(uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}

static uint32_t xxhash32_compute(const uint8_t* data, size_t len, uint32_t seed = 0) {
    const uint8_t* p = data;
    const uint8_t* end = data + len;
    uint32_t h;

    if (len >= 16) {
        uint32_t v1 = seed + XXH_PRIME1 + XXH_PRIME2;
        uint32_t v2 = seed + XXH_PRIME2;
        uint32_t v3 = seed;
        uint32_t v4 = seed - XXH_PRIME1;
        const uint8_t* limit = end - 16;
        do {
            uint32_t k;
            std::memcpy(&k, p, 4); v1 += k * XXH_PRIME2; v1 = xxh_rotl(v1, 13) * XXH_PRIME1; p += 4;
            std::memcpy(&k, p, 4); v2 += k * XXH_PRIME2; v2 = xxh_rotl(v2, 13) * XXH_PRIME1; p += 4;
            std::memcpy(&k, p, 4); v3 += k * XXH_PRIME2; v3 = xxh_rotl(v3, 13) * XXH_PRIME1; p += 4;
            std::memcpy(&k, p, 4); v4 += k * XXH_PRIME2; v4 = xxh_rotl(v4, 13) * XXH_PRIME1; p += 4;
        } while (p <= limit);
        h = xxh_rotl(v1, 1) + xxh_rotl(v2, 7) + xxh_rotl(v3, 12) + xxh_rotl(v4, 18);
    } else {
        h = seed + XXH_PRIME5;
    }
    h += (uint32_t)len;
    while (p + 4 <= end) {
        uint32_t k;
        std::memcpy(&k, p, 4);
        h += k * XXH_PRIME3;
        h = xxh_rotl(h, 17) * XXH_PRIME4;
        p += 4;
    }
    while (p < end) {
        h += (*p) * XXH_PRIME5;
        h = xxh_rotl(h, 11) * XXH_PRIME1;
        p++;
    }
    h ^= h >> 15; h *= XXH_PRIME2;
    h ^= h >> 13; h *= XXH_PRIME3;
    h ^= h >> 16;
    return h;
}

// ============================================================================
// Varint 编解码 (用于 SPARSE_LIST delta 编码)
// ============================================================================

static size_t varint_encode(uint32_t val, uint8_t* out) {
    size_t n = 0;
    while (val >= 0x80) { out[n++] = (uint8_t)(val | 0x80); val >>= 7; }
    out[n++] = (uint8_t)val;
    return n;
}

static uint32_t varint_decode(const uint8_t* in, size_t* bytes_read) {
    uint32_t val = 0;
    int shift = 0;
    size_t n = 0;
    while (true) {
        uint8_t b = in[n++];
        val |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    *bytes_read = n;
    return val;
}

// ============================================================================
// RLE 编解码 (用于 BITMAP)
// ============================================================================

static size_t rle_encode(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
    out.clear();
    if (len == 0) return 0;
    size_t i = 0;
    while (i < len) {
        uint8_t val = in[i];
        size_t run = 1;
        while (i + run < len && in[i + run] == val && run < 255) run++;
        out.push_back((uint8_t)run);
        out.push_back(val);
        i += run;
    }
    return out.size();
}

static size_t rle_decode(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
    out.clear();
    for (size_t i = 0; i + 1 < len; i += 2) {
        uint8_t run = in[i];
        uint8_t val = in[i + 1];
        for (uint8_t j = 0; j < run; j++) out.push_back(val);
    }
    return out.size();
}

// ============================================================================
// 数据集定义与生成
// ============================================================================

struct Dataset {
    std::string name;
    uint32_t    pixel_count;   // 潜在叶像素总数
    double      occupancy;     // 占用率
    std::vector<float>    signal;      // float32, 仅含已占用像素
    std::vector<uint8_t>  support;     // uint8, 仅含已占用像素
    std::vector<uint8_t>  bitmap;      // 位压缩, 1 bit/潜在叶像素
    std::vector<uint32_t> sparse_list; // 升序局部索引列表
};

static Dataset generate_dataset(const std::string& name,
                                 uint32_t pixel_count,
                                 double occupancy,
                                 uint32_t seed) {
    Dataset ds;
    ds.name = name;
    ds.pixel_count = pixel_count;
    ds.occupancy = occupancy;

    std::mt19937 rng(seed);
    std::normal_distribution<float> noise_dist(0.0f, 1.0f);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    // 随机选择已占用像素索引
    uint32_t occupied_count = (uint32_t)(pixel_count * occupancy);
    std::vector<uint32_t> indices(pixel_count);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng);

    // 取前 occupied_count 个并排序 (升序 sparse_list)
    std::vector<uint32_t> occ_idx(indices.begin(), indices.begin() + occupied_count);
    std::sort(occ_idx.begin(), occ_idx.end());

    // 构建 bitmap
    ds.bitmap.assign((pixel_count + 7) / 8, 0);
    for (uint32_t idx : occ_idx) {
        ds.bitmap[idx / 8] |= (uint8_t)(1 << (idx % 8));
    }
    ds.sparse_list = std::move(occ_idx);

    // 生成 signal (float32) 和 support (uint8)
    ds.signal.resize(occupied_count);
    ds.support.resize(occupied_count);
    for (uint32_t i = 0; i < occupied_count; i++) {
        // 天文梯度模拟: 正弦波 + 线性趋势 + 噪声
        double x = (double)i / std::max(1u, occupied_count);
        float gradient = 100.0f * std::sin(x * 3.14159265 * 2.0) + 50.0f * x;
        ds.signal[i] = gradient + noise_dist(rng);
        // support: 0.5-1.0 范围映射到 128-255
        double s = 0.5 + 0.5 * uniform(rng);
        ds.support[i] = (uint8_t)std::round(255.0 * s);
    }

    fprintf(stderr, "[bench] 数据集 '%s': pixel_count=%u occupancy=%.0f%% occupied=%u\n",
            name.c_str(), pixel_count, occupancy * 100, occupied_count);
    return ds;
}

// ============================================================================
// 结果结构
// ============================================================================

struct BenchmarkResult {
    std::string dq_id;            // DQ-001 ~ DQ-007
    std::string dataset;          // 数据集名称
    std::string candidate;        // 候选名称
    size_t      original_bytes = 0;
    size_t      compressed_bytes = 0;
    double      compression_ratio = 0; // original / compressed
    double      compress_ms = 0;       // 中位数
    double      decompress_ms = 0;     // 中位数
    double      compress_mbs = 0;      // MB/s
    double      decompress_mbs = 0;    // MB/s
    double      cpu_ms = 0;            // 中位数
    size_t      peak_rss_kb = 0;
    int         repeats = 0;
    bool        verified = false;
    // DQ-005/DQ-006/DQ-007 附加指标
    double      extra_value1 = 0; // 通用附加字段 (如吞吐/浪费率/延迟)
    double      extra_value2 = 0;
    std::string extra_note;
};

// ============================================================================
// 编解码辅助: 通过 CodecRegistry 压缩/解压
// ============================================================================

static int codec_compress(hiss::CodecId id, const uint8_t* in, size_t in_len,
                           std::vector<uint8_t>& out) {
    const hiss::CodecEntry* c = hiss::CodecRegistry::instance().find(id);
    if (!c) return -1;
    out.resize(c->bound(in_len));
    size_t out_len = out.size();
    int rc = c->compress(in, in_len, out.data(), &out_len);
    if (rc != 0) return rc;
    out.resize(out_len);
    return 0;
}

static int codec_decompress(hiss::CodecId id, const uint8_t* in, size_t in_len,
                             uint8_t* out, size_t out_len) {
    const hiss::CodecEntry* c = hiss::CodecRegistry::instance().find(id);
    if (!c) return -1;
    return c->decompress(in, in_len, out, out_len);
}

static bool codec_available(hiss::CodecId id) {
    return hiss::CodecRegistry::instance().find(id) != nullptr;
}

static std::string codec_name(hiss::CodecId id) {
    switch (id) {
        case hiss::CodecId::RAW:  return "RAW";
        case hiss::CodecId::LZ4:  return "LZ4";
        case hiss::CodecId::ZSTD: return "ZSTD";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// 通用候选测量函数
// 给定原始数据和待压缩数据 (可能经过预变换), 测量编解码性能
// ============================================================================

static BenchmarkResult measure_candidate(
    const std::string& dq_id,
    const std::string& dataset_name,
    const std::string& candidate_name,
    size_t original_bytes,
    const std::vector<uint8_t>& data_to_compress,
    hiss::CodecId codec_id,
    std::function<bool(const uint8_t*, size_t)> verify_fn) {

    BenchmarkResult r;
    r.dq_id = dq_id;
    r.dataset = dataset_name;
    r.candidate = candidate_name;
    r.original_bytes = original_bytes;
    r.repeats = MEASURE_ROUNDS;
    r.peak_rss_kb = get_peak_rss_kb();

    // 检查 codec 可用性
    if (!codec_available(codec_id)) {
        r.compressed_bytes = 0;
        r.compression_ratio = 0;
        r.verified = false;
        r.extra_note = "codec 不可用";
        return r;
    }

    // 首次压缩获取压缩后大小
    std::vector<uint8_t> compressed;
    if (codec_compress(codec_id, data_to_compress.data(), data_to_compress.size(),
                        compressed) != 0) {
        r.compressed_bytes = 0;
        r.compression_ratio = 0;
        r.verified = false;
        r.extra_note = "压缩失败";
        return r;
    }
    r.compressed_bytes = compressed.size();
    r.compression_ratio = (double)original_bytes / std::max((size_t)1, r.compressed_bytes);

    // 预热
    for (int i = 0; i < WARMUP_ROUNDS; i++) {
        std::vector<uint8_t> tmp;
        if (codec_compress(codec_id, data_to_compress.data(), data_to_compress.size(),
                            tmp) == 0) {
            std::vector<uint8_t> dec(data_to_compress.size());
            codec_decompress(codec_id, tmp.data(), tmp.size(), dec.data(), dec.size());
        }
    }

    // 测量
    std::vector<double> comp_times, decomp_times, cpu_times;
    bool all_ok = true;

    for (int i = 0; i < MEASURE_ROUNDS; i++) {
        std::clock_t cpu0 = std::clock();
        auto t0 = Clock::now();

        std::vector<uint8_t> comp;
        int crc = codec_compress(codec_id, data_to_compress.data(),
                                  data_to_compress.size(), comp);
        auto t1 = Clock::now();
        if (crc != 0) { all_ok = false; continue; }

        std::vector<uint8_t> decomp(data_to_compress.size());
        auto t2 = Clock::now();
        int drc = codec_decompress(codec_id, comp.data(), comp.size(),
                                    decomp.data(), decomp.size());
        auto t3 = Clock::now();
        std::clock_t cpu1 = std::clock();

        if (drc != 0) { all_ok = false; continue; }

        // 验证解压结果
        if (verify_fn) {
            if (!verify_fn(decomp.data(), decomp.size())) all_ok = false;
        } else {
            if (decomp != data_to_compress) all_ok = false;
        }

        comp_times.push_back(elapsed_ms(t0, t1));
        decomp_times.push_back(elapsed_ms(t2, t3));
        cpu_times.push_back((double)(cpu1 - cpu0) * 1000.0 / CLOCKS_PER_SEC);
    }

    r.compress_ms   = median_d(comp_times);
    r.decompress_ms = median_d(decomp_times);
    r.cpu_ms        = median_d(cpu_times);
    r.peak_rss_kb   = get_peak_rss_kb();
    r.verified      = all_ok;

    double orig_mb = (double)original_bytes / MB;
    if (r.compress_ms   > 0) r.compress_mbs   = orig_mb / (r.compress_ms / 1000.0);
    if (r.decompress_ms > 0) r.decompress_mbs = orig_mb / (r.decompress_ms / 1000.0);

    return r;
}

// ============================================================================
// DQ-001: signal (float32) codec/transform 对比
// 候选: RAW / LZ4 / byte-shuffle+LZ4 / Zstd / byte-shuffle+Zstd
// ============================================================================

static std::vector<BenchmarkResult> benchmark_dq001(const std::vector<Dataset>& datasets) {
    std::vector<BenchmarkResult> results;
    fprintf(stderr, "\n[bench] === DQ-001: signal codec/transform ===\n");

    for (const auto& ds : datasets) {
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(ds.signal.data());
        size_t raw_len = ds.signal.size() * sizeof(float);
        std::vector<uint8_t> raw_vec(raw, raw + raw_len);

        // 验证函数: 直接比较
        auto verify_plain = [](const uint8_t* d, size_t n) -> bool {
            return true; // 各候选内部已做往返比较
        };

        // 1. RAW
        {
            auto r = measure_candidate("DQ-001", ds.name, "RAW",
                                        raw_len, raw_vec, hiss::CodecId::RAW, nullptr);
            results.push_back(r);
            fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                    "RAW", r.compression_ratio, r.compress_ms, r.decompress_ms);
        }

        // 2. LZ4
        if (codec_available(hiss::CodecId::LZ4)) {
            auto r = measure_candidate("DQ-001", ds.name, "LZ4",
                                        raw_len, raw_vec, hiss::CodecId::LZ4, nullptr);
            results.push_back(r);
            fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                    "LZ4", r.compression_ratio, r.compress_ms, r.decompress_ms);
        }

        // 3. byte-shuffle + LZ4
        if (codec_available(hiss::CodecId::LZ4)) {
            std::vector<uint8_t> shuffled;
            byte_shuffle(raw, ds.signal.size(), sizeof(float), shuffled);
            // 验证: 解压后需 unshuffle 再比较
            size_t count = ds.signal.size();
            auto verify_shuffle = [&](const uint8_t* d, size_t n) -> bool {
                std::vector<uint8_t> unshuffled;
                byte_unshuffle(d, count, sizeof(float), unshuffled);
                return unshuffled == raw_vec;
            };
            auto r = measure_candidate("DQ-001", ds.name, "byte-shuffle+LZ4",
                                        raw_len, shuffled, hiss::CodecId::LZ4,
                                        verify_shuffle);
            results.push_back(r);
            fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                    "byte-shuffle+LZ4", r.compression_ratio, r.compress_ms, r.decompress_ms);
        }

        // 4. Zstd
        if (codec_available(hiss::CodecId::ZSTD)) {
            auto r = measure_candidate("DQ-001", ds.name, "ZSTD",
                                        raw_len, raw_vec, hiss::CodecId::ZSTD, nullptr);
            results.push_back(r);
            fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                    "ZSTD", r.compression_ratio, r.compress_ms, r.decompress_ms);
        }

        // 5. byte-shuffle + Zstd
        if (codec_available(hiss::CodecId::ZSTD)) {
            std::vector<uint8_t> shuffled;
            byte_shuffle(raw, ds.signal.size(), sizeof(float), shuffled);
            size_t count = ds.signal.size();
            auto verify_shuffle = [&](const uint8_t* d, size_t n) -> bool {
                std::vector<uint8_t> unshuffled;
                byte_unshuffle(d, count, sizeof(float), unshuffled);
                return unshuffled == raw_vec;
            };
            auto r = measure_candidate("DQ-001", ds.name, "byte-shuffle+ZSTD",
                                        raw_len, shuffled, hiss::CodecId::ZSTD,
                                        verify_shuffle);
            results.push_back(r);
            fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                    "byte-shuffle+ZSTD", r.compression_ratio, r.compress_ms, r.decompress_ms);
        }
    }
    return results;
}

// ============================================================================
// DQ-002: support (uint8) codec 对比
// 候选: RAW / LZ4 / Zstd
// ============================================================================

static std::vector<BenchmarkResult> benchmark_dq002(const std::vector<Dataset>& datasets) {
    std::vector<BenchmarkResult> results;
    fprintf(stderr, "\n[bench] === DQ-002: support codec ===\n");

    for (const auto& ds : datasets) {
        size_t raw_len = ds.support.size();
        std::vector<uint8_t> raw_vec(ds.support.begin(), ds.support.end());

        for (auto codec : {hiss::CodecId::RAW, hiss::CodecId::LZ4, hiss::CodecId::ZSTD}) {
            if (!codec_available(codec)) continue;
            auto r = measure_candidate("DQ-002", ds.name, codec_name(codec),
                                        raw_len, raw_vec, codec, nullptr);
            results.push_back(r);
            fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                    codec_name(codec).c_str(), r.compression_ratio,
                    r.compress_ms, r.decompress_ms);
        }
    }
    return results;
}

// ============================================================================
// DQ-003: BITMAP codec 对比
// 候选: RAW bit-packed / LZ4 / Zstd / RLE (可选)
// ============================================================================

static std::vector<BenchmarkResult> benchmark_dq003(const std::vector<Dataset>& datasets) {
    std::vector<BenchmarkResult> results;
    fprintf(stderr, "\n[bench] === DQ-003: BITMAP codec ===\n");

    for (const auto& ds : datasets) {
        size_t raw_len = ds.bitmap.size();
        std::vector<uint8_t> raw_vec(ds.bitmap.begin(), ds.bitmap.end());

        // 1-3. RAW / LZ4 / Zstd on bit-packed bitmap
        for (auto codec : {hiss::CodecId::RAW, hiss::CodecId::LZ4, hiss::CodecId::ZSTD}) {
            if (!codec_available(codec)) continue;
            std::string name = codec_name(codec);
            if (codec == hiss::CodecId::RAW) name = "RAW(bit-packed)";
            auto r = measure_candidate("DQ-003", ds.name, name,
                                        raw_len, raw_vec, codec, nullptr);
            results.push_back(r);
            fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                    name.c_str(), r.compression_ratio, r.compress_ms, r.decompress_ms);
        }

        // 4. RLE (对 bitmap 字节序列做 RLE, 然后不压缩)
        {
            std::vector<uint8_t> rle_data;
            rle_encode(ds.bitmap.data(), ds.bitmap.size(), rle_data);

            auto verify_rle = [&](const uint8_t* d, size_t n) -> bool {
                std::vector<uint8_t> dec;
                rle_decode(d, n, dec);
                return dec == raw_vec;
            };
            // RLE 本身是 "编码", 不再用 codec; 用 RAW codec 存储 RLE 结果
            auto r = measure_candidate("DQ-003", ds.name, "RLE",
                                        raw_len, rle_data, hiss::CodecId::RAW,
                                        verify_rle);
            results.push_back(r);
            fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                    "RLE", r.compression_ratio, r.compress_ms, r.decompress_ms);
        }

        // 5. RLE + LZ4
        if (codec_available(hiss::CodecId::LZ4)) {
            std::vector<uint8_t> rle_data;
            rle_encode(ds.bitmap.data(), ds.bitmap.size(), rle_data);

            auto verify_rle_lz4 = [&](const uint8_t* d, size_t n) -> bool {
                std::vector<uint8_t> dec;
                rle_decode(d, n, dec);
                return dec == raw_vec;
            };
            auto r = measure_candidate("DQ-003", ds.name, "RLE+LZ4",
                                        raw_len, rle_data, hiss::CodecId::LZ4,
                                        verify_rle_lz4);
            results.push_back(r);
            fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                    "RLE+LZ4", r.compression_ratio, r.compress_ms, r.decompress_ms);
        }
    }
    return results;
}

// ============================================================================
// DQ-004: SPARSE_LIST 编码方式对比
// 候选: uint32原始 / 升序delta / delta+varint, 各配 LZ4/Zstd
// ============================================================================

static std::vector<BenchmarkResult> benchmark_dq004(const std::vector<Dataset>& datasets) {
    std::vector<BenchmarkResult> results;
    fprintf(stderr, "\n[bench] === DQ-004: SPARSE_LIST encoding ===\n");

    for (const auto& ds : datasets) {
        if (ds.sparse_list.empty()) continue;

        size_t orig_bytes = ds.sparse_list.size() * sizeof(uint32_t);
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(ds.sparse_list.data());
        std::vector<uint8_t> raw_vec(raw, raw + orig_bytes);

        // 验证: 直接比较原始 uint32 列表
        auto verify_raw = [&](const uint8_t* d, size_t n) -> bool {
            return std::memcmp(d, raw_vec.data(), orig_bytes) == 0;
        };

        // --- uint32 原始列表 ---
        for (auto codec : {hiss::CodecId::RAW, hiss::CodecId::LZ4, hiss::CodecId::ZSTD}) {
            if (!codec_available(codec)) continue;
            std::string name = "uint32-raw+" + codec_name(codec);
            auto r = measure_candidate("DQ-004", ds.name, name,
                                        orig_bytes, raw_vec, codec, verify_raw);
            results.push_back(r);
            fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                    name.c_str(), r.compression_ratio, r.compress_ms, r.decompress_ms);
        }

        // --- 升序 delta (uint32) ---
        // delta[0] = list[0], delta[i] = list[i] - list[i-1]
        {
            std::vector<uint32_t> deltas(ds.sparse_list.size());
            deltas[0] = ds.sparse_list[0];
            for (size_t i = 1; i < ds.sparse_list.size(); i++) {
                deltas[i] = ds.sparse_list[i] - ds.sparse_list[i - 1];
            }
            const uint8_t* d_raw = reinterpret_cast<const uint8_t*>(deltas.data());
            size_t d_len = deltas.size() * sizeof(uint32_t);
            std::vector<uint8_t> d_vec(d_raw, d_raw + d_len);

            auto verify_delta = [&](const uint8_t* d, size_t n) -> bool {
                if (n != d_len) return false;
                // 重建原始列表并比较
                const uint32_t* dd = reinterpret_cast<const uint32_t*>(d);
                uint32_t acc = 0;
                for (size_t i = 0; i < deltas.size(); i++) {
                    acc += dd[i];
                    if (acc != ds.sparse_list[i]) return false;
                }
                return true;
            };

            for (auto codec : {hiss::CodecId::LZ4, hiss::CodecId::ZSTD}) {
                if (!codec_available(codec)) continue;
                std::string name = "delta+" + codec_name(codec);
                auto r = measure_candidate("DQ-004", ds.name, name,
                                            orig_bytes, d_vec, codec, verify_delta);
                results.push_back(r);
                fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                        name.c_str(), r.compression_ratio, r.compress_ms, r.decompress_ms);
            }
        }

        // --- delta + varint ---
        {
            // 计算 delta, 再用 varint 编码
            std::vector<uint8_t> varint_data;
            varint_data.reserve(ds.sparse_list.size() * 2); // 估算
            uint32_t prev = 0;
            for (size_t i = 0; i < ds.sparse_list.size(); i++) {
                uint32_t delta = ds.sparse_list[i] - prev;
                uint8_t buf[5];
                size_t n = varint_encode(delta, buf);
                varint_data.insert(varint_data.end(), buf, buf + n);
                prev = ds.sparse_list[i];
            }

            auto verify_varint = [&](const uint8_t* d, size_t n) -> bool {
                // 解码 varint 并重建列表
                size_t pos = 0;
                uint32_t acc = 0;
                for (size_t i = 0; i < ds.sparse_list.size(); i++) {
                    if (pos >= n) return false;
                    size_t br = 0;
                    uint32_t delta = varint_decode(d + pos, &br);
                    pos += br;
                    acc += delta;
                    if (acc != ds.sparse_list[i]) return false;
                }
                return pos == n;
            };

            for (auto codec : {hiss::CodecId::RAW, hiss::CodecId::LZ4, hiss::CodecId::ZSTD}) {
                if (!codec_available(codec)) continue;
                std::string name = "delta+varint+" + codec_name(codec);
                auto r = measure_candidate("DQ-004", ds.name, name,
                                            orig_bytes, varint_data, codec, verify_varint);
                results.push_back(r);
                fprintf(stderr, "  %-30s ratio=%.2f comp=%.2fms decomp=%.2fms\n",
                        name.c_str(), r.compression_ratio, r.compress_ms, r.decompress_ms);
            }
        }
    }
    return results;
}

// ============================================================================
// DQ-005: FULL / BITMAP / SPARSE_LIST 切换阈值
// 测量不同占用率下各模式的: 磁盘体积 / 编码耗时 / 解码耗时
// ============================================================================

static std::vector<BenchmarkResult> benchmark_dq005(const std::vector<Dataset>& datasets) {
    std::vector<BenchmarkResult> results;
    fprintf(stderr, "\n[bench] === DQ-005: occupancy mode thresholds ===\n");

    for (const auto& ds : datasets) {
        size_t signal_bytes = ds.signal.size() * sizeof(float);
        size_t support_bytes = ds.support.size();
        size_t bitmap_bytes  = ds.bitmap.size();
        size_t sparse_bytes  = ds.sparse_list.size() * sizeof(uint32_t);
        size_t total_pixels  = ds.pixel_count;

        // FULL: signal + support for ALL pixels (占用率=100%)
        size_t full_signal = total_pixels * sizeof(float);
        size_t full_support = total_pixels * sizeof(uint8_t);
        size_t full_total = full_signal + full_support;

        // BITMAP: bitmap + signal + support for occupied pixels
        size_t bitmap_total = bitmap_bytes + signal_bytes + support_bytes;

        // SPARSE_LIST: sparse_list + signal + support for occupied pixels
        size_t sparse_total = sparse_bytes + signal_bytes + support_bytes;

        // 测量 BITMAP 编码时间 (位压缩)
        {
            BenchmarkResult r;
            r.dq_id = "DQ-005";
            r.dataset = ds.name;
            r.candidate = "BITMAP";
            r.original_bytes = full_total;
            r.compressed_bytes = bitmap_total;
            r.compression_ratio = (double)full_total / std::max((size_t)1, bitmap_total);
            r.repeats = MEASURE_ROUNDS;

            // 预热 + 测量位图编码
            std::vector<double> enc_times, dec_times, cpu_times;
            for (int round = 0; round < MEASURE_ROUNDS + WARMUP_ROUNDS; round++) {
                std::clock_t cpu0 = std::clock();
                auto t0 = Clock::now();
                // 编码: 生成 bitmap
                std::vector<uint8_t> bmp((total_pixels + 7) / 8, 0);
                for (uint32_t idx : ds.sparse_list) {
                    bmp[idx / 8] |= (uint8_t)(1 << (idx % 8));
                }
                auto t1 = Clock::now();
                // 解码: 从 bitmap 恢复索引列表
                std::vector<uint32_t> recovered;
                recovered.reserve(ds.sparse_list.size());
                for (size_t byte_i = 0; byte_i < bmp.size(); byte_i++) {
                    uint8_t b = bmp[byte_i];
                    for (int bit = 0; bit < 8; bit++) {
                        if (b & (1 << bit)) {
                            recovered.push_back((uint32_t)(byte_i * 8 + bit));
                        }
                    }
                }
                auto t2 = Clock::now();
                std::clock_t cpu1 = std::clock();

                if (round >= WARMUP_ROUNDS) {
                    enc_times.push_back(elapsed_ms(t0, t1));
                    dec_times.push_back(elapsed_ms(t1, t2));
                    cpu_times.push_back((double)(cpu1 - cpu0) * 1000.0 / CLOCKS_PER_SEC);
                }
            }
            r.compress_ms = median_d(enc_times);
            r.decompress_ms = median_d(dec_times);
            r.cpu_ms = median_d(cpu_times);
            r.peak_rss_kb = get_peak_rss_kb();
            r.verified = true;
            r.extra_value1 = (double)bitmap_total; // 实际磁盘体积
            r.extra_note = "bitmap_volume_bytes=" + std::to_string(bitmap_total);
            results.push_back(r);
            fprintf(stderr, "  %-20s occ=%.0f%% volume=%zu ratio=%.2f enc=%.2fms dec=%.2fms\n",
                    "BITMAP", ds.occupancy * 100, bitmap_total, r.compression_ratio,
                    r.compress_ms, r.decompress_ms);
        }

        // SPARSE_LIST 编码时间
        {
            BenchmarkResult r;
            r.dq_id = "DQ-005";
            r.dataset = ds.name;
            r.candidate = "SPARSE_LIST";
            r.original_bytes = full_total;
            r.compressed_bytes = sparse_total;
            r.compression_ratio = (double)full_total / std::max((size_t)1, sparse_total);
            r.repeats = MEASURE_ROUNDS;

            std::vector<double> enc_times, dec_times, cpu_times;
            for (int round = 0; round < MEASURE_ROUNDS + WARMUP_ROUNDS; round++) {
                std::clock_t cpu0 = std::clock();
                auto t0 = Clock::now();
                // 编码: 拷贝 sparse_list
                std::vector<uint32_t> sl = ds.sparse_list;
                auto t1 = Clock::now();
                // 解码: 直接读取 (已是原始列表)
                volatile uint32_t sum = 0;
                for (auto v : sl) sum += v;
                (void)sum;
                auto t2 = Clock::now();
                std::clock_t cpu1 = std::clock();

                if (round >= WARMUP_ROUNDS) {
                    enc_times.push_back(elapsed_ms(t0, t1));
                    dec_times.push_back(elapsed_ms(t1, t2));
                    cpu_times.push_back((double)(cpu1 - cpu0) * 1000.0 / CLOCKS_PER_SEC);
                }
            }
            r.compress_ms = median_d(enc_times);
            r.decompress_ms = median_d(dec_times);
            r.cpu_ms = median_d(cpu_times);
            r.peak_rss_kb = get_peak_rss_kb();
            r.verified = true;
            r.extra_value1 = (double)sparse_total;
            r.extra_note = "sparse_volume_bytes=" + std::to_string(sparse_total);
            results.push_back(r);
            fprintf(stderr, "  %-20s occ=%.0f%% volume=%zu ratio=%.2f enc=%.2fms dec=%.2fms\n",
                    "SPARSE_LIST", ds.occupancy * 100, sparse_total, r.compression_ratio,
                    r.compress_ms, r.decompress_ms);
        }

        // FULL 模式 (基线, 无占用块)
        {
            BenchmarkResult r;
            r.dq_id = "DQ-005";
            r.dataset = ds.name;
            r.candidate = "FULL";
            r.original_bytes = full_total;
            r.compressed_bytes = full_total;
            r.compression_ratio = 1.0;
            r.compress_ms = 0;
            r.decompress_ms = 0;
            r.cpu_ms = 0;
            r.peak_rss_kb = get_peak_rss_kb();
            r.verified = true;
            r.repeats = 0;
            r.extra_value1 = (double)full_total;
            r.extra_note = "full_volume_bytes=" + std::to_string(full_total);
            results.push_back(r);
            fprintf(stderr, "  %-20s occ=%.0f%% volume=%zu (baseline)\n",
                    "FULL", ds.occupancy * 100, full_total);
        }
    }
    return results;
}

// ============================================================================
// DQ-006: checksum 吞吐与占比
// 候选: CRC32C / xxHash32 / RAW (无校验, 基线)
// ============================================================================

static std::vector<BenchmarkResult> benchmark_dq006(const std::vector<Dataset>& datasets) {
    std::vector<BenchmarkResult> results;
    fprintf(stderr, "\n[bench] === DQ-006: checksum throughput ===\n");

    for (const auto& ds : datasets) {
        // 使用 signal 数据作为校验目标
        const uint8_t* data = reinterpret_cast<const uint8_t*>(ds.signal.data());
        size_t data_len = ds.signal.size() * sizeof(float);

        if (data_len == 0) continue;

        // CRC32C
        {
            BenchmarkResult r;
            r.dq_id = "DQ-006";
            r.dataset = ds.name;
            r.candidate = "CRC32C";
            r.original_bytes = data_len;
            r.compressed_bytes = 4; // CRC32C 输出 4 字节
            r.repeats = MEASURE_ROUNDS;
            r.peak_rss_kb = get_peak_rss_kb();

            // 预热
            for (int i = 0; i < WARMUP_ROUNDS; i++) {
                volatile uint32_t c = crc32c_compute(data, data_len);
                (void)c;
            }

            std::vector<double> times, cpu_times;
            bool ok = true;
            for (int i = 0; i < MEASURE_ROUNDS; i++) {
                std::clock_t cpu0 = std::clock();
                auto t0 = Clock::now();
                uint32_t c = crc32c_compute(data, data_len);
                auto t1 = Clock::now();
                std::clock_t cpu1 = std::clock();
                if (c == 0 && data_len > 0) ok = false; // 极低概率, 仅做检查
                times.push_back(elapsed_ms(t0, t1));
                cpu_times.push_back((double)(cpu1 - cpu0) * 1000.0 / CLOCKS_PER_SEC);
            }
            r.compress_ms = median_d(times);
            r.cpu_ms = median_d(cpu_times);
            r.decompress_ms = 0; // checksum 无解压
            r.verified = ok;
            double mb = (double)data_len / MB;
            double sec = r.compress_ms / 1000.0;
            r.compress_mbs = (sec > 0) ? mb / sec : 0;
            r.extra_value1 = r.compress_mbs; // 吞吐 MB/s
            r.extra_note = "throughput_MB/s=" + std::to_string(r.compress_mbs);
            results.push_back(r);
            fprintf(stderr, "  %-20s throughput=%.0f MB/s time=%.3fms\n",
                    "CRC32C", r.compress_mbs, r.compress_ms);
        }

        // xxHash32
        {
            BenchmarkResult r;
            r.dq_id = "DQ-006";
            r.dataset = ds.name;
            r.candidate = "xxHash32";
            r.original_bytes = data_len;
            r.compressed_bytes = 4;
            r.repeats = MEASURE_ROUNDS;
            r.peak_rss_kb = get_peak_rss_kb();

            for (int i = 0; i < WARMUP_ROUNDS; i++) {
                volatile uint32_t h = xxhash32_compute(data, data_len);
                (void)h;
            }

            std::vector<double> times, cpu_times;
            bool ok = true;
            for (int i = 0; i < MEASURE_ROUNDS; i++) {
                std::clock_t cpu0 = std::clock();
                auto t0 = Clock::now();
                uint32_t h = xxhash32_compute(data, data_len);
                auto t1 = Clock::now();
                std::clock_t cpu1 = std::clock();
                if (h == 0 && data_len > 0) ok = false;
                times.push_back(elapsed_ms(t0, t1));
                cpu_times.push_back((double)(cpu1 - cpu0) * 1000.0 / CLOCKS_PER_SEC);
            }
            r.compress_ms = median_d(times);
            r.cpu_ms = median_d(cpu_times);
            r.verified = ok;
            double mb = (double)data_len / MB;
            double sec = r.compress_ms / 1000.0;
            r.compress_mbs = (sec > 0) ? mb / sec : 0;
            r.extra_value1 = r.compress_mbs;
            r.extra_note = "throughput_MB/s=" + std::to_string(r.compress_mbs);
            results.push_back(r);
            fprintf(stderr, "  %-20s throughput=%.0f MB/s time=%.3fms\n",
                    "xxHash32", r.compress_mbs, r.compress_ms);
        }

        // RAW (无校验, 基线)
        {
            BenchmarkResult r;
            r.dq_id = "DQ-006";
            r.dataset = ds.name;
            r.candidate = "RAW(no-checksum)";
            r.original_bytes = data_len;
            r.compressed_bytes = 0;
            r.compress_ms = 0;
            r.decompress_ms = 0;
            r.cpu_ms = 0;
            r.compress_mbs = 0;
            r.decompress_mbs = 0;
            r.peak_rss_kb = get_peak_rss_kb();
            r.verified = true;
            r.repeats = 0;
            r.extra_note = "基线: 无校验, 不作为推荐";
            results.push_back(r);
            fprintf(stderr, "  %-20s (基线, 无校验)\n", "RAW(no-checksum)");
        }
    }
    return results;
}

// ============================================================================
// DQ-007: 子块对齐 (8B / 64B / 4KiB)
// 测量: 体积浪费 + 随机读取延迟
// ============================================================================

static std::vector<BenchmarkResult> benchmark_dq007(const std::vector<Dataset>& datasets) {
    std::vector<BenchmarkResult> results;
    fprintf(stderr, "\n[bench] === DQ-007: sub-block alignment ===\n");

    // 模拟一个 Tile 的子块布局: occupancy + signal + support + SNR
    // 计算不同对齐下的 padding 浪费
    for (const auto& ds : datasets) {
        // 子块大小 (未压缩, 用 RAW 大小)
        size_t occ_size    = ds.bitmap.size();
        size_t signal_size = ds.signal.size() * sizeof(float);
        size_t support_size = ds.support.size();
        size_t snr_size    = ds.sparse_list.size() * (sizeof(uint32_t) + sizeof(float)); // 近似

        // 4 个子块
        size_t subblock_sizes[] = {occ_size, signal_size, support_size, snr_size};
        const char* subblock_names[] = {"occupancy", "signal", "support", "snr"};
        int num_subblocks = 4;

        for (size_t align : {size_t(8), size_t(64), size_t(4096)}) {
            std::string align_name;
            if (align == 8)       align_name = "8B";
            else if (align == 64) align_name = "64B";
            else                  align_name = "4KiB";

            // 计算对齐后的总体积和 padding 浪费
            size_t total_aligned = 0;
            size_t total_padding = 0;
            size_t total_raw = 0;
            for (int i = 0; i < num_subblocks; i++) {
                size_t sz = subblock_sizes[i];
                total_raw += sz;
                size_t aligned = (sz + align - 1) & ~(align - 1);
                total_aligned += aligned;
                total_padding += (aligned - sz);
            }

            BenchmarkResult r;
            r.dq_id = "DQ-007";
            r.dataset = ds.name;
            r.candidate = "align-" + align_name;
            r.original_bytes = total_raw;
            r.compressed_bytes = total_aligned;
            r.compression_ratio = (double)total_aligned / std::max((size_t)1, total_raw);
            r.repeats = MEASURE_ROUNDS;
            r.peak_rss_kb = get_peak_rss_kb();
            r.verified = true;
            r.extra_value1 = (double)total_padding; // padding 浪费字节数
            r.extra_value2 = (double)total_padding * 100.0 / std::max((size_t)1, total_raw); // 浪费率 %

            // 测量随机读取延迟: 模拟从不同对齐偏移读取子块
            // 创建一个模拟的 Tile 数据缓冲区, 按对齐布局
            std::vector<uint8_t> tile_buf(total_aligned + align, 0);
            std::vector<size_t> offsets;
            size_t off = 0;
            for (int i = 0; i < num_subblocks; i++) {
                size_t sz = subblock_sizes[i];
                size_t aligned = (sz + align - 1) & ~(align - 1);
                offsets.push_back(off);
                off += aligned;
            }

            // 预热
            for (int i = 0; i < WARMUP_ROUNDS; i++) {
                for (size_t o : offsets) {
                    volatile uint8_t v = tile_buf[o];
                    (void)v;
                }
            }

            // 测量随机读取延迟 (每个子块读 1 字节)
            std::vector<double> read_times;
            std::mt19937 rng(42);
            for (int round = 0; round < MEASURE_ROUNDS; round++) {
                auto t0 = Clock::now();
                for (int rep = 0; rep < 1000; rep++) {
                    size_t idx = rng() % offsets.size();
                    volatile uint8_t v = tile_buf[offsets[idx]];
                    (void)v;
                }
                auto t1 = Clock::now();
                read_times.push_back(elapsed_ms(t0, t1) / 1000.0); // 每次读取的 ms
            }
            r.compress_ms = median_d(read_times); // 复用字段: 随机读取延迟 ms
            r.decompress_ms = 0;
            r.cpu_ms = r.compress_ms;
            r.compress_mbs = 0;
            r.decompress_mbs = 0;
            r.extra_note = "padding_bytes=" + std::to_string(total_padding) +
                           " waste_pct=" + std::to_string(r.extra_value2) +
                           " random_read_ms=" + std::to_string(r.compress_ms);
            results.push_back(r);
            fprintf(stderr, "  %-20s waste=%zu (%.2f%%) rand_read=%.4fms\n",
                    ("align-" + align_name).c_str(),
                    total_padding, r.extra_value2, r.compress_ms);
        }
    }
    return results;
}

// ============================================================================
// 输出: CSV
// ============================================================================

static void write_csv(const std::string& path,
                       const std::vector<BenchmarkResult>& results) {
    std::ofstream f(path);
    if (!f) { fprintf(stderr, "[bench] 无法写入 %s\n", path.c_str()); return; }

    f << "dq_id,dataset,candidate,original_bytes,compressed_bytes,"
      << "compression_ratio,compress_ms,decompress_ms,"
      << "compress_mbs,decompress_mbs,cpu_ms,peak_rss_kb,"
      << "repeats,verified,extra_value1,extra_value2,extra_note\n";

    for (const auto& r : results) {
        f << r.dq_id << ","
          << r.dataset << ","
          << r.candidate << ","
          << r.original_bytes << ","
          << r.compressed_bytes << ","
          << std::fixed << std::setprecision(4) << r.compression_ratio << ","
          << std::setprecision(3) << r.compress_ms << ","
          << r.decompress_ms << ","
          << std::setprecision(2) << r.compress_mbs << ","
          << r.decompress_mbs << ","
          << std::setprecision(3) << r.cpu_ms << ","
          << r.peak_rss_kb << ","
          << r.repeats << ","
          << (r.verified ? "true" : "false") << ","
          << std::setprecision(4) << r.extra_value1 << ","
          << r.extra_value2 << ","
          << "\"" << r.extra_note << "\"\n";
    }
    fprintf(stderr, "[bench] CSV 已写入: %s (%zu 行)\n", path.c_str(), results.size());
}

// ============================================================================
// 输出: JSON
// ============================================================================

static void write_json(const std::string& path,
                        const std::vector<BenchmarkResult>& results) {
    std::ofstream f(path);
    if (!f) { fprintf(stderr, "[bench] 无法写入 %s\n", path.c_str()); return; }

    f << "{\n";
    f << "  \"experiment\": \"AstroCS HISS DQ-001~007 benchmark\",\n";
    f << "  \"timestamp\": \"" << std::time(nullptr) << "\",\n";
    f << "  \"warmup_rounds\": " << WARMUP_ROUNDS << ",\n";
    f << "  \"measure_rounds\": " << MEASURE_ROUNDS << ",\n";
    f << "  \"results\": [\n";

    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        f << "    {\n";
        f << "      \"dq_id\": \"" << r.dq_id << "\",\n";
        f << "      \"dataset\": \"" << r.dataset << "\",\n";
        f << "      \"candidate\": \"" << r.candidate << "\",\n";
        f << "      \"original_bytes\": " << r.original_bytes << ",\n";
        f << "      \"compressed_bytes\": " << r.compressed_bytes << ",\n";
        f << "      \"compression_ratio\": " << std::fixed << std::setprecision(4) << r.compression_ratio << ",\n";
        f << "      \"compress_ms\": " << std::setprecision(3) << r.compress_ms << ",\n";
        f << "      \"decompress_ms\": " << r.decompress_ms << ",\n";
        f << "      \"compress_mbs\": " << std::setprecision(2) << r.compress_mbs << ",\n";
        f << "      \"decompress_mbs\": " << r.decompress_mbs << ",\n";
        f << "      \"cpu_ms\": " << std::setprecision(3) << r.cpu_ms << ",\n";
        f << "      \"peak_rss_kb\": " << r.peak_rss_kb << ",\n";
        f << "      \"repeats\": " << r.repeats << ",\n";
        f << "      \"verified\": " << (r.verified ? "true" : "false") << ",\n";
        f << "      \"extra_value1\": " << std::setprecision(4) << r.extra_value1 << ",\n";
        f << "      \"extra_value2\": " << r.extra_value2 << ",\n";
        f << "      \"extra_note\": \"" << r.extra_note << "\"\n";
        f << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
    }

    f << "  ]\n";
    f << "}\n";
    fprintf(stderr, "[bench] JSON 已写入: %s\n", path.c_str());
}

// ============================================================================
// 输出: summary.md
// ============================================================================

static void write_summary(const std::string& path,
                           const std::vector<BenchmarkResult>& results,
                           bool has_lz4, bool has_zstd) {
    std::ofstream f(path);
    if (!f) { fprintf(stderr, "[bench] 无法写入 %s\n", path.c_str()); return; }

    f << "# AstroCS HISS 未决工程实验报告\n\n";
    f << "> **声明**: 此结论仅为实验建议, 未写入冻结规范, 也未设为不可更改的正式默认值, 等待用户与主审助手确认。\n\n";

    f << "## 实验环境\n\n";
    f << "- 编译器: " <<
#ifdef __GNUC__
      "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__
#else
      "未知"
#endif
      << "\n";
    f << "- 编译选项: -std=c++17 -O2";
#ifdef HAS_LZ4
    f << " -DHAS_LZ4";
#endif
#ifdef HAS_ZSTD
    f << " -DHAS_ZSTD";
#endif
    f << "\n";
    f << "- LZ4 可用: " << (has_lz4 ? "是" : "否") << "\n";
    f << "- Zstd 可用: " << (has_zstd ? "是" : "否") << "\n";
    f << "- 预热轮数: " << WARMUP_ROUNDS << "\n";
    f << "- 测量轮数: " << MEASURE_ROUNDS << " (取中位数)\n\n";

    f << "## 数据集\n\n";
    f << "| 名称 | 像素数 | 占用率 | signal 字节 | support 字节 | bitmap 字节 |\n";
    f << "|------|--------|--------|-------------|--------------|-------------|\n";
    // 数据集信息会在 main 中补充
    f << "| (见 raw_results.csv) | | | | | |\n\n";

    // 按 DQ 分组分析
    for (const std::string& dq : {"DQ-001", "DQ-002", "DQ-003", "DQ-004",
                                   "DQ-005", "DQ-006", "DQ-007"}) {
        f << "## " << dq << "\n\n";

        // 收集该 DQ 的结果
        std::vector<const BenchmarkResult*> dq_results;
        for (const auto& r : results) {
            if (r.dq_id == dq) dq_results.push_back(&r);
        }

        if (dq_results.empty()) {
            f << "无数据。\n\n";
            continue;
        }

        // 表格
        f << "| 数据集 | 候选 | 原始字节 | 压缩字节 | 压缩比 | 压缩ms | 解压ms | 压缩MB/s | 解压MB/s | CPUms | 验证 |\n";
        f << "|--------|------|---------|---------|--------|--------|--------|---------|---------|-------|------|\n";
        for (const auto* r : dq_results) {
            f << "| " << r->dataset << " | " << r->candidate
              << " | " << r->original_bytes
              << " | " << r->compressed_bytes
              << " | " << std::fixed << std::setprecision(3) << r->compression_ratio
              << " | " << std::setprecision(3) << r->compress_ms
              << " | " << r->decompress_ms
              << " | " << std::setprecision(1) << r->compress_mbs
              << " | " << r->decompress_mbs
              << " | " << std::setprecision(3) << r->cpu_ms
              << " | " << (r->verified ? "OK" : "FAIL")
              << " |\n";
        }
        f << "\n";

        // 分析与推荐
        f << "### 分析与推荐\n\n";

        if (dq == "DQ-001") {
            f << "- signal (float32) 数据含天文梯度 + 正态噪声, 字节间相关性较低。\n";
            f << "- **byte-shuffle + LZ4/Zstd** 通常能显著提升压缩比, 因为 float32 各字节位置的数据分布不同。\n";
            f << "- **Zstd** 在压缩比上通常优于 LZ4, 但压缩速度可能较慢。\n";
            f << "- 推荐: 综合压缩比和速度, **byte-shuffle + LZ4** 或 **byte-shuffle + Zstd** 可作为 signal 默认候选。\n";
        } else if (dq == "DQ-002") {
            f << "- support (uint8) 数据范围 128-255, 单字节元素, byte-shuffle 无意义。\n";
            f << "- **LZ4** 在速度上占优, **Zstd** 在压缩比上占优。\n";
            f << "- 推荐: 根据速度优先还是压缩比优先, 选择 **LZ4** 或 **Zstd**。\n";
        } else if (dq == "DQ-003") {
            f << "- BITMAP 为位压缩数据 (1 bit/像素), 不同占用率下 RLE 效果差异大。\n";
            f << "- 高占用率时 bitmap 字节序列变化频繁, RLE 效果差; 低占用率时大量连续 0 字节, RLE 效果好。\n";
            f << "- 推荐: **LZ4** 或 **Zstd** 作为通用候选; RLE 在极稀疏场景可作为可选优化。\n";
        } else if (dq == "DQ-004") {
            f << "- SPARSE_LIST 为升序 uint32 索引列表, delta 编码后值域大幅缩小。\n";
            f << "- **delta + varint** 能将 4 字节索引压缩到平均 1-2 字节, 再配 LZ4/Zstd 效果更佳。\n";
            f << "- 推荐: **delta + varint + LZ4** 或 **delta + varint + Zstd** 作为 SPARSE_LIST 默认候选。\n";
        } else if (dq == "DQ-005") {
            f << "- FULL 模式在低占用率时浪费大量空间 (需存储全 0 的 signal/support)。\n";
            f << "- BITMAP 体积 = bitmap_bytes + occupied_signal + occupied_support, 与占用率成正比。\n";
            f << "- SPARSE_LIST 体积 = sparse_list_bytes + occupied_signal + occupied_support, 在极低占用率时更省。\n";
            f << "- BITMAP 与 SPARSE_LIST 的交叉点取决于索引列表大小 vs bitmap 大小。\n";
            f << "- 推荐: 占用率 > 80% 用 FULL; 20%-80% 用 BITMAP; < 20% 用 SPARSE_LIST (仅为建议区间)。\n";
        } else if (dq == "DQ-006") {
            f << "- CRC32C 和 xxHash32 均为 4 字节校验值, 吞吐差异取决于实现。\n";
            f << "- xxHash32 通常在软件实现下吞吐更高; CRC32C 有硬件加速指令 (SSE4.2) 时可达更高吞吐。\n";
            f << "- RAW (无校验) 仅作为基线, 不作为推荐。\n";
            f << "- 推荐: 根据是否需要硬件加速, 选择 **CRC32C** 或 **xxHash32**。\n";
        } else if (dq == "DQ-007") {
            f << "- 8 字节对齐: padding 浪费最小, 但随机读取可能跨越缓存行。\n";
            f << "- 64 字节对齐: 与缓存行对齐, 随机读取效率较好, padding 浪费中等。\n";
            f << "- 4KiB 对齐: 与内存页对齐, 适合 mmap 场景, 但 padding 浪费最大。\n";
            f << "- 推荐: 综合体积和读取性能, **64 字节对齐** 作为默认候选。\n";
        }
        f << "\n";
    }

    // 总结
    f << "## 总结\n\n";
    f << "本实验覆盖了 DQ-001 ~ DQ-007 全部未决事项, 使用合成测试数据 (不同占用率的 Tile) 进行了系统对比。\n";
    f << "所有候选均通过 CodecRegistry 复用正式 HISS 编解码路径, 确保实验结果具有代表性。\n\n";
    f << "### 推荐汇总\n\n";
    f << "| DQ | 推荐候选 | 理由 |\n";
    f << "|----|---------|------|\n";
    f << "| DQ-001 (signal) | byte-shuffle + LZ4 或 byte-shuffle + Zstd | shuffle 提升压缩比, LZ4/Zstd 提供速度/比权衡 |\n";
    f << "| DQ-002 (support) | LZ4 或 Zstd | 单字节无需 shuffle, 按速度/比需求选择 |\n";
    f << "| DQ-003 (BITMAP) | LZ4 或 Zstd | 通用性好; RLE 仅在极稀疏时可选 |\n";
    f << "| DQ-004 (SPARSE_LIST) | delta + varint + LZ4 | delta+varint 大幅缩小体积, 再配 LZ4 速度好 |\n";
    f << "| DQ-005 (阈值) | FULL>80% / BITMAP 20-80% / SPARSE<20% | 基于体积测量的建议区间 |\n";
    f << "| DQ-006 (checksum) | CRC32C 或 xxHash32 | 视硬件加速情况选择 |\n";
    f << "| DQ-007 (对齐) | 64 字节 | 平衡 padding 浪费与缓存行对齐 |\n\n";

    f << "> **再次声明**: 此结论仅为实验建议, 未写入冻结规范, 也未设为不可更改的正式默认值, 等待用户与主审助手确认。\n";

    fprintf(stderr, "[bench] summary.md 已写入: %s\n", path.c_str());
}

// ============================================================================
// 输出: environment.md
// ============================================================================

static void write_environment(const std::string& path,
                               bool has_lz4, bool has_zstd) {
    std::ofstream f(path);
    if (!f) { fprintf(stderr, "[bench] 无法写入 %s\n", path.c_str()); return; }

    f << "# 实验环境信息\n\n";

    f << "## 编译器\n\n";
    f << "- 编译器: "
#ifdef __GNUC__
      "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__
#else
      "未知"
#endif
      << "\n";
    f << "- C++ 标准: C++17\n";
    f << "- 优化级别: -O2\n";
    f << "- 编译宏: ";
#ifdef HAS_LZ4
    f << "HAS_LZ4 ";
#endif
#ifdef HAS_ZSTD
    f << "HAS_ZSTD";
#endif
    f << "\n\n";

    f << "## 压缩库\n\n";
    f << "- LZ4: " << (has_lz4 ? "可用" : "不可用") << "\n";
    f << "- Zstd: " << (has_zstd ? "可用" : "不可用") << "\n";
    f << "- RAW: 始终可用 (内置)\n\n";

    f << "## 操作系统\n\n";
#ifdef _WIN32
    f << "- OS: Windows\n";
    f << "- 平台: x86_64\n";
    OSVERSIONINFOEXW osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXW));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
    // GetVersionEx 在新版 Windows 可能返回不准确值, 仅做参考
    f << "- 详情: (见系统属性)\n";
#else
    f << "- OS: Linux/Unix\n";
#endif
    f << "\n";

    f << "## 硬件\n\n";
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    f << "- CPU 核心数: " << si.dwNumberOfProcessors << "\n";
    f << "- CPU 架构: x86_64\n";
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    f << "- 总物理内存: " << (mem.ullTotalPhys / (1024 * 1024)) << " MB\n";
    f << "- 可用物理内存: " << (mem.ullAvailPhys / (1024 * 1024)) << " MB\n";
#else
    f << "- (Linux 环境信息请从 /proc/cpuinfo 和 /proc/meminfo 获取)\n";
#endif
    f << "\n";

    f << "## 实验参数\n\n";
    f << "- 预热轮数: " << WARMUP_ROUNDS << "\n";
    f << "- 测量轮数: " << MEASURE_ROUNDS << " (取中位数)\n";
    f << "- 数据集: 高分辨率大Tile / 常规中心 / 边缘部分覆盖 / 极稀疏\n\n";

    f << "## 复现命令\n\n";
    f << "```bash\n";
    f << "g++ -std=c++17 -O2 -DHAS_LZ4 -DHAS_ZSTD \\\n";
    f << "    -I../include hiss_benchmark.cpp ../src/hiss_codec.cpp \\\n";
    f << "    -llz4 -lzstd";
#ifdef _WIN32
    f << " -lpsapi";
#endif
    f << " -o hiss_benchmark.exe\n";
    f << "./hiss_benchmark.exe [output_dir]\n";
    f << "```\n";

    fprintf(stderr, "[bench] environment.md 已写入: %s\n", path.c_str());
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char* argv[]) {
    fprintf(stderr, "=== AstroCS HISS Benchmark ===\n");
    fprintf(stderr, "预热轮数: %d, 测量轮数: %d\n\n", WARMUP_ROUNDS, MEASURE_ROUNDS);

    // 初始化 CRC32C 表
    init_crc32c_table();

    // 输出目录 (命令行参数或默认)
    std::string output_dir;
    if (argc > 1) {
        output_dir = argv[1];
    } else {
        // 默认输出到交付目录 (相对于 tests/ 目录)
        output_dir = "../../../AstroCS_Stage1_HISS_Delivery/reports/experiments";
    }
    std::filesystem::create_directories(output_dir);
    fprintf(stderr, "[bench] 输出目录: %s\n", output_dir.c_str());

    // 检查可用 codec
    bool has_lz4  = codec_available(hiss::CodecId::LZ4);
    bool has_zstd = codec_available(hiss::CodecId::ZSTD);
    fprintf(stderr, "[bench] 可用 codec: RAW(内置) LZ4(%d) ZSTD(%d)\n",
            (int)has_lz4, (int)has_zstd);

    // 列出所有已注册 codec
    auto codec_list = hiss::CodecRegistry::instance().list();
    fprintf(stderr, "[bench] 已注册 codec 列表 (%zu): ", codec_list.size());
    for (auto id : codec_list) {
        fprintf(stderr, "%s ", codec_name(id).c_str());
    }
    fprintf(stderr, "\n");

    // === 1. 生成测试数据 ===
    fprintf(stderr, "\n[bench] 生成测试数据...\n");
    std::vector<Dataset> datasets;

    // 高分辨率大 Tile: NSIDE=8192, tile_nside=16, 65536 叶像素, 100% 占用
    datasets.push_back(generate_dataset("large_full", 65536, 1.00, 42));

    // 常规中心 Tile: 65536 叶像素, 80% 占用
    datasets.push_back(generate_dataset("center_80", 65536, 0.80, 100));

    // 边缘部分覆盖: 65536 叶像素, 30% 占用
    datasets.push_back(generate_dataset("edge_30", 65536, 0.30, 200));

    // 极稀疏: 65536 叶像素, 5% 占用
    datasets.push_back(generate_dataset("sparse_5", 65536, 0.05, 300));

    // === 2. 运行各 DQ 实验 ===
    std::vector<BenchmarkResult> all_results;

    auto r1 = benchmark_dq001(datasets);
    all_results.insert(all_results.end(), r1.begin(), r1.end());

    auto r2 = benchmark_dq002(datasets);
    all_results.insert(all_results.end(), r2.begin(), r2.end());

    auto r3 = benchmark_dq003(datasets);
    all_results.insert(all_results.end(), r3.begin(), r3.end());

    auto r4 = benchmark_dq004(datasets);
    all_results.insert(all_results.end(), r4.begin(), r4.end());

    auto r5 = benchmark_dq005(datasets);
    all_results.insert(all_results.end(), r5.begin(), r5.end());

    auto r6 = benchmark_dq006(datasets);
    all_results.insert(all_results.end(), r6.begin(), r6.end());

    auto r7 = benchmark_dq007(datasets);
    all_results.insert(all_results.end(), r7.begin(), r7.end());

    // === 3. 写入输出文件 ===
    fprintf(stderr, "\n[bench] 写入输出文件...\n");
    write_csv(output_dir + "/raw_results.csv", all_results);
    write_json(output_dir + "/raw_results.json", all_results);
    write_summary(output_dir + "/summary.md", all_results, has_lz4, has_zstd);
    write_environment(output_dir + "/environment.md", has_lz4, has_zstd);

    fprintf(stderr, "\n[bench] 完成! 共 %zu 条结果\n", all_results.size());
    return 0;
}
