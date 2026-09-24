// 方差面可用性策略（SCI-NOISE-001 §7/§9 + DATA_SEMANTICS §4a 逐像素三态表）
//
// 用途: 消费侧（阶段二 drizzle 节点）判定一张**已 fill 出来的逐像素 variance 面**
//   是否可以挂到帧上。生产者是 snr_noise_model_v1_fill。
//
// 判据（两态穷尽，与生产者同判据）:
//   * 合法态 A「方差可用」: variance > 0 ∧ isfinite(variance)
//   * 合法态 B「有覆盖但方差不可用」: variance == 0（§4a；对应平面预测 ≤ 0
//     或该像素在产品 dtype 中不可表示）
//   * 非法态（产品损坏）: variance < 0 或非有限 ⇒ **整面拒绝**（§4a「损坏 ⇒ rc=−6
//     硬失败，禁 clamp/禁静默跳过」）
//
// 为什么「**部分**含 0 的平面」必须被接受而不是整张丢弃:
//   0 是 §4a 明文规定的**合法产品态**（有覆盖、方差不可用 ⇒ 零权重），投影侧
//   （drizzle_engine 的 varianceValue<=0 分支）按「只跳过方差累加、不丢
//   signal/support」消费它。整张丢弃会把**可用像素的空间方差结构**一并丢掉，
//   退化成帧级常数场——那是纯损失，不是保护。
//
// 为什么「**整幅全 0** 的平面」必须被拒绝（与上一条不矛盾）:
//   上一条的理由是「保住可用像素的结构」；整幅全 0 时**不存在可用像素**，
//   该理由不适用，而代价是实在的：投影侧对 varianceValue<=0 不累加 vnum
//   （drizzle_engine.cpp 的 `if (varianceValue > 0.0f)`）⇒ 阶段二写 variance tile 时
//   covered_area 全 0 ⇒ aio_hips_write_variance_tile 硬失败（rc=-5）⇒ **整个 mosaic
//   阶段 exit 7**。不挂块则消费侧走 variancePtr==nullptr 路径、不写 variance 子产品，
//   阶段二正常完成。故全 0 面与「空面」同类：接受它等于把「无事可做」伪装成
//   「全部合法」，是恒真门。
//
// 逐位中性: 全正平面下本策略只读不写，输出面逐字节不变。
#ifndef ASTROCS_NOISE_VARIANCE_PLANE_POLICY_H
#define ASTROCS_NOISE_VARIANCE_PLANE_POLICY_H

#include <cmath>
#include <cstddef>

namespace astrocs {
namespace noise {

struct VariancePlaneVerdict {
    bool accepted = false;      // 整面是否可挂
    std::size_t n_usable = 0;       // variance > 0 ∧ 有限的像素数（§4a 合法可用态）
    std::size_t n_unavailable = 0;  // variance == 0 的像素数（§4a 合法不可用态）
    std::size_t n_corrupt = 0;      // variance < 0 或非有限的像素数（产品损坏）
};

// 逐像素扫描一张 variance 面。
// accepted 当且仅当 **v != nullptr ∧ n > 0 ∧ n_corrupt == 0 ∧ n_usable > 0**。
// 「n == 0」（空面）与「n_usable == 0」（整幅全 0）都显式判**拒绝**：两者都没有
//   任何像素可用，接受它们等于把「无事可做」伪装成「全部合法」——那是恒真门
//   （对任意无可用像素的输入都给同一结论，无证据资格）；且整幅全 0 面会直接
//   导致阶段二 variance tile 写入硬失败（见文件头）。
inline VariancePlaneVerdict classify_variance_plane(const float* v, std::size_t n) {
    VariancePlaneVerdict r;
    if (v == nullptr || n == 0) return r;   // 空指针/空面 ⇒ 拒绝（accepted 保持 false）
    for (std::size_t i = 0; i < n; ++i) {
        const float x = v[i];
        if (std::isnan(x) || x < 0.0f) { ++r.n_corrupt; continue; }
        if (x == 0.0f) { ++r.n_unavailable; continue; }
        if (!std::isfinite(x)) { ++r.n_corrupt; continue; }   // +inf
        ++r.n_usable;
    }
    r.accepted = (r.n_corrupt == 0) && (r.n_usable > 0);
    return r;
}

// 旧判据（「任一像素非正 ⇒ 整面拒绝」）只作对照臂保留，**不得**用于生产判定:
//   它把 §4a 的合法零方差态误判成损坏，从而丢掉可用像素的空间结构。
inline bool legacy_plane_all_strictly_positive(const float* v, std::size_t n) {
    if (v == nullptr || n == 0) return false;   // 空面不得恒真
    for (std::size_t i = 0; i < n; ++i) {
        const float x = v[i];
        if (!(std::isfinite(x) && x > 0.0f)) return false;
    }
    return true;
}

// ── SCI-VAR-ADAPT-01 (SCI-NOISE-001 §5d): 凸包内负区 = 拟合缺陷, fail-closed ──
// §5d:「凸包内出现大面积预测 ≤ 0 属**拟合缺陷**, 必须按 §8 登记并 fail-closed,
//      不得静默出片」; 凸包**外**的预测 ≤ 0 才是合法不可用态（真正的边缘外推）。
// 生产者（snr_noise_model_v1/_f64 的 build）在控制点凸包内施加**非负约束** ⇒
// NoiseWeightModelV1.hull_nonpositive_frac（凸包内 {预测 ≤ 0} 的面积占比）应恒为 0。
// 本函数把「0」变成可执行门: 任何 > 0 的占比都判**不可审计**（消费方不得据此发布）。
//   **无数值阈值**: 0 是"结构非负"约束的**定义值**, 不是可调门限 —— 与 §5d
//   「禁止写死阈值常数」一致。
// 与 classify_variance_plane 正交: 后者判「这张已 fill 的面能不能挂」, 本函数判
//   「这张面的**拟合**是否可信」; 两者都过才应发布（缺一即 fail-closed）。
// 判据取**严格等于 0**: 任何 ≠ 0 的取值（正残差、负值、NaN）一律判不可审计 ——
// fail-closed 且不含任何数值容差（0 是约束的定义值, 不是"小到可以忽略"的阈值）。
inline bool variance_plane_auditable(double hull_nonpositive_frac) {
    return std::isfinite(hull_nonpositive_frac) && hull_nonpositive_frac == 0.0;
}

}  // namespace noise
}  // namespace astrocs

#endif  // ASTROCS_NOISE_VARIANCE_PLANE_POLICY_H
