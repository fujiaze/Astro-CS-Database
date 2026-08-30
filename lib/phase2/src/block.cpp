// lib/phase2/src/block.cpp — Phase2 W6 动态分块
#include "astro/phase2/block.h"

#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" {

int p2_block_plan(const P2BlockPlannerInput* in, P2BlockPlan* out) {
    if (in == nullptr || out == nullptr) return 1;
    std::memset(out, 0, sizeof(*out));
    if (in->memory_limit_bytes == 0) {
        std::strncpy(out->error, "memory limit must be > 0",
                     sizeof(out->error) - 1);
        out->status = 1;
        return 0;
    }
    const double bytes_per_sample = (in->precision == 1) ? 8.0 : 4.0;
    const double sf = in->safety_factor > 0.0 ? in->safety_factor : 0.75;

    // 峰值 ≈ P·N_B·bytes_per_sample + P·scratch_per_pixel +
    // (P·N_B·scratch_per_sample if provided) + fixed
    const double sample_work = static_cast<double>(in->output_pixels) *
        static_cast<double>(in->covering_frames) * bytes_per_sample;
    const double per_pixel_scratch =
        static_cast<double>(in->output_pixels) *
        static_cast<double>(in->scratch_bytes_per_pixel);
    const double per_sample_scratch =
        static_cast<double>(in->output_pixels) *
        static_cast<double>(in->covering_frames) *
        static_cast<double>(in->scratch_bytes_per_sample);
    const double peak = sample_work + per_pixel_scratch +
                        per_sample_scratch +
                        static_cast<double>(in->fixed_overhead);
    out->block_pixels = in->output_pixels;
    out->estimated_peak_bytes = static_cast<std::uint64_t>(peak);

    const double budget = static_cast<double>(in->memory_limit_bytes) * sf;
    if (peak <= budget) {
        out->status = 0;
        return 0;
    }
    // 超出预算：正常路径缩块（不 swap），计算真实可行的 block_pixels。
    // 每输出像素成本 = N_B*sample_bytes + scratch_per_pixel + N_B*scratch_per_sample
    const double per_px =
        static_cast<double>(in->covering_frames) * bytes_per_sample +
        static_cast<double>(in->scratch_bytes_per_pixel) +
        static_cast<double>(in->covering_frames) *
            static_cast<double>(in->scratch_bytes_per_sample);
    if (per_px <= 0.0) {
        out->micro_chunk_required = 1;
        out->status = 0;
        return 0;
    }
    const double available = budget - static_cast<double>(in->fixed_overhead);
    if (available < per_px) {
        // 最小 chunk（1 像素）都无法运行：明确不可行
        out->status = 1;
        std::strncpy(out->error,
                     "memory limit 低于最小 chunk（1 像素 × N_B）需求",
                     sizeof(out->error) - 1);
        out->block_pixels = 1;
        return 0;
    }
    const double max_px = std::floor(available / per_px);
    out->block_pixels = static_cast<std::uint64_t>(max_px);
    if (out->block_pixels == 0) out->block_pixels = 1;
    // 缩块后重算峰值 (峰值 RAM 必须符合 plan 误差界 ≤ budget)
    out->estimated_peak_bytes =
        static_cast<std::uint64_t>(static_cast<double>(out->block_pixels) * per_px +
                                   static_cast<double>(in->fixed_overhead));
    out->micro_chunk_required = 1;
    out->status = 0;
    return 0;
}

} // extern "C"
