// P1-WCS-TEST · apbp 组 (WCS-003: AP/BP 布局扩展 + 消费方迭代式反演冻结门)
//
// 合同锚: docs/algorithms/PLATESOLVE.md §11.4 F2 (SIP 前向/逆向 roundtrip
// |Δ|≤1e-4 px, 冻结不放宽); owner 裁决 1 选 B (2026-09-09): AP/BP 布局扩展
// + 消费方迭代式反演, 恢复冻结门, 不接受缩小 fixture 畸变量级。
// **分层门**（GATE-WCS-01 裁决 2; 依据 run/GATE-DERIVE-01/REPORT.md §5.4/§6.3）:
//   G-P1-WCS-RT      全链冻结门 1e-4 px（SCI/ALG 冻结值, 不放宽; 判据力弱）
//   G-P1-WCS-RT-ITER 迭代反演路径紧门 = κ_iter·τ = 1e-8 px（τ = 1e-9 px）
//   G-P1-WCS-RT-APBP AP/BP 多项式逆**表示门** = 10% × 边缘畸变预算(px)
//   三条路径地板相差 5–9 个量级 ⇒ 同一个数不能同时服务它们（REPORT §6.3）。
//
// 六类验收覆盖 (本组全部落断言, 数值证据导出 run/p1wcs_wcs003/):
//   1. 中心 90% 区域: 三档畸变 fixture 冻结门 <1e-4 px
//   2. 图像边界与角点: 四边 + 四角 冻结门 <1e-4 px
//   3. ≥1000 确定性随机采样点 (固定 seed): 三档 × 1200 点 <1e-4 px
//   4. 奇点/非有限/超限确定性拒绝 (零 UB, 两次调用 bitwise 一致)
//   5. 1 worker / N worker 一致性 (采样编排并行, 反演逐点 bitwise)
//   6. Astropy 第三方交叉验证 (独立 ctest p1wcs_astropy_cross, 只入测试面)
//
// 被测面: extract_wcs_sip (扩展逆向 APx/BPx, ipv_wcs.cpp 5.4 段) +
//         wcs_sky_to_pixel_iterative (迭代式反演, ipv_wcs.cpp 尾段)。
// 独立性: 期望值/前向锚全由 p1wcs_oracle.hpp 推导 (oracle-4 TAN 前向),
//         被测函数只出现在被对拍一侧。
// 原始高畸变 fixture (FIX-WCS-B) 原样保留, 不进本组 (既有 oracle 组锚);
// 本组使用 FIX-WCS-F 三档 (low 0.1× / mid 0.5× / high 1.0×, high 与冻结
// fixture 畸变同量级 ~118px)。
#include "p1wcs_test_main.hpp"
#include "p1wcs_fixtures.hpp"
#include "p1wcs_oracle.hpp"

#include <omp.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "ipv_itertrans.h"
#include "ipv_solver.h"
#include "ipv_wcs.h"   // wcs_sky_to_pixel_iterative (WCS-003 迭代式反演)

using namespace p1wcs;

namespace {

constexpr unsigned kSeedF1 = 20260911u;  // FIX-WCS-F low
constexpr unsigned kSeedF2 = 20260912u;  // FIX-WCS-F mid
constexpr unsigned kSeedF3 = 20260913u;  // FIX-WCS-F high
constexpr unsigned kSeedRand = 20260914u;  // 随机采样点 (固定 seed)

constexpr int kRandPoints = 1200;  // ≥1000 确定性随机采样点
constexpr double kFreezePx = 1e-4; // F2 冻结门 (px, 不放宽)

// ---- 分层门（GATE-WCS-01 裁决 2）------------------------------------------
// τ = 迭代反演收敛容差(px, 契约) —— 反演判据 max|F(u)| < τ, F(u)=u+A(u)−UV。
// 门值 = κ_iter·τ（κ_iter = 10: 实测最坏 2.91e-9 px = 2.9·τ ⇒ 余量 3.4×,
// κ_iter=10 再为跨平台 libm/FMA 差异留余量）。
constexpr double kIterTolPx = 1e-9;      // τ（invert_point() 的单一来源）
constexpr double kIterGateKappa = 10.0;  // κ_iter
constexpr double kIterGatePx = kIterGateKappa * kIterTolPx;   // 1e-8 px
// AP/BP 多项式逆**表示门**: 一步直加残差 ≤ 10% × 边缘畸变预算(px)。
// 实测（本组 fixture, APx 阶 7/81×81 拟合）: low 1.9e-4%、mid 0.036%、high 1.89%
// ⇒ 最坏档余量 5.3×。该门回答「多项式表示残差是否在畸变预算的合理比例内」,
// **不是** FP64 地板门（~184 px 畸变下多项式逆根本达不到 1e-4 px: 实测 3.46 px）。
constexpr double kApbpBudgetFrac = 0.10;

struct SolveOut {
    ipv::WcsFitResult w;
    bool ok;
};

SolveOut solve_f(const FixWcsF& fx) {
    const ipv::IterTransResult r =
        ipv::iter_trans_solve(fx.U, fx.W, fx.pairs, 5.0, 2);
    SolveOut so;
    so.ok = r.success;
    if (r.success) {
        extract_wcs_sip(r.trans, fx.truth.ra0, fx.truth.dec0, fx.width,
                        fx.height, fx.truth.s0, fx.U, fx.W, r.inliers, &so.w,
                        nullptr);
    }
    return so;
}

// 反演逐点结果 (POD, 逐字段 bitwise 比较)
struct IterPoint {
    double x, y;
    int iterations;
    int reject_code;
    bool converged;
};

// τ 可变的反演（用于「度量随 τ 缩小」的判别性负例, 与生产契约同函数）
IterPoint invert_point_tau(const ipv::WcsFitResult& w, double ra, double dec,
                           double tau_px) {
    const ipv::WcsIterativeResult r =
        ipv::wcs_sky_to_pixel_iterative(w, ra, dec, tau_px, 64);
    IterPoint p;
    p.x = r.x;
    p.y = r.y;
    p.iterations = r.iterations;
    p.reject_code = r.reject_code;
    p.converged = r.converged;
    return p;
}

// 生产契约 τ = kIterTolPx（单一来源; 反演判据 max|F(u)| < τ）
IterPoint invert_point(const ipv::WcsFitResult& w, double ra, double dec) {
    return invert_point_tau(w, ra, dec, kIterTolPx);
}

bool iter_point_bitwise(const IterPoint& a, const IterPoint& b) {
    return a.converged == b.converged && a.reject_code == b.reject_code &&
           a.iterations == b.iterations &&
           std::memcmp(&a.x, &b.x, sizeof(double)) == 0 &&
           std::memcmp(&a.y, &b.y, sizeof(double)) == 0;
}

// 真值 ra/dec (fixture 真值域: 真值像素 → truth_apply_f → gnomonic_inv)
void truth_radec(const FixWcsF& fx, double x_f, double y_f,
                 double* ra, double* dec) {
    const double cx = fx.width / 2.0, cy = fx.height / 2.0;
    const double u_up = x_f - 0.5 - cx;
    const double v_up = cy - (y_f - 0.5);
    double wx, wy;
    truth_apply_f(fx, u_up, v_up, &wx, &wy);
    oracle_gnomonic_inv(wx / 3600.0, wy / 3600.0, fx.truth.ra0, fx.truth.dec0,
                        ra, dec);
}

// 闭合 roundtrip: 真值像素 → oracle-4 前向 (生产系数) → 生产迭代反演 → 像素
// (前向锚=独立 oracle-4 实现, 反演=被测; 误差即 AP/BP/迭代反演口径)
double closure_rt(const ipv::WcsFitResult& w, double x_f, double y_f,
                  double* ra_out = nullptr, double* dec_out = nullptr,
                  IterPoint* ip_out = nullptr) {
    double ra, dec;
    oracle_wcs_forward(w.cd.cd11, w.cd.cd12, w.cd.cd21, w.cd.cd22,
                       w.crval[0], w.crval[1], w.crpix[0], w.crpix[1],
                       w.sip.A, w.sip.B, w.sip.order, x_f, y_f, &ra, &dec);
    if (ra_out) *ra_out = ra;
    if (dec_out) *dec_out = dec;
    const IterPoint ip = invert_point(w, ra, dec);
    if (ip_out) *ip_out = ip;
    if (!ip.converged) return -1.0;  // 未收敛 (拒绝路径由调用方处理)
    return std::hypot(ip.x - x_f, ip.y - y_f);
}

struct DistStats {
    double max_rt;
    double sum_rt;
    int n;
    int n_reject;
    double max_reject_rt;  // 拒绝点不计入门值
};

// 三档 fixture 常量 (dist_scale: low/mid/high; high 与冻结 fixture 同量级)
struct FixtureSpec {
    const char* name;
    unsigned seed;
    double scale;
};

const FixtureSpec kFixtures[3] = {
    {"low", kSeedF1, 0.1},
    {"mid", kSeedF2, 0.5},
    {"high", kSeedF3, 1.0},
};

}  // namespace

namespace p1wcs {

int test_apbp() {
    CheckState cs;
    cs.failures = 0;
    cs.fault_reported = false;

    // ------------------------------------------------------------------
    // 共用采样点集 (确定性):
    //   center90 网格 21×21, 边界四边 33×4 + 四角 4, 随机 1200 (固定 seed)
    // ------------------------------------------------------------------
    std::vector<std::pair<double, double>> center90_pts;
    {
        const double lo = 0.05 * 1024.0, hi = 0.95 * 1024.0;
        for (int i = 0; i < 21; ++i)
            for (int j = 0; j < 21; ++j)
                center90_pts.emplace_back(
                    lo + (hi - lo) * i / 20.0, lo + (hi - lo) * j / 20.0);
    }
    std::vector<std::pair<double, double>> boundary_pts;
    {
        const double eps = 0.5, W = 1024.0;
        for (int i = 0; i < 33; ++i) {
            const double t = eps + (W - 2.0 * eps) * i / 32.0;
            boundary_pts.emplace_back(eps, t);        // 左边
            boundary_pts.emplace_back(W - eps, t);    // 右边
            boundary_pts.emplace_back(t, eps);        // 下边
            boundary_pts.emplace_back(t, W - eps);    // 上边
        }
        boundary_pts.emplace_back(eps, eps);                   // 角
        boundary_pts.emplace_back(W - eps, eps);
        boundary_pts.emplace_back(eps, W - eps);
        boundary_pts.emplace_back(W - eps, W - eps);
    }
    std::vector<std::pair<double, double>> random_pts;
    {
        std::uint64_t st = kSeedRand;
        for (int k = 0; k < kRandPoints; ++k) {
            const double x = 0.5 + uniform01(st) * (1024.0 - 1.0);
            const double y = 0.5 + uniform01(st) * (1024.0 - 1.0);
            random_pts.emplace_back(x, y);
        }
    }

    // ------------------------------------------------------------------
    // f2x_layout_expand: 三档求解 + 布局扩展登记 (apx_order=7, 网格 81,
    // 拟合阶 7; 36 项兼容层 ap_order=5 不回归) + APx 一步误差表
    // ------------------------------------------------------------------
    SolveOut sol[3];
    double apx_onestep_max[3] = {0, 0, 0};
    double ap36_onestep_max[3] = {0, 0, 0};
    double edge_dist_px[3] = {0, 0, 0};
    for (int f = 0; f < 3; ++f) {
        const FixWcsF fx = fix_wcs_f_distortion(kFixtures[f].seed,
                                                kFixtures[f].scale);
        sol[f] = solve_f(fx);
        P1WCS_CHECK(cs, sol[f].ok && sol[f].w.success,
                    "f2x_layout_expand");
        P1WCS_CHECK(cs, sol[f].w.sip.apx_order == 7 &&
                            sol[f].w.sip.ap_order == 5,
                    "f2x_layout_expand");
        // 边缘畸变量级 (真值, 像素域): 角点 (512,512) 前向畸变位移 / s0
        {
            double wx, wy;
            truth_apply_f(fx, 512.0, 512.0, &wx, &wy);
            double wl_x, wl_y;
            const TruthLinear& tr = fx.truth;
            wl_x = tr.m00 * 512.0 + tr.m01 * 512.0 + tr.t0;
            wl_y = tr.m10 * 512.0 + tr.m11 * 512.0 + tr.t1;
            edge_dist_px[f] = std::hypot(wx - wl_x, wy - wl_y) / tr.s0;
        }
        // APx 一步直加误差 (center90 网格, 独立 oracle-7 求值 vs 真值逆):
        // 真值逆映射 u*(UV) 由 oracle_wcs_reverse (A/B 固定点迭代 60 次,
        // 收敛 <1e-9 px) 给出 — APx 一步 = UV + APx(UV)。
        const ipv::WcsFitResult& w = sol[f].w;
        double i00, i01, i10, i11;
        oracle_invert2(w.cd.cd11, w.cd.cd12, w.cd.cd21, w.cd.cd22,
                       &i00, &i01, &i10, &i11);
        for (const auto& pt : center90_pts) {
            double ra, dec;
            oracle_wcs_forward(w.cd.cd11, w.cd.cd12, w.cd.cd21, w.cd.cd22,
                               w.crval[0], w.crval[1], w.crpix[0], w.crpix[1],
                               w.sip.A, w.sip.B, w.sip.order, pt.first,
                               pt.second, &ra, &dec);
            double xi, eta;
            oracle_gnomonic(ra, dec, w.crval[0], w.crval[1], &xi, &eta);
            const double uvx = i00 * xi + i01 * eta;
            const double uvy = i10 * xi + i11 * eta;
            double cx1, cy1, cx2, cy2;
            oracle_sip_eval_stride(w.sip.APx, w.sip.BPx, w.sip.apx_order,
                                   10, uvx, uvy, &cx1, &cy1);
            oracle_sip_eval_stride(w.sip.AP, w.sip.BP, w.sip.ap_order,
                                   6, uvx, uvy, &cx2, &cy2);
            double xr, yr;
            oracle_wcs_reverse(w.cd.cd11, w.cd.cd12, w.cd.cd21, w.cd.cd22,
                               w.crval[0], w.crval[1], w.crpix[0], w.crpix[1],
                               w.sip.A, w.sip.B, w.sip.order, ra, dec,
                               &xr, &yr);
            apx_onestep_max[f] = std::max(
                apx_onestep_max[f],
                std::hypot(uvx + cx1 - (xr - w.crpix[0]),
                           uvy + cy1 - (yr - w.crpix[1])));
            ap36_onestep_max[f] = std::max(
                ap36_onestep_max[f],
                std::hypot(uvx + cx2 - (xr - w.crpix[0]),
                           uvy + cy2 - (yr - w.crpix[1])));
        }
        // 布局扩展防退化观察线 (非验收线): 一步直加不劣于旧行为量级
        P1WCS_CHECK(cs, ap36_onestep_max[f] < 50.0, "f2x_layout_expand");
    }

    // ------------------------------------------------------------------
    // 1. f2x_center90_freeze: 中心 90% 区域冻结门 <1e-4 px (三档)
    // 2. f2x_boundary_freeze: 图像边界与角点冻结门 <1e-4 px (三档)
    // 3. f2x_random_freeze:   1200 确定性随机点冻结门 <1e-4 px (三档)
    // 4. f2x_truth_anchor:    真值锚 (真值 ra/dec → 反演 → 真值像素,
    //                         含 CD/A/CRVAL 拟合误差全链) <1e-4 px (三档)
    // ------------------------------------------------------------------
    DistStats st_c90[3], st_bnd[3], st_rnd[3], st_tru[3];
    for (int f = 0; f < 3; ++f) {
        const ipv::WcsFitResult& w = sol[f].w;
        const FixWcsF fx = fix_wcs_f_distortion(kFixtures[f].seed,
                                                kFixtures[f].scale);
        auto run_set = [&](const std::vector<std::pair<double, double>>& pts,
                           DistStats* st, bool truth_anchor) {
            *st = DistStats{0.0, 0.0, 0, 0, 0.0};
            for (const auto& pt : pts) {
                double rt = -1.0;
                IterPoint ip{};
                if (truth_anchor) {
                    double ra, dec;
                    truth_radec(fx, pt.first, pt.second, &ra, &dec);
                    ip = invert_point(w, ra, dec);
                    if (ip.converged)
                        rt = std::hypot(ip.x - pt.first, ip.y - pt.second);
                } else {
                    rt = closure_rt(w, pt.first, pt.second, nullptr, nullptr,
                                    &ip);
                }
                if (!ip.converged) {
                    ++st->n_reject;
                    continue;
                }
                st->max_rt = std::max(st->max_rt, rt);
                st->sum_rt += rt;
                ++st->n;
            }
        };
        run_set(center90_pts, &st_c90[f], false);
        run_set(boundary_pts, &st_bnd[f], false);
        run_set(random_pts, &st_rnd[f], false);
        run_set(center90_pts, &st_tru[f], true);

        P1WCS_CHECK(cs, st_c90[f].n == (int)center90_pts.size() &&
                            st_c90[f].max_rt < kFreezePx,
                    "f2x_center90_freeze");
        P1WCS_CHECK(cs, st_bnd[f].n == (int)boundary_pts.size() &&
                            st_bnd[f].max_rt < kFreezePx,
                    "f2x_boundary_freeze");
        P1WCS_CHECK(cs, st_rnd[f].n == kRandPoints &&
                            st_rnd[f].max_rt < kFreezePx,
                    "f2x_random_freeze");
        P1WCS_CHECK(cs, st_tru[f].n == (int)center90_pts.size() &&
                            st_tru[f].max_rt < kFreezePx,
                    "f2x_truth_anchor");

        // ------------------------------------------------------------------
        // 7a. f2x_iter_floor_freeze（G-P1-WCS-RT-ITER）: 迭代反演路径紧门
        //     max(center90 ∪ boundary ∪ random) < κ_iter·τ = 1e-8 px
        //     （该量 = 独立 oracle 前向锚 + 生产迭代反演 ⇒ 纯反演地板;
        //      不含拟合误差, 故可用 τ 量级设门）
        // ------------------------------------------------------------------
        const double iter_worst = std::max(
            std::max(st_c90[f].max_rt, st_bnd[f].max_rt), st_rnd[f].max_rt);
        P1WCS_CHECK(cs, iter_worst > 0.0 && iter_worst < kIterGatePx,
                    "f2x_iter_floor_freeze");
        // ------------------------------------------------------------------
        // 7b. f2x_apbp_represent_freeze（G-P1-WCS-RT-APBP）: APx 一步直加
        //     **表示残差** ≤ 10% × 边缘畸变预算（表示门, 与 τ 门分开登记）
        // ------------------------------------------------------------------
        P1WCS_CHECK(cs, edge_dist_px[f] > 0.0 &&
                            apx_onestep_max[f] < kApbpBudgetFrac * edge_dist_px[f],
                    "f2x_apbp_represent_freeze");
        std::fprintf(stdout,
                     "[GATE-WCS-01] %s: iter_worst=%.4g px (gate=%.3g = %g*tau) "
                     "apx1s=%.4g px budget=%.4g px (%.3f%% of edge_dist) "
                     "frozen_1e-4_margin=%.3g\n",
                     kFixtures[f].name, iter_worst, kIterGatePx, kIterGateKappa,
                     apx_onestep_max[f], kApbpBudgetFrac * edge_dist_px[f],
                     100.0 * apx_onestep_max[f] / edge_dist_px[f],
                     kFreezePx / iter_worst);
    }

    // ------------------------------------------------------------------
    // 7c. 负例非退化（能红能绿）:
    //   ① 真值无效应⇒归零: 零畸变 fixture（dist_scale = 0）⇒ 迭代反演往返与
    //      APx 一步表示残差都塌缩到 FP64 地板（≪ 各自门值）;
    //   ② 判据能红（迭代门）: 前向像素注入 +1e-3 px 后再反演 ⇒ 往返 ≈1e-3 px
    //      ≫ κ_iter·τ ⇒ 同一判据必红;
    //   ③ 判据能红（表示门）: 把 APx/BPx 系数清零（等价缺陷）⇒ 一步残差 ≈ 畸变
    //      量级 ≫ 10% 预算 ⇒ 表示门必红。
    // ------------------------------------------------------------------
    {
        // ① 零畸变 ⇒ 归零
        const FixWcsF fx0 = fix_wcs_f_distortion(kSeedF1, 0.0);
        const SolveOut s0 = solve_f(fx0);
        P1WCS_CHECK(cs, s0.ok && s0.w.success, "f2x_iter_floor_freeze");
        double rt0 = 0.0, apx0 = 0.0;
        {
            const ipv::WcsFitResult& w = s0.w;
            double i00, i01, i10, i11;
            oracle_invert2(w.cd.cd11, w.cd.cd12, w.cd.cd21, w.cd.cd22, &i00, &i01,
                           &i10, &i11);
            for (const auto& pt : center90_pts) {
                double ra, dec;
                oracle_wcs_forward(w.cd.cd11, w.cd.cd12, w.cd.cd21, w.cd.cd22,
                                   w.crval[0], w.crval[1], w.crpix[0], w.crpix[1],
                                   w.sip.A, w.sip.B, w.sip.order, pt.first,
                                   pt.second, &ra, &dec);
                const IterPoint ip = invert_point(w, ra, dec);
                if (ip.converged)
                    rt0 = std::max(rt0, std::hypot(ip.x - pt.first, ip.y - pt.second));
                double xi, eta;
                oracle_gnomonic(ra, dec, w.crval[0], w.crval[1], &xi, &eta);
                const double uvx = i00 * xi + i01 * eta;
                const double uvy = i10 * xi + i11 * eta;
                double cx1, cy1;
                oracle_sip_eval_stride(w.sip.APx, w.sip.BPx, w.sip.apx_order, 10,
                                       uvx, uvy, &cx1, &cy1);
                double xr, yr;
                oracle_wcs_reverse(w.cd.cd11, w.cd.cd12, w.cd.cd21, w.cd.cd22,
                                   w.crval[0], w.crval[1], w.crpix[0], w.crpix[1],
                                   w.sip.A, w.sip.B, w.sip.order, ra, dec, &xr, &yr);
                apx0 = std::max(apx0, std::hypot(uvx + cx1 - (xr - w.crpix[0]),
                                                 uvy + cy1 - (yr - w.crpix[1])));
            }
        }
        P1WCS_CHECK(cs, rt0 > 0.0 && rt0 < kIterGatePx, "f2x_iter_floor_freeze");
        // 表示门「真值无效应⇒归零」: 零畸变 ⇒ APx 表示残差塌缩到 FP64 地板,
        // 且比 high 档（畸变 ~184 px）小 ≥1e6 倍（实测 2.6e-11 vs 3.46 px）
        P1WCS_CHECK(cs, apx0 < 1e-9 && apx0 * 1e6 < apx_onestep_max[2],
                    "f2x_apbp_represent_freeze");
        std::fprintf(stdout,
                     "[GATE-WCS-01] zero-distortion control: iter_rt=%.4g px "
                     "apx1s=%.4g px（表示残差塌缩 %.3g× vs high 档）\n",
                     rt0, apx0, apx_onestep_max[2] / apx0);

        // ①' 迭代门判别性（度量非退化）: 把 τ 收紧 1000×（1e-9 → 1e-12）⇒ 往返
        //     误差显著缩小（实测 1.655e-9 → 3.918e-10, 收缩 4.23×, 断言 ≥2×）
        //     ⇒ 该度量确实测**反演地板**, 不是常数、不是恒真判据。
        //     **如实登记**: 收缩比 ≪ 1000× ⇒ 除 τ 之外还存在 ~4e-10 px 量级的
        //     FP64/迭代结构地板（牛顿残差判据 |F|∞<τ 之外的分量）; 这正是
        //     G-P1-WCS-RT-ITER 取 κ_iter=10 而非 1 的定量理由。
        {
            const ipv::WcsFitResult& w = sol[2].w;   // high 档
            double rt_tight = 0.0;
            for (const auto& pt : center90_pts) {
                double ra, dec;
                oracle_wcs_forward(w.cd.cd11, w.cd.cd12, w.cd.cd21, w.cd.cd22,
                                   w.crval[0], w.crval[1], w.crpix[0], w.crpix[1],
                                   w.sip.A, w.sip.B, w.sip.order, pt.first,
                                   pt.second, &ra, &dec);
                const IterPoint ip = invert_point_tau(w, ra, dec, 1e-12);
                if (ip.converged)
                    rt_tight =
                        std::max(rt_tight, std::hypot(ip.x - pt.first, ip.y - pt.second));
            }
            P1WCS_CHECK(cs, rt_tight > 0.0 && rt_tight * 2.0 < st_c90[2].max_rt,
                        "f2x_iter_floor_freeze");   // τ↓1000× ⇒ 误差至少↓2×
            std::fprintf(stdout,
                         "[GATE-WCS-01] tau-scaling control: tau=1e-9 ⇒ %.4g px, "
                         "tau=1e-12 ⇒ %.4g px（收缩 %.3g×; 度量测地板, 非恒真）\n",
                         st_c90[2].max_rt, rt_tight, st_c90[2].max_rt / rt_tight);
        }

        // ② 迭代门能红: 前向像素注入 +1e-3 px
        {
            const ipv::WcsFitResult& w = sol[2].w;   // high 档
            const double x_f = center90_pts[0].first;
            const double y_f = center90_pts[0].second;
            double ra, dec;
            oracle_wcs_forward(w.cd.cd11, w.cd.cd12, w.cd.cd21, w.cd.cd22,
                               w.crval[0], w.crval[1], w.crpix[0], w.crpix[1],
                               w.sip.A, w.sip.B, w.sip.order, x_f + 1e-3, y_f,
                               &ra, &dec);
            const IterPoint ip = invert_point(w, ra, dec);
            const double rt_bias =
                ip.converged ? std::hypot(ip.x - x_f, ip.y - y_f) : -1.0;
            P1WCS_CHECK(cs, rt_bias > kIterGatePx,
                        "f2x_iter_floor_freeze");   // 注入必红（非退化）
        }

        // ③ 表示门能红: APx/BPx 清零（等价缺陷）
        {
            ipv::WcsFitResult wbad = sol[2].w;   // high 档
            for (int i = 0; i < 100; ++i) {
                wbad.sip.APx[i] = 0.0;
                wbad.sip.BPx[i] = 0.0;
            }
            double i00, i01, i10, i11;
            oracle_invert2(wbad.cd.cd11, wbad.cd.cd12, wbad.cd.cd21, wbad.cd.cd22,
                           &i00, &i01, &i10, &i11);
            double apx_bad = 0.0;
            for (const auto& pt : center90_pts) {
                double ra, dec;
                oracle_wcs_forward(wbad.cd.cd11, wbad.cd.cd12, wbad.cd.cd21,
                                   wbad.cd.cd22, wbad.crval[0], wbad.crval[1],
                                   wbad.crpix[0], wbad.crpix[1], wbad.sip.A,
                                   wbad.sip.B, wbad.sip.order, pt.first, pt.second,
                                   &ra, &dec);
                double xi, eta;
                oracle_gnomonic(ra, dec, wbad.crval[0], wbad.crval[1], &xi, &eta);
                const double uvx = i00 * xi + i01 * eta;
                const double uvy = i10 * xi + i11 * eta;
                double cx1, cy1;
                oracle_sip_eval_stride(wbad.sip.APx, wbad.sip.BPx, wbad.sip.apx_order,
                                       10, uvx, uvy, &cx1, &cy1);
                double xr, yr;
                oracle_wcs_reverse(wbad.cd.cd11, wbad.cd.cd12, wbad.cd.cd21,
                                   wbad.cd.cd22, wbad.crval[0], wbad.crval[1],
                                   wbad.crpix[0], wbad.crpix[1], wbad.sip.A,
                                   wbad.sip.B, wbad.sip.order, ra, dec, &xr, &yr);
                apx_bad = std::max(apx_bad, std::hypot(uvx + cx1 - (xr - wbad.crpix[0]),
                                                       uvy + cy1 - (yr - wbad.crpix[1])));
            }
            P1WCS_CHECK(cs, apx_bad > kApbpBudgetFrac * edge_dist_px[2],
                        "f2x_apbp_represent_freeze");   // 注入必红（非退化）
            std::fprintf(stdout,
                         "[GATE-WCS-01] negative controls: bias+1e-3px ⇒ 迭代门必红; "
                         "APx 清零 ⇒ 表示门必红 (apx_bad=%.4g px > %.4g px)\n",
                         apx_bad, kApbpBudgetFrac * edge_dist_px[2]);
        }
    }

    // ------------------------------------------------------------------
    // 5. f2x_reject_deterministic: 确定性拒绝/收敛失败 (零 UB)
    //    a) 投影背面 (角距 90°, cosc≈0) → reject 1
    //    b) NaN 输入 → reject 1
    //    c) max_iter=0 → reject 2 (超限语义, 确定性)
    //    d) 畸变场 J 奇异域 (构造 A_20 使 det(J)=0 落入解域) → 拒绝
    //    每路径两次调用逐字段 bitwise 一致
    // ------------------------------------------------------------------
    {
        const ipv::WcsFitResult& w = sol[2].w;  // high 档
        // a) 背面: dec = dec0 − 90° (沿经线角距恰 90°)
        {
            const double dec_back = w.crval[1] - 90.0;
            const IterPoint p1 = invert_point(w, w.crval[0], dec_back);
            const IterPoint p2 = invert_point(w, w.crval[0], dec_back);
            P1WCS_CHECK(cs, !p1.converged && p1.reject_code == 1,
                        "f2x_reject_deterministic");
            P1WCS_CHECK(cs, iter_point_bitwise(p1, p2),
                        "f2x_reject_deterministic");
        }
        // b) NaN 输入
        {
            const double nan_v = std::numeric_limits<double>::quiet_NaN();
            const IterPoint p1 = invert_point(w, nan_v, 45.0);
            const IterPoint p2 = invert_point(w, nan_v, 45.0);
            P1WCS_CHECK(cs, !p1.converged && p1.reject_code == 1,
                        "f2x_reject_deterministic");
            P1WCS_CHECK(cs, iter_point_bitwise(p1, p2),
                        "f2x_reject_deterministic");
        }
        // c) max_iter=0 (未执行即超限, 确定性)
        {
            const ipv::WcsIterativeResult r1 =
                ipv::wcs_sky_to_pixel_iterative(w, 210.001, 45.001, 1e-9, 0);
            const ipv::WcsIterativeResult r2 =
                ipv::wcs_sky_to_pixel_iterative(w, 210.001, 45.001, 1e-9, 0);
            P1WCS_CHECK(cs, !r1.converged && r1.reject_code == 2 &&
                                r1.iterations == 0,
                        "f2x_reject_deterministic");
            P1WCS_CHECK(cs, r1.reject_code == r2.reject_code &&
                                r1.iterations == r2.iterations,
                        "f2x_reject_deterministic");
        }
        // d) J 奇异域: 手工 WcsFitResult (cd=I, A_20 = −1/1024 →
        //    det(J) = 1 + 2·A_20·u 在 u=512 处为零)。UV=(512,0) 度,
        //    初值 u≈512 → |det|≈0 → reject 3, 或牛顿步发散 → reject 2。
        //    断言: 明确拒绝/收敛失败 + 两次 bitwise 一致 (确定性)。
        {
            ipv::WcsFitResult ws{};
            ws.cd.cd11 = 1.0; ws.cd.cd22 = 1.0;
            ws.crval[0] = 210.0; ws.crval[1] = 45.0;
            ws.crpix[0] = 513.0; ws.crpix[1] = 513.0;
            ws.sip.order = 2;
            ws.sip.A[12] = -1.0 / 1024.0;  // A_20
            double ra, dec;
            oracle_gnomonic_inv(512.0, 0.0, 210.0, 45.0, &ra, &dec);
            const IterPoint p1 = invert_point(ws, ra, dec);
            const IterPoint p2 = invert_point(ws, ra, dec);
            P1WCS_CHECK(cs, !p1.converged &&
                                (p1.reject_code == 2 || p1.reject_code == 3),
                        "f2x_reject_deterministic");
            P1WCS_CHECK(cs, iter_point_bitwise(p1, p2),
                        "f2x_reject_deterministic");
        }
    }

    // ------------------------------------------------------------------
    // 6. f2x_parity_workers: 1 worker / N worker 一致性
    //    反演逐点纯函数 (无共享态), 采样编排 omp parallel 1 vs 4 线程,
    //    结果向量逐字段 bitwise 一致; 同线程重复 3 次 bitwise 一致。
    // ------------------------------------------------------------------
    {
        const ipv::WcsFitResult& w = sol[2].w;
        const int n = (int)random_pts.size();
        std::vector<IterPoint> r1(n), r4(n), r3[3];
        auto fill = [&](std::vector<IterPoint>* out) {
            std::vector<double> ra(n), dec(n);
            for (int i = 0; i < n; ++i) {
                double r, d;
                oracle_wcs_forward(w.cd.cd11, w.cd.cd12, w.cd.cd21, w.cd.cd22,
                                   w.crval[0], w.crval[1], w.crpix[0],
                                   w.crpix[1], w.sip.A, w.sip.B, w.sip.order,
                                   random_pts[i].first, random_pts[i].second,
                                   &r, &d);
                ra[i] = r;
                dec[i] = d;
            }
#pragma omp parallel for schedule(static)
            for (int i = 0; i < n; ++i) {
                (*out)[i] = invert_point(w, ra[i], dec[i]);
            }
        };
        omp_set_num_threads(1);
        fill(&r1);
        for (int k = 0; k < 3; ++k) {
            r3[k].resize(n);
            fill(&r3[k]);
        }
        omp_set_num_threads(4);
        fill(&r4);
        omp_set_num_threads(1);
        bool same14 = true, same_rep = true;
        for (int i = 0; i < n; ++i) {
            if (!iter_point_bitwise(r1[i], r4[i])) same14 = false;
            if (!iter_point_bitwise(r1[i], r3[0][i]) ||
                !iter_point_bitwise(r1[i], r3[1][i]) ||
                !iter_point_bitwise(r1[i], r3[2][i]))
                same_rep = false;
        }
        P1WCS_CHECK(cs, same14, "f2x_parity_workers");
        P1WCS_CHECK(cs, same_rep, "f2x_parity_workers");
        P1WCS_CHECK(cs, r4[0].converged, "f2x_parity_workers");  // 有效样本
    }

    // ------------------------------------------------------------------
    // 数值证据 + Astropy 交叉验证输入导出 (run/p1wcs_wcs003/)
    // ------------------------------------------------------------------
    if (p1wcs::FaultRegistry::instance().injected("f2x_cross_export")) {
        cs.note_injected("f2x_cross_export");
        ++cs.failures;
    }
    {
        const char* out_path = std::getenv("P1WCS_CROSS_OUT");
        const std::string path = (out_path && out_path[0])
                                     ? std::string(out_path)
                                     : std::string("run/p1wcs_wcs003/"
                                                   "wcs003_cross_input.json");
        FILE* fp = std::fopen(path.c_str(), "w");
        if (fp == nullptr) {
            // 目录不存在则创建后重试一次 (run/ 运行区)
            std::error_code ec;
            std::filesystem::create_directories(
                std::filesystem::path(path).parent_path(), ec);
            fp = std::fopen(path.c_str(), "w");
        }
        if (fp == nullptr) {
            P1WCS_CHECK(cs, false, "f2x_cross_export");
        } else {
            std::fprintf(fp,
                         "{\"schema\":\"p1wcs/wcs003-cross-input-v1\","
                         "\"seed_random\":%u,\"n_random\":%d,"
                         "\"fixtures\":[",
                         kSeedRand, kRandPoints);
            for (int f = 0; f < 3; ++f) {
                const ipv::WcsFitResult& w = sol[f].w;
                if (f > 0) std::fprintf(fp, ",");
                std::fprintf(fp,
                             "{\"name\":\"%s\",\"dist_scale\":%.17g,"
                             "\"edge_dist_px\":%.17g,"
                             "\"crpix\":[%.17g,%.17g],"
                             "\"crval\":[%.17g,%.17g],"
                             "\"cd\":[[%.17g,%.17g],[%.17g,%.17g]],"
                             "\"sip_order\":%d,\"apx_order\":%d,"
                             "\"A\":[",
                             kFixtures[f].name, kFixtures[f].scale,
                             edge_dist_px[f], w.crpix[0], w.crpix[1],
                             w.crval[0], w.crval[1], w.cd.cd11, w.cd.cd12,
                             w.cd.cd21, w.cd.cd22, w.sip.order,
                             w.sip.apx_order);
                for (int i = 0; i < 36; ++i)
                    std::fprintf(fp, "%s%.17g", i ? "," : "", w.sip.A[i]);
                std::fprintf(fp, "],\"B\":[");
                for (int i = 0; i < 36; ++i)
                    std::fprintf(fp, "%s%.17g", i ? "," : "", w.sip.B[i]);
                std::fprintf(fp, "],\"APx\":[");
                for (int i = 0; i < 100; ++i)
                    std::fprintf(fp, "%s%.17g", i ? "," : "", w.sip.APx[i]);
                std::fprintf(fp, "],\"BPx\":[");
                for (int i = 0; i < 100; ++i)
                    std::fprintf(fp, "%s%.17g", i ? "," : "", w.sip.BPx[i]);
                std::fprintf(fp, "],\"points\":[");
                for (int i = 0; i < (int)random_pts.size(); ++i) {
                    double ra, dec;
                    IterPoint ip;
                    const double rt = closure_rt(w, random_pts[i].first,
                                                 random_pts[i].second, &ra,
                                                 &dec, &ip);
                    std::fprintf(fp,
                                 "%s{\"x_f\":%.17g,\"y_f\":%.17g,"
                                 "\"ra_fwd\":%.17g,\"dec_fwd\":%.17g,"
                                 "\"x_iter\":%.17g,\"y_iter\":%.17g,"
                                 "\"rt_px\":%.17g,\"converged\":%d}",
                                 i ? "," : "", random_pts[i].first,
                                 random_pts[i].second, ra, dec, ip.x, ip.y,
                                 rt, ip.converged ? 1 : 0);
                }
                std::fprintf(fp, "]}");
            }
            std::fprintf(fp,
                         "],\"onestep_apx\":{\"low\":%.17g,\"mid\":%.17g,"
                         "\"high\":%.17g},\"onestep_ap36\":{\"low\":%.17g,"
                         "\"mid\":%.17g,\"high\":%.17g},"
                         "\"center90_max\":{\"low\":%.17g,\"mid\":%.17g,"
                         "\"high\":%.17g},"
                         "\"boundary_max\":{\"low\":%.17g,\"mid\":%.17g,"
                         "\"high\":%.17g},"
                         "\"random_max\":{\"low\":%.17g,\"mid\":%.17g,"
                         "\"high\":%.17g},"
                         "\"truth_anchor_max\":{\"low\":%.17g,\"mid\":%.17g,"
                         "\"high\":%.17g}}",
                         apx_onestep_max[0], apx_onestep_max[1],
                         apx_onestep_max[2], ap36_onestep_max[0],
                         ap36_onestep_max[1], ap36_onestep_max[2],
                         st_c90[0].max_rt, st_c90[1].max_rt, st_c90[2].max_rt,
                         st_bnd[0].max_rt, st_bnd[1].max_rt, st_bnd[2].max_rt,
                         st_rnd[0].max_rt, st_rnd[1].max_rt, st_rnd[2].max_rt,
                         st_tru[0].max_rt, st_tru[1].max_rt, st_tru[2].max_rt);
            std::fclose(fp);
            // 数值证据摘要 (stdout, 证据表来源)
            for (int f = 0; f < 3; ++f) {
                std::fprintf(stdout,
                             "[WCS-003] %s: edge_dist=%.2fpx "
                             "c90_max=%.3e bnd_max=%.3e rnd_max=%.3e "
                             "truth_max=%.3e apx1s=%.3e ap36_1s=%.3e\n",
                             kFixtures[f].name, edge_dist_px[f],
                             st_c90[f].max_rt, st_bnd[f].max_rt,
                             st_rnd[f].max_rt, st_tru[f].max_rt,
                             apx_onestep_max[f], ap36_onestep_max[f]);
            }
        }
    }

    if (cs.failures == 0) {
        std::fprintf(stdout, "P1WCS APBP PASS\n");
        return 0;
    }
    std::fprintf(stderr, "P1WCS APBP FAIL (%d check(s))\n", cs.failures);
    return 1;
}

}  // namespace p1wcs
