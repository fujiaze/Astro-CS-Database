#ifndef PC_SPECTRUM_INTEGRATOR_H
#define PC_SPECTRUM_INTEGRATOR_H
// 契约锚: docs/algorithms/PHOTOMETRIC_FIT.md (ALG-PHOTOMETRIC-FIT-*) + docs/science/PHOTOMETRY.md (SCI-PHOT-001) + lib/algorithms/photometry/docs/algorithm.md §3 ; 数值: Akima 子样条(范围外 fill=0, 严格递增)+Simpson 1/3 复合(奇数区间末3用 3/8, 等间距) — NUMERIC/COMMENT 锚点, 实现见 spectrum_integrator.cpp akima_interpolate/simpson_integrate/compute_f_syn*.

// spectrum_integrator.h - 光谱积分器
// 功能: 对 Gaia BP/RP 采样谱做 Akima 插值 + Simpson 1/3 积分, 得到参考合成通量 F_syn
// 参考: lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/fsyn_astrocs.py
// (numpy 移植, 与本文件 Akima+Simpson 数值等价; 旧引 lib/algorithms/photometry/
//  spectrum_integrator/python/synthetic_photometry.py 全仓不存在 — L28c-E-002 订正)
//
// ── 参考通量口径 (权威: docs/science/PHOTOMETRY.md §2a, claim PHOT-FSYN-CANON-001) ──
// 生产口径 (唯一权威写法, 见 compute_f_syn_cached_xpsd):
//     F_syn = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ                       [W·m⁻²·nm]
//     F_λ(λ_i) = byte_i·flux_mul + flux_min                  [W·m⁻²·nm⁻¹]  (XPSD 官方解码)
//   **不含** 10^(-0.4·magG) 因子: magG 既不进入 F_syn, 也不作归一化。
//   量纲: W·m⁻²·nm⁻¹ · 1 · 1 · nm · nm = W·m⁻²·nm; 物理含义 F_syn = hc·N_γ (光子计数率 × hc)。
//   T(λ): 滤光片透过率 [0,1] (Akima 重采样到谱网格, 区间外 0)
//   Q(λ): CCD QE 曲线 [0,1] (同上; 可为 nullptr -> Q≡1.0, 显式未建模项)
//   λ:    光子计数权重 (CCD 计量光子数; 单位波长间隔光子数 ∝ F_λ·λ/(hc), 1/(hc) 被零点吸收)
//   与官方定义 (Gaia DR3 文档 §5.4.1 式 5.41) 的差别只是去掉了与星无关的归一化分母
//   ∫T(λ)Q(λ)λdλ —— 该分母是逐帧常数, 被 location/ZP_syn 吸收。
//
// 退化 (不得给出数值, 见 PHOTOMETRY.md §2a.1/§8):
//   T·Q 在整个谱网格上为 0 (通带不重叠 / QE 曲线落在网格外) => F_syn ≡ 0;
//   flux_mul<=0 或量化参数非有限 => 显式返回 0;
//   解码后 F_λ 含负值使积分 F_syn<=0 => 函数不钳位, 由调用方 F_syn>0 有效域判据拒绝。
//
// 非生产通道 (保留供数值对拍, **不得**用于生产定标): compute_f_syn / compute_f_syn_cached
//   把 uint8 当"与星无关的相对谱形"再乘 10^(-0.4·magG)。该写法给逐星 r_i 注入
//   +0.4·G_i (dex) 的加性项, 单标量 location 吸收不掉 (真实 M42 样本上 MAD-σ = 0.459 dex
//   = 1.147 mag, run/SCI-PHOT-FORMULA-01/evidence/c2_negative_controls.json → N2);
//   且 XPSD 的量化参数 flux_min/flux_mul **逐星不同**, uint8 数组本身不是可比的相对谱形。

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

// [非生产通道] 计算单颗星相对合成流量 F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ × 10^(-0.4*mag_g)
// 仅用于与历史口径的数值对拍; 生产定标必须走 compute_f_syn_cached_xpsd (见文件头口径块)。
// 参数:
// spectrum_uint8: Gaia BP/RP uint8 光谱 (长度 = spectrum_count)
// spectrum_count: 光谱点数 (通常 343)
// spectrum_wl: 光谱波长数组 [336, 338, ..., 1020] nm (长度 = wl_count)
// wl_count: 波长数组长度 (必须 == spectrum_count)
// filter_wl: 滤光片波长数组 (nm)
// filter_trans: 滤光片透过率 [0,1]
// filter_count: 滤光片点数
// qe_wl: CCD QE 波长数组 (nm), 可为 nullptr (此时 Q(λ)=1.0)
// qe_trans: CCD QE 透过率数组 [0,1], qe_wl 为 nullptr 时忽略
// qe_count: QE 点数, qe_wl 为 nullptr 时可为 0
// mag_g: Gaia G 星等 (此通道按历史口径作乘性归一; 生产口径不使用该参数)
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

// 预处理滤光片+QE 曲线, 缓存重采样结果 (Task 11 +)
// 在 OpenMP 循环前调用一次, 避免每颗星重复排序 + Akima 插值
// 参数:
// filter_wl: 滤光片波长数组 [filter_count]
// filter_trans: 滤光片透过率数组 [filter_count]
// filter_count: 滤光片点数
// qe_wl: CCD QE 波长数组 [qe_count], 可为 nullptr (此时 Q(λ)=1.0)
// qe_trans: CCD QE 透过率数组 [qe_count], qe_wl 为 nullptr 时忽略
// qe_count: QE 点数, qe_wl 为 nullptr 时可为 0
// spectrum_wl: 光谱波长数组 [spectrum_count]
// spectrum_count: 光谱波长数
// 返回: SpectrumIntegratorCache, 失败时各数组为空
SpectrumIntegratorCache prepare_filter_cache(
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    const double* spectrum_wl, int spectrum_count);

// [非生产通道] 带缓存的相对 F_syn 计算 (Task 11)
// 循环内调用, 复用预处理的滤光片缓存, 只做 SED + 星等归一化 + 积分。
// 语义与 compute_f_syn 相同 (含 10^(-0.4*mag_g)); 生产定标用 compute_f_syn_cached_xpsd。
// 参数:
// cache: 预处理的滤光片缓存 (来自 prepare_filter_cache)
// spectrum_uint8: uint8 光谱数据 [spectrum_count]
// spectrum_count: 光谱点数 (必须 == cache.spectrum_wl.size)
// mag_g: Gaia G 星等 (此通道的乘性归一)
// 返回: F_syn 值, 失败返回 0.0
double compute_f_syn_cached(
    const SpectrumIntegratorCache& cache,
    const uint8_t* spectrum_uint8, int spectrum_count,
    double mag_g);

// ============================================================================
// XPSD 官方解码变体 —— **生产定标路径** (Phase1 Final Closure, 2026-08-08)
// PCL GaiaDatabaseFile::EncodedStarSPData:
// flux_min / flux_mul 为每星记录内 float32 量化参数,
// F(λ) = byte*flux_mul + flux_min (W*m^-2*nm^-1, 绝对谱辐照度)
// F_syn = ∫ F(λ)·T(λ)·Q(λ)·λ dλ   (W·m⁻²·nm; **不含** 10^(-0.4·G))
// 绝对刻度实证 (26211 颗真实 XPSD 星, 官方 Gaia G 通带 + GaiaXPy 零点 -26.4899):
//   median(m_syn - magG) = -0.0037 mag, MAD = 0.0033 mag (G∈[6,18], n=11272);
//   乘 10^(-0.4·G) 的变体会偏移 +16.34 mag。
//   证据: run/SCI-PHOT-FORMULA-01/evidence/a1_xpsd_absolute_check.json
// 适用域: G ≲ 18; 通带(含 Q)必须与谱网格有非零重叠, 否则 F_syn≡0 (退化, 由调用方拒绝)。
// ============================================================================
double compute_f_syn_cached_xpsd(
    const SpectrumIntegratorCache& cache,
    const uint8_t* spectrum_uint8, int spectrum_count,
    double flux_min, double flux_mul);

} // namespace photo_calib

#endif // PC_SPECTRUM_INTEGRATOR_H
