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
 * max_tiles≤0 → 恢复默认(8); 请求可降不可升超物理内存守卫由上层校验。 */
void p3_sampler_set_max_tiles(P3Sampler* s, int max_tiles);

/* nearest: 返回含样本方向的叶级像素值; coverage: 1=有值, 0=tile 缺失。
 * tile 内 NaN → *value=NaN, coverage=1(§4 非错误语义)。 */
P3ResampleStatus p3_sample_nearest(P3Sampler* s, double ra_deg, double dec_deg,
                                         float* value, int* coverage);

/* bilinear(跨 tile): 切平面四象限最近像素中心的双线性合成;
 * 任一角 tile 缺失 → coverage=0 且 *value=NaN; NaN 参与时 → S=NaN, C=1。 */
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

/* bilinear 权重暴露版: 输出 G4 冻结权重 weights[4] 与四角 leaf ipix[4]
 * (角序: [0][0],[1][0],[0][1],[1][1], 即 Σc_k 权重序); 退化角(重复填充)
 * 权重如实输出(重复 leaf 计入多次, Σc_k=1 不变量保持)。 */
P3ResampleStatus p3_sample_bilinear_ex(P3Sampler* s, double ra_deg, double dec_deg,
                                       float* value, int* coverage,
                                       double weights[4], uint64_t leaf_ipix[4]);

/* 传播 (科学权威 = docs/science/UNCERTAINTY_AND_COVARIANCE.md Phase3 节):
 *   npts=1 (nearest): var_out = u_in
 *   npts=4 (bilinear): var_out = Σ_k c_k²·u_k  (c_k=G4 冻结权重, Σc_k=1;
 *     Σc_k²≠1 是正确物理——bilinear 平均去相关, 禁止 Σc_k=1 归一 variance)
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
