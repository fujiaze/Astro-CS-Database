// lib/acr/tests/classic/e17_model_fit.cpp — E17 Hardware Profile 拟合验证
//
// 设计（17 §18 + 06 §13）：
//   1. 将原始 benchmark 数据拟合为按 log2 尺寸的分段曲线（Curve::predict 已实现）
//   2. 使用留出测试点（holdout）验证预测误差
//   3. median 相对预测误差目标 <=15%
//   4. 交叉点附近允许更宽但必须报告
//   5. 不满足时增加采样点或标记低置信度
//   6. 禁止通过在线运行改写模型
//
// 验证策略（v2：训练点 log2 间隔=1，验证点在半整数 log2 中点）：
//   - 训练点：整数 log2 尺寸（4KB, 8KB, 16KB, ..., 64MB），log2 间隔=1
//   - 验证点：相邻训练点的几何中点（log2=k+0.5），用真实模型计算期望值
//   - 这样训练点在 log2 空间连续，Curve::predict 的 log2 线性插值误差最小
//   - 对线性数据 value∝size：log2 间隔=1 时中点误差≈6%（远低于 15% 门限）
//   - 对对数数据 value∝log2(size)：log2 空间线性，中点误差≈0
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "astro/compute/hardware_profile.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// 合成 benchmark 数据点（size, median_ns）
struct DataPoint {
    std::size_t size;
    double value;  // median 耗时（ns）或吞吐
};

// 留出验证结果
struct HoldoutResult {
    double median_rel_err{0.0};   // median 相对预测误差
    double max_rel_err{0.0};       // 最大相对预测误差
    double p95_rel_err{0.0};       // 95 分位相对预测误差
    std::size_t n_holdout{0};       // 留出点数
    std::vector<double> rel_errs;   // 每个留出点的相对误差
};

// 模型函数：value = f(size)
using ModelFn = std::function<double(std::size_t)>;

// 训练 + 验证数据集
// 训练点：整数 log2 尺寸（log2 间隔=1）
// 验证点：相邻训练点的几何中点（log2=k+0.5），用真实模型计算期望值
struct TrainValidate {
    std::vector<DataPoint> train;     // 整数 log2 尺寸的训练点
    std::vector<DataPoint> validate;  // 半整数 log2 尺寸的验证点
};

// 生成训练点（整数 log2）+ 验证点（相邻训练点的几何中点）
// 训练点 log2 间隔=1，验证点在 log2 中点，使 Curve::predict 的 log2 线性插值误差最小
TrainValidate make_train_validate(ModelFn model, double noise_pct, std::uint64_t seed,
                                    std::size_t min_bytes = 4 * 1024,
                                    std::size_t max_bytes = 64 * 1024 * 1024) {
    TrainValidate tv;
    LCG rng(seed);
    std::size_t s = min_bytes;
    while (s <= max_bytes) {
        // 训练点：整数 log2 尺寸
        double v = model(s);
        if (v < 1.0) v = 1.0;
        double noise = (rng.next_double() * 2.0 - 1.0) * noise_pct * v;
        tv.train.push_back({s, v + noise});

        // 验证点：当前训练点与下一个训练点的几何中点
        // mid = sqrt(s * next_s)，log2(mid) = (log2(s) + log2(next_s)) / 2
        std::size_t next_s = s << 1;
        if (next_s <= max_bytes && next_s > s) {
            double mid_d = std::sqrt(static_cast<double>(s) * static_cast<double>(next_s));
            std::size_t mid = static_cast<std::size_t>(mid_d);
            // 确保 mid 在 (s, next_s) 之间
            if (mid > s && mid < next_s) {
                double vm = model(mid);
                if (vm < 1.0) vm = 1.0;
                double noise_m = (rng.next_double() * 2.0 - 1.0) * noise_pct * vm;
                tv.validate.push_back({mid, vm + noise_m});
            }
        }
        s = next_s;
    }
    return tv;
}

// 从训练数据点构建 Curve
Curve build_curve(const std::vector<DataPoint>& train) {
    Curve curve;
    for (const auto& p : train) {
        CurvePoint cp;
        cp.size = p.size;
        cp.median = p.value;
        cp.p95 = p.value * 1.05;
        cp.mad = p.value * 0.02;
        curve.points.push_back(cp);
    }
    return curve;
}

// 计算验证点的相对预测误差
HoldoutResult evaluate_validate(const Curve& curve, const std::vector<DataPoint>& validate) {
    HoldoutResult r;
    r.n_holdout = validate.size();
    if (validate.empty()) return r;
    r.rel_errs.reserve(validate.size());
    for (const auto& p : validate) {
        double pred = curve.predict(p.size);
        double actual = p.value;
        double rel_err = 0.0;
        if (std::fabs(actual) > 1e-30) {
            rel_err = std::fabs(pred - actual) / std::fabs(actual);
        } else if (std::fabs(pred) > 1e-30) {
            rel_err = 1.0;  // 实际 0 但预测非 0
        }
        r.rel_errs.push_back(rel_err);
        if (rel_err > r.max_rel_err) r.max_rel_err = rel_err;
    }
    // median
    std::vector<double> sorted_errs = r.rel_errs;
    std::sort(sorted_errs.begin(), sorted_errs.end());
    std::size_t m = sorted_errs.size();
    r.median_rel_err = (m % 2 == 1) ? sorted_errs[m / 2]
                                    : (sorted_errs[m / 2 - 1] + sorted_errs[m / 2]) * 0.5;
    // p95
    if (m == 1) {
        r.p95_rel_err = sorted_errs[0];
    } else {
        double idx = 0.95 * (m - 1);
        std::size_t lo = static_cast<std::size_t>(idx);
        std::size_t hi = (lo + 1 < m) ? lo + 1 : lo;
        double frac = idx - static_cast<double>(lo);
        r.p95_rel_err = sorted_errs[lo] * (1.0 - frac) + sorted_errs[hi] * frac;
    }
    return r;
}

// ===== 模型函数 =====

// 线性模型：value = a * size + b（如 STREAM Triad 耗时）
// 在 log2 空间是指数曲线，log2 间隔=1 时中点误差≈6%
inline double linear_model(double a, double b, std::size_t s) {
    return a * static_cast<double>(s) + b;
}

// 对数模型：value = a * log2(size) + b（如固定开销主导的小尺寸）
// 在 log2 空间是线性，中点误差≈0
inline double log_model(double a, double b, std::size_t s) {
    return a * std::log2(static_cast<double>(s)) + b;
}

// 分段模型：小尺寸走对数，大尺寸走线性（交叉点附近）
inline double piecewise_model(double a_small, double b_small,
                               double a_large, double b_large,
                               std::size_t crossover, std::size_t s) {
    if (s <= crossover) {
        return a_small * std::log2(static_cast<double>(s)) + b_small;
    }
    return a_large * static_cast<double>(s) + b_large;
}

// 运行拟合验证并返回结果
struct FitTestResult {
    bool pass{false};
    double median_rel_err{0.0};
    double max_rel_err{0.0};
    std::size_t n_train{0};
    std::size_t n_holdout{0};
    std::string model_name;
};

FitTestResult run_fit_test(const std::string& model_name,
                            const TrainValidate& tv,
                            double target_median_rel_err = 0.15) {
    FitTestResult r;
    r.model_name = model_name;
    Curve curve = build_curve(tv.train);
    r.n_train = curve.points.size();
    r.n_holdout = tv.validate.size();
    auto holdout_result = evaluate_validate(curve, tv.validate);
    r.median_rel_err = holdout_result.median_rel_err;
    r.max_rel_err = holdout_result.max_rel_err;
    r.pass = (r.median_rel_err <= target_median_rel_err);
    return r;
}

} // anonymous namespace

// ===== GoogleTest 入口 =====

// E17.1: 线性模型（无噪声）拟合验证
// 线性数据 value=a*size+b 在 log2 空间是指数曲线
// 训练点 log2 间隔=1，验证点在中点，误差应≈6%（远低于 15%）
TEST(E17ModelFit, LinearNoNoise) {
    auto tv = make_train_validate(
        [](std::size_t s) { return linear_model(1.5, 100.0, s); },
        0.0, FIXED_SEED);
    auto r = run_fit_test("linear_no_noise", tv);
    ResultSink::instance().push(make_result("E17", "linear_no_noise", "fp64", tv.train.size(),
        r.pass, ErrorStats{r.max_rel_err, r.max_rel_err, r.median_rel_err},
        TimingStats{r.median_rel_err * 1000.0, 0.0},
        r.pass ? "PASS" : "FAIL",
        r.pass ? "" : "median_rel_err > 15% threshold"));
    EXPECT_TRUE(r.pass) << "median_rel_err=" << r.median_rel_err
                        << " (target <=0.15), n_train=" << r.n_train
                        << ", n_holdout=" << r.n_holdout;
}

// E17.2: 线性模型（5% 噪声）拟合验证
TEST(E17ModelFit, LinearWithNoise5pct) {
    auto tv = make_train_validate(
        [](std::size_t s) { return linear_model(1.5, 100.0, s); },
        0.05, FIXED_SEED);
    auto r = run_fit_test("linear_noise_5pct", tv);
    ResultSink::instance().push(make_result("E17", "linear_noise_5pct", "fp64", tv.train.size(),
        r.pass, ErrorStats{r.max_rel_err, r.max_rel_err, r.median_rel_err},
        TimingStats{r.median_rel_err * 1000.0, 0.0},
        r.pass ? "PASS" : "FAIL",
        r.pass ? "" : "median_rel_err > 15% threshold"));
    EXPECT_TRUE(r.pass) << "median_rel_err=" << r.median_rel_err;
}

// E17.3: 对数模型（10% 噪声）拟合验证
// 对数数据在 log2 空间线性，中点误差≈0
TEST(E17ModelFit, LogModelWithNoise10pct) {
    auto tv = make_train_validate(
        [](std::size_t s) { return log_model(50.0, 200.0, s); },
        0.10, FIXED_SEED ^ 0x10C0ULL);
    auto r = run_fit_test("log_noise_10pct", tv);
    ResultSink::instance().push(make_result("E17", "log_noise_10pct", "fp64", tv.train.size(),
        r.pass, ErrorStats{r.max_rel_err, r.max_rel_err, r.median_rel_err},
        TimingStats{r.median_rel_err * 1000.0, 0.0},
        r.pass ? "PASS" : "FAIL",
        r.pass ? "" : "median_rel_err > 15% threshold"));
    EXPECT_TRUE(r.pass) << "median_rel_err=" << r.median_rel_err;
}

// E17.4: 分段模型（交叉点附近，8% 噪声）拟合验证
// 交叉点附近允许更宽误差，但仍需 <=15%
TEST(E17ModelFit, PiecewiseCrossoverWithNoise8pct) {
    // 交叉点在 1MB（L3 边界附近）
    const std::size_t crossover = 1u << 20;
    auto tv = make_train_validate(
        [crossover](std::size_t s) {
            return piecewise_model(50.0, 200.0, 1.5, 100.0, crossover, s);
        },
        0.08, FIXED_SEED ^ 0xB1ECULL);
    auto r = run_fit_test("piecewise_crossover_8pct", tv);
    ResultSink::instance().push(make_result("E17", "piecewise_crossover_8pct", "fp64", tv.train.size(),
        r.pass, ErrorStats{r.max_rel_err, r.max_rel_err, r.median_rel_err},
        TimingStats{r.median_rel_err * 1000.0, 0.0},
        r.pass ? "PASS" : "FAIL",
        r.pass ? "" : "median_rel_err > 15% threshold at crossover"));
    EXPECT_TRUE(r.pass) << "median_rel_err=" << r.median_rel_err << " at crossover";
}

// E17.5: Curve::predict 边界条件测试（空曲线、单点、超出范围外推）
TEST(E17ModelFit, CurvePredictEdgeCases) {
    Curve empty;
    EXPECT_DOUBLE_EQ(empty.predict(1024), 0.0);

    Curve single;
    CurvePoint sp;
    sp.size = 1024;
    sp.median = 100.0;
    single.points.push_back(sp);
    EXPECT_DOUBLE_EQ(single.predict(512), 100.0);
    EXPECT_DOUBLE_EQ(single.predict(2048), 100.0);

    Curve two;
    CurvePoint p1; p1.size = 1024; p1.median = 100.0;
    CurvePoint p2; p2.size = 4096; p2.median = 400.0;
    two.points.push_back(p1);
    two.points.push_back(p2);
    // 在 2048 处应线性插值（log2 尺寸）：log2(2048)=11, log2(1024)=10, log2(4096)=12
    // t = (11-10)/(12-10) = 0.5, pred = 100 + 0.5*(400-100) = 250
    double pred = two.predict(2048);
    EXPECT_NEAR(pred, 250.0, 1.0);
    bool ok = std::fabs(pred - 250.0) < 1.0;
    ResultSink::instance().push(make_result("E17", "curve_predict_edge", "fp64", 3,
        ok, ErrorStats{std::fabs(pred - 250.0), 0.0, 0.0},
        TimingStats{0.0, 0.0}, ok ? "PASS" : "FAIL",
        ok ? "" : "log2 interpolation incorrect"));
}

// E17.6: 大量合成数据点拟合验证
// 线性模型 + 3% 噪声，训练点 log2 间隔=1，验证点在中点
TEST(E17ModelFit, DenseLog2SizesFitting) {
    auto tv = make_train_validate(
        [](std::size_t s) { return linear_model(2.0, 50.0, s); },
        0.03, FIXED_SEED);
    auto r = run_fit_test("dense_log2_linear_3pct", tv);
    ResultSink::instance().push(make_result("E17", "dense_log2_linear_3pct", "fp64", tv.train.size(),
        r.pass, ErrorStats{r.max_rel_err, r.max_rel_err, r.median_rel_err},
        TimingStats{r.median_rel_err * 1000.0, 0.0},
        r.pass ? "PASS" : "FAIL",
        r.pass ? "" : "median_rel_err > 15% threshold"));
    EXPECT_TRUE(r.pass) << "median_rel_err=" << r.median_rel_err
                        << ", n_train=" << r.n_train
                        << ", n_holdout=" << r.n_holdout;
    EXPECT_GE(tv.train.size(), 10u) << "expected at least 10 training points";
}

// classic_runner 调用入口
extern "C" std::vector<CaseResult> run_e17() {
    std::vector<CaseResult> results;
    const std::size_t crossover = 1u << 20;

    auto add = [&](const std::string& case_id, const TrainValidate& tv, double target = 0.15) {
        auto r = run_fit_test(case_id, tv, target);
        results.push_back(make_result("E17", case_id, "fp64", tv.train.size(),
            r.pass, ErrorStats{r.max_rel_err, r.max_rel_err, r.median_rel_err},
            TimingStats{r.median_rel_err * 1000.0, 0.0},
            r.pass ? "PASS" : "FAIL",
            r.pass ? "" : "median_rel_err > 15% threshold"));
    };

    add("linear_no_noise",
        make_train_validate([](std::size_t s) { return linear_model(1.5, 100.0, s); }, 0.0, FIXED_SEED));
    add("linear_noise_5pct",
        make_train_validate([](std::size_t s) { return linear_model(1.5, 100.0, s); }, 0.05, FIXED_SEED));
    add("log_noise_10pct",
        make_train_validate([](std::size_t s) { return log_model(50.0, 200.0, s); }, 0.10, FIXED_SEED ^ 0x10C0ULL));
    add("piecewise_crossover_8pct",
        make_train_validate([crossover](std::size_t s) { return piecewise_model(50.0, 200.0, 1.5, 100.0, crossover, s); }, 0.08, FIXED_SEED ^ 0xB1ECULL));
    add("dense_log2_linear_3pct",
        make_train_validate([](std::size_t s) { return linear_model(2.0, 50.0, s); }, 0.03, FIXED_SEED));
    return results;
}
