// ── RETIRED-CODE-RETAINED (ENGINEERING_SPEC §2 保留则注释) ─────────────
// WHAT:       V6 Phase3 投影内核层（registry v3：TAN/SIN/CAR/AIT 四内核 + 逐像素立体角 Ω'
//             + R/S 行/列归一二元语义 + 计划/奇点/wrap）。合同锚 = p3_proj_v6.h 头注。
// WHY-KEPT:   ① contracts/schemas/projection_registry.schema.json:5,9 把本文件登记为
//             projection registry 的 implementation 导出锚（contracts/** 不属本任务文件域）；
//             ② 任务书 CLEAN-401 步骤 7 明令「v6 SIN 内核容差问题转 FIX-406，不在本任务删」，
//             且 FIX-406 正在本文件上做往返 Oracle 复核（run/FIX-406/ 实时在写）；
//             ③ 本内核是 p3_projection_registry.h 冻结表 SIN/CAR/AIT = kKernelOnly 的
//             「实现未声明 = 隐藏能力（禁止）」判据的对照面。
// STATUS:     内核-only，非产品声明，未接入生产。生产 projection target
//             astrocs_p3_projection_wcs 只编 p3_wcs.cpp（TAN，lib/algorithms/projection/
//             CMakeLists.txt:19）；本文件仅被 tests/unit/v6_p3_proj、tests/integration/v6_p3、
//             lib/algorithms/projection/tests/p3wcs 三个测试 target 编译。
//             **已知缺陷（FIX-406 交接）**：p3_proj_v6.cpp:195 sin_world2pix 用
//             ctheta = sqrt(1 - stheta^2)，投影中心 theta→pi/2 时灾难性消去 ⇒ 往返误差
//             ∝ 1/离轴距离 × 1/像素角尺度、无上界：0.5"/px 实测 2.5e-5 px、0.05"/px 1.0e-4 px、
//             精确参考像素最坏 6.1e-3 px（0.5"/px）/ 1.7e-2 px（0.05"/px）；
//             独立 Oracle（astropy 7.0.1 / WCSLIB 8.4）与独立切基式同 WCS 可达 ~2.5e-10 px
//             ⇒ 属实现条件数缺陷，非双精度固有极限。证据 run/FIX-406/SIN_ROUNDTRIP_ORACLE.md。
//             复现门（修好即转红）：tests/unit/v6_p3_proj/sin_roundtrip_gate.py
//             —— 偏差仍复现 = rc 0（绿）；内核被修好 = rc 1（红），强制同步本登记块与
//             p3_projection_registry.h 的已知偏差登记。
// EXIT:       ① FIX-406 裁决「修好」⇒ 本块改为在役说明、删除已知缺陷段、把 SIN 行从
//             kKernelOnly 提升为产品声明（需独立往返 Oracle + 适用域声明）；
//             ② FIX-406 裁决「退役」⇒ 随 v6 家族整体删除，删除需同批改
//             contracts/schemas/projection_registry.schema.json:5,9、
//             lib/algorithms/projection/tests/p3wcs/CMakeLists.txt:16-19 与
//             p3_projection_registry_test.cpp、tests/unit/v6_p3_proj/**、
//             tests/integration/v6_p3/**，以及 eng/ci/checks.json 的 v6_p3_proj_* ctest_targets
//             （DOC-403 文件域）。
// AUTHORITY:  ENGINEERING_SPEC.md §2（历史实现处置：保留则注释）；ASTROCS_DESIGN.md §6.3
//             （未实现的投影被选择时显式报「不支持」，当前仅 TAN 可用）；
//             lib/algorithms/projection/p3_projection_registry.h「已知偏差登记（FIX-406）」；
//             工程控制/RELEASE-04/GAP_AUDIT.md G3-6 / G2-1。
// ──────────────────────────────────────────────────────────────────────
// lib/algorithms/projection/p3_proj_v6.h — V6 Phase3 投影实现层（标准 FITS WCS Paper II
// registry v3 已实现四投影 + 逐像素立体角 Ω + R/S 行/列归一二元语义 + 计划/奇点/wrap）
//
// 任务: IMPL-P3-PROJ-001（wave 5, write_scope=lib/algorithms/projection/）
// 上位冻结:
//   * ASTROCS_DESIGN.md §5.3（内置多种投影，首批冻结 TAN/SIN/CAR/AIT/STG/MOL/
//     CEA/ZEA 八投影；每投影声明适用域/奇点/经度 wrap/轴手性/CRPIX/CRVAL/CD/
//     PC/CDELT/CTYPE；新增投影经 registry 注册并附独立往返 Oracle）。
//   * DESIGN-P3-001 §2（WCS 计划）/§3（反向映射、逐像素面积元、禁止平面近似）/
//     §4（C_y=R C_x Rᵀ）。
//   * ALG-P3-001 §1.3（统一线性模型：Ω'_i=|det(d sky/d pixel)|_i、
//     R_ij=|Ω_j∩Ω'_i|/Ω'_i 行归一、S_ij=|Ω_j∩Ω'_i|/Ω_j 列归一、S_ij=R_ij Ω'_i/Ω_j）
//     §2.4（逐像素面积元必须真实计算；禁常数 Ω）+ 独立 Oracle 实测值。
//   * docs/contracts/v6/frozen/01_DATA_CONTRACT_FREEZE.md（Omega 单位 sr；
//     phase3_var_out BUNIT=(signal BUNIT)²）；docs/contracts/v6/data/08_phase3.md §2。
//   * FZ-P3-OMEGA-NONCONST（FROZEN）：常数 Ω 冒充逐像素面积元 → REJECT。
//   * 单位表: signal_sb=ADU/px^2, sb_variance_out=ADU^2/px^4, W_info=ADU^-2,
//     psfsw_robust_weight=1；本层 Ω 单位 = sr（phase3.v1.omega.units="sr"）。
//
// 关键约定（标准 FITS WCS，独立 oracle 用 astropy/WCSLIB 交叉验证）:
//   * CD-only（禁 PC+CDELT），FITS 1-based 关键字 / 内部 0-based 像素中心：
//     pixel x -> FITS x+1。
//   * TAN/SIN：zenithal，参考点 θ0=CRVAL2（legacy 冻结逐式路径，与 astropy 一致）。
//   * CAR/AIT：参考点 (φ0,θ0)=(0,0)，native 极恒在 θ=+90°；**CRVAL2 进入映射**——
//     按 Paper II §2.2 三 Euler 角 (α_p, δ_p, φ_p) 解旋转，LONPOLE 取标准默认
//     （δ0 ≥ θ0 ⇒ 0°，否则 180°）。平面中间坐标 CAR: X=φ, Y=θ（declination 随 y
//     增加）；AIT: γ=√2/√(1+cosθcos(φ/2))，X=2γ cosθ sin(φ/2), Y=γ sinθ（等积）。
//   * CAR native 极行 θ=±90° 整行塌缩（Ω=0、RA 无定义）⇒ fail-closed 拒绝
//     （|θ|≥90° → kParam，正向/逆向一致）。
//   * 已实现四投影统一 |CRVAL dec|≤85° 中心守卫；域界 fail-closed：
//     TAN r≥π/2 / SIN ρ>1 / AIT A>1（A=xp²/4+yp²，标准椭圆域 A≤1）→ kHemisphere；
//     CAR |θ|≥90° → kParam。
//
// 与 legacy registry v1 的关系:
//   * 本层 kProjectionRegistryVersion=3 是 V6 唯一在役 registry。legacy v1
//     （p3_projection.h/.cpp，kP3ProjectionRegistryVersion=1）已 **RETIRED**（退场）：
//     CAR 用 Y=−θ（declination 反号）、AIT 缺 √2、AIT 域判据 A<2、CRVAL2 不进映射
//     四项偏差保留为历史对照（ALG-P3-PROJ-IMPL-001 §15.9 v1 偏差表 + 证据门
//     p3_proj_legacy_deviation.py），禁止新消费方引用；本层不再"保留 legacy 符号
//     零改动"式回避，legacy 偏差不构成本层行为依据。
//   * registry 权威冻结集合 = ASTROCS_DESIGN.md §5.3 八投影
//     （TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA）；本层 v3 注册已实现子集（当前 4 项，
//     STG/MOL/CEA/ZEA 实施归 P3-001，GAP-011）。
#ifndef ASTROCS_P3_PROJ_V6_H
#define ASTROCS_P3_PROJ_V6_H

#include <cstddef>
#include <string>

namespace astrocs::phase3proj::v6 {

// 版本化 registry 版本（DESIGN §5.3 八投影冻结集 + 变更 claim）。
// v1 = 首批四投影，CRVAL2 不进映射（**已 RETIRED**，见下）；
// v2 = CAR/AIT 符号与 √2 修正，但 CRVAL2 仍不进映射、AIT 域 A<2；
// v3 = Paper II §2.2 三 Euler 角把 CRVAL2（含 LONPOLE 默认）纳入 CAR/AIT 映射 +
//      AIT 域界 A≤1 + CAR native 极行 θ=±90° fail-closed。
// 表内容或语义变化必须递增此版本并在 ALG-P3-PROJ-IMPL-001 §15 登记变更 claim。
constexpr int kProjectionRegistryVersion = 3;

// registry 权威冻结集合（ASTROCS_DESIGN.md §5.3 首批八投影）。本层 kRegistry 是其
// 已实现子集；新增投影必须先落在该集合内并附独立往返 Oracle（DESIGN §5.3）。
//
// ⚠ **本层是内核 registry, 不是产品声明**（FIX-205, 2026-09-20）：本表行只表示
// 「内核已实现 + 单测/Oracle 覆盖」；**产品可声明集**的唯一权威 =
// lib/algorithms/projection/p3_projection_registry.h（当前 D={TAN}）。本表中
// 非产品声明的行必须在该文件冻结表内标为 kKernelOnly（跨注册表一致判据见
// tests/p3wcs/p3_projection_registry_test.cpp C5）。**禁止**把本表行当作
// 「已支持投影」对外声明（DESIGN §5.3「禁止声称支持」）。
constexpr int kFrozenProjectionCount = 8;
const char* const* registry_frozen_set(int* count);
bool registry_is_frozen_code(const char* code);   // 冻结集合成员判定（大小写敏感）

// phase3.v1.omega 单位（docs/contracts/v6/data/08_phase3.md §2）。
constexpr const char* kUnitOmegaSr = "sr";

enum class ProjectionId : int {
    kTAN = 0,   // gnomonic 局部切平面
    kSIN = 1,   // orthographic
    kCAR = 2,   // plate carrée
    kAIT = 3    // Aitoff（等积宽场/全天空展示）
};

enum class ProjStatus : int {
    kOk = 0,
    kParam = 1,         // 参数非法 / 空指针 / 非有限 / CAR native 极行 |θ|≥90°
    kUnsupported = 2,   // 未知投影码 / 越界 id（registry 未注册）
    kHemisphere = 3     // 中间坐标越投影域（TAN r≥π/2 / SIN ρ>1 / AIT A>1）
};

// 投影 descriptor（CD-only，FITS 1-based CRPIX，内部 0-based 像素中心）。
struct Descriptor {
    ProjectionId id = ProjectionId::kTAN;
    double crval_ra_deg = 0;
    double crval_dec_deg = 0;
    double crpix_x = 0;
    double crpix_y = 0;
    double cd[2][2] = {{0, 0}, {0, 0}};   // deg/px, FITS 顺序 CD[i][j]
    int width_px = 0;
    int height_px = 0;
};

// 经纬方向 + CTYPE 规则 + 适用天区/奇点/合法 FOV 的六要素声明（DESIGN §5.3）。
struct Spec {
    ProjectionId id;
    const char* code;
    const char* ctype1;
    const char* ctype2;
    double max_abs_crval_dec_deg;   // 中心 |CRVAL2| 守卫
    double max_fov_deg;             // 合法 FOV 声明（非 make 硬门）
    const char* singularity_kind;   // 奇点声明（静态串）
    ProjStatus (*pix2world)(const Descriptor*, double, double, double*, double*);
    ProjStatus (*world2pix)(const Descriptor*, double, double, double*, double*);
};

// ---- registry（唯一入口，fail-closed，无 CLI switch）----
const Spec* registry_table(int* count);
const Spec* registry_find(const char* code);       // 精确匹配；未注册 -> nullptr
const Spec* registry_find_id(ProjectionId id);     // 越界 -> nullptr
int registry_selfcheck();                          // 全过 0，否则首个失败行索引+1

// ---- 计划（DESIGN-P3-001 §2）----
// 足迹合法域 + 奇点 + RA wrap + 逐像素 Ω 采样摘要。
struct Plan {
    ProjStatus status = ProjStatus::kParam;
    Descriptor descriptor{};
    double fov_x_deg = 0;             // 边界采样 RA unwrap span
    double fov_y_deg = 0;             // 边界采样 Dec span
    double dec_min_deg = 0;
    double dec_max_deg = 0;
    double pole_margin_deg = 0;       // 90 − max(|dec_min|,|dec_max|)
    bool crosses_ra_wrap = false;     // 足迹跨 0/360
    bool domain_valid = false;        // 全部采样像素在投影域内
    bool singularity_free = false;    // 全部采样像素离奇点有正 margin
    double min_domain_margin = 0;     // 投影特定域 margin（>0 = 域内）
    double omega_min_sr = 0;          // 采样网格逐像素 Ω 极值
    double omega_max_sr = 0;
    double omega_max_min_ratio = 0;   // max/min
    int n_omega_samples = 0;
};

// make: 校验序（out 非空→registry id→parity→|dec|≤85°→scale>0→W,H∈[1,20000]
//       →G1 CD 构造→四角投影域守卫）；失败时 *out 零初始化。
//       四角守卫对 CAR 即「禁触碰 native 极行 |θ|≥90°」（禁止整行塌缩进入产物）。
ProjStatus make(ProjectionId id,
                double centre_ra_deg, double centre_dec_deg,
                double scale_deg_per_px,
                int width_px, int height_px,
                const char* parity, double rotation_pa_deg,
                Descriptor* out);

ProjStatus pix2world(const Descriptor* d, double x, double y,
                     double* ra_deg, double* dec_deg);
ProjStatus world2pix(const Descriptor* d, double ra_deg, double dec_deg,
                     double* x, double* y);

// FITS 关键词文本（CTYPE 经 registry 解析；空 descriptor/未注册 -> 空串）。
std::string fits_keywords(const Descriptor* d);

// plan: 构建 descriptor 并做足迹/奇点/wrap/Ω 采样；失败不产半成品。
ProjStatus plan(ProjectionId id,
                double centre_ra_deg, double centre_dec_deg,
                double scale_deg_per_px,
                int width_px, int height_px,
                const char* parity, double rotation_pa_deg,
                Plan* out);

// ---- 逐像素立体角 Ω_i (sr)，真实计算（禁常数 Ω；FZ-P3-OMEGA-NONCONST）----
// pixel_solid_angle: 像素四角经真实 WCS 反向映射到球面后，用球面四边形盈余
//   （Van Oosterom & Strackee：tan(E/2)=|a·(b×c)|/(1+a·b+b·c+c·a)）积分；
//   四角任一越域 -> 返回该非 OK 状态且 *omega_sr 不变（fail-closed，禁零填）。
ProjStatus pixel_solid_angle(const Descriptor* d, double x, double y,
                             double* omega_sr);

// pixel_solid_angle_differential: 微分面积元 |∂r/∂x × ∂r/∂y|（r=天球单位向量，
//   中心差分）；与盈余法互为独立面积算法（同一 WCS 映射），用于交叉校验。
ProjStatus pixel_solid_angle_differential(const Descriptor* d, double x, double y,
                                          double* omega_sr);

// solid_angle_grid: 行主序填充 width*height 个 Ω(sr)；越域像素写 NaN 并返回
//   首个非 OK 状态（状态数组可选，逐像素状态）。
ProjStatus solid_angle_grid(const Descriptor* d, double* omega_out,
                            ProjStatus* status_out);

// ---- R/S 行/列归一二元语义（ALG-P3-001 §1.3，R 与 S 不可互替）----
// R_ij = A_ij/Ω'_i（行归一：Σ_j R_ij = 1，常量面亮度不变量）
// S_ij = A_ij/Ω_j （列归一：Σ_i S_ij = 1，点源总通量守恒）
// S_ij = R_ij Ω'_i/Ω_j
enum class SampleSemantics : int {
    kSurfaceBrightnessRowNorm = 0,   // surface_brightness: 仅行归一
    kPointSourceFluxColNorm = 1,     // point_source_flux: 仅列归一
    kVisualizationNone = 2           // visualization: 非科学采样，无归一
};

// 语义 -> 必需归一（row/col）；未知语义 -> false/false 并返回 kParam。
ProjStatus semantics_required_normalization(SampleSemantics s,
                                            bool* row_required,
                                            bool* col_required);
// 声明的归一是否与语义一致（R/S 不可互替门）。
bool semantics_compatible(SampleSemantics s, bool row_normalized,
                          bool col_normalized);

// A: m 输出像素 × n 输入像素（行主序 A[i*n+j]=|Ω_j∩Ω'_i|，单位 sr）。
// Ω_out[i] = Ω'_i；Ω_in[j]=Ω_j。
// 任一 Ω ≤0 / 非有限 / 空指针 / 维度 ≤0 -> kParam 且输出零初始化（fail-closed）。
ProjStatus row_normalise(const double* a_overlap_sr, int m, int n,
                         const double* omega_out_sr, double* r_out);
ProjStatus col_normalise(const double* a_overlap_sr, int m, int n,
                         const double* omega_in_sr, double* s_out);
ProjStatus row_to_col(const double* r, int m, int n,
                      const double* omega_out_sr, const double* omega_in_sr,
                      double* s_out);
ProjStatus matrix_row_sums(const double* r, int m, int n, double* sums);
ProjStatus matrix_col_sums(const double* s, int m, int n, double* sums);

// ---- 单位/量纲注记（冻结表）----
// signal_sb=ADU/px^2; sb_variance_out=ADU^2/px^4; W_info=ADU^-2;
// Q=ADU^-1; F_hat=ADU; psfsw_robust_weight=1; phase3_var_out=BUNIT^2。
// Ω 只以 sr 出现，禁与 variance/weight/ivar 混名。
constexpr const char* kUnitSignalSb = "ADU/px^2";
constexpr const char* kUnitSbVarianceOut = "ADU^2/px^4";
constexpr const char* kUnitWInfo = "ADU^-2";
constexpr const char* kUnitPsfswWeight = "1";

}  // namespace astrocs::phase3proj::v6

#endif  // ASTROCS_P3_PROJ_V6_H
