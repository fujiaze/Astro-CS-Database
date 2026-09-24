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

// ── SCI-NOISE-001 §5d: 「可观测量（必须写入 provenance）」的**机检清单** ────────
// 规范原文（docs/science/NOISE_MODEL.md §5d:270-271）:
//   「**可观测量（必须写入 provenance）**：控制点方差的动态范围、平面系数、
//     凸包内预测 ≤ 0 的像素占比、被剔除 patch 数与 `R` 的分布。
//     **缺任一项即视为该帧的方差面不可审计。**」
// 逐条 → 键名（与生产者 NoiseWeightModelV1 的尾部字段一一对应，
// snr_estimator.h:187-201；键名也是逐帧 provenance 的产品面键名）:
//   控制点方差的动态范围        → ctrl_variance_range
//   平面系数 var(x,y)=a+b·x+c·y → plane_a, plane_b, plane_c
//   凸包内预测 ≤ 0 的像素占比   → hull_nonpositive_frac
//   被剔除 patch 数             → n_structure_rejected_patches
//   R 的分布                    → r_min, r_median, r_max, r_fence
//
// 为什么这条清单必须**可执行**：§5d 的最后一句是「缺任一项即视为该帧的方差面
//   不可审计」——它把「字段齐全」直接定义为「可审计」的**必要条件**。若清单只
//   活在文档里，任何一处落盘遗漏都只能靠人读 JSON 发现（本仓实测：这些字段曾
//   只写进**内存里的节点 manifest**，产品里一个都没有）。故清单在此以代码形式
//   给出，生产者（p1_op_drizzle）、消费/校验者（p1_op_writer、产品级检查器、
//   测试）**必须**引用同一份，不得各写一份。
//
// 判据（与 variance_plane_auditable 正交，两者都过才可发布）:
//   variance_plane_auditable(...)          判「这张面的**拟合**是否可信」；
//   variance_audit_missing_fields(...)     判「这张面的**可审计信息**是否在产品里」。
// 后者对**每一个**字段要求「存在 ∧ 是有限数」（整数字段按有限整数判）。
inline const char* const* variance_audit_required_fields() {
    static const char* const kFields[] = {
        "ctrl_variance_range",
        "plane_a",
        "plane_b",
        "plane_c",
        "hull_nonpositive_frac",
        "n_structure_rejected_patches",
        "r_min",
        "r_median",
        "r_max",
        "r_fence",
    };
    return kFields;
}

// 清单长度（与 variance_audit_required_fields() 同步；单独给函数是为了让调用方
// 不必自己数，避免加字段时漏改一处）。
inline std::size_t variance_audit_required_field_count() { return 10u; }

}  // namespace noise
}  // namespace astrocs

#endif  // ASTROCS_NOISE_VARIANCE_PLANE_POLICY_H
