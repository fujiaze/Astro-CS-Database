#ifndef SNR_ESTIMATOR_H
#define SNR_ESTIMATOR_H

#include <cstdint>

#ifdef _WIN32
#define SNR_API __declspec(dllexport)
#else
#define SNR_API
#endif

// ============================================================================
// C ABI 版本化 (claim SC-009 / MASK-002；先例 = SCI-FIX-AIO SC-006 的
// struct_size/abi_version 原位版本化纪律)
// ----------------------------------------------------------------------------
// 所有跨边界结构首部两个 uint32_t: struct_size @0 (= 调用方编译期 sizeof) +
// abi_version @4 (= 本文件对应版本常量)。公共入口**逐元素**校验二者, 失配即
// fail-closed 返回 SNR_ABI_MISMATCH(-9), 不进入科学路径。
// 无头部 (全零 = 未版本化的旧调用方) 一律拒绝: 掩膜语义已由「统一半径」升级为
// 「逐星半径 + 天空预算收缩」(SCI-NOISE-001 §5a), 静默兼容会把旧语义当新语义消费。
// ============================================================================
#define SNR_ABI_MISMATCH               (-9)
#define SNR_NOISE_CONFIG_ABI_VERSION   1u
#define SNR_NOISE_MODEL_ABI_VERSION    1u
#define SNR_ABI_HEADER(TYPE, VERSION)      \
    uint32_t struct_size; /* = sizeof(TYPE) @0 */ \
    uint32_t abi_version; /* = VERSION @4 */

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SNR/Noise 科学重构 — 三层模型 (SNR_SCIENCE_DERIVATION.md / SNR_REDESIGN_CONTRACT.md)
//
// 旧乘法模型 (SNR_phot × SNR_psf/median + IDW) 已降级为 legacy heuristic /
// diagnostic only (见 snr_extract_model_* 与 snr_estimate_*), 不再作为生产科学权重。
// 术语口径 (SCI-PSF §2 / SNR-008): PSF 块第 8 列 (index 7) 历史称 "mad",
// 实为 10-90% trimmed mean absolute residual, 权威语义 residual_scale;
// 旧 (A-B)/mad 控制点公式即已退休 (SNR-008) 的 legacy 路径, 本头文件中
// "mad" 字样均为该列历史别名, 不指 median-absolute-deviation。
// 拆分为:
// 1. PhotometricCalibrationQuality — 帧级测光定标质量 (systematic metadata)
// 2. PsfFitQuality — 星点级 PSF 拟合质量代理 (QA/剔星)
// 3. NoiseWeightModelV1 — source-masked blank-sky 稳健方差 → ivar
// (Phase2 科学加权唯一来源)
// ============================================================================

// ---------------------------------------------------------------------------
// 1. PhotometricCalibrationQuality
// 单位: sigma_residual 来自测光定标 r_i = log10(F_instr/F_syn) 的稳健散度,
// 单位为 dex (log10 flux-ratio)。它不是 mag, 也不是像素随机噪声 σ。
// sigma_mag = 2.5 × sigma_logflux_dex (mag 空间)
// sigma_cal_rel ≈ ln(10) × sigma_logflux_dex (相对通量散度)
// 用途: QA / frame flag / calibration systematic metadata;
// 禁止当作逐像素 inverse-variance 权重。
// ---------------------------------------------------------------------------
typedef struct {
    double sigma_logflux_dex;  // 测光残差散度 (dex / log10 flux-ratio) — 逐星散度, 非零点误差
    double sigma_mag;          // 2.5 × dex (mag) — 逐星散度
    double sigma_cal_rel;      // ln(10) × dex (相对标定散度, 无量纲) — 逐星散度
    // P5-SNR 订正 (2026-09-14, 负责人授权; SCI-PHOT-001 / PHOTOMETRY_LITERATURE_REVIEW C.2.2):
    // sigma_residual 是参考星样本的散射, 零点标准误必须除以 sqrt(N):
    //   sigma_location_se_dex ≈ 1.253 * sigma_logflux_dex / sqrt(n_matches)
    double sigma_location_se_dex;  // 零点标准误 (dex); N<=0 或 sigma<=0 时为 0
    double sigma_location_se_mag;  // 2.5 × sigma_location_se_dex (mag)
    int    n_matches;          // 匹配 Gaia 星数
    int    fit_status;         // 0=ok, 1=degenerate (无有效匹配), 2=invalid input
} PhotometricCalibrationQuality;

// 从测光定标残差 sigma (dex) 构造 PhotometricCalibrationQuality。
// sigma_logflux_dex <= 0 或非有限 → fit_status=2, 各 sigma 置 0。
SNR_API int snr_phot_cal_quality(double sigma_logflux_dex, int n_matches,
                                 PhotometricCalibrationQuality* out);

// ---------------------------------------------------------------------------
// 2. PsfFitQuality
// 每星 PSF 拟合质量代理。psf 行布局为冻结的 [N,9]:
// status(0) B(1) flux(2) cx(3) cy(4) fwhm(5) A(6) mad(7) eccentricity(8)
// 第 8 列 (index 7) 历史称 "mad", 实际是 10-90% trimmed mean absolute residual
// (非真 MAD), 权威语义 residual_scale,
// 对 Gaussian N(0,σ²) 期望 ≈ 0.731673 σ。
// 本接口准确重命名语义并输出:
// residual_scale — 原统计量 (不称 MAD)
// robust_residual_sigma — residual_scale / 0.731673 (Gaussian 假设)
// q_psf — amplitude_above_bg / residual_scale (拟合质量代理)
// 注意: q_psf 是 fit-quality proxy, 不是图像噪声 SNR, 默认不作为
// Phase2 逐像素 science weight。
// ---------------------------------------------------------------------------
typedef struct {
    double flux;                // PSF 通量
    double amplitude_above_bg;  // A (背景以上峰值振幅)
    double background;          // B (局部背景)
    double fwhm;                // 半高全宽
    double eccentricity;        // 椭率
    double residual_scale;      // trimmed-mean-abs residual (原 mad 列, 准确语义)
    double robust_residual_sigma; // Gaussian 假设下 ≈ residual_scale/0.731673
    double q_psf;               // A / residual_scale (fit-quality proxy)
    uint32_t fit_status;        // 0=ok, 1=rejected(status!=0), 2=saturated/quality flag,
                                // 3=invalid input 行
} PsfFitQualityRow;

// 从 PSF 块 [N,9] 计算每星 PsfFitQuality。
// star_ids / quality_flags 可空 (空则填 0)。
// out 至少 n_stars 行。返回 0=成功, 3=nullptr 参数。
SNR_API int snr_psf_fit_quality(const double* psf, int n_stars,
                                const int64_t* star_ids,
                                const uint32_t* quality_flags,
                                PsfFitQualityRow* out);

// ---------------------------------------------------------------------------
// 3. NoiseWeightModelV1 — 合同锚点: docs/algorithms/NOISE_ESTIMATION.md + docs/science/NOISE_MODEL.md
// source-masked blank-sky 稳健方差 (production 基线)。
// 控制点来自空背景噪声, 与星亮度/星族解耦 (SNR-003/SNR-010)。
// 默认 patch grid 扫描校准帧:
// - 星点掩膜 (fixed conservative：统一半径，不按振幅； 冻结) +
// 饱和/边缘排除
// - patch 内 robust location (median) + robust scale
// σ_bg = 1.482602218505602 × median(|x − median(x)|)
// - 合格 patch 成为控制点, 可选最小二乘平面空间方差场
// - 全局兜底 = 合格 patch 的 median variance
// gain/read-noise 已知时可交叉验证 Poisson+read 模型 (SNR-005),
// 缺失时经验 fallback (SNR-014)。
// ---------------------------------------------------------------------------
typedef struct {
    SNR_ABI_HEADER(SnrNoiseModelConfig, SNR_NOISE_CONFIG_ABI_VERSION)
    int    patch_grid_x;         // 每边 patch 数 (默认 8, >=2)
    int    patch_grid_y;         // 每边 patch 数 (默认 8, >=2)
    double source_mask_radius_px;   // 星点固定保守掩膜半径 (默认 10 px)
    double mask_radius_scale;       // 固定半径乘数 (默认 6.0 → 统一 rmax=60 px)
    double gain_e_per_adu;          // 增益 e-/ADU (0=未知, 默认 0)
    double read_noise_e;            // 读出噪声 e- (0=未知, 默认 0)
    double saturation_level;        // 饱和电平 ADU (0/负/非有限 = 未提供电平 unset, 默认 0;
                                    //   != "无饱和"; 来源优先级 cfg>SATURATE>DATAMAX,
                                    //   未提供时调用方须显式声明降级 -- SCI NOISE_MODEL §4
                                    //   「饱和域」claim SC-008, 见 astrocs/noise/saturation_policy.h)
    double cosmic_clip_sigma;       // patch 内 cosmic/hot 稳健裁剪 (默认 5.0)
    int    min_patch_samples;       // patch 合格最小 sky 样本数 (默认 64)
    int    max_clip_rounds;         // cosmic 裁剪轮数 (默认 2)
    uint32_t use_gain_model;        // 1=gain+readnoise 已知时优先模型; 默认 0 (经验优先)
    uint32_t enable_spatial_field;  // 1=最小二乘平面空间方差场 (默认 1)
    double   variance_floor;        // ivar 分母下限 (默认 1e-12)
    // --- MASK-002 (claim SC-009): 逐星掩膜半径参数, 权威 = SCI-NOISE-001 §5a ---
    //   r_i = clip(r_local(F_i, FWHM_i, k·σ_bg), r_min, rmax)
    //   r_max = max(1,source_mask_radius_px)·max(1,mask_radius_scale) 为**硬上界**
    double   mask_k_sigma;            // 掩膜边缘残余面亮度系数 k (默认 0.1σ_bg)
    double   mask_r_min_px;           // 逐星半径下界绝对项 (默认 1.5 px)
    double   mask_fwhm_floor_scale;   // r_min 的 FWHM 项系数 (默认 0.75)
    uint32_t mask_budget_min_patches; // 天空预算: 合格 patch 数下限 (默认 8)
    uint32_t mask_budget_min_sky;     // 天空预算: 未掩膜合法像素数下限 (默认 9216)
} SnrNoiseModelConfig;

// 默认噪声模型配置
SNR_API int snr_noise_model_v1_default_config(SnrNoiseModelConfig* cfg);

typedef struct {
    SNR_ABI_HEADER(NoiseWeightModelV1, SNR_NOISE_MODEL_ABI_VERSION)
    uint32_t n_control_points;   // 合格 patch 数
    double*  ctrl_x_px;          // patch 中心 x (0-based)
    double*  ctrl_y_px;          // patch 中心 y
    double*  ctrl_sigma;         // patch 稳健 σ
    double*  ctrl_variance;      // σ²
    double*  ctrl_ivar;          // 1/σ²
    double   sigma_bg_global;    // 全局兜底 σ
    double   variance_bg_global; // 全局兜底 variance
    double   ivar_bg_global;     // 全局兜底 ivar
    uint32_t n_qualified_patches;
    uint32_t n_rejected_patches; // 样本不足/饱和/非有限
    uint8_t  source;             // 0=empirical blank-sky, 1=gain+readnoise model,
                                 // 2=mixed (control=empirical, fill 用模型)
    uint8_t  has_spatial_field;  // 1=合格 patch>=4 且控制点几何张成二维 (见 plane_geometry_ratio)
    uint8_t  degenerate;         // 1=无合格 patch, 全局兜底也退化 (ivar=0)
    uint8_t  reserved;
    // --- MASK-002 (claim SC-009) 掩膜诊断标, 消费方可 fail-closed ---
    uint32_t mask_degraded;      // 位标: bit0=1 MASK_LEGACY (无 flux/FWHM, 退回统一 rmax 或 4·FWHM);
                                 //       bit1=1 MASK_DEGRADED (天空预算收缩生效后 n_qualified<预算 或 全局兜底)
    double   mask_radius_p50;    // 逐星半径中位数 (px); 统一半径/手工掩膜通道 = 0
    double   mask_frac;          // 掩膜覆盖比 = 掩膜像素数 / (h*w); 无掩膜 = 0
} NoiseWeightModelV1;

// 从校准帧估计 blank-sky 稳健方差模型。
// data: FLOAT32 [h*w] (校准后, ADU 空间); source_mask 可空;
// star_x/star_y: 星点像素坐标 (0-based), n_stars 可 0;
// star_flux/star_fwhm: **MASK-002 新增可选逐星数组** (与 star_x/y 同序等长,
//   flux=ADU, fwhm=px; 生产调用点 = psf 块 row[2]/row[5]); 传 NULL 时按
//   SCI §5a 回调规则降级并置 MASK_LEGACY 位标 (不静默);
// cfg: 可空 (=默认配置); 非空时其 struct_size/abi_version 必须匹配, 否则 -9;
// out_model: 调用者用 snr_noise_model_v1_free 释放。
// 返回 0=成功 (含全局兜底), 1=完全退化 (ivar=0, 调用方应拒绝加权),
// SNR_ABI_MISMATCH(-9)=cfg ABI 头部失配 (fail-closed), 3=nullptr / 非法尺寸。
SNR_API int snr_noise_model_v1(const float* data, int h, int w,
                               const float* source_mask,
                               const double* star_x, const double* star_y,
                               const double* star_flux, const double* star_fwhm,
                               int n_stars,
                               const SnrNoiseModelConfig* cfg,
                               NoiseWeightModelV1* out_model);

// FP64 变体 (与 v1 语义一致, data 为 double)
SNR_API int snr_noise_model_v1_f64(const double* data, int h, int w,
                                   const float* source_mask,
                                   const double* star_x, const double* star_y,
                                   const double* star_flux, const double* star_fwhm,
                                   int n_stars,
                                   const SnrNoiseModelConfig* cfg,
                                   NoiseWeightModelV1* out_model);

// ---------------------------------------------------------------------------
// ABI 头部助手 (claim SC-009): 调用方自建结构体时必须先 stamp 再传入;
// 校验函数返回 0=匹配, SNR_ABI_MISMATCH(-9)=失配。
// ---------------------------------------------------------------------------
SNR_API void snr_noise_model_v1_abi_stamp_config(SnrNoiseModelConfig* cfg);
SNR_API void snr_noise_model_v1_abi_stamp_model(NoiseWeightModelV1* model);
SNR_API int  snr_noise_model_v1_abi_check_config(const SnrNoiseModelConfig* cfg);
SNR_API int  snr_noise_model_v1_abi_check_model(const NoiseWeightModelV1* model);

// 填充逐像素 variance / ivar (FLOAT32 输出; 可空任一输出)。
// 空间场启用且有 >=4 控制点 → 最小二乘平面 var(x,y)=a+b·x+c·y
// （负预测 clamp 到 variance_floor）；否则全局常量。 冻结。
// 返回 0=成功, 3=nullptr/尺寸非法, SNR_ABI_MISMATCH(-9)=model ABI 头部失配,
// SNR_FLOOR_UNBOUND(-10)=模型未绑定 variance_floor（FIX-405 G3-6 fail-closed：
//   原实现对此静默回退 1e-12，使配置的 variance_floor 在生产 fill 路径上被无声
//   忽略；现改为显式拒绝，调用方须经 build 或
//   snr_noise_model_v1_bind_variance_floor 显式绑定）。
SNR_API int snr_noise_model_v1_fill(const NoiseWeightModelV1* model,
                                    int h, int w,
                                    float* out_variance, float* out_ivar);

// ---------------------------------------------------------------------------
// variance_floor 显式绑定 + 钳位触发计数 (FIX-405 G3-6: 钳位 fail-open → fail-closed)
//
// 语义（不改变任何科学数值）:
//   * bind: 把 floor 登记为该模型的 fill 下限（与 build 内部登记同一注册表）。
//     floor 必须有限且 > 0，否则返回 SNR_FLOOR_UNBOUND(-10) 显式拒绝
//     （禁 NaN/≤0 静默通过）。
//   * clamp_count: 返回该模型 build 期间被 floor 钳位的数值个数
//     （全局兜底 + 逐控制点；0 = 未触发）。触发即计数，消费方可显式登记状态。
// ---------------------------------------------------------------------------
#define SNR_FLOOR_UNBOUND (-10)
SNR_API int snr_noise_model_v1_bind_variance_floor(NoiseWeightModelV1* model,
                                                   double floor_var);
SNR_API int64_t snr_noise_model_v1_floor_clamp_count(
    const NoiseWeightModelV1* model);

// 释放模型内部数组
SNR_API void snr_noise_model_v1_free(NoiseWeightModelV1* model);

// ---------------------------------------------------------------------------
// 噪声传播法则 (SNR-002)
// x' = α x → Var(x') = α² Var(x), ivar' = ivar / α²
// ---------------------------------------------------------------------------
SNR_API void snr_noise_scale_law(double alpha,
                                 double* variance, double* ivar);

// Poisson+read-noise 方差模型 (ADU 空间, signal 单位 ADU):
// var_ADU = max(signal,0)/gain + (read_noise_e/gain)²
SNR_API double snr_noise_gain_variance(double signal,
                                       double gain_e_per_adu,
                                       double read_noise_e);

// ============================================================================
// P5-SNR 逐源科学 SNR (2026-09-14, 负责人授权修改冻结/科学文档)
//
// 依据 (逐字锚): run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md
//   §C.3.1 Horne 1986 最优提取  Var(F)^-1 = sum_i P_i^2/sigma_i^2, SNR_F = F/sigma_F
//   §C.3.1 孔径 CCD 方程 (Howell 1989) + Moffat4 beta=4 孔径改正 (growth curve)
//   §C.3.1 帧级科学基准 = 5-sigma 点源深度 m_5 = ZP - 2.5*log10(5*sigma_F(ref))
//   §C.2.2 零点标准误 sigma_kappa,stat ~ 1.253*sigma_residual/sqrt(N)
//   §C.3.3 旧 snr_phot=1/(ln10*sigma_residual) 与真值之比跨 3 个数量级 (非 SNR)
//   §A.4.5 旧 (A-B)/residual_scale 已由 SNR-008 宣布退休
//
// 单位/量纲 (强制, 见 §C.3.1):
//   flux_adu [ADU];
//   fwhm_px [pixel] = **检测块**椭圆高斯 FWHM (DATA-P1-SOURCES; FWHM =
//     2.3548200450309493*sigma, SCI-P1-STAR-001 §2 / ALG-STARDET-001 §2);
//   sigma_px [pixel] = 本块 Moffat4 轮廓尺度 sigma (与 fwhm_px 同一 sigma 尺度;
//     PSF 块行 (FWHM=1.230310*sigma) 的调用方必须先按 PSF 块因子换算为 sigma 再传入,
//     禁止把 PSF 块 FWHM 填入 fwhm_px —— 跨块混用使 sigma 高估 1.914005x, DISP-STAR-007);
//   sigma_sky_adu [ADU];
//   gain_e_per_adu [e-/ADU]; read_noise_e [e-]; zero_point_mag [mag];
//   snr_* [无量纲]; sigma_f_* [ADU]; flux5_adu [ADU]; m5_mag [mag]
// ============================================================================

// σ_sky 入参语义 (SCI-B D1 定案; 口径正本 docs/plugins/algorithms_phase1/07_noise_snr.md §4.2a):
//   逐像素噪声 sigma_i^2 = sigma_sky^2 + (RN/g)^2 + F*P_i/g  —— **读噪只出现一次**。
//   sigma_sky_source 声明 sigma_sky 承载哪一种语义, 调用方**必须显式给出**:
//     SHOT_ONLY            : 天光+暗流**散粒**(不含读噪) => 组合时加 (RN/g)^2
//     EMPIRICAL_TOTAL_RMS  : 经验**总** rms(含读噪, 如 noise_model 空天稳健尺度)
//                            => 组合时**不得**再加 (RN/g)^2 (否则读噪双计)
//   UNSPECIFIED 为 legacy 缺省(等价 SHOT_ONLY 组合, 保持既有调用点逐位不变)。
#define SNR_SIGMA_SKY_UNSPECIFIED          0
#define SNR_SIGMA_SKY_SHOT_ONLY            1
#define SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS  2

// 逐源 SNR 输入参数 (全部显式, 无隐式帧级标量)
typedef struct {
    double flux_adu;            // F: 总通量 [ADU] (>0 必须)
    double fwhm_px;             // **检测块高斯** FWHM [pixel] (=2.3548200450309493*sigma);
                                // <=0 时改用 sigma_px。PSF 块 FWHM 禁止填入本字段。
    double sigma_px;            // 本块 Moffat4 各向同性 sigma [pixel] (与 fwhm_px 同尺度)
    double sigma_sky_adu;       // 逐像素空背景 rms [ADU] (>0 必须)
    double gain_e_per_adu;      // 增益 [e-/ADU]; <=0 = 未知 (不加源泊松项)
    double read_noise_e;        // 读出噪声 [e-]; 仅 gain>0 时进入
    double aperture_radius_px;  // 孔径半径 [pixel]; <=0 -> 1.5*FWHM
    double n_sky;               // 天空环像素数; <=0 -> n_pix
    double zero_point_mag;      // 零点 [mag]; 0 -> m5_mag = NaN
    int    profile_half_px;     // 轮廓网格半边长 [pixel]; 0 -> auto max(30,ceil(12*FWHM))
    int    sigma_sky_source;    // sigma_sky 语义: SNR_SIGMA_SKY_* (见上方块注释; 0=legacy)
} SnrSourceParams;

// 逐源 SNR 输出 (单位见上)
typedef struct {
    double snr_optimal;            // Horne 1986 最优提取 SNR [无量纲]
    double sigma_f_optimal_adu;    // 最优提取通量不确定度 sigma_F [ADU]
    double snr_aperture;           // 孔径 CCD 方程 SNR [无量纲] (未做孔径改正)
    double sigma_f_aperture_adu;   // 孔径通量不确定度 (含孔径改正) [ADU]
    double enclosed_fraction;      // Moffat4 beta=4 圈入流量分数 f_in(r) [无量纲]
    double aperture_correction;    // 1/f_in(r) [无量纲]
    double n_pix;                  // 孔径面积 pi*r^2 [pixel]
    double sum_p2;                 // sum_i P_i^2 (匹配滤波因子) [1/pixel]
    double snr_peak;               // 峰值型 SNR (仅诊断, 不得作科学输出) [无量纲]
    int    sigma_sky_source_effective;  // 实际生效组合 provenance:
                                        //   1=(RN/g)^2 已加(散粒口径);
                                        //   2=(RN/g)^2 未加(经验总 rms 已含读噪);
                                        //   3=gain<=0 不可加(天空受限)
    double flux5_adu;              // 5*sigma_F,opt [ADU]
    double m5_mag;                 // 5-sigma 点源深度 [mag]; zero_point<=0 时 NaN
    int    status;                 // 0=ok, 1=退化输入 (NaN/非正)
} SnrSourceResult;

// 逐源科学 SNR (Horne 最优提取 + 孔径 CCD 方程 + 孔径改正)
SNR_API int snr_source_snr_f64(const SnrSourceParams* params,
                               SnrSourceResult* out);

// 离散归一化 Moffat4 beta=4 轮廓统计 (oracle 锚, 网格规则冻结)
// fwhm_px = 检测块高斯 FWHM [pixel] (按 2.3548200450309493 换算 sigma; >0 优先);
// sigma_px = 本块 Moffat4 sigma [pixel]; 网格半边长 half_px<=0 -> max(30,ceil(12*FWHM_moffat4))。
// 输出 sum_i P_i^2 [1/pixel] 与中心像素 P [1]
SNR_API int snr_moffat4_profile_f64(double fwhm_px, double sigma_px, int half_px,
                                    double* out_sum_p2, double* out_p_center);

// 帧级科学基准 = 5-sigma 点源深度。reference 必须来自显式参考源。
// F_5 = 5*sigma_F(ref) [ADU]; m_5 = ZP - 2.5*log10(F_5) [mag]
SNR_API int snr_frame_depth_f64(const SnrSourceResult* reference,
                                double zero_point_mag,
                                double* out_flux5_adu,
                                double* out_m5_mag);

// 零点标准误 (dex): 1.253 * sigma_logflux_dex / sqrt(n_matches)
SNR_API double snr_calib_zero_point_standard_error(double sigma_logflux_dex,
                                                   int n_matches);

// ============================================================================
// SNR 估算 - 旧乘法模型 (legacy heuristic, 非科学 SNR)
// SNR(pixel) = SNR_phot × (SNR_psf(pixel) / median(SNR_psf))
// P5-SNR: 本段仅保留 ABI; 生产控制点值已改为逐源 Horne SNR (见上)。
//
// 输入:
// data - 图像像素 float32 [h*w] (行优先, 来自 CALIBRATE 阶段)
// h, w - 图像尺寸
// psf - PSF 拟合结果 double [n_stars*9]
// 每行: [status, B, flux, cx, cy, fwhm, A, mad, eccentricity]
// 列索引: status=0, B=1, flux=2, cx=3, cy=4, fwhm=5, A=6, mad=7, eccentricity=8
// 注: 第 8 列 (index 7, "mad") 实为 residual_scale (10-90% trimmed mean abs
// residual, SCI-PSF §2); 列名沿用历史, [N,9] 冻结布局与数值语义均不变。
// n_stars - PSF 星数量
// sigma_residual - 测光残差 sigma (来自 photo_stats 块 SIGMA_RESIDUAL)
// out_snr - 输出 SNR 图 float32 [h*w] (调用者分配)
//
// 返回: 0=成功, 1=n_stars<=0(退化,全填SNR_phot), 2=sigma_residual<=0(退化,全填1.0), 3=nullptr
//
// 注意: 此接口保留用于测试/调试, 管线中不再调用 (改用 snr_extract_model)
// ============================================================================
SNR_API int snr_estimate(const float* data, int h, int w,
                         const double* psf, int n_stars,
                         double sigma_residual,
                         float* out_snr);

// ============================================================================
// FP64 版本 ( 双精度 ABI 改造)
//
// 与 snr_estimate 逻辑一致, 仅 data 类型由 float 改为 double.
// 输出 out_snr 仍为 float32 (HISS SNR 子块格式已冻结为 float32, 见 02_FROZEN §17;
// SNR 是诊断值不是科学累加值, 精度损失可接受).
//
// 注意: 本接口保留用于测试/调试, 管线中不再调用 (改用 snr_extract_model, 后者
// 仅依赖 PSF double 参数, 与图像精度无关, 无需 f64 变体).
// ============================================================================
SNR_API int snr_estimate_f64(const double* data, int h, int w,
                             const double* psf, int n_stars,
                             double sigma_residual,
                             float* out_snr);

// ============================================================================
// SIP 前向系数 (A/B 多项式, 用于 pixel→sky 方向)
// 系数按 a[i*6+j] 存储, 对应 dx^i * dy^j, 下三角 i+j<=order
// 阶数上限 5 (36 元素), 与 FITS SIP 标准和 healpix_drizzle/wcs_sip.h 一致
// a_order<=0 表示无 SIP 修正 (仅用 CD+TAN)
// ============================================================================
#define SNR_SIP_MAX_ORDER 5
#define SNR_SIP_COEFF_SIZE 36  // (ORDER+1)*(ORDER+2)/2 上限, 用 6x6 数组存储

typedef struct {
    int a_order;                  // 前向 A 多项式阶数 (0=无)
    int b_order;                  // 前向 B 多项式阶数 (0=无)
    double a[SNR_SIP_COEFF_SIZE]; // A_i_j 系数 (前向, 像素→中间坐标)
    double b[SNR_SIP_COEFF_SIZE]; // B_i_j 系数
} SnrSipCoeffs;

// ============================================================================
// WCS 参数 (完整版, 含前向 SIP A/B, 用于像素→球面坐标转换)
// CRPIX 是 1-based (FITS 标准)
// ============================================================================
typedef struct {
    double crval1;  // 参考点赤经 (度)
    double crval2;  // 参考点赤纬 (度)
    double crpix1;  // 参考点像素 X (1-based)
    double crpix2;  // 参考点像素 Y (1-based)
    double cd[4];   // CD 矩阵 [cd11, cd12, cd21, cd22] (度/像素)
    SnrSipCoeffs sip;  // 前向 SIP 系数 (a_order=0 时无修正)
} SnrWcsParams;

// ============================================================================
// SNR-001: SNR 丢弃原因枚举 (SNR 不得静默丢点)
// 每个未写入 HISS 的 SNR 控制点必须有唯一原因分类。
// 值 0=未丢弃(已写入); 1-127=SNR 阶段丢弃(snr_extract_model 内部过滤);
// 128-254=Drizzle 阶段丢弃(写入 HISS 时); 255=其他未知原因。
// ============================================================================
typedef enum {
    SNR_DROP_NOT_DROPPED      = 0,    // 已写入 (有效控制点)
    // SNR 阶段丢弃 (snr_extract_model 内部过滤)
    SNR_DROP_OUTSIDE_TILE     = 1,    // 点不在任何 Tile 范围内 (drizzle 阶段判断)
    SNR_DROP_NO_OVERLAP       = 2,    // 点所在 Tile 无 signal 覆盖 (drizzle 阶段判断)
    SNR_DROP_INVALID_PSF      = 3,    // PSF 拟合失败: status != 0 或 residual_scale (列 7, 历史名 mad) <= 0
    SNR_DROP_INVALID_WCS      = 4,    // WCS 转换失败
    SNR_DROP_ZERO_FLUX        = 5,    // 通量为零 / 振幅不足 (A <= B)
    SNR_DROP_DUPLICATE_IPIX   = 6,    // 重复 local_ipix (保留首次, drizzle 阶段)
    SNR_DROP_OTHER            = 255   // 其他未知原因
} SnrDropReason;

// SNR 丢弃原因计数数组大小 (覆盖 0-255 全部枚举值)
#define SNR_DROP_REASON_COUNT 256

// ============================================================================
// SNR 控制点 (球面坐标 + snr_psf 值)
// 修复: 添加 #pragma pack(1) 确保 sizeof==20, 与 HioSnrControlPoint 二进制布局一致
// 根因: 未打包时 sizeof(SnrControlPoint)=24 (4字节尾部填充),
// 但 orchestrator 序列化用 memcpy(dst, points, n*20) 按 20 字节连续拷贝,
// 导致从第 2 个点起 ra/dec 错位, 产生 1609 个"越界"点.
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    double ra;       // 球面赤经 (度)
    double dec;      // 球面赤纬 (度)
    // P5-SNR (2026-09-14): 逐源最优提取 SNR_F [无量纲, Horne 1986]。
    // 旧 (A-B)/residual_scale 为 SNR-008 已退休量, 不再进入生产路径。
    float  snr_psf;
} SnrControlPoint;
#pragma pack(pop)
static_assert(sizeof(SnrControlPoint) == 20, "SnrControlPoint must be 20 bytes (packed, matches HioSnrControlPoint)");

// ============================================================================
// FP64 SNR 控制点 (BLOCKER-TYPE-002: FP64 模式 SNR 值保留 double 精度)
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    double ra;       // 球面赤经 (度)
    double dec;      // 球面赤纬 (度)
    double snr_psf;  // 逐源最优提取 SNR_F [无量纲, Horne 1986] (P5-SNR)
} SnrControlPointF64;
#pragma pack(pop)
static_assert(sizeof(SnrControlPointF64) == 24, "SnrControlPointF64 must be 24 bytes");

// ============================================================================
// SNR 控制点 v3 (Phase1 Final Signoff): 携带 stable star_id 与状态字段
// 行布局 (打包): ra f64 | dec f64 | snr f32/f64 | star_id u64 |
// quality_flags u32 | photometric_status u32
// f32: 8+8+4+8+4+4 = 36 字节; f64: 8+8+8+8+4+4 = 40 字节
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    double   ra;
    double   dec;
    float    snr_psf;
    int64_t  star_id;
    uint32_t quality_flags;
    uint32_t photometric_status;
} SnrControlPointV3;
#pragma pack(pop)
static_assert(sizeof(SnrControlPointV3) == 36, "SnrControlPointV3 must be 36 bytes");

#pragma pack(push, 1)
typedef struct {
    double   ra;
    double   dec;
    double   snr_psf;
    int64_t  star_id;
    uint32_t quality_flags;
    uint32_t photometric_status;
} SnrControlPointF64V3;
#pragma pack(pop)
static_assert(sizeof(SnrControlPointF64V3) == 40, "SnrControlPointF64V3 must be 40 bytes");

// quality_flags 位定义 (, 与 orchestrator 序列化一致)
enum SnrQualityFlagBits {
    SNR_QF_PSF_OK          = 1u << 0,  // PSF 拟合收敛 (status==0 DPSF_FIT_OK；3=ITERATION_LIMIT 不置位)
    SNR_QF_SATURATED       = 1u << 1,  // 星点饱和标志
    SNR_QF_HAS_SATURATED   = 1u << 2,  // 邻域含饱和像素
    SNR_QF_PHOTO_MATCHED   = 1u << 3,  // 测光已匹配 (status==1)
    SNR_QF_PHOTO_REJECTED  = 1u << 4   // 测光被拒绝 (status==2)
};

// ============================================================================
// SNR 模型 v3 (版本化, 支持 F32/F64 SNR 值 + star_id/status)
// ============================================================================
typedef struct {
    uint32_t n_points;
    uint8_t  value_dtype;   // 0 = SnrControlPointV3[], 1 = SnrControlPointF64V3[]
    uint8_t  reserved[3];
    void*    points;           // 逐源绝对 SNR_F 控制点
    double   snr_phot;         // IDW 全局尺度 [无量纲] = median(SNR_F) (P5-SNR 重定义)
    double   median_snr;       // IDW 归一化基准 [无量纲] = median(SNR_F) (P5-SNR 重定义)
    double   idw_power;
    double   median_source_snr;     // median(逐源 SNR_F) [无量纲]
    double   frame_depth_flux5_adu; // F_5 = 5*sigma_F(ref) [ADU]
    double   frame_depth_m5_mag;    // m_5 [mag]; 无 ZP 时 NaN
} SnrModelV3;

// ============================================================================
// SNR 模型 v2 (版本化, 支持 F32/F64 SNR 值)
// value_dtype: 0 = points 指向 SnrControlPoint[] (f32 snr)
// 1 = points 指向 SnrControlPointF64[] (f64 snr)
// ============================================================================
typedef struct {
    uint32_t n_points;
    uint8_t  value_dtype;
    uint8_t  reserved[3];
    void*    points;           // 按 value_dtype 解释 (逐源绝对 SNR_F)
    double   snr_phot;         // IDW 全局尺度 [无量纲] = median(SNR_F) (P5-SNR 重定义)
    double   median_snr;       // IDW 归一化基准 [无量纲] = median(SNR_F) (P5-SNR 重定义)
    double   idw_power;
    double   median_source_snr;     // median(逐源 SNR_F) [无量纲]
    double   frame_depth_flux5_adu; // F_5 = 5*sigma_F(ref) [ADU]
    double   frame_depth_m5_mag;    // m_5 [mag]; 无 ZP 时 NaN
} SnrModelV2;

// ============================================================================
// SNR 模型 (稀疏控制点 + 全局参数)
//
// SNR(ra,dec) = snr_phot × (IDW_spherical(points, query) / median_snr)
// IDW: weight = 1/γ^idw_power, γ=球面大圆弧角距离
// ============================================================================
typedef struct {
    uint32_t n_points;          // 控制点数
    SnrControlPoint* points;    // 控制点数组 (调用者负责释放, 用 snr_free_model)
    // P5-SNR 重定义 (2026-09-14): 控制点 snr_psf 现为绝对逐源 SNR_F, 故 IDW 重建
    // 的全局尺度与归一化基准均置 1.0 (使 snr_phot*IDW/median_snr == IDW(SNR_F))。
    // 二者不再是"帧级 SNR 标量"; 帧级科学基准见 snr_frame_depth_f64 (m_5)。
    double   snr_phot;          // IDW 全局尺度 [无量纲] = median(SNR_F) (P5-SNR 重定义)
    double   median_snr;        // IDW 归一化基准 [无量纲] = median(SNR_F) (P5-SNR 重定义)
    double   idw_power;         // IDW 幂次 (默认 2.0)
    // P5-SNR 新增帧级科学量 (有定义式/单位; 取代"整帧 SNR 标量"):
    double   median_source_snr;     // median(逐源 SNR_F) [无量纲]
    double   frame_depth_flux5_adu; // 5-sigma 点源极限通量 F_5 = 5*sigma_F(ref) [ADU]
    double   frame_depth_m5_mag;    // 5-sigma 点源深度 ZP-2.5*log10(F_5) [mag]; 无 ZP 时 NaN
} SnrModel;

// ============================================================================
// snr_extract_model - 从 PSF 块提取稀疏 SNR 控制点模型
//
// 输入:
// psf - PSF 拟合结果 double [n_stars*9] (同 snr_estimate)
// n_stars - PSF 星数量
// sigma_residual - 测光残差 sigma
// wcs - WCS 参数 (用于像素坐标→球面坐标转换)
// out_model - 输出 SNR 模型 (调用者负责用 snr_free_model 释放)
//
// 返回: 0=成功, 1=n_stars<=0或无有效星(退化), 2=sigma_residual<=0(退化), 3=nullptr
//
// 有效星条件: status==0, A>B, residual_scale (列 7, 历史名 mad) > 0
// 控制点坐标: PSF 星位置 (cx,cy) 经 WCS 转球面 (ra,dec)
// ============================================================================
SNR_API int snr_extract_model(const double* psf, int n_stars,
                               double sigma_residual,
                               const SnrWcsParams* wcs,
                               SnrModel* out_model);

// v2: 提取稀疏 SNR 控制点模型, value_dtype=0 (f32) / 1 (f64) 真实存储。
// FP64 模式下 snr_psf 为 double 计算并 double 存储 (非 float 扩展)。
SNR_API int snr_extract_model_v2(const double* psf, int n_stars,
                                  double sigma_residual,
                                  const SnrWcsParams* wcs,
                                  int value_dtype,
                                  SnrModelV2* out_model);

// v3: 提取稀疏 SNR 控制点模型并携带 stable star_id / quality_flags /
// photometric_status。star_ids/quality_flags/photometric_status 与
// psf 行对齐 (长度 n_stars), 有效星按原行拷贝其 ID/状态。
// 有效星条件与 v2 一致: status==0, A>B, residual_scale (列 7, 历史名 mad) > 0。
SNR_API int snr_extract_model_v3(const double* psf, int n_stars,
                                  double sigma_residual,
                                  const SnrWcsParams* wcs,
                                  int value_dtype,
                                  const int64_t* star_ids,
                                  const uint32_t* quality_flags,
                                  const uint32_t* photometric_status,
                                  SnrModelV3* out_model);

// ============================================================================
// snr_free_model - 释放 SnrModel 内部资源
// ============================================================================
SNR_API void snr_free_model(SnrModel* model);
SNR_API void snr_free_model_v2(SnrModelV2* model);
SNR_API void snr_free_model_v3(SnrModelV3* model);

#ifdef __cplusplus
}
#endif

#endif // SNR_ESTIMATOR_H
