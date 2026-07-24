#ifndef PC_SPECTRUM_INTEGRATOR_H
#define PC_SPECTRUM_INTEGRATOR_H

// spectrum_integrator.h - 光谱积分器
// 功能: 对 Gaia BP/RP uint8 光谱做 Akima 插值 + Simpson 1/3 积分, 得到合成流量 F_syn
// 参考: lib/photometric_calib/spectrum_integrator/python/synthetic_photometry.py
//   F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ × 10^(-0.4*mag_g)
//   S(λ): uint8 光谱转 float64
//   T(λ): 滤光片透过率 (Akima 插值重采样到 SED 波长网格)
//   Q(λ): CCD QE 曲线 (Akima 插值重采样到 SED 波长网格, 可为 nullptr -> Q=1.0)
//   λ:    波长加权 (CCD 光子数 ∝ λ × 能量流量)

#include <cstdint>
#include <vector>

namespace photo_calib {

// Akima 子样条插值 (参考 scipy.interpolate.Akima1DInterpolator)
// 超出 x_src 范围的 x_dst 点返回 fill
// 输入: x_src 必须严格单调递增
std::vector<double> akima_interpolate(
    const std::vector<double>& x_src, const std::vector<double>& y_src,
    const std::vector<double>& x_dst, double fill = 0.0);

// Simpson 1/3 复合积分 (等间距网格)
// 当区间数为奇数时, 末尾 3 个区间用 Simpson 3/8 公式 (与 scipy.integrate.simpson 一致)
// 输入: x 必须严格单调递增且等间距
double simpson_integrate(
    const std::vector<double>& x, const std::vector<double>& y);

// 计算单颗星合成流量 F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ × 10^(-0.4*mag_g)
// 参数:
//   spectrum_uint8: Gaia BP/RP uint8 光谱 (长度 = spectrum_count)
//   spectrum_count: 光谱点数 (通常 343)
//   spectrum_wl:    光谱波长数组 [336, 338, ..., 1020] nm (长度 = wl_count)
//   wl_count:        波长数组长度 (必须 == spectrum_count)
//   filter_wl:       滤光片波长数组 (nm)
//   filter_trans:    滤光片透过率 [0,1]
//   filter_count:    滤光片点数
//   qe_wl:           CCD QE 波长数组 (nm), 可为 nullptr (此时 Q(λ)=1.0)
//   qe_trans:        CCD QE 透过率数组 [0,1], qe_wl 为 nullptr 时忽略
//   qe_count:        QE 点数, qe_wl 为 nullptr 时可为 0
//   mag_g:           Gaia G 星等 (用于绝对通量归一化)
// 返回: F_syn 标量, 失败返回 0.0
double compute_f_syn(
    const uint8_t* spectrum_uint8, int spectrum_count,
    const double* spectrum_wl, int wl_count,
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    double mag_g);

// ============================================================================
// 滤光片曲线预处理缓存 (Task 11)
// 循环前预处理一次, 循环内只算 SED + 星等归一化 + 积分
// ============================================================================
struct SpectrumIntegratorCache {
    std::vector<double> spectrum_wl;  // 光谱波长网格 [336, 338, ..., 1020] nm
    std::vector<double> filter_trans; // 滤光片透过率重采样到光谱网格
    std::vector<double> qe_trans;     // CCD QE 透过率重采样到光谱网格 (无 QE 时为空)
    std::vector<double> weighted_wl;  // λ × T(λ) × Q(λ) 预计算 (积分核的一部分)
};

// 预处理滤光片+QE 曲线, 缓存重采样结果 (Task 11 + GAP-012)
// 在 OpenMP 循环前调用一次, 避免每颗星重复排序 + Akima 插值
// 参数:
//   filter_wl:       滤光片波长数组 [filter_count]
//   filter_trans:    滤光片透过率数组 [filter_count]
//   filter_count:    滤光片点数
//   qe_wl:           CCD QE 波长数组 [qe_count], 可为 nullptr (此时 Q(λ)=1.0)
//   qe_trans:        CCD QE 透过率数组 [qe_count], qe_wl 为 nullptr 时忽略
//   qe_count:        QE 点数, qe_wl 为 nullptr 时可为 0
//   spectrum_wl:     光谱波长数组 [spectrum_count]
//   spectrum_count:  光谱波长数
// 返回: SpectrumIntegratorCache, 失败时各数组为空
SpectrumIntegratorCache prepare_filter_cache(
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    const double* spectrum_wl, int spectrum_count);

// 带缓存的 F_syn 计算 (Task 11)
// 循环内调用, 复用预处理的滤光片缓存, 只做 SED + 星等归一化 + 积分
// 参数:
//   cache:           预处理的滤光片缓存 (来自 prepare_filter_cache)
//   spectrum_uint8:  uint8 光谱数据 [spectrum_count]
//   spectrum_count:  光谱点数 (必须 == cache.spectrum_wl.size())
//   mag_g:           Gaia G 星等
// 返回: F_syn 值, 失败返回 0.0
double compute_f_syn_cached(
    const SpectrumIntegratorCache& cache,
    const uint8_t* spectrum_uint8, int spectrum_count,
    double mag_g);

} // namespace photo_calib

#endif // PC_SPECTRUM_INTEGRATOR_H
