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
 * variance/ivar/flux-per-pixel/weight → UNSUPPORTED。 */
P3ResampleStatus p3_resample_check_mode(const char* input_mode);

/* 采样句柄: 打开 signal 子产品(严格 properties 校验=P3-001 复用) */
struct P3SamplerImpl;
struct P3Sampler {
    P3SamplerImpl* impl = nullptr;
};
P3ResampleStatus p3_sampler_open(const char* product_dir, P3Sampler* out,
                                       std::string* err);

/* nearest: 返回含样本方向的叶级像素值; coverage: 1=有值, 0=tile 缺失。
 * tile 内 NaN → *value=NaN, coverage=1(§4 非错误语义)。 */
P3ResampleStatus p3_sample_nearest(P3Sampler* s, double ra_deg, double dec_deg,
                                         float* value, int* coverage);

/* bilinear(跨 tile): 切平面四象限最近像素中心的双线性合成;
 * 任一角 tile 缺失 → coverage=0 且 *value=NaN; NaN 参与时 → S=NaN, C=1。 */
P3ResampleStatus p3_sample_bilinear(P3Sampler* s, double ra_deg, double dec_deg,
                                          float* value, int* coverage);

void p3_sampler_close(P3Sampler* s);

}  // namespace astrocs::phase3

#endif  // ASTROCS_P3_RESAMPLE_H
