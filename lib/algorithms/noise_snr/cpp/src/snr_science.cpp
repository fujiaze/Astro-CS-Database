// snr_science.cpp - 逐源 SNR 科学实现 (P5-SNR)
//
// 依据: run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md
//   C.3.1 (i)  Horne 1986 最优提取: sigma_F^-2 = sum_i P_i^2 / sigma_i^2, SNR_F = F/sigma_F
//   C.3.1 (ii) 孔径 CCD 方程 + 孔径改正 (Moffat4 beta=4 解析 growth curve)
//   C.3.1 (iii) 帧级科学基准 = 5-sigma 点源深度 m_5 = ZP - 2.5*log10(5*sigma_F(ref))
//   C.2.2     零点标准误 sigma_kappa,stat ~ 1.253*sigma_residual/sqrt(N)
//
// 本 TU 是 SNR 科学量的**唯一权威实现**。控制点值/帧级量在
// snr_estimator.cpp 中经此实现计算, 不再使用退休量 (A-B)/residual_scale (SNR-008)。
//
// 单位约定 (强制):
//   flux_adu            [ADU]      总通量 (与输入图像同标度)
//   fwhm_px             [pixel]    **检测块**母函数宽度 = 椭圆高斯 FWHM
//                                  (FWHM = 2.3548200450309493*sigma, TWO_SQRT_2_LOG2;
//                                   SCI-P1-STAR-001 §2/§5, ALG-STARDET-001 §2)。
//                                  DATA-P1-SOURCES.fwhm_px 即该量 (wrapper 检测侧产出)。
//   sigma_px            [pixel]    本块 Moffat4 轮廓的尺度参数 sigma [pixel]
//                                  (与 fwhm_px 同一 sigma 尺度; fwhm_px<=0 时直接采用)
//
// 跨块口径 (SCI-P1-STAR-001 §2 :31-34 / ALG-STARDET-001 :64,253-258, DISP-STAR-007):
//   检测块 FWHM_gauss = 2.3548200450309493*sigma, PSF 块 FWHM_moffat4 = 1.230310*sigma,
//   同 sigma 下相差 1.914005x, **两列禁止跨块比较/互换**。本块消费的是检测块列
//   (DATA-P1-SOURCES.fwhm_px) ⇒ 必须用**高斯因子**换算 sigma; 用 PSF 块因子 1.230310
//   反解会使 sigma 高估 1.914005x (CONFORM-SWEEP-1-001 修复面)。本块自身的 Moffat4
//   轮廓模型 FWHM 一律由 sigma 经 kMoffat4FwhmFactor 正向导出, 不与输入列比较。
//   sigma_sky_adu       [ADU]      逐像素空背景 rms
//   gain_e_per_adu      [e-/ADU]   未知传 <=0 (则不加源泊松项)
//   read_noise_e        [e-]       仅在 gain>0 时进入
//   snr_*               [1]        无量纲信噪比
//   sigma_f_*           [ADU]      通量不确定度
//   flux5_adu           [ADU]      5-sigma 点源极限通量
//   m5_mag              [mag]      5-sigma 点源深度 (ZP<=0 时 NaN)

// P8-SNR-LINUX: 相对包含, 使本 TU 可被任意目标 (含根图 astrocs_phase1_noise)
// 以 sources 直接编译, 无需为该目标额外注入 include 目录 (生产源零副本)。
#include "../include/snr_estimator.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace {

// log10↔ln 换算常数 kLn10: 本模块唯一定义点 = noise_model.cpp (V12-N-16；
// 此处原为逐位等值的复制字面量且全文件零引用, 已删除)
constexpr double kPi   = 3.14159265358979323846;
// 检测块母函数因子: FWHM = 2.3548200450309493 * sigma (椭圆高斯, TWO_SQRT_2_LOG2;
//   ALG-STARDET-001 §2 :63, SCI-P1-STAR-001 §5 :68)。DATA-P1-SOURCES.fwhm_px 属该块。
constexpr double kGaussFwhmFactor = 2.3548200450309493;
// PSF 块母函数因子: FWHM = 1.230310 * sigma (各向同性 Moffat4 beta=4; SCI-PSF-001 §5)。
//   只用于本块**自身 Moffat4 模型**的 sigma -> FWHM 正向导出 (网格/孔径半径);
//   **禁止**用它反解检测块输入的 fwhm_px (跨块比较, DISP-STAR-007)。
constexpr double kMoffat4FwhmFactor = 1.230310;
// 10-90% trimmed mean |residual| -> Gaussian sigma (noise_model.cpp:37 同源常数)
constexpr double kTrimMeanToSigma = 0.7316727929211932;

// 检测块高斯 FWHM -> sigma (与 snr_estimator.cpp 的 PSF 块路径 fwhm/1.230310 互斥:
// 两条路径的输入列来自不同块, 因子必须随数据来源选择, 不得混用)。
inline double detectionSigmaFromFwhm(double fwhm_px) {
    return fwhm_px / kGaussFwhmFactor;
}

// 离散归一化 Moffat4 beta=4 轮廓: I(r) = 1/(1 + r^2/(2 sigma^2))^4
// 在 (2*half+1)^2 单位像素网格上取 P_i = I_i / sum_j I_j (严格 sum P_i = 1)
// 输出:
//   out_sum_p2   = sum_i P_i^2                [1/pixel]
//   out_p_center = P at grid center           [1]
// 网格规则 (冻结, oracle 必须复现): 边长 half = (half_px>0)?half_px:max(30, ceil(12*fwhm))
inline void moffat4Discrete(double sigma_px, int half_px,
                            double* out_sum_p2, double* out_p_center) {
    const double alpha2 = 2.0 * sigma_px * sigma_px;  // 各向同性 Q = 0.5 r^2/sigma^2
    const int half = half_px;
    double sum = 0.0;
    double sum2 = 0.0;
    double center = 0.0;
    for (int j = -half; j <= half; ++j) {
        for (int i = -half; i <= half; ++i) {
            const double r2 = (double)i * (double)i + (double)j * (double)j;
            const double t = 1.0 + r2 / alpha2;
            const double t2 = t * t;
            const double v = 1.0 / (t2 * t2);   // t^-4
            sum += v;
            sum2 += v * v;
            if (i == 0 && j == 0) center = v;
        }
    }
    if (!(sum > 0.0)) {
        if (out_sum_p2) *out_sum_p2 = 0.0;
        if (out_p_center) *out_p_center = 0.0;
        return;
    }
    // sum_i (v_i/sum)^2 = sum2 / sum^2
    if (out_sum_p2) *out_sum_p2 = sum2 / (sum * sum);
    if (out_p_center) *out_p_center = center / sum;
}

inline int autoHalf(double fwhm_px) {
    int h = (int)std::ceil(12.0 * fwhm_px);
    if (h < 30) h = 30;
    if (h > 256) h = 256;  // 安全上界 (远超实际 PSF 尺度)
    return h;
}

}  // namespace

extern "C" {

// ============================================================================
// snr_moffat4_profile_f64 - 离散归一化 Moffat4 beta=4 轮廓统计 (oracle 锚)
// fwhm_px = 检测块高斯 FWHM (DATA-P1-SOURCES); sigma_px = 本块 Moffat4 sigma
// (fwhm_px>0 时优先, 按 kGaussFwhmFactor 换算)。half_px<=0 时按 autoHalf 规则取网格。
// 返回 0=成功, 3=非法参数。
// ============================================================================
SNR_API int snr_moffat4_profile_f64(double fwhm_px, double sigma_px, int half_px,
                                    double* out_sum_p2, double* out_p_center) {
    if (!out_sum_p2 || !out_p_center) return 3;
    double sigma = (fwhm_px > 0.0) ? detectionSigmaFromFwhm(fwhm_px) : sigma_px;
    if (!std::isfinite(sigma) || !(sigma > 0.0)) {
        *out_sum_p2 = 0.0;
        *out_p_center = 0.0;
        return 3;
    }
    // 网格/孔径用的 Moffat4 FWHM 由本块 sigma 正向导出 (非输入列本身)
    double fwhm_eff = sigma * kMoffat4FwhmFactor;
    const int half = (half_px > 0) ? half_px : autoHalf(fwhm_eff);
    moffat4Discrete(sigma, half, out_sum_p2, out_p_center);
    return 0;
}

// ============================================================================
// snr_source_snr_f64 - 逐源科学 SNR (Horne 1986 最优提取 + 孔径 CCD 方程)
//
// 输入宽度列: p->fwhm_px 属**检测块高斯** (FWHM=2.3548200450309493*sigma,
//   SCI-P1-STAR-001 §2), p->sigma_px 为本块 Moffat4 sigma; 二者同一 sigma 尺度,
//   禁止把 PSF 块的 Moffat4 FWHM 直接填入 fwhm_px (跨块混用, DISP-STAR-007)。
//
// 最优提取:
//   sigma_i^2 = sigma_sky^2 + max(F*P_i,0)/gain + (read_noise_e/gain)^2   [gain>0]
//   sigma_i^2 = sigma_sky^2 + (RN/g)^2 + F*P_i/g   [gain>0, sigma_sky=散粒]
//   sigma_i^2 = sigma_sky^2 + F*P_i/g              [gain>0, sigma_sky=经验总 rms(含读噪)]
//   sigma_i^2 = sigma_sky^2                                               [gain<=0]
//   Var(F) = 1 / sum_i (P_i^2/sigma_i^2) ; SNR_F = F / sqrt(Var(F))
//
// 孔径 (半径 r, 面积 n_pix=pi r^2, 天空环 n_sky):
//   f_in(r) = 1 - (1 + r^2/(2 sigma^2))^-3      (Moffat4 beta=4 解析 enclosed fraction)
//   S_ap    = F * f_in(r)
//   Var_ap  = S_ap/gain + n_pix*sigma_sky^2*(1 + n_pix/n_sky)   [Howell 1989 CCD 方程]
//   SNR_ap  = S_ap / sqrt(Var_ap) ; sigma_F,ap = sqrt(Var_ap)/f_in(r)  (含孔径改正)
// ============================================================================
SNR_API int snr_source_snr_f64(const SnrSourceParams* p, SnrSourceResult* out) {
    if (!p || !out) return 3;
    std::memset(out, 0, sizeof(SnrSourceResult));
    out->m5_mag = std::nan("");
    out->status = 1;

    double sigma = (p->fwhm_px > 0.0) ? detectionSigmaFromFwhm(p->fwhm_px) : p->sigma_px;
    if (!std::isfinite(sigma) || !(sigma > 0.0)) return 0;
    if (!std::isfinite(p->flux_adu) || !(p->flux_adu > 0.0)) return 0;
    if (!std::isfinite(p->sigma_sky_adu) || !(p->sigma_sky_adu > 0.0)) return 0;

    const double F = p->flux_adu;
    const double sig_sky = p->sigma_sky_adu;
    const double fwhm_eff = sigma * kMoffat4FwhmFactor;
    const int half = (p->profile_half_px > 0) ? p->profile_half_px : autoHalf(fwhm_eff);

    // --- 最优提取 (Horne 1986) ---
    // gain<=0: sigma_i 均匀 -> Var(F) = sigma_sky^2 / sum P_i^2
    // gain>0 : 逐像素含源泊松项
    double var_f = 0.0;
    double sum_p2 = 0.0;
    double p_center = 0.0;
    if (p->gain_e_per_adu > 0.0) {
        const double alpha2 = 2.0 * sigma * sigma;
        // SCI-B D1 定案 (07_noise_snr.md 4.2a): sigma_sky 声明为经验总 rms(含读噪)时
        // **不得**再加 (RN/g)^2 —— 否则读噪双计, sigma_F 高估 +12.8%~+34.0%。
        const bool rn_in_sky = (p->sigma_sky_source == SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS);
        const double rn_term = (!rn_in_sky && p->read_noise_e > 0.0)
                                   ? (p->read_noise_e / p->gain_e_per_adu) *
                                         (p->read_noise_e / p->gain_e_per_adu)
                                   : 0.0;
        out->sigma_sky_source_effective = rn_in_sky ? 2 : 1;
        double sum = 0.0;
        std::vector<double> v;
        v.reserve((size_t)(2 * half + 1) * (2 * half + 1));
        for (int j = -half; j <= half; ++j) {
            for (int i = -half; i <= half; ++i) {
                const double r2 = (double)i * (double)i + (double)j * (double)j;
                const double t = 1.0 + r2 / alpha2;
                const double t2 = t * t;
                const double val = 1.0 / (t2 * t2);
                v.push_back(val);
                sum += val;
            }
        }
        if (!(sum > 0.0)) return 0;
        for (size_t k = 0; k < v.size(); ++k) {
            const double Pi = v[k] / sum;
            const double si = F * Pi;
            double var_i = sig_sky * sig_sky + rn_term;
            if (si > 0.0) var_i += si / p->gain_e_per_adu;
            var_f += (Pi * Pi) / var_i;
            sum_p2 += Pi * Pi;
            if (k == v.size() / 2) p_center = Pi;  // 中心像素 (奇数网格)
        }
        var_f = (var_f > 0.0) ? (1.0 / var_f) : 0.0;
    } else {
        moffat4Discrete(sigma, half, &sum_p2, &p_center);
        var_f = (sum_p2 > 0.0) ? (sig_sky * sig_sky / sum_p2) : 0.0;
        out->sigma_sky_source_effective = 3;   // gain<=0: 天空受限, (RN/g)^2 不可加
    }
    if (!(var_f > 0.0)) return 0;
    out->sum_p2 = sum_p2;
    out->sigma_f_optimal_adu = std::sqrt(var_f);
    out->snr_optimal = F / out->sigma_f_optimal_adu;
    out->snr_peak = F * p_center / sig_sky;
    out->flux5_adu = 5.0 * out->sigma_f_optimal_adu;

    // --- 孔径路径 (CCD 方程 + Moffat4 孔径改正) ---
    const double r = (p->aperture_radius_px > 0.0) ? p->aperture_radius_px
                                                   : 1.5 * fwhm_eff;
    const double alpha2 = 2.0 * sigma * sigma;
    const double u = 1.0 + (r * r) / alpha2;
    const double f_in = 1.0 - 1.0 / (u * u * u);  // 1 - u^-3
    const double n_pix = kPi * r * r;
    const double n_sky = (p->n_sky > 0.0) ? p->n_sky : n_pix;
    const double s_ap = F * f_in;
    double var_ap = n_pix * sig_sky * sig_sky * (1.0 + n_pix / n_sky);
    if (p->gain_e_per_adu > 0.0 && s_ap > 0.0) var_ap += s_ap / p->gain_e_per_adu;
    out->n_pix = n_pix;
    out->enclosed_fraction = f_in;
    out->aperture_correction = (f_in > 0.0) ? (1.0 / f_in) : 0.0;
    if (var_ap > 0.0) {
        out->sigma_f_aperture_adu = (f_in > 0.0) ? (std::sqrt(var_ap) / f_in) : 0.0;
        out->snr_aperture = s_ap / std::sqrt(var_ap);
    }

    // --- 5-sigma 深度 ---
    if (p->zero_point_mag != 0.0 && std::isfinite(p->zero_point_mag) &&
        out->flux5_adu > 0.0) {
        out->m5_mag = p->zero_point_mag - 2.5 * std::log10(out->flux5_adu);
    }
    out->status = 0;
    return 0;
}

// ============================================================================
// snr_frame_depth_f64 - 帧级科学基准 = 5-sigma 点源深度
// F_5 = 5*sigma_F(ref)  [ADU]; m_5 = ZP - 2.5*log10(F_5)  [mag]
// sigma_F(ref) 必须来自显式参考源 (reference 结果), 禁止用整帧标量替代。
// ============================================================================
SNR_API int snr_frame_depth_f64(const SnrSourceResult* reference,
                                double zero_point_mag,
                                double* out_flux5_adu,
                                double* out_m5_mag) {
    if (!reference) return 3;
    const double f5 = 5.0 * reference->sigma_f_optimal_adu;
    if (out_flux5_adu) *out_flux5_adu = f5;
    if (out_m5_mag) {
        if (zero_point_mag != 0.0 && std::isfinite(zero_point_mag) && f5 > 0.0) {
            *out_m5_mag = zero_point_mag - 2.5 * std::log10(f5);
        } else {
            *out_m5_mag = std::nan("");
        }
    }
    return 0;
}

// ============================================================================
// snr_calib_zero_point_standard_error - 零点标准误 (dex)
//   sigma_kappa,stat ~ 1.253 * sigma_residual / sqrt(N)
// 高斯下 median 的标准误常数 1.253 = sqrt(pi/2)。sigma_residual 为逐星定标散度,
// 不得直接当作零点误差 (N=200 时高估约 11x)。
// ============================================================================
SNR_API double snr_calib_zero_point_standard_error(double sigma_logflux_dex,
                                                   int n_matches) {
    if (!std::isfinite(sigma_logflux_dex) || !(sigma_logflux_dex > 0.0)) return 0.0;
    if (n_matches <= 0) return 0.0;
    return 1.253 * sigma_logflux_dex / std::sqrt((double)n_matches);
}

}  // extern "C"
