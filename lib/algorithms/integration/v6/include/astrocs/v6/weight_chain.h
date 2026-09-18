/* weight_chain.h — Phase2 SNR → 逆方差权重链（科学计算核心）
 *
 * 权威依据（只读，不改）:
 *   - ASTROCS_DESIGN.md §4.3:255-261「逆方差叠加：SNR → 逆方差权重；
 *       w = 1/σ² = SNR²/F_ref² ∝ SNR²；不是直接用 SNR 加权」
 *   - ASTROCS_DESIGN.md §3.4:163-175「帧级 SNR 是未加权的原始通量型信噪比
 *       F_ref/σ_F（Horne 1986），写入 HiPS 头；稀疏 SNR 层启用时 实际 SNR = 帧级 × 帧内」
 *   - docs/plugins/algorithms_phase1/07_noise_snr.md §4.1:55-70（通量型口径，
 *       w_k = SNR_k(F_ref)² / F_ref² = 1/σ_F,k²；稀疏层每控制点存未加权 SNR）
 *   - docs/plugins/algorithms_phase2/13_integration.md §4.0:32-38
 *       （自动检测稀疏层；有→帧级×帧内，无→帧级；损坏/不可重建→明确失败，
 *        不得静默回退帧级）
 *   - docs/algorithms/v6/phase2-point/ALG-P2-POINT-001_SPEC.md:116-132
 *       （SNR_k = F_ref·sqrt(W_info,k)；SNR_combined² = Σ SNR_k² 仅独立帧成立）
 *   - contracts/schemas/unified/sparse_snr_layer.schema.json（control_points{x,y,sparse_snr_value}）
 *
 * 本模块只做「SNR 元数据 → 逆方差权重」的换算与合成，不做排异、不做叠加；
 * 调用方（scheduler / stage2 接线）按报告给出的约定取用。
 *
 * fail-closed 纪律（DESIGN §3.4:156、13_integration.md §7:80）:
 *   - 帧级 SNR 缺失/非有限/非正 → 拒绝，不静默退化为等权；
 *   - F_ref 缺失/非有限/非正 → 拒绝（换算不成立）；
 *   - 稀疏层存在但损坏/不可重建/越界 → 拒绝，**不得静默回退帧级**；
 *   - SNR 语义非「通量型未加权原始 SNR」（如 CONTROL_WEIGHT_SNR 的相对质量权重
 *     support×snr² 或 median_source_snr 诊断量）→ 拒绝；
 *   - legacy_allow_weight_fallback 即使被显式置真，也只返回 fail-closed 并
 *     显式报出「权重链未闭合」，绝不把等权结果标记为成功。
 *
 * 单位:
 *   frame_snr / intra_snr / actual_snr : 无量纲 [1]
 *   reference_flux F_ref               : 与帧产品同通量标度 [ADU]
 *   weights w                          : [ADU^-2]（= 1/σ_F²，与 Phase1 W_info 同量纲）
 *
 * 前置条件（DESIGN §4.3:256 + WEIGHT-SCI-001 配对性定理）: 调用前帧已由 UPM
 *   归一到公共通量尺度，F_ref 为**组内公共常数**；且传入的 reference_flux 必须
 *   **等于定义帧级 frame_snr 时所用的那个参考通量**（配对性:
 *     SNR_f = a_f·F_ref/σ_f  ⇒  SNR_f²/F_ref² = a_f²/σ_f² = w_f
 *   成立当且仅当分子分母同源）。逐帧 F_ref（各帧自己的检出通量）会丢掉帧间标度
 *   因子 a_f²，且使存头 SNR 混入本帧检出亮度（帧间不可比较）⇒ **不得使用**。
 *   未归一化/分母与 SNR 定义不同源时换算不成立，调用方不得调用本模块。
 *   注: 本条为约定澄清，**签名与实现不变**（判为正确；缺陷在 Phase1 写侧）。
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
/* 稀疏帧内 SNR 层（contracts/schemas/unified/sparse_snr_layer.schema.json） */
/* ------------------------------------------------------------------ */
struct SparseSnrPoint {
  double x = 0.0;    /* 控制点 x（像素坐标） */
  double y = 0.0;    /* 控制点 y（像素坐标） */
  double snr = 0.0;  /* 未加权帧内 SNR 参考值 [1]（>0 且有限） */
};

/* 重建算子（显式声明，写入 manifest；禁止隐式外推）:
 *   - regular_grid=true : 规则网格双线性插值（nx*ny 控制点，行主序 j*nx+i，
 *                         x=x0+i*dx, y=y0+j*dy）；越界 → fail-closed。
 *   - regular_grid=false: 最近控制点，必须显式给 max_radius_px>0；
 *                         超半径/未给半径 → fail-closed（不外推、不回退帧级）。 */
struct SparseSnrLayer {
  bool present = false;
  std::vector<SparseSnrPoint> points;
  bool regular_grid = false;
  int nx = 0, ny = 0;
  double x0 = 0.0, y0 = 0.0, dx = 1.0, dy = 1.0;
  double grid_tol = 1e-6;      /* 网格位置一致性容差 [px] */
  double max_radius_px = -1.0; /* 散点模式覆盖半径 [px]；<=0 视为未声明 */
};

/* 单次重建结果（进 manifest 的「重建算子与误差」）。 */
struct SparseReconstruction {
  std::string operator_id;                 /* "" = 未使用；否则 bilinear_regular_grid_v1 /
                                              nearest_control_point_v1 */
  std::size_t n_control_points = 0;
  double node_reproduction_max_abs = 0.0;  /* 控制点自身复现最大绝对残差；应 ~0 */
  bool out_of_domain = false;              /* true = 查询点不在层定义域内 */
};

/* 由稀疏层在 (x,y) 重建帧内 SNR。失败返回 false 并写 err（fail-closed）。 */
bool reconstruct_sparse_snr(const SparseSnrLayer& layer, double x, double y,
                            double* out_snr, SparseReconstruction* info,
                            std::string* err);

/* ------------------------------------------------------------------ */
/* 标量换算                                                             */
/* ------------------------------------------------------------------ */
/* w = SNR² / F_ref² = 1/σ_F²。reference_flux 必须是**定义 snr 时所用的同一
 * 参考通量**（组内公共 F_ref，配对性定理；逐帧参考会丢掉 a_f²）。
 * 任一输入非有限/非正 → false（fail-closed）。签名/实现保持正确不变。 */
bool weight_from_snr(double snr, double reference_flux, double* out_weight,
                     std::string* err);

/* 实际 SNR = 帧级 × 帧内（DESIGN §3.4:174）。任一非有限/非正 → false。 */
bool compose_actual_snr(double frame_snr, double intra_snr, double* out_snr,
                        std::string* err);

/* ------------------------------------------------------------------ */
/* 逐输出像素的多帧权重链                                                */
/* ------------------------------------------------------------------ */
struct FrameWeightInput {
  std::string frame_id;                       /* 追溯用（可空） */
  FrameSnrKind kind = FrameSnrKind::kUnknown; /* 必须 kFluxTypeUnweightedSnr */
  bool has_frame_snr = false;
  double frame_snr = 0.0;                     /* F_ref/σ_F，>0 有限 */
  const SparseSnrLayer* sparse = nullptr;     /* 可空；非空且 present 即参与合成 */
  double x = 0.0, y = 0.0;                    /* 该帧像素坐标系下输出像素位置 */
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
};
const char* weight_closure_token(WeightClosure c);

struct WeightChainPolicy {
  /* 兼容旧调用方签名。**置真不会产生成功结果**：仅触发
     kUnclosedLegacyFallbackRejected + 显式错误串（消除静默假绿）。 */
  bool legacy_allow_weight_fallback = false;
};

struct WeightChainResult {
  bool ok = false;                /* 仅 kClosed 为真 */
  bool weight_chain_closed = false;
  bool production_allowed = false;
  WeightClosure closure = WeightClosure::kUnclosedMissingFrameSnr;
  std::string error;              /* kClosed 时为空 */
  std::string weight_source;      /* "frame_snr" | "frame_snr_x_sparse_snr" | "none" */
  double reference_flux = 0.0;
  std::vector<double> intra_snr;   /* 逐帧（无层 = 1.0） */
  std::vector<double> actual_snr;  /* 逐帧 = frame_snr × intra_snr */
  std::vector<double> weights;     /* 逐帧 = actual_snr²/F_ref² [ADU^-2] */
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

/* 显式、非生产的等权基线（ablation/baseline 比较用）。它**不是**权重链闭合：
   weight_chain_closed=false, production_allowed=false。 */
WeightChainResult make_equal_weight_baseline(std::size_t n_frames);

}  /* namespace p2weight */
}  /* namespace v6 */
}  /* namespace astrocs */
