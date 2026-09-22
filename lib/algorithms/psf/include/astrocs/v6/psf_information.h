/* psf_information.h - Phase1/V6 A_NEA 与 effective PSF (IMPL-P1-PSFW-001)
 *
 * 合同锚 (docs/design/PHASE1_DETAILED_DESIGN.md;
 *         docs/science/PSF_SIGNAL_WEIGHT.md):
 *   - FZ-COND-WHITENOISE : P_k(u,v) >= 0, Sum_p P_k,p = 1 ;
 *                          A_NEA,k = 1 / Sum_p P_k,p^2   [px^2]
 *   - FZ-GATE-PSFSW-EPSF : effective PSF 必输; 只给 FWHM 标量不构成 effective PSF
 *   - FZ-FORMULA-COV-PROP: effective PSF 是实际组合算子的脉冲响应 (conventional coadd)
 *
 * 单位: A_NEA = px^2 (冻结表 eng/contracts/data/v6_clause_registry_v1.json#weight_vocabulary §1
 *       PSFSW 行; docs/science/PSF_SIGNAL_WEIGHT.md)。
 *
 * 纪律: 本头/源不接线 session; 不含任何权重/方差反推; 不引用 legacy median SNR。
 * 复用: 无 (纯 std + libm)。
 */
#ifndef ASTROCS_V6_P1PSFW_PSF_INFORMATION_H
#define ASTROCS_V6_P1PSFW_PSF_INFORMATION_H

#include <cstddef>
#include <string>
#include <vector>

namespace astrocs {
namespace v6 {
namespace p1psfw {

/* 冻结容差: PSF 归一 / A_NEA 复算 (FZ-AP1-GLS-QW-RTOL 的 PSF 侧). */
constexpr double kPsfNormRtol = 1e-9;

/* ------------------------------------------------------------------------- */
/* PSF 归一与噪声等效面积 (FZ-COND-WHITENOISE)                                */
/* ------------------------------------------------------------------------- */
struct PsfProfileStats {
    bool ok = false;
    /* 拒绝原因 (ok=false 时非空):
     *   "null_input" | "empty_support" | "non_finite"
     *   "negative_sample"  (P_p < 0, 违反 P >= 0)
     *   "not_normalized"   (|Sum P - 1| > rtol, 违反 Sum P = 1)          */
    const char* reject = nullptr;
    std::size_t n = 0;          /* 支持域像素数 */
    double sum_p = 0.0;
    double sum_p2 = 0.0;
    double a_nea = 0.0;         /* A_NEA = 1 / Sum P^2   [px^2] */
    double norm_abs_error = 0.0;/* |Sum P - 1|        (机器可读证据) */
};

/* 计算 PSF 剖面统计量。 norm_rtol 默认 kPsfNormRtol (冻结)。 */
PsfProfileStats psf_profile_stats(const double* p, std::size_t n,
                                  double norm_rtol = kPsfNormRtol);

/* 便捷: 仅 A_NEA; 未通过归一/正性检查时返回 0 并置 reject (见 PsfProfileStats)。 */
double psf_anea(const double* p, std::size_t n);

/* ------------------------------------------------------------------------- */
/* effective PSF —— conventional coadd 算子脉冲响应 (FZ-GATE-PSFSW-EPSF)      */
/* ------------------------------------------------------------------------- */
enum class EffectivePsfNormalization { peak, integral };

const char* to_string(EffectivePsfNormalization n);

struct EffectivePsf {
    bool ok = false;
    const char* reject = nullptr;
    std::string effective_psf_id;
    EffectivePsfNormalization normalization = EffectivePsfNormalization::peak;
    std::vector<double> profile;   /* 与输入 PSF 同一网格 */
    std::vector<double> grid_x;    /* 可选: x 坐标 (空则用像素索引) */
    double fwhm = 0.0;
    double ee_r1 = 0.0;            /* 1*FWHM 内 encircled energy */
    double ee_r2 = 0.0;            /* 2*FWHM 内 encircled energy */
    /* 审计门 (FZ-GATE-PSFSW-EPSF): 只给 FWHM 标量不构成 effective PSF。 */
    bool only_fwhm_scalar = true;
    bool normalization_declared = true;
};

/* P_eff(x; x_o) = [ Sum_k alpha_k a_k (P_k)(x) ] / [ Sum_k alpha_k a_k P_k(0) ]
 *   (peak 归一; integral 版再除以 Sum_x 分子)
 * profiles 逐个长度 n, 同网格。 a_k 为光度响应, alpha_k 为实际组合系数。
 * 传入空 profiles / 长度不符 / alpha 全零 -> reject。 */
EffectivePsf conventional_effective_psf(
    const std::vector<const double*>& profiles, std::size_t n,
    const std::vector<double>& a_k, const std::vector<double>& alpha_k,
    EffectivePsfNormalization normalization, const std::string& effective_psf_id);

/* FWHM 从 P_eff 剖面测量 (线性插值半高点); 不做逐帧 median 代替。 */
double measure_fwhm(const double* profile, std::size_t n);

/* 给定半径 (px, 居中索引 n/2) 内的累积能量 / 总能量。 */
double measure_encircled_energy(const double* profile, std::size_t n, double radius_px);

}  /* namespace p1psfw */
}  /* namespace v6 */
}  /* namespace astrocs */

#endif /* ASTROCS_V6_P1PSFW_PSF_INFORMATION_H */
