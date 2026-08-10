// lib/phase2/src/block.cpp — Phase2 W6 动态分块
#include "astro/phase2/block.h"

#include <algorithm>
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
    //        (P·N_B·scratch_per_sample if provided) + fixed
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
    // 超出预算：缩块（normal path），不 swap。
    // 单 tile 已超出（调用方 output_pixels=单 tile）：标记 micro-chunk。
    out->micro_chunk_required = 1;
    out->status = 0;
    return 0;
}

} // extern "C"