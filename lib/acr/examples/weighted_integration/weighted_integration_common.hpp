// lib/acr/examples/weighted_integration/weighted_integration_common.hpp
//
// ACR 架构冻结（07 号计划 C）：加权积分最小接入样例核心契约。
//   OperationId: synthetic.weighted_integration.fp64acc
//   - frame-major 连续输入：frames[f * pixel_count + p]，权重 weights[f]
//   - FP32 输入/权重、FP64 累加、FP32 输出
//   - 对每个输出像素 p：output[p] = Σ_f weight[f]*frame[f,p] / Σ_f weight[f]
//   - 数据生成与误差统计（计时外；固定 seed 可重复）
//
// 本头文件只含纯 host 算法与数据契约，不依赖任何 ACR 调度/桥接类型，
// 供 CPU/OpenMP 参考、CPU launcher 与测试共同复用，避免算法语义分叉。
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace astro::compute::weighted_integration {

inline constexpr const char* kOperationId =
    "synthetic.weighted_integration.fp64acc";

// ===== 输入视图 =====
struct WeightedIntegrationView {
    const float* frames{};      // frame-major: frames[f * pixels + p]
    const float* weights{};
    std::size_t frame_count{};
    std::size_t pixel_count{};
};

// ===== 逐像素核心（FP64 累加；禁止 fast-math 语义破坏）=====
inline float integrate_one_pixel(const WeightedIntegrationView& v,
                                 std::size_t p) {
    double numerator = 0.0;
    double denominator = 0.0;
    for (std::size_t f = 0; f < v.frame_count; ++f) {
        const double w = static_cast<double>(v.weights[f]);
        numerator += w * static_cast<double>(v.frames[f * v.pixel_count + p]);
        denominator += w;
    }
    if (!(denominator > 0.0)) {
        throw std::runtime_error("weighted integration denominator <= 0");
    }
    return static_cast<float>(numerator / denominator);
}

// ===== 连续范围核心（CPU launcher 只调用本函数，内部无 OpenMP）=====
inline void integrate_range(const WeightedIntegrationView& v,
                            std::size_t begin,
                            std::size_t end,
                            float* output) {
    for (std::size_t p = begin; p < end; ++p) {
        output[p] = integrate_one_pixel(v, p);
    }
}

// ===== 确定性廉价 hash ∈ [0,1)（数据生成计时外）=====
inline float hash01(std::uint64_t seed, std::size_t f, std::size_t p) {
    std::uint64_t x = seed ^ (0x9E3779B97F4A7C15ull * (f + 1));
    x ^= 0xBF58476D1CE4E5B9ull * (p + 1);
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27; x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return static_cast<float>((x >> 40) * (1.0 / 16777216.0));
}

// ===== 合成数据（固定 seed、有限、无 NaN/Inf）=====
inline void generate_synthetic(std::uint64_t seed,
                               std::size_t frame_count,
                               std::size_t pixel_count,
                               std::vector<float>& frames,
                               std::vector<float>& weights) {
    frames.resize(frame_count * pixel_count);
    weights.resize(frame_count);
    for (std::size_t f = 0; f < frame_count; ++f) {
        weights[f] = 0.5f + 0.01f * static_cast<float>((f * 37) % 101);
        for (std::size_t p = 0; p < pixel_count; ++p) {
            frames[f * pixel_count + p] =
                0.25f + 0.5f * hash01(seed, f, p)
                + 0.01f * static_cast<float>(f % 17);
        }
    }
}

// 仅生成权重（resident-reuse 场景：同一帧栈连续 4 组不同权重）
inline void generate_weights(std::uint64_t seed,
                             std::size_t frame_count,
                             std::vector<float>& weights) {
    (void)seed;
    weights.resize(frame_count);
    for (std::size_t f = 0; f < frame_count; ++f) {
        weights[f] = 0.5f + 0.01f * static_cast<float>((f * 37 + 13) % 101);
    }
}

// ===== 误差统计 =====
struct ErrorStats {
    double max_abs{};
    double relative_l2{};
    bool finite{true};
    std::size_t coverage{0};  // 参与比较的像素数
};

inline ErrorStats compare(const std::vector<float>& ref,
                          const std::vector<float>& got) {
    if (ref.size() != got.size()) {
        throw std::runtime_error("weighted integration size mismatch");
    }
    double max_abs = 0.0, diff2 = 0.0, ref2 = 0.0;
    bool finite = true;
    std::size_t coverage = 0;
    for (std::size_t i = 0; i < ref.size(); ++i) {
        finite = finite && std::isfinite(got[i]) && std::isfinite(ref[i]);
        const double d = static_cast<double>(got[i]) - ref[i];
        max_abs = std::max(max_abs, std::abs(d));
        diff2 += d * d;
        ref2 += static_cast<double>(ref[i]) * ref[i];
        ++coverage;
    }
    return {max_abs, std::sqrt(diff2 / std::max(ref2, 1e-300)),
            finite, coverage};
}

} // namespace astro::compute::weighted_integration
