#ifndef ASTROCS_PHOTOMETRY_FRAME_FIT_H
#define ASTROCS_PHOTOMETRY_FRAME_FIT_H
// ============================================================================
// frame_photometry_fit.h - 单帧测光比例 k_photo 的**文件无关**装配入口
//
// 规范依据: docs/science/PHOTOMETRY.md (SCI-PHOT-001, FROZEN)
//   k_photo = scale = 10^(-location),
//   location = 全体匹配星 r_i = log10(F_instr,i / F_syn,i) 的 Tukey-IRLS
//   (c=4.685) 稳健位置; F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ。
//
// 本入口把"装配"（滤光片/QE 曲线加载、Gaia XPSD 锥形搜索、光谱网格）与
// "调用生产星匹配链"（pc_calibrate_simple_with_gaia_f64_v2_qf）封在测光模块
// 内，供 Phase1 节点（lib/infrastructure/scheduler/src/module_adapters.cpp
// 的 p1_op_photometry）取到该标量并施加 I_photo = k_photo·I_cal。
//
// 与 orchestrator::run_stage_photometric 同源同口径（同一 C API、同一
// filter 名称映射、同一 FOV 半径公式、同一 mag_min=6.0）。本文件只做
// 装配，不做任何科学公式改动。
// ============================================================================

#include <cstdint>
#include <string>

namespace astrocs {
namespace photometry {

struct FramePhotFitRequest {
  // 已校准帧像素（FP64, row-major [height*width]）。F_instr 来自 psf_flux。
  const double* pixels = nullptr;
  int width = 0;
  int height = 0;
  // PSF 星（逐星）: 位置像素坐标 + 通量 + 拟合状态(0=OK) + 质量位(可空)
  //
  // ── F-INSTR-CONFORM-FIX: psf_flux 的**域契约**（SCI-PHOT-001 §9a, FROZEN）──
  // psf_flux[i] 必须且只能是 **PSF 拟合域解析通量**
  //   F_instr = 2πA·s_x·s_y/3   (Moffat4, β=4; SCI-PSF-001 §2/§5, 单位 ADU)
  // 即 p1_psf.json 的 psf_params[].flux（由 A,sx,sy 复算）或 orchestrator psf 块
  // 的 row[2]（dpsf_psf.cpp:428）。
  // **禁止**传入检测域 5×5 正性截断盒和（star_detector.cpp:151 s.flux = m00）:
  // 盒和捕获的 PSF 能量份额随 seeing 变化（实测 seeing 2.0→4.0 px 时给出
  // 0.50 mag 的假帧间差, 孔径扫描 M_seeing 1.35 mag）, 会把视宁度当成测光零点。
  // psf_status[i] != 0（拟合失败/未进入拟合子集）的星不进入匹配（§4 有效域）。
  const double* psf_cx = nullptr;
  const double* psf_cy = nullptr;
  const double* psf_flux = nullptr;
  const int* psf_status = nullptr;
  const uint32_t* psf_quality = nullptr;  // PC_QF_*; nullptr = 不过滤
  int n_psf = 0;
  // WCS（TAN+SIP）
  double crval1 = 0.0, crval2 = 0.0, crpix1 = 0.0, crpix2 = 0.0;
  double cd11 = 0.0, cd12 = 0.0, cd21 = 0.0, cd22 = 0.0;
  int sip_order = 0;
  const double* sip_a = nullptr;
  const double* sip_b = nullptr;
  const double* sip_ap = nullptr;
  const double* sip_bp = nullptr;
  // 配置
  std::string gaia_data_dir;   // XPSD 数据目录（光谱星必需, 通常 .../GaiaDR3SP）
  std::string filter_name;     // FITS FILTER 关键字值（内部映射到 filters.json 键）
  // 配置**声明**的通带名（块级 filter_passband）。非空时按
  // map_filter_name(filter_name) 与它核对：不等 ⇒ 拟合**拒绝产出标度**
  // （配置声明与实际使用的通带不是同一条曲线）。空 = 未声明，跳过该核对。
  std::string declared_filter_passband;
  std::string filters_json;    // filters.json 路径
  std::string qe_json;         // qe_curves.json 路径（可空）
  std::string qe_name;         // QE 曲线键（可空）
  double mag_min = 6.0;
  double mag_max = 16.0;
};

// SCI-PHOT-001 §4/§8 的**求解前提**（不是星数准入门槛）: |r_consistent| >= 3 才
// 进 IRLS, 否则 NO_DATA（scale 保持 1.0、fit_used=0、不迭代）。SCI-PHOT-001 §16.5
// 明确「星数不构成拒绝条件」——星少到该前提不成立时, 后果是**拟合本就不产出
// 标度**（按拟合失败上报）, 而不是由某个门禁去卡帧。本常量只用于**上报**拟合
// 是否真的产出标度, 不改变 §4 判据本身。
inline constexpr int kMinFitStars = 3;

// 失败**作用域**：回答"这次失败该由谁承担"——是本帧自身的判决，还是运行环境/
// 配置的问题。调用方据此决定**只把该帧判 fail**还是**中止整个运行**。
//   kNone        : fit_ok=true（产出了标度），不适用；
//   kFrame       : 本帧数据/拟合自身的判决 —— 无 PSF 星、NO_DATA（§4 求解前提
//                  |r_consistent|>=3 不成立）、非物理标度。**只该帧 fail**：
//                  同一批输入里的其他帧照常拟合与施加（帧间独立，SCI-PHOT-001
//                  §1/§16.5；负责人裁决「拟合失败这帧报 error/fail，不阻塞其他帧」）。
//   kEnvironment : 运行环境/配置问题 —— 配置缺项（gaia_data_dir / filter /
//                  filters_json）、响应曲线文件不可读、星表目录不可打开、
//                  光谱参数不可得、冻结 C 入口自身 rc!=0（含锥形搜索失败）。
//                  **必须中止**：这不是数据问题，换一帧也不会好（AGENTS §10
//                  「权限/数据/环境缺失」；LOG 合同 §6 D3「科学语义改变 ⇒ 不是
//                  降级，必须 fail-closed 上行」）。
enum class FitFailureScope : uint8_t {
  kNone = 0,
  kFrame = 1,
  kEnvironment = 2,
};

struct FramePhotFitResult {
  int rc = -1;                  // 0=成功算出 k_photo; <0 失败/未产出标度
  double k_photo = 1.0;         // 10^(-location)
  // 失败作用域（见 FitFailureScope 的逐值语义）。fit_ok=true 时恒为 kNone。
  FitFailureScope failure_scope = FitFailureScope::kNone;
  // P1-PHOT-BROKEN: fit_ok 是**唯一**允许调用方据以施加 k_photo 的判据。
  // rc==0 不足以说明"拟合产出了标度" —— 冻结 C 入口
  // pc_calibrate_simple_with_gaia_f64_v2_qf 在 NO_DATA/退化分支（无 PSF 星、
  // 无光谱星、滤光片缓存失败、|r_consistent|<3）**返回 0 且 scale=1.0**。
  // 修复前 module_adapters 只看 finite&&>0, 于是把 NO_DATA 的占位 1.0 当作
  // "已拟合标度"施加并声明 photometry_applied=true（伪造 1.0）。
  // fit_ok=true ⇔ rc==0 且 n_matched>=kMinFitStars（§4 求解前提）且 scale 有限且 >0。
  bool fit_ok = false;
  int n_matched = 0;            // IRLS 后 inliers
  double sigma_residual_dex = 0.0;
  int n_gaia = 0;               // 锥形搜索返回的光谱星数
  int psf_valid = 0;            // status==0 的 PSF 星数
  int robust_iterations = 0;
  std::string degraded_reason;  // fit_ok=false 时说明退化分支（机器可读）
  std::string error;
  // ── FREF-BASELINE-001: 绝对合成星等零点 ZP_syn ─────────────────────────
  // 约定与 snr_science.cpp:234 的 m_5 = ZP - 2.5*log10(F) 一致:
  //     mag = ZP_syn - 2.5*log10(F_syn)      [F_syn = XPSD 绝对谱积分]
  // 取值 = 锥形搜索星族上 median_i( magG_i + 2.5*log10 F_syn,i )，由 Gaia DR3
  // XP **绝对**谱 (XPSD: F(λ)=byte*flux_mul+flux_min) 经本帧滤光片+QE 曲线
  // **正向**合成得到。只依赖 (filter, QE, 天区星族)，**与帧无关** ⇒ 可作跨帧
  // 公共绝对参考锚（不依赖任何逐帧检出星群统计）。
  // 严禁用它反推增益/口径/曝光（§9.42 物理闭合禁令）：它只是星等↔合成通量换算。
  // zero_point_n_stars < kMinFitStars ⇒ zero_point_valid=false（不得使用）。
  bool zero_point_valid = false;
  double zero_point_mag = 0.0;          // ZP_syn [mag]
  int zero_point_n_stars = 0;           // 参与中位数的锥形搜索星数
  double zero_point_scatter_mag = 0.0;  // 1.4826*MAD(ZP_i) [mag]（星族 SED 散布）
  // ── 通带身份自述（PASSBAND-IDENTITY-GATE-01）─────────────────────────────
  // 「实际用于合成 F_syn 的那条曲线是谁」的机器可读记录：由**曲线对象自身**的
  // 自述字段 + 实际数组算出（不是把请求名回抄一遍）。调用方必须把它落进
  // p1_phot.json 的 provenance，使「配置声明的通带」与「实际用的曲线」在产品里
  // 可独立核对（docs/science/PHOTOMETRY.md §2a.4「比较不同帧/不同模型的
  // sigma_residual 时必须声明所用模型通带」）。
  // filter_key：声明名经 map_filter_name 解析出的库键（= filters.json 对象键）。
  // filter_curve_name：该对象 name 字段的自述值；身份门要求它与 filter_key 相等
  //   （不等 ⇒ fit 失败，不会有结果产出）。
  // n_points / wl_min_nm / wl_max_nm / val_min / val_max：实际装载数组的统计量。
  std::string filter_key;
  std::string filter_curve_name;
  int filter_n_points = 0;
  double filter_wl_min_nm = 0.0;
  double filter_wl_max_nm = 0.0;
  double filter_val_min = 0.0;
  double filter_val_max = 0.0;
};

// 运行单帧生产星匹配链, 返回 k_photo。
// 失败或未产出标度时 rc<0、fit_ok=false 且 k_photo=1.0（占位, 不得施加）。
FramePhotFitResult fit_frame_photometry(const FramePhotFitRequest& req);

}  // namespace photometry
}  // namespace astrocs

#endif  // ASTROCS_PHOTOMETRY_FRAME_FIT_H
