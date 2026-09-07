// ============================================================================
// test_p0_pc_api_exception_barrier.cpp - P0 pc_api C 边界异常屏障测试
// (bug 狩猎 R5, bughunt_p1_batchC)
//
// 背景: lib/photometric_calib/cpp/src/pc_api.cpp 全部 6 个 PC_API 导出入口
// 此前无任何 try/catch —— 内部 StarMatcher::matchWithKdTree (nanoflann KdTree
// 构建) / std::vector 分配可抛 bad_alloc, WcsTransform 可抛
// std::invalid_argument (SIP order 越界拒绝), 直接穿越 extern "C" 边界
// (Python ctypes/未来 GUI 只面对 C ABI, 跨界展开 = 未定义行为)。
//
// 修复口径 (对齐 f1cb487c 家族方案): 逐入口包 try/catch, int 型返回 →
// 错误码 -4 (内部异常; 0/-1/-2/-3 此前已占用)。
//
// 覆盖 (独立 g++ 驱动, 直接编入 pc_api.cpp 及其依赖):
//   T1 sip_order=8 调 pc_calibrate_simple      → -4 (拦截 invalid_argument)
//   T2 sip_order=8 调 pc_calibrate_simple_f64  → -4
//   T3 sip_order=-1 调 pc_calibrate_simple     → -4
//   T4 sip_order=0 合法调用                    → 0 (正常路径不受屏障影响)
//   T5 空指针参数                              → -1 (屏障前原参数校验不变)
//
// 编译 (lib/photometric_calib/cpp/ 目录):
//   g++ -O1 -g -std=c++17 -fopenmp -Iinclude -Isrc \
//     test/test_p0_pc_api_exception_barrier.cpp src/pc_api.cpp \
//     src/wcs_transform.cpp src/star_matcher.cpp src/image_corrector.cpp \
//     ../../gaia_client/src/gaia_client.cpp \
//     -o test/test_p0_pc_api_exception_barrier -lm
//   ./test/test_p0_pc_api_exception_barrier
// ============================================================================
#include "photometric_calib.h"
#include "gaia_client.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [PASS] %s\n", (msg)); ++g_pass; } \
    else      { std::printf("  [FAIL] %s\n", (msg)); ++g_fail; } \
} while (0)

namespace {

constexpr int W = 8, H = 8;

// ---- 链接 stub (本测试只覆盖 simple/f64 的 SIP 越界 → -4 路径, 不触发
// with_gaia 系列的 gaia_client 跨 DLL 调用; 仅为链接提供符号, 永不执行) ----
extern "C" {
int gaia_client_cone_search_with_spectrum(
    GaiaClient*, double, double, double, double, double,
    GaiaSpectrumStar**, uint8_t**, int*) { return -1; }
int gaia_client_get_spectrum_params(
    GaiaClient*, int*, int*, int*) { return -1; }
}


struct Fixture {
    std::vector<float> pix_f32;
    std::vector<double> pix_f64;
    std::vector<double> gaia_ra, gaia_dec, gaia_mag, gaia_fsyn;
    std::vector<double> psf_cx, psf_cy, psf_flux;
    std::vector<int> psf_status;
    std::vector<double> sip_zero;
    Fixture() : pix_f32((size_t)W * H, 100.0f), pix_f64((size_t)W * H, 100.0),
                gaia_ra(2, 10.0), gaia_dec(2, 20.0), gaia_mag(2, 15.0),
                gaia_fsyn(2, 1000.0),
                psf_cx{50.0, 150.0}, psf_cy{100.0, 100.0},
                psf_flux(2, 1000.0), psf_status(2, 0),
                sip_zero(36, 0.0) {}
};

} // namespace

int main() {
    std::printf("=== P0 pc_api C 边界异常屏障测试 (R5) ===\n");
    Fixture fx;
    int n_matched = -999;
    double scale = -999.0, sigma = -999.0;

    // T1: SIP order=8 → pc_calibrate_simple 返回 -4 (异常拦在 C ABI 内)
    {
        int rc = pc_calibrate_simple(
            fx.pix_f32.data(), W, H,
            fx.gaia_ra.data(), fx.gaia_dec.data(),
            fx.gaia_mag.data(), fx.gaia_fsyn.data(), (int)fx.gaia_ra.size(),
            fx.psf_cx.data(), fx.psf_cy.data(),
            fx.psf_flux.data(), fx.psf_status.data(), (int)fx.psf_cx.size(),
            nullptr, nullptr, 0,
            10.0, 20.0, 100.0, 100.0, 1.0e-4, 0.0, 0.0, 1.0e-4,
            8, fx.sip_zero.data(), fx.sip_zero.data(), nullptr, nullptr,
            fx.pix_f32.data(), &n_matched, &scale, &sigma, nullptr);
        CHECK(rc == -4, "T1 pc_calibrate_simple sip_order=8 → -4 (异常转错误码)");
    }

    // T2: FP64 入口同口径
    {
        int rc = pc_calibrate_simple_f64(
            fx.pix_f64.data(), W, H,
            fx.gaia_ra.data(), fx.gaia_dec.data(),
            fx.gaia_mag.data(), fx.gaia_fsyn.data(), (int)fx.gaia_ra.size(),
            fx.psf_cx.data(), fx.psf_cy.data(),
            fx.psf_flux.data(), fx.psf_status.data(), (int)fx.psf_cx.size(),
            nullptr, nullptr, 0,
            10.0, 20.0, 100.0, 100.0, 1.0e-4, 0.0, 0.0, 1.0e-4,
            8, fx.sip_zero.data(), fx.sip_zero.data(), nullptr, nullptr,
            fx.pix_f64.data(), &n_matched, &scale, &sigma, nullptr);
        CHECK(rc == -4, "T2 pc_calibrate_simple_f64 sip_order=8 → -4");
    }

    // T3: 负 SIP order
    {
        int rc = pc_calibrate_simple(
            fx.pix_f32.data(), W, H,
            fx.gaia_ra.data(), fx.gaia_dec.data(),
            fx.gaia_mag.data(), fx.gaia_fsyn.data(), (int)fx.gaia_ra.size(),
            fx.psf_cx.data(), fx.psf_cy.data(),
            fx.psf_flux.data(), fx.psf_status.data(), (int)fx.psf_cx.size(),
            nullptr, nullptr, 0,
            10.0, 20.0, 100.0, 100.0, 1.0e-4, 0.0, 0.0, 1.0e-4,
            -1, fx.sip_zero.data(), fx.sip_zero.data(), nullptr, nullptr,
            fx.pix_f32.data(), &n_matched, &scale, &sigma, nullptr);
        CHECK(rc == -4, "T3 pc_calibrate_simple sip_order=-1 → -4");
    }

    // T4: 合法路径 (sip_order=0) 不受屏障影响 → 0
    {
        int rc = pc_calibrate_simple(
            fx.pix_f32.data(), W, H,
            fx.gaia_ra.data(), fx.gaia_dec.data(),
            fx.gaia_mag.data(), fx.gaia_fsyn.data(), (int)fx.gaia_ra.size(),
            fx.psf_cx.data(), fx.psf_cy.data(),
            fx.psf_flux.data(), fx.psf_status.data(), (int)fx.psf_cx.size(),
            nullptr, nullptr, 0,
            10.0, 20.0, 100.0, 100.0, 1.0e-4, 0.0, 0.0, 1.0e-4,
            0, nullptr, nullptr, nullptr, nullptr,
            fx.pix_f32.data(), &n_matched, &scale, &sigma, nullptr);
        CHECK(rc == 0, "T4 pc_calibrate_simple sip_order=0 合法路径 → 0 (不误伤)");
        CHECK(scale > 0.0, "T4 输出 scale 已填充 (>0)");
    }

    // T5: 屏障前原参数校验不变 (空输出指针 → -1)
    {
        int rc = pc_calibrate_simple(
            fx.pix_f32.data(), W, H,
            fx.gaia_ra.data(), fx.gaia_dec.data(),
            fx.gaia_mag.data(), fx.gaia_fsyn.data(), (int)fx.gaia_ra.size(),
            fx.psf_cx.data(), fx.psf_cy.data(),
            fx.psf_flux.data(), fx.psf_status.data(), (int)fx.psf_cx.size(),
            nullptr, nullptr, 0,
            10.0, 20.0, 100.0, 100.0, 1.0e-4, 0.0, 0.0, 1.0e-4,
            0, nullptr, nullptr, nullptr, nullptr,
            nullptr, &n_matched, &scale, &sigma, nullptr);
        CHECK(rc == -1, "T5 out_pixels=nullptr → -1 (原参数校验语义不变)");
    }

    std::printf("=== 结果: PASS=%d FAIL=%d ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
