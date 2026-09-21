#ifndef ASTROCS_CALIBRATION_V6_CALIBRATION_COVARIANCE_H
#define ASTROCS_CALIBRATION_V6_CALIBRATION_COVARIANCE_H

/* ============================================================================
 * AstroCS V6 Phase1 calibration covariance — ALG-P1-CAL-COV-001
 * ----------------------------------------------------------------------------
 * 实现面（目标态，未接线 Phase session）:
 *   信号:   dark_opt=0  y_p = (r_p - d_p) / max(f_p, 0.1)          [无 flat 不除法]
 *           dark_opt=1  y_p = (r_p - b_p - alpha*(d_p - b_p)) / max(f_p, 0.1)
 *                       alpha = t_light / t_dark ;  f_p<=0/非有限 -> REJECT
 *   线性化: J = [ 1/f, -(1-alpha)/f, -alpha/f, -y/f ]
 *           C_cal = J C_in J^T ; 逐像素 Var(y_p) = diag(J C_in J^T)
 *   独立项: read_noise=(read_noise_e/gain)^2 ; photon_light=max(r,0)/gain ;
 *           quantization=q_adu^2/12 (q_adu 缺省 1 ADU，须声明) ;
 *           dark_photon=max(d,0)/gain
 *   同 master 折叠: 光路与暗路 bias 为同一 master_id 时系数折叠为 -(1-alpha)/f；
 *           不同 master_id 时为 -1/f 与 +alpha/f 两个独立项（OI-02 精确形式）。
 *   共享 master: 低秩 L L^T / 相关核 sigma+kernel / 共同 master_id + alpha_m 三通道之一；
 *           联合方差必须严格大于按独立项求和（ratio > 1），否则 REJECT。
 *   fail-closed: 缺 gain/read_noise（无 empirical_mad_fallback）-> unavailable；
 *           缺 master_id/归一版本/单位 -> unavailable；f<=0 或非有限 -> REJECT；
 *           共享项不可表示且无系统误差预算 -> unavailable。
 *
 * 权威锚（冻结，不得偏离）:
 *   docs/algorithms/v6/phase1/ALG_P1_001_PHASE1_ALGORITHM_SPEC.md §2
 *   docs/science/v6/frozen/01_SEMANTIC_FREEZE.md  FZ-PROV-SHARED-SYSTEMATIC /
 *       FZ-FORMULA-COV-PROP / FZ-CAL-FLOOR / FZ-CAL-QUANTUM-DEFAULT
 *   docs/contracts/v6/data/03_covariance.md ; 01_units_and_bunit.md
 *   reports/v6/contract-review/04_OPEN_ITEMS_AND_SIGNOFF.md  DI-03 (OPEN, fail-closed)
 *
 * 单位（冻结）: light/master signal = ADU ; variance = ADU^2 ;
 *              calibrated signal y = ADU ; v_cal = ADU^2 ; W_info = ADU^-2。
 * ==========================================================================*/

#include <string>
#include <vector>

namespace astrocs {
namespace calibration {
namespace v6 {

/* 合同/任务标识（只读；不改变 module.yaml 的 legacy 标识）。 */
const char* calibration_covariance_contract_id();  /* "ALG-P1-CAL-COV-001" */
const char* fault_injection_env_var();             /* "ASTROCS_V6_CAL_FAULT" (test-only) */

/* ────────────────────────────── 枚举与语义 ────────────────────────────── */

enum class CalStatus { kOk, kRejected, kUnavailable };

enum class DarkOption {
  kDarkIncludesBias = 0, /* y = (r - d) / f ; dark 已含 bias */
  kExplicitBiasDark = 1  /* y = (r - b - alpha (d - b)) / f */
};

enum class VarianceSource { kPhysical, kEmpiricalMadFallback };

enum class SharedRepresentationKind {
  kNone,               /* 无共享系统项 */
  kLowRank,            /* C_shared = L L^T */
  kCorrelationKernel,  /* sigma (scale) + kernel */
  kCommonMasterId      /* master_id + alpha_m + master variance */
};

enum class CalReason {
  kNone = 0,
  kMissingGainOrReadNoise, /* 缺 gain/read_noise 且未声明 empirical_mad_fallback */
  kMissingMasterIdentity,  /* 缺 master_id / 归一版本 / 单位 */
  kNonPositiveFlat,        /* f_p <= 0 或非有限（不得静默 floor 造值） */
  kNonFiniteInput,         /* r/b/d 非有限 */
  kSharedUnrepresentable,  /* 共享项不可表示且无系统误差预算 */
  kSharedNotDetected,      /* 共享项存在但 joint/naive <= 1 -> 按独立处理 REJECT */
  kInvalidConfig
};

const char* to_string(CalStatus status);
const char* to_string(CalReason reason);
const char* to_string(VarianceSource source);
const char* to_string(SharedRepresentationKind kind);

/* ───────────────────────────── 探测器与 master ──────────────────────────── */

struct DetectorMetadata {
  bool has_gain = false;        /* gain [e-/ADU], > 0 */
  double gain = 0.0;
  bool has_read_noise = false;  /* read_noise_e [e-] */
  double read_noise_e = 0.0;
  bool quantum_declared = false; /* q_adu 必须声明；缺省 1 ADU 并登记 */
  double q_adu = 1.0;            /* ADC quantum [ADU] */
  VarianceSource variance_source = VarianceSource::kPhysical;
};

/* master 自身估计方差: V(master) = V_single_frame / N_combined + 已声明共同模式项。
 * combine 规则与 N_combined 必须记录（ALG-P1-001 §2.3）。 */
struct MasterIdentity {
  std::string master_id;
  std::string normalization_version; /* 归一版本，缺 -> 单位不可判 */
  std::string units;                 /* 声明单位，缺 -> 单位不可判 */
  std::string combine_rule;          /* mean | median | sigma_clip */
  int n_combined = 0;
  double v_single_frame = 0.0;       /* ADU^2 */
  bool common_mode_declared = false;
  double common_mode_variance = 0.0; /* ADU^2 */

  double self_variance() const;      /* ADU^2；非法输入返回 -1 */
  bool identity_complete() const;    /* master_id / normalization_version / units 非空 */
};

struct RandomTermVariances {
  double read_noise = 0.0;    /* (read_noise_e/gain)^2        [ADU^2] */
  double photon_light = 0.0;  /* max(r,0)/gain                [ADU^2] */
  double quantization = 0.0;  /* q_adu^2/12                   [ADU^2] */
  double dark_photon = 0.0;   /* max(d,0)/gain                [ADU^2] */
  double light_frame() const; /* read_noise + photon_light + quantization */
};

RandomTermVariances independent_random_terms(double r_adu, double d_adu,
                                             const DetectorMetadata& md);

/* ──────────────────────────── 共享系统项表示 ─────────────────────────── */

struct SharedLowRank {
  std::string factor_ref;        /* L 因子引用/摘要（DI-03 数据面实例化 OPEN） */
  int rank = 0;
  std::vector<double> factor;    /* n_pix * rank, 行主序；仅数值求值时使用 */
};

struct SharedCorrelationKernel {
  std::string kernel_id;
  std::string kernel_version;
  double scale = 0.0;            /* sigma（尺度）> 0 */
};

struct SharedCommonMaster {
  std::string master_id;
  double alpha_m = 0.0;          /* 强度参数（同一 master 折叠后） */
  double master_variance = 0.0;  /* ADU^2 */
};

struct SharedSystematic {
  SharedRepresentationKind kind = SharedRepresentationKind::kNone;
  SharedLowRank low_rank;
  SharedCorrelationKernel correlation_kernel;
  SharedCommonMaster common_master;
  bool system_error_budget_declared = false;
  double system_error_budget_variance = 0.0; /* ADU^2，仅不可表示时兜底 */

  bool has_shared_term() const;            /* kind != kNone */
  bool structurally_representable() const; /* 通道载荷完整且可进入传播链 */
};

/* ───────────────────────────── 校准 covariance ───────────────────────── */

struct CalPixelInput {
  double r = 0.0; /* light signal [ADU] */
  bool has_bias_light = false; /* 光路 bias [ADU] */
  double bias_light = 0.0;
  bool has_bias_dark = false;  /* 暗路 bias [ADU]（显式分离路径） */
  double bias_dark = 0.0;
  bool has_dark = false;       /* dark master signal [ADU] */
  double dark = 0.0;
  bool has_flat = false;       /* 归一化 flat（无量纲） */
  double flat = 1.0;
  bool has_empirical_variance = false; /* empirical_mad_fallback 时的备用方差 */
  double empirical_variance = 0.0;     /* ADU^2 */
};

struct CalConfig {
  DarkOption dark_opt = DarkOption::kExplicitBiasDark;
  double alpha = 1.0; /* t_light / t_dark */
  DetectorMetadata detector;
  MasterIdentity bias_light_master;
  MasterIdentity bias_dark_master;
  MasterIdentity dark_master;
  MasterIdentity flat_master;
  SharedSystematic shared;
};

struct MasterIdRecord {
  std::string role;          /* bias_light | bias_dark | dark | flat */
  std::string master_id;
  std::string combine_rule;
  int n_combined = 0;
  double self_variance = 0.0; /* ADU^2 */
  bool folded_same_master = false;
};

struct CalResult {
  CalStatus status = CalStatus::kRejected;
  CalReason reason = CalReason::kInvalidConfig;
  double y = 0.0;          /* calibrated signal [ADU] */
  /* 逐像素方差三分（避免共享项对角重复计）：
   *   v_cal = v_cal_independent + v_cal_shared_diagonal
   * v_cal_independent 只含独立随机项（read/photon/quantization/dark photon）；
   * v_cal_shared_diagonal 为共享 master 自方差折叠后的对角贡献（ADJ-OBS-01：
   * 共享系统项进 covariance 面，其对角即此处；非对角由低秩/相关核/共同
   * master_id 通道表达）。evaluate_shared_gate 的 diagonal_variance 参数应传
   * v_cal_independent，否则共享通道对角会被重复计入。 */
  double v_cal = 0.0;              /* total per-pixel variance [ADU^2] */
  double v_cal_independent = 0.0;  /* independent random diagonal [ADU^2] */
  double v_cal_shared_diagonal = 0.0; /* shared systematic diagonal [ADU^2] */
  double sigma_cal = 0.0;  /* sqrt(v_cal) [ADU] */
  /* 折叠后方向导数 [dr, db, dd, df]（ADU 输入 -> ADU 输出） */
  double jacobian[4] = {0.0, 0.0, 0.0, 0.0};
  RandomTermVariances terms;
  VarianceSource variance_source = VarianceSource::kPhysical;
  bool same_bias_master_folded = false;
  bool flat_floor_applied = false;
  bool quantum_default_applied = false;
  std::vector<MasterIdRecord> master_ids;
  SharedSystematic shared_systematic;
};

/* 逐像素校准 + 方差传播（主入口）。 */
CalResult calibrate_pixel(const CalConfig& cfg, const CalPixelInput& px);

/* 逐像素批量（同 config；保持逐像素语义，不做跨像素聚合）。 */
std::vector<CalResult> calibrate_pixels(const CalConfig& cfg,
                                        const std::vector<CalPixelInput>& pixels);

/* 独立项对角方差闭式（供 Oracle 交叉校验）：
 *   v = jr^2 V_r + jb^2 V_b + jd^2 V_d + jf^2 V_f
 * 仅做二次型，不做任何裁剪/夹紧。 */
double diagonal_quadratic_variance(const double jacobian[4], double v_r,
                                   double v_b, double v_d, double v_f);

/* ────────────────────────── 共享项二次型与门 ─────────────────────────── */

struct SharedGateResult {
  bool ok = true;
  bool shared_present = false;
  double shared_variance = 0.0; /* 共享通道贡献 [ADU^2] */
  double naive_variance = 0.0;  /* 只按独立对角项 [ADU^2] */
  double joint_variance = 0.0;  /* naive + shared [ADU^2] */
  double ratio = 1.0;           /* joint / naive */
  CalReason reason = CalReason::kNone;
};

/* 独立对角二次型 Σ c_p^2 d_p（对角-only 下界）。 */
double naive_diagonal_variance(const std::vector<double>& c,
                               const std::vector<double>& diagonal_variance);

/* 低秩共享项二次型 || L^T c ||^2；L 为 n_pix x rank 行主序。 */
double low_rank_variance(const std::vector<double>& c,
                         const std::vector<double>& L, int n_pix, int rank);

/* 共同 master rank-1 二次型 (Σ_p c_p alpha_p)^2 * V_master。 */
double common_master_variance(const std::vector<double>& c,
                              const std::vector<double>& alpha_per_pixel,
                              double master_variance);

/* 相关核（显式物化矩阵 K，行主序 n x n）二次型 scale^2 * c^T K c。
 * 核函数形式本身 DI-03 未冻结 -> 生产不发明核；仅接受已物化 K。 */
double correlation_kernel_variance(const std::vector<double>& c,
                                   const std::vector<double>& K, int n_pix,
                                   double scale);

/* 共享门（ADJ-OBS-01-SHARED / FZ-PROV-SHARED-SYSTEMATIC）：
 * 共享项存在时要求 joint/naive > 1 + 1e-9；不可表示且无预算 -> unavailable。
 * 注意：diagonal_variance 必须是**独立随机项对角**（即 CalResult.v_cal_independent），
 * 不含共享 master 自方差，否则共享通道对角会被重复计入。 */
SharedGateResult evaluate_shared_gate(const std::vector<double>& c,
                                      const std::vector<double>& diagonal_variance,
                                      const SharedSystematic& shared,
                                      const std::vector<double>& alpha_per_pixel,
                                      const std::vector<double>& correlation_matrix);

/* ───────────────────────────── 单位与 covariance 记录 ───────────────────── */

struct UnitLaw {
  std::string signal_unit;
  std::string variance_unit;
  std::string ivar_unit;
};

UnitLaw calibration_unit_law(void);
bool unit_law_consistent(const UnitLaw& law); /* variance == signal^2 */

struct CovarianceRecord {
  std::string propagation = "C_out = R C_in R^T";
  std::string representation; /* covariance.v1 representation 枚举 */
  std::vector<double> combination_coefficients;
  std::string variance_from = "actual_combination_coefficients";
  std::string input_covariance = "declared";
  std::string avail = "available";
  std::string unavailable_reason;
  std::string operator_summary_ref;
  bool operator_reconstructable = false;
};

/* 从 CalResult 组装 covariance.v1 记录（字段名对齐
 * eng/contracts/proposals/v6/data/astrocs.v6.covariance.v1.schema.json）。 */
CovarianceRecord make_calibration_covariance_record(const CalResult& result);

bool is_forbidden_variance_source(const std::string& token);

/* 冻结门校验：C_out = R C_in R^T / variance_from / representation /
 * input_covariance / unavailable 原因白名单 / 禁止诊断来源。 */
bool validate_covariance_record(const CovarianceRecord& rec, std::string* error);

}  // namespace v6
}  // namespace calibration
}  // namespace astrocs

#endif /* ASTROCS_CALIBRATION_V6_CALIBRATION_COVARIANCE_H */
