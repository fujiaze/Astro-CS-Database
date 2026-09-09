// P1-WCS-TEST · negative 组 (F4 失败语义负例 + 缺陷行为锚)
//
// 合同锚: docs/algorithms/PLATESOLVE.md §11.4 F4 + §11.3 DISP-WCS-001..006
// (登记不改码)。失败-置信度语义冻结: CD det 退化必须以 success=0 呈现,
// 禁止坍缩值冒充解 (DISP-WCS-001 核心语义, 本组在 iter_trans 内核路径
// 直接验收)。
//
// 覆盖 (§11.4 F4 逐项, 平台可达性注记见各组内注释):
//   0/1/2 星 → success=0 进程不崩 (C ABI ret=0/error_msg 通道由 P1-WCS-IMPL
//              落地后验收, 见文件尾登记); n=0 空对 → 确定性路径直接断言
//              (WCS-002 翻锚: 原 fork 隔离现状锚, 空对 guard 零 UB 实证后
//              翻为三组合确定性断言);
//   指向偏差>FOV (等价注入: 星表-图像无真实对应) → 显式失败;
//   CD det 退化 (共线场) → success=0 禁止坍缩;
//   NaN/Inf 输入 → 不崩 (fork 隔离);
//   WcsTan det 退化 → 坍缩返回 CRPIX (DISP-WCS-001 同族现状行为锚)。
#include "p1wcs_test_main.hpp"
#include "p1wcs_fixtures.hpp"
#include "p1wcs_oracle.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <limits>
#include <vector>

#include "ipv_itertrans.h"
#include "ipv_solver.h"
#include "ipv_triangle.h"
#include "wcs_tan.h"

using namespace p1wcs;

namespace {

// fork 隔离执行: 返回 {exited, rc, signaled, sig}。被测函数在疑缺陷路径
// (空 initial_pairs / NaN 输入) 可能段错误, 隔离保证测试进程存活并锁定
// 现状行为 (缺陷锚, 非恒 PASS 占位 — 行为漂移即 fail)。
struct ForkOutcome {
    bool exited = false;
    int rc = -1;
    bool signaled = false;
    int sig = 0;
};

ForkOutcome run_forked(int (*fn)(void)) {
    ForkOutcome out;
    const pid_t pid = fork();
    if (pid < 0) {
        std::perror("p1wcs fork");
        return out;
    }
    if (pid == 0) {
        const int rc = fn();
        _exit(rc);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        out.exited = true;
        out.rc = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        out.signaled = true;
        out.sig = WTERMSIG(status);
    }
    return out;
}

// --- fork 子进程体 (各负例; 返回 0=行为符合断言) ---

int child_few_stars_1() {
    std::vector<ipv::StarPoint> U, W;
    std::vector<ipv::MatchPair> p;
    fix_wcs_c_few_stars(&U, &W, &p, 1);
    const ipv::IterTransResult r = ipv::iter_trans_solve(U, W, p, 5.0, 1);
    return (r.success == false) ? 0 : 1;
}

int child_few_stars_2() {
    std::vector<ipv::StarPoint> U, W;
    std::vector<ipv::MatchPair> p;
    fix_wcs_c_few_stars(&U, &W, &p, 2);
    const ipv::IterTransResult r = ipv::iter_trans_solve(U, W, p, 5.0, 1);
    return (r.success == false) ? 0 : 1;
}

// WCS-002 翻锚: 空对确定性路径 (原 fork 隔离现状锚 → 直接确定性断言)。
// 原锚 (P1-WCS-TEST 2026-09-09): 空 initial_pairs (U/W 全空) 首测 SIGSEGV、
// 复测优雅 success=0, 状态依赖不可稳定复现 (空 vector data()==nullptr UB
// 表现) → fork 隔离锁主流行为。WCS-002 验证生产空对路径已为确定性实现:
// iter_trans_solve 空 initial_pairs guard (ipv_itertrans.cpp) 确定返回
// success=0 + 确定日志, U/W 空而 pairs 非空时 calc_trans_general 索引
// 先检查后解引用 (mp.u/w 越界 continue → 正规方程全零 → gauss_solve
// 奇异返回 false), 零 UB (本文件 asan+ubsan 实证)。翻锚为直接调用
// (不 fork 隔离): 断言确定 rc (success=0) + 确定零输出态 (trans 无效/
// 零内点/零残差), 任何 UB 崩溃即测试进程死亡 = 红。三空对组合全覆盖:
//   a) U/W/p 全空 (原首测构造);
//   b) U/W 空, initial_pairs 非空 (索引越界对 → 奇异失败路径);
//   c) U/W 非空, initial_pairs 空 (guard 路径)。
int child_zero_pairs_all_empty() {
    std::vector<ipv::StarPoint> U, W;
    std::vector<ipv::MatchPair> p;
    fix_wcs_c_few_stars(&U, &W, &p, 0);  // 全空 (对齐原首测构造)
    const ipv::IterTransResult r = ipv::iter_trans_solve(U, W, p, 5.0, 1);
    return (r.success || r.trans.valid || r.n_inliers != 0 ||
            r.rms != 0.0 || !r.residuals.empty() || !r.inliers.empty())
               ? 1 : 0;
}

int child_zero_pairs_idx_out_of_range() {
    std::vector<ipv::StarPoint> U, W;
    std::vector<ipv::MatchPair> p;
    fix_wcs_c_few_stars(&U, &W, &p, 0);
    for (int k = 0; k < 8; ++k) p.push_back({0, 0});  // 索引越界对 (U/W 空)
    const ipv::IterTransResult r = ipv::iter_trans_solve(U, W, p, 5.0, 1);
    return (r.success || r.trans.valid || r.n_inliers != 0 ||
            r.rms != 0.0 || !r.residuals.empty() || !r.inliers.empty())
               ? 1 : 0;
}

int child_zero_pairs_no_pairs() {
    std::vector<ipv::StarPoint> U, W;
    std::vector<ipv::MatchPair> p;
    fix_wcs_c_few_stars(&U, &W, &p, 8);
    p.clear();  // U/W 非空, 空 initial_pairs
    const ipv::IterTransResult r = ipv::iter_trans_solve(U, W, p, 5.0, 1);
    return (r.success || r.trans.valid || r.n_inliers != 0 ||
            r.rms != 0.0 || !r.residuals.empty() || !r.inliers.empty())
               ? 1 : 0;
}

// CD det 退化 (共线场): 冻结语义 success=0 + 禁止坍缩值冒充解
int child_collinear() {
    std::vector<ipv::StarPoint> U, W;
    std::vector<ipv::MatchPair> p;
    fix_wcs_e_collinear(&U, &W, &p, 30);
    const ipv::IterTransResult r = ipv::iter_trans_solve(U, W, p, 5.0, 1);
    if (r.success) return 1;  // 坍缩冒充解 = 违背 DISP-WCS-001 冻结语义
    // trans 全零/invalid (非病态数值冒充)
    if (r.trans.valid) return 1;
    return 0;
}

// 指向偏差>FOV 等价注入 (无真实对应场): 全链 triangle→iter_trans 显式失败
int child_unmatched_offset() {
    std::vector<ipv::StarPoint> U, W;
    std::vector<ipv::MatchPair> p;
    fix_wcs_d_unmatched(&U, &W, &p, 128);
    const ipv::TriangleMatchResult tm =
        ipv::triangle_match(U, W, 60, 60, 0.002, 0.4);
    const ipv::IterTransResult r =
        ipv::iter_trans_solve(U, W, tm.top_pairs, 5.0, 1);
    // 显式失败: 拟合不成功 (内点枯竭/匹配崩塌), 禁止以大 rms 解冒充
    if (r.success && r.rms > 0.5) return 1;  // 大残差"解"=冒充
    if (r.success && r.n_inliers < 12) return 1;
    return 0;
}

// NaN/Inf 输入: 进程不崩 (冻结 F4 "进程不崩溃"), 失败或显式输出皆可
int child_nan_input() {
    std::vector<ipv::StarPoint> U, W;
    std::vector<ipv::MatchPair> p;
    fix_wcs_c_few_stars(&U, &W, &p, 8);
    U[3].x = std::nan("");
    U[5].y = std::numeric_limits<double>::infinity();
    W[2].x = std::nan("");    const ipv::IterTransResult r = ipv::iter_trans_solve(U, W, p, 5.0, 1);
    (void)r;  // 不崩即达 F4 底线; success 值不约束 (现状未登记 NaN 语义)
    return 0;
}

}  // namespace

namespace p1wcs {

int test_negative() {
    CheckState cs;
    cs.failures = 0;
    cs.fault_reported = false;

    // F4: 1/2 星 → success=0 (fork 隔离, 进程不崩)
    {
        const ForkOutcome a = run_forked(child_few_stars_1);
        const ForkOutcome b = run_forked(child_few_stars_2);
        P1WCS_CHECK(cs, a.exited && a.rc == 0, "n1_few_stars");
        P1WCS_CHECK(cs, b.exited && b.rc == 0, "n1_few_stars");
    }

    // WCS-002 翻锚: 空对确定性路径 (直接调用, 不 fork — 零 UB 由本断言
    // 进程存活 + asan/ubsan 实证背书; 三空对组合全覆盖, 断言确定 rc
    // success=0 + 确定零输出态)。注入名沿用 n1_zero_pairs_anchor。
    {
        const int ra = child_zero_pairs_all_empty();
        const int rb = child_zero_pairs_idx_out_of_range();
        const int rc = child_zero_pairs_no_pairs();
        P1WCS_CHECK(cs, ra == 0, "n1_zero_pairs_anchor");
        P1WCS_CHECK(cs, rb == 0, "n1_zero_pairs_anchor");
        P1WCS_CHECK(cs, rc == 0, "n1_zero_pairs_anchor");
    }

    // F4/DISP-WCS-001: 共线退化 → success=0, 禁止坍缩冒充
    {
        const ForkOutcome c = run_forked(child_collinear);
        P1WCS_CHECK(cs, c.exited && c.rc == 0, "n1_collinear_success");
    }

    // F4: 指向偏差>FOV (无对应场) → 显式失败
    {
        const ForkOutcome d = run_forked(child_unmatched_offset);
        P1WCS_CHECK(cs, d.exited && d.rc == 0, "n1_unmatched_offset");
    }

    // F4: NaN/Inf → 进程不崩
    {
        const ForkOutcome e = run_forked(child_nan_input);
        P1WCS_CHECK(cs, e.exited && e.rc == 0, "n1_nan_input");
    }

    // DISP-WCS-001 同族现状行为锚: WcsTan.sky2pix det 退化 → 返回 CRPIX
    // (wcs_tan.cpp:48-51 冻结现状; 整改归 P1-WCS-IMPL, 本锚锁定漂移)
    {
        astrocs::phase1::WcsTan bad;
        bad.crpix1 = 10.0;
        bad.crpix2 = 20.0;
        bad.crval1 = 150.0;
        bad.crval2 = 2.0;
        bad.cd11 = 1e-16;
        bad.cd12 = 0.0;
        bad.cd21 = 0.0;
        bad.cd22 = 1e-16;  // det=1e-32 < 1e-30
        double x = -1.0, y = -1.0;
        bad.sky2pix(160.0, 3.0, &x, &y);
        P1WCS_CHECK(cs, x == bad.crpix1 && y == bad.crpix2,
                    "n1_wcs_tan_degenerate");
    }

    // 新发现缺陷行为锚 (P1-WCS-TEST 2026-09-09 登记, 建议 DISP-WCS-007):
    // WcsTan.pix2sky 将 (ξ,η) 的 deg 数值未转 rad 直接进球面公式
    // (wcs_tan.cpp:15-16 xi=cd·dx 后直接 atan/asin; sky2pix 同族输出单位
    // 错与之成对抵消 → roundtrip 自洽掩盖)。交叉对拍现状差 ~3.2° 量级
    // (u=±470px 处)。本锚锁定"缺陷存在"现状: 偏差 >1e-6 deg 为真。
    // P1-WCS-IMPL 修复 (单位一致化) 后本锚必红, 须同步翻转为 ≤1e-9 并在
    // TASK_RESULT 登记整改。
    {
        astrocs::phase1::WcsTan wt;
        wt.crpix1 = 512.5;
        wt.crpix2 = 512.5;
        wt.crval1 = 150.0;
        wt.crval2 = 2.0;
        wt.cd11 = 1.109723e-4;
        wt.cd12 = 5.553241e-6;
        wt.cd21 = 5.553241e-6;
        wt.cd22 = -1.109723e-4;  // deg/px (DOC §9 冻结语义)
        double ra1 = 0.0, dec1 = 0.0, ra2 = 0.0, dec2 = 0.0;
        wt.pix2sky(40.0, 1000.0, &ra1, &dec1);
        oracle_wcs_forward(wt.cd11, wt.cd12, wt.cd21, wt.cd22, wt.crval1,
                           wt.crval2, wt.crpix1, wt.crpix2, nullptr, nullptr, 0,
                           40.0, 1000.0, &ra2, &dec2);
        const double dra = std::fabs(ra1 - ra2) * std::cos(dec2 * kDegToRad);
        P1WCS_CHECK(cs, std::max(dra, std::fabs(dec1 - dec2)) > 1e-6,
                    "n1_wcs_tan_unit_anchor");
    }

    // DISP-WCS-004 相邻面行为锚: extract_wcs_sip 对全零 trans (共线产物)
    // 输出语义现状锁定 (probe 待测项, 影子树首跑校准; 断言"输出不包含
    // NaN 冒充"底线)
    {
        ipv::WcsFitResult w;
        std::memset(&w, 0, sizeof(w));
        ipv::Trans zero;
        std::vector<ipv::StarPoint> U, W;
        std::vector<ipv::MatchPair> p;
        fix_wcs_e_collinear(&U, &W, &p, 12);
        extract_wcs_sip(zero, 150.0, 2.0, 1024, 1024, 0.4, U, W, p, &w,
                        nullptr);
        // 底线: 输出数值不得为 NaN/Inf (坍缩可以为零, 但禁止毒值冒充)
        const bool poison =
            !std::isfinite(w.cd.cd11) || !std::isfinite(w.cd.cd12) ||
            !std::isfinite(w.cd.cd21) || !std::isfinite(w.cd.cd22) ||
            !std::isfinite(w.rms_arcsec) || !std::isfinite(w.rms_px);
        P1WCS_CHECK(cs, !poison, "n1_extract_zero_trans");
        // 现状锚: 全零 trans → cd 全零 (不冒充真实解)
        P1WCS_CHECK(cs, w.cd.cd11 == 0.0 && w.cd.cd22 == 0.0,
                    "n1_extract_zero_trans");
    }

    if (cs.failures == 0) {
        std::fprintf(stdout, "P1WCS NEGATIVE PASS\n");
        return 0;
    }
    std::fprintf(stderr, "P1WCS NEGATIVE FAIL (%d check(s))\n", cs.failures);
    return 1;
}

}  // namespace p1wcs
