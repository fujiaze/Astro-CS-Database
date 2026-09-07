// ============================================================================
// test_p0_sip_order_guard.cpp - P0 SIP order 越界读恶意 fixture 测试
// (bug 狩猎 R5, bughunt_p1_batchC)
//
// 背景: R3 P0 修复 f1cb487c 将 healpix_drizzle 的 SIP order 收敛到 [0,5]
// (越界硬失败/拒绝), 但同根因漏改 lib/photometric_calib/cpp/src/
// wcs_transform.cpp —— 其 WcsTransform 构造函数对 sip_order 无界, 36 项
// m_sip_*_buf 成员缓冲按 i*6+j 索引, evalSip 循环 coeffs[i*6+j] 在
// order>=6 时 (i=order, j=0 → 6*order > 35) 越界读对象外内存
// (外部 header A_ORDER=6/8 即触发, 可被恶意 FITS header 直接打中)。
//
// 修复口径 (对齐 f1cb487c): SIP order ∈ [0,5], 越界构造硬失败抛
// std::invalid_argument, 禁止静默截断; pc_api C 边界异常屏障转错误码 -4。
//
// 覆盖:
//   T1 order=6            → 构造抛 std::invalid_argument (修复前: 构造成功
//                            且 pixelToSky ASAN member-load 越界读)
//   T2 order=8            → 抛
//   T3 order=-1           → 抛
//   T4 order=0 (无SIP)    → 不抛, pixelToSky/skyToPixel 正常
//   T5 order=5 (合法上界) → 不抛, 往返一致 (天顶附近)
//   T6 order=5 越界系数   → 36 项缓冲内不越界 (合法上界边界值)
//
// 落位依据: 本模块 test/ 目录独立 g++ 驱动模式 (test_spectrum_integrator.cpp
// 同型); f1cb487c 共址 fixture 模式 (lib/.../tests/p0_sip_order_guard_test.cpp)。
//
// 编译 (lib/photometric_calib/cpp/ 目录):
//   g++ -O1 -g -std=c++17 -Iinclude -Isrc \
//     test/test_p0_sip_order_guard.cpp src/wcs_transform.cpp \
//     -o test/test_p0_sip_order_guard -lm
//   ./test/test_p0_sip_order_guard
// ============================================================================
#include "wcs_transform.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [PASS] %s\n", (msg)); ++g_pass; } \
    else      { std::printf("  [FAIL] %s\n", (msg)); ++g_fail; } \
} while (0)

namespace {

// 恶意/合法 SIP 系数: 36 项, 下三角合法值 (模拟外部 header 注入)
static void fill_sip_coeffs(std::vector<double>& a, std::vector<double>& b,
                            double scale) {
    a.assign(36, 0.0);
    b.assign(36, 0.0);
    // 合法域 [0,5] 内给少量非零项: A_3_0, A_2_1, B_0_3, B_1_2
    if (a.size() > 3 * 6 + 0) a[3 * 6 + 0] = scale * 1.0e-3;
    if (a.size() > 2 * 6 + 1) a[2 * 6 + 1] = scale * 2.0e-3;
    if (b.size() > 0 * 6 + 3) b[0 * 6 + 3] = scale * 1.5e-3;
    if (b.size() > 1 * 6 + 2) b[1 * 6 + 2] = scale * 2.5e-3;
}

} // namespace

int main() {
    std::printf("=== P0 SIP order 越界读恶意 fixture (wcs_transform, R5) ===\n");

    const std::vector<double> zero4(36, 0.0);

    // T1: A_ORDER=6 (边界外第一档) → 硬失败
    {
        bool threw = false;
        try {
            pc::WcsTransform wcs(10.0, 20.0, 100.0, 100.0,
                                 1.0e-4, 0.0, 0.0, 1.0e-4,
                                 6, zero4.data(), zero4.data(), nullptr, nullptr);
        } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw, "T1 sip_order=6 构造抛 std::invalid_argument (拒绝, 不截断)");
    }

    // T2: A_ORDER=8 (修复前 ASAN member-load 越界读档位) → 硬失败
    {
        bool threw = false;
        try {
            pc::WcsTransform wcs(10.0, 20.0, 100.0, 100.0,
                                 1.0e-4, 0.0, 0.0, 1.0e-4,
                                 8, zero4.data(), zero4.data(), nullptr, nullptr);
        } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw, "T2 sip_order=8 构造抛 std::invalid_argument");
    }

    // T3: 负 order → 硬失败
    {
        bool threw = false;
        try {
            pc::WcsTransform wcs(10.0, 20.0, 100.0, 100.0,
                                 1.0e-4, 0.0, 0.0, 1.0e-4,
                                 -1, zero4.data(), zero4.data(), nullptr, nullptr);
        } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw, "T3 sip_order=-1 构造抛 std::invalid_argument");
    }

    // T4: order=0 (无 SIP, 向后兼容) → 正常
    {
        bool threw = false;
        double ra = 0.0, dec = 0.0;
        try {
            pc::WcsTransform wcs(10.0, 20.0, 100.0, 100.0,
                                 1.0e-4, 0.0, 0.0, 1.0e-4,
                                 0, nullptr, nullptr, nullptr, nullptr);
            wcs.pixelToSky(100.0, 100.0, ra, dec);
        } catch (...) { threw = true; }
        CHECK(!threw, "T4 sip_order=0 构造+pixelToSky 正常 (无SIP)");
        CHECK(ra > 9.999 && ra < 10.001 && dec > 19.999 && dec < 20.001,
              "T4 天顶往返 RA/Dec 合理 (CRVAL 处)");
    }

    // T5+T6: order=5 合法上界 → 正常且数值稳定 (36 项内)
    {
        std::vector<double> a, b;
        fill_sip_coeffs(a, b, 1.0e-2);
        bool threw = false;
        double ra = 0.0, dec = 0.0, x = -1.0, y = -1.0;
        try {
            pc::WcsTransform wcs(10.0, 20.0, 100.0, 100.0,
                                 1.0e-4, 0.0, 0.0, 1.0e-4,
                                 5, a.data(), b.data(), nullptr, nullptr);
            wcs.pixelToSky(100.0, 100.0, ra, dec);
            wcs.skyToPixel(ra, dec, x, y);
        } catch (...) { threw = true; }
        CHECK(!threw, "T5 sip_order=5 (合法上界) 不拒绝");
        CHECK(x > 99.9 && x < 100.1 && y > 99.9 && y < 100.1,
              "T5 order=5 往返 pixelToSky→skyToPixel 收敛到原点");
    }

    std::printf("=== 结果: PASS=%d FAIL=%d ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
