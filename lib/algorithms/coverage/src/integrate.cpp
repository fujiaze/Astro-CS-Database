// lib/algorithms/coverage/src/integrate.cpp — Phase2 加权叠加（ 冻结语义）
#include "astro/phase2/integrate.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

extern "C" {

int p2_validate_candidate_weights(const double* weights, std::uint32_t count) {
    if (weights == nullptr) return 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        // 零权重合法（无统计贡献）；负/NaN/Inf 违规。
        if (!std::isfinite(weights[i]) || weights[i] < 0.0) return 1;
    }
    return 0;
}

int p2_integrate_pixel(const P2PixelStack* in, P2PixelResult* out) {
    if (in == nullptr || out == nullptr) return 1;
    std::memset(out, 0, sizeof(*out));
    out->n_candidates = in->count;
    if (in->count == 0 || in->values == nullptr) {
        out->status = P2_INTEGRATE_NO_CANDIDATES;
        return 0;
    }

    // integration eligibility：finite(value)/finite(support>0)/
    // finite(weight>0)/accepted；任一非 finite 输入 → INVALID_INPUT
    bool invalid_input = false;
    double wsum = 0.0, vs = 0.0, sup_max = 0.0;
    std::uint32_t n_finite = 0, n_positive_weight = 0, n_accepted = 0;
    for (std::uint32_t i = 0; i < in->count; ++i) {
        const bool acc = (in->accepted == nullptr) || in->accepted[i];
        if (acc) ++n_accepted;
        if (!acc) continue;
        if (!std::isfinite(in->values[i])) { invalid_input = true; continue; }
        if (in->support != nullptr &&
            (!std::isfinite(in->support[i]) || in->support[i] <= 0.0)) {
            invalid_input = true;
            continue;
        }
        ++n_finite;
        // B2-A7: canonical reducer 契约 = max(**accepted** support)，作用域是
        // 通过资格门 (accepted ∧ value/支持 finite) 的全部样本，**不含**权重
        // 正性要求（w==0 合法但不贡献 signal）。故 sup_max 必须在权重分支
        // 之前更新；旧实现置于 w==0 continue 之后会让零权 accepted 样本的
        // support 被静默丢弃，低估 coverage 并集（integrate.h:17-19 冻结语义）。
        if (in->support != nullptr)
            sup_max = std::max(sup_max, in->support[i]);  // canonical reducer
        double w = 1.0;
        if (in->weights != nullptr) {
            w = in->weights[i];
            if (!std::isfinite(w)) { invalid_input = true; continue; }
            if (w < 0.0) { invalid_input = true; continue; }  // 负权重=契约违规
            if (w == 0.0) continue;  // 零权重=合法但不贡献（ZERO_VALID_WEIGHT）
        }
        ++n_positive_weight;
        vs += w * in->values[i];
        wsum += w;
        ++out->n_used;
    }
    out->n_finite = n_finite;
    out->n_positive_weight = n_positive_weight;
    out->n_accepted = n_accepted;
    if (invalid_input) {
        out->status = P2_INTEGRATE_INVALID_INPUT;
        return 0;
    }
    if (n_positive_weight == 0) {
        out->status = (n_accepted == 0) ? P2_INTEGRATE_ALL_REJECTED
                                        : P2_INTEGRATE_ZERO_VALID_WEIGHT;
        // support 是**覆盖并集下界**，与 signal 是否可用**解耦**（integrate.h 冻结语义：
        // 「output support 唯一 canonical reducer = max(accepted support)」）。
        // 全零权只表示"没有可用的统计贡献"（ZERO_VALID_WEIGHT），不表示"没有覆盖"；
        // 此处若不发布 support，零权 accepted 样本的覆盖会被静默丢弃，下游把
        // "有覆盖但方差不可用"误判成"无覆盖"并据此 fail-closed 判 tile 缺失。
        // ALL_REJECTED（无任何 accepted 样本）的 sup_max 恒为 0，保持不发布。
        if (out->status == P2_INTEGRATE_ZERO_VALID_WEIGHT)
            out->support = (in->support != nullptr) ? sup_max : 1.0;
        return 0;
    }
    // 输出侧有限性门（**发布面**，与上面的输入侧资格门是两道不同的门）：
    // 上面那道只保证「每个样本的 value/support/weight 有限」，**不保证归约结果
    // 有限** —— N 个各自有限的大数相加仍可溢出到 ±Inf（w·v 或 wsum 都可以），
    // 而 vs/wsum 在 (±Inf, ±Inf) 上得 NaN。旧实现没有这道门：溢出时照样
    // `status = P2_INTEGRATE_OK` 并把 NaN 当科学值发布，调用方按 OK 读 ⇒
    // NaN 进入产品面且该像素计入有效像素数（tools/stage2.cpp 的
    // `ok ? signal*area : 0` / `valid[p]` / `++total_pixels` 三处）。
    // 判据对**发布量**本身取有限性（不是对判据自身取——那才是恒真门）：
    //   isfinite(vs) ∧ isfinite(wsum) ∧ isfinite(vs/wsum)。
    // 为什么不复用既有五态：输入全合法 ⇒ INVALID_INPUT 是谎报；权重全正 ⇒
    // ZERO_VALID_WEIGHT 是谎报；这是第 6 态（integrate.h）。
    {
        const double sig = vs / wsum;   // wsum>0 已由上面的 ZERO_VALID_WEIGHT 分支保证
        if (!std::isfinite(vs) || !std::isfinite(wsum) || !std::isfinite(sig)) {
            // 发布面：无效的唯一表示 = NaN（NUMERIC_STANDARD「覆盖级 NaN」条），
            // 禁 0/±Inf/哨兵值伪装。support 仍按 canonical reducer 发 sup_max：
            // 覆盖确实存在（样本通过了资格门），失效的只是统计量——与
            // ZERO_VALID_WEIGHT 分支同一条「support 与 signal 解耦」的冻结
            // 语义（integrate.h）；发 0 会把「有覆盖」误报成「无覆盖」。
            out->signal = std::numeric_limits<double>::quiet_NaN();
            out->support = (in->support != nullptr) ? sup_max : 1.0;
            out->status = P2_INTEGRATE_NON_FINITE_OUTPUT;
            return 0;
        }
        out->signal = sig;
    }
    out->support = (in->support != nullptr) ? sup_max : 1.0;
    out->status = P2_INTEGRATE_OK;
    return 0;
}

} // extern "C"
