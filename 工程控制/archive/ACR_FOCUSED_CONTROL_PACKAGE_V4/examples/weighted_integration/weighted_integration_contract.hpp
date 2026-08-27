#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <cmath>
#include <stdexcept>

namespace acr_example {

inline constexpr const char* kOperationId =
    "synthetic.weighted_integration.fp64acc";

struct WeightedIntegrationView {
    const float* frames{};        // frame-major: frames[f * pixels + p]
    const float* weights{};
    std::size_t frame_count{};
    std::size_t pixel_count{};
};

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

inline void integrate_range(const WeightedIntegrationView& v,
                            std::size_t begin,
                            std::size_t end,
                            float* output) {
    for (std::size_t p = begin; p < end; ++p) {
        output[p] = integrate_one_pixel(v, p);
    }
}

// Deterministic, cheap hash in [0,1). Data generation is outside timed regions.
inline float hash01(std::uint64_t seed, std::size_t f, std::size_t p) {
    std::uint64_t x = seed ^ (0x9E3779B97F4A7C15ull * (f + 1));
    x ^= 0xBF58476D1CE4E5B9ull * (p + 1);
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27; x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return static_cast<float>((x >> 40) * (1.0 / 16777216.0));
}

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

struct ErrorStats {
    double max_abs{};
    double relative_l2{};
    bool finite{true};
};

inline ErrorStats compare(const std::vector<float>& ref,
                          const std::vector<float>& got) {
    if (ref.size() != got.size()) throw std::runtime_error("size mismatch");
    double max_abs = 0.0, diff2 = 0.0, ref2 = 0.0;
    bool finite = true;
    for (std::size_t i = 0; i < ref.size(); ++i) {
        finite = finite && std::isfinite(got[i]);
        const double d = static_cast<double>(got[i]) - ref[i];
        max_abs = std::max(max_abs, std::abs(d));
        diff2 += d * d;
        ref2 += static_cast<double>(ref[i]) * ref[i];
    }
    return {max_abs, std::sqrt(diff2 / std::max(ref2, 1e-300)), finite};
}

} // namespace acr_example
