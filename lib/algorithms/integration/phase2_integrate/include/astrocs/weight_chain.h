/* weight_chain.h — Phase2 SNR → 逆方差权重链（科学计算核心）
 *
 * 权威依据（只读，不改；行号为引用时刻的现况）:
 *   - docs/ASTROCS_DESIGN.md §3.1:264「帧级 SNR 与稀疏 SNR 层是两个独立对象…
 *       稀疏层存控制点处的**绝对** SNR；稀疏层不以帧级 SNR 为尺度基准，消费时
 *       直接由绝对控制点重建为稠密 SNR 场（不经帧级 SNR 乘除）。两者共用同一物理
 *       定义与同一逐帧参考通量 F_ref，权重换算 w = SNR²/F_ref² 对两者一致」
 *   - docs/ASTROCS_DESIGN.md §3.1:263「HiPS 里存的是帧级 SNR 与稀疏控制点上的绝对
 *       SNR；权重是 Phase2 集成时按天球像素对应的输入帧集合**现场计算的派生量**」
 *   - docs/ASTROCS_DESIGN.md §3.1:267 + §2.4:240「三条 SNR 重建口径
 *       （dense / sparse_reconstruct / frame_reconstruct）是**重建方式**的选择，
 *       不是权重口径的选择：三者都产出同一物理量的稠密表示，都走同一条逆方差定权式」
 *   - docs/ASTROCS_DESIGN.md §5.3:439（w = 1/σ² = SNR²/F_ref²，F_ref 逐帧；Zackay & Ofek）
 *   - docs/science/PSF_SIGNAL_WEIGHT.md:75/87（w_k = SNR_k²/F_ref,k² ≡ 1/σ_F,k²；
 *       阶段二 w(x,y) = SNR(x,y)²/F_ref²）；docs/science/UNIFIED_SCIENCE_MODEL.md:62
 *       「SNR(x,y) 由稀疏控制点上的**绝对** SNR 重建（控制点值即绝对量本身，
 *       不乘/除帧级标量），F_ref 为**逐帧**参考通量」
 *   - eng/contracts/schemas/unified/sparse_snr_layer.schema.json（冻结）:
 *       sparse_snr_semantics const = "absolute_flux_type_snr"；控制点值 =
 *       F_ref/sigma_F,c（与 frame_snr 同口径、同逐帧 F_ref）；
 *       「消费时**不得**乘/除帧级 SNR 做还原」
 *   - docs/detail/algorithms_phase2/13_integration.md §4.0（稀疏层逐像素消费面）
 *
 * 本模块只做「SNR 元数据 → 逆方差权重」的换算，不做排异、不做叠加；调用方
 * （scheduler / stage2 接线）按报告给出的约定取用。
 *
 * **逐像素消费面（本模块的唯一稀疏层消费面）**：稀疏层的消费发生在**逐输出像素**
 * 的权重路径上——weight_from_sparse_layer_pixel() 按
 *   w(x,y) = (SNR_layer(x,y) / F_ref,k)² · g_k²
 * 现场换算，其中 SNR_layer 是层给出的**绝对**信噪比本身。**禁止**把它再乘/除任何
 * 帧级 SNR 标量（既违冻结 schema，也使权重口径变成「帧级² × 绝对」的错口径）。
 * 层值缺失不是「乘 1」：逐像素面的缺层/语义不符/越界/非正值一律显式降级
 * （generated_from_absent_layer）或判红（closure 非 kClosed），绝不静默照常出权。
 *
 * fail-closed 纪律（13_integration.md §7:80、DESIGN §3.4:154-156）:
 *   - 帧级 SNR 缺失/非有限/非正 → 拒绝，不静默退化为等权；
 *   - F_ref 缺失/非有限/非正 → 拒绝（换算不成立）；
 *   - 稀疏层存在但损坏/不可重建/越界 → 拒绝，**不得静默回退帧级**；
 *   - 逐像素权重面需要层而层缺失 → 显式降级（generated_from_absent_layer）或
 *     判红（require_sparse_layer_for_pixel_weights=true），**不得当作“乘 1”照常出权**；
 *   - 层声明为相对语义（非 absolute_flux_type_snr）→ 拒绝（冻结 schema 判红）。
 *   - SNR 语义非「通量型未加权原始 SNR」（如 CONTROL_WEIGHT_SNR 的相对质量权重
 *     support×snr² 或 median_source_snr 诊断量）→ 拒绝；
 *   - legacy_allow_weight_fallback 即使被显式置真，也只返回 fail-closed 并
 *     显式报出「权重链未闭合」，绝不把等权结果标记为成功。
 *
 * 单位:
  *   frame_snr / layer_snr / snr_value   : 无量纲 [1]（通量型 SNR = F_ref/sigma_F）
 *   reference_flux F_ref               : 与帧产品同通量标度 [ADU]
 *   weights w                          : [ADU^-2]（= 1/σ_F²，与 Phase1 W_info 同量纲）
 *
 * 前置条件（DESIGN §4.3:256 + WEIGHT-SCI-001 配对性定理「帧间独立」
 *   **配对性只要求同一帧内 SNR 与 F_ref 同源**，即
 *     SNR_k = a_k·F_ref,k/σ_k  ⇒  SNR_k²/F_ref,k² = a_k²/σ_k² = w_k
 *   成立当且仅当**该帧**的分子分母同源。**不要求跨帧相等。**
 *
 *   ⚠ F_ref **不是**组内公共常数：逐帧 F_ref 合法且必需（依据：
 *   FREF-BASELINE-001 的 scope="frame_independent_fixed_magnitude" 下
 *   F_ref,k = 10^(−0.4(m_ref−ZP_k))，ZP_k 依赖**该帧自己的**光学系统/滤镜
 *   ⇒ 不同指向、不同光学系统的帧**合法地**有不同的 F_ref,k。组公共常数口径
 *   等价于「不同光学系统的帧混装即报错」，并使 weight_mode=2
 *   在多指向拼接上完全不可用（实测 6/6 帧被拒）。用组标量在跨指向时
 *   **破坏**同源性。变更 claim: WEIGHT-FREF-PERFRAME-001。
 *
 *   现行为：逐帧取 f.ref_flux（>0）为准，未提供时回退组标量 reference_flux；
 *   两者都非有限/非正 ⇒ fail-closed（kUnclosedInvalidReferenceFlux）。
 *   未归一化/分母与 SNR 定义**不同帧或不同源**时换算仍不成立，调用方不得调用本模块。
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace astrocs {
namespace v6 {
namespace p2weight {

/* ------------------------------------------------------------------ */
/* SNR 语义判别（防「对象冒充」DESIGN §2:84）                            */
/* ------------------------------------------------------------------ */
enum class FrameSnrKind : int {
  /* 唯一合法输入：帧级未加权原始通量型 SNR = F_ref/σ_F（HiPS 头 frame_snr） */
  kFluxTypeUnweightedSnr = 0,
  /* CONTROL_WEIGHT_SNR §2/§4 的 snr_v / local_snr：无量纲相对质量权重，
     不是科学信噪比（SCI-CW §2a:49-50 / §7:120 明确禁止当科学 SNR）→ 拒绝 */
  kRelativeQualityWeight = 1,
  /* median_source_snr / snr_phot 等诊断量 → 拒绝（FZ-GATE-MEDIAN-SNR） */
  kDiagnosticMedianSnr = 2,
  kUnknown = 3,
};
const char* frame_snr_kind_token(FrameSnrKind k);

/* ------------------------------------------------------------------ */
/* 稀疏帧内 SNR 层（eng/contracts/schemas/unified/sparse_snr_layer.schema.json） */
/* ------------------------------------------------------------------ */
/* 控制点值的数值语义（层自带，冻结判别式）。schema 的 propertyNames 禁止层内出现
 * 名为 `snr`/`value` 的裸键，故层值走 SparseSnrPoint::snr 字段（内存面），而语义
 * 判别式落在本枚举上：层只能声明 **absolute_flux_type_snr**（= F_ref/σ_F,c，与
 * frame_snr 同口径、同逐帧 F_ref）；相对语义（按帧级/中位归一）在 schema 层判红，
 * 本模块在消费前也独立复核一次（不以「上游已校验」为由跳过）。 */
enum class SparseSnrSemantics : int {
  kAbsoluteFluxTypeSnr = 0,   /* 冻结语义：绝对通量型未加权 SNR（唯一合法） */
  kRelativeToFrameSnr = 1,    /* 相对因子（intra/帧内相对）→ 拒绝 */
  kUnspecified = 2,           /* 调用方未声明 → 视为未声明，不得当作合法绝对语义 */
};
const char* sparse_snr_semantics_token(SparseSnrSemantics s);

struct SparseSnrPoint {
  double x = 0.0;    /* 控制点 x（像素坐标） */
  double y = 0.0;    /* 控制点 y（像素坐标） */
  double snr = 0.0;  /* 未加权帧内 SNR 参考值 [1]（>0 且有限） */
};

/* ------------------------------------------------------------------ */
/* 重建算子（冻结词表；算子标识 = 唯一配置面，写入 manifest）             */
/* ------------------------------------------------------------------ */
/* 每个算子把「核 + 是否开 3×3 mesh 中值前置滤波 + 是否做值域钳制」**整组**
 * 绑成一个不可拆分的标识。为什么不做成独立布尔开关（见
 * docs/detail/algorithms_phase1/07_noise_snr.md §4.5、实验 EXP-04 §2.7/§4.4）：
 *   ① 值域钳制**不是可选项**：去掉它，光滑插值类在病态控制网格上失控
 *      （E 达 2.48e4），且会给出**负的 σ**（实测 min = −0.5585，非物理）；
 *   ② 中值前置滤波在默认目标域（地面/seeing-limited）**有害**
 *      （真实地面帧上劣 39~74%、解析可分辨域劣 9.3 倍），只在 HST 类高对比域必需；
 *   ③ 独立布尔可组合出「双线性 + 滤波」等**从未实测**的配置，标识化后不可表达。
 * 词表（token ↔ 语义）：
 *   natural_bicubic_spline_clip_v1             默认。可分离自然边界双三次样条
 *                                              + 钳到有效控制值值域。
 *   natural_bicubic_spline_clip_mesh_median_v1 同上，且在样条前插入 3×3 mesh
 *                                              中值滤波（边界 replicate，无条件）。
 *   bilinear_regular_grid_v1                   规则网格双线性（现行实现，保留为
 *                                              对照/回退；无钳制、无滤波）。
 *   nearest_control_point_v1                   最近控制点（散点模式唯一合法算子）。
 * 未识别 token ⇒ fail-closed（不得回退默认）。 */
enum class SparseReconOperator : int {
  kNaturalBicubicSplineClip = 0,
  kNaturalBicubicSplineClipMeshMedian = 1,
  kBilinearRegularGrid = 2,
  kNearestControlPoint = 3,
};
const char* sparse_recon_operator_token(SparseReconOperator op);
/* 默认算子（规则网格）。散点模式默认 = nearest_control_point_v1。 */
const char* sparse_recon_operator_default_token();
bool parse_sparse_recon_operator(const std::string& token, SparseReconOperator* out);
/* 该算子是否含 3×3 mesh 中值前置滤波（= 按数据来源的开关，默认关）。 */
bool sparse_recon_operator_uses_mesh_median(SparseReconOperator op);
/* 该算子是否做值域钳制（默认算子必须为真；不得单独关闭）。 */
bool sparse_recon_operator_clips_to_ctrl_range(SparseReconOperator op);

/* 按**数据来源**选择重建算子的显式规则（把 07_noise_snr.md §4.5 的选择规则做成
 * 可执行、可测的单点，避免各处自行判断）：
 *   high_contrast_unresolved_sources = true  ⇒ 高对比域档（cell 内含未分辨点源：
 *                                               空间高分辨率 / HST 类）
 *   false                                    ⇒ 默认档（地面 seeing-limited 与一般情形）
 * **无法判断时传 false**（默认目标域上 mesh 滤波有害；不得从控制网格自身推断）。 */
const char* sparse_recon_operator_for_source(bool high_contrast_unresolved_sources);

/* 稀疏帧内层几何（显式声明，写入 manifest；禁止隐式外推）:
 *   - regular_grid=true : 规则网格插值。nx*ny 控制点，行主序 j*nx+i。
 *       **节点落在所属 cell 的中心**（与 Phase2 UPM 8×8/tile 控制网格同一约定，
 *       upm.cpp 的 centered bilinear basis）：cell i 覆盖像素
 *       [grid_origin_x + i*dx, grid_origin_x + (i+1)*dx - 1]，
 *       其中心 = grid_origin_x + i*dx + (dx-1)/2
 *       ⇒ 必须 x0 = grid_origin_x + (dx-1)/2（y 同）。
 *       把节点当 cell 角点（x0 = grid_origin_x）会使整场平移半个 cell
 *       （Δ=64 时 31.5 px）；该错位由 cell 中心门 fail-closed 拦截。
 *       **查询坐标 = 像素中心坐标**（像素序号 p ↔ 坐标 p）。
 *       **定义域 = 层覆盖的 cell 并集**：[grid_origin_x − 0.5,
 *       grid_origin_x + nx·dx − 0.5]（y 同）——cell 内非节点处的值由插值给出，
 *       最外半个 cell 由端点节点常数延拓（与 SExtractor/photutils 的
 *       mesh 背景覆盖整帧同语义）。越出该并集 → fail-closed（不外推、
 *       不回退帧级）。
 *   - regular_grid=false: 最近控制点，必须显式给 max_radius_px>0；
 *                         超半径/未给半径 → fail-closed（不外推、不回退帧级）。
 *   - 控制点值语义见 sparse_snr_layer.schema.json：**绝对**通量型 SNR。 */
struct SparseSnrLayer {
  bool present = false;
  /* 控制点值语义（冻结）。必须显式置 kAbsoluteFluxTypeSnr；缺省 kUnspecified
   * 与 kRelativeToFrameSnr 都在逐像素消费面 fail-closed（不以「上游已校验」
   * 为由跳过复核）。 */
  SparseSnrSemantics semantics = SparseSnrSemantics::kUnspecified;
  std::vector<SparseSnrPoint> points;
  bool regular_grid = false;
  int nx = 0, ny = 0;
  double x0 = 0.0, y0 = 0.0, dx = 1.0, dy = 1.0;
  double grid_tol = 1e-6;      /* 网格位置一致性容差 [px] */
  double max_radius_px = -1.0; /* 散点模式覆盖半径 [px]；<=0 视为未声明 */
  /* 重建算子声明（token 见上；空串 = 默认算子）。
   * 该声明同时承载「是否开 mesh 中值滤波」——滤波由算子标识编码，
   * 不作为独立开关（理由见上）。 */
  std::string reconstruction_operator;
  /* cell 网格原点（像素坐标）。默认 0 = 帧原点。 */
  double grid_origin_x = 0.0;
  double grid_origin_y = 0.0;
};

/* 单次重建结果（进 manifest 的「重建算子与误差」）。 */
struct SparseReconstruction {
  std::string operator_id;                 /* 实际生效算子 token（"" = 未使用） */
  std::size_t n_control_points = 0;
  double node_reproduction_max_abs = 0.0;  /* 控制点自身复现最大绝对残差；应 ~0 */
  bool out_of_domain = false;              /* true = 查询点不在层定义域内 */
  /* 落地可审计量（全部进 manifest；不参与科学换算） */
  std::size_t n_invalid_control_points_filled = 0; /* NaN 节点按 nearest_valid 填充数 */
  bool mesh_median_applied = false;        /* 是否施加 3×3 mesh 中值前置滤波 */
  bool value_range_clipped = false;        /* 是否做值域钳制 */
  double clip_low = 0.0, clip_high = 0.0;  /* 钳制区间 [min,max]（未钳制 = 0/0） */
  double cell_center_offset_max_abs = 0.0; /* 节点相对 cell 中心的最大偏移 [px]；应 ~0 */
};

/* 预置重建器：把与查询点无关的预处理（网格校验 / NaN 填充 / mesh 中值 /
 * 自然样条二阶导）做一次，之后逐像素 eval 只做 O(nx) 求值。
 * 生产按输出像素现场求值，逐像素重做预处理会把 O(nx*ny) 乘进每个像素。
 * prepare/eval 不持有可变共享状态 ⇒ 同一实例可被多 worker 并发只读调用，
 * 结果与 worker 数无关（逐位一致）。 */
class SparseSnrReconstructor {
 public:
  /* 校验层 + 预处理。任何退化 ⇒ false 并写 err（fail-closed）。 */
  bool prepare(const SparseSnrLayer& layer, std::string* err);
  /* 在 (x,y) 求值（像素中心坐标）。未 prepare ⇒ false。 */
  bool eval(double x, double y, double* out_snr, SparseReconstruction* info,
            std::string* err) const;

  bool ready() const { return ready_; }
  SparseReconOperator op() const { return op_; }
  const char* operator_id() const { return sparse_recon_operator_token(op_); }
  std::size_t n_control_points() const { return n_control_points_; }
  double node_reproduction_max_abs() const { return node_residual_; }
  std::size_t n_invalid_control_points_filled() const { return n_filled_; }
  bool mesh_median_applied() const { return mesh_median_; }
  bool value_range_clipped() const { return clips_; }
  double clip_low() const { return clip_low_; }
  double clip_high() const { return clip_high_; }
  double cell_center_offset_max_abs() const { return cell_center_offset_; }

 private:
  void fill_info(SparseReconstruction* info) const;

  bool ready_ = false;
  bool regular_grid_ = false;
  SparseReconOperator op_ = SparseReconOperator::kNaturalBicubicSplineClip;
  int nx_ = 0, ny_ = 0;
  double x0_ = 0.0, y0_ = 0.0, dx_ = 1.0, dy_ = 1.0, tol_ = 1e-6;
  double grid_origin_x_ = 0.0, grid_origin_y_ = 0.0;
  double max_radius_px_ = -1.0;
  std::vector<double> grid_;               /* ny*nx，已填充（行主序 j*nx+i） */
  std::vector<double> my_;                 /* y 向自然样条二阶导（ny*nx） */
  std::vector<SparseSnrPoint> points_;     /* 散点模式 */
  bool clips_ = false;
  bool mesh_median_ = false;
  double clip_low_ = 0.0, clip_high_ = 0.0;
  double node_residual_ = 0.0;
  double cell_center_offset_ = 0.0;
  std::size_t n_filled_ = 0;
  std::size_t n_control_points_ = 0;
};

/* 由稀疏层在 (x,y) 重建帧内 SNR。失败返回 false 并写 err（fail-closed）。
 * 等价于 SparseSnrReconstructor::prepare + eval（单次调用走同一代码路径，
 * 逐位一致）。 */
bool reconstruct_sparse_snr(const SparseSnrLayer& layer, double x, double y,
                            double* out_snr, SparseReconstruction* info,
                            std::string* err);

/* ------------------------------------------------------------------ */
/* 标量换算                                                             */
/* ------------------------------------------------------------------ */
/* w = SNR² / F_ref² = 1/σ_F²。reference_flux 必须是**定义 snr 时所用的同一
 * 参考通量**（配对性只要求同一帧内 SNR 与 F_ref 同源，逐帧与组标量皆合法，
 * 口径与推导见本文件头「前置条件」一节）。任一输入非有限/非正 → false
 * （fail-closed）。 */
bool weight_from_snr(double snr, double reference_flux, double* out_weight,
                     std::string* err);



/* w = 1/Var(corrected)（Var>0 有限）[ADU^-2]：逐像素归一化方差的逆。
 * 这是 P2b 的正确权重面（w 是 Var(corrected) 的严格单调递减函数 ⇒ 高归一化
 * SNR 帧权重 ≥ 低归一化 SNR 帧由构造保证）。**禁止**由权重标量反推 variance
 * （upm.h:228）；本函数只做 1/var。Var<=0/非有限 → false（fail-closed）。 */
bool weight_from_corrected_variance(double variance, double* out_weight,
                                    std::string* err);

/* ------------------------------------------------------------------ */
/* 逐输出像素的多帧权重链                                                */
/* ------------------------------------------------------------------ */
struct FrameWeightInput {
  std::string frame_id;                       /* 追溯用（可空） */
  FrameSnrKind kind = FrameSnrKind::kUnknown; /* 必须 kFluxTypeUnweightedSnr */
  bool has_frame_snr = false;
  double frame_snr = 0.0;                     /* F_ref/σ_F，>0 有限 */
  /* 本帧**自己的**参考通量 F_ref,k（>0 有限；0 = 未提供，回退到组标量）。
   * 为什么必须逐帧（帧间独立 + FREF-BASELINE-001）：
   *   FREF-BASELINE 的 scope="frame_independent_fixed_magnitude" 下
   *   F_ref,k = 10^(−0.4(m_ref−ZP_k))，ZP_k 依赖**该帧自己的**光学系统/滤镜
   *   ⇒ 不同指向或不同光学系统的帧**合法地**有不同的 F_ref,k。
   *   配对性定理（WEIGHT-SCI-001）只要求**分子分母同源**（同一帧的 SNR 与 F_ref），
   *   **不要求**跨帧相等。组公共 F_ref 闸门口径等价于「不同光学系统的帧混装即报错」，
   *   并使 weight_mode=2 在多指向拼接上完全不可用
   *   （实测 6/6 帧被拒）。
   *   ⇒ 跨帧一致性降级为**报告字段**（见 module_adapters 的
   *   reference_flux_spread_*），不再是门。 */
  double ref_flux = 0.0;
  /* 可空。非空且 present ⇒ 本帧的权释放到**逐像素面**（本函数不给该帧标量权，
   * weights[k]=0、weight_deferred_to_pixel_path[k]=true）；层损坏/越界 ⇒ 判红。 */
  const SparseSnrLayer* sparse = nullptr;
  double x = 0.0, y = 0.0;                    /* 该帧像素坐标系下输出像素位置 */
  /* 可空乘性光度响应 g_k（>0 有限）。归一化含 corrected=(y−ĝ)/g_k 时必填：
   * 帧级权重须 w = SNR²/F_ref²·g_k²（Var(corrected)=Var(y)/g² ⇒ w 乘 g²；
   * q2-snr-smooth §2/§7）。nullptr = 未声明（按 g=1），仅当
   * policy.require_frame_gain=false 时合法；声明要求而缺 → fail-closed
   * （kUnclosedMissingGain），不得静默按 g=1 冒充。 */
  const double* gain = nullptr;
};

/* 权重链闭合状态。只有 kClosed 代表科学权重成立。 */
enum class WeightClosure : int {
  kClosed = 0,                               /* w=SNR²/F_ref² 全部有效 */
  kUnclosedMissingFrameSnr = 1,
  kUnclosedInvalidFrameSnr = 2,
  kUnclosedInvalidReferenceFlux = 3,
  kUnclosedSparseLayerUnreconstructible = 4,
  kUnclosedInvalidIntraSnr = 5,
  kUnclosedNonFiniteWeight = 6,
  kUnclosedWrongSnrSemantics = 7,
  kUnclosedLegacyFallbackRejected = 8,       /* 显式请求等权降级 → 仍 fail-closed */
  kUnclosedEmptyInput = 9,
  kBaselineEqualWeight = 10,                 /* 显式非生产基线，非闭合 */
  kUnclosedMissingGain = 11,                 /* require_frame_gain 但 gain 缺失 */
  kUnclosedInvalidGain = 12,                 /* gain 非有限/非正 */
  /* 逐像素权重面：该帧需要稀疏层但层未提供/未声明 present ⇒ 显式降级
     （默认，generated_from_absent_layer=true）或判红（policy 置真时）。 */
  kUnclosedSparseLayerRequiredMissing = 13,
  /* 逐像素面：重建出的层值非正/非有限。 */
  kUnclosedInvalidLayerSnr = 14,
};
const char* weight_closure_token(WeightClosure c);

struct WeightChainPolicy {
  /* 兼容旧调用方签名。**置真不会产生成功结果**：仅触发
     kUnclosedLegacyFallbackRejected + 显式错误串（消除静默假绿）。 */
  bool legacy_allow_weight_fallback = false;
  /* 1 = 调用方声明归一化含 /g_k ⇒ 每帧 gain 必填（缺一 fail-closed）。
     默认 0 = 兼容旧调用方（gain 缺失按 g=1，仅加性-only 方案 B 合法）。 */
  bool require_frame_gain = false;
  /* 逐像素权重面：true = 层缺失直接判红（kUnclosedSparseLayerRequiredMissing）；
     false = 该帧显式降级到帧级标量（结果 generated_from_absent_layer=true）。
     两者都不是「层值 = 1」的静默乘法。 */
  bool require_sparse_layer_for_pixel_weights = false;
};

struct WeightChainResult {
  bool ok = false;                /* 仅 kClosed 为真 */
  bool weight_chain_closed = false;
  bool production_allowed = false;
  WeightClosure closure = WeightClosure::kUnclosedMissingFrameSnr;
  std::string error;              /* kClosed 时为空 */
  /* "frame_snr"（帧级标量路径：该帧无有效层） | "sparse_snr_layer_absolute_snr"
     （该帧有有效层 ⇒ 权释放到逐像素面） | "none" */
  std::string weight_source;
  double reference_flux = 0.0;     /* 组标量（审计/回退用） */
  /* 逐帧**实际生效**的 F_ref,k = (输入 ref_flux>0 ? 输入 : 组标量)。
   * 权重 w_k = SNR_k² / reference_flux_k[k]² · g_k² 用的就是它
   * （与逐像素面 weight_from_sparse_layer_pixel 的 F_ref 逐帧同源）。 */
  std::vector<double> reference_flux_k;
  /* 逐帧层重建结果（仅供审计；层的权不在本函数给出，而在逐像素面
     weight_from_sparse_layer_pixel）。无层 = 0。 */
  std::vector<double> layer_snr;
  /* 逐帧实际帧级 SNR（无层时 = 输入 frame_snr；有效层时该帧权不由本函数
     产出，本行仅作审计）。 */
  std::vector<double> actual_snr;
  /* 逐帧：true = 该帧权释放到逐像素面（该帧有有效层）。 */
  std::vector<bool> weight_deferred_to_pixel_path;
  std::vector<double> frame_gain;  /* 逐帧 g_k（未声明 = 1.0；审计用） */
  std::vector<double> weights;     /* 逐帧 w = SNR²/F_ref,k²·g_k² [ADU^-2]；
                                      有效层所在帧为 0（须走逐像素面） */
  std::vector<std::string> sparse_operator_ids; /* 逐帧（无层 = ""） */
  std::vector<double> sparse_node_residual;     /* 逐帧 */
  /* 显式 legacy 请求时填充，仅供诊断；ok/weight_chain_closed 恒为 false。 */
  bool legacy_equal_weight_used = false;
  std::vector<double> diagnostic_equal_weights;
};

/* 多帧逆方差权重链（一个输出像素的一组输入）。 */
WeightChainResult compute_inverse_variance_weights(
    const std::vector<FrameWeightInput>& frames, double reference_flux,
    const WeightChainPolicy& policy = WeightChainPolicy());

/* ------------------------------------------------------------------ */
/* 逐像素消费面：稀疏层（绝对 SNR）→ w(x,y)                              */
/* ------------------------------------------------------------------ */
/* 稀疏层的**唯一**消费面 = 逐输出像素的权重路径。层值已是**绝对** SNR，故
 *   w(x,y) = (SNR_layer(x,y) / F_ref,k)² · g_k²   ≡   1/σ_F(x,y)²
 * （docs/science/UNIFIED_SCIENCE_MODEL.md:59、PSF_SIGNAL_WEIGHT.md:87、
 *   docs/detail/algorithms_phase2/13_integration.md §4.0）。
 * **不得**再乘/除帧级 SNR：层与帧级是两个独立对象，共用同一物理定义与同一逐帧
 * F_ref（ASTROCS_DESIGN §3.1:264；冻结 schema 明文「消费时不得乘/除帧级 SNR」）。 */
struct PixelWeightInput {
  std::string frame_id;                    /* 追溯用（可空） */
  const SparseSnrLayer* layer = nullptr;   /* 必填（非空且 present） */
  double x = 0.0, y = 0.0;                 /* 输出像素在该帧像素域的坐标 */
  double ref_flux_k = 0.0;                 /* 该帧**自己的** F_ref,k [ADU]，>0 有限 */
  /* 可空乘性光度响应 g_k（>0 有限）。归一化含 corrected=(y−ĝ)/g_k 时必填；
   * nullptr = 未声明（按 g=1），仅当 policy.require_frame_gain=false 时合法。 */
  const double* gain = nullptr;
};

struct PixelWeightResult {
  bool ok = false;                  /* 仅 closure==kClosed 为真 */
  bool weight_chain_closed = false;
  bool production_allowed = false;
  WeightClosure closure = WeightClosure::kUnclosedSparseLayerRequiredMissing;
  std::string error;                /* ok 时为空 */
  std::string weight_source;        /* "sparse_snr_layer_absolute_snr" | "none" */
  std::string sparse_operator_id;   /* 实际生效重建算子 token（"" = 未使用） */
  double sparse_node_residual = 0.0;/* 控制点自身复现最大绝对残差（审计） */
  /* 重建出的**绝对**层值 = F_ref/σ_F(x,y)，无量纲 [1]。ok 时 >0 有限。
   * 该值就是权重式分子里的 SNR，**不是**相对因子。 */
  double layer_snr = 0.0;
  double reference_flux_k = 0.0;    /* 实际生效 F_ref,k [ADU] */
  double gain = 1.0;                /* 实际生效 g_k（未声明 = 1.0，审计用） */
  double weight = 0.0;              /* w(x,y) [ADU^-2]；ok 时 >0 有限 */
  /* true = 该帧没有可用层，本结果**不是**层消费结果：调用方须显式降级到帧级
   * 标量路径（weights=0、closure 非 kClosed）。它**不等于**「层值按 1 处理」。 */
  bool generated_from_absent_layer = false;
  /* 逐像素面的**量纲/口径自证**（可被调用方与门直接断言）：
   *   weight_units       = "ADU^-2"
   *   snr_units          = "dimensionless"
   *   reference_flux_units = "ADU"
   *   dimensional_identity = weight * reference_flux_k² - layer_snr² * gain²
   * 后式应恒为 0（相对残差 ≤ 1e-15），即 w[ADU^-2]·F_ref[ADU]² = SNR²[1]·g²。 */
  const char* weight_units = "ADU^-2";
  const char* snr_units = "dimensionless";
  const char* reference_flux_units = "ADU";
  double dimensional_identity = 0.0;
};

/* 单帧单像素的层消费。失败语义（全部 fail-closed，无「乘 1」）：
 *   - policy.require_sparse_layer_for_pixel_weights=true 且层缺失/未 present
 *     → kUnclosedSparseLayerRequiredMissing；
 *   - 层 present 但 semantics != kAbsoluteFluxTypeSnr → kUnclosedWrongSnrSemantics
 *     （相对语义层混入即判红）；
 *   - 层 present 但不可重建/越界 → kUnclosedSparseLayerUnreconstructible；
 *   - 重建值非有限/非正 → kUnclosedInvalidLayerSnr；
 *   - F_ref,k 非有限/非正 → kUnclosedInvalidReferenceFlux；
 *   - gain 缺（require_frame_gain）→ kUnclosedMissingGain；gain 非法 → kUnclosedInvalidGain。
 * 层缺失且 policy 未要求层时：返回 ok=false、generated_from_absent_layer=true
 * （**显式降级**，调用方须改走帧级标量路径），而不是返回 weight=1/σ²(帧级)。 */
PixelWeightResult weight_from_sparse_layer_pixel(
    const PixelWeightInput& in,
    const WeightChainPolicy& policy = WeightChainPolicy());

/* 同上，但复用已 prepare 的重建器（生产按输出像素现场求值，避免逐像素重做
 * 预处理；算法与 weight_from_sparse_layer_pixel 逐位一致）。
 * reconstructor 必须 ready()；layer 仍用于 semantics 复核。 */
PixelWeightResult weight_from_sparse_layer_pixel_prepared(
    const SparseSnrReconstructor& reconstructor, const PixelWeightInput& in,
    const WeightChainPolicy& policy = WeightChainPolicy());

/* 显式、非生产的等权基线（ablation/baseline 比较用）。它**不是**权重链闭合：
   weight_chain_closed=false, production_allowed=false。 */
WeightChainResult make_equal_weight_baseline(std::size_t n_frames);

}  /* namespace p2weight */
}  /* namespace v6 */
}  /* namespace astrocs */
