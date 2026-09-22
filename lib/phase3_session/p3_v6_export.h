// ── RETIRED-CODE-RETAINED (ENGINEERING_SPEC §2 保留则注释) ─────────────
// WHAT:       Phase3 V6 三模式产品导出接线层（OutputGrid / ExportInputs / build_output_grid /
//             export_product / verify_product_on_disk；承载 FZ-P3-MODES / FZ-P3-QW-RECOMPUTE /
//             FZ-P3-BUNIT-QUADRATIC / FZ-P3-KERNEL-REGISTRY 四条 v6 合同）。
// WHY-KEPT:   删除会同时打断三处他域锚，本轮不能删：
//             ① eng/ci/checks.json CHK-CONTRACT-TEST 以 ctest_targets 登记 v6_p3_export_positive /
//                v6_p3_export_negative / v6_p3_export_oracle（并在 V6-CTEST-INTEGRATION step 的
//                --expect 列出），eng/tools/quality/check_ctest_registration.py 的 C4 对
//                「ctest_targets 匹配不到现存目标」fail-closed ⇒ 删测试即判红；eng/ci/checks.json 属
//                DOC-403 文件域，本任务无权同步；
//             ② eng/ci/spec_named_impls.json SNI-S4-P3X-06/P3X-12 与 eng/ci/ledgers/spec_named_impl_gaps.json
//                以本文件为锚（删除须同提交改表，属 CI 登记面，需与 DOC-403 同批）；
//             ③ docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv:338 仍点名本文件（属该清单域）。
// STATUS:     未接入生产。不在任何生产 target 的源列表内（grep -c p3_v6_export CMakeLists.txt = 0），
//             仅被 eng/tests/integration/v6_p3 编译；生产 export 路径 = lib/phase3_session/p3_session.cpp
//             → lib/algorithms/projection/p3_wcs.cpp（TAN），不依赖本文件任何符号
//             （p3_session.cpp:3-17 的 include 面无 p3_v6_export.h）。
// EXIT:       删除（ENGINEERING_SPEC §2 第 1 种处置），需同批完成：
//             ① DOC-403 从 eng/ci/checks.json 移除 v6_p3_export_* 三个 ctest_targets 及
//                V6-CTEST-INTEGRATION step 的三条 --expect；
//             ② 同提交删除 eng/ci/spec_named_impls.json 的 SNI-S4-P3X-06/SNI-S4-P3X-12 两条与
//                eng/ci/ledgers/spec_named_impl_gaps.json 的 SNI-S4-P3X-06 条；
//             ③ 把 PRODUCTION_EXECUTION_INVENTORY.csv:338 的 production=yes 更正为 retired；
//             ④ 删除 eng/tests/integration/v6_p3/** 与 CMakeLists.txt:980 的 add_subdirectory。
// AUTHORITY:  ENGINEERING_SPEC.md §2（历史实现处置：保留则注释）；ASTROCS_DESIGN.md §6.3
//             （注册表中未实现的投影被选择时显式报「不支持」，当前仅 TAN 可用）；
//             RELEASE-04 GAP_AUDIT G2-1/G3-2（包已出库）；eng/ci/spec_named_impls.json SNI-S4-P3X-06。
// ──────────────────────────────────────────────────────────────────────
// lib/phase3_session/p3_v6_export.h — Phase3 V6 三模式产品导出接线层 (P3-INTEGRATE-001)
//
// 任务: P3-INTEGRATE-001 (Wave 7, write_scope = lib/phase3_session/, eng/tests/integration/v6_p3/)
// 上位冻结（逐条接线，不重定义）:
//   * 宪章 §7.1/§7.2/§7.3（Phase3 输出带合法 FITS-WCS/coverage/不确定度传播/provenance 的
//     二维平面 FITS；投影/重采样是独立算法节点，AIO 读/写是基建服务）。
//   * FZ-P3-MODES: 生产输出模式 = surface_brightness | point_source_flux | visualization。
//   * FZ-P3-FAILCLOSED: 三模式 fail-closed（SB 无 Ω 却 flux 换算 / 测量却无 uncertainty 原因 /
//     BUNIT 非二次律 / 对角无相关核；PSF 缺 PSF/未归一/缺 point_information 且不可重建/
//     缺 a/未出 effective PSF；VIS measurement_capable=true 或写测量层 -> REJECT）。
//   * FZ-P3-QW-RECOMPUTE: Q=a*pi^T C_y^-1 f; W=a^2*pi^T C_y^-1 pi; pi=S p;
//     禁止重采样输入 Q/W；消费上游 W_info 不重算/不替换。
//   * FZ-P3-BUNIT-QUADRATIC / FZ-UNIT-*: variance BUNIT = (signal BUNIT)^2, ivar = 1/variance；
//     signal_sb=ADU/sr, sb_variance_out=ADU^2/sr^2, W_info=ADU^-2, Q=ADU^-1, flux=ADU, psfsw=1。
//   * FZ-PROV-MINIMAL-SET: 写盘 provenance 最小集 + 重开可校验。
//   * FZ-P3-KERNEL-REGISTRY: 未注册/未验证核进生产 -> REJECT；nearest 仅离散/诊断/显式选择。
//   * ALG-P3-008 §7: 原子发布 tmp -> fsync -> DATASUM/CHECKSUM -> rename -> 重开独立验证；
//     失败/取消不留可见半成品。
//
// 接线来源（Wave 5 交付，本层只调用不修改）:
//   * lib/algorithms/projection/p3_proj_v6.h      —— registry v2 四投影 + 逐像素 Ω + R/S 二元语义。
//   * lib/algorithms/resample/p3_rsmp.h         —— 三模式传播、C_y=R C_x R^T、输出帧 Q/W、kernel registry。
//   * lib/infrastructure/aio/v6/...         —— 流式 FITS + CHECKSUM + 原子发布 + provenance + 单位门。
//
// 单位/权重面纪律（本层硬约束）:
//   * 不把 median(SNR_F)/support/coverage/FWHM 接成任何权重或方差来源（FZ-GATE-MEDIAN-SNR /
//     FZ-GATE-SUPPORT-COVERAGE；C-004.2）。
//   * 不写 ivar 于 psfsw；不把相对复合权重当 ivar（RULINGS #5）。
//   * psf_snr_power 保持 DEFERRED，不进入生产路由（FZ-MODE-DEFERRED；C-004.1）。
//
// 产品 HDU 布局（生产 schema 约束下的唯一自洽布局，见 p3_v6_export.cpp 顶部注记）:
//   * 所有模式主 HDU = SIGNAL（面亮度，BUNIT=ADU/sr）—— 因 provenance schema 规定
//     bunit="ADU" 只能声明 pixel_semantics=surface_brightness/pixel_area_power=-2，
//     纯积分通量主面在 v6 生产 schema 下不可表达（FZ-UNIT-FLUX=ADU 与 FZ-BUNIT-SEMANTICS
//     的交叉张力已登记为 finding；通量以扩展 HDU 承载）。
//   * surface_brightness: SIGNAL + VARIANCE(+IVAR) + COVERAGE。
//   * point_source_flux : SIGNAL + VARIANCE + FLUX(ADU) + FLUX_VARIANCE(ADU^2)
//                         + EFFECTIVE_PSF(1) + COVERAGE；Q/W 输出帧重算并写盘。
//   * visualization     : 仅 SIGNAL（measurement_capable=false，禁写测量 HDU），
//                         provenance 显式登记 uncertainty unavailable。
#ifndef ASTROCS_P3_V6_EXPORT_H
#define ASTROCS_P3_V6_EXPORT_H

#include <cstdint>
#include <string>
#include <vector>

#include "astro/aio/v6_atomic_publish.h"
#include "astro/aio/v6_fits.h"
#include "astro/aio/v6_product_io.h"
#include "astro/aio/v6_provenance.h"
#include "astro/aio/v6_validation.h"
#include "p3_proj_v6.h"
#include "p3_rsmp.h"

namespace astrocs {
namespace phase3 {
namespace v6 {

// ---------------------------------------------------------------------------
// 三输出模式（FZ-P3-MODES）——本层不发明第四种模式。
// ---------------------------------------------------------------------------
enum class ExportMode : int {
  kSurfaceBrightness = 0,
  kPointSourceFlux = 1,
  kVisualization = 2,
};
const char* export_mode_token(ExportMode m);
bool parse_export_mode(const std::string& token, ExportMode* out);
p3rsmp::P3Mode to_rsmp_mode(ExportMode m);

// ---------------------------------------------------------------------------
// 输出网格：phase3proj::v6 registry v2 descriptor + 逐像素 Ω'（sr，真实计算，禁常数 Ω）。
// ---------------------------------------------------------------------------
struct OutputGrid {
  phase3proj::v6::Descriptor descriptor{};
  int width = 0;
  int height = 0;
  std::vector<double> omega_out_sr;  // width*height，全部 > 0
  double fov_x_deg = 0.0;
  double fov_y_deg = 0.0;
  double omega_min_sr = 0.0;
  double omega_max_sr = 0.0;
};

// registry v2 计划 + solid_angle_grid；越域/奇点/奇异 Ω -> fail（不产半成品）。
phase3proj::v6::ProjStatus build_output_grid(
    phase3proj::v6::ProjectionId id, double centre_ra_deg, double centre_dec_deg,
    double scale_deg_per_px, int width_px, int height_px, const char* parity,
    double rotation_pa_deg, OutputGrid* out, std::string* err);

// ---------------------------------------------------------------------------
// 输入面 + 采样计划。
// ---------------------------------------------------------------------------
struct ExportInputs {
  int in_width = 0;
  int in_height = 0;
  std::vector<double> omega_in_sr;  // n_in，全部 > 0（真实输入像素立体角）
  std::vector<double> x;            // 输入面亮度 SB（ADU/sr）

  // covariance 输入（SB/PSF）。完整 C_x 为生产路径；对角仅作阴性对照。
  p3rsmp::DenseMatrix c_in;
  bool covariance_diagonal = false;        // true -> C_x 仅对角（须相关核，否则 REJECT）
  bool correlation_kernel_present = false; // 对角时是否另有精确相关核

  // point_source_flux 输入
  std::vector<double> psf_p;               // 输入 effective PSF，Σp=1
  bool psf_present = false;
  bool point_information_present = false;  // 上游 W_info 存在（消费不重算）
  bool rebuildable_from_frames = false;
  double photometric_scale_a = 0.0;
  bool effective_psf_present = false;
  bool effective_psf_fwhm_only = false;
  bool effective_psf_normalization_declared = false;
  bool input_qw_resampled = false;         // 禁止项（FZ-P3-QW-RECOMPUTE）
  bool w_from_sum_input = false;           // 禁止项
  bool upstream_w_recomputed = false;      // 禁止项

  // surface_brightness 输入
  bool flux_conversion_requested = false;  // 需逐像素 Ω
  bool uncertainty_available = true;
  std::string uncertainty_unavailable_reason;
  bool measurement_capable = true;         // visualization 必须 false

  // 采样核（FZ-P3-KERNEL-REGISTRY）
  std::string kernel_id = "bilinear_4quad";

  // provenance 最小集元数据
  std::string software_sha;             // 40 hex
  std::string run_id;
  std::vector<std::string> input_product_hashes;  // >=1
  std::string config_hash;
  std::string provider = "cpu_baseline";
  std::string module_id = "phase3.export";
  std::string module_build_id = "p3intg1";
  std::vector<std::string> algorithm_ids;  // >=1
  std::string normalisation_version = "phase3_v6_surface_brightness_row_normalised";
  std::string weight_mode_version = "phase3_v6_no_weight_face";
  // 上游 HiPS 相关核/k_corr 摘要（FZ-PROV-SHARED-SYSTEMATIC / FZ-PROV-KCORR）；
  // Phase3 自身不做 patch 排异，值继承自输入 mosaic 的标定。
  std::string correlation_kernel_id = "phase3_input_mosaic_rho_v1";
  double correlation_scale = 1.0;
  double k_corr_value = 1.4;
  std::string k_corr_geometry = "phase3_resample_input_hips";
  std::string k_corr_calibration_script = "run/v6/phase2/upm/kcorr_calib.py";
  long long k_corr_seed = 20260915;
  std::string k_corr_run_id = "upmw-inherited";
  std::string generated_utc = "2026-09-16T00:00:00Z";

  // 阴性注入（仅测试；生产默认空/false）。
  std::string inject_weight_source;        // 非空 -> 触发 G-P3-GLB-01
  std::string omit_provenance_key;         // 非空 -> 从 provenance JSON 删除该键
  std::string force_variance_unit;         // 非空 -> 覆写 provenance 方差单位（破坏二次律）
  bool force_omega_absent = false;         // true -> 主面声称无逐像素 Ω（触发 G-P3-SB-01）
  bool force_publish_verify_fail = false;  // true -> 发布后重开验证注入失败（撤销产物）
};

// 输出像素在输入索引坐标中的位置（bilinear 4 象限；out_step=1 时 1:1）。
struct ResamplePlan {
  double out_origin_x = 0.0;
  double out_origin_y = 0.0;
  double out_step = 1.0;
};

struct ExportResult {
  p3rsmp::Status status = p3rsmp::Status::Ok;
  std::string code;
  std::string reason;

  // 原子发布
  astrocs::aio::PublishResult publish;
  std::string product_dir;   // 目标目录（产物 = product_dir/product.fits + provenance.json）
  std::string fits_path;
  std::string provenance_path;
  std::vector<std::string> hdu_names;

  // 科学量（FP64 内存态，供独立 Oracle 对照）
  std::vector<double> signal;    // 主 HDU（SB）或 visualization 显示面
  std::vector<double> variance;  // diag(C_y)（SB/PSF）
  std::vector<double> flux;      // point_source_flux 输出帧 f = S d
  std::vector<double> effective_psf;  // pi = S p
  double Q = p3rsmp::kNaN;
  double W = p3rsmp::kNaN;
  double F_hat = p3rsmp::kNaN;
  double var_F_hat = p3rsmp::kNaN;
  bool frame_is_output_recompute = false;

  // 重开验证（磁盘）
  astrocs::aio::FitsVerifyResult reopen;
  astrocs::aio::ValidationReport provenance_reopen_check;
  bool wrote_variance = false;
  bool wrote_flux = false;
  bool wrote_effective_psf = false;
};

// 三模式导出（propagate -> 组层 -> 流式 FITS + 原子发布 -> 重开验证）。
// 任何冻结门命中即返回非 Ok 且**不写盘**（fail-closed）。
ExportResult export_product(ExportMode mode, const OutputGrid& grid,
                            const ExportInputs& in, const ResamplePlan& plan,
                            const std::string& product_dir,
                            const astrocs::aio::PublishOptions& opts,
                            const astrocs::aio::CancelFn& cancel);

// 独立重开校验（供下游/测试复用）：verify_fits_file + provenance JSON 门 + BUNIT 二次律。
astrocs::aio::ValidationReport verify_product_on_disk(
    const std::string& product_dir,
    const std::vector<astrocs::aio::ExpectedHdu>& expected,
    astrocs::aio::FitsVerifyResult* reopen_out);

}  // namespace v6
}  // namespace phase3
}  // namespace astrocs

#endif  // ASTROCS_P3_V6_EXPORT_H
