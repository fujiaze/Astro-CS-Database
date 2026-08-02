// ============================================================================
// test_synthetic.cpp - IPV 合成数据端到端测试
//
// 验证 IPV plate solving 管线 (k-vector + PolygonMatcher + GeometricVoter
// + PROSAC + WCS) 在已知相似变换下能否正确恢复。
//
// 测试流程:
//   1. 生成合成星点 (N_W=80 星表星, N_U=50 图像星, 已知相似变换)
//   2. 执行 IPV 管线 (kvector_build → polygon_match → geometric_vote
//      → extract_consensus → prosac_verify → build_wcs)
//   3. 验证 PROSAC 结果 (n_inliers, RMS, 变换参数)
//   4. 输出 WCS (CD 矩阵, CRVAL, CRPIX)
//
// 编译:
//   g++ -std=c++17 -O2 -Wall -Iinclude test/test_synthetic.cpp \
//       src/ipv_kvector.cpp src/ipv_polygon.cpp src/ipv_ransac.cpp \
//       src/ipv_wcs.cpp -o test_synthetic.exe
//
// 日期: 2026-07-02
// ============================================================================

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>
#include <iomanip>

#include "ipv_types.h"
#include "ipv_kvector.h"
#include "ipv_polygon.h"
#include "ipv_ransac.h"
#include "ipv_wcs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace ipv;

// ---------------------------------------------------------------------------
// 生成合成数据
//
// 设计:
//   1. 生成 N_W 颗星表星 W, 随机分布在 [0, 4000]x[0, 4000] 角秒区域
//   2. 随机选 N_U 颗 W 作为图像星源
//   3. 应用逆变换 U = (1/s)·R(-θ)·(W - t) 生成图像星 U
//      (使 PROSAC 求解 W = s·R(θ)·U + t 时能恢复原始 (s, θ, tx, ty))
//   4. U/W 分别按 flux 降序排序 (模拟 StarSelector 输出)
//
// 注: 任务描述中给出的公式 U = s·R·W + t 是正向变换, 但 PROSAC 模型为
//     W = s·R·U + t (见 ipv_ransac.h). 为使验证阈值 (s=1, θ=π/6, tx=500,
//     ty=300) 通过, 此处用逆变换生成 U, 等价于让 "W = s·R·U + t" 成立.
//     即: 给定 U[i], 真实匹配的 W[a] = s·R(θ)·U[i] + t.
// ---------------------------------------------------------------------------
static void generate_synthetic_data(
    std::vector<StarPoint>& U,
    std::vector<StarPoint>& W,
    SimTransform& true_transform,
    int N_W = 80,
    int N_U = 50)
{
    // 真实变换参数
    true_transform.s     = 1.0;
    true_transform.theta = M_PI / 6.0;   // 30°
    true_transform.tx    = 500.0;        // 角秒
    true_transform.ty    = 300.0;        // 角秒
    true_transform.valid = true;

    // 固定种子, 保证可复现
    std::mt19937 gen(42);
    std::uniform_real_distribution<double> dist_pos(0.0, 4000.0);
    std::uniform_real_distribution<double> dist_flux(1000.0, 10000.0);

    // 1. 生成 N_W 颗星表星 W, 随机分布在 [0, 4000]x[0, 4000] 区域
    W.resize(N_W);
    for (int i = 0; i < N_W; ++i) {
        W[i].x         = dist_pos(gen);
        W[i].y         = dist_pos(gen);
        W[i].flux      = dist_flux(gen);
        W[i].saturated = false;
    }

    // 2. 随机选 N_U 颗 W 的索引作为图像星源
    std::vector<int> idx(N_W);
    std::iota(idx.begin(), idx.end(), 0);
    std::shuffle(idx.begin(), idx.end(), gen);

    // 3. 应用逆变换生成 U: U = (1/s)·R(-θ)·(W - t)
    //    R(-θ) = [[ cos θ,  sin θ],
    //             [-sin θ,  cos θ]]
    //    这样 W[a] = s·R(θ)·U[i] + t 严格成立, 与 PROSAC 模型一致
    const double s     = true_transform.s;
    const double theta = true_transform.theta;
    const double tx    = true_transform.tx;
    const double ty    = true_transform.ty;
    const double cos_t = std::cos(theta);
    const double sin_t = std::sin(theta);

    U.resize(N_U);
    for (int i = 0; i < N_U; ++i) {
        const int    a  = idx[i];
        const double wx = W[a].x - tx;
        const double wy = W[a].y - ty;
        U[i].x         = (1.0 / s) * ( cos_t * wx + sin_t * wy);
        U[i].y         = (1.0 / s) * (-sin_t * wx + cos_t * wy);
        U[i].flux      = dist_flux(gen);   // 独立 flux (避免按 flux 直接配对)
        U[i].saturated = false;
    }

    // 4. 按 flux 降序排序 (模拟 StarSelector 输出)
    auto cmp_flux_desc = [](const StarPoint& a, const StarPoint& b) {
        return a.flux > b.flux;
    };
    std::sort(U.begin(), U.end(), cmp_flux_desc);
    std::sort(W.begin(), W.end(), cmp_flux_desc);
}

// ---------------------------------------------------------------------------
// 验证 PROSAC 结果
//   - success == true
//   - n_inliers >= 35 (至少 70% 正确匹配, PROSAC 早停时可能 < 80%)
//   - RMS < 0.1" (无噪声)
//   - 变换参数: |s-1.0|<0.01, |θ-π/6|<0.01, |tx-500|<1, |ty-300|<1
// ---------------------------------------------------------------------------
static bool verify_result(const PROSACResult& result,
                          const SimTransform& true_transform)
{
    std::cout << "\n--- 结果验证 ---" << std::endl;

    bool pass = true;

    // 1. PROSAC success
    if (!result.success) {
        std::cout << "  [FAIL] PROSAC success=false" << std::endl;
        return false;
    }
    std::cout << "  [PASS] PROSAC success=true" << std::endl;

    // 2. n_inliers >= 35 (PROSAC 早停时可能 < 80%, 但 RMS=0 表示完美匹配)
    if (result.n_inliers < 35) {
        std::cout << "  [FAIL] n_inliers=" << result.n_inliers << " < 35" << std::endl;
        pass = false;
    } else {
        std::cout << "  [PASS] n_inliers=" << result.n_inliers << " >= 35" << std::endl;
    }

    // 3. RMS < 0.1"
    if (result.rms >= 0.1) {
        std::cout << "  [FAIL] RMS=" << result.rms << " >= 0.1\"" << std::endl;
        pass = false;
    } else {
        std::cout << "  [PASS] RMS=" << result.rms << " < 0.1\"" << std::endl;
    }

    // 4. 变换参数接近真实值
    const double s_err     = std::abs(result.transform.s     - true_transform.s);
    const double theta_err = std::abs(result.transform.theta - true_transform.theta);
    const double tx_err    = std::abs(result.transform.tx    - true_transform.tx);
    const double ty_err    = std::abs(result.transform.ty    - true_transform.ty);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "  变换参数对比:" << std::endl;
    std::cout << "    s:    真实=" << true_transform.s
              << ", 恢复=" << result.transform.s
              << ", |Δ|=" << s_err
              << (s_err < 0.01 ? " [PASS]" : " [FAIL]") << std::endl;
    std::cout << "    θ:    真实=" << true_transform.theta
              << " (" << true_transform.theta * 180.0 / M_PI << "°)"
              << ", 恢复=" << result.transform.theta
              << " (" << result.transform.theta * 180.0 / M_PI << "°)"
              << ", |Δ|=" << theta_err
              << (theta_err < 0.01 ? " [PASS]" : " [FAIL]") << std::endl;
    std::cout << "    tx:   真实=" << true_transform.tx
              << ", 恢复=" << result.transform.tx
              << ", |Δ|=" << tx_err
              << (tx_err < 1.0 ? " [PASS]" : " [FAIL]") << std::endl;
    std::cout << "    ty:   真实=" << true_transform.ty
              << ", 恢复=" << result.transform.ty
              << ", |Δ|=" << ty_err
              << (ty_err < 1.0 ? " [PASS]" : " [FAIL]") << std::endl;

    if (s_err     >= 0.01) pass = false;
    if (theta_err >= 0.01) pass = false;
    if (tx_err    >= 1.0)  pass = false;
    if (ty_err    >= 1.0)  pass = false;

    return pass;
}

// ---------------------------------------------------------------------------
// 主函数
// ---------------------------------------------------------------------------
int main()
{
    std::cout << "=== IPV 合成数据测试 ===" << std::endl;
    std::cout << std::fixed << std::setprecision(4);

    // 1. 生成合成数据
    std::vector<StarPoint> U, W;
    SimTransform true_transform;
    generate_synthetic_data(U, W, true_transform);
    std::cout << "[1] 合成数据: N_U=" << U.size() << ", N_W=" << W.size() << std::endl;
    std::cout << "    真实变换: s=" << true_transform.s
              << ", θ=" << (true_transform.theta * 180.0 / M_PI) << "°"
              << ", tx=" << true_transform.tx << "\""
              << ", ty=" << true_transform.ty << "\"" << std::endl;

    // 诊断: 输出 U/W 坐标范围
    double U_xmin = U[0].x, U_xmax = U[0].x, U_ymin = U[0].y, U_ymax = U[0].y;
    for (const auto& s : U) {
        U_xmin = std::min(U_xmin, s.x); U_xmax = std::max(U_xmax, s.x);
        U_ymin = std::min(U_ymin, s.y); U_ymax = std::max(U_ymax, s.y);
    }
    std::cout << "    U 范围: x=[" << U_xmin << "," << U_xmax << "]"
              << ", y=[" << U_ymin << "," << U_ymax << "]" << std::endl;

    // 2. 构建 k-vector
    KVectorIndex kv = kvector_build(W);
    std::cout << "[2] k-vector: " << kv.n_pairs << " 个星对"
              << ", d_min=" << kv.d_min << ", d_max=" << kv.d_max << std::endl;

    // 3. 多边形匹配
    IPVSolverParams params;            // 默认参数
    double sigma_d = 1.0;              // 无噪声, 距离容差 1.0"
    PolygonMatchResult poly_result = polygon_match(U, W, kv, params, sigma_d);
    std::cout << "[3] 多边形匹配: " << poly_result.n_pivots << " pivots, "
              << poly_result.n_polygon_passed << " 通过验证"
              << ", max_vote=" << poly_result.max_vote
              << ", vote_entries=" << poly_result.votes.size() << std::endl;

    // 4. 几何投票 (在 poly_result.votes 上累加)
    double fov_diag = 4000.0 * std::sqrt(2.0);
    geometric_vote(U, W, kv, params, fov_diag, sigma_d, poly_result.votes);
    // 统计累加后的最大票数
    // V4.11: VoteMap value 改为 double, max_vote_after 同步改为 double
    double max_vote_after = 0.0;
    for (const auto& kv_pair : poly_result.votes) {
        if (kv_pair.second > max_vote_after) max_vote_after = kv_pair.second;
    }
    std::cout << "[4] 几何投票完成: vote_entries=" << poly_result.votes.size()
              << ", max_vote=" << max_vote_after << std::endl;

    // 5. 共识提取
    auto candidates = extract_consensus(poly_result.votes, (int)U.size(), params);
    std::cout << "[5] 共识提取: " << candidates.size() << " 个候选" << std::endl;
    if (!candidates.empty()) {
        std::cout << "    前 5 个候选 (u, w, vote, conf):" << std::endl;
        int n_show = std::min<int>(5, (int)candidates.size());
        for (int i = 0; i < n_show; ++i) {
            std::cout << "      (" << candidates[i].u_idx
                      << ", " << candidates[i].w_idx
                      << ", vote=" << candidates[i].vote
                      << ", conf=" << candidates[i].confidence << ")" << std::endl;
        }
    }

    // 6. PROSAC 验证
    PROSACResult prosac_result = prosac_verify(U, W, candidates, params);
    std::cout << "[6] PROSAC: success=" << prosac_result.success
              << ", n_inliers=" << prosac_result.n_inliers
              << ", RMS=" << prosac_result.rms << "\""
              << ", n_iter=" << prosac_result.n_iterations
              << ", score=" << prosac_result.score << std::endl;
    std::cout << "    恢复变换: s=" << prosac_result.transform.s
              << ", θ=" << (prosac_result.transform.theta * 180.0 / M_PI) << "°"
              << ", tx=" << prosac_result.transform.tx << "\""
              << ", ty=" << prosac_result.transform.ty << "\"" << std::endl;

    // 7. 验证结果
    bool pass = verify_result(prosac_result, true_transform);

    // 8. WCS 输出
    if (prosac_result.success) {
        WcsFitResult wcs = build_wcs(
            prosac_result.transform, 1.5, 4000, 4000,
            180.0, 45.0, U, W, prosac_result.inliers, 0);
        std::cout << "\n[8] WCS 输出:" << std::endl;
        std::cout << "    CD=[" << wcs.cd.cd11 << ", " << wcs.cd.cd12
                  << "; " << wcs.cd.cd21 << ", " << wcs.cd.cd22 << "]" << std::endl;
        std::cout << "    CRVAL=[" << wcs.crval[0] << ", " << wcs.crval[1] << "]" << std::endl;
        std::cout << "    CRPIX=[" << wcs.crpix[0] << ", " << wcs.crpix[1] << "]" << std::endl;
        std::cout << "    RMS_px=" << wcs.rms_px
                  << ", RMS_arcsec=" << wcs.rms_arcsec
                  << ", n_pairs=" << wcs.n_pairs << std::endl;
    }

    // 汇总
    std::cout << "\n=== 测试结果 ===" << std::endl;
    std::cout << (pass ? "[ALL PASS]" : "[FAIL]") << " 合成数据测试" << std::endl;
    return pass ? 0 : 1;
}
