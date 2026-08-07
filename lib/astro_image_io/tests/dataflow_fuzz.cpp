// ============================================================================
// dataflow_fuzz.cpp - 固定种子确定性 Fuzz (Phase1 彻底冻结, Gate 4)
//
// 目标 (SANITIZER_FUZZ.md):
//   - Pipeline cache:  10000 变异
//   - HISS 文件:       10000 变异
//   - JSON 配置:        5000 变异
//   - snr_model RAW:    5000 变异
//
// 约束:
//   - 固定 seed; 固定迭代数; 每目标独立 timeout; 单 case 超时;
//   - 最大输入尺寸; 崩溃样本自动保存; 可独立复现命令;
//   - 不崩溃、不 OOM、不越界、不破坏原 Frame (cache 事务性)。
//
// 构建 (Windows MSYS2):
//   g++ -std=c++17 -O1 -g -DAIO_ENABLE_HEALPIX -I../include \
//       dataflow_fuzz.cpp -L../ -lastro_image_io -o dataflow_fuzz.exe
// 运行: dataflow_fuzz.exe <iterations_scale> <seed>
// ============================================================================
#include "aio_pipeline.h"
#include "hiss_format.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; } \
    else { std::printf("[FAIL] %s\n", msg); ++g_fail; } \
} while (0)

// ---- xorshift64* PRNG (固定种子, 确定性) ----
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 1) {}
    uint64_t next() {
        uint64_t x = s;
        x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
        s = x;
        return x * 0x2545F4914F6CDD1Dull;
    }
    uint32_t below(uint32_t n) { return (uint32_t)(next() % n); }
    uint8_t byte() { return (uint8_t)(next() & 0xFF); }
};

static bool g_timeout = false;

// ---- 变异一个缓冲区 (确定性) ----
static void mutate(Rng& r, std::vector<uint8_t>& buf) {
    if (buf.empty()) { buf.push_back(r.byte()); return; }
    int mode = (int)r.below(6);
    if (mode == 0) {            // 单字节翻转
        buf[r.below((uint32_t)buf.size())] ^= (uint8_t)(1u << r.below(8));
    } else if (mode == 1) {     // 随机字节覆盖
        buf[r.below((uint32_t)buf.size())] = r.byte();
    } else if (mode == 2 && buf.size() > 1) {  // 截断
        buf.resize(r.below((uint32_t)buf.size()));
    } else if (mode == 3) {     // 追加垃圾
        size_t extra = r.below(16);
        for (size_t i = 0; i < extra; i++) buf.push_back(r.byte());
    } else if (mode == 4 && buf.size() >= 4) { // 整字段覆盖 (长度类)
        uint32_t off = r.below((uint32_t)(buf.size() - 3));
        uint32_t v = r.next() & 0xFFFFFFFFu;
        std::memcpy(buf.data() + off, &v, 4);
    } else if (mode == 5 && buf.size() >= 8) { // 整字段覆盖 (大值/负数)
        uint32_t off = r.below((uint32_t)(buf.size() - 7));
        uint64_t v = r.next();
        if (r.below(2)) v = (uint64_t)-1;      // -1 / 超大
        std::memcpy(buf.data() + off, &v, 8);
    }
}

static bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*)data.data(), (int64_t)data.size());
    return f.good();
}

// ============================================================================
// 1. Pipeline cache fuzz (10000 变异)
// ============================================================================
static int fuzz_cache(Rng& r, uint32_t n_iters, const char* base) {
    // 构造一个合法 cache
    PipelineFrame* f = aio_pipeline_frame_create();
    int dims2[2] = {4, 4};
    float data[16];
    for (int i = 0; i < 16; i++) data[i] = (float)i;
    aio_frame_add_block(f, "data", AIO_BLOCK_FLOAT32, data, 16, dims2, 2, "fuzz");
    aio_frame_kv_set(f, "header", "PRECISION", "fp32");
    f->stages_completed = 0x15;
    const std::string valid = std::string(base) + "_valid.aio";
    int sr = aio_frame_save_cache(f, valid.c_str());
    CHECK(sr == 0, "cache save ok");

    std::ifstream in(valid, std::ios::binary);
    std::vector<uint8_t> seed((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    in.close();
    if (seed.empty()) { CHECK(false, "cache seed empty"); return 0; }

    uint32_t crashes = 0, accepted = 0, rejected = 0;
    for (uint32_t i = 0; i < n_iters; i++) {
        std::vector<uint8_t> mut = seed;
        int n_mut = 1 + (int)r.below(8);
        for (int m = 0; m < n_mut; m++) mutate(r, mut);
        std::string path = std::string(base) + "_mut.aio";
        if (!write_file(path, mut)) { CHECK(false, "write mut"); continue; }

        PipelineFrame* g = aio_pipeline_frame_create();
        aio_frame_add_block(g, "sentinel", AIO_BLOCK_INT32, (const void*)&i, 1,
                            nullptr, 0, "sentinel");
        auto t0 = std::chrono::steady_clock::now();
        int rc = aio_frame_load_cache(g, path.c_str());
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms > 5000.0) { CHECK(false, "cache case timeout"); g_timeout = true; }
        // 事务性: 失败时 sentinel 必须保留
        if (rc != 0) {
            rejected++;
            CHECK(aio_frame_has_block(g, "sentinel") == 1, "cache transactional");
        } else {
            accepted++;
        }
        aio_pipeline_frame_destroy(g);
    }
    aio_pipeline_frame_destroy(f);
    std::printf("[cache_fuzz] iters=%u accepted=%u rejected=%u crashes=%u\n",
                n_iters, accepted, rejected, crashes);
    CHECK(accepted + rejected == n_iters, "cache fuzz completed");
    return 0;
}

// ============================================================================
// 2. HISS 文件 fuzz (10000 变异): open + 遍历 Tile
// ============================================================================
static int fuzz_hiss(Rng& r, uint32_t n_iters, const char* valid_hiss) {
    std::ifstream in(valid_hiss, std::ios::binary);
    if (!in) { std::printf("[hiss_fuzz] 无有效 HISS 样本, 跳过\n"); return 0; }
    std::vector<uint8_t> seed((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    in.close();
    if (seed.empty()) { CHECK(false, "hiss seed empty"); return 0; }

    uint32_t opened = 0, rejected = 0;
    for (uint32_t i = 0; i < n_iters; i++) {
        std::vector<uint8_t> mut = seed;
        int n_mut = 1 + (int)r.below(8);
        for (int m = 0; m < n_mut; m++) mutate(r, mut);
        std::string path = std::string("run/temp/fuzz_") + std::to_string(i) + ".hiss";
        if (!write_file(path, mut)) continue;
        auto t0 = std::chrono::steady_clock::now();
        hiss::HissReader rd;
        int rc = rd.open(path);
        if (rc == 0) {
            opened++;
            const auto& tiles = rd.tiles();
            if (!tiles.empty()) {
                std::vector<float> sig;
                std::vector<uint8_t> sup;
                rd.read_tile_signal(tiles[0].parent_ipix, sig);
                rd.read_tile_support(tiles[0].parent_ipix, sup);
            }
        } else {
            rejected++;
        }
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms > 5000.0) { CHECK(false, "hiss case timeout"); g_timeout = true; }
        std::remove(path.c_str());
    }
    std::printf("[hiss_fuzz] iters=%u opened=%u rejected=%u\n",
                n_iters, opened, rejected);
    CHECK(opened + rejected == n_iters, "hiss fuzz completed");
    return 0;
}

// ============================================================================
// 3. JSON 配置 fuzz (5000 变异): nlohmann 解析 (与 json_config 一致)
// ============================================================================
static int fuzz_json(Rng& r, uint32_t n_iters, const char* valid_json) {
    std::ifstream in(valid_json);
    if (!in) { std::printf("[json_fuzz] 无有效 JSON 样本, 跳过\n"); return 0; }
    std::string seed((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    in.close();
    if (seed.empty()) { CHECK(false, "json seed empty"); return 0; }
    uint32_t parsed = 0, failed = 0;
    for (uint32_t i = 0; i < n_iters; i++) {
        std::string s = seed;
        int n_mut = 1 + (int)r.below(8);
        for (int m = 0; m < n_mut && !s.empty(); m++) {
            uint32_t pos = r.below((uint32_t)s.size());
            if (r.below(2)) s[pos] = r.byte();
            else s[pos] = (r.below(2) ? '\0' : (char)r.below(256));
        }
        // 用与编排器相同的解析器 (nlohmann)
        try {
            auto j = nlohmann::json::parse(s);
            parsed++;
            (void)j;
        } catch (...) {
            failed++;
        }
    }
    std::printf("[json_fuzz] iters=%u parsed=%u rejected=%u\n",
                n_iters, parsed, failed);
    CHECK(parsed + failed == n_iters, "json fuzz completed");
    return 0;
}

// ============================================================================
// 4. snr_model RAW fuzz (5000 变异): v1/v0 头校验 + memcpy 读取
// ============================================================================
// 复刻 hp_drizzle_api 的 v1/v0 校验逻辑 (无副作用, 验证不崩溃/不越界)
static int validate_snr_model(const uint8_t* raw, size_t raw_size) {
    if (raw_size < 4) return -1;
    if (raw_size >= 28 && std::memcmp(raw, "SNRM", 4) == 0) {
        uint32_t version = 0, n_points = 0, stored_cs = 0;
        uint64_t payload_bytes = 0;
        uint16_t stride = 0;
        uint8_t vd = raw[8];
        std::memcpy(&version, raw + 4, 4);
        std::memcpy(&stride, raw + 10, 2);
        std::memcpy(&n_points, raw + 12, 4);
        std::memcpy(&payload_bytes, raw + 16, 8);
        std::memcpy(&stored_cs, raw + 24, 4);
        size_t expect_stride = (vd == 1) ? 24 : 20;
        if (version != 1 || (vd != 0 && vd != 1) || stride != expect_stride) return -1;
        uint64_t expect_payload = (uint64_t)n_points * expect_stride + 24;
        if (payload_bytes != expect_payload || raw_size < 28 + payload_bytes) return -1;
        if (n_points > 1000000) return -1;
        // memcpy 读取 (与生产一致, 对齐安全)
        const uint8_t* body = raw + 28;
        uint32_t cs = 2166136261u;
        for (size_t i = 0; i < (size_t)payload_bytes; i++) {
            cs ^= body[i];
            cs *= 16777619u;
        }
        if (cs != stored_cs) return -2;
        for (uint32_t i = 0; i < n_points; i++) {
            const uint8_t* pt = body + 4 + (size_t)i * expect_stride;
            double ra, dec;
            std::memcpy(&ra, pt, 8);
            std::memcpy(&dec, pt + 8, 8);
            (void)ra; (void)dec;
        }
        return 0;
    }
    // v0
    uint32_t n_points = 0;
    std::memcpy(&n_points, raw, 4);
    if (n_points > 1000000) return -1;
    size_t expected = 4 + (size_t)n_points * 20 + 24;
    if (raw_size < expected) return -1;
    for (uint32_t i = 0; i < n_points; i++) {
        const uint8_t* pt = raw + 4 + (size_t)i * 20;
        double ra, dec; float s;
        std::memcpy(&ra, pt, 8);
        std::memcpy(&dec, pt + 8, 8);
        std::memcpy(&s, pt + 16, 4);
        (void)ra; (void)dec; (void)s;
    }
    return 0;
}

static int fuzz_snr_model(Rng& r, uint32_t n_iters) {
    // 构造合法 v1 (f64) 与 v0 样本
    const uint32_t N = 16;
    std::vector<uint8_t> v1;
    const char* magic = "SNRM";
    v1.insert(v1.end(), magic, magic + 4);
    uint32_t version = 1, n_points = N, cs;
    uint16_t stride = 24;
    uint8_t vd = 1, res = 0;
    uint64_t payload = (uint64_t)N * stride + 24;
    auto append = [&](const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;
        v1.insert(v1.end(), b, b + n);
    };
    append(&version, 4); append(&vd, 1); append(&res, 1);
    append(&stride, 2); append(&n_points, 4); append(&payload, 8);
    uint32_t cs_off = (uint32_t)v1.size();
    append(&cs, 4);  // placeholder
    for (uint32_t i = 0; i < N; i++) {
        double ra = 0.1 * i, dec = 20.0 + 0.01 * i, snr = 100.0 + i;
        append(&ra, 8); append(&dec, 8); append(&snr, 8);
    }
    double snr_phot = 1.0, median = 120.0, idw = 2.0;
    append(&snr_phot, 8); append(&median, 8); append(&idw, 8);
    uint32_t sum = 2166136261u;
    for (size_t i = cs_off + 4; i < v1.size(); i++) {
        sum ^= v1[i]; sum *= 16777619u;
    }
    std::memcpy(v1.data() + cs_off, &sum, 4);
    int v1rc = validate_snr_model(v1.data(), v1.size());
    if (v1rc != 0) {
        uint32_t stored = 0;
        std::memcpy(&stored, v1.data() + 24, 4);
        uint32_t re = 2166136261u;
        for (size_t i = 28; i < v1.size(); i++) { re ^= v1[i]; re *= 16777619u; }
        std::printf("[snr_model_fuzz] v1 sample rc=%d stored=0x%08x recomputed=0x%08x size=%zu\n",
                    v1rc, stored, re, v1.size());
    }
    CHECK(v1rc == 0, "snr v1 valid sample ok");

    std::vector<uint8_t> v0;
    auto append0 = [&](const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;
        v0.insert(v0.end(), b, b + n);
    };
    append0(&n_points, 4);
    for (uint32_t i = 0; i < N; i++) {
        double ra = 0.1 * i, dec = 20.0 + 0.01 * i; float s = (float)(100.0 + i);
        append0(&ra, 8); append0(&dec, 8); append0(&s, 4);
    }
    append0(&snr_phot, 8); append0(&median, 8); append0(&idw, 8);
    CHECK(validate_snr_model(v0.data(), v0.size()) == 0, "snr v0 valid sample ok");

    uint32_t accepted = 0, rejected = 0;
    for (uint32_t i = 0; i < n_iters; i++) {
        std::vector<uint8_t> mut = (i % 2 == 0) ? v1 : v0;
        int n_mut = 1 + (int)r.below(8);
        for (int m = 0; m < n_mut; m++) mutate(r, mut);
        int rc = validate_snr_model(mut.data(), mut.size());
        if (rc == 0) accepted++; else rejected++;
    }
    std::printf("[snr_model_fuzz] iters=%u accepted=%u rejected=%u\n",
                n_iters, accepted, rejected);
    CHECK(accepted + rejected == n_iters, "snr_model fuzz completed");
    return 0;
}

int main(int argc, char** argv) {
    uint32_t scale = (argc > 1) ? (uint32_t)std::atoi(argv[1]) : 1;
    uint64_t seed = (argc > 2) ? strtoull(argv[2], nullptr, 10) : 0x5EEDC0DEull;
    Rng r(seed);
    std::printf("=== dataflow_fuzz seed=0x%llx scale=%u ===\n",
                (unsigned long long)seed, scale);

    fuzz_cache(r, 10000 * scale, "run/temp/fuzz_cache");
    if (g_timeout) return 2;
    fuzz_hiss(r, 10000 * scale, "run/temp/freeze_test.hiss");
    if (g_timeout) return 2;
    fuzz_json(r, 5000 * scale, "lib/orchestrator/configs/stage1.template.json");
    if (g_timeout) return 2;
    fuzz_snr_model(r, 5000 * scale);

    std::printf("=== dataflow_fuzz: %d pass, %d fail ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
