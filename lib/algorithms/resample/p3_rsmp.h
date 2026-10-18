// lib/algorithms/resample/p3_rsmp.h
//
// ACSD Phase3 科学重采样传播（IMPL-P3-RSMP-001）
//
// 本模块实现 ALG-P3-001（SPEC §1–§9）的算法层生产代码：
//   * 三输出模式（FZ-P3-MODES）：surface_brightness / point_source_flux / visualization
//   * 统一线性模型：y = R x（R 行归一），f = S d（S 列归一），S_ij = R_ij * Omega'_i/Omega_j
//   * covariance 传播：C_y = R C_x Rᵀ（FZ-FORMULA-COV-PROP；唯一来源，禁权重标量反推）
//   * effective PSF 传播：pi = S p，Σ pi = 1（ALG-P3-007）
//   * 输出帧 Q/W 重算：Q = a·piᵀC_y⁻¹f，W = a²·piᵀC_y⁻¹pi（FZ-P3-QW-RECOMPUTE；禁重采样输入 Q/W）
//   * 采样核 registry（FZ-P3-KERNEL-REGISTRY；bilinear_4quad 须独立 Oracle+误差界+边界）
//   * 三模式 fail-closed 12 门 + kernel/covariance/QW/epsf/provenance 扩展门（FZ-P3-FAILCLOSED）
//
// 单位（冻结表 docs/science/unified/DATA_SEMANTICS.md「单位与量纲表」一节）：
//   signal_sb=ADU/sr、pixel_variance_in=ADU²、sb_variance_out=ADU²/sr²、
//   sb_ivar_out=sr²/ADU²、W_info=ADU⁻²、Q=ADU⁻¹、flux=ADU、psfsw=1、
//   phase3_var_out=(主 HDU signal BUNIT)²（FZ-P3-BUNIT-QUADRATIC）。
//
// 本文件不发明任何冻结阈值：OPEN 项（CF-T-P3-CORR-EPSILON / SO-07）以「未签字即 fail-closed」
// 方式实现（GateConfig::epsilon_corr_ratified 默认 false → 近似 covariance 面 UNAVAILABLE）。
#ifndef ACSD_P3_RSMP_H
#define ACSD_P3_RSMP_H

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace acsd {
namespace p3rsmp {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ---------------------------------------------------------------------------
// 0. 状态与模式
// ---------------------------------------------------------------------------
enum class Status {
  Ok = 0,
  Reject,       // fail-closed：违反冻结条件
  Unavailable,  // 显式不可用（须带原因）
  NotImplemented,
  InvalidArgument,
};
const char* to_string(Status s);

enum class P3Mode { SurfaceBrightness, PointSourceFlux, Visualization };
const char* to_string(P3Mode m);
const char* mode_token(P3Mode m);

// 生产模式集合（FZ-P3-MODES）。返回 false 表示未声明/非生产/legacy。
bool is_production_mode(P3Mode m);
// 解析模式 token；legacy {auto, support_x_snr2, 0, 1, 2} 与 psf_snr_power 一律拒绝。
Status parse_mode(const std::string& token, P3Mode* out);
// legacy 整数 / DEFERRED 词（FZ-MODE-DEFERRED / FZ-FIELD-WEIGHTMODE / C-004.1）。
bool is_retired_mode_token(const std::string& token);
bool is_deferred_weight_mode_token(const std::string& token);

// ---------------------------------------------------------------------------
// 1. 单位与 BUNIT 二次律（FZ-UNIT-* / FZ-BUNIT-SEMANTICS / FZ-P3-BUNIT-QUADRATIC）
// ---------------------------------------------------------------------------
// 线性基单位（Issue #9，docs/science/unified/DATA_SEMANTICS.md §3 / PHOTOMETRY §4）:
//   * Adu     = 原生校准计数积分量（ADU）；未施加带单位标度的帧。
//   * XpModel = 已施加 Gaia XP 合成通带标度后的**模型通带通量**单位。它由该帧
//               实际装载的 response（滤镜曲线 + QE + XP 绝对谱约定）归一，是
//               相对模型单位，**不是** SI 绝对光谱辐照度，也不得写成 ADU。
// 二者是不同量纲族: 跨基单位相乘/相加无量纲意义 ⇒ mixed_base=true 即不可判。
enum class LinearBase { Adu = 0, XpModel = 1 };
// 线性基 canonical token（大小写敏感）: "ADU" | "XPMODEL"。
const char* to_string(LinearBase b);
// 解析线性基 token；未知/空/大小写不符 ⇒ false（不静默归一）。
bool parse_linear_base(const std::string& token, LinearBase* out);
// 面亮度链三串（signal / 其方差 / 其逆方差）由线性基单位唯一导出。
std::string surface_brightness_bunit(LinearBase b);           // "ADU/sr" | "XPMODEL/sr"
std::string surface_brightness_variance_bunit(LinearBase b);  // "ADU^2/sr^2" | "XPMODEL^2/sr^2"
std::string surface_brightness_ivar_bunit(LinearBase b);      // "sr^2/ADU^2" | "sr^2/XPMODEL^2"

// BUNIT 表示为线性基单位幂次 × 立体角幂次（sr）（canonical 串固定，不发明第三套词表）。
struct Bunit {
  int adu_power = 0;   // 线性基单位幂次（历史字段名；base 决定基单位是 ADU 还是 XPMODEL）
  int px_power = 0;
  LinearBase base = LinearBase::Adu;
  bool mixed_base = false;   // 跨基单位相乘 ⇒ 量纲不可判（显式失败，不静默取其一）
  std::string canonical() const;
  bool operator==(const Bunit& o) const {
    return adu_power == o.adu_power && px_power == o.px_power &&
           base == o.base && mixed_base == o.mixed_base;
  }
  bool operator!=(const Bunit& o) const { return !(*this == o); }
};

// 面亮度 signal 单位 {1,-2,base}（Issue #9）。
Bunit surface_brightness_unit(LinearBase b);

Bunit bunit_mul(const Bunit& a, const Bunit& b);
Bunit bunit_square(const Bunit& a);
Bunit bunit_inverse(const Bunit& a);

// 冻结单位表（eng/contracts/data/clause_registry.json units_table）。
namespace units {
extern const Bunit signal_sb;          // ADU/sr
extern const Bunit pixel_variance_in;  // ADU^2
extern const Bunit sb_variance_out;    // ADU^2/sr^2
extern const Bunit sb_ivar_out;        // sr^2/ADU^2
extern const Bunit w_info;             // ADU^-2
extern const Bunit q_stat;             // ADU^-1
extern const Bunit flux;               // ADU
extern const Bunit psfsw;              // 1
// Issue #9: 已施加 Gaia XP 模型标度的对应面亮度链（模型通带通量族）。
extern const Bunit signal_sb_model;          // XPMODEL/sr
extern const Bunit sb_variance_out_model;    // XPMODEL^2/sr^2
extern const Bunit sb_ivar_out_model;        // sr^2/XPMODEL^2
}  // namespace units

// FZ-P3-BUNIT-QUADRATIC：variance BUNIT == (signal BUNIT)^2 且 ivar == 1/variance。
bool is_quadratic_variance(const Bunit& signal, const Bunit& variance);
bool is_inverse_pair(const Bunit& variance, const Bunit& ivar);

// FZ-BUNIT-SEMANTICS：BUNIT 必须量纲可判。
enum class PixelSemantics { Unspecified = 0, SurfaceBrightness, IntegratedFlux };
struct BunitProvenance {
  PixelSemantics pixel_semantics = PixelSemantics::Unspecified;
  bool pixel_area_power_present = false;
  int pixel_area_power = 0;
};
struct BunitResolution {
  bool resolvable = false;
  Bunit resolved;
  std::string reason;
};
// 显式立体角幂次 canonical 串恒可判；裸 "ADU" 须 provenance 声明像素语义与 pixel_area_power。
BunitResolution resolve_bunit(const std::string& bunit_str, const BunitProvenance& prov);

// ---------------------------------------------------------------------------
// 2. 采样核 registry（FZ-P3-KERNEL-REGISTRY / ALG-P3-003）
// ---------------------------------------------------------------------------
enum class KernelStatus { RegisteredWithOracle = 0, RegisteredRestricted, RequiresRegistration };
enum class KernelUse { ContinuousField = 0, DiscreteMask, Diagnostics, ExplicitUserSelection };
enum class KernelNormalization { UnitSum = 0, ExactOverlap, Discrete };
enum class BoundaryPolicy { MissingIsNaN = 0, Undefined };
enum class OracleKind { None = 0, Structural, Analytic, MonteCarlo };
const char* to_string(KernelStatus v);
const char* to_string(KernelUse v);
const char* to_string(BoundaryPolicy v);
const char* to_string(OracleKind v);

struct KernelOracleEvidence {
  OracleKind kind = OracleKind::None;
  bool ok = false;
  bool independent = false;
  std::string oracle_id;
  // bilinear_4quad 注册证据（ALG-P3-001_KERNEL_REGISTRY §3.3）。
  double max_interp_err = kNaN;
  double analytic_bound = kNaN;
  double err_over_bound = kNaN;
  double max_weight_sum_dev = kNaN;
  bool boundary_fail_closed = false;
  double zero_fill_error = kNaN;
};

struct KernelDescriptor {
  std::string kernel_id;
  KernelStatus status = KernelStatus::RequiresRegistration;
  std::vector<KernelUse> allowed_uses;
  KernelNormalization normalization = KernelNormalization::Discrete;
  bool supports_row_normalized = false;     // 面亮度语义（R）
  bool supports_column_normalized = false;  // 通量语义（S）
  bool error_bound_present = false;
  std::string error_bound_expression;
  bool boundary_defined = false;
  std::string boundary_definition;
  BoundaryPolicy boundary_policy = BoundaryPolicy::Undefined;
  KernelOracleEvidence oracle;
  bool production_science_default = false;

  bool allows(KernelUse u) const;
};

// 门结果：code 为空且 status==Ok 表示通过。
struct GateResult {
  Status status = Status::Ok;
  std::string code;
  std::string reason;
  bool ok() const { return status == Status::Ok; }
};

class KernelRegistry {
 public:
  static const KernelRegistry& frozen();

  const KernelDescriptor* find(const std::string& kernel_id) const;
  const std::vector<KernelDescriptor>& all() const { return kernels_; }

  // 注册前置结构门（缺 Oracle/误差界/边界定义 → REJECT）。G-P3-KRN-03/04。
  GateResult validate_registration(const KernelDescriptor& k) const;

  // 生产准入：未注册核、nearest 作连续场科学默认、语义不匹配 → REJECT。
  GateResult admit(const std::string& kernel_id, KernelUse use, P3Mode mode) const;

 private:
  std::vector<KernelDescriptor> kernels_;
  void add(const KernelDescriptor& k);
};

// ---------------------------------------------------------------------------
// 3. 线性算子 R / S 与跨 tile 邻域
// ---------------------------------------------------------------------------
struct OverlapEntry {
  int out_index = 0;
  int in_index = 0;
  double overlap_sr = 0.0;  // |Omega_j ∩ Omega'_i|
};

struct Geometry {
  std::vector<double> omega_in_sr;   // 输入像素立体角
  std::vector<double> omega_out_sr;  // 输出像素立体角（投影 Jacobian）
};

// 每个输出像素的邻域（可跨 tile）：missing_contributors>0 表示缺 tile/越界，
// 按 BoundaryPolicy::MissingIsNaN 输出 NaN 且**不零填**（C-P3-PROP-6）。
struct OutputNeighborhood {
  int out_index = 0;
  double omega_out_sr = kNaN;
  std::vector<int> in_index;
  std::vector<double> overlap_sr;
  int missing_contributors = 0;
};

struct NeighborhoodSet {
  Geometry geom;
  std::vector<OutputNeighborhood> outputs;
};

struct SparseOperator {
  int n_out = 0;
  int n_in = 0;
  std::vector<int> out_idx;
  std::vector<int> in_idx;
  std::vector<double> weight;
  std::vector<bool> valid_out;        // false → 输出 NaN（fail-closed）
  std::vector<double> coverage;       // Σ_j a_ij / Omega'_i（行归一语义）
  std::string kernel_id;
  bool row_normalized = false;
  bool column_normalized = false;

  double at(int o, int i) const;
  std::vector<double> row_sums() const;
  std::vector<double> col_sums() const;
  bool full_coverage(double tol) const;
};

// R_ij = a_ij / Omega'_i（ALG-P3-002/SPEC §1.3）。缺 tile → valid_out=false，不零填。
SparseOperator build_row_normalized(const NeighborhoodSet& nb, const std::string& kernel_id);
// S_ij = a_ij / Omega_j（列归一）。S_ij = R_ij * Omega'_i / Omega_j。
SparseOperator build_column_normalized(const NeighborhoodSet& nb, const std::string& kernel_id);

SparseOperator operator_from_entries_row(const std::vector<OverlapEntry>& e, const Geometry& g,
                                         int n_out, int n_in, const std::string& kernel_id);
SparseOperator operator_from_entries_col(const std::vector<OverlapEntry>& e, const Geometry& g,
                                         int n_out, int n_in, const std::string& kernel_id);

std::vector<double> apply_operator(const SparseOperator& op, const std::vector<double>& x);
std::vector<double> apply_operator_transpose(const SparseOperator& op, const std::vector<double>& y);

// ---- 2D 网格 / 跨 tile 邻域生成 ----
struct InputGrid2D {
  int width = 0;
  int height = 0;
  std::vector<double> value;  // size = width*height
  double at(int x, int y) const { return value[static_cast<std::size_t>(y) * width + x]; }
};

struct TileMask {
  int tile_px = 1;
  int tiles_x = 1;
  int tiles_y = 1;
  std::vector<unsigned char> present;  // size = tiles_x*tiles_y（0=缺 tile）
  bool present_at(int ix, int iy) const;
};

struct GridPlan {
  int out_width = 0;
  int out_height = 0;
  double out_origin_x = 0.0;  // 输出像素 (0,0) 中心在输入索引坐标中的位置
  double out_origin_y = 0.0;
  double out_step = 1.0;      // 输出像素间距（输入像素单位）
};

// 四象限最近中心双线性（ALG-P3-001_KERNEL_REGISTRY §3.1）：取夹住输出中心的 2×2 输入像素中心，
// 权重为线性距离分数 w，Σw=1。重叠面积记为 w * Omega'_i（使 R_ij = w_ij）。
NeighborhoodSet make_bilinear_4quad_neighborhood(const InputGrid2D& in, const TileMask& tiles,
                                                 const GridPlan& plan, double omega_in_sr,
                                                 double omega_out_sr);
// 最近邻（离散语义；仅 mask/诊断/显式选择）。用于 oracle 负向对照。
NeighborhoodSet make_nearest_neighborhood(const InputGrid2D& in, const TileMask& tiles,
                                          const GridPlan& plan, double omega_in_sr,
                                          double omega_out_sr);

// ---------------------------------------------------------------------------
// 4. covariance 传播 C_y = R C_x Rᵀ
// ---------------------------------------------------------------------------
struct DenseMatrix {
  int rows = 0;
  int cols = 0;
  std::vector<double> a;
  DenseMatrix() = default;
  DenseMatrix(int r, int c) : rows(r), cols(c), a(static_cast<std::size_t>(r) * c, 0.0) {}
  double& operator()(int r, int c) { return a[static_cast<std::size_t>(r) * cols + c]; }
  double operator()(int r, int c) const { return a[static_cast<std::size_t>(r) * cols + c]; }
  static DenseMatrix identity(int n);
  static DenseMatrix diagonal(const std::vector<double>& d);
};

DenseMatrix propagate_covariance(const SparseOperator& op, const DenseMatrix& c_in);
std::vector<double> matrix_diagonal(const DenseMatrix& m);
DenseMatrix correlation_kernel(const DenseMatrix& cy);
double max_abs_offdiag_correlation(const DenseMatrix& cy);

struct CholeskyResult {
  bool ok = false;
  std::string reason;
  DenseMatrix L;
};
CholeskyResult cholesky_factor(const DenseMatrix& a);
// 解 C z = b（C 正定）。失败（非正定/奇异）→ fail-closed，禁伪逆静默。
Status cholesky_solve(const DenseMatrix& c, const std::vector<double>& b, std::vector<double>* z,
                      std::string* reason);

// ---------------------------------------------------------------------------
// 5. 传播与输出帧 Q/W 重算
// ---------------------------------------------------------------------------
enum class CovarianceRepresentation {
  ExactFull = 0,          // C_y = R C_x Rᵀ 完整
  ExactCorrelationKernel, // 等价相关核（无近似）
  ApproximateCorrelation, // 带近似误差的核（须已签 epsilon_corr）
  DiagonalOnly,           // 仅对角（须相关核/近似误差，否则 REJECT）
  WeightDerived,          // 由权重标量反推 → 恒 REJECT
};
const char* to_string(CovarianceRepresentation v);

struct GateConfig {
  // OPEN 项 CF-T-P3-CORR-EPSILON / SO-07：未签字前不得自定值（fail-closed）。
  bool epsilon_corr_ratified = false;
  double epsilon_corr = kNaN;
  // 数值守卫（实现局部，不是科学阈值；不得覆盖任何冻结值）。
  double psf_sum_tol = 1e-9;
  double row_sum_tol = 1e-9;
  double bunit_tol = 1e-12;
};

struct SurfaceBrightnessInput {
  std::vector<double> x;         // SB（ADU/sr），size n_in
  DenseMatrix c_in;              // C_x，size n_in × n_in
  bool measurement_capable = true;
  bool flux_conversion_requested = false;
  bool omega_per_pixel_present = false;
  bool uncertainty_available = true;
  std::string uncertainty_unavailable_reason;
  CovarianceRepresentation covariance = CovarianceRepresentation::ExactFull;
  std::string variance_from;          // 必须为空或合法来源
  std::vector<std::string> weight_sources;
  bool uses_relative_weight_as_ivar = false;
  bool correlation_approx_error_available = false;
  double correlation_approx_error = kNaN;
  bool provenance_complete = true;
  std::vector<std::string> provenance_missing_keys;
};

struct SurfaceBrightnessResult {
  Status status = Status::Ok;
  std::string code;
  std::string reason;
  std::vector<double> y;         // SB 输出（无效像素 = NaN）
  std::vector<double> variance;  // diag(C_y)
  DenseMatrix c_y;
  std::vector<bool> valid;
  std::vector<double> coverage;
};

SurfaceBrightnessResult propagate_surface_brightness(const SparseOperator& r_op,
                                                     const SurfaceBrightnessInput& in,
                                                     const GateConfig& cfg);

struct PointSourceInput {
  SparseOperator s_op;                 // 通量算子 S（列归一）
  std::vector<double> x;               // 输入 SB（ADU/sr）；若 x_is_surface_brightness=false 则为积分通量 d
  DenseMatrix c_x;                     // Cov(x) 或 Cov(d)
  Geometry geom;                       // omega_in 用于 d = x*Omega 与 Cov(d)
  bool x_is_surface_brightness = true;
  std::vector<double> psf_p;           // 输入 effective PSF，Σp=1
  bool psf_present = false;
  bool point_information_present = false;  // 上游 W_info 存在（消费不重算）
  bool rebuildable_from_frames = false;
  bool photometric_scale_present = false;
  double a = 0.0;                      // 光度响应尺度
  bool effective_psf_present = false;
  bool effective_psf_fwhm_only = false;
  bool effective_psf_normalization_declared = false;
  // 禁止项（FZ-P3-QW-RECOMPUTE）
  bool input_qw_resampled = false;
  bool w_from_sum_input = false;
  bool upstream_w_recomputed = false;
  CovarianceRepresentation covariance = CovarianceRepresentation::ExactFull;
  bool correlation_approx_error_available = false;
  double correlation_approx_error = kNaN;
  bool optimism_audit_performed = false;
  bool diagonal_optimism_detected = false;
  std::string variance_from;
  std::vector<std::string> weight_sources;
  bool uses_relative_weight_as_ivar = false;
  bool variance_from_weight = false;
  bool provenance_complete = true;
  std::vector<std::string> provenance_missing_keys;
};

struct PointSourceResult {
  Status status = Status::Ok;
  std::string code;
  std::string reason;
  std::vector<double> f;   // 输出帧积分通量 f = S d
  std::vector<double> pi;  // 输出 effective PSF pi = S p
  DenseMatrix c_y;         // Cov(f)
  double Q = kNaN;
  double W = kNaN;
  double F_hat = kNaN;
  double var_F_hat = kNaN;
  bool frame_is_output_recompute = false;
};

PointSourceResult propagate_point_source_flux(const PointSourceInput& in, const GateConfig& cfg);

struct VisualizationInput {
  bool measurement_capable = false;
  bool writes_variance = false;
  bool writes_ivar = false;
  bool writes_point_information = false;
  bool degradation_reason_present = false;
  std::string degradation_reason;
};
struct VisualizationResult {
  Status status = Status::Ok;
  std::string code;
  std::string reason;
};
VisualizationResult propagate_visualization(const VisualizationInput& in);

// ---------------------------------------------------------------------------
// 6. 三模式 fail-closed 12 门 + 扩展门
// ---------------------------------------------------------------------------
struct ProductRecord {
  bool mode_declared = false;
  P3Mode mode = P3Mode::SurfaceBrightness;

  // surface_brightness
  bool flux_conversion = false;
  bool omega_per_pixel = false;
  bool measurement_capable = false;
  bool uncertainty_available = true;
  std::string uncertainty_unavailable_reason;
  Bunit signal_bunit = units::signal_sb;
  Bunit variance_bunit = units::sb_variance_out;
  Bunit ivar_bunit = units::sb_ivar_out;
  bool bunit_quadratic_law_ok = true;
  bool variance_diagonal_only = false;
  bool correlation_kernel_present = false;
  bool correlation_approx_error_available = false;
  double correlation_approx_error = kNaN;

  // point_source_flux
  bool psf_present = false;
  double psf_sum = 1.0;
  bool point_information_present = false;
  bool rebuildable_from_frames = false;
  bool photometric_scale_present = false;
  double photometric_scale = kNaN;
  bool effective_psf_present = false;
  bool effective_psf_fwhm_only = false;
  bool effective_psf_normalization_declared = true;

  // visualization
  bool writes_variance = false;
  bool writes_ivar = false;
  bool writes_point_information = false;

  // 全局权重/方差来源（FZ-GATE-MEDIAN-SNR / FZ-GATE-SUPPORT-COVERAGE / RULINGS #5）
  std::vector<std::string> weight_sources;
  std::string variance_from;
  bool uses_relative_weight_as_ivar = false;
  bool variance_from_weight = false;

  // Q/W 输出帧
  bool input_qw_resampled = false;
  bool w_from_sum_input = false;
  bool upstream_w_recomputed = false;
  CovarianceRepresentation covariance = CovarianceRepresentation::ExactFull;
  bool optimism_audit_performed = false;
  bool diagonal_optimism_detected = false;

  // provenance 最小集（FZ-PROV-MINIMAL-SET）
  bool provenance_complete = true;
  std::vector<std::string> provenance_missing_keys;
};

const std::vector<std::string>& forbidden_weight_source_tokens();
const std::vector<std::string>& forbidden_psfsw_product_keys();
bool token_is_forbidden_weight_source(const std::string& token);

// 退役对象（PSFSW-RETIRE-01）：psfsw_robust_weight **不是现行对象**
// （docs/ACSD_DESIGN.md §3.1 订正后；docs/detail/UNIFIED_MODEL.md:58；统一对象 14→13）。
// 旧产品若在 variance_from / weight_sources 声明该对象 ⇒ 显式拒绝 + 迁移提示，
// 不得静默接受，也不得再把它当作"在役的相对复合权重"。
// 注意：它同时仍在 forbidden_weight_source_tokens 里（拒绝面），本函数只提供
// "退役对象"这一更可诊断的判据，不替代 token 门。
bool is_retired_canonical_weight_object(const std::string& token);

// 退役对象的拒绝说明（含被拒对象、允许面与迁移提示）；未命中返回空串。
std::string retired_object_reject_detail(const std::string& token);

// 返回第一个违规；无违规则 status==Ok。
GateResult check_failclosed(const ProductRecord& rec, const GateConfig& cfg);
// 返回全部违规（用于覆盖与 mutation 断言）。
std::vector<GateResult> check_failclosed_all(const ProductRecord& rec, const GateConfig& cfg);

}  // namespace p3rsmp
}  // namespace acsd

#endif  // ACSD_P3_RSMP_H
