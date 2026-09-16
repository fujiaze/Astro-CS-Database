// mini_sip1000.cpp - P0-1 ASAN 对比专用最小 driver
// A_ORDER=1000: atoi 无上界 → 循环 wcs.sip.a[i*6+j] 索引达 1000*6+1000=7006,
// 远超 a[36] (冲出 SipCoeffs/FitsImage 栈对象) → ASAN stack-buffer-overflow。
// 修复后: SIP order 校验 [0,5] → hp_drizzle_run 返回 -10, 无任何越界。
#include "hp_drizzle_api.h"
#include "aio_pipeline.h"
#include <cstdio>
#include <cstring>
#include <vector>

int main() {
    PipelineFrame* frame = aio_pipeline_frame_create();
    const int W = 4, H = 3;
    std::vector<float> px((size_t)W * H, 100.0f);
    int dims[2] = { H, W };
    aio_frame_add_block(frame, "data", AIO_BLOCK_FLOAT32, px.data(),
                        (int64_t)px.size(), dims, 2, "mini");
    aio_frame_kv_set_double(frame, "header", "CRVAL1", 202.5);
    aio_frame_kv_set_double(frame, "header", "CRVAL2", 47.2);
    aio_frame_kv_set_double(frame, "header", "CRPIX1", 2.0);
    aio_frame_kv_set_double(frame, "header", "CRPIX2", 1.5);
    aio_frame_kv_set_double(frame, "header", "CD1_1", -1.0e-4);
    aio_frame_kv_set_double(frame, "header", "CD2_2", 1.0e-4);
    aio_frame_kv_set(frame, "header", "A_ORDER", "1000");
    aio_frame_kv_set(frame, "header", "B_ORDER", "1000");
    aio_frame_kv_set(frame, "header", "A_999_999", "1.0e-6");
    HpDrizzleResult result;
    std::memset(&result, 0, sizeof(result));
    int rc = hp_drizzle_run(frame, 4, 1, 0.5, nullptr, &result, 0);
    printf("MINI-RESULT: hp_drizzle_run rc=%d err='%s'\n", rc, result.error_msg);
    aio_pipeline_frame_destroy(frame);
    return (rc == -10) ? 0 : 1;
}
