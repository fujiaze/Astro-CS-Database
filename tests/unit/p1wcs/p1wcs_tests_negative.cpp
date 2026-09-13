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

// B2-A1: 两天球坐标角距 (deg, haversine; 经度环绕安全) — 绝对正确性门度量。
double angular_sep_deg(double ra1, double dec1, double ra2, double dec2) {
    const double d1 = dec1 * kDegToRad, d2 = dec2 * kDegToRad;
    double dra = (ra2 - ra1) * kDegToRad;
    while (dra > M_PI) dra -= 2.0 * M_PI;
    while (dra < -M_PI) dra += 2.0 * M_PI;
    const double sh = std::sin((d2 - d1) / 2.0);
    const double sn = std::sin(dra / 2.0);
    double s = sh * sh + std::cos(d1) * std::cos(d2) * sn * sn;
    if (s > 1.0) s = 1.0;
    if (s < 0.0) s = 0.0;
    return 2.0 * std::asin(std::sqrt(s)) / kDegToRad;
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

    // B2-A1 翻锚 (原为"缺陷行为锚", 锁定 DISP-WCS-007 现状: 偏差 >1e-6):
    // WcsTan.pix2sky 的 ξ/η deg→rad 单位错已修 (wcs_tan.cpp 显式 ·π/180;
    // sky2pix 同族单位错同步翻转; 权威 FITS-WCS Paper I §2.2 / Paper II)。
    // 本锚改为**绝对正确性锚**: 与 p1wcs_oracle.hpp 的独立 TAN 逆投影
    // (oracle_wcs_forward, gnomonic 闭式解, 不调用 WcsTan) 对拍, 偏差必须
    // ≤1e-9 deg。阈值依据 (与生产门 module_adapters.cpp p1_op_wcs 一致):
    // 两条独立路径均为 FP64, 本点位移 ~678 px × 1.11e-4 deg/px ⇒ |ξ,η| ~
    // 0.075 deg, 舍入 ~1e-15 deg; 1e-9 deg (=3.6e-6") 高出舍入 6 个量级,
    // 又比修复前偏差 (~5.6e-1 deg) 低 8 个量级 ⇒ 非恒真且可检出回归。
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
        const double err_deg = angular_sep_deg(ra1, dec1, ra2, dec2);
        P1WCS_CHECK(cs, err_deg <= 1e-9, "n1_wcs_tan_unit_anchor");
    }

    // B2-A1 增补: 桥接注入必败 (杀死"去桥接 / 双桥接")。
    // WcsTan 契约 (wcs_tan.h): crpix 1-based、dx = x − crpix1 ⇒ 消费方传入
    // FITS 1-based 像素。声明用法与独立 oracle 前向一致 (≤1e-9 deg);
    // "去桥接"(误把 1-based 当 0-based, 传 x−1) 或"双桥接"(再 +1) 均引入
    // 1 px 量级偏差 (≈1.11e-4 deg @ |CD|≈1.1097e-4 deg/px), 必须被同一
    // 绝对门检出 (≥1e-6 deg; 与声明值差 3 个量级, 非恒真)。
    {
        astrocs::phase1::WcsTan wt;
        wt.crpix1 = 512.5;
        wt.crpix2 = 512.5;
        wt.crval1 = 150.0;
        wt.crval2 = 2.0;
        wt.cd11 = 1.109723e-4;
        wt.cd12 = 5.553241e-6;
        wt.cd21 = 5.553241e-6;
        wt.cd22 = -1.109723e-4;
        const double x1 = 700.0, y1 = 300.0;  // FITS 1-based (声明契约)
        double ra_ref = 0.0, dec_ref = 0.0;
        oracle_wcs_forward(wt.cd11, wt.cd12, wt.cd21, wt.cd22, wt.crval1,
                           wt.crval2, wt.crpix1, wt.crpix2, nullptr, nullptr, 0,
                           x1, y1, &ra_ref, &dec_ref);
        double ra_d = 0.0, dec_d = 0.0, ra_nb = 0.0, dec_nb = 0.0;
        double ra_db = 0.0, dec_db = 0.0;
        wt.pix2sky(x1, y1, &ra_d, &dec_d);                  // 声明桥接
        wt.pix2sky(x1 - 1.0, y1 - 1.0, &ra_nb, &dec_nb);    // 去桥接
        wt.pix2sky(x1 + 1.0, y1 + 1.0, &ra_db, &dec_db);    // 双桥接
        const double e_decl = angular_sep_deg(ra_d, dec_d, ra_ref, dec_ref);
        const double e_nb = angular_sep_deg(ra_nb, dec_nb, ra_ref, dec_ref);
        const double e_db = angular_sep_deg(ra_db, dec_db, ra_ref, dec_ref);
        P1WCS_CHECK(cs, e_decl <= 1e-9, "n1_wcs_tan_bridge_declared");
        P1WCS_CHECK(cs, e_nb >= 1e-6, "n1_wcs_tan_bridge_removed");
        P1WCS_CHECK(cs, e_db >= 1e-6, "n1_wcs_tan_bridge_doubled");
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
