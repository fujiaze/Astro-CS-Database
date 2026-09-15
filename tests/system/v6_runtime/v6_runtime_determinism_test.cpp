// tests/system/v6_runtime/v6_runtime_determinism_test.cpp
// RUNTIME-CI-001 (Wave 9): 并行确定性回归锁（宪章 §10.4/§10.6 + FZ-RUNTIME-DETERMINISM）。
//
// 契约: 同一输入在不同 worker 数 / 分块调度（blocked / round-robin / reversed）下，
// 科学载荷产物必须逐字节一致。判定 = 规范归约（每 work unit 独立部分和，按 unit
// 索引以固定二叉结合序合并），调度只决定"谁先算"，绝不改变结合序。
//
// 负向（negative）: 以真实多线程 + 共享累积（结合序 = 调度完成序）实现同一归约；
// 该实现违反确定性契约，正/负向产物必须出现逐字节差异 → 门禁应判红。
//
// 用法: v6_runtime_determinism_test <positive|negative> [budget] [units]
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "v6_runtime_contract.h"

using namespace astrocs::v6runtime;

struct Payload {
    uint64_t n_terms = 0;
    uint64_t n_units = 0;
    std::vector<double> unit_partials;  // 按 unit 索引
    double combined = 0.0;
};

static std::vector<uint8_t> serialize(const Payload& p) {
    std::vector<uint8_t> out;
    out.resize(sizeof(uint64_t) * 2 + sizeof(double) * p.unit_partials.size() +
               sizeof(double));
    size_t off = 0;
    std::memcpy(out.data() + off, &p.n_terms, sizeof(uint64_t)); off += sizeof(uint64_t);
    std::memcpy(out.data() + off, &p.n_units, sizeof(uint64_t)); off += sizeof(uint64_t);
    for (double v : p.unit_partials) { std::memcpy(out.data() + off, &v, sizeof(double)); off += sizeof(double); }
    std::memcpy(out.data() + off, &p.combined, sizeof(double));
    return out;
}

// 规范数据: 混合幅度的确定性伪随机项（保证浮点加法非结合性可被观测）。
static std::vector<double> make_terms(uint64_t n, uint64_t seed) {
    std::vector<double> v(n);
    uint64_t s = seed;
    auto next = [&]() {
        s += 0x9E3779B97F4A7C15ULL;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    };
    for (uint64_t i = 0; i < n; ++i) {
        const double mant = static_cast<double>(next() % 1000000ULL) + 1.0;
        const double scale = static_cast<double>(i % 17) + 1.0;   // 幅度分层
        const double sign = (next() & 1ULL) ? 1.0 : -1.0;
        v[i] = sign * mant / scale;
    }
    return v;
}

// 固定二叉结合序（按 unit 索引），与调度无关。
static double combine_canonical(const std::vector<double>& partials) {
    std::vector<double> level = partials;
    while (level.size() > 1) {
        std::vector<double> nextl;
        for (size_t i = 0; i + 1 < level.size(); i += 2) nextl.push_back(level[i] + level[i + 1]);
        if (level.size() % 2 == 1) nextl.push_back(level.back());
        level.swap(nextl);
    }
    return level.empty() ? 0.0 : level[0];
}

// 规范并行归约: unit 切分固定; 调度策略只影响 unit 的执行顺序, 不影响结果。
enum class Sched { kBlocked, kRoundRobin, kReversed };

static Payload compute_canonical(const std::vector<double>& terms, uint64_t n_units,
                                 uint32_t workers, Sched sched) {
    Payload p;
    p.n_terms = terms.size();
    p.n_units = n_units;
    p.unit_partials.assign(n_units, 0.0);
    const uint64_t per = (terms.size() + n_units - 1) / n_units;
    // 调度序（仅决定执行顺序；结果按 unit 索引回收）
    std::vector<uint64_t> order;
    if (sched == Sched::kReversed) {
        for (uint64_t i = n_units; i-- > 0;) order.push_back(i);
    } else if (sched == Sched::kRoundRobin) {
        for (uint32_t r = 0; r < workers; ++r)
            for (uint64_t u = r; u < n_units; u += workers) order.push_back(u);
    } else {
        for (uint64_t u = 0; u < n_units; ++u) order.push_back(u);
    }
    std::atomic<size_t> next{0};
    auto worker_fn = [&]() {
        for (;;) {
            const size_t idx = next.fetch_add(1, std::memory_order_relaxed);
            if (idx >= order.size()) break;
            const uint64_t u = order[idx];
            const uint64_t b = u * per;
            const uint64_t e = (b + per < terms.size()) ? (b + per) : terms.size();
            double acc = 0.0;
            for (uint64_t i = b; i < e; ++i) acc += terms[i];   // unit 内序固定
            p.unit_partials[u] = acc;                            // 按 unit 索引回收
        }
    };
    // 预算来自进程唯一预算源（§10.4）: worker 数 = 租约授予数, 非硬编码。
    ProcessBudgetRegistry& R = ProcessBudgetRegistry::instance();
    R.reset_for_test();
    (void)R.register_source("v6_runtime_determinism_test", workers);
    std::string err;
    const uint32_t leased = R.request_lease("reduction", workers, &err);
    const uint32_t nthreads = leased > 0 ? leased : 1;
    std::vector<std::thread> pool;
    for (uint32_t t = 0; t < nthreads; ++t) pool.emplace_back(worker_fn);
    for (auto& th : pool) th.join();
    R.release_lease(leased);
    p.combined = combine_canonical(p.unit_partials);
    return p;
}

// 违规实现: 共享原子累积, 结合序 = 调度完成序（多线程下逐字节不可复现）。
static Payload compute_order_dependent(const std::vector<double>& terms,
                                       uint64_t n_units, uint32_t workers) {
    Payload p;
    p.n_terms = terms.size();
    p.n_units = n_units;
    p.unit_partials.assign(n_units, 0.0);
    const uint64_t per = (terms.size() + n_units - 1) / n_units;
    std::atomic<uint64_t> next{0};
    double shared = 0.0;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto worker_fn = [&]() {
        for (;;) {
            const uint64_t u = next.fetch_add(1, std::memory_order_relaxed);
            if (u >= n_units) break;
            const uint64_t b = u * per;
            const uint64_t e = (b + per < terms.size()) ? (b + per) : terms.size();
            double acc = 0.0;
            for (uint64_t i = b; i < e; ++i) acc += terms[i];
            p.unit_partials[u] = acc;
            // 非同步累积: 结合序由线程竞争决定（确定性被破坏）
            shared += acc;
        }
    };
    std::vector<std::thread> pool;
    for (uint32_t t = 0; t < (workers > 0 ? workers : 1); ++t) pool.emplace_back(worker_fn);
    for (auto& th : pool) th.join();
    p.combined = shared;
    return p;
}

static bool fail(const std::string& msg) {
    std::fprintf(stderr, "[FAIL] %s\n", msg.c_str());
    return false;
}

int main(int argc, char** argv) {
    const std::string mode = (argc > 1) ? argv[1] : "positive";
    const uint64_t n_terms = (argc > 3) ? std::strtoull(argv[3], nullptr, 10) : 200000ULL;
    const uint64_t n_units = 64;
    const std::vector<double> terms = make_terms(n_terms, 20260915ULL);

    if (mode == "positive") {
        const Payload ref = compute_canonical(terms, n_units, 1, Sched::kBlocked);
        const std::vector<uint8_t> ref_bytes = serialize(ref);
        struct Case { uint32_t workers; Sched sched; const char* name; };
        const Case cases[] = {
            {1, Sched::kBlocked,    "w1-blocked"},
            {2, Sched::kBlocked,    "w2-blocked"},
            {4, Sched::kRoundRobin, "w4-round-robin"},
            {8, Sched::kReversed,   "w8-reversed"},
            {16, Sched::kRoundRobin,"w16-round-robin"},
        };
        bool ok = true;
        for (const auto& c : cases) {
            const Payload p = compute_canonical(terms, n_units, c.workers, c.sched);
            const std::vector<uint8_t> b = serialize(p);
            if (b != ref_bytes) { ok = fail(std::string("byte mismatch at ") + c.name); }
            else std::fprintf(stderr, "[ok] %s bytes=%zu\n", c.name, b.size());
        }
        // 同预算重复运行亦须逐字节一致
        const Payload rep = compute_canonical(terms, n_units, 8, Sched::kBlocked);
        if (serialize(rep) != ref_bytes) ok = fail("repeat run with budget=8 differs");
        if (ok) std::fprintf(stderr, "DETERMINISM_POSITIVE_PASS units=%llu terms=%llu\n",
                             (unsigned long long)n_units, (unsigned long long)n_terms);
        return ok ? 0 : 1;
    }
    if (mode == "negative") {
        const Payload ref = compute_canonical(terms, n_units, 1, Sched::kBlocked);
        const std::vector<uint8_t> ref_bytes = serialize(ref);
        int detected = 0, trials = 0;
        for (int rep = 0; rep < 8; ++rep) {
            const Payload bad = compute_order_dependent(terms, n_units, 8);
            ++trials;
            if (serialize(bad) != ref_bytes) ++detected;
        }
        if (detected == 0)
            return fail("order-dependent reduction NOT detected (determinism gate blind)") ? 0 : 1;
        std::fprintf(stderr, "DETERMINISM_NEGATIVE_PASS detected=%d/%d\n", detected, trials);
        return 0;
    }
    std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
    return 2;
}
