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
  std::string filters_json;    // filters.json 路径
  std::string qe_json;         // qe_curves.json 路径（可空）
  std::string qe_name;         // QE 曲线键（可空）
  double mag_min = 6.0;
  double mag_max = 16.0;
};

// SCI-PHOT-001 §4/§8 冻结门: |r_consistent| >= 3 才进 IRLS, 否则 NO_DATA
// （scale 保持 1.0、fit_used=0、不迭代）。本常量只用于**上报**拟合是否真的
// 产出标度, 不改变 §4 判据本身。
inline constexpr int kMinFitStars = 3;

struct FramePhotFitResult {
  int rc = -1;                  // 0=成功算出 k_photo; <0 失败/未产出标度
  double k_photo = 1.0;         // 10^(-location)
  // P1-PHOT-BROKEN: fit_ok 是**唯一**允许调用方据以施加 k_photo 的判据。
  // rc==0 不足以说明"拟合产出了标度" —— 冻结 C 入口
  // pc_calibrate_simple_with_gaia_f64_v2_qf 在 NO_DATA/退化分支（无 PSF 星、
  // 无光谱星、滤光片缓存失败、|r_consistent|<3）**返回 0 且 scale=1.0**。
  // 修复前 module_adapters 只看 finite&&>0, 于是把 NO_DATA 的占位 1.0 当作
  // "已拟合标度"施加并声明 photometry_applied=true（伪造 1.0）。
  // fit_ok=true ⇔ rc==0 且 n_matched>=kMinFitStars 且 scale 有限且 >0。
  bool fit_ok = false;
  int n_matched = 0;            // IRLS 后 inliers
  double sigma_residual_dex = 0.0;
  int n_gaia = 0;               // 锥形搜索返回的光谱星数
  int psf_valid = 0;            // status==0 的 PSF 星数
  int robust_iterations = 0;
  std::string degraded_reason;  // fit_ok=false 时说明退化分支（机器可读）
  std::string error;
};

// 运行单帧生产星匹配链, 返回 k_photo。
// 失败或未产出标度时 rc<0、fit_ok=false 且 k_photo=1.0（占位, 不得施加）。
FramePhotFitResult fit_frame_photometry(const FramePhotFitRequest& req);

}  // namespace photometry
}  // namespace astrocs

#endif  // ASTROCS_PHOTOMETRY_FRAME_FIT_H
