// lib/acr/tests/classic/classic_common.hpp — Phase H 经典实验公共框架
// 设计（ 09_PHASE_H_CLASSIC_EXPERIMENTS_SPEC.md）：
// 1. 固定 seed 0xA57C5AC20260802（确定性、可复现）
// 2. LCG / xorshift64 确定性 PRNG（无 std::random 依赖，跨平台一致）
// 3. FP32/FP64 容差：abs(a-b) <= atol + rtol * max(|a|,|b|)
// 4. JSON 结果（实验 ID/case/seed/backend/device/precision/size/correct/误差/时延/status）
// 5. 公共头不暴露第三方类型；GoogleTest 仅在 .cpp 用
// 6. ResultSink 全局收集器：TEST 与 classic_runner 共用同一实验逻辑
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace astro::compute::classic {

// ===== 固定 seed =====
constexpr std::uint64_t FIXED_SEED = 0xA57C5AC20260802ULL;

// ===== 确定性 PRNG：LCG（Numerical Recipes 常量）=====
// 线性同余：x_{n+1} = 6364136223846793005 * x_n + 1442695040888963407 (mod 2^64)
class LCG {
public:
    explicit LCG(std::uint64_t seed = FIXED_SEED) noexcept : state_(seed) {}
    std::uint64_t next() noexcept {
        state_ = 6364136223846793005ULL * state_ + 1442695040888963407ULL;
        return state_;
    }
    // 返回 [0, 1) 双精度
    double next_double() noexcept {
        return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
    }
    // 返回 [lo, hi] 整数（闭区间）
    std::uint64_t next_range(std::uint64_t lo, std::uint64_t hi) noexcept {
        if (hi <= lo) return lo;
        return lo + (next() % (hi - lo + 1));
    }
    void reseed(std::uint64_t s) noexcept { state_ = s; }
private:
    std::uint64_t state_;
};

// ===== 确定性 PRNG：xorshift64 =====
class Xorshift64 {
public:
    explicit Xorshift64(std::uint64_t seed = FIXED_SEED) noexcept : state_(seed ? seed : 1) {}
    std::uint64_t next() noexcept {
        std::uint64_t x = state_;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        state_ = x;
        return x;
    }
    double next_double() noexcept {
        return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
    }
    void reseed(std::uint64_t s) noexcept { state_ = s ? s : 1; }
private:
    std::uint64_t state_;
};

// ===== 容差比较 =====
// FP32 通用：abs(a-b) <= 1e-5 + 5e-5 * max(|a|,|b|)
inline bool fp32_close(float a, float b) noexcept {
    float aa = std::fabs(a), ab = std::fabs(b);
    float max_abs = aa > ab ? aa : ab;
    float diff = std::fabs(a - b);
    return diff <= 1e-5f + 5e-5f * max_abs;
}
// FP64 通用：abs(a-b) <= 1e-12 + 1e-11 * max(|a|,|b|)
inline bool fp64_close(double a, double b) noexcept {
    double aa = std::fabs(a), ab = std::fabs(b);
    double max_abs = aa > ab ? aa : ab;
    double diff = std::fabs(a - b);
    return diff <= 1e-12 + 1e-11 * max_abs;
}

// ===== 误差统计 =====
struct ErrorStats {
    double max_abs{0.0};
    double max_rel{0.0};
    double rmse{0.0};
};

// 计算 max_abs / max_rel / rmse（数组 vs reference）
template<class T>
ErrorStats compute_errors(const T* actual, const T* reference, std::size_t n) {
    ErrorStats s;
    double sum_sq = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        double a = static_cast<double>(actual[i]);
        double r = static_cast<double>(reference[i]);
        double diff = std::fabs(a - r);
        if (diff > s.max_abs) s.max_abs = diff;
        double max_abs = std::fabs(a) > std::fabs(r) ? std::fabs(a) : std::fabs(r);
        if (max_abs > 1e-30) {
            double rel = diff / max_abs;
            if (rel > s.max_rel) s.max_rel = rel;
        }
        sum_sq += (a - r) * (a - r);
    }
    s.rmse = n > 0 ? std::sqrt(sum_sq / static_cast<double>(n)) : 0.0;
    return s;
}

// 整数实验：exact 比较，max_abs = 不匹配数，max_rel = 不匹配比例
inline ErrorStats compute_errors_int(const int* actual, const int* reference, std::size_t n) {
    ErrorStats s;
    std::size_t mismatch = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (actual[i] != reference[i]) {
            ++mismatch;
            double diff = std::fabs(static_cast<double>(actual[i] - reference[i]));
            if (diff > s.max_abs) s.max_abs = diff;
        }
    }
    s.max_rel = n > 0 ? static_cast<double>(mismatch) / static_cast<double>(n) : 0.0;
    s.rmse = 0.0;
    return s;
}

// ===== 计时 =====
// 多次执行 kernel，返回 median_ms / p95_ms
struct TimingStats {
    double median_ms{0.0};
    double p95_ms{0.0};
};

template<class KernelFn>
TimingStats measure_timing(KernelFn&& fn, std::uint32_t rounds = 11) {
    TimingStats t;
    if (rounds == 0) return t;
    std::vector<double> times_ms;
    times_ms.reserve(rounds);
    for (std::uint32_t r = 0; r < rounds; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        fn();
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times_ms.push_back(ms);
    }
    std::sort(times_ms.begin(), times_ms.end());
    // median
    std::size_t mid = times_ms.size() / 2;
    t.median_ms = times_ms[mid];
    // p95：取第 95 百分位（线性插值）
    if (times_ms.size() == 1) {
        t.p95_ms = times_ms[0];
    } else {
        double idx = 0.95 * (times_ms.size() - 1);
        std::size_t lo = static_cast<std::size_t>(idx);
        std::size_t hi = (lo + 1 < times_ms.size()) ? lo + 1 : lo;
        double frac = idx - static_cast<double>(lo);
        t.p95_ms = times_ms[lo] * (1.0 - frac) + times_ms[hi] * frac;
    }
    return t;
}

// ===== Case 结果 =====
struct CaseResult {
    std::string experiment_id;   // "E01"
    std::string case_id;         // "copy_1M"
    std::string seed;            // "0xA57C5AC20260802"
    std::string backend;         // "cpu"
    std::string device;          // "cpu"
    std::string precision;       // "fp32" / "fp64" / "integer"
    std::size_t  size{0};        // 元素数或像素数
    bool         correct{false};
    double       max_abs{0.0};
    double       max_rel{0.0};
    double       rmse{0.0};
    double       median_ms{0.0};
    double       p95_ms{0.0};
    std::string  status;         // "PASS" / "FAIL" / "SKIPPED"
    std::string  reason;         // 失败/跳过原因
};

// JSON 转义（极简：处理双引号/反斜杠/控制字符）
inline std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// 序列化 CaseResult 为 JSON 对象（单行，无换行）
inline std::string to_json(const CaseResult& r) {
    std::ostringstream os;
    os << "{";
    os << "\"experiment_id\":\"" << json_escape(r.experiment_id) << "\",";
    os << "\"case_id\":\"" << json_escape(r.case_id) << "\",";
    os << "\"seed\":\"" << json_escape(r.seed) << "\",";
    os << "\"backend\":\"" << json_escape(r.backend) << "\",";
    os << "\"device\":\"" << json_escape(r.device) << "\",";
    os << "\"precision\":\"" << json_escape(r.precision) << "\",";
    os << "\"size\":" << r.size << ",";
    os << "\"correct\":" << (r.correct ? "true" : "false") << ",";
    os << "\"max_abs\":" << r.max_abs << ",";
    os << "\"max_rel\":" << r.max_rel << ",";
    os << "\"rmse\":" << r.rmse << ",";
    os << "\"median_ms\":" << r.median_ms << ",";
    os << "\"p95_ms\":" << r.p95_ms << ",";
    os << "\"status\":\"" << json_escape(r.status) << "\",";
    os << "\"reason\":\"" << json_escape(r.reason) << "\"";
    os << "}";
    return os.str();
}

// ===== 全局结果收集器（线程安全）=====
class ResultSink {
public:
    static ResultSink& instance() {
        static ResultSink s;
        return s;
    }
    void push(CaseResult r) {
        std::lock_guard<std::mutex> lk(mu_);
        results_.push_back(std::move(r));
    }
    std::vector<CaseResult> all() const {
        std::lock_guard<std::mutex> lk(mu_);
        return results_;
    }
    void clear() {
        std::lock_guard<std::mutex> lk(mu_);
        results_.clear();
    }
    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mu_);
        return results_.size();
    }
private:
    ResultSink() = default;
    mutable std::mutex mu_;
    std::vector<CaseResult> results_;
};

// ===== 辅助：构造标准 CaseResult =====
inline CaseResult make_result(const std::string& exp_id, const std::string& case_id,
                              const std::string& precision, std::size_t size,
                              bool correct, const ErrorStats& err, const TimingStats& tm,
                              const std::string& status = "PASS",
                              const std::string& reason = "",
                              const std::string& backend = "cpu",
                              const std::string& device = "cpu") {
    CaseResult r;
    r.experiment_id = exp_id;
    r.case_id = case_id;
    r.seed = "0xA57C5AC20260802";
    r.backend = backend;
    r.device = device;
    r.precision = precision;
    r.size = size;
    r.correct = correct;
    r.max_abs = err.max_abs;
    r.max_rel = err.max_rel;
    r.rmse = err.rmse;
    r.median_ms = tm.median_ms;
    r.p95_ms = tm.p95_ms;
    r.status = status;
    r.reason = reason;
    return r;
}

// ===== 标准问题规模（1K / 64K / 1M）=====
struct SizeTriple {
    std::size_t small;
    std::size_t medium;
    std::size_t large;
};
constexpr SizeTriple kStandardSizes = {1u << 10, 1u << 16, 1u << 20};

} // namespace astro::compute::classic
