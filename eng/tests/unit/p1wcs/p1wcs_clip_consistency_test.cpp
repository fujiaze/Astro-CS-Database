// P1-WCS · R-58-3-1 回归锁: 报告的残差统计必须与解自己的 sigma-clip 规则一致
// ---------------------------------------------------------------------------
// 缺陷 (R-57 条款 3 第二处独立缺陷, 见 run/FINAL-07/审核包/端到端/五帧越闸定性报告.md §6.2):
//   at_recalc_trans 内部做二轮 sigma-clip 后**用裁剪集重拟合** TRANS, 并把裁剪集放在
//   IterTransResult::inliers / .rms; 但 ipv_solver 的三个调用点把 at_match_lists 的
//   **未裁剪**匹配表塞回 rep_result.matched, 于是 extract_wcs_sip 在**未裁剪**表上
//   重算 rms/n_pairs —— 报告值与解自己的裁剪规则不一致。
//   实测锚 (M42_M5@035959): 解按 32 对收敛 (0.121"), 报告却是 34 对 0.634" ⇒ 6.2x 虚高。
//
// 本测试 = 该缺陷的"能红能绿"闭环:
//   绿: 报告统计 == 在 inliers 上算出的统计, 且与未裁剪统计有量级差 (32/34 锚);
//   红: 若 at_recalc_trans 不再返回裁剪集 (result.inliers = matched_pairs),
//       或 ipv_solver 又发布未裁剪表 (源面守卫), 断言立刻失败。
// 期望值全部由本文件的独立仿射求值器推导, 不经被测函数 (oracle 纪律)。
// ---------------------------------------------------------------------------
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ipv_itertrans.h"
#include "ipv_solver.h"
#include "ipv_types.h"

using ipv::MatchPair;
using ipv::StarPoint;
using ipv::Trans;
using ipv::IterTransResult;
using ipv::WcsFitResult;
using ipv::at_recalc_trans;
using ipv::extract_wcs_sip;

namespace {

int g_fail = 0;
int g_check = 0;

void check(bool ok, const std::string& what) {
    ++g_check;
    if (!ok) { ++g_fail; std::printf("  [FAIL] %s\n", what.c_str()); }
    else     { std::printf("  [ ok ] %s\n", what.c_str()); }
}

// ---- 独立仿射/多项式求值 (oracle 侧, 不调用被测函数) ----
void oracle_eval(const Trans& t, double ux, double uy, double* wx, double* wy) {
    const double x2 = ux * ux, y2 = uy * uy, xy = ux * uy;
    const double x3 = x2 * ux, y3 = y2 * uy;
    *wx = t.x00 + t.x10 * ux + t.x01 * uy + t.x20 * x2 + t.x11 * xy + t.x02 * y2 +
          t.x30 * x3 + t.x21 * x2 * uy + t.x12 * ux * y2 + t.x03 * y3;
    *wy = t.y00 + t.y10 * ux + t.y01 * uy + t.y20 * x2 + t.y11 * xy + t.y02 * y2 +
          t.y30 * x3 + t.y21 * x2 * uy + t.y12 * ux * y2 + t.y03 * y3;
}

// 在给定匹配表上算 RMS (角秒) —— 与 at_recalc_trans 的统计口径同式, 但独立实现
double rms_over(const Trans& t, const std::vector<StarPoint>& U,
                const std::vector<StarPoint>& W, const std::vector<MatchPair>& pairs) {
    double s = 0.0;
    for (const MatchPair& mp : pairs) {
        double wx, wy;
        oracle_eval(t, U[mp.u].x, U[mp.u].y, &wx, &wy);
        const double dx = wx - W[mp.w].x, dy = wy - W[mp.w].y;
        s += dx * dx + dy * dy;
    }
    return pairs.empty() ? 0.0 : std::sqrt(s / static_cast<double>(pairs.size()));
}

std::string read_file(const char* path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::size_t count_sub(const std::string& hay, const std::string& needle) {
    std::size_t n = 0, pos = 0;
    while ((pos = hay.find(needle, pos)) != std::string::npos) { ++n; pos += needle.size(); }
    return n;
}

}  // namespace

int main() {
    std::printf("R58-3-1 CLIP CONSISTENCY\n");

    // ---- fixture: 34 对 (32 内点 + 2 未裁剪粗差), 复刻 M42_M5@035959 的形状 ----
    // 真值仿射: W = A·U + b (U 为 IPV 内部帧像素, W 为角秒)
    const double a11 = -0.9669, a12 = 0.0, a21 = 0.0, a22 = 0.9669;
    const double b1 = 3.5, b2 = -2.25;
    const int N_IN = 32, N_OUT = 2;
    const int N = N_IN + N_OUT;
    // 内点残差: ±0.12" 交替 (零均值、不可被仿射吸收) ⇒ 裁剪后 RMS ~ 0.12"
    const double sig_in = 0.12;
    // 未裁剪粗差: 2.57" (R-57 报告里由 0.634"/32 对反解出的单对偏差)
    const double out_off = 2.57;

    std::vector<StarPoint> U(N), W(N);
    std::vector<MatchPair> pairs(N);
    for (int i = 0; i < N; ++i) {
        const int gx = i % 8, gy = i / 8;
        const double ux = -1200.0 + 300.0 * gx + 7.0 * (i % 3);
        const double uy = -800.0 + 220.0 * gy - 5.0 * (i % 4);
        U[i].x = ux; U[i].y = uy; U[i].flux = 1000.0; U[i].saturated = false;
        double wx = a11 * ux + a12 * uy + b1;
        double wy = a21 * ux + a22 * uy + b2;
        if (i < N_IN) {
            wx += ((i % 2 == 0) ? sig_in : -sig_in);
        } else {
            wx += out_off;
        }
        W[i].x = wx; W[i].y = wy; W[i].flux = 1000.0; W[i].saturated = false;
        pairs[i].u = i; pairs[i].w = i;
    }

    std::printf("  fixture: N=%d (内点 %d 残差 ±%.2f\", 粗差 %d 偏移 %.2f\")\n",
                N, N_IN, sig_in, N_OUT, out_off);

    // ---- 1) 提供方 (at_recalc_trans) 必须返回裁剪集, 且 rms 只在裁剪集上统计 ----
    IterTransResult res = at_recalc_trans(U, W, pairs, 1);
    check(res.success, "at_recalc_trans 成功");
    if (!res.success) { std::printf("R58-3-1 CLIP CONSISTENCY: FAIL\n"); return 1; }
    check(static_cast<int>(res.inliers.size()) == N_IN,
          "inliers = 裁剪集 (期望 32, 实得 " + std::to_string(res.inliers.size()) + ")");
    check(res.n_inliers == static_cast<int>(res.inliers.size()),
          "n_inliers 与 inliers.size() 一致");
    const double rms_unclipped = rms_over(res.trans, U, W, pairs);
    const double rms_clipped = rms_over(res.trans, U, W, res.inliers);
    std::printf("  rms: 报告=%.4f\" | inliers(%zu)=%.4f\" | 未裁剪(%d)=%.4f\"\n",
                res.rms, res.inliers.size(), rms_clipped, N, rms_unclipped);
    check(std::fabs(res.rms - rms_clipped) < 1e-9,
          "报告 rms == 在 inliers 上算出的 rms (差 " +
              std::to_string(std::fabs(res.rms - rms_clipped)) + ")");
    check(res.rms < 0.5 * rms_unclipped,
          "裁剪/未裁剪统计有量级差 (比 " + std::to_string(res.rms / rms_unclipped) + " < 0.5)");
    check(res.rms > 0.05 && res.rms < 0.35, "裁剪后 rms 落在内点噪声量级 (0.05~0.35\")");
    check(rms_unclipped > 0.30 && rms_unclipped < 0.95,
          "未裁剪 rms 落在 R-57 锚量级 (0.30~0.95\")");

    // ---- 2) 发布面: extract_wcs_sip 的报告值取决于**喂进去的**匹配表 ----
    WcsFitResult pub_clipped, pub_unclipped;
    extract_wcs_sip(res.trans, 83.8, -5.4, 4096, 4096, 0.9669, U, W,
                    res.inliers, &pub_clipped, nullptr);
    extract_wcs_sip(res.trans, 83.8, -5.4, 4096, 4096, 0.9669, U, W,
                    pairs, &pub_unclipped, nullptr);
    std::printf("  发布面: 喂裁剪集 n_pairs=%d rms_px=%.4f | 喂未裁剪表 n_pairs=%d rms_px=%.4f\n",
                pub_clipped.n_pairs, pub_clipped.rms_px,
                pub_unclipped.n_pairs, pub_unclipped.rms_px);
    check(pub_unclipped.n_pairs == N && pub_clipped.n_pairs == N_IN,
          "extract_wcs_sip 的 n_pairs 等于传入匹配表大小");
    check(pub_unclipped.rms_px > 3.0 * pub_clipped.rms_px,
          "喂未裁剪表会把报告 rms_px 抬高 >3x (缺陷的可观测后果)");

    // ---- 3) 调用方源面守卫: 不得再把未裁剪表发布成 rep_result.matched ----
    const char* kSrc = P1WCS_IPV_SOLVER_SRC;
    const std::string src = read_file(kSrc);
    check(!src.empty(), std::string("可读取 ipv_solver.cpp: ") + kSrc);
    check(count_sub(src, "rep_result.matched = hi_matches;") == 0,
          "ipv_solver.cpp 不再发布未裁剪的 hi_matches");
    check(count_sub(src, "rep_result.matched = hi_result.inliers;") == 3,
          "ipv_solver.cpp 三个调用点均发布 hi_result.inliers (实测 " +
              std::to_string(count_sub(src, "rep_result.matched = hi_result.inliers;")) + "/3)");

    std::printf("R58-3-1 CLIP CONSISTENCY: %s (%d 检查, %d 失败)\n",
                g_fail == 0 ? "PASS" : "FAIL", g_check, g_fail);
    return g_fail == 0 ? 0 : 1;
}
