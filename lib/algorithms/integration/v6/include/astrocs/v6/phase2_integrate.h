/* phase2_integrate.h — V6 Phase2 三模式产品链集成 / 磁盘重开消费 / 原子发布
 *
 * 任务: P2-INTEGRATE-001 (Wave 8, write_scope = lib/algorithms/coverage/src/integrate.{cpp,h},
 *       lib/algorithms/coverage/src/block.{cpp,h}, lib/algorithms/integration/, lib/phase2_session/,
 *       eng/tests/integration/v6_p2/)。本层只"接线"，不新增科学公式：
 *   - Phase1 单帧产品消费面 : astrocs::v6::phase1 (P1-INTEGRATE-001)
 *   - UPM 乘加求解/参数协方差: p2_upm_ma_* (IMPL-P2-UPM-001)
 *   - 分类排异 sigma_eff^2   : p2_reject_classify / p2_reject_calibration (IMPL-P2-REJ-001)
 *   - 空间求值/标量降级/覆盖  : p2_spatial_model_eval|summary、p2_scalar_degrade_gate、
 *                             p2_coverage_support_classify、p2_weight_mode_check、
 *                             p2_weight_source_token_reject (IMPL-P2-SAMP-001)
 *   - PSFSW 复合/组内归一/conventional coadd/covariance 传播/effective PSF :
 *                             astrocs::v6::p1psfw (IMPL-P1-PSFW-001)
 *   - FITS/原子发布/provenance/BUNIT : astrocs::aio (IMPL-AIO-001)
 *
 * 冻结锚（逐条符合，不得放宽）:
 *   FZ-MODE-PRODUCTION {point_information, surface_gls, psfsw_robust}
 *   FZ-MODE-DEFERRED   psf_snr_power 保持 DEFERRED，不进生产路由（C-004.1）
 *   FZ-FIELD-WEIGHTMODE legacy 0=support×snr² / auto / support_x_snr2 REJECT
 *   FZ-FORMULA-Q/WINFO/FHAT   Q_k=a_k P_k^T C_k^-1 d_k; W=a_k^2 P_k^T C_k^-1 P_k;
 *                             F_hat=Q/W; Var=1/W
 *   FZ-FORMULA-GLS           x_hat=(A^T C^-1 A)^-1 A^T C^-1 d; Cov=(A^T C^-1 A)^-1;
 *                             R=(A^T C^-1 A)^-1 A^T C^-1
 *   FZ-GATE-PIXIVAR-APPROX   rho=Var_approx/Var_GLS; p95<=1+eps(0.05); per-element
 *                             hard cap 1+0.20
 *   FZ-AP2S-IDENT-RTOL 1e-9 / FZ-AP2S-EPSF-RTOL 1e-12 /
 *   FZ-AP2PT-SNR-IDENT-RTOL 1e-9 / FZ-AP2PT-CORR-RATIO-MIN 1.05
 *   FZ-FORMULA-PSFSW-COMPOSITE / FZ-FIELD-PSFSW-UNIT / FZ-FIELD-PSFSW-4COMP
 *   FZ-FORMULA-COV-PROP      C_out = R C_in R^T；variance_from=combination_coefficients；
 *                            禁 1/W_psfsw、禁权重/诊断反推
 *   FZ-GATE-PSFSW-EPSF       effective PSF 必输；只给 FWHM 标量 REJECT
 *   FZ-GATE-MEDIAN-SNR / FZ-GATE-SUPPORT-COVERAGE 诊断量不得进权重/方差来源
 *   FZ-PROV-MINIMAL-SET / FZ-BUNIT-SEMANTICS / FZ-P3-BUNIT-QUADRATIC
 *   FZ-GATE-PSFSW-FAILCLOSED 单帧不足 n_common / 组不足 2 帧 fail-closed
 *   P33 撤销保持（C-004.2）: 不得重新引入 snr_frame_coefficient / snr_coefficient /
 *                             support_x_snr*；帧级 median(SNR_F) 只作诊断
 *   IMPL-P3-INTEGRATE-001 约定: HDU 命名 SIGNAL / FLUX / EFFECTIVE_PSF
 *
 * 权词表单源（W6 canonical，禁第三套）:
 *   weight.{kind,units,group_normalized,normalization.scope,
 *           normalization.median_target,normalization.constants_version,weight_value}
 */
#ifndef ASTROCS_V6_PHASE2_INTEGRATE_H
#define ASTROCS_V6_PHASE2_INTEGRATE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "astro/aio/v6_atomic_publish.h"
#include "astro/aio/v6_fits.h"
#include "astro/aio/v6_provenance.h"
#include "astro/aio/v6_validation.h"
#include "astrocs/v6/phase1_product.h"

namespace astrocs {
namespace v6 {
namespace p2int {

/* ------------------------------------------------------------------ */
/* 冻结常数（逐字来自 docs/contracts/v6/frozen，不重新定值）             */
/* ------------------------------------------------------------------ */
constexpr double kEpsPixivar = 0.05;      /* FZ-AP2S-EPS-PIXIVAR */
constexpr double kEpsPixivarSup = 0.20;   /* FZ-AP2S-EPS-PIXIVAR-SUP */
constexpr double kIdentRtol = 1e-9;       /* FZ-AP2S-IDENT-RTOL */
constexpr double kSnrIdentRtol = 1e-9;    /* FZ-AP2PT-SNR-IDENT-RTOL */
constexpr double kCorrRatioMin = 1.05;    /* FZ-AP2PT-CORR-RATIO-MIN */
constexpr double kEpsfRtol = 1e-12;       /* FZ-AP2S-EPSF-RTOL */
constexpr int    kCommonMin = 3;          /* PSFSW-T-NMIN */
constexpr double kKCorrFrozen = 1.4;      /* FZ-PROV-KCORR-VALUE */

constexpr const char* kPhase2ProductSchema = "astrocs.v6.phase2-product/v1";
constexpr const char* kPhase2RecordFile = "phase2_product.json";
constexpr const char* kPhase2ScienceFile = "mosaic.fits";
constexpr const char* kTypePoint = "astrocs.phase2.point_source.v1";
constexpr const char* kTypeSurface = "astrocs.phase2.surface_brightness.v1";
constexpr const char* kTypePsfsw = "astrocs.phase2.psfsw_integration.v1";

/* ------------------------------------------------------------------ */
/* 三模式路由（FZ-MODE-PRODUCTION / FZ-MODE-DEFERRED / FZ-FIELD-WEIGHTMODE） */
/* ------------------------------------------------------------------ */
enum class WeightMode : int {
  kPointInformation = 0,
  kSurfaceGls = 1,
  kPsfswRobust = 2,
};
const char* weight_mode_token(WeightMode m);
const char* phase2_type_id(WeightMode m);
bool parse_weight_mode(const std::string& token, WeightMode* out);
/* 生产路由门：allowed={point_information,surface_gls,psfsw_robust}；
 * psf_snr_power/auto/support_x_snr2/0/未知 -> REJECT；equal/pixel_ivar -> baseline 非生产。
 * 返回 0=生产模式；1=REJECT；2=baseline（非生产）。 */
int route_weight_mode(const char* mode, WeightMode* out, char* err,
                      std::size_t err_cap);

/* ------------------------------------------------------------------ */
/* 最小 FITS 平面读取器（BITPIX=-64；extname 空=PRIMARY）                 */
/* ------------------------------------------------------------------ */
bool read_fits_plane_f64(const std::string& path, const std::string& extname,
                         std::vector<double>* values, std::string* bunit,
                         std::vector<std::uint64_t>* naxis, std::string* err);

/* ------------------------------------------------------------------ */
/* 磁盘重开的 Phase1 帧集（Phase2 消费面的唯一入口）                      */
/* ------------------------------------------------------------------ */
struct Phase1Frame {
  std::string dir;
  phase1::Phase1ProductView view;
  std::vector<double> psf_profile;      /* P_k, Sum=1 */
  double a = 1.0;                       /* 光度响应 a_k */
  std::string record_sha;               /* phase1_product.json 实际 sha256 */
  std::vector<double> signal_sb;        /* 磁盘 SIG (ADU/px^2) */
  std::vector<double> variance_sb;      /* 磁盘 VARIANCE (ADU^2/px^4) */
  std::string signal_bunit, variance_bunit;
};

struct FrameSet {
  bool ok = false;
  std::string error;
  std::vector<Phase1Frame> frames;
  std::vector<std::string> evidence;
  std::vector<std::string> violations;
};

/* 逐个 open_phase1_product 重开 + 读盘 signal/variance；单帧不足/词表越界 fail-closed。 */
FrameSet open_phase2_frame_set(const std::vector<std::string>& product_dirs);

/* ------------------------------------------------------------------ */
/* 相关帧联合 covariance（调用方声明的可表示共享系统项；K*m 方阵）          */
/* ------------------------------------------------------------------ */
struct JointCovariance {
  bool provided = false;
  std::uint64_t m = 0;               /* 每帧支持域像素数 */
  std::vector<double> c_in;          /* (K*m)^2 row-major */
  std::string kernel_id;
  double rho_mean = 0.0;
  double rho_max = 0.0;
  bool has_off_diagonal = false;     /* 由 c_in 真实扫描得出 */
  /* 联合 GLS 的原始量（K*m）：A = a_k P_k 与 d；须与磁盘 psf_profile 一致。 */
  std::vector<double> a_psf;
  std::vector<double> data;
};

/* ------------------------------------------------------------------ */
/* 运行元数据 + 负向注入（仅测试）                                       */
/* ------------------------------------------------------------------ */
struct RunMeta {
  std::string run_id;
  std::string software_sha;                 /* 40 hex */
  std::string config_hash;
  std::string provider = "cpu_baseline";
  std::string build_id = "phase2-v6-integrate";
  std::string generated_utc = "2026-09-16T00:00:00Z";
  /* 负向注入：非空/false 默认关闭，生产恒为空。 */
  std::string inject_weight_source;         /* 进 weight.sources -> G-DIAG */
  std::string inject_variance_from;         /* 覆写 covariance.variance_from */
  std::string omit_provenance_key;          /* 从 provenance JSON 删除该键 */
  std::string inject_p33_key;               /* 重新引入 P33 系数键 */
  bool force_naive_sum = false;             /* 相关帧仍用 Sum -> REJECT */
  bool force_publish_verify_fail = false;   /* 发布后重开验证注入失败 */
  bool force_bunit_undecidable = false;     /* BUNIT=ADU 且无 pixel 语义 */
  bool force_effective_psf_only_fwhm = false; /* 只给 FWHM 标量 */
  bool force_variance_from_weight = false;  /* psfsw 用 1/W_psfsw */
};

struct ProductResult {
  bool ok = false;
  std::string error;
  WeightMode mode = WeightMode::kPointInformation;
  std::string product_dir;
  std::string output_sha256;

  /* 科学量（供独立 Oracle 对照） */
  double q = 0.0;
  double w_info = 0.0;
  double flux = 0.0;
  double flux_variance = 0.0;
  double x_hat = 0.0;
  double var_gls = 0.0;
  double var_approx = 0.0;
  double rho = 0.0;
  double rho_p05 = 0.0, rho_p50 = 0.0, rho_p95 = 0.0, rho_max = 0.0;
  double naive_variance = 0.0;
  double joint_variance = 0.0;
  double corr_ratio = 0.0;
  double median_wt = 0.0;

  std::vector<double> combination_coefficients; /* 实际组合系数 */
  std::vector<double> effective_psf;            /* P_eff values */
  std::string effective_psf_id;
  double effective_psf_fwhm = 0.0;
  std::string effective_psf_normalization;      /* peak | integral */

  bool joint_covariance_used = false;
  bool pixel_ivar_approx_used = false;
  bool approx_gate_passed = false;
  bool weight_value_null = false;

  std::vector<double> signal_out;   /* 主 HDU（SB） */
  std::vector<double> variance_out; /* diag(C_out)（SB） */
  std::vector<double> flux_out;     /* FLUX HDU（ADU） */
  std::vector<std::string> evidence;

  astrocs::aio::PublishResult publish;
};

/* surface_gls 输入（设计矩阵/数据/输入协方差由调用方按像素给出） */
struct SurfaceInputs {
  std::vector<std::string> product_dirs;
  std::uint64_t n_out = 1;            /* 输出元素数 */
  std::uint64_t n_pix_per_frame = 0;  /* m */
  std::vector<double> design;         /* A: (K*m) x n_out row-major */
  std::vector<double> data;           /* d: (K*m) x n_out row-major */
  std::vector<double> c_in;           /* (K*m)^2 row-major */
  std::vector<double> a_k;            /* 逐帧光度响应（结构性准入 a_k 一致性） */
  bool pixel_ivar_approx = false;     /* 使用 R~ 并过 epsilon 门 */
  bool a_k_ignored = false;           /* 负向：忽略 a_k 的一致性 */
  bool force_missing_error_gate = false; /* 负向：无误差门声明 */
  std::string correlation_kernel_id = "declared_shared_systematic_v1";
  double rho_mean = 0.0, rho_max = 0.0;
};

/* psfsw_robust 输入 */
struct PsfswInputs {
  std::vector<std::string> product_dirs;
  std::vector<std::vector<double>> d;         /* K x P 磁盘 SB */
  std::vector<std::vector<char>> validity;    /* K x P，可空=全有效 */
  std::vector<double> c_in;                   /* K*K row-major（实际系数协方差） */
  std::string correlation_kernel_id = "declared_shared_systematic_v1";
  double rho_mean = 0.0, rho_max = 0.0;
};

/* 三模式运行：磁盘重开 -> 组合 -> 原子写盘 -> 重开校验（失败无半成品）。 */
ProductResult run_point_information(const std::vector<std::string>& product_dirs,
                                    const JointCovariance& joint,
                                    const RunMeta& meta,
                                    const std::string& target_dir);
ProductResult run_surface_gls(const SurfaceInputs& in, const RunMeta& meta,
                              const std::string& target_dir);
ProductResult run_psfsw_robust(const PsfswInputs& in, const RunMeta& meta,
                               const std::string& target_dir);

/* ------------------------------------------------------------------ */
/* 磁盘重开独立校验                                                      */
/* ------------------------------------------------------------------ */
struct Phase2OpenResult {
  bool ok = false;
  std::string error;
  std::string output_sha256;
  std::string mode;
  std::vector<std::string> violations; /* G-xxx / FZ-xxx 逐条 */
  std::vector<astrocs::aio::FitsHduInfo> hdus;
  bool has_flux = false, has_effective_psf = false;
};

Phase2OpenResult open_phase2_product(const std::string& target_dir);

/* ------------------------------------------------------------------ */
/* UPM / REJ / SAMP 接线（overlap graph + 乘加模型 + 分类排异 + 空间门）  */
/* ------------------------------------------------------------------ */
struct UpmRejSampResult {
  bool ok = false;
  std::string error;
  std::uint64_t n_components = 0;
  std::uint64_t n_free = 0;
  std::uint64_t rank = 0;
  double kappa = 0.0;
  std::uint64_t n_overlap_controls = 0;
  std::uint64_t n_obs = 0;
  std::vector<double> g_k, b_k, s_p;
  double control_variance = 0.0;
  double sigma_eff2 = 0.0;
  double z = 0.0;
  int reject_status = 0;
  std::uint32_t accepted_count = 0;
  std::uint32_t rejected_low = 0, rejected_high = 0;
  double spatial_p05 = 0.0, spatial_p50 = 0.0, spatial_p95 = 0.0;
  double spatial_max_dev = 0.0, spatial_coverage = 0.0, spatial_model_error = 0.0;
  int scalar_verdict = -1;
  int weight_mode_rc = -1;
  int coverage_rc = -1;
  std::uint64_t n_supported = 0, n_uncovered = 0, n_unavailable = 0;
  std::vector<std::string> evidence;
};

/* 从磁盘 Phase1 帧集构建 overlap graph + 乘加 UPM + 排异 + 空间/覆盖门。 */
UpmRejSampResult run_upm_rej_samp_wiring(const FrameSet& fs,
                                         const RunMeta& meta);

}  /* namespace p2int */
}  /* namespace v6 */
}  /* namespace astrocs */

#endif  /* ASTROCS_V6_PHASE2_INTEGRATE_H */
