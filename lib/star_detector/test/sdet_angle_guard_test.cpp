// ============================================================================
// sdet_angle_guard_test.cpp - SDET-ANGLE-001 有界/fail-closed 回归锁
//
// 被测面: lib/star_detector/src/sdet_angle_guard.h
//   normalize_angle_deg_bounded(angle_deg, &out)
//
// 锁定内容 (P11 生产挂死级缺陷):
//   1. 非有限输入 (±inf / NaN) 与超界有限输入 (1e300 / DBL_MAX / 1e17)
//      必须**在有界时间内返回 false**; 修复前同一逻辑 (内联 while 迭代)
//      对 ±inf 永不返回 —— 见本批 REPORT §同类缺陷排查 / 修复前红证据。
//   2. 有限且可归一化输入必须返回 true, 且结果与冻结迭代式**逐位一致**
//      (oracle 在本测试内以相同运算序列复算)。
//   3. fail-closed: 返回 false 时不得写出 *out。
//   4. 边界: ±90 / ±270 / ±450 / 90+180*64 / 90+180*64+1 等。
//
// 日期: 2026-09-14
// ============================================================================
#include "sdet_angle_guard.h"

#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace sd = astrocs::star_detector;

static int g_fail = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++g_fail;
    }
}

// 逐位比较 (含 -0.0 == 0.0 之外的所有位)
static bool bitwise_equal(double a, double b) {
    std::uint64_t ba = 0, bb = 0;
    std::memcpy(&ba, &a, sizeof(ba));
    std::memcpy(&bb, &b, sizeof(bb));
    return ba == bb;
}

// 冻结实现 oracle: 仅用于**已知会在少量步内终止**的有限输入。
// 运算序列与旧 sdet_api.cpp 内联循环完全一致。
static double legacy_normalize(double angle_deg) {
    while (std::fabs(angle_deg) > 90.0) {
        if (angle_deg > 0.0) angle_deg -= 180.0;
        else angle_deg += 180.0;
    }
    return angle_deg;
}

static void expect_reject(double x, const char* what) {
    double out = 42.0;  // 哨兵: 失败路径不得写出
    bool ok = sd::normalize_angle_deg_bounded(x, &out);
    check(!ok, what);
    check(bitwise_equal(out, 42.0), what);
}

static void expect_accept_bitwise(double x, const char* what) {
    double out = 42.0;
    bool ok = sd::normalize_angle_deg_bounded(x, &out);
    check(ok, what);
    double ref = legacy_normalize(x);
    check(bitwise_equal(out, ref), what);
    check(std::fabs(out) <= 90.0, what);
}

int main() {
    auto t0 = std::chrono::steady_clock::now();

    // --- 1. 挂死输入: 必须失败且立即返回 ---
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    expect_reject(inf, "reject +inf");
    expect_reject(-inf, "reject -inf");
    expect_reject(nan, "reject NaN");
    expect_reject(1e300, "reject 1e300");
    expect_reject(-1e300, "reject -1e300");
    expect_reject(DBL_MAX, "reject DBL_MAX");
    expect_reject(-DBL_MAX, "reject -DBL_MAX");
    expect_reject(1e17, "reject 1e17");
    expect_reject(-1e17, "reject -1e17");
    // 闭式预判上界: (|x|-90)/180 > 64 -> 拒绝
    expect_reject(90.0 + 180.0 * 64.0 + 1.0, "reject just over half-turn cap");
    expect_reject(sd::kMaxAngleHalfTurns * 180.0 + 1000.0, "reject far over cap");

    // --- 2. 有限可归一化输入: 接受且与冻结实现逐位一致 ---
    const double accept_cases[] = {
        0.0, 1.0, -1.0, 45.0, -45.0,
        89.9999999, 90.0, -90.0, 90.0000001, -90.0000001,
        179.9, 180.0, -180.0, 270.0, -270.0, 450.0, -450.0, 629.5,
        90.0 + 180.0 * 1.0, 90.0 + 180.0 * 2.0, 90.0 + 180.0 * 63.0,
        90.0 + 180.0 * 64.0,                 // 恰好 64 次: 接受
        -(90.0 + 180.0 * 64.0),
        1000.0, -1000.0, 11500.0, -11500.0,
    };
    for (double x : accept_cases) {
        expect_accept_bitwise(x, "accept+bitwise finite");
    }

    // 恰好 64 次归一化的闭式边界 (90+180*64 = 11610): |out| <= 90
    {
        double out = 0.0;
        bool ok = sd::normalize_angle_deg_bounded(90.0 + 180.0 * 64.0, &out);
        check(ok, "cap boundary accepted");
        check(std::fabs(out) <= 90.0, "cap boundary normalized into range");
    }

    // --- 3. 出参为 null: fail-closed ---
    check(!sd::normalize_angle_deg_bounded(0.0, nullptr), "reject null out");

    // --- 4. 有界时间: 全部调用应远小于 100ms ---
    double sink = 0.0;
    for (int i = 0; i < 100000; ++i) {
        double out = 0.0;
        if (sd::normalize_angle_deg_bounded((i % 1000) - 500, &out)) sink += out;
    }
    (void)sink;
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    check(ms < 100.0, "bounded elapsed < 100ms");

    if (g_fail != 0) {
        std::printf("SDET_ANGLE_GUARD: FAIL (%d checks)\n", g_fail);
        return 1;
    }
    std::printf("SDET_ANGLE_GUARD: PASS (elapsed %.3f ms)\n", ms);
    return 0;
}
