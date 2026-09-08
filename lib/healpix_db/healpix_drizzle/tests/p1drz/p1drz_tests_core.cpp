// P1-DRZ-TEST · core 组实现 (units/properties/oracle/negative)
//
// 合同锚: DRIZZLE_GEOMETRY.md §9 TEST-DRZ-DESIGN-001 冻结容差;
//         DRIZZLE.md §5 公式/§7 不变量/§8 极端条件表。
// 覆盖: FIX-DRZ-A..F 全 fixture; 冻结容差 FP64<1e-6 (主域), FP32/FP64
//       逐 leaf <1e-5, α² worst_rel <1e-4, 1/2/4 线程一致; 负面矩阵按 §8 表。
// 域基线: HEAD 09ee5363 (NaN/Inf 传播已入库); 与 drizzle_nonfinite_test
//       互补 (其负面合同, 本面 FIX-DRZ-E 正面传播断言)。
#include "drizzle_engine.h"

#include "p1drz_fixtures.hpp"
#include "p1drz_oracle.hpp"
#include "p1drz_test_main.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

using namespace p1drz;
using drizzle::DrizzleConfig;
using drizzle::DrizzleStats;
using drizzle::DrizzleEngine;
using drizzle::FitsImage;
using drizzle::TileAccumulatorT;

namespace {

constexpr int W = 16, H = 16;
constexpr int NSIDE = 512;
constexpr double SCALE = 300.0;
constexpr double B0 = 1000.0;

CheckState g_cs;

std::string run_drizzle_f64(const FitsImage& img, const DrizzleConfig& cfg,
                            const std::vector<float>* variance,
                            std::vector<TileAccumulatorT<double>>& tiles,
                            DrizzleStats& stats) {
    DrizzleEngine eng;
    std::string err;
    const float* vp = variance ? variance->data() : nullptr;
    const bool ok = eng.drizzleTiled_f64(img, cfg, nullptr, nullptr, vp,
                                         tiles, stats, err);
    return ok ? std::string() : ("drizzleTiled_f64: " + err);
}

std::string run_drizzle_f32(const FitsImage& img, const DrizzleConfig& cfg,
                            std::vector<TileAccumulatorT<float>>& tiles,
                            DrizzleStats& stats) {
    DrizzleEngine eng;
    std::string err;
    const bool ok = eng.drizzleTiled(img, cfg, nullptr, nullptr, nullptr,
                                     tiles, stats, err);
    return ok ? std::string() : ("drizzleTiled: " + err);
}

// ===========================================================================
// units: FIX-DRZ-A 双语义 + FIX-DRZ-D 脉冲 + FP64 通量闭合 (冻结 1e-6 主域)
// ===========================================================================
int group_units() {
    const DrizzleConfig cfg = make_cfg(NSIDE, 1.0, 1, true);

    // -- FIX-DRZ-A 常量面亮度: S_p = B0 (SCI-003 主语义恒等) --
    {
        FitsImage img = fix_drz_a_const_sb(W, H, B0, SCALE, false);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        const std::string err = run_drizzle_f64(img, cfg, nullptr, tiles, st);
        P1DRZ_CHECK_MSG(g_cs, err.empty(), "flux_closure", "units: A-const-sb run: %s",
                        err.c_str());
        if (err.empty()) {
            const std::vector<LeafRec> leafs = extract_leafs(tiles, 9);
            P1DRZ_CHECK_MSG(g_cs, !leafs.empty(), "flux_closure",
                            "units: A-const-sb touched=%zu", leafs.size());
            const UniformityOracle u = oracle_const_sb(leafs, B0);
            P1DRZ_CHECK_MSG(g_cs, u.ok_mean, "uniformity",
                            "units: A-const-sb S_p=B0: max|S/B0-1|=%.3e (<1e-3, oracle 口径预算)",
                            u.max_abs_dev);
            P1DRZ_CHECK_MSG(g_cs, u.ok_uniform, "uniformity",
                            "units: A-const-sb uniformity rel_std=%.3e (<1e-4, DOC §9)",
                            u.rel_std);
        }
    }

    // -- FIX-DRZ-A 对照: 每像素常量 ADU → S_p = C/A_drop ≠ C (SCI-003 反向) --
    {
        const double C = 500.0;
        FitsImage img = fix_drz_a_const_adu(W, H, C, SCALE);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        const std::string err = run_drizzle_f64(img, cfg, nullptr, tiles, st);
        P1DRZ_CHECK_MSG(g_cs, err.empty(), "adu_inverse", "units: A-const-adu run: %s",
                        err.c_str());
        if (err.empty()) {
            const std::vector<LeafRec> leafs = extract_leafs(tiles, 9);
            const bool inv = oracle_const_adu_inverse(leafs, img.wcs, C,
                                                      SCALE, 1.0);
            P1DRZ_CHECK_MSG(g_cs, inv, "adu_inverse",
                            "units: A-const-adu S_p·A_drop=C 闭合 + S_p≠C (SCI-003 反向语义)");
        }
    }

    // -- FIX-DRZ-B 点源高斯: FP64 通量闭合 <1e-6 (冻结主域门, DOC §9) --
    {
        FitsImage img = fix_drz_b_gaussian(W, H, 5000.0, 3.0, 20260907ULL, 0.0);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        const std::string err = run_drizzle_f64(img, cfg, nullptr, tiles, st);
        P1DRZ_CHECK_MSG(g_cs, err.empty(), "flux_closure", "units: B-gauss run: %s",
                        err.c_str());
        if (err.empty()) {
            const std::vector<LeafRec> leafs = extract_leafs(tiles, 9);
            const ClosureOracle o = oracle_flux_closure(img, leafs);
            P1DRZ_CHECK_MSG(g_cs, std::fabs(o.ratio_flux - 1.0) < 1e-6,
                            "flux_closure",
                            "units: B-gauss FP64 closure ratio=%.12f (<1e-6, DOC §9 冻结)",
                            o.ratio_flux);
        }
    }

    // -- FIX-DRZ-D 脉冲: S_p = amp/A_drop (oracle Van Oosterom 期望) --
    {
        const double amp = 40000.0;
        FitsImage img = fix_drz_d_impulse(W, H, W / 2, H / 2, amp);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        const std::string err = run_drizzle_f64(img, cfg, nullptr, tiles, st);
        P1DRZ_CHECK_MSG(g_cs, err.empty(), "impulse", "units: D-impulse run: %s",
                        err.c_str());
        if (err.empty()) {
            const std::vector<LeafRec> leafs = extract_leafs(tiles, 9);
            P1DRZ_CHECK_MSG(g_cs, !leafs.empty(), "impulse",
                            "units: D-impulse touched=%zu", leafs.size());
            const ImpulseOracle io = oracle_impulse(leafs, amp, img.wcs, W, H,
                                                    cfg.pixfrac);
            P1DRZ_CHECK_MSG(g_cs, io.ok, "impulse",
                            "units: D-impulse 闭合: ΣF/amp=%.9f (<1e-6), ΣD/ΣA_drop=%.6f (<5e-3)",
                            io.ratio_flux, io.ratio_area);
        }
    }

    // -- FP32/FP64 逐 leaf 一致 <1e-5 (冻结, DOC §9 freeze 口径) --
    {
        // 常数场 (值域 ~2, 无暗尾翼): float 输入量化差 6e-8 ≪ 门, 暴露的
        // 是引擎 f32/f64 累加与面积路径差本身
        FitsImage img = fix_drz_a_const_sb(W, H, B0, SCALE, false);
        std::vector<TileAccumulatorT<double>> tiles64;
        DrizzleStats st64;
        const std::string e64 = run_drizzle_f64(img, cfg, nullptr, tiles64, st64);
        std::vector<TileAccumulatorT<float>> tiles32;
        DrizzleStats st32;
        const std::string e32 = run_drizzle_f32(img, cfg, tiles32, st32);
        P1DRZ_CHECK_MSG(g_cs, e64.empty() && e32.empty(), "flux_closure",
                        "units: f32/f64 runs: %s|%s", e64.c_str(), e32.c_str());
        if (e64.empty() && e32.empty()) {
            const std::vector<LeafRec> l64 = extract_leafs(tiles64, 9);
            const std::vector<LeafRec> l32 = extract_leafs(tiles32, 9);
            bool sizes_eq = l32.size() == l64.size();
            P1DRZ_CHECK_MSG(g_cs, sizes_eq, "flux_closure",
                            "units: f32/f64 touched size %zu vs %zu",
                            l64.size(), l32.size());
            if (sizes_eq) {
                // freeze_test 同款口径: |f32-f64|/max(|f64|,1) (float 输入
                // 表示量化使暗尾翼 leaf 的纯相对差放大, freeze :124 口径)
                double worst = 0.0;
                for (std::size_t i = 0; i < l64.size(); ++i) {
                    const double d = std::fabs((double)l32[i].sumFlux - l64[i].sumFlux) /
                                     std::max(std::fabs(l64[i].sumFlux), 1.0);
                    worst = std::max(worst, d);
                }
                P1DRZ_CHECK_MSG(g_cs, worst < 1e-5, "flux_closure",
                                "units: f32/f64 逐 leaf worst=%.3e (<1e-5, DOC §9 freeze 口径)",
                                worst);
            }
        }
    }
    return g_cs.failures == 0 ? 0 : 1;
}

// ===========================================================================
// properties: 1/2/4 线程一致 + 同线程重复 bitwise + α² 缩放律 (冻结 1e-4)
// ===========================================================================
int group_properties() {
    FitsImage img = fix_drz_b_gaussian(W, H, 5000.0, 3.0, 20260908ULL, 0.0);
    const int thread_set[] = {1, 2, 4};
    std::vector<std::vector<LeafRec>> runs;
    for (int t : thread_set) {
        const DrizzleConfig cfg = make_cfg(NSIDE, 1.0, t, true);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        const std::string err = run_drizzle_f64(img, cfg, nullptr, tiles, st);
        P1DRZ_CHECK_MSG(g_cs, err.empty(), "determinism", "properties: %d-thread run: %s",
                        t, err.c_str());
        if (err.empty()) runs.push_back(extract_leafs(tiles, 9));
    }
    if (runs.size() == 3) {
        // 整数域: touched 集合与 nContrib 恒等 (线程调度无关)
        P1DRZ_CHECK_MSG(g_cs,
                        leafmap_equal_int(runs[0], runs[1], true) &&
                            leafmap_equal_int(runs[0], runs[2], true),
                        "determinism",
                        "properties: 1/2/4 线程 touched+nContrib 整数域恒等");
        // 数值域: 归并顺序偏差在浮点噪声带 (<1e-12 相对)
        const double w12 = leafmap_worst_rel(runs[0], runs[1]);
        const double w13 = leafmap_worst_rel(runs[0], runs[2]);
        P1DRZ_CHECK_MSG(g_cs, w12 < 1e-12 && w13 < 1e-12, "determinism",
                        "properties: 1/2/4 线程 sumFlux worst_rel=%.3e/%.3e (<1e-12)",
                        w12, w13);
        // bitwise: 同线程数重复跑必须 bitwise 一致 (确定性铁律)
        const DrizzleConfig cfg1 = make_cfg(NSIDE, 1.0, 1, true);
        std::vector<TileAccumulatorT<double>> tiles_r;
        DrizzleStats st_r;
        const std::string err_r = run_drizzle_f64(img, cfg1, nullptr, tiles_r, st_r);
        if (err_r.empty()) {
            const std::vector<LeafRec> r1 = extract_leafs(tiles_r, 9);
            bool bitwise = r1.size() == runs[0].size();
            for (std::size_t i = 0; bitwise && i < r1.size(); ++i)
                bitwise = std::memcmp(&r1[i].sumFlux, &runs[0][i].sumFlux,
                                      sizeof(double)) == 0 &&
                          r1[i].ipix == runs[0][i].ipix &&
                          r1[i].nContrib == runs[0][i].nContrib;
            P1DRZ_CHECK_MSG(g_cs, bitwise, "determinism",
                            "properties: 同线程重复跑 bitwise 一致");
        }
    }

    // -- α² 缩放律 (SCI-DRZ-014, 冻结 worst_rel <1e-4) --
    {
        const double alpha = 3.0;
        const DrizzleConfig cfg = make_cfg(NSIDE, 1.0, 1, true);
        const double sigma2 = 25.0;
        std::vector<float> var1((std::size_t)W * H, (float)sigma2);
        std::vector<float> var2((std::size_t)W * H, (float)(sigma2 * alpha * alpha));

        FitsImage base = fix_drz_b_gaussian(W, H, 5000.0, 3.0, 20260909ULL, 0.0);
        FitsImage scaled = base;  // x→αx
        for (auto& v : scaled.pixels_f64) v *= alpha;
        for (auto& v : scaled.pixels) v = (float)((double)v * alpha);

        std::vector<TileAccumulatorT<double>> t1, t2;
        DrizzleStats s1, s2;
        const std::string e1 = run_drizzle_f64(base, cfg, &var1, t1, s1);
        const std::string e2 = run_drizzle_f64(scaled, cfg, &var2, t2, s2);
        P1DRZ_CHECK_MSG(g_cs, e1.empty() && e2.empty(), "variance",
                        "properties: variance runs: %s|%s", e1.c_str(), e2.c_str());
        if (e1.empty() && e2.empty()) {
            const std::vector<LeafRec> l1 = extract_leafs(t1, 9);
            const std::vector<LeafRec> l2 = extract_leafs(t2, 9);
            if (l1.size() == l2.size()) {
                double worst = 0.0;
                for (std::size_t i = 0; i < l1.size(); ++i) {
                    const double expect = l1[i].variance * alpha * alpha;
                    const double denom = std::max(std::fabs(expect), 1e-300);
                    worst = std::max(worst,
                                     std::fabs(l2[i].variance - expect) / denom);
                }
                P1DRZ_CHECK_MSG(g_cs, worst < 1e-4, "variance",
                                "properties: α² 缩放律 worst_rel=%.3e (<1e-4, SCI-DRZ-014 冻结)",
                                worst);
            } else {
                P1DRZ_CHECK(g_cs, false, "variance");
            }
            // 值域: signal 也随 α 缩放 (x→αx ⇒ F→αF)
            bool sig_ok = l1.size() == l2.size();
            double sworst = 0.0;
            for (std::size_t i = 0; sig_ok && i < l1.size(); ++i) {
                const double expect = l1[i].sumFlux * alpha;
                sworst = std::max(sworst, std::fabs(l2[i].sumFlux - expect) /
                                              std::max(std::fabs(expect), 1e-300));
            }
            P1DRZ_CHECK_MSG(g_cs, sig_ok && sworst < 1e-12, "flux_closure",
                            "properties: x→αx ⇒ F→αF worst_rel=%.3e", sworst);
        }
    }
    return g_cs.failures == 0 ? 0 : 1;
}

// ===========================================================================
// oracle: 独立方差闭合 + NaN/Inf 污染 leaf 集合 (正面传播) + SIP 激活
// ===========================================================================
int group_oracle() {
    // -- 方差独立闭合: var_p = σ²/A_drop² (中心 leaf, oracle 几何重算) --
    {
        const DrizzleConfig cfg = make_cfg(NSIDE, 1.0, 1, true);
        const double sigma2 = 25.0;
        FitsImage img = fix_drz_a_const_sb(W, H, B0, SCALE, false);
        std::vector<float> var((std::size_t)W * H, (float)sigma2);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        const std::string err = run_drizzle_f64(img, cfg, &var, tiles, st);
        P1DRZ_CHECK_MSG(g_cs, err.empty(), "variance", "oracle: var run: %s",
                        err.c_str());
        if (err.empty()) {
            const std::vector<LeafRec> leafs = extract_leafs(tiles, 9);
            const VarOracle vo = oracle_variance_const(leafs, img.wcs, cfg,
                                                       sigma2, img);
            P1DRZ_CHECK_MSG(g_cs, vo.leaves_hit, "variance",
                            "oracle: 零漏选 (超采样 hit ⊆ touched, DOC §9)");
            P1DRZ_CHECK_MSG(g_cs, vo.variance_closed, "variance",
                            "oracle: var=σ²/A_drop² 闭合 n=%d worst_rel=%.3e (<5e-3)",
                            vo.n_checked, vo.worst_rel);
        }
    }

    // -- FIX-DRZ-E NaN/Inf 污染 leaf 集合 (DRIZZLE.md:96 正面传播) --
    {
        const DrizzleConfig cfg = make_cfg(NSIDE, 1.0, 1, true);
        const FixDrzE fx = fix_drz_e_nonfinite(W, H, B0, SCALE);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        const std::string err = run_drizzle_f64(fx.im, cfg, nullptr, tiles, st);
        P1DRZ_CHECK_MSG(g_cs, err.empty(), "nonfinite", "oracle: E run: %s",
                        err.c_str());
        if (err.empty()) {
            const std::vector<LeafRec> leafs = extract_leafs(tiles, 9);
            std::vector<std::pair<double, double>> injected = {
                {(double)(W / 2), (double)(H / 2)},
                {(double)(W / 2 + 1), (double)(H / 2)},
                {(double)(W / 2), (double)(H / 2 + 1)}};
            const NonFiniteOracle no = oracle_nonfinite_pollution(
                leafs, fx.im.wcs, cfg, injected);
            P1DRZ_CHECK_MSG(g_cs, no.ok, "nonfinite",
                            "oracle: E 污染 leaf 集合 oracle 一致 (expect=%zu got=%zu total=%zu)",
                            no.expect_polluted.size(), no.got_polluted.size(),
                            leafs.size());
            // 无注入贡献的 leaf 必须保持有限 (污染有界, 不扩全场)
            P1DRZ_CHECK_MSG(g_cs, no.got_polluted.size() < leafs.size(),
                            "nonfinite",
                            "oracle: E 传播有界 (polluted=%zu < total=%zu)",
                            no.got_polluted.size(), leafs.size());
        }
    }

    // -- FIX-DRZ-F SIP 激活 (路径生效: on/off touched 集合差异) --
    {
        FitsImage img_sip = fix_drz_f_sip_patch(W, H, B0);
        FitsImage img_plain = fix_drz_a_const_sb(W, H, B0, SCALE, false);
        const DrizzleConfig cfg = make_cfg(NSIDE, 1.0, 1, true);
        std::vector<TileAccumulatorT<double>> t_sip, t_plain;
        DrizzleStats s_sip, s_plain;
        const std::string e1 = run_drizzle_f64(img_sip, cfg, nullptr, t_sip, s_sip);
        const std::string e2 = run_drizzle_f64(img_plain, cfg, nullptr, t_plain, s_plain);
        P1DRZ_CHECK_MSG(g_cs, e1.empty() && e2.empty(), "sip_active",
                        "oracle: F runs: %s|%s", e1.c_str(), e2.c_str());
        if (e1.empty() && e2.empty()) {
            const std::vector<LeafRec> l_sip = extract_leafs(t_sip, 9);
            const std::vector<LeafRec> l_plain = extract_leafs(t_plain, 9);
            P1DRZ_CHECK_MSG(g_cs, oracle_sip_active(l_sip, l_plain), "sip_active",
                            "oracle: F SIP on/off touched 集合差异 (畸变生效)");
        }
    }
    return g_cs.failures == 0 ? 0 : 1;
}

// ===========================================================================
// negative: DRIZZLE.md §8 表逐行 (值域负面合同主所有权在
// drizzle_nonfinite_test, 此处为矩阵级回归锁定)
// ===========================================================================
int group_negative() {
    DrizzleEngine eng;
    std::string err;

    // -- pixfrac = 0 / 负 / >1 拒绝 (§8 行 1) --
    for (double pf : {0.0, -0.5, 1.5}) {
        FitsImage img = fix_drz_a_const_sb(W, H, B0, SCALE, false);
        DrizzleConfig cfg = make_cfg(NSIDE, pf, 1, true);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        err.clear();
        const bool ok = eng.drizzleTiled_f64(img, cfg, nullptr, nullptr, nullptr,
                                             tiles, st, err);
        P1DRZ_CHECK_MSG(g_cs, !ok, "negative_matrix",
                        "negative: pixfrac=%.1f 拒绝 (§8)", pf);
        P1DRZ_CHECK_MSG(g_cs, !err.empty(), "negative_matrix",
                        "negative: pixfrac=%.1f 错误消息非空", pf);
    }

    // -- RING ordering 拒绝 (§8 行 2, HISS 统一 NESTED) --
    {
        FitsImage img = fix_drz_a_const_sb(W, H, B0, SCALE, false);
        DrizzleConfig cfg = make_cfg(NSIDE, 1.0, 1, true);
        cfg.nested = false;
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        err.clear();
        const bool ok = eng.drizzleTiled_f64(img, cfg, nullptr, nullptr, nullptr,
                                             tiles, st, err);
        P1DRZ_CHECK_MSG(g_cs, !ok, "negative_matrix", "negative: RING 拒绝 (§8)");
        P1DRZ_CHECK_MSG(g_cs, !err.empty(), "negative_matrix",
                        "negative: RING 错误消息非空");
    }

    // -- 多通道拒绝 (§8 行 3) --
    {
        FitsImage img = fix_drz_a_const_sb(W, H, B0, SCALE, false);
        img.channels = 3;
        DrizzleConfig cfg = make_cfg(NSIDE, 1.0, 1, true);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        err.clear();
        const bool ok = eng.drizzleTiled_f64(img, cfg, nullptr, nullptr, nullptr,
                                             tiles, st, err);
        P1DRZ_CHECK_MSG(g_cs, !ok, "negative_matrix", "negative: 多通道拒绝 (§8)");
        P1DRZ_CHECK_MSG(g_cs, !err.empty(), "negative_matrix",
                        "negative: 多通道错误消息非空");
    }

    // -- 缺 WCS / 无效尺度: compute_auto_nside 失败 (§8 行 4) --
    {
        FitsImage img = fix_drz_a_const_sb(W, H, B0, SCALE, false);
        img.wcs.has_wcs = false;
        const int n = drizzle::compute_auto_nside(img.wcs, img.width, img.height);
        P1DRZ_CHECK_MSG(g_cs, n == 0, "negative_matrix",
                        "negative: 缺 WCS compute_auto_nside=0 (§8)");
    }

    // -- NaN 面输入: 引擎层不崩溃、返回成功、无伪输出 leaf (值传播,
    //    下游 INVALID_INPUT 合同; §8 行 5 的矩阵级回归) --
    {
        FitsImage img = fix_drz_f_buffer(W, H, std::numeric_limits<float>::quiet_NaN());
        DrizzleConfig cfg = make_cfg(NSIDE, 1.0, 1, true);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        err.clear();
        const bool ok = eng.drizzleTiled_f64(img, cfg, nullptr, nullptr, nullptr,
                                             tiles, st, err);
        P1DRZ_CHECK_MSG(g_cs, ok, "negative_matrix",
                        "negative: NaN 面引擎层成功 (传播合同, §8 行 5)");
        std::size_t finite_leafs = 0;
        for (const auto& tile : tiles)
            for (uint32_t local : tile.touched) {
                if (local < tile.pixels.size() &&
                    std::isfinite((double)tile.pixels[local].sumFlux))
                    ++finite_leafs;
            }
        P1DRZ_CHECK_MSG(g_cs, finite_leafs == 0, "negative_matrix",
                        "negative: NaN 面无有限伪输出 (finite=%zu)", finite_leafs);
    }

    // -- 空图 (零尺寸) 拒绝 --
    {
        FitsImage img = fix_drz_a_const_sb(W, H, B0, SCALE, false);
        img.width = 0;
        img.height = 0;
        img.pixels.clear();
        img.pixels_f64.clear();
        DrizzleConfig cfg = make_cfg(NSIDE, 1.0, 1, true);
        std::vector<TileAccumulatorT<double>> tiles;
        DrizzleStats st;
        err.clear();
        const bool ok = eng.drizzleTiled_f64(img, cfg, nullptr, nullptr, nullptr,
                                             tiles, st, err);
        P1DRZ_CHECK_MSG(g_cs, !ok, "negative_matrix", "negative: 零尺寸图拒绝");
        P1DRZ_CHECK_MSG(g_cs, !err.empty(), "negative_matrix",
                        "negative: 零尺寸错误消息非空");
    }

    // -- 非法 nside (非 2 幂): 引擎 healpix 层以 std::invalid_argument 拒绝;
    //    异常从 OpenMP region 逃逸被 libgomp terminate → 进程 abort (现状
    //    行为, catch 不可达)。锁定方式 = fork 子进程, 断言"非正常完成"
    //    (异常捕获/返回 false/abort 三者皆算拒绝, 正常 rc=0 才算漏) --
    {
        FitsImage img = fix_drz_a_const_sb(W, H, B0, SCALE, false);
        const pid_t pid = fork();
        P1DRZ_CHECK_MSG(g_cs, pid >= 0, "negative_matrix", "negative: fork 失败");
        if (pid == 0) {
            // 子进程: 跑非法 nside, 任何受控拒绝 → _exit(42); abort → 信号
            DrizzleConfig cfg = make_cfg(500, 1.0, 1, true);
            std::vector<TileAccumulatorT<double>> tiles;
            DrizzleStats st;
            std::string err2;
            try {
                DrizzleEngine eng2;
                const bool ok = eng2.drizzleTiled_f64(img, cfg, nullptr, nullptr,
                                                      nullptr, tiles, st, err2);
                _exit(ok ? 0 : 42);
            } catch (...) {
                _exit(42);
            }
        }
        if (pid > 0) {
            int status = 0;
            waitpid(pid, &status, 0);
            const bool rejected =
                WIFEXITED(status) ? WEXITSTATUS(status) != 0 : true;
            P1DRZ_CHECK_MSG(g_cs, rejected, "negative_matrix",
                            "negative: nside=500 非正常完成 (status=%d, abort/异常/err 皆拒绝)",
                            status);
        }
    }
    return g_cs.failures == 0 ? 0 : 1;
}

}  // namespace

int p1drz_run_core_groups(int argc, char** argv);

int p1drz_run_core_groups(int argc, char** argv) {
    const p1drz::TestGroup groups[] = {
        {"units", group_units},
        {"properties", group_properties},
        {"oracle", group_oracle},
        {"negative", group_negative},
    };
    return run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
