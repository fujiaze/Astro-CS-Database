// ============================================================================
// test_extract_wcs_sip_failclosed.cpp — B4-1 (M1a-C-002) 共址回归锁
//
// 被验条目: M1a-C-002 "trans 线性奇异仍 success=true"。
// 冻结语义 (不改 SCI): SCI-WCS-001 §8 退化与错误语义表 —
//   "trans 线性奇异（共线/退化配置）→ 拒 SIP 推导，返回参数错误";
//   ALG-WCS-001 §11.1 — "CD det 退化必须视为求解失败 (success=0)，禁止以
//   CRPIX 坍缩值冒充解"; 宪章 §6.3 (禁静默降级) / §14.4 (fail-fast)。
//
// 回归锁语义 (未修复必红 / 已修复必绿):
//   RED  (修复前 ipv_wcs.cpp det<1e-15 分支仅 logger->warn 且函数恒
//         result->success = true): 断言 1/2 失败 → 本测试返回非 0 → ctest 红。
//   GREEN(修复后该分支置 result->success=false + 非空 error + 提前 return):
//         断言全过 → 返回 0 → ctest 绿。
// 判别力 (非恒真): 断言 3 用同一函数、同一装配路径的**非奇异** trans 证明
// 正常路径仍 success=true，故本锁不是"永远失败"或"永远通过"的常量断言。
//
// 注入对象直接是 extract_wcs_sip (不是 iter_trans_solve 之类的邻接函数)：
// 旧负面用例 p1wcs_tests_negative.cpp::child_collinear 断言的是
// iter_trans_solve 返回 0，不覆盖本分支 (findings 已判"判别力不足")。
//
// 编译 (独立可执行, 无第三方依赖):
//   g++ -std=c++17 -O2 -Iinclude src/ipv_wcs.cpp src/ipv_sip.cpp \
//       test/test_extract_wcs_sip_failclosed.cpp -o /tmp/t && /tmp/t
// ctest 目标名: ipv_extract_wcs_sip_failclosed (注册钩子见 REPORT "越域残余")
//
// 日期: 2026-09-14 (RQS B4-phase1)
// ============================================================================

#include "ipv_solver.h"
#include "ipv_itertrans.h"
#include "ipv_types.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) {                                                       \
            std::printf("  [PASS] %s\n", msg);                            \
        } else {                                                          \
            std::printf("  [FAIL] %s\n", msg);                            \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

// 共线 (退化) U/W 匹配场: 全部星点落在一条直线上 ⇒ 匹配点集 rank-deficient,
// 线性拟合奇异 (与 p1wcs 的 fix_wcs_e_collinear 同一物理构型)。
// 直接喂给 extract_wcs_sip (而不是 iter_trans_solve)。
// 坐标口径 (与 ipv_wcs.cpp 的 apply_trans(U)≈W 一致):
//   U = 图像侧像素偏移 (原点图像中心, Y-up); W = 星表侧角秒 (gnomonic ξ/η)。
void build_collinear(int n, std::vector<ipv::StarPoint>* U,
                     std::vector<ipv::StarPoint>* W,
                     std::vector<ipv::MatchPair>* pairs) {
    U->clear(); W->clear(); pairs->clear();
    for (int i = 0; i < n; ++i) {
        const double u = -50.0 + 100.0 * i / (n - 1);
        // 直线: v = 0.7·u + 3.0 (图像侧); W 侧为同一直线的一致性仿射像
        // (2 倍缩放 + 平移), 故非奇异 trans 下残差恰为 0 — 用于对照断言 3。
        const double v = 0.7 * u + 3.0;
        U->push_back({u, v});
        W->push_back({2.0 * u + 5.0, 2.0 * v - 1.0});
        pairs->push_back({i, i});
    }
}

}  // namespace

int main() {
    using namespace ipv;
    std::printf("[B4-1] extract_wcs_sip fail-closed 回归锁 (M1a-C-002)\n");

    const int W_IMG = 1024, H_IMG = 1024;
    const double RA0 = 150.0, DEC0 = 2.0, S0 = 0.4;

    std::vector<StarPoint> U, W;
    std::vector<MatchPair> pairs;
    build_collinear(24, &U, &W, &pairs);

    // ── 1. 共线 trans (线性项全零, det_lin == 0 < 1e-15) ───────────────────
    {
        Trans trans;                 // 默认 order=1, valid=false, 系数全 0
        trans.order = 3;             // 走 order>=2 分支 (SIP 推导被触发)
        trans.valid = false;         // 共线拟合的真实产物语义
        WcsFitResult res;
        std::memset(&res, 0, sizeof(res));
        extract_wcs_sip(trans, RA0, DEC0, W_IMG, H_IMG, S0, U, W, pairs,
                        &res, nullptr);
        CHECK(res.success == false,
              "collinear-zero-trans: success==false (旧实现恒 true)");
        CHECK(res.error[0] != '\0',
              "collinear-zero-trans: error 非空 (失败原因可诊断)");
    }

    // ── 2. 病态小 det (1e-30 < 1e-15) 但 trans.valid==true ───────────────
    // 直接命中 "det < 1e-15" 判据本身 (与 trans.valid 解耦)。
    {
        Trans trans;
        trans.order = 3;
        trans.valid = true;
        trans.x10 = 1e-15;  trans.x01 = 0.0;
        trans.y10 = 0.0;    trans.y01 = 1e-15;   // det_lin = 1e-30
        WcsFitResult res;
        std::memset(&res, 0, sizeof(res));
        extract_wcs_sip(trans, RA0, DEC0, W_IMG, H_IMG, S0, U, W, pairs,
                        &res, nullptr);
        CHECK(res.success == false,
              "singular-small-det: success==false (det<1e-15 判据)");
        CHECK(res.error[0] != '\0', "singular-small-det: error 非空");
    }

    // ── 3. 判别力对照: 非奇异 trans 走正常路径仍必须 success==true ───────
    // 与 fixture 严格一致的线性 trans (x10=2.0 arcsec/px, y01=2.0, 平移
    // (5,-1)); 反演后残差恰 0, 24 内点 ≥ 12, 尺度比 s0=2.0 ⇒ ratio=1。
    // 若本断言失败说明门把正常解也拒了 (过度收紧)。
    {
        Trans trans;
        trans.order = 1;
        trans.valid = true;
        trans.x10 = 2.0; trans.x01 = 0.0;
        trans.y10 = 0.0; trans.y01 = 2.0;
        trans.x00 = 5.0; trans.y00 = -1.0;
        WcsFitResult res;
        std::memset(&res, 0, sizeof(res));
        extract_wcs_sip(trans, RA0, DEC0, W_IMG, H_IMG, /*s0=*/2.0, U, W, pairs,
                        &res, nullptr);
        CHECK(res.success == true,
              "nonsingular-control: success==true (门非恒失败)");
    }

    if (g_failures == 0) {
        std::printf("B4-1 EXTRACT_WCS_SIP FAIL-CLOSED PASS\n");
        return 0;
    }
    std::printf("B4-1 EXTRACT_WCS_SIP FAIL-CLOSED FAIL (%d check(s))\n", g_failures);
    return 1;
}
