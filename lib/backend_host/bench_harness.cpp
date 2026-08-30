// lib/backend_host/bench_harness.cpp — harness 流程实现 (06 §1/§4) — BENCH-002
#include "bench_harness.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>

#include "sha256.h"

namespace astrocs::backend_host {

namespace {

using Clock = std::chrono::steady_clock;

double percentile_sorted(std::vector<double>& sorted, double q) {  // q∈[0,1]
    if (sorted.empty()) return 0;
    const auto idx = static_cast<size_t>(q * (sorted.size() - 1) + 0.5);
    return sorted[std::min(idx, sorted.size() - 1)];
}

}  // namespace

BenchResult bench_kernel(const astrocs_host_services_v1* host,
                         const char* backend_id,
                         acs_status (*fn)(
                             const astrocs_host_services_v1*, const void*, uint32_t,
                             const void*, void*),
                         const acs_baseline_params_v1& params,
                         const std::vector<double>& expected_ref,
                         double tol_rel, int warmup, int samples) {
    BenchResult r;
    r.backend_id = backend_id;
    if (!fn) { r.verdict = "ERROR"; r.reason = "null fn"; return r; }

    // ── 1. 正确性筛选(独立 scalar Oracle; 失败=禁用, 不计时) ──
    std::vector<float> out(params.out0.count, 0.0f);
    acs_baseline_params_v1 p = params;
    p.out0 = ACS_SPAN_F32(out.data(), out.size());
    const acs_status rc = fn(host, &p, sizeof(p), nullptr, nullptr);
    if (rc != ACS_OK) {
        r.verdict = (rc == ACS_ERR_CANCELLED) ? "ERROR" : "ORACLE_FAIL";
        r.reason = "kernel rc=" + std::to_string(static_cast<int>(rc));
        return r;
    }
    const uint32_t n = p.w * p.h;
    for (uint32_t i = 0; i < n; ++i) {
        const double got = out[i], ref = expected_ref[i];
        if (!std::isfinite(got) || std::fabs(got - ref) > tol_rel * std::max(1.0, std::fabs(ref))) {
            r.verdict = "ORACLE_FAIL";
            r.reason = "mismatch at " + std::to_string(i) + ": got=" + std::to_string(got) +
                       " ref=" + std::to_string(ref);
            return r;   // 禁用: 不预热、不计时、不进入候选
        }
    }
    {   // correctness hash(06 §4)
        crypto::Sha256 h;
        h.update(out.data(), static_cast<size_t>(n) * sizeof(float));
        r.correctness_hash = h.final_hex();
    }

    // ── 2. 预热(不计时) ──
    for (int i = 0; i < warmup; ++i) fn(host, &p, sizeof(p), nullptr, nullptr);

    // ── 3. 计时(单调高分辨率钟; samples≥7) ──
    std::vector<double> samples_ns;
    samples_ns.reserve(static_cast<size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        const auto t0 = Clock::now();
        fn(host, &p, sizeof(p), nullptr, nullptr);
        const auto t1 = Clock::now();
        samples_ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
    }

    // ── 4. 稳健统计 ──
    std::sort(samples_ns.begin(), samples_ns.end());
    r.samples = samples;
    r.median_ns = percentile_sorted(samples_ns, 0.5);
    double mad_acc = 0;
    for (double x : samples_ns) mad_acc += std::fabs(x - r.median_ns);
    r.mad_ns = mad_acc / samples_ns.size();
    r.p05_ns = percentile_sorted(samples_ns, 0.05);
    r.p95_ns = percentile_sorted(samples_ns, 0.95);
    r.workers = p.workers_used;
    r.verdict = "OK";
    return r;
}

std::string select_winner(const std::vector<BenchResult>& results) {
    const BenchResult* best = nullptr;
    for (const auto& r : results) {
        if (r.verdict != "OK") continue;               // 错误路径结构性不可胜出(06 §1)
        if (!best || r.median_ns < best->median_ns) best = &r;
    }
    return best ? best->backend_id : std::string();
}

/* ── 候选生成与内存基线(06 §3) ── */

std::vector<uint32_t> worker_candidates(uint32_t available_cpus) {
    std::vector<uint32_t> c = {1u, (available_cpus + 1) / 2u, available_cpus};
    std::sort(c.begin(), c.end());
    c.erase(std::unique(c.begin(), c.end()), c.end());
    return c;
}

std::vector<uint64_t> block_candidates(uint64_t l2_bytes, uint64_t elt_size) {
    const uint64_t base = std::min<uint64_t>(
        std::max<uint64_t>(l2_bytes / (elt_size * 8), 1024u), 1ull << 20);
    std::vector<uint64_t> c;
    for (uint64_t b = base / 16; b <= base * 4; b *= 4) c.push_back(std::max<uint64_t>(b, 1));
    return c;
}

uint64_t current_rss_bytes() {
#if defined(_WIN32)
    return 0;
#else
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("VmRSS:", 0) == 0) {
            long kb = 0;
            if (std::sscanf(line.c_str(), "VmRSS: %ld kB", &kb) == 1)
                return static_cast<uint64_t>(kb) * 1024ull;
        }
    return 0;
#endif
}

MemoryReport bench_memory(uint64_t n, int reps) {
    MemoryReport rep;
    using Clock = std::chrono::steady_clock;
    auto gbs = [](uint64_t bytes, double ns) {
        return ns > 0 ? static_cast<double>(bytes) / (ns / 1e9) / 1e9 : 0.0;
    };
    auto med_ns = [](std::vector<double>& v) {
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    const uint64_t rss0 = current_rss_bytes();

    std::vector<float> a(n), b(n), c(n);
    std::vector<double> a64(n), b64(n), c64(n);
    for (uint64_t i = 0; i < n; ++i) {
        a[i] = static_cast<float>(i % 1024);
        b[i] = static_cast<float>((i * 7) % 1024);
        c[i] = 1.5f;
        a64[i] = a[i];
        b64[i] = b[i];
        c64[i] = 1.5;
    }

    auto time_read = [&]() {                       // read: Σa
        std::vector<double> t;
        double acc = 0;
        for (int r = 0; r < reps; ++r) {
            const auto t0 = Clock::now();
            for (uint64_t i = 0; i < n; ++i) acc += a[i];
            const auto t1 = Clock::now();
            t.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
        }
        return med_ns(t) + (acc == 1234.5678 ? 1.0 : 0.0);   // 防 DCE(值恒真路径不触发)
    };
    auto time_write = [&](std::vector<float>& dst) {          // write: 填充
        std::vector<double> t;
        for (int r = 0; r < reps; ++r) {
            const auto t0 = Clock::now();
            for (uint64_t i = 0; i < n; ++i) dst[i] = 1.25f;
            const auto t1 = Clock::now();
            t.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
        }
        return med_ns(t);
    };
    auto time_copy = [&](std::vector<float>& dst, const std::vector<float>& src) {  // copy
        std::vector<double> t;
        for (int r = 0; r < reps; ++r) {
            const auto t0 = Clock::now();
            for (uint64_t i = 0; i < n; ++i) dst[i] = src[i];
            const auto t1 = Clock::now();
            t.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
        }
        return med_ns(t);
    };
    auto time_triad = [&](auto& A, const auto& B, const auto& C, double q, uint64_t bytes_moved) {
        std::vector<double> t;
        for (int r = 0; r < reps; ++r) {
            const auto t0 = Clock::now();
            for (uint64_t i = 0; i < n; ++i) A[i] = B[i] + q * C[i];
            const auto t1 = Clock::now();
            t.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
        }
        return gbs(bytes_moved, med_ns(t));
    };

    rep.read_gbs = gbs(n * 4, time_read());
    rep.write_gbs = gbs(n * 4, time_write(c));
    rep.copy_gbs = gbs(n * 4 * 2, time_copy(b, a));
    rep.triad32_gbs = time_triad(a, b, c, 3.0, n * 4 * 3);
    rep.triad64_gbs = time_triad(a64, b64, c64, 3.0, n * 8 * 3);
    rep.rss_delta_bytes = current_rss_bytes() > rss0 ? current_rss_bytes() - rss0 : 0;
    return rep;
}

std::string select_with_noise_margin(const std::vector<BenchResult>& results,
                                     const std::string& conservative_backend_id,
                                     double margin_rel) {
    const BenchResult* best = nullptr;
    for (const auto& r : results) {
        if (r.verdict != "OK") continue;
        if (!best || r.median_ns < best->median_ns) best = &r;
    }
    if (!best) return {};
    if (best->backend_id == conservative_backend_id) return conservative_backend_id;
    // 保守路径存在且收益不足裕量 → 更保守(06 §4)
    for (const auto& r : results)
        if (r.backend_id == conservative_backend_id && r.verdict == "OK") {
            const double gain = (r.median_ns - best->median_ns) / r.median_ns;
            if (gain < margin_rel) return conservative_backend_id;
        }
    return best->backend_id;
}

NoProfilePolicy no_profile_policy(uint32_t available_cpus) {
    NoProfilePolicy p;
    p.backend_id = "baseline";
    p.workers = available_cpus > 0 ? available_cpus : 1;   // ≥1; 可用≥2 时不得退 1(06 §6)
    p.reason = "no_valid_profile";
    return p;
}

}  // namespace astrocs::backend_host
