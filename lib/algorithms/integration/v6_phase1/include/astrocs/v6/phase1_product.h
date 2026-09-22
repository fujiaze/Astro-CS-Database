/* phase1_product.h — V6 Phase1 单帧产品装配 / 原子落盘 / 磁盘重开 / Phase2 消费面
 *
 * 任务: P1-INTEGRATE-001 (Wave 7)。写域: lib/phase1/, lib/phase1_session/,
 *       eng/tests/integration/v6_p1/。本层只"接线"，不新增科学公式：
 *   - 校准 covariance  : astrocs::calibration::v6 (IMPL-P1-CAL-001)
 *   - PSF/A_NEA/epsf   : astrocs::v6::p1psfw::psf_information (IMPL-P1-PSFW-001)
 *   - W_info/Q/F_hat   : astrocs::v6::p1psfw::information_weight (IMPL-P1-PSFW-001)
 *   - PSFSW 四分量/复合 : astrocs::v6::p1psfw::psfsw (IMPL-P1-PSFW-001)
 *   - 球面 Drizzle      : astrocs::v6::drizzle (IMPL-P1-DRZ-001)
 *   - FITS/provenance/BUNIT/原子发布/HiPS manifest : astrocs::aio (IMPL-AIO-001)
 *
 * 冻结锚（逐条符合，不得放宽）:
 *   FZ-UNIT-SIGNAL-SB  signal_sb           = ADU/sr
 *   FZ-UNIT-VAR-IN     pixel_variance_in   = ADU^2
 *   FZ-UNIT-VAR-SB     sb_variance_out     = ADU^2/sr^2
 *   FZ-UNIT-IVAR-SB    sb_ivar_out         = sr^2/ADU^2
 *   FZ-UNIT-WINFO      W_info              = ADU^-2
 *   FZ-UNIT-Q          Q                   = ADU^-1
 *   FZ-UNIT-FLUX       flux (F_hat)        = ADU
 *   FZ-UNIT-PSFSW      psfsw_robust_weight = 1 —— **已退役**（FZ-MODE-RETIRED），
 *                      产品**不再写出**该单位项（PSFSW-RETIRE-03 合同收口）：
 *                      产品/消费者声明它走显式拒绝 + 迁移提示（ASTROCS_DESIGN.md §3.1；
 *                      UNIFIED_MODEL.md:58），不静默接受。单位串仅为历史产品可判而保留
 *                      在 units_frozen_ok（不构成接受依据）。
 *   FZ-BUNIT-SEMANTICS / FZ-P3-BUNIT-QUADRATIC : BUNIT 量纲可判 + variance=signal^2
 *   FZ-FORMULA-DRIZZLE-SB/-VAR/-COV-PROP/-COND-FLUX-CONSERV
 *   FZ-FIELD-PSFSW-UNIT / FZ-FIELD-PSFSW-4COMP / FZ-FORMULA-PSFSW-COMPOSITE
 *   FZ-GATE-PSFSW-COV / FZ-GATE-PSFSW-EPSF / FZ-GATE-PSFSW-FAILCLOSED
 *   FZ-GATE-MEDIAN-SNR / FZ-GATE-SUPPORT-COVERAGE : 诊断量不得进权重面
 *   FZ-PROV-MINIMAL-SET / FZ-PROV-SHARED-SYSTEMATIC / FZ-PROV-KCORR
 *   FZ-MODE-PRODUCTION {point_information, surface_gls}
 *                      （原集合里的 psfsw_robust 已按负责人裁决退役）
 *   FZ-MODE-RETIRED    psfsw_robust_weight 不是现行对象 ⇒ 显式拒绝 + 迁移提示：
 *                      Phase2 消费面 consume_phase1_group_for_psfsw **整体退役**（无条件
 *                      fail-closed，不产出 w_psfsw）；open_phase1_product 对仍携带退役
 *                      声明的旧产品逐条登记 FZ-MODE-RETIRED 违规并置
 *                      view.retired_weight_object_declared（不静默接受）。
 *                      产品 schema 已把该声明从 required 移出 ⇒ 新产品不需要它（Phase1
 *                      产品链保持可用），旧产品仍被识别并按退役处理。
 *   OI-01 (OPEN): Phase1 单帧只出四分量 + 未归一 Wt + 归一契约；
 *                 组内 median=1 归一归 Phase2（fail-closed 登记）。
 *   C-004.2 / P33 撤销: 帧级 median(SNR_F) 系数不得接入权重面（本层不引用）。
 *
 * 权词表单源（W6 canonical，禁第三套）——**仅适用于仍携带退役声明的旧产品**:
 *   weight.{kind,units,group_normalized,normalization.scope,
 *           normalization.median_target,normalization.constants_version,
 *           weight_value}
 *   退役 canonical = kind="psfsw_robust_weight" / units="1" /
 *   group_normalized=true / scope="group" / median_target=1.0。
 *   PSFSW-RETIRE-03：新产品不写该块；旧产品携带时其形状必须逐条等于上式（否则判红）。
 */
#ifndef ASTROCS_V6_PHASE1_PRODUCT_H
#define ASTROCS_V6_PHASE1_PRODUCT_H

#include <cstdint>
#include <string>
#include <vector>

#include "astrocs/calibration/v6_calibration_covariance.h"
#include "astrocs/v6/information_weight.h"
#include "astrocs/v6/psf_information.h"
#include "astrocs/v6/psfsw.h"
#include "v6_drizzle_science.h"
#include "v6_spherical_overlap.h"

namespace astrocs {
namespace v6 {
namespace phase1 {

constexpr const char* kPhase1ProductSchema = "astrocs.v6.phase1-product/v1";
constexpr const char* kPhase1TypeId = "astrocs.phase1.frame_hips.v1";
constexpr const char* kPhase1ScienceFile = "science.fits";
constexpr const char* kPhase1RecordFile = "phase1_product.json";

/* ── 冻结单位串（逐字与 FZ-UNIT-* / 生产 schema 一致；不发明新串） ── */
struct Phase1Units {
  std::string signal_sb = "ADU/sr";
  std::string pixel_variance_in = "ADU^2";
  std::string sb_variance_out = "ADU^2/sr^2";
  std::string sb_ivar_out = "sr^2/ADU^2";
  std::string w_info = "ADU^-2";
  std::string q = "ADU^-1";
  std::string flux = "ADU";
  std::string psfsw_robust_weight = "1";
  std::string support = "px^2";
  std::string pixel_semantics = "surface_brightness";
  int pixel_area_power = -2;
};

/* ── 逐源点源信息输入（一个共同星；C 为对角逐像素方差） ── */
struct PointSourceInput {
  std::string star_id;
  std::vector<double> psf_profile;  /* P, Sum P = 1, >=0, length m */
  std::vector<double> sigma2;       /* diag C [ADU^2], length m */
  std::vector<double> data;         /* d [ADU], length m */
  double a = 1.0;                   /* 光度响应 a_k */
};

struct CalibrationInput {
  calibration::v6::CalConfig config;
  calibration::v6::CalPixelInput pixel;  /* 代表性像素（逐像素语义同配置） */
  std::string input_frame_sha256;
};

struct PsfswInput {
  std::vector<double> fhat;      /* 逐共同星 PSF 拟合通量 [ADU] */
  std::vector<char> fhat_valid;  /* 可选，空=全有效 */
  double a_nea = 0.0;            /* px^2；<=0 时由 psf_profile 复算 */
  double a_ref = 0.0;            /* px^2；0 => a_nea */
  double background_robust_mean = 0.0; /* ADU，必须 > 0 */
  std::vector<double> signal_samples;
  std::vector<double> concentration_samples;
  std::vector<double> noise_samples;
  std::vector<double> background_samples;
  std::string common_star_set_id;
  std::string selection_function_id;
  std::string members_hash;
  std::string construction = "external_catalog";
  std::string independence_proof = "external_reference_catalog";
  std::string exclusion_flags; /* 逗号分隔；写入 schema 数组 */
  std::string calibration_sample_id = "phase1_psfsw_calib";
  std::string acceptance_sample_id = "phase1_psfsw_accept";
};

/* ── 球面 Drizzle 输入。pixel_to_sky 由调用方给出（本层不实现 WCS）。 ── */
struct DrizzleInput {
  int nside = 2;
  int nx = 0;
  int ny = 0;
  double pixfrac = 1.0;
  double closure_rel_tol = 1e-6;
  std::vector<double> pixel_values;    /* x_j [ADU], length nx*ny */
  std::vector<double> pixel_variance;  /* v_j [ADU^2], length nx*ny */
  spherical::PixelToSkyFn pixel_to_sky = nullptr;
  void* user_data = nullptr;
};

struct Phase1FrameInputs {
  std::string frame_id;
  std::string run_id;
  std::string software_sha;  /* 40 hex */
  std::string config_hash;
  std::string coordinate_frame = "icrs";
  std::string coordinate_epoch = "J2000";
  std::string provider = "cpu_baseline";
  std::string build_id = "phase1-v6-integrate";
  CalibrationInput calibration;
  PointSourceInput point_source;
  PsfswInput psfsw;
  DrizzleInput drizzle;
  Phase1Units units;
};

/* ── 写出结果 ── */
struct Phase1WriteResult {
  bool ok = false;
  std::string error;
  std::string target_dir;
  std::string output_sha256;
  std::vector<std::string> evidence; /* 门裁决/冻结 id 逐条留痕 */
};

/* 原子发布一个 Phase1 单帧产品目录（tmp→fsync→CHECKSUM→rename→重开验证）。
 * 目标目录必须不存在或为空；失败时不得留下可见半成品。 */
Phase1WriteResult write_phase1_product(const Phase1FrameInputs& in,
                                       const std::string& target_dir);

/* ── 磁盘重开视图（Phase2 消费面的最小集） ── */
struct Phase1ProductView {
  bool ok = false;
  std::string error;
  std::string frame_id;
  std::string run_id;
  std::string output_hash;
  /* 单位与 BUNIT */
  std::string signal_bunit, variance_bunit, ivar_bunit, support_bunit;
  std::string signal_unit, variance_unit, ivar_unit;
  std::string pixel_semantics;
  int pixel_area_power = 0;
  /* 点源信息 */
  double w_info = 0.0, q = 0.0, flux = 0.0, flux_variance = 0.0;
  /* 权重面（canonical psfsw 词表）。注：psfsw_robust_weight 已退役
   * （FZ-MODE-RETIRED），这些字段只用于**判定/登记**，不得作为接受依据。 */
  std::string weight_kind, weight_units, norm_scope, constants_version;
  /* FZ-MODE-RETIRED：产品是否声明退役对象 psfsw_robust_weight（显式登记，不静默）。 */
  bool retired_weight_object_declared = false;
  std::string retired_weight_object;
  bool group_normalized = false;
  double norm_median_target = 0.0;
  bool has_weight_value = false;
  double weight_value = 0.0;
  /* psfsw validity */
  bool valid = false;
  std::string reason;
  int n_common = 0;
  std::string common_star_set_id, selection_function_id;
  int n_components = 0;
  std::vector<std::string> component_names;
  /* Phase1 未归一复合（OI-01：组内归一归 Phase2） */
  double unnormalized_wt = 0.0;
  std::string normalization_deferred_to;
  /* 几何与产物 */
  std::string science_path;
  std::uint64_t science_bytes = 0;
  int nside = 0;
  std::vector<std::uint64_t> target_ipix;
};

struct Phase1OpenResult {
  bool ok = false;
  std::string error;
  Phase1ProductView view;
  std::vector<std::string> violations; /* G-xxx / FZ-xxx 逐条 */
};

/* 重开并独立验证：CHECKSUM/DATASUM、SHA-256 vs manifest、BUNIT/二次律、
 * provenance 最小集、psfsw canonical 词表、禁止诊断来源。 */
Phase1OpenResult open_phase1_product(const std::string& target_dir);

/* ── Phase2 消费面：psfsw 组内归一权重（**已整体退役**） ──
 * FZ-MODE-RETIRED（PSFSW-RETIRE-03）：本面的唯一产物就是退役对象 psfsw_robust_weight
 * 的组内归一权重 w_psfsw（PSF 拟合质量代理的复合权重），ASTROCS_DESIGN.md §3.1 明确
 * 这类量不得进入科学叠加权重 ⇒ **无条件 fail-closed**（ok=false，error 含
 * FZ-MODE-RETIRED + 对象名 + 允许的权重对象 + 迁移提示），不静默接受、不产出 w_psfsw。
 * 保留该符号只为"旧产品声明退役对象"这一情形可判、理由可诊断（不是接受面）。 */
struct Phase1GroupConsumption {
  bool ok = false;
  std::string error;
  int n_frames = 0;
  std::vector<double> wt_unnormalized; /* 逐帧未归一（来自磁盘） */
  std::vector<double> w_psfsw;         /* 组内归一，median=1 */
  double median_wt = 0.0;
  bool record_ok = false;
  std::vector<std::string> findings;
};

Phase1GroupConsumption consume_phase1_group_for_psfsw(
    const std::vector<std::string>& product_dirs);

}  /* namespace phase1 */
}  /* namespace v6 */
}  /* namespace astrocs */

#endif  /* ASTROCS_V6_PHASE1_PRODUCT_H */
