// ============================================================================
// p0_sip_order_guard_test.cpp - P0-1 SIP order 越界写恶意 fixture 复现测试
// (bughunt_p0_io)
//
// 背景: run_drizzle_internal 从 frame header KV atoi 读 A_ORDER/B_ORDER/
// AP_ORDER/BP_ORDER 后直接循环 wcs.sip.a[i*6+j] (a[36])。恶意 frame
// A_ORDER=8 时索引达 8*6+8=56 > 35 → 栈对象越界写。
// 修复口径 (与 hp_drizzle_reverse_run :86-92 一致): SIP order 正式支持
// [0,5], 非法值硬失败返回 -10, 禁止静默截断。
//
// 覆盖:
//   T1 A_ORDER=8            → hp_drizzle_run 返回 -10 (修复前: ASAN
//                             stack-buffer-overflow)
//   T2 A_ORDER=-1           → -10
//   T3 A_ORDER=6            → -10 (边界外第一档)
//   T4 A_ORDER=5 (合法上界) → 不因 SIP 拒绝 (rc != -10)
//   T5 AP_ORDER=9/B_ORDER=7 → -10 (AP/BP 同口径)
//   T6 A_ORDER=2 合法       → 不因 SIP 拒绝
//   T7 无 A_ORDER           → 不因 SIP 拒绝 (向后兼容)
//
// 落位依据: 本模块 tests/ 目录既有独立 g++ 驱动模式 (drizzle_l0_test.cpp 等)。
//
// 编译 (healpix_drizzle/ 目录):
//   g++ -O1 -g -std=c++17 -fopenmp -DAIO_ENABLE_HEALPIX \
//     -I. -I../../astro_image_io/include -I../../astro_image_io/src -I../../../common \
//     tests/p0_sip_order_guard_test.cpp \
//     hp_drizzle_api.cpp drizzle_engine.cpp fits_reader.cpp wcs_sip.cpp \
//     poly_clip.cpp spherical_overlap.cpp reverse_drizzle.cpp astro_sphere_sink.cpp \
//     ../../common/healpix/healpix_core.cpp healpix_core.cpp snr_evaluator.cpp \
//     ../../astro_image_io/src/aio_pipeline.cpp \
//     ../../astro_image_io/src/aio_log.cpp \
//     ../../astro_image_io/src/aio_api.cpp \
//     ../../astro_image_io/src/aio_xisf.cpp \
//     ../../astro_image_io/src/aio_fits.cpp \
//     ../../astro_image_io/third_party/cfitsio/*.o \
//     -lzstd -llz4 -lz -lm -lpthread \
//     -o tests/p0_sip_order_guard_test
//   ./tests/p0_sip_order_guard_test
// ============================================================================
#include "hp_drizzle_api.h"
#include "aio_pipeline.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", (msg)); ++g_pass; } \
    else      { printf("  [FAIL] %s\n", (msg)); ++g_fail; } \
} while (0)

// 构造最小合法 frame: data(FLOAT32 4x3) + header WCS KV + 可选 SIP order KV
// 返回码约定: 0=构造成功
static int make_frame(const char* a_order, const char* b_order,
                      const char* ap_order, const char* bp_order,
                      PipelineFrame** out) {
    PipelineFrame* frame = aio_pipeline_frame_create();
    if (!frame) return 1;

    // data 块: 4x3 float32 常数 100.0
    const int W = 4, H = 3;
    std::vector<float> px((size_t)W * H, 100.0f);
    int dims[2] = { H, W };
    if (aio_frame_add_block(frame, "data", AIO_BLOCK_FLOAT32,
                            px.data(), (int64_t)px.size(), dims, 2,
                            "p0 test data") != 0) {
        aio_pipeline_frame_destroy(frame);
        return 2;
    }

    // header KV: 完整 WCS (has_cd) + 可选 SIP
    aio_frame_kv_set_double(frame, "header", "CRVAL1", 202.5);
    aio_frame_kv_set_double(frame, "header", "CRVAL2", 47.2);
    aio_frame_kv_set_double(frame, "header", "CRPIX1", 2.0);
    aio_frame_kv_set_double(frame, "header", "CRPIX2", 1.5);
    aio_frame_kv_set_double(frame, "header", "CD1_1", -1.0e-4);
    aio_frame_kv_set_double(frame, "header", "CD2_2", 1.0e-4);
    if (a_order)   aio_frame_kv_set(frame, "header", "A_ORDER", a_order);
    if (b_order)   aio_frame_kv_set(frame, "header", "B_ORDER", b_order);
    if (ap_order)  aio_frame_kv_set(frame, "header", "AP_ORDER", ap_order);
    if (bp_order)  aio_frame_kv_set(frame, "header", "BP_ORDER", bp_order);
    // 恶意/合法 SIP 系数对 (用于复现越界写: A_8_8 键存在时 i=8,j=8 → idx 56)
    if (a_order && std::strcmp(a_order, "8") == 0) {
        aio_frame_kv_set(frame, "header", "A_8_8", "1.0e-6");
        aio_frame_kv_set(frame, "header", "A_7_7", "1.0e-6");
        aio_frame_kv_set(frame, "header", "A_6_6", "1.0e-6");
    }
    if (a_order && std::strcmp(a_order, "7") == 0) aio_frame_kv_set(frame, "header", "A_7_7", "1.0e-6");
    *out = frame;
    return 0;
}

// 运行一次 hp_drizzle_run 并返回其返回码 (result 错误消息打印到 stdout)
static int run_case(const char* a, const char* b, const char* ap, const char* bp,
                    HpDrizzleResult* result) {
    PipelineFrame* frame = nullptr;
    if (make_frame(a, b, ap, bp, &frame) != 0) return -999;
    int rc = hp_drizzle_run(frame, /*nside=*/4, /*nested=*/1, /*pixfrac=*/0.5,
                            /*output_path=*/nullptr, result,
                            /*precision_mode=*/0);
    aio_pipeline_frame_destroy(frame);
    return rc;
}

int main() {
    printf("== P0-1 SIP order guard: malicious frame header tests ==\n");
    HpDrizzleResult result;

    // T1: 恶意 A_ORDER=8 (复现越界写现场) — 修复后必须硬失败 -10
    std::memset(&result, 0, sizeof(result));
    int rc = run_case("8", nullptr, nullptr, nullptr, &result);
    CHECK(rc == -10, "T1 A_ORDER=8 硬失败 (rc=-10, 修复前越界写栈对象)");
    if (rc == -10) {
        CHECK(result.error_msg[0] != '\0' &&
              std::strstr(result.error_msg, "SIP order") != nullptr,
              "T1b 错误消息注明 SIP order 非法");
    }

    // T2: 负 order
    std::memset(&result, 0, sizeof(result));
    rc = run_case("-1", nullptr, nullptr, nullptr, &result);
    CHECK(rc == -10, "T2 A_ORDER=-1 硬失败 (rc=-10)");

    // T3: 边界外第一档 6
    std::memset(&result, 0, sizeof(result));
    rc = run_case("6", nullptr, nullptr, nullptr, &result);
    CHECK(rc == -10, "T3 A_ORDER=6 硬失败 (rc=-10, 上界 5 之外)");

    // T4: 合法上界 5 不因 SIP 拒绝
    std::memset(&result, 0, sizeof(result));
    rc = run_case("5", nullptr, nullptr, nullptr, &result);
    CHECK(rc != -10, "T4 A_ORDER=5 (合法上界) 不被 SIP 校验拒绝");
    CHECK(rc != -999, "T4b frame 构造正常");

    // T5: AP/BP 同口径 (A_ORDER 存在时四键同查)
    std::memset(&result, 0, sizeof(result));
    rc = run_case("2", "7", nullptr, nullptr, &result);
    CHECK(rc == -10, "T5a A_ORDER=2 + B_ORDER=7 硬失败 (rc=-10)");
    std::memset(&result, 0, sizeof(result));
    rc = run_case("2", "2", "9", nullptr, &result);
    CHECK(rc == -10, "T5b AP_ORDER=9 硬失败 (rc=-10)");
    std::memset(&result, 0, sizeof(result));
    rc = run_case("2", "2", "2", "-3", &result);
    CHECK(rc == -10, "T5c BP_ORDER=-3 硬失败 (rc=-10)");
    // T5d: 仅 B_ORDER=7 (无 A_ORDER) — SIP 解析不激活 (A_ORDER 为 SIP 存在
    // 标志, 系数循环不可达), 无越界面; 保持既有语义: 不拒绝、不崩溃。
    std::memset(&result, 0, sizeof(result));
    rc = run_case(nullptr, "7", nullptr, nullptr, &result);
    CHECK(rc != -10 && rc != -999, "T5d 仅 B_ORDER=7 (无 A_ORDER) 不激活 SIP, 语义保持");

    // T6: 合法低阶不受影响
    std::memset(&result, 0, sizeof(result));
    rc = run_case("2", nullptr, nullptr, nullptr, &result);
    CHECK(rc != -10, "T6 A_ORDER=2 合法不被拒绝");
    CHECK(rc != -999, "T6b frame 构造正常");

    // T7: 无 A_ORDER (既有合法路径) 不受影响
    std::memset(&result, 0, sizeof(result));
    rc = run_case(nullptr, nullptr, nullptr, nullptr, &result);
    CHECK(rc != -10 && rc != -999, "T7 无 A_ORDER 向后兼容 (不走 SIP 校验)");

    printf("== RESULT: pass=%d fail=%d ==\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
