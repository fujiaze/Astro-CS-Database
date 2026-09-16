#ifndef SDET_ANGLE_GUARD_H
#define SDET_ANGLE_GUARD_H

// ============================================================================
// sdet_angle_guard.h - 朝向角归一化的有界 / fail-closed 实现
//
// 背景 (SDET-ANGLE-001, P11 生产挂死级缺陷):
//   原 sdet_api.cpp sdet_lm_fit 在 GSL trust-region LM 返回 alpha=±inf
//   (或 |angle_deg| 极大) 时执行:
//       while (fabs(angle_deg) > 90.0) { angle_deg ∓ 180.0; }
//   该循环对 ±inf 永不终止 (inf ∓ 180 仍为 inf), 对 1e300 量级需 ~1e298 次
//   迭代, 均等价于挂死; 紧随其后 |angle_deg| > 10000 的保护位于循环**之后**,
//   属死代码, 无法兜底。
//
// 契约 (与冻结合同 § 一致的最小修复):
//   1. 有界: 迭代次数硬上界 kMaxAngleHalfTurns; 半程数先用闭式表达式
//      ceil((|angle|-90)/180) 预判, 超界立即拒绝, 不做无谓迭代。
//   2. fail-closed: 非有限输入 / 超界输入 -> 返回 false, 调用方必须按
//      拟合失败处理 (SDET_FIT_NO_CONVERGENCE), 绝不静默产出错角度。
//   3. 有限值逐位不变: 对闭式预判通过的所有有限 angle_deg, 本函数执行的
//      ±180.0 运算序列与冻结迭代式**逐步完全相同** (同一操作数、同一顺序、
//      同一舍入), 因此输出逐位一致; 未改用 fmod/remainder 等闭式三角化,
//      正是因为它对 (90,270] 等区间的 tie/舍入与冻结序列不逐位等价。
//
// 日期: 2026-09-14
// ============================================================================

#include <cmath>

namespace astrocs {
namespace star_detector {

// 归一化最多允许的半程 (180°) 次数。物理上 alpha 为朝向角, 归一化后必落在
// [-90, 90] (两端均为不动点); 64 次 (≥ 11610°) 对任何真实朝向都是宽松上界, 但足以让
// "迭代不收敛到 [-90,90]" 的病态输入在有限步内被明确拒绝。
inline constexpr int kMaxAngleHalfTurns = 64;

// 将 angle_deg 归一化到 [-90, 90], 行为与冻结迭代式对有限输入逐位一致。
// 返回 true 时 *out_angle_deg 已写为归一化结果;
// 返回 false 表示输入非有限或所需半程数超界 (fail-closed, 不写 *out)。
inline bool normalize_angle_deg_bounded(double angle_deg, double* out_angle_deg) {
    if (out_angle_deg == nullptr) return false;

    // 1) 非有限 (含 ±inf / NaN) 一律 fail-closed。
    if (!std::isfinite(angle_deg)) return false;

    // 2) 闭式半程数上界预判: 需要 n = ceil((|angle|-90)/180) 次 ±180。
    //    超界 (含所需次数本身不可表示的极大有限值) 立即拒绝, 不进入迭代。
    const double over = std::fabs(angle_deg) - 90.0;
    if (over > 0.0) {
        const double required = std::ceil(over / 180.0);
        if (!(required <= static_cast<double>(kMaxAngleHalfTurns))) return false;
    }

    // 3) 与冻结迭代式完全相同的运算序列 (有限值逐位不变);
    //    硬上界只用于把"本不该到达的"病态输入转成 fail-closed。
    int steps = 0;
    while (std::fabs(angle_deg) > 90.0) {
        if (steps >= kMaxAngleHalfTurns) return false;
        angle_deg += (angle_deg > 0.0) ? -180.0 : 180.0;
        ++steps;
    }

    *out_angle_deg = angle_deg;
    return true;
}

}  // namespace star_detector
}  // namespace astrocs

#endif  // SDET_ANGLE_GUARD_H
