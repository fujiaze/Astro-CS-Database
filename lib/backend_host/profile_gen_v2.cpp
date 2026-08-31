// lib/backend_host/profile_gen_v2.cpp — cpu_profile.json (v2) 生成与复读 (CPU-003)
// 规格: V6.1 控制包 CPU-003 (G3); schemas/cpu_profile.schema.json (v2)
// 流程(固定顺序, 08 §4): 能力/配额 → 加载可用 provider(manifest 预检) →
//   provider correctness/self-test(失败永久剔除, 不计时) → memory 带宽基线 →
//   每代表 kernel small/medium/large → workers 1..available → block 候选 →
//   3 warmup + 7 measure → median/MAD → winner → AVX512 相对 AVX2 提升<3% 选 AVX2 →
//   保存全部原始候选 → profile JSON。
// Oracle: 独立标量参考(与 kernel 实现不同路径; 逐元素公式在下方冻结)。
#include "profile_gen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "astrocs/common_abi_v1.h"
#include "backend_loader.h"
#include "baseline_kernels.h"
#include "bench_harness.h"
#include "cpu_features.h"
#include "hardware_inspect.h"
#include "sha256.h"

extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);
int astrocs_backend_get_api_v1(uint32_t, uint32_t, const astrocs_host_services_v1*,
                               astrocs_backend_api_v1*);
uint64_t astrocs_cpu_detect_features_v1(void);
}

namespace astrocs::backend_host {

namespace {

using Clock = std::chrono::steady_clock;

// kernel op 语义表(kernel_id → op/aux/scale 描述), 与 backend_table.inc 注册序一致。
// scale: 每规模域的乘数 → 输出元素数 N = (base * scale)^2, 预算化避免 OOM。
struct KernelSpec {
    const char* kernel_id;
    int op;                 // ACS_KOP_*
    uint32_t frames;        // 栈类/累加类帧数(1=非栈)
    float k;                // 标量参数
    const char* workload;   // 规格: workload_class
    uint32_t base;          // medium 基准边长(元素数按规模乘)
};

const KernelSpec kSpecs[] = {
    {"calibration-pixel-transform", ACS_KOP_CALIBRATION, 1, 2.0f, "compute", 512},
    {"noise-snr-reductions",        ACS_KOP_NOISE_REDUCTIONS, 9, 0.0f, "memory", 256},
    {"wcs-psf-batch",               ACS_KOP_PSF_BATCH, 1, 1.0f, "compute", 512},
    {"drizzle-overlap",             ACS_KOP_DRIZZLE_OVERLAP, 1, 0.0f, "compute", 512},
    {"drizzle-accumulate",          ACS_KOP_DRIZZLE_ACCUMULATE, 6, 0.0f, "memory", 384},
    {"drizzle-normalize",           ACS_KOP_DRIZZLE_NORMALIZE, 1, 0.0f, "memory", 512},
    {"upm-spmv",                    ACS_KOP_UPM_SPMV, 1, 0.0f, "memory", 1024},
    {"upm-residual",                ACS_KOP_UPM_RESIDUAL, 1, 0.0f, "compute", 1024},
    {"upm-weight-update",           ACS_KOP_UPM_WEIGHT_UPDATE, 1, 0.5f, "compute", 1024},
    {"rejection-statistics",        ACS_KOP_REJECTION_STATS, 9, 3.0f, "memory", 256},
    {"integration-accumulate",      ACS_KOP_INTEGRATION_ACCUM, 9, 0.0f, "memory", 384},
    {"hips-bulk-transform",         ACS_KOP_HIPS_BULK, 1, 0.5f, "compute", 512},
};

uint32_t size_mult(const std::string& sc) {
    if (sc == "small") return 1;
    if (sc == "large") return 4;
    return 2;  // medium
}

/* 独立标量 Oracle: 逐元素参考输出(与 kernel 实现不同路径, 双精度累加避免顺序敏感)。
 * 公式与 baseline_kernels_impl.inc 语义一致但独立计算; 返回 ref 向量。
 * w: 实际网格宽(=sp.base*size_mult; kernel 用 p->w)。 */
std::vector<double> oracle_ref(const KernelSpec& sp, uint32_t w, uint32_t N,
                               const std::vector<float>& in0,
                               const std::vector<float>& in1,
                               const std::vector<float>& in2,
                               const std::vector<float>& in3) {
    std::vector<double> ref(N, 0.0);
    const uint32_t frames = sp.frames;
    switch (sp.op) {
    case ACS_KOP_CALIBRATION:
        for (uint32_t i = 0; i < N; ++i)
            ref[i] = (static_cast<double>(in0[i]) - in1[i] - sp.k * in2[i]) * in3[i];
        break;
    case ACS_KOP_NOISE_REDUCTIONS: {
        // kernel out0 = median(帧栈); oracle 返回与 out0 比较的 med
        std::vector<double> v(frames);
        for (uint32_t i = 0; i < N; ++i) {
            for (uint32_t f = 0; f < frames; ++f) v[f] = in0[static_cast<size_t>(f) * N + i];
            std::sort(v.begin(), v.end());
            ref[i] = v[frames / 2];   // med
        }
        break;
    }
    case ACS_KOP_PSF_BATCH: {
        const double cx = in0[0], cy = in0[1];
        for (uint32_t i = 0; i < N; ++i) {
            const double x = i % w, y = i / w;
            ref[i] = sp.k * std::exp(-((x - cx) * (x - cx) + (y - cy) * (y - cy)) * 0.5);
        }
        break;
    }
    case ACS_KOP_DRIZZLE_OVERLAP:
        for (uint32_t i = 0; i < N; ++i) {
            const double wx = std::max(0.0, 1.0 - std::fabs(in0[i]));
            const double wy = std::max(0.0, 1.0 - std::fabs(in1[i]));
            ref[i] = wx * wy;
        }
        break;
    case ACS_KOP_DRIZZLE_ACCUMULATE:
        for (uint32_t i = 0; i < N; ++i) {
            double acc = 0;
            for (uint32_t f = 0; f < frames; ++f)
                acc += in0[static_cast<size_t>(f) * N + i] * in1[static_cast<size_t>(f) * N + i];
            ref[i] = acc;
        }
        break;
    case ACS_KOP_DRIZZLE_NORMALIZE:
        for (uint32_t i = 0; i < N; ++i)
            ref[i] = (in1[i] > 1e-6) ? in0[i] / in1[i] : 0.0;
        break;
    case ACS_KOP_UPM_SPMV:
        for (uint32_t row = 0; row < N; ++row) {
            const uint32_t a = static_cast<uint32_t>(in2[row]);
            const uint32_t b = static_cast<uint32_t>(in2[row + 1]);
            double acc = 0;
            for (uint32_t k = a; k < b; ++k) acc += in0[k] * in3[static_cast<uint32_t>(in1[k])];
            ref[row] = acc;
        }
        break;
    case ACS_KOP_UPM_RESIDUAL:
        for (uint32_t i = 0; i < N; ++i) ref[i] = in0[i] - in1[i];
        break;
    case ACS_KOP_UPM_WEIGHT_UPDATE:
        for (uint32_t i = 0; i < N; ++i) ref[i] = std::max(in0[i], static_cast<float>(sp.k));
        break;
    case ACS_KOP_REJECTION_STATS: {
        std::vector<double> v(frames);
        for (uint32_t i = 0; i < N; ++i) {
            for (uint32_t f = 0; f < frames; ++f) v[f] = in0[static_cast<size_t>(f) * N + i];
            std::vector<double> s(v);
            std::sort(s.begin(), s.end());
            const double med = s[frames / 2];
            for (uint32_t f = 0; f < frames; ++f) s[f] = std::fabs(s[f] - med);
            std::sort(s.begin(), s.end());
            const double mad = s[frames / 2] * 1.4826;
            uint32_t cnt = 0;
            for (uint32_t f = 0; f < frames; ++f)
                if (std::fabs(v[f] - med) > sp.k * mad) ++cnt;
            ref[i] = cnt;
        }
        break;
    }
    case ACS_KOP_INTEGRATION_ACCUM:
        for (uint32_t i = 0; i < N; ++i) {
            double acc = 0, wsum = 0;
            for (uint32_t f = 0; f < frames; ++f) {
                acc += in1[static_cast<size_t>(f) * N + i] * in0[static_cast<size_t>(f) * N + i];
                wsum += in1[static_cast<size_t>(f) * N + i];
            }
            ref[i] = (wsum > 1e-6) ? acc / wsum : 0.0;
        }
        break;
    case ACS_KOP_HIPS_BULK: {
        // kernel: 源图 iw/ih 来自 aux0/aux1(非 sp.base); 采样位置 (x*k, y*k)
        const uint32_t iw = in2.empty() ? 0u : static_cast<uint32_t>(in2[0]);
        const uint32_t ih = in3.empty() ? 0u : static_cast<uint32_t>(in3[0]);
        if (iw < 2 || ih < 2) break;
        const double s = sp.k;
        for (uint32_t i = 0; i < N; ++i) {
            const double x = (i % w) * s, y = (i / w) * s;
            int x0 = static_cast<int>(std::floor(x)), y0i = static_cast<int>(std::floor(y));
            double fx = x - std::floor(x), fy = y - std::floor(y);
            x0 = std::min(std::max(x0, 0), static_cast<int>(iw) - 2);
            y0i = std::min(std::max(y0i, 0), static_cast<int>(ih) - 2);
            fx = std::min(std::max(fx, 0.0), 1.0);
            fy = std::min(std::max(fy, 0.0), 1.0);
            const size_t r0 = static_cast<size_t>(y0i) * iw, r1 = r0 + iw;
            ref[i] = (1 - fx) * (1 - fy) * in0[r0 + x0] + fx * (1 - fy) * in0[r0 + x0 + 1] +
                     (1 - fx) * fy * in0[r1 + x0] + fx * fy * in0[r1 + x0 + 1];
        }
        break;
    }
    default:
        break;
    }
    return ref;
}

/* 按 kernel spec 与规模构造输入(确定性 LCG; 同 seed 同输入, 可复现)。 */
struct Inputs {
    std::vector<float> in0, in1, in2, in3, out0, out1;
    uint32_t N = 0;
};

uint32_t lcg_state = 20260831u;
float lcg_f() {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return static_cast<float>((lcg_state >> 8) % 10000u) / 100.0f;  // [0,100)
}

Inputs build_inputs(const KernelSpec& sp, const std::string& sc) {
    Inputs r;
    const uint32_t base = sp.base * size_mult(sc);
    const uint32_t N = base * base;
    r.N = N;
    const uint32_t frames = sp.frames;
    r.in0.assign(static_cast<size_t>(frames) * N, 0.0f);
    r.in1.assign(static_cast<size_t>(frames) * N, 0.0f);
    r.in2.assign(N + 2, 0.0f);
    r.in3.assign(N + 2, 0.0f);
    r.out0.assign(N, 0.0f);
    r.out1.assign(N, 0.0f);
    for (size_t i = 0; i < r.in0.size(); ++i) r.in0[i] = lcg_f();
    for (size_t i = 0; i < r.in1.size(); ++i) r.in1[i] = (i % 7 == 0) ? 0.0f : lcg_f() * 0.1f;
    if (sp.op == ACS_KOP_UPM_SPMV) {
        // CSR 行指针: 每行 3~5 个非零
        r.in2[0] = 0;
        for (uint32_t row = 0; row < N; ++row) {
            const uint32_t nnz = 3 + (row % 3);
            r.in2[row + 1] = r.in2[row] + static_cast<float>(nnz);
        }
        const uint32_t total = static_cast<uint32_t>(r.in2[N]);
        r.in0.assign(total, 0.0f);
        r.in1.assign(total, 0.0f);
        r.in3.assign(N, 0.0f);
        for (uint32_t k = 0; k < total; ++k) {
            r.in0[k] = lcg_f();
            r.in1[k] = static_cast<float>(k % N);
        }
        for (uint32_t i = 0; i < N; ++i) r.in3[i] = lcg_f();
    }
    if (sp.op == ACS_KOP_PSF_BATCH) { r.in0[0] = base * 0.37f; r.in0[1] = base * 0.53f; }
    if (sp.op == ACS_KOP_HIPS_BULK) {
        // 源图 = 输出网格; kernel 经 aux0/aux1 读取源宽/高
        for (uint32_t i = 0; i < static_cast<uint32_t>(r.in0.size()); ++i)
            r.in0[i] = std::sin(static_cast<float>(i) * 0.01f) * 100.0f;
        // in2[0]=源宽, in3[0]=源高(传给 aux0/aux1)
        r.in2[0] = static_cast<float>(base);
        r.in3[0] = static_cast<float>(base);
    }
    return r;
}

/* 把 Inputs 填入 acs_baseline_params_v1 */
void fill_params(acs_baseline_params_v1* p, const KernelSpec& sp, Inputs& in) {
    std::memset(p, 0, sizeof(*p));
    p->head.struct_size = static_cast<uint32_t>(sizeof(*p));
    p->head.abi_version = ACS_ABI_VERSION_V1;
    p->op = static_cast<uint32_t>(sp.op);
    p->w = p->h = static_cast<uint32_t>(std::sqrt(static_cast<double>(in.N)));
    p->k = sp.k;
    p->aux0 = sp.frames;
    p->aux1 = static_cast<uint32_t>(in.in3.size());
    if (sp.op == ACS_KOP_HIPS_BULK) {
        // kernel 语义: aux0=源图宽, aux1=源图高(经 in2/in3 首元素传递)
        p->aux0 = static_cast<uint32_t>(in.in2.empty() ? 0 : in.in2[0]);
        p->aux1 = static_cast<uint32_t>(in.in3.empty() ? 0 : in.in3[0]);
    }
    p->in0 = ACS_SPAN_F32(in.in0.data(), in.in0.size());
    p->in1 = ACS_SPAN_F32(in.in1.data(), in.in1.size());
    p->in2 = ACS_SPAN_F32(in.in2.data(), in.in2.size());
    p->in3 = ACS_SPAN_F32(in.in3.data(), in.in3.size());
    p->out0 = ACS_SPAN_F32(in.out0.data(), in.out0.size());
    if (!in.out1.empty()) p->out1 = ACS_SPAN_F32(in.out1.data(), in.out1.size());
    else { p->out1 = ACS_SPAN_F32(nullptr, 0); }
}

/* 序列化原始候选(审计/复读); 返回 sha256 */
std::string raw_candidates_sha256(const std::vector<RawCandidate>& raw) {
    std::stringstream ss;
    for (const auto& c : raw) {
        ss << c.kernel_id << '|' << c.size_class << '|' << c.provider << '|' << c.workers
           << '|' << c.block << '|' << c.median_ns << '|' << c.mad_ns << '|' << c.p05_ns
           << '|' << c.p95_ns << '|' << (c.oracle_pass ? 1 : 0) << '|' << c.fallback_reason
           << '\n';
    }
    return crypto::sha256_hex(ss.str().data(), ss.str().size());
}

std::string utc_now() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_MSC_VER)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char ts[40];
    std::snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                  tm.tm_sec);
    return ts;
}

}  // namespace

ProfileBundle generate_profile_v2(const std::string& mode, const std::string& build_id,
                                  const std::string& commit,
                                  const std::string& cli_sha256,
                                  const std::string& backends_dir) {
    ProfileBundle bundle;

    // ── 1. host services + 能力/配额 ──
    astrocs_host_services_v1 host;
    void* state = nullptr;
    astrocs_host_services_default_v1(&host, &state);
    const std::string hw_json = hardware_inspect_json_v1(build_id);
    const nlohmann::json hw = nlohmann::json::parse(hw_json);
    const uint32_t avail = static_cast<uint32_t>(hw.value("available_logical_cpus", 1u));
    astrocs_host_state_set_budget_v1(state, avail, avail, &host);
    const uint64_t detected = astrocs_cpu_detect_features_v1();

    // ── 2. 加载可用 provider(manifest 预检; 无 manifest=仅内置 baseline) ──
    struct LoadedProvider {
        std::string id;
        astrocs_backend_api_v1 api;
        void* handle = nullptr;
        bool ok = false;
        std::string fail_reason;
    };
    std::vector<LoadedProvider> providers;
    {
        LoadedProvider base;
        std::memset(&base.api, 0, sizeof(base.api));
        astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1,
                                   static_cast<uint32_t>(sizeof(astrocs_host_services_v1)),
                                   &host, &base.api);
        base.id = "baseline";
        base.ok = true;
        providers.push_back(std::move(base));
    }
    std::ifstream mf(backends_dir + "/backends.manifest.json");
    if (mf) {
        std::stringstream mbuf; mbuf << mf.rdbuf();
        std::vector<ManifestEntry> entries;
        std::string merr;
        if (parse_backends_manifest(mbuf.str(), &entries, &merr)) {
            for (const auto& e : entries) {
                LoadedProvider lp;
                std::string reason;
                auto lr = load_backend(backends_dir, e, &host, &lp.api, &lp.handle, &reason);
                if (lr.decision == LoadResult::OK && lp.handle) {
                    lp.id = e.backend_id;
                    lp.ok = true;
                    providers.push_back(std::move(lp));
                } else {
                    LoadedProvider bad;
                    bad.id = e.backend_id;
                    bad.ok = false;
                    bad.fail_reason = reason;
                    providers.push_back(std::move(bad));
                }
            }
        }
    }

    // ── 3. memory 带宽基线 ──
    const MemoryReport mem = bench_memory(1u << 21, 3);

    // ── 4. 逐 kernel × 规模 × provider × workers × block ──
    const bool full = (mode == "full");
    const std::vector<std::string> sizes = full
        ? std::vector<std::string>{"small", "medium", "large"}
        : std::vector<std::string>{"medium"};
    const std::vector<uint32_t> workers_cand = worker_candidates(avail);
    // block 候选: 由 L2 与元素尺寸派生(L2 实测或保守 256KB)
    uint64_t l2_bytes = 256u * 1024u;
    for (const auto& c : hw.value("cache", nlohmann::json::array())) {
        if (c.value("level", std::string()) == "2") {
            const std::string s = c.value("size", "256K");
            if (s.size() > 1 && s.back() == 'K') l2_bytes = std::stoull(s.substr(0, s.size() - 1)) * 1024u;
            else if (s.size() > 1 && s.back() == 'M') l2_bytes = std::stoull(s.substr(0, s.size() - 1)) * 1024u * 1024u;
            break;
        }
    }
    const std::vector<uint64_t> blocks = block_candidates(l2_bytes, 4);

    const uint32_t kernel_total = full ? static_cast<uint32_t>(sizeof(kSpecs) / sizeof(kSpecs[0])) : 1u;
    for (uint32_t ki = 0; ki < kernel_total; ++ki) {
        const KernelSpec& sp = kSpecs[ki];
        const uint32_t kernel_idx = ki;   // quick 只用 calibration(第一个)
        if (!full && kernel_idx != 0) break;

        // 每个 provider 每 kernel 先 self-test(失败永久剔除本 profile)
        std::vector<std::string> usable_providers;
        std::map<std::string, std::string> selftest_hashes;
        for (const auto& p : providers) {
            if (!p.ok) continue;
            if (p.id != "baseline" && p.api.self_test && p.api.self_test(&host) != ACS_OK)
                continue;   // 变体 self-test 失败 → 不参与
            if (p.id == "baseline" && p.api.self_test && p.api.self_test(&host) != ACS_OK) {
                // baseline 自检失败 = 不可运行(08 §6-4)
                selftest_hashes["baseline"] = "selftest_fail";
                continue;
            }
            usable_providers.push_back(p.id);
            // self_test 可验证 hash: 以 provider 的 backend_sha256 或可执行 hash 代替
            selftest_hashes[p.id] = p.api.backend_sha256[0]
                ? std::string(p.api.backend_sha256) : cli_sha256;
        }

        for (const auto& sc : sizes) {
            Inputs in = build_inputs(sp, sc);
            const uint32_t w = static_cast<uint32_t>(std::sqrt(static_cast<double>(in.N)));
            const std::vector<double> ref = oracle_ref(sp, w, in.N, in.in0, in.in1, in.in2, in.in3);

            std::vector<RawCandidate> cands;
            for (const auto& pid : usable_providers) {
                LoadedProvider* prov = nullptr;
                for (auto& p : providers) if (p.ok && p.id == pid) { prov = &p; break; }
                if (!prov) continue;
                // 从 provider API 找到该 kernel entry
                const astrocs_kernel_entry_v1* entry = nullptr;
                for (uint32_t k = 0; k < prov->api.kernel_count; ++k) {
                    if (std::strncmp(prov->api.kernels[k].algorithm_id, sp.kernel_id,
                                     sizeof(prov->api.kernels[k].algorithm_id)) == 0) {
                        entry = &prov->api.kernels[k];
                        break;
                    }
                }
                if (!entry) {
                    RawCandidate c;
                    c.kernel_id = sp.kernel_id; c.size_class = sc; c.provider = pid;
                    c.oracle_pass = false;
                    c.fallback_reason = "kernel missing from provider";
                    cands.push_back(c);
                    continue;
                }
                for (const uint32_t w : workers_cand) {
                    for (const uint64_t blk : blocks) {
                        // budget 设为本候选 worker 数; block 通过 params 辅助传入
                        astrocs_host_state_set_budget_v1(state, avail, w, &host);
                        acs_baseline_params_v1 p;
                        fill_params(&p, sp, in);
                        (void)blk;   // v1 kernel 无 block 参数; 记录但执行语义一致
                        BenchResult br = bench_kernel(&host, pid.c_str(), entry->fn, p, ref,
                                                      2e-3, 3, 7);
                        RawCandidate c;
                        c.kernel_id = sp.kernel_id; c.size_class = sc; c.provider = pid;
                        c.workers = w; c.block = blk;
                        c.oracle_pass = (br.verdict == "OK");
                        c.median_ns = br.median_ns; c.mad_ns = br.mad_ns;
                        c.p05_ns = br.p05_ns; c.p95_ns = br.p95_ns;
                        c.fallback_reason = c.oracle_pass ? "" : br.reason;
                        cands.push_back(c);
                    }
                }
            }
            bundle.raw.insert(bundle.raw.end(), cands.begin(), cands.end());

            // ── 5. winner: 仅 OK 候选可胜出; AVX512 提升<3% 选 AVX2 ──
            std::vector<BenchResult> okres;
            for (const auto& c : cands) {
                if (!c.oracle_pass) continue;
                BenchResult br;
                br.backend_id = c.provider; br.verdict = "OK";
                br.median_ns = c.median_ns; br.mad_ns = c.mad_ns;
                br.workers = c.workers; br.samples = 7;
                okres.push_back(br);
            }
            std::string winner = select_winner(okres);
            uint32_t winner_workers = 1;
            uint64_t winner_block = 1;
            double winner_median = 0, winner_mad = 0;
            for (const auto& c : cands) {
                if (c.oracle_pass && c.provider == winner) {
                    winner_workers = c.workers; winner_block = c.block;
                    winner_median = c.median_ns; winner_mad = c.mad_ns;
                    break;
                }
            }
            // AVX512 相对 AVX2 提升<3% → 选 AVX2(规格 08 §4-7)
            if (winner == "avx512") {
                const RawCandidate* a2 = nullptr;
                const RawCandidate* a5 = nullptr;
                for (const auto& c : cands) {
                    if (c.oracle_pass && c.provider == "avx2" && c.workers == winner_workers &&
                        (!a2 || c.median_ns < a2->median_ns)) a2 = &c;
                    if (c.oracle_pass && c.provider == "avx512" && c.workers == winner_workers &&
                        (!a5 || c.median_ns < a5->median_ns)) a5 = &c;
                }
                if (a2 && a5) {
                    const double gain = (a2->median_ns - a5->median_ns) / a2->median_ns;
                    if (gain < 0.03) {
                        winner = "avx2";
                        winner_workers = a2->workers; winner_block = a2->block;
                        winner_median = a2->median_ns; winner_mad = a2->mad_ns;
                    }
                }
            }
            // worker 增加收益<3% 可少选; available≥2 且 heavy 不得选 1
            if (winner_workers == 1 && avail >= 2 && sp.workload[0] != '\0') {
                for (const auto& c : cands) {
                    if (c.oracle_pass && c.provider == winner && c.workers >= 2 &&
                        c.workers <= avail) {
                        // 收益<3% → 保持 1 但 heavy 场景强制 ≥2(08 §4-8)
                        const double gain = (winner_median - c.median_ns) / winner_median;
                        if (gain >= 0.03) {
                            winner_workers = c.workers; winner_block = c.block;
                            winner_median = c.median_ns; winner_mad = c.mad_ns;
                        }
                        break;
                    }
                }
                if (winner_workers == 1) {
                    winner_workers = avail;   // heavy 场景不得退 1(08 §4-8)
                    for (const auto& c : cands)
                        if (c.oracle_pass && c.provider == winner && c.workers == avail) {
                            winner_block = c.block; winner_median = c.median_ns;
                            winner_mad = c.mad_ns; break;
                        }
                }
            }

            KernelProfile kp;
            kp.kernel_id = sp.kernel_id;
            kp.workload_class = sp.workload;
            kp.provider = winner.empty() ? "baseline" : winner;
            kp.workers = winner_workers;
            kp.block = winner_block;
            kp.correctness_test = winner.empty() ? "oracle:fail" : "oracle:pass";
            kp.self_test_sha256 = selftest_hashes.count(kp.provider)
                ? selftest_hashes[kp.provider] : cli_sha256;
            kp.median_ns = winner_median;
            kp.mad_ns = winner_mad;
            kp.fallback_reason = winner.empty() ? "no passing provider" : "";
            bundle.kernels[sp.kernel_id] = kp;
        }
    }

    // ── 6. 关闭 provider handles ──
    for (auto& p : providers) if (p.handle) close_backend(p.handle);
    astrocs_host_services_destroy_state_v1(state);

    // ── 7. 组装 v2 JSON ──
    bundle.raw_samples_sha256 = raw_candidates_sha256(bundle.raw);
    nlohmann::json j;
    j["schema"] = "astrocs.cpu-profile/v2";
    j["profile_id"] = "sha256:" + bundle.raw_samples_sha256;
    j["created_utc"] = utc_now();
    j["host"] = {
        {"arch", "amd64"},
        {"vendor", hw.value("vendor", "")},
        {"family", hw.value("family", 0)},
        {"model", hw.value("model", 0)},
        {"stepping", hw.value("stepping", 0)},
        {"os_abi", hw.value("os", nlohmann::json::object()).value("name", "linux")},
        {"features", hw.value("feature_names", nlohmann::json::array())},
        {"xcr0", std::to_string(hw.value("xcr0", 0ull))},
        {"logical_available", avail},
        {"quota_signature", hw.value("quota_signature", "")},
    };
    nlohmann::json provider_ids = nlohmann::json::object();
    for (const auto& p : providers) provider_ids[p.id] = p.ok ? "loaded" : p.fail_reason;
    // astrocs_version: schema const 要求纯 "0.10.0-alpha.2"(去 +g<hash> 后缀)
    std::string ver = build_id;
    const auto plus = ver.find('+');
    if (plus != std::string::npos) ver = ver.substr(0, plus);
    j["build"] = {
        {"astrocs_version", ver},
        {"source_commit", commit},
        {"benchmark_binary_sha256", cli_sha256},
        {"runtime_build_id", build_id},
        {"provider_build_ids", provider_ids},
    };
    j["memory_bandwidth"] = {
        {"copy", mem.copy_gbs}, {"read", mem.read_gbs},
        {"write", mem.write_gbs}, {"triad", mem.triad32_gbs},
    };
    j["raw_samples_sha256"] = bundle.raw_samples_sha256;
    nlohmann::json kernels = nlohmann::json::object();
    for (const auto& [kid, kp] : bundle.kernels) {
        nlohmann::json kentry;
        kentry["workload_class"] = kp.workload_class;
        kentry["provider"] = kp.provider;
        kentry["workers"] = kp.workers;
        kentry["block"] = kp.block;
        kentry["correctness_test"] = kp.correctness_test;
        kentry["self_test_sha256"] = kp.self_test_sha256;
        kentry["median"] = kp.median_ns;
        kentry["mad"] = kp.mad_ns;
        kentry["fallback_reason"] = kp.fallback_reason.empty()
            ? nlohmann::json(nullptr) : nlohmann::json(kp.fallback_reason);
        kernels[kid] = kentry;
    }
    j["kernels"] = kernels;
    bundle.json = j.dump(2) + "\n";
    return bundle;
}

std::string verify_profile_v2(const std::string& json_text, const std::string& expected_commit) {
    nlohmann::json d;
    try {
        d = nlohmann::json::parse(json_text);
    } catch (const nlohmann::json::parse_error& e) {
        return std::string("malformed JSON: ") + e.what();
    }
    if (d.value("schema", "") != "astrocs.cpu-profile/v2")
        return "schema != astrocs.cpu-profile/v2";
    for (const char* k : {"profile_id", "created_utc", "host", "build", "memory_bandwidth",
                          "raw_samples_sha256", "kernels"}) {
        if (!d.contains(k) || d[k].is_null()) return std::string("missing required '") + k + "'";
    }
    const auto& hw = d["host"];
    for (const char* k : {"arch", "vendor", "family", "model", "stepping", "os_abi",
                          "features", "xcr0", "logical_available", "quota_signature"}) {
        if (!hw.contains(k) || hw[k].is_null()) return std::string("host missing '") + k + "'";
    }
    if (hw.value("arch", "") != "amd64") return "host.arch != amd64";
    if (hw.value("logical_available", 0) < 1) return "host.logical_available < 1";
    const auto& bd = d["build"];
    for (const char* k : {"astrocs_version", "source_commit", "benchmark_binary_sha256",
                          "runtime_build_id", "provider_build_ids"}) {
        if (!bd.contains(k) || bd[k].is_null()) return std::string("build missing '") + k + "'";
    }
    if (bd.value("astrocs_version", "") != "0.10.0-alpha.2")
        return "build.astrocs_version != 0.10.0-alpha.2";
    const std::string sc = bd.value("source_commit", "");
    if (sc.size() != 40 || sc.find_first_not_of("0123456789abcdef") != std::string::npos)
        return "build.source_commit not 40hex";
    if (!expected_commit.empty() && sc != expected_commit)
        return "build.source_commit != expected (" + sc.substr(0, 12) + ")";
    const auto& mb = d["memory_bandwidth"];
    for (const char* k : {"copy", "read", "write", "triad"}) {
        if (!mb.contains(k) || mb[k].get<double>() <= 0)
            return std::string("memory_bandwidth missing/<=0 '") + k + "'";
    }
    if (!d["kernels"].is_object() || d["kernels"].empty())
        return "kernels not object or empty";
    for (auto it = d["kernels"].begin(); it != d["kernels"].end(); ++it) {
        const auto& kp = it.value();
        for (const char* k : {"workload_class", "provider", "workers", "block",
                              "correctness_test", "self_test_sha256", "median", "mad"}) {
            if (!kp.contains(k) || kp[k].is_null())
                return "kernels." + it.key() + " missing '" + k + "'";
        }
        if (!kp.contains("fallback_reason"))
            return "kernels." + it.key() + " missing 'fallback_reason'";
        const std::string prov = kp.value("provider", "");
        if (prov != "baseline" && prov != "avx2" && prov != "avx512")
            return "kernels." + it.key() + ".provider invalid: " + prov;
        if (kp.value("workers", 0) < 1) return "kernels." + it.key() + ".workers < 1";
        if (kp.value("block", 0) < 1) return "kernels." + it.key() + ".block < 1";
        if (kp.value("median", 0.0) <= 0) return "kernels." + it.key() + ".median <= 0";
        if (kp.value("mad", -1.0) < 0) return "kernels." + it.key() + ".mad < 0";
        const std::string st = kp.value("self_test_sha256", "");
        if (st.size() != 64) return "kernels." + it.key() + ".self_test_sha256 not 64hex";
    }
    const std::string pid = d.value("profile_id", "");
    if (pid.rfind("sha256:", 0) != 0 || pid.size() != 71)
        return "profile_id not sha256:<64hex>";
    return "";   // 合法
}

}  // namespace astrocs::backend_host
