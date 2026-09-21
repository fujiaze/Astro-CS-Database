// P1-WCS-TEST · properties 组 (F5 确定性 + 结构性质)
//
// 合同锚: docs/algorithms/PLATESOLVE.md §11.4 F5 + §5c 禁令 (禁止跨线程
// 浮点重结合)。冻结设计: "同输入同线程数 3 次运行 IpvWcsResult bitwise
// 一致; 线程 1/2/4 下 bitwise 一致 (投票归并为整数求和、拟合单线程, 无跨
// 线程浮点重结合); 若实测违背, P1-WCS-TEST 如实登记不得放宽语义"。
// 1-N worker 覆盖: triangle_match OpenMP 并行区在 1/2/4 线程下 bitwise。
// 性质断言: order=2 对纯线性场二阶项≈0 (模型嵌套); triangle_match 平移
// 不变性 (描述符 ba/ca 平移不变, Valdes 1995); fixture 同 seed 确定性。
#include "p1wcs_test_main.hpp"
#include "p1wcs_fixtures.hpp"
#include "p1wcs_oracle.hpp"

#include <omp.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ipv_itertrans.h"
#include "ipv_solver.h"
#include "ipv_triangle.h"

using namespace p1wcs;

namespace {

constexpr unsigned kSeedA = 20260907u;

// bitwise 语义比较: 逐字段 memcmp (struct 级 memcmp 会抓到未初始化
// padding 差异 — 非科学值差异, 禁用; P1-NOISE-TEST 同族教训)
template <typename T>
bool bitwise_equal(const T& a, const T& b) {
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}

// StarPoint 逐字段 bitwise (struct memcmp 会抓到未初始化 padding 差异 —
// 非科学值差异, 禁用)
bool bitwise_equal_stars(const std::vector<ipv::StarPoint>& a,
                         const std::vector<ipv::StarPoint>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!bitwise_equal(a[i].x, b[i].x) || !bitwise_equal(a[i].y, b[i].y) ||
            !bitwise_equal(a[i].flux, b[i].flux) ||
            a[i].saturated != b[i].saturated)
            return false;
    }
    return true;
}

bool bitwise_equal_trans(const ipv::Trans& a, const ipv::Trans& b) {
    return a.order == b.order && bitwise_equal(a.x00, b.x00) &&
           bitwise_equal(a.x10, b.x10) && bitwise_equal(a.x01, b.x01) &&
           bitwise_equal(a.y00, b.y00) && bitwise_equal(a.y10, b.y10) &&
           bitwise_equal(a.y01, b.y01) && bitwise_equal(a.x20, b.x20) &&
           bitwise_equal(a.x11, b.x11) && bitwise_equal(a.x02, b.x02) &&
           bitwise_equal(a.y20, b.y20) && bitwise_equal(a.y11, b.y11) &&
           bitwise_equal(a.y02, b.y02) && bitwise_equal(a.x30, b.x30) &&
           bitwise_equal(a.x21, b.x21) && bitwise_equal(a.x12, b.x12) &&
           bitwise_equal(a.x03, b.x03) && bitwise_equal(a.y30, b.y30) &&
           bitwise_equal(a.y21, b.y21) && bitwise_equal(a.y12, b.y12) &&
           bitwise_equal(a.y03, b.y03) && a.nr == b.nr && a.nm == b.nm &&
           bitwise_equal(a.sig, b.sig) && bitwise_equal(a.sx, b.sx) &&
           bitwise_equal(a.sy, b.sy) && a.valid == b.valid;
}

bool bitwise_equal_sip(const ipv::SIPCoeffs& a, const ipv::SIPCoeffs& b) {
    for (int i = 0; i < 36; ++i)
        if (!bitwise_equal(a.A[i], b.A[i]) || !bitwise_equal(a.B[i], b.B[i]) ||
            !bitwise_equal(a.AP[i], b.AP[i]) || !bitwise_equal(a.BP[i], b.BP[i]))
            return false;
    return a.order == b.order && a.ap_order == b.ap_order;
}

bool bitwise_equal_wcs(const ipv::WcsFitResult& a, const ipv::WcsFitResult& b) {
    return bitwise_equal(a.cd.cd11, b.cd.cd11) &&
           bitwise_equal(a.cd.cd12, b.cd.cd12) &&
           bitwise_equal(a.cd.cd21, b.cd.cd21) &&
           bitwise_equal(a.cd.cd22, b.cd.cd22) &&
           bitwise_equal(a.crval[0], b.crval[0]) &&
           bitwise_equal(a.crval[1], b.crval[1]) &&
           bitwise_equal(a.crpix[0], b.crpix[0]) &&
           bitwise_equal(a.crpix[1], b.crpix[1]) &&
           bitwise_equal(a.rms_px, b.rms_px) &&
           bitwise_equal(a.rms_arcsec, b.rms_arcsec) &&
           a.n_pairs == b.n_pairs && a.success == b.success &&
           a.trans_order == b.trans_order &&
           std::memcmp(a.ctype, b.ctype, sizeof(a.ctype)) == 0 &&
           bitwise_equal_sip(a.sip, b.sip);
}

template <typename T>
bool bitwise_equal_vec(const std::vector<T>& a, const std::vector<T>& b) {
    if (a.size() != b.size()) return false;
    if (a.empty()) return true;
    return std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0;
}

// 全链一次: FIX-WCS-A → iter_trans_solve(order=1) → extract_wcs_sip
struct SolveSnapshot {
    ipv::Trans trans;
    int n_inliers;
    double rms;
    ipv::WcsFitResult wcs;
};

SolveSnapshot solve_once(unsigned seed) {
    const FixWcsA fx = fix_wcs_a_linear(seed, 0.0, 0.0);
    const ipv::IterTransResult r =
        ipv::iter_trans_solve(fx.U, fx.W, fx.pairs, 5.0, 1);
    ipv::WcsFitResult w;
    extract_wcs_sip(r.trans, fx.truth.ra0, fx.truth.dec0, fx.width, fx.height,
                    fx.truth.s0, fx.U, fx.W, r.inliers, &w, nullptr);
    SolveSnapshot s;
    s.trans = r.trans;
    s.n_inliers = r.n_inliers;
    s.rms = r.rms;
    s.wcs = w;
    return s;
}

}  // namespace

namespace p1wcs {

int test_properties() {
    CheckState cs;
    cs.failures = 0;
    cs.fault_reported = false;

    // ------------------------------------------------------------------
    // fixture 同 seed 确定性 (可复用验证的根: fixture 必须完全由 seed 决定)
    // ------------------------------------------------------------------
    {
        const FixWcsA a1 = fix_wcs_a_linear(kSeedA, 0.0, 0.0);
        const FixWcsA a2 = fix_wcs_a_linear(kSeedA, 0.0, 0.0);
        const bool same = bitwise_equal_stars(a1.U, a2.U) &&
                          bitwise_equal_stars(a1.W, a2.W) &&
                          bitwise_equal_vec(a1.pairs, a2.pairs);
        P1WCS_CHECK(cs, same, "p1_fixture_deterministic");
    }

    // ------------------------------------------------------------------
    // F5a: 同输入同线程 3 次运行 bitwise 一致 (迭代重投影拟合 + WCS 提取)
    // ------------------------------------------------------------------
    {
        const SolveSnapshot s1 = solve_once(kSeedA);
        const SolveSnapshot s2 = solve_once(kSeedA);
        const SolveSnapshot s3 = solve_once(kSeedA);
        const bool same = bitwise_equal_trans(s1.trans, s2.trans) &&
                          bitwise_equal_trans(s1.trans, s3.trans) &&
                          s1.n_inliers == s2.n_inliers &&
                          s1.n_inliers == s3.n_inliers &&
                          bitwise_equal(s1.rms, s2.rms) &&
                          bitwise_equal(s1.rms, s3.rms) &&
                          bitwise_equal_wcs(s1.wcs, s2.wcs) &&
                          bitwise_equal_wcs(s1.wcs, s3.wcs);
        P1WCS_CHECK(cs, same, "p1_det_bitwise");
    }

    // ------------------------------------------------------------------
    // F5b: 线程 1/2/4 下 triangle_match (OpenMP 投票并行区) bitwise 一致
    // ------------------------------------------------------------------
    {
        const FixWcsA fx = fix_wcs_a_linear(kSeedA, 0.0, 0.0);
        ipv::TriangleMatchResult rt[3];
        for (int t = 0; t < 3; ++t) {
            omp_set_num_threads(1 << t);  // 1/2/4
            rt[t] = ipv::triangle_match(fx.U, fx.W, 60, 60, 0.002,
                                        fx.truth.s0);
        }
        omp_set_num_threads(1);  // 恢复, 不污染后续组
        const bool same = bitwise_equal_vec(rt[0].top_pairs, rt[1].top_pairs) &&
                          bitwise_equal_vec(rt[0].top_pairs, rt[2].top_pairs) &&
                          bitwise_equal_vec(rt[0].votes, rt[1].votes) &&
                          bitwise_equal_vec(rt[0].votes, rt[2].votes) &&
                          rt[0].n_triangles_A == rt[1].n_triangles_A &&
                          rt[0].n_triangles_B == rt[2].n_triangles_B &&
                          bitwise_equal(rt[0].max_vote, rt[2].max_vote);
        P1WCS_CHECK(cs, same, "p1_thread_bitwise");
        P1WCS_CHECK(cs, rt[0].success && rt[0].top_pairs.size() >= 12,
                    "p1_thread_bitwise");  // 确认比对发生在有效匹配上

        // 拟合主体单线程: 并行区线程数不影响 iter_trans/extract 输出
        const FixWcsA fx2 = fix_wcs_a_linear(kSeedA + 1u, -25.0, 40.0);
        ipv::WcsFitResult wq[3];
        for (int t = 0; t < 3; ++t) {
            omp_set_num_threads(1 << t);
            const ipv::IterTransResult r =
                ipv::iter_trans_solve(fx2.U, fx2.W, fx2.pairs, 5.0, 1);
            extract_wcs_sip(r.trans, fx2.truth.ra0, fx2.truth.dec0, fx2.width,
                            fx2.height, fx2.truth.s0, fx2.U, fx2.W, r.inliers,
                            &wq[t], nullptr);
        }
        omp_set_num_threads(1);
        const bool fit_same = bitwise_equal_wcs(wq[0], wq[1]) &&
                              bitwise_equal_wcs(wq[0], wq[2]);
        P1WCS_CHECK(cs, fit_same, "p1_thread_bitwise");
    }

    // ------------------------------------------------------------------
    // 性质: order=2 拟合纯线性场 → 二阶项≈0 (模型嵌套, SIP 系数≈0 px)
    // ------------------------------------------------------------------
    {
        const FixWcsA fx = fix_wcs_a_linear(kSeedA + 2u, 0.0, 0.0);
        const ipv::IterTransResult r =
            ipv::iter_trans_solve(fx.U, fx.W, fx.pairs, 5.0, 2);
        P1WCS_CHECK(cs, r.success, "p1_order2_linear");
        ipv::WcsFitResult w;
        extract_wcs_sip(r.trans, fx.truth.ra0, fx.truth.dec0, fx.width,
                        fx.height, fx.truth.s0, fx.U, fx.W, r.inliers, &w,
                        nullptr);
        double max_ab = 0.0;
        for (int i = 0; i < 36; ++i) {
            max_ab = std::max(max_ab, std::fabs(w.sip.A[i]));
            max_ab = std::max(max_ab, std::fabs(w.sip.B[i]));
        }
        P1WCS_CHECK(cs, max_ab < 1e-9, "p1_order2_linear");
    }

    // ------------------------------------------------------------------
    // 性质: triangle_match 平移不变性 (W 平移 → 描述符不变 → 匹配 bitwise 同)
    // ------------------------------------------------------------------
    {
        const FixWcsA fx = fix_wcs_a_linear(kSeedA + 3u, 0.0, 0.0);
        std::vector<ipv::StarPoint> Wt = fx.W;
        for (auto& w : Wt) {
            w.x += 13.7;
            w.y -= 9.2;
        }
        omp_set_num_threads(2);
        const ipv::TriangleMatchResult r0 =
            ipv::triangle_match(fx.U, fx.W, 60, 60, 0.002, fx.truth.s0);
        const ipv::TriangleMatchResult r1 =
            ipv::triangle_match(fx.U, Wt, 60, 60, 0.002, fx.truth.s0);
        omp_set_num_threads(1);
        const bool inv = bitwise_equal_vec(r0.top_pairs, r1.top_pairs) &&
                         bitwise_equal_vec(r0.votes, r1.votes);
        P1WCS_CHECK(cs, inv, "p1_translate_invariant");
        P1WCS_CHECK(cs, r0.success && r0.top_pairs.size() >= 12,
                    "p1_translate_invariant");
    }

    if (cs.failures == 0) {
        std::fprintf(stdout, "P1WCS PROPERTIES PASS\n");
        return 0;
    }
    std::fprintf(stderr, "P1WCS PROPERTIES FAIL (%d check(s))\n", cs.failures);
    return 1;
}

}  // namespace p1wcs
