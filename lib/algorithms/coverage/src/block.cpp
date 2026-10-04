// lib/algorithms/coverage/src/block.cpp — Phase2 动态分块
#include "astro/phase2/block.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

// safety_factor 的值域上界。budget = memory_limit · sf，故 sf > 1 等于「按比
// 用户给的内存上限更大的预算规划」——那不是余量，是把 memory_limit 当摆设。
constexpr double kSafetyFactorMax = 1.0;

}  // namespace

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
    // precision 是枚举面：契约只有 fp32(0)/fp64(1) 两值。非 0/1 的取值不是
    // 「按 fp32 处理」的许可，而是调用方算错了；悄悄换成 fp32 会让内存估算
    // 在调用方以为是 fp64 的地方按 fp32 计（低估一半），错误不可见。
    if (in->precision != 0 && in->precision != 1) {
        std::strncpy(out->error, "precision 必须是 0(fp32) 或 1(fp64)",
                     sizeof(out->error) - 1);
        out->status = 1;
        return 0;
    }
    // safety_factor **无默认**：非有限 / ≤0 / >1 一律不可行。
    // 为什么不给隐式兜底：0.75 在本仓的唯一在册出处是调用点
    // lib/algorithms/coverage/tools/stage2.cpp（tile 级与 tile 循环各一处
    // 都显式写 0.75）与 docs/science/algorithms/PHASE2_MOSAIC_WRITE.md 对
    // 同一字面量的登记——两处都只是**登记**不是推导；而峰值公式的每一项
    // 都来自 N_B / 像素数 / 每样本字节三类输入，没有任何一项能定出「余量」，
    // 它属**需标定的工程参数**（给公式未建模的运行期分配留份额），不能由
    // 输入几何或物理关系导出。若把越界值悄悄换成 0.75，「调用方算错了」在
    // 运行期就表现为「一切正常」⇒ fail-closed。
    // （落配置面是它的最终归属，但 config_registry.json / schema /
    // 调用点三处都在本车道写面之外；此处只保证不再静默吞掉非法值。）
    if (!std::isfinite(in->safety_factor) ||
        !(in->safety_factor > 0.0) ||
        in->safety_factor > kSafetyFactorMax) {
        std::strncpy(out->error,
                     "safety_factor 必须在 (0, 1]（无默认；越界值不静默替换）",
                     sizeof(out->error) - 1);
        out->status = 1;
        return 0;
    }
    const double bytes_per_sample = (in->precision == 1) ? 8.0 : 4.0;
    const double sf = in->safety_factor;

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
        // 走到这里说明上面的 peak<=budget 已经不成立。三项输入全是无符号，
        // 故 per_px==0 ⇒ N_B=0 且两项 scratch 均为 0 ⇒ sample_work /
        // per_pixel_scratch / per_sample_scratch 全为 0 ⇒ peak 恒等于
        // fixed_overhead。于是 fixed_overhead 本身就超预算：连 0 像素的块
        // 都装不下，micro-chunk 缩小块尺寸也救不了这一项（它与块大小无关）。
        // 返回「不可行」而不是 status=0 ——否则调用方会拿着一份
        // estimated_peak_bytes > budget 的计划当可行继续跑（假绿）。
        out->status = 1;
        std::strncpy(out->error,
                     "per-pixel cost is 0 but peak still exceeds budget: "
                     "fixed_overhead alone is above the memory limit",
                     sizeof(out->error) - 1);
        out->block_pixels = 1;
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
