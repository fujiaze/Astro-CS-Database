// lib/backend_host/bench_harness.cpp — harness 流程实现 (06 §1/§4) — BENCH-002
#include "bench_harness.h"

#include <algorithm>
#include <chrono>
#include <cmath>

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
    p.out0 = {out.data(), out.size()};
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

}  // namespace astrocs::backend_host
