// lib/phase3_session/p3_resample.h — Phase3 重采样: order 选择/tile 查找/nearest/bilinear (ALG-P3-003) — P3-003
// 覆盖: 跨 tile 采样、coverage/mask(二值)、NaN 语义(S=NaN+C=1, §4)、单位固定 surface brightness、
// 未支持输入模式(variance/ivar/flux-per-pixel/weight)显式拒(UNSUPPORTED)。
#ifndef ASTROCS_P3_RESAMPLE_H
#define ASTROCS_P3_RESAMPLE_H

#include <cstdint>
#include <string>

namespace astrocs::phase3 {

typedef enum {
    P3_RS_OK = 0,
    P3_RS_PARAM = 1,
    P3_RS_UNSUPPORTED = 2,
    P3_RS_IO = 3
} P3ResampleStatus;

/* order selector(确定性): 选最小 order L∈[0,max] 使 HiPS 叶级分辨率 ≤ 输出 scale;
 * 无更细层时取 max_order。 */
P3ResampleStatus p3_order_select(int max_order, double scale_deg_per_px, int* out_order);

/* 未支持输入模式显式拒(§4): input_mode ∈ {surface_brightness}*;
 * DATA-P3-UNC-001 §30.4-4 supersession(上位=宪章 §7.1/§7.3, 2026-09-09 冻结):
 * "variance"/"ivar" 从 UNSUPPORTED 拒绝项移除(转 uncertainty 子产品消费面,
 * 见 p3_uncertainty_open) → 归未知输入模式 PARAM; "weight"/"flux-per-pixel"
 * 拒绝项不变(UNSUPPORTED)。 */
P3ResampleStatus p3_resample_check_mode(const char* input_mode);

/* 采样句柄: 打开 signal 子产品(严格 properties 校验=P3-001 复用) */
struct P3SamplerImpl;
struct P3Sampler {
    P3SamplerImpl* impl = nullptr;
};
P3ResampleStatus p3_sampler_open(const char* product_dir, P3Sampler* out,
                                       std::string* err);
/* 打开并暴露输入实际 order 与 BUNIT: order_sel 上限须来自输入实际 order,
 * 禁止仅写 metadata; BUNIT 来源输入合同(缺省 ADU, 绝不 Jy/beam 默认)。 */
P3ResampleStatus p3_sampler_open_ex(const char* product_dir, P3Sampler* out,
                                    int* out_order, std::string* out_bunit,
                                    std::string* err);

/* P3-006/DOC-003: 配置 tile cache 容量上限(内存守卫 ARCH-P3 §3)。
 * max_tiles≤0 → 恢复默认(8); 请求可降不可升超物理内存守卫由上层校验。
 * P30: 容量作用在**共享** tile 缓存上 (单位 = 512²f32 tile = 1 MiB); 多 worker
 * 共享同一实例时总容量仍 = max_tiles, 与 worker 数无关 (峰值内存有界)。 */
void p3_sampler_set_max_tiles(P3Sampler* s, int max_tiles);

/* P30 (Phase3 导出性能 P0): 让 dst 与 src 共享同一个有界 tile 缓存。
 * 生产路径 (module_adapters p3_op_resample / p3_session) 的每个行带 worker 各自
 * open 一个 sampler, 修复前 worker 缓存容量恒为默认 8 (max_tiles 只设到了主
 * sampler) → 产物工作集 (真实 M42: 523 tile) 远大于 8 时逐行抖动重复解码。
 * 语义: 只共享只读 tile 数据与"缺失"负缓存, 不改变任何像素值; src 必须已 open;
 * attach 后 dst 仍只由单线程使用。禁止在 signal 与 variance/ivar 之间共享
 * (同一 tile ipix 键指向不同子产品数据)。 */
void p3_sampler_attach_cache(P3Sampler* dst, const P3Sampler* src);

/* P30: tile 缓存观测 (§10.5 资源证据: 命中率 + 真实 tile 读/失败 open 次数)。

 * 语义: open_failures = 真实打进"不存在的 tile 文件"的 open 次数 (无论是否开了
 * 负缓存), 是"未覆盖区域代价"的**可观测计数器** —— 回归用例
 * eng/tests/unit/p3_sampler_cache_test.cpp 据此断言"重复采样不线性增长底层 open 次数"。 */
struct P3CacheStats {
    unsigned long long cap_tiles = 0;         // 容量 (tile 数)
    unsigned long long resident_tiles = 0;    // 当前常驻 (含已知缺失项)
    unsigned long long hits = 0;
    unsigned long long misses = 0;
    unsigned long long absent_reads = 0;      // 写入负缓存的次数 (= 去重后的失败 open)
    unsigned long long open_failures = 0;     // 真实失败 open 总次数 (含重试)
    unsigned long long absent_entries = 0;    // 负缓存条目数
    unsigned long long evictions = 0;
};
void p3_sampler_cache_stats(const P3Sampler* s, P3CacheStats* out);

/* P30 回归对照专用: 开关"缺失 tile 负缓存"(默认 1=开)。
 * enabled=0 精确退化为修复前语义 (每个缺失像素重试一次失败 open) —— 仅用于
 * eng/tests/unit/p3_sampler_cache_test.cpp 的**阴性对照**, 生产路径不得关闭。
 * 同一输入下开关只影响 open 次数与耗时, 不改变任何像素值。 */
void p3_sampler_set_absent_cache(P3Sampler* s, int enabled);

/* nearest: 返回含样本方向的叶级像素值; coverage: 1=有值, 0=tile 缺失。
 * tile 内 NaN → *value=NaN, coverage=1(§4 非错误语义)。
 * 单样本口径 (§4 第 91 行冻结): 零合格样本(该 tile 像素 ¬isfinite) ⇒ S=NaN 且 C=1
 * (覆盖级 NaN); tile 缺失 ⇒ S=NaN 且 C=0。**强制计数在单样本下由 (value, coverage)
 * 唯一确定**(C=0 ⇒ 无候选样本 ⇒ n_rejected_nonfinite=0; C=1 且值非有限 ⇒ 1;
 * C=1 且值有限 ⇒ 0) ⇒ 本路径无需额外出参即可满足「计数 0 与字段缺失可区分」。 */
P3ResampleStatus p3_sample_nearest(P3Sampler* s, double ra_deg, double dec_deg,
                                         float* value, int* coverage);

/* bilinear(跨 tile): 切平面四象限最近像素中心的双线性合成;
 * 任一角 tile 缺失 → coverage=0 且 *value=NaN;
 * 邻域样本 ¬isfinite（NaN 与 ±Inf 同类）→ **样本级掩膜 + 剩余有效邻域重归一**
 * （rule_id NAN-SAMPLE-MASK-COVERAGE-NAN, ALG-P3-003 §2 G4 / §4）;
 * 仅零合格样本（或有效权重和 D_p=0）→ S=NaN（覆盖级 NaN）; C 只判足迹内有无
 * tile 像素（值 NaN 不改 C, 4 个 tile 均可读则 C=1）。 */
P3ResampleStatus p3_sample_bilinear(P3Sampler* s, double ra_deg, double dec_deg,
                                          float* value, int* coverage);

void p3_sampler_close(P3Sampler* s);

/* ── 不确定度传播面 (DATA-P3-UNC-001 §30.4, DATA-UNC-001 2026-09-09 冻结;
 * 上位=宪章 §7.1/§7.3; supersession SCI-P3 §9a-10 variance/ivar 拒绝语义) ──
 * 输入 HiPS 含 variance/ 子产品 → u=variance; 否则含 ivar/ → u=1/ivar
 * (ivar==0 像素 = u 无效→NaN 传播态); 两者并存 → variance 优先;
 * 两者皆无 → P3_UNC_NONE (输出面显式 unavailable, 禁静默丢弃禁占位 HDU)。 */
typedef enum {
    P3_UNC_NONE = 0,
    P3_UNC_VARIANCE = 1,
    P3_UNC_IVAR = 2
} P3UncertaintySource;

/* 逐像素传播状态 (§30.4 invalid policy 输出面唯一权威的三态分解):
 * OK=正常传播(Σc_k²·u_k); NAN=NaN 传播态(足迹内 u 为 NaN/ivar==0 无效);
 * MISSING=覆盖不一致(leaf u tile 缺失 → 输出 NaN + provenance 计数)。 */
typedef enum {
    P3_U_OK = 0,
    P3_U_NAN = 1,
    P3_U_MISSING = 2
} P3UncPixelState;

/* 打开 uncertainty 子产品: 探测 <root>/variance → <root>/ivar (variance 优先);
 * 皆无 → *out_src=P3_UNC_NONE 且 out 置空句柄(合法 unavailable, 非 IO 错误);
 * properties 存在但非法(键集/order 与 signal_order 不匹配) → P3_RS_PARAM
 * (产品损坏显式拒, 不静默降级 NONE); reader 打开失败 → P3_RS_IO。
 * out 可为 NULL(仅探测)。u 值域守卫(负/Inf→损坏)在逐像素 propagate 时执行。 */
P3ResampleStatus p3_uncertainty_open(const char* product_dir, int signal_order,
                                     P3UncertaintySource* out_src, P3Sampler* out);

void p3_uncertainty_close(P3Sampler* s);

/* nearest 权重暴露版: 输出样本 leaf ipix (nearest 传播 var_out=u_in 所需)。 */
P3ResampleStatus p3_sample_nearest_ex(P3Sampler* s, double ra_deg, double dec_deg,
                                      float* value, int* coverage,
                                      uint64_t* leaf_ipix);

/* bilinear 权重暴露版: 输出**实际生效权重** weights[4] 与四角 leaf ipix[4]
 * (角序: [0][0],[1][0],[0][1],[1][1], 即 Σc_k 权重序); 退化角(重复填充)
 * 权重如实输出(重复 leaf 计入多次, Σc_k=1 不变量保持)。
 * weights 语义 = 样本级掩膜后的重归一权重
 *   c'_k = w_k / Σ_{合格 j} w_j   （合格样本; 被剔除样本恰为 0.0）;
 * 全部合格时与几何权重逐值同量级（重归一为恒等, 差异 ≤ k·ULP, ALG-P3-003 §9）。
 * 依据: DATA-002 §2a 规则 1 要求不合格样本从**分子、分母、方差三项**一并剔除并
 * 重新归一 ⇒ 方差传播 Σc'_k²u_k 必须消费本权重（不得沿用未重归一的几何权重）。
 * 零合格样本（或 D_p=0）时四权重全 0（此时 S=NaN, C=1; Σc'_k=1 不适用）。 */
P3ResampleStatus p3_sample_bilinear_ex(P3Sampler* s, double ra_deg, double dec_deg,
                                       float* value, int* coverage,
                                       double weights[4], uint64_t leaf_ipix[4]);

/* ── 样本级掩膜强制计数 (rule_id NAN-SAMPLE-MASK-COVERAGE-NAN) ───────────────
 * 权威 (逐字同口径三处; 分歧以 DATA-002 §2a 为准):
 *   · docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md §2a
 *     `invalid_handling` 块 —— **唯一正本**: rule_id / aggregation=
 *     sample_level_mask_with_renormalisation / zero_eligible_samples=
 *     nan_signal_support_le_0 / zero_substitution=forbidden /
 *     rejection_counting=mandatory / **count_field=n_rejected_nonfinite**;
 *   · docs/standards/NUMERIC_STANDARD.md §MUST「NaN/Inf 契约」;
 *   · ALG-P3-003 §2 G4 + §4 (docs/algorithms/PHASE3_RESAMPLE.md) —— 本核冻结口径
 *     (样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数; 禁「零填」替代语义)。
 * 分类为**互斥、可加**三项 (DATA-002 §2a 规则 3): 值非有限 / 方差非有限 / 权重非正。
 * 信号核只消费 signal 平面 ⇒ variance 项恒 0; 权重由邻域几何唯一确定 ⇒ 权重非正项恒 0
 * (几何权重 ∈[0,1], 非有限权重不可达)。计数为 0 与「字段缺失」必须可区分 ⇒ 本结构
 * 由调用方零初始化后逐像素填入, 生产路径不得省略该出参。 */
struct P3SampleRejection {
    int n_rejected_nonfinite = 0;            // 合计 = 下列三项之和
    int n_rejected_nonfinite_value = 0;      // 值非有限 (NaN/±Inf)
    int n_rejected_nonfinite_variance = 0;   // 方差面非有限 (信号核不消费方差面 ⇒ 恒 0)
    int n_rejected_nonpositive_weight = 0;   // 权重非有限或 ≤0 (几何权重 ⇒ 恒 0)
    int n_eligible = 0;                      // 合格(isfinite)邻域样本数; 0 ⇒ 覆盖级 NaN
};

/* bilinear 样本级掩膜版: 与 p3_sample_bilinear_ex **同一数学路径**, 额外输出
 * 强制计数 rejection (可为 NULL = 与 _ex 等价)。
 * 语义与 _ex 逐字一致 (掩膜 + 重归一 + 覆盖级 NaN + weights=生效权重),
 * 差别仅在暴露被剔除样本计数。C 语义不变 (值 NaN 不改 C)。 */
P3ResampleStatus p3_sample_bilinear_nanmask_ex(P3Sampler* s, double ra_deg, double dec_deg,
                                               float* value, int* coverage,
                                               double weights[4], uint64_t leaf_ipix[4],
                                               P3SampleRejection* rejection);

/* 传播 (科学权威 = docs/science/UNCERTAINTY_AND_COVARIANCE.md Phase3 节):
 *   npts=1 (nearest): var_out = u_in
 *   npts=4 (bilinear): var_out = Σ_k c_k²·u_k  (Σc_k=1;
 *     Σc_k²≠1 是正确物理——bilinear 平均去相关, 禁止 Σc_k=1 归一 variance)
 *   c_k = **p3_sample_bilinear_ex 暴露的生效权重**（样本级掩膜后重归一; 被剔除
 *   样本恰为 0）—— 依据 DATA-002 §2a 规则 1「不合格样本从分子、分母、方差三项
 *   一并剔除并重新归一」: 剔除后权重变了, 方差项必须用重归一后的权重算, 不得
 *   沿用原几何权重。生效权重为 0 的样本从方差项一并剔除 (c²u 恰为 0), 不参与
 *   NaN/missing 合成; 四样本全为 0 (= 零合格样本 / D_p=0) ⇒ 覆盖级 NaN
 *   (禁静默 0 冒充无效)。u 仍逐样本读入 ⇒ 负/Inf 产品损坏仍 fail-closed 拒绝。
 * u 值读取: variance→u 原值; ivar→u=1/ivar (ivar==0→NaN 传播态)。
 * 返回: P3_RS_OK + *u_out + *st (P3_U_OK/NAN/MISSING);
 * u<0 或 Inf (产品损坏, 含负 ivar/负 variance/Inf variance) → P3_RS_PARAM
 * (run 面显式拒绝, 禁 clamp/补 0/静默跳过, §30.4-3)。
 * weights 在 npts=1 时可为 NULL。FP64 累加。 */
P3ResampleStatus p3_uncertainty_propagate(P3Sampler* u, const double* weights,
                                          const uint64_t* leaf_ipix, int npts,
                                          double* u_out, P3UncPixelState* st);

}  // namespace astrocs::phase3

#endif  // ASTROCS_P3_RESAMPLE_H
