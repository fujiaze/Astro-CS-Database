// ============================================================================
// ipv_ransac.cpp - IPV PROSAC 验证模块实现
//
// 实现:
// - solve_similarity_transform: 2 对匹配解析求解相似变换 (相对向量法)
// - umeyama_estimate: 所有内点 Umeyama SVD 闭合解 (手写 2x2 SVD)
// - prosac_verify: PROSAC 按 vote 降序优先采样, 尺度约束, 内点验证, Umeyama 精化
//
// 模型: W = s·R(θ)·U + t
// U = 图像侧星点 (角秒坐标, 原点图像中心)
// W = 星表侧星点 (角秒坐标)
// R = [[cos θ, -sin θ], [sin θ, cos θ]]
//
// 设计要点:
// - 2x2 SVD 手写实现 (不依赖 Eigen, 用对称矩阵特征值法)
// - 随机数 std::mt19937 固定种子 (42) 保证可复现性
// - 退化检查: 两图像点距离 < 10" 重采
// - 尺度约束: s ∈ [s_min, s_max] (默认 [0.95, 1.05])
// - 内点阈值: τ = ransac_inlier_threshold_arcsec (默认 3.0")
// - 成功条件: best_n_inliers >= 4 且 best_RMS < 5.0"
// - 日志: 模块级静态 Logger, 默认输出到 stderr
//
// 日期: 2026-07-02
// ============================================================================

#include "ipv_ransac.h"
#include "ipv_log.h"

#include <cmath>
#include <random>
#include <algorithm>
#include <string>

namespace ipv {

// ---------------------------------------------------------------------------
// 模块级日志器 (默认输出到 stderr; 外部可调用 init_ransac_logger() 写文件)
// ---------------------------------------------------------------------------
static Logger g_ransac_logger;

Logger& ransac_logger() {
    return g_ransac_logger;
}

void init_ransac_logger(const std::string& path) {
    g_ransac_logger.init(path);
}

// ===========================================================================
// 内部工具: 2x2 SVD (手写, 不依赖 Eigen)
//
// 通过对称矩阵 A^T A 的特征值/特征向量分解得到 SVD:
// 1. E = A^T A (对称正定 2x2)
// 2. 特征值: λ = (tr(E)/2) ± sqrt((tr(E)/2 - e00)² + e01²)
// 3. 奇异值: σ_i = sqrt(λ_i)
// 4. V 的列 = E 的特征向量
// 5. U 的列 = A·v_i / σ_i (σ_i > 0 时), 否则取正交补
//
// 矩阵存储: row-major [m00, m01, m10, m11]
// 分解: A = U · diag(S) · V^T
// ===========================================================================
static void svd_2x2(const double A[4], double U[4], double S[2], double V[4]) {
    const double a = A[0], b = A[1], c = A[2], d = A[3];

    // E = A^T A (对称 2x2)
    const double e00 = a * a + c * c;
    const double e01 = a * b + c * d;
    const double e11 = b * b + d * d;

    // 对称 2x2 矩阵特征值
    const double sum_e  = e00 + e11;
    const double diff_e = e00 - e11;
    const double disc   = std::sqrt(diff_e * diff_e / 4.0 + e01 * e01);
    const double lam0   = sum_e / 2.0 + disc;  // 较大特征值
    const double lam1   = sum_e / 2.0 - disc;  // 较小特征值

    S[0] = std::sqrt(std::max(lam0, 0.0));
    S[1] = std::sqrt(std::max(lam1, 0.0));

    // V 的列 = E 的特征向量
    // 对于 lam0: v0 = (e01, lam0 - e00) 或 (lam0 - e11, e01), 取范数大的方向避免数值问题
    double v0x, v0y;
    if (std::abs(e01) > 1e-15) {
        v0x = e01;
        v0y = lam0 - e00;
    } else {
        // E 为对角阵, 特征向量即坐标轴
        if (e00 >= e11) { v0x = 1.0; v0y = 0.0; }
        else            { v0x = 0.0; v0y = 1.0; }
    }
    double n0 = std::sqrt(v0x * v0x + v0y * v0y);
    if (n0 < 1e-15) { v0x = 1.0; v0y = 0.0; }
    else { v0x /= n0; v0y /= n0; }

    // v1 与 v0 正交 (逆时针 90°)
    const double v1x = -v0y;
    const double v1y =  v0x;

    // V row-major: V[0]=v0x, V[1]=v1x, V[2]=v0y, V[3]=v1y
    V[0] = v0x; V[1] = v1x;
    V[2] = v0y; V[3] = v1y;

    // U 的列: u_i = A · v_i / σ_i
    if (S[0] > 1e-15) {
        U[0] = (a * v0x + b * v0y) / S[0];
        U[2] = (c * v0x + d * v0y) / S[0];
    } else {
        U[0] = 1.0; U[2] = 0.0;
    }
    if (S[1] > 1e-15) {
        U[1] = (a * v1x + b * v1y) / S[1];
        U[3] = (c * v1x + d * v1y) / S[1];
    } else {
        // σ_1 ≈ 0: u_1 取与 u_0 正交的向量
        U[1] = -U[2];
        U[3] =  U[0];
    }
}

// ===========================================================================
// solve_similarity_transform: 从 2 对匹配求解相似变换
//
// 相对向量法消去平移:
// ΔU = U[u2] - U[u1], ΔW = W[w2] - W[w1]
// s = |ΔW| / |ΔU|
// θ = atan2(ΔW.y, ΔW.x) - atan2(ΔU.y, ΔU.x)
// tx = W[w1].x - s·(cos θ · U[u1].x - sin θ · U[u1].y)
// ty = W[w1].y - s·(sin θ · U[u1].x + cos θ · U[u1].y)
// ===========================================================================
SimTransform solve_similarity_transform(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    int u1, int w1, int u2, int w2)
{
    SimTransform result{1.0, 0.0, 0.0, 0.0, false};

    // 边界检查
    const int N_U = (int)U.size();
    const int N_W = (int)W.size();
    if (u1 < 0 || u1 >= N_U || u2 < 0 || u2 >= N_U ||
        w1 < 0 || w1 >= N_W || w2 < 0 || w2 >= N_W) {
        return result;
    }

    // 图像侧相对向量
    const double dUx = U[u2].x - U[u1].x;
    const double dUy = U[u2].y - U[u1].y;
    // 星表侧相对向量
    const double dWx = W[w2].x - W[w1].x;
    const double dWy = W[w2].y - W[w1].y;

    const double len_U = std::sqrt(dUx * dUx + dUy * dUy);
    const double len_W = std::sqrt(dWx * dWx + dWy * dWy);

    // 退化检查: 两图像点重合
    if (len_U < 1e-10) {
        return result;
    }

    // 尺度与旋转
    const double s     = len_W / len_U;
    const double theta = std::atan2(dWy, dWx) - std::atan2(dUy, dUx);

    // 平移
    const double cos_t = std::cos(theta);
    const double sin_t = std::sin(theta);
    const double tx = W[w1].x - s * (cos_t * U[u1].x - sin_t * U[u1].y);
    const double ty = W[w1].y - s * (sin_t * U[u1].x + cos_t * U[u1].y);

    result.s     = s;
    result.theta = theta;
    result.tx    = tx;
    result.ty    = ty;
    result.valid = true;
    return result;
}

// ===========================================================================
// umeyama_estimate: Umeyama SVD 完整估计
//
// 算法 (参考 vm44_fit.cpp 的 Umeyama 实现, 重写为 IPV 风格):
// 1. 计算质心: mu_U = mean(U[pairs.u]), mu_W = mean(W[pairs.w])
// 2. 中心化: U_c[i] = U[u_i] - mu_U, W_c[i] = W[w_i] - mu_W
// 3. 协方差矩阵 H = Σ W_c[i] · U_c[i]^T (2x2)
// 4. SVD: H = U_svd · Σ · V_svd^T (手写 2x2 SVD)
// 5. S = diag(1, det(U_svd · V_svd^T))
// 6. R = U_svd · S · V_svd^T
// 7. s = trace(Σ · S) / Σ|U_c[i]|²
// 8. t = mu_W - s · R · mu_U
// 9. θ = atan2(R[1][0], R[0][0])
// ===========================================================================
SimTransform umeyama_estimate(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& pairs)
{
    SimTransform result{1.0, 0.0, 0.0, 0.0, false};

    const int n = (int)pairs.size();
    if (n < 2) {
        g_ransac_logger.warn("umeyama_estimate: 点数不足 (n < 2)");
        return result;
    }

    // 1. 计算质心
    double mu_Ux = 0.0, mu_Uy = 0.0;
    double mu_Wx = 0.0, mu_Wy = 0.0;
    for (int i = 0; i < n; ++i) {
        const int u = pairs[i].u;
        const int w = pairs[i].w;
        mu_Ux += U[u].x; mu_Uy += U[u].y;
        mu_Wx += W[w].x; mu_Wy += W[w].y;
    }
    mu_Ux /= n; mu_Uy /= n;
    mu_Wx /= n; mu_Wy /= n;

    // 2. 中心化 + 3. 协方差矩阵 H = Σ W_c · U_c^T (row-major [h00, h01, h10, h11])
    double H[4] = {0.0, 0.0, 0.0, 0.0};
    double var_U = 0.0;  // Σ |U_c|²
    for (int i = 0; i < n; ++i) {
        const int u = pairs[i].u;
        const int w = pairs[i].w;
        const double Ucx = U[u].x - mu_Ux;
        const double Ucy = U[u].y - mu_Uy;
        const double Wcx = W[w].x - mu_Wx;
        const double Wcy = W[w].y - mu_Wy;

        H[0] += Wcx * Ucx;  // h00
        H[1] += Wcx * Ucy;  // h01
        H[2] += Wcy * Ucx;  // h10
        H[3] += Wcy * Ucy;  // h11

        var_U += Ucx * Ucx + Ucy * Ucy;
    }

    if (var_U < 1e-15) {
        g_ransac_logger.warn("umeyama_estimate: 图像侧方差为 0 (点重合)");
        return result;
    }

    // 4. SVD: H = U_svd · Σ · V_svd^T
    double Us[4], Ss[2], Vs[4];
    svd_2x2(H, Us, Ss, Vs);

    // 5. S = diag(1, det(U_svd · V_svd^T))
    // U_svd · V_svd^T (2x2, row-major):
    // [Us[0]·Vs[0] + Us[1]·Vs[1], Us[0]·Vs[2] + Us[1]·Vs[3]]
    // [Us[2]·Vs[0] + Us[3]·Vs[1], Us[2]·Vs[2] + Us[3]·Vs[3]]
    const double UVt_00 = Us[0] * Vs[0] + Us[1] * Vs[1];
    const double UVt_01 = Us[0] * Vs[2] + Us[1] * Vs[3];
    const double UVt_10 = Us[2] * Vs[0] + Us[3] * Vs[1];
    const double UVt_11 = Us[2] * Vs[2] + Us[3] * Vs[3];
    const double det_UVt = UVt_00 * UVt_11 - UVt_01 * UVt_10;

    const double S_diag[2] = {1.0, (det_UVt < 0.0) ? -1.0 : 1.0};

    // 6. R = U_svd · diag(S_diag) · V_svd^T
    // 先计算 U_svd · diag(S_diag) (列乘 S_diag)
    double US[4];
    US[0] = Us[0] * S_diag[0];
    US[1] = Us[1] * S_diag[1];
    US[2] = Us[2] * S_diag[0];
    US[3] = Us[3] * S_diag[1];
    // R = US · V_svd^T
    double R[4];
    R[0] = US[0] * Vs[0] + US[1] * Vs[1];
    R[1] = US[0] * Vs[2] + US[1] * Vs[3];
    R[2] = US[2] * Vs[0] + US[3] * Vs[1];
    R[3] = US[2] * Vs[2] + US[3] * Vs[3];

    // 7. 尺度 s = trace(Σ · S) / Σ|U_c|²
    const double s = (Ss[0] * S_diag[0] + Ss[1] * S_diag[1]) / var_U;

    // 8. 平移 t = mu_W - s · R · mu_U
    const double tx = mu_Wx - s * (R[0] * mu_Ux + R[1] * mu_Uy);
    const double ty = mu_Wy - s * (R[2] * mu_Ux + R[3] * mu_Uy);

    // 9. 从 R 提取 θ (R = [[cos θ, -sin θ], [sin θ, cos θ]])
    const double theta = std::atan2(R[2], R[0]);

    result.s     = s;
    result.theta = theta;
    result.tx    = tx;
    result.ty    = ty;
    result.valid = true;

    g_ransac_logger.infof("umeyama_estimate: n=%d s=%.6f theta=%.6f deg tx=%.4f ty=%.4f",
                          n, s, theta * 57.295779513082323, tx, ty);
    return result;
}

// ===========================================================================
// prosac_verify: PROSAC 验证主函数
//
// 算法:
// 1. 初始化最优结果
// 2. 初始池 T0 = min(max(10, |M|/4), |M|)
// 3. 迭代 (最多 ransac_max_iter 次):
// a. 从前 T 个候选中随机采样 2 对不同匹配
// b. 退化检查: |U[u1] - U[u2]| < 10" 重采
// c. solve_similarity_transform 求解 (s, θ, tx, ty)
// d. 尺度约束: s ∈ [s_min, s_max], 否则跳过
// e. 内点验证: 残差 < τ 加入内点
// f. 更新最优 (内点数优先, RMS 次之)
// g. 每 10 次迭代扩展池 T
// h. 连续 100 次无改进且 T >= |M| 提前终止
// 4. best_n_inliers >= 4 时用 Umeyama 精化
// 5. 组装 PROSACResult
// ===========================================================================
PROSACResult prosac_verify(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<CandidateMatch>& candidates,
    const IPVSolverParams& params)
{
    PROSACResult result;
    result.transform     = {1.0, 0.0, 0.0, 0.0, false};
    result.rms           = 1e9;
    result.n_inliers     = 0;
    result.n_iterations  = 0;
    result.score         = 0.0;
    result.success       = false;

    const int M = (int)candidates.size();
    if (M < 2) {
        g_ransac_logger.warnf("prosac_verify: 候选数 %d < 2, 无法求解", M);
        return result;
    }

    g_ransac_logger.infof("prosac_verify: 开始 (M=%d, max_iter=%d, tau=%.2f px, s=[%.3f,%.3f], K_greedy=%d)",
                          M, params.ransac_max_iter, params.ransac_inlier_threshold_arcsec,
                          params.s_min, params.s_max, std::min(15, M));

    // 固定种子保证可复现性
    std::mt19937 gen(42);

    // 渐进阈值: tau_wide 找候选, tau_tight 收紧验证
    // 用户指导: "做个实验, 还可以搞成渐进阈值之类的"
    // 宽阈值找到候选解 (容纳投影畸变), 紧阈值排除错误配对
    const double tau_wide  = params.ransac_inlier_threshold_arcsec;       // 默认 3 px
    const double tau_tight = std::max(1.5, tau_wide * 0.5);               // 1.5 px
    g_ransac_logger.infof("prosac_verify: 渐进阈值 tau_wide=%.2f px, tau_tight=%.2f px",
                          tau_wide, tau_tight);

    // 诊断: 打印候选 vote 分布 (前 20 个)
    // vote 改为 double (支持 angle bonus), 格式串 %d → %.2f
    {
        std::string dist = "vote分布(前20): ";
        int show_n = std::min(20, M);
        for (int i = 0; i < show_n; ++i) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f ", candidates[i].vote);
            dist += buf;
        }
        g_ransac_logger.infof("prosac_verify: %s", dist.c_str());
    }

    // 最优结果
    int best_n_inliers = 0;
    double best_RMS = 1e9;
    SimTransform best_transform{1.0, 0.0, 0.0, 0.0, false};
    std::vector<MatchPair> best_inliers;

    // 初始池大小: T0 = min(max(20, |M|/2), |M|)
    // 注: 原 T0 = min(max(10, |M|/4), |M|) 对小候选集过小,
    // 增大至 M/2 让更多高票候选参与初期采样, 提高找到正确匹配对的概率。
    int T = std::min(std::max(20, M / 2), M);

    int no_improve_count = 0;
    const int max_iter = params.ransac_max_iter;
    const double tau = tau_wide;  // 初验用宽阈值
    const double deg = 57.295779513082323;

    g_ransac_logger.infof("prosac_verify: 初始池 T0=%d", T);

    // === 第一阶段: 贪心枚举前 K 个高票候选的所有两两组合 ===
    // 确保最高票候选的所有配对都被尝试, 避免随机采样错过正确匹配
    // K_greedy 从 5 增大到 15, 覆盖更多高票候选 (extract_consensus top-3 后候选数增多)
    const int K_greedy = std::min(15, M);
    int greedy_pairs = 0;
    bool early_stop = false;  // 早停标志: 三阶段共享, 触发后跳过后续阶段
    for (int a = 0; a < K_greedy && !early_stop; ++a) {
        for (int b = a + 1; b < K_greedy; ++b) {
            const int u1 = candidates[a].u_idx, w1 = candidates[a].w_idx;
            const int u2 = candidates[b].u_idx, w2 = candidates[b].w_idx;

            // 退化检查: 两图像点距离 < 10" 跳过
            const double dx = U[u1].x - U[u2].x;
            const double dy = U[u1].y - U[u2].y;
            const double dist_img = std::sqrt(dx * dx + dy * dy);
            if (dist_img < 10.0) continue;

            // 跳过相同 u 的配对 (extract_consensus top-3 会产生同 u 不同 w 的候选)
            if (u1 == u2) continue;

            const SimTransform tf = solve_similarity_transform(U, W, u1, w1, u2, w2);
            if (!tf.valid) continue;
            if (tf.s < params.s_min || tf.s > params.s_max) continue;

            // 内点验证
            const double cos_t = std::cos(tf.theta);
            const double sin_t = std::sin(tf.theta);

            // 诊断: 第一个贪心对打印详细残差分布
            if (greedy_pairs == 0) {
                g_ransac_logger.infof("  诊断[第一个贪心对]: a=%d b=%d u1=%d w1=%d u2=%d w2=%d",
                                      a, b, u1, w1, u2, w2);
                g_ransac_logger.infof("    s=%.4f theta=%.2f deg tx=%.1f ty=%.1f",
                                      tf.s, tf.theta * deg, tf.tx, tf.ty);
                g_ransac_logger.infof("    U[u1]=(%.1f,%.1f) U[u2]=(%.1f,%.1f)",
                                      U[u1].x, U[u1].y, U[u2].x, U[u2].y);
                g_ransac_logger.infof("    W[w1]=(%.1f,%.1f) W[w2]=(%.1f,%.1f)",
                                      W[w1].x, W[w1].y, W[w2].x, W[w2].y);
                // 打印所有候选的残差 (排序后前 15 个最小)
                std::vector<double> residuals(M);
                for (int k = 0; k < M; ++k) {
                    const int uk = candidates[k].u_idx;
                    const int wk = candidates[k].w_idx;
                    const double x_pred = tf.s * (cos_t * U[uk].x - sin_t * U[uk].y) + tf.tx;
                    const double y_pred = tf.s * (sin_t * U[uk].x + cos_t * U[uk].y) + tf.ty;
                    const double rx = W[wk].x - x_pred;
                    const double ry = W[wk].y - y_pred;
                    residuals[k] = std::sqrt(rx * rx + ry * ry);
                }
                std::sort(residuals.begin(), residuals.end());
                std::string rstr = "    残差排序(前15): ";
                for (int k = 0; k < 15 && k < M; ++k) {
                    char buf[16]; std::snprintf(buf, sizeof(buf), "%.2f ", residuals[k]);
                    rstr += buf;
                }
                g_ransac_logger.infof("%s", rstr.c_str());
            }
            std::vector<MatchPair> inliers;
            inliers.reserve(M);
            double sum_sq = 0.0;
            for (int k = 0; k < M; ++k) {
                const int u = candidates[k].u_idx;
                const int w = candidates[k].w_idx;
                const double x_pred = tf.s * (cos_t * U[u].x - sin_t * U[u].y) + tf.tx;
                const double y_pred = tf.s * (sin_t * U[u].x + cos_t * U[u].y) + tf.ty;
                const double rx = W[w].x - x_pred;
                const double ry = W[w].y - y_pred;
                const double r = std::sqrt(rx * rx + ry * ry);
                if (r < tau) {
                    inliers.push_back({u, w});
                    sum_sq += r * r;
                }
            }
            const int n_inliers = (int)inliers.size();
            const double rms = (n_inliers > 0) ? std::sqrt(sum_sq / n_inliers) : 1e9;
            greedy_pairs++;

            if (n_inliers > best_n_inliers ||
                (n_inliers == best_n_inliers && rms < best_RMS)) {
                // 渐进收紧验证 - 用 tau_tight 重新筛 inliers
                // 排除被宽阈值放行的错误配对, 保留真实匹配
                std::vector<MatchPair> tight_inliers;
                double tight_sum_sq = 0.0;
                for (int k = 0; k < M; ++k) {
                    const int u = candidates[k].u_idx;
                    const int w = candidates[k].w_idx;
                    const double x_pred = tf.s * (cos_t * U[u].x - sin_t * U[u].y) + tf.tx;
                    const double y_pred = tf.s * (sin_t * U[u].x + cos_t * U[u].y) + tf.ty;
                    const double rx = W[w].x - x_pred;
                    const double ry = W[w].y - y_pred;
                    const double r = std::sqrt(rx * rx + ry * ry);
                    if (r < tau_tight) {
                        tight_inliers.push_back({u, w});
                        tight_sum_sq += r * r;
                    }
                }
                const int n_tight = (int)tight_inliers.size();
                const double rms_tight = (n_tight > 0) ? std::sqrt(tight_sum_sq / n_tight) : 1e9;

                best_n_inliers   = n_tight;       // 用收紧后的内点数
                best_RMS         = rms_tight;
                best_transform   = tf;
                best_inliers     = tight_inliers;
                no_improve_count = 0;
                g_ransac_logger.infof(
                    "prosac greedy[%d,%d] s=%.4f theta=%.4f deg inliers=%d(wide=%d) RMS=%.4f px",
                    a, b, tf.s, tf.theta * deg, n_tight, n_inliers, rms_tight);
            }

            // 早停: 找到足够好解 (: good_rms_threshold 现在是像素单位)
            if (best_n_inliers >= 4 && best_RMS < params.good_rms_threshold) {
                g_ransac_logger.infof("prosac_verify: 早停 (n_inliers=%d, RMS=%.4f < %.2f px)",
                                      best_n_inliers, best_RMS, params.good_rms_threshold);
                early_stop = true;
                break;
            }
        }
    }
    g_ransac_logger.infof("prosac_verify: 贪心阶段完成 (K=%d, 尝试 %d 对, best_inliers=%d)",
                          K_greedy, greedy_pairs, best_n_inliers);

    // === 第二阶段: top-1 锚点固定枚举 ( 新增) ===
    // 固定 candidates[0] (最高票), 枚举 candidates[1..M-1] 作为第二锚点
    // 动机: NGC55_T3_01 max_vote=26 但 top-5 候选 theta 差异巨大 (-100° vs -63°),
    // 说明 top-5 中混入错误匹配。锚点固定枚举确保最高票候选被尝试所有组合,
    // 如果最高票是真实匹配, 正确变换必被找到。
    // 计算量: M-1 次求解, 每次 O(M) 内点验证, 总计 O(M²) ~ 22500, 非常快
    if (!early_stop) {
        const int u1 = candidates[0].u_idx, w1 = candidates[0].w_idx;
        int anchor_pairs = 0;
        int anchor_valid = 0;
        for (int b = 1; b < M; ++b) {
            const int u2 = candidates[b].u_idx, w2 = candidates[b].w_idx;

            // 退化检查
            const double dx = U[u1].x - U[u2].x;
            const double dy = U[u1].y - U[u2].y;
            const double dist_img = std::sqrt(dx * dx + dy * dy);
            if (dist_img < 10.0) continue;

            // 跳过相同 u 的配对
            if (u1 == u2) continue;

            const SimTransform tf = solve_similarity_transform(U, W, u1, w1, u2, w2);
            if (!tf.valid) continue;
            if (tf.s < params.s_min || tf.s > params.s_max) continue;
            anchor_valid++;

            // 内点验证
            const double cos_t = std::cos(tf.theta);
            const double sin_t = std::sin(tf.theta);
            std::vector<MatchPair> inliers;
            inliers.reserve(M);
            double sum_sq = 0.0;
            for (int k = 0; k < M; ++k) {
                const int u = candidates[k].u_idx;
                const int w = candidates[k].w_idx;
                const double x_pred = tf.s * (cos_t * U[u].x - sin_t * U[u].y) + tf.tx;
                const double y_pred = tf.s * (sin_t * U[u].x + cos_t * U[u].y) + tf.ty;
                const double rx = W[w].x - x_pred;
                const double ry = W[w].y - y_pred;
                const double r = std::sqrt(rx * rx + ry * ry);
                if (r < tau) {
                    inliers.push_back({u, w});
                    sum_sq += r * r;
                }
            }
            const int n_inliers = (int)inliers.size();
            const double rms = (n_inliers > 0) ? std::sqrt(sum_sq / n_inliers) : 1e9;
            anchor_pairs++;

            if (n_inliers > best_n_inliers ||
                (n_inliers == best_n_inliers && rms < best_RMS)) {
                // 渐进收紧验证
                std::vector<MatchPair> tight_inliers;
                double tight_sum_sq = 0.0;
                for (int k = 0; k < M; ++k) {
                    const int u = candidates[k].u_idx;
                    const int w = candidates[k].w_idx;
                    const double x_pred = tf.s * (cos_t * U[u].x - sin_t * U[u].y) + tf.tx;
                    const double y_pred = tf.s * (sin_t * U[u].x + cos_t * U[u].y) + tf.ty;
                    const double rx = W[w].x - x_pred;
                    const double ry = W[w].y - y_pred;
                    const double r = std::sqrt(rx * rx + ry * ry);
                    if (r < tau_tight) {
                        tight_inliers.push_back({u, w});
                        tight_sum_sq += r * r;
                    }
                }
                const int n_tight = (int)tight_inliers.size();
                const double rms_tight = (n_tight > 0) ? std::sqrt(tight_sum_sq / n_tight) : 1e9;

                best_n_inliers   = n_tight;
                best_RMS         = rms_tight;
                best_transform   = tf;
                best_inliers     = tight_inliers;
                no_improve_count = 0;
                g_ransac_logger.infof(
                    "prosac anchor[0,%d] s=%.4f theta=%.4f deg inliers=%d(wide=%d) RMS=%.4f px",
                    b, tf.s, tf.theta * deg, n_tight, n_inliers, rms_tight);
            }

            // 早停: 找到足够好解
            if (best_n_inliers >= 4 && best_RMS < params.good_rms_threshold) {
                g_ransac_logger.infof("prosac_verify: 早停 (n_inliers=%d, RMS=%.4f < %.2f px)",
                                      best_n_inliers, best_RMS, params.good_rms_threshold);
                break;
            }
        }
        g_ransac_logger.infof("prosac_verify: 锚点固定枚举完成 (尝试 %d 对, 有效 %d, best_inliers=%d)",
                              anchor_pairs, anchor_valid, best_n_inliers);

        // 阶段间检查: 满足早停条件则跳过第三阶段
        if (best_n_inliers >= 4 && best_RMS < params.good_rms_threshold) {
            early_stop = true;
        }
    }

    // === 第三阶段: 随机采样 (原 PROSAC 逻辑) ===
    if (!early_stop) {
    for (int iter = 0; iter < max_iter; ++iter) {
        result.n_iterations = iter + 1;

        // a. 从前 T 个候选中随机采样 2 对不同匹配
        int i1 = -1, i2 = -1;
        int sample_attempts = 0;
        bool sample_ok = false;
        while (sample_attempts < 50) {
            std::uniform_int_distribution<int> dist(0, T - 1);
            const int a = dist(gen);
            const int b = dist(gen);
            if (a == b) { sample_attempts++; continue; }

            const int u1 = candidates[a].u_idx;
            const int u2 = candidates[b].u_idx;

            // b. 退化检查: 两图像点距离 < 10" 重采
            const double dx = U[u1].x - U[u2].x;
            const double dy = U[u1].y - U[u2].y;
            const double dist_img = std::sqrt(dx * dx + dy * dy);
            if (dist_img < 10.0) { sample_attempts++; continue; }

            i1 = a; i2 = b;
            sample_ok = true;
            break;
        }
        if (!sample_ok) continue;

        const int u1 = candidates[i1].u_idx, w1 = candidates[i1].w_idx;
        const int u2 = candidates[i2].u_idx, w2 = candidates[i2].w_idx;

        // c. 求解相似变换
        const SimTransform tf = solve_similarity_transform(U, W, u1, w1, u2, w2);
        if (!tf.valid) continue;

        // d. 尺度约束
        if (tf.s < params.s_min || tf.s > params.s_max) continue;

        // e. 内点验证: 对所有候选匹配计算残差
        const double cos_t = std::cos(tf.theta);
        const double sin_t = std::sin(tf.theta);
        std::vector<MatchPair> inliers;
        inliers.reserve(M);
        double sum_sq = 0.0;
        for (int k = 0; k < M; ++k) {
            const int u = candidates[k].u_idx;
            const int w = candidates[k].w_idx;
            const double x_pred = tf.s * (cos_t * U[u].x - sin_t * U[u].y) + tf.tx;
            const double y_pred = tf.s * (sin_t * U[u].x + cos_t * U[u].y) + tf.ty;
            const double rx = W[w].x - x_pred;
            const double ry = W[w].y - y_pred;
            const double r = std::sqrt(rx * rx + ry * ry);
            if (r < tau) {
                inliers.push_back({u, w});
                sum_sq += r * r;
            }
        }
        const int n_inliers = (int)inliers.size();
        const double rms = (n_inliers > 0) ? std::sqrt(sum_sq / n_inliers) : 1e9;

        // f. 更新最优 (内点数优先, 内点数相同时取 RMS 更小者)
        if (n_inliers > best_n_inliers ||
            (n_inliers == best_n_inliers && rms < best_RMS)) {
            // 渐进收紧验证
            std::vector<MatchPair> tight_inliers;
            double tight_sum_sq = 0.0;
            for (int k = 0; k < M; ++k) {
                const int u = candidates[k].u_idx;
                const int w = candidates[k].w_idx;
                const double x_pred = tf.s * (cos_t * U[u].x - sin_t * U[u].y) + tf.tx;
                const double y_pred = tf.s * (sin_t * U[u].x + cos_t * U[u].y) + tf.ty;
                const double rx = W[w].x - x_pred;
                const double ry = W[w].y - y_pred;
                const double r = std::sqrt(rx * rx + ry * ry);
                if (r < tau_tight) {
                    tight_inliers.push_back({u, w});
                    tight_sum_sq += r * r;
                }
            }
            const int n_tight = (int)tight_inliers.size();
            const double rms_tight = (n_tight > 0) ? std::sqrt(tight_sum_sq / n_tight) : 1e9;

            best_n_inliers   = n_tight;
            best_RMS         = rms_tight;
            best_transform   = tf;
            best_inliers     = tight_inliers;
            no_improve_count = 0;

            // 前 20 次或每 50 次改进记录日志
            if (iter < 20 || iter % 50 == 0) {
                g_ransac_logger.infof(
                    "prosac iter=%d T=%d s=%.4f theta=%.4f deg inliers=%d(wide=%d) RMS=%.4f px",
                    iter, T, tf.s, tf.theta * deg, n_tight, n_inliers, rms_tight);
            }
        } else {
            no_improve_count++;
        }

        // g. 每 10 次迭代扩展池
        if ((iter + 1) % 10 == 0) {
            const int expand = std::max(1, M / 20);
            const int T_old = T;
            T = std::min(T + expand, M);
            if (T != T_old) {
                g_ransac_logger.debugf("prosac iter=%d 池扩展 %d -> %d", iter + 1, T_old, T);
            }
        }

        // h. 提前终止: 连续 100 次无改进且 T >= |M|
        if (no_improve_count >= 100 && T >= M) {
            g_ransac_logger.infof(
                "prosac_verify: 提前终止 (iter=%d, 100 次无改进, T=%d)",
                iter + 1, T);
            break;
        }

        // 早停: 找到足够好解
        if (best_n_inliers >= 4 && best_RMS < params.good_rms_threshold) {
            g_ransac_logger.infof("prosac_verify: 早停 (n_inliers=%d, RMS=%.4f < %.2f px)",
                                  best_n_inliers, best_RMS, params.good_rms_threshold);
            break;
        }
    }
    }

    g_ransac_logger.infof(
        "prosac_verify: 迭代结束 (n_iter=%d, best_inliers=%d, best_RMS=%.4f px)",
        result.n_iterations, best_n_inliers, best_RMS);

    // 4. Umeyama 完整估计 (内点数 >= 4)
    if (best_n_inliers >= 4) {
        const SimTransform ume = umeyama_estimate(U, W, best_inliers);
        if (ume.valid) {
            // 重新计算 RMS (用 Umeyama 结果)
            const double cos_t = std::cos(ume.theta);
            const double sin_t = std::sin(ume.theta);
            double sum_sq = 0.0;
            for (const auto& p : best_inliers) {
                const double x_pred = ume.s * (cos_t * U[p.u].x - sin_t * U[p.u].y) + ume.tx;
                const double y_pred = ume.s * (sin_t * U[p.u].x + cos_t * U[p.u].y) + ume.ty;
                const double rx = W[p.w].x - x_pred;
                const double ry = W[p.w].y - y_pred;
                sum_sq += rx * rx + ry * ry;
            }
            best_RMS = std::sqrt(sum_sq / best_inliers.size());
            best_transform = ume;

            g_ransac_logger.infof(
                "prosac_verify: Umeyama 精化 s=%.6f theta=%.6f deg RMS=%.4f px",
                ume.s, ume.theta * 57.295779513082323, best_RMS);
        } else {
            g_ransac_logger.warn("prosac_verify: Umeyama 估计失败, 保留 PROSAC 最优变换");
        }
    }

    // 5. 组装结果 (: 5.0 现在是像素单位)
    result.transform    = best_transform;
    result.inliers      = best_inliers;
    result.rms          = best_RMS;
    result.n_inliers    = best_n_inliers;
    result.score        = best_n_inliers / (1.0 + best_RMS);
    result.success      = (best_n_inliers >= 4 && best_RMS < 5.0);

    g_ransac_logger.infof(
        "prosac_verify: 完成 (n_inliers=%d, RMS=%.4f px, score=%.4f, success=%d)",
        result.n_inliers, result.rms, result.score, result.success ? 1 : 0);

    return result;
}

// ===========================================================================
// full_verify_transform: 全量验证 ( 新增)
//
// 用 PROSAC 最优变换对所有 U 中星点预测 W 中位置, 找最近邻 w,
// 若距离 < tau 则加入内点。解决 PROSAC 只验证 candidates 导致遗漏真实匹配的问题。
//
// 算法: O(N_U × N_W) 暴力最近邻
// 去重: 每个 w 只能匹配一个 u (取距离最小的)
// ===========================================================================
std::vector<MatchPair> full_verify_transform(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const SimTransform& tf,
    double tau)
{
    std::vector<MatchPair> inliers;

    if (!tf.valid) return inliers;

    const int N_U = (int)U.size();
    const int N_W = (int)W.size();
    if (N_U == 0 || N_W == 0) return inliers;

    const double cos_t = std::cos(tf.theta);
    const double sin_t = std::sin(tf.theta);

    // 对每个 u 找最近邻 w
    // 使用 "w 已匹配" 标记避免重复分配
    std::vector<bool> w_used(N_W, false);

    // 先收集所有 (u, best_w, dist) 三元组
    struct UWDist {
        int    u;
        int    w;
        double dist;
    };
    std::vector<UWDist> pairs;
    pairs.reserve(N_U);

    for (int u = 0; u < N_U; ++u) {
        const double x_pred = tf.s * (cos_t * U[u].x - sin_t * U[u].y) + tf.tx;
        const double y_pred = tf.s * (sin_t * U[u].x + cos_t * U[u].y) + tf.ty;

        int    best_w   = -1;
        double best_dist = 1e18;
        for (int w = 0; w < N_W; ++w) {
            const double dx = W[w].x - x_pred;
            const double dy = W[w].y - y_pred;
            const double d  = dx * dx + dy * dy;  // 用平方距离避免 sqrt
            if (d < best_dist) {
                best_dist = d;
                best_w    = w;
            }
        }

        if (best_w >= 0 && best_dist < tau * tau) {
            pairs.push_back({u, best_w, std::sqrt(best_dist)});
        }
    }

    // 按距离升序排序, 贪心分配 w (避免一个 w 匹配多个 u)
    std::sort(pairs.begin(), pairs.end(),
              [](const UWDist& a, const UWDist& b) { return a.dist < b.dist; });

    for (const auto& p : pairs) {
        if (w_used[p.w]) continue;
        w_used[p.w] = true;
        inliers.push_back({p.u, p.w});
    }

    g_ransac_logger.infof("full_verify_transform: N_U=%d, N_W=%d, tau=%.2f\", "
                          "candidate_pairs=%zu, inliers=%zu",
                          N_U, N_W, tau, pairs.size(), inliers.size());

    return inliers;
}

// ===========================================================================
// iter_trans_verify — 鲁棒线性 TRANS 拟合 (PROSAC 备选路径)
//
// 算法:
// 1. 取票数最高的 6 对 (AT_MATCH_STARTN_LINEAR=6) 起始拟合 6 参数线性 TRANS
// 2. 6 参数仿射变换:
// W.x = a00*U.x + a01*U.y + tx
// W.y = a10*U.x + a11*U.y + ty
// 拆分为两个独立的 3x3 正规方程 (x-row 与 y-row 独立):
// For x-row: [Σu.x², Σu.x*u.y, Σu.x; Σu.x*u.y, Σu.y², Σu.y; Σu.x, Σu.y, N] · [a00; a01; tx] = [Σu.x*w.x; Σu.y*w.x; Σw.x]
// For y-row: 同上 but RHS = [Σu.x*w.y; Σu.y*w.y; Σw.y]
// 3. 计算所有候选对残差, 取 35% 百分位作为有效 sigma
// 4. 剔除残差 > 10*sigma 的星对, 用剩余星对重新拟合
// 5. 重复 5 次或直到残差变化 < 10%
// 6. 用最终 TRANS 做全量匹配 (tau 半径)
// 7. 转换为 SimTransform: s = sqrt(|a00*a11 - a01*a10|), θ = atan2(a10, a00)
// ===========================================================================
namespace {

// 解 3x3 线性方程组 A·x = b (Cramer 法则, 行列式不为零时有效)
// A row-major: [a00, a01, a02; a10, a11, a12; a20, a21, a22]
// 返回 true 表示成功, false 表示奇异
bool solve_3x3(const double A[9], const double b[3], double x[3]) {
    // 行列式展开
    double det = A[0] * (A[4] * A[8] - A[5] * A[7])
               - A[1] * (A[3] * A[8] - A[5] * A[6])
               + A[2] * (A[3] * A[7] - A[4] * A[6]);
    if (std::abs(det) < 1e-15) return false;

    // Cramer 法则
    double Ax[9] = {b[0], A[1], A[2], b[1], A[4], A[5], b[2], A[7], A[8]};
    double Ay[9] = {A[0], b[0], A[2], A[3], b[1], A[5], A[6], b[2], A[8]};
    double Az[9] = {A[0], A[1], b[0], A[3], A[4], b[1], A[6], A[7], b[2]};

    auto det3 = [](const double M[9]) {
        return M[0] * (M[4] * M[8] - M[5] * M[7])
             - M[1] * (M[3] * M[8] - M[5] * M[6])
             + M[2] * (M[3] * M[7] - M[4] * M[6]);
    };

    x[0] = det3(Ax) / det;
    x[1] = det3(Ay) / det;
    x[2] = det3(Az) / det;
    return true;
}

// 6 参数线性 TRANS 结构
struct LinearTrans {
    double a00, a01, tx;   // W.x = a00*U.x + a01*U.y + tx
    double a10, a11, ty;   // W.y = a10*U.x + a11*U.y + ty
    bool   valid;
};

// 用 N 对匹配拟合 6 参数线性 TRANS (过约束最小二乘)
// 输入 pairs: (u_idx, w_idx) 对
LinearTrans fit_linear_trans(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& pairs)
{
    LinearTrans lt{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, false};
    const int n = (int)pairs.size();
    if (n < 3) return lt;  // 至少 3 对才能解 3x3

    // 构建正规方程 A^T A x = A^T b (两个独立 3x3 系统)
    // 系数矩阵 (x-row 和 y-row 共用):
    // [Σu.x², Σu.x*u.y, Σu.x;
    // Σu.x*u.y, Σu.y², Σu.y;
    // Σu.x, Σu.y, N]
    double M[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    double bx[3] = {0, 0, 0};
    double by[3] = {0, 0, 0};

    for (int i = 0; i < n; ++i) {
        const int u = pairs[i].u;
        const int w = pairs[i].w;
        const double ux = U[u].x, uy = U[u].y;
        const double wx = W[w].x, wy = W[w].y;

        M[0] += ux * ux; M[1] += ux * uy; M[2] += ux;
        M[3] += ux * uy; M[4] += uy * uy; M[5] += uy;
        M[6] += ux;      M[7] += uy;      M[8] += 1.0;

        bx[0] += ux * wx; bx[1] += uy * wx; bx[2] += wx;
        by[0] += ux * wy; by[1] += uy * wy; by[2] += wy;
    }

    // 解 3x3 正规方程
    double sol_x[3], sol_y[3];
    if (!solve_3x3(M, bx, sol_x) || !solve_3x3(M, by, sol_y)) {
        return lt;  // 奇异矩阵
    }

    lt.a00 = sol_x[0]; lt.a01 = sol_x[1]; lt.tx = sol_x[2];
    lt.a10 = sol_y[0]; lt.a11 = sol_y[1]; lt.ty = sol_y[2];
    lt.valid = true;
    return lt;
}

// 计算一对匹配的残差 (像素)
inline double residual_px(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const LinearTrans& lt,
    int u_idx, int w_idx)
{
    const double ux = U[u_idx].x, uy = U[u_idx].y;
    const double pred_x = lt.a00 * ux + lt.a01 * uy + lt.tx;
    const double pred_y = lt.a10 * ux + lt.a11 * uy + lt.ty;
    const double dx = W[w_idx].x - pred_x;
    const double dy = W[w_idx].y - pred_y;
    return std::sqrt(dx * dx + dy * dy);
}

// ===========================================================================
// IterTransResult - iter_trans_inner 返回值
// ===========================================================================
struct IterTransResult {
    LinearTrans lt;                    // 最终拟合的线性变换
    std::vector<MatchPair> inliers;    // 最终内点 (用于残差计算的星对)
    double sigma;                      // 最终 sigma (35% 百分位)
    int    n_iterations;               // 实际迭代次数
    bool   converged;                  // 是否收敛 (nb==0 或 sigma<=halt_sigma)
    bool   success;                    // 是否成功 (未触发失败条件)
};

// ===========================================================================
// iter_trans_inner: sigma-clip 迭代核心
//
// 算法:
// 1. 初始 TRANS: calc_trans(initial_pairs=6 or nbright, ...) — 6 对 12 方程过约束最小二乘
// (不使用 2 对解析解, 因 2 对精确解会导致 sigma=0 过早收敛)
// 2. 循环 (最多 5 次):
// a) fit_linear_trans(working_set) → lt
// b) 计算所有候选对残差平方 dist2[k] (IPV 扩展: 算全部 M 候选, 原实现只算 nr 个)
// c) 绝对阈值剔除: dist2 > MAX_DIST² -> bad [AT_MATCH_MAXDIST]
// d) sigma = 35% 百分位 (dist² 单位, 四舍五入索引) [find_percentile]
// e) 若 sigma <= HALT_SIGMA: 设 is_ok=true (但不立即退出!) [break 被注释]
// f) 相对阈值剔除: dist2 > min((tau*5)², 10*sigma) → bad [IPV 双阈值]
// g) nb = 剔除数
// h) 若 nb == 0: 设 is_ok=true (但不立即退出!) [break 被注释]
// i) 收集剩余星对 new_set, nr = |new_set|
// j) 若 nr < REQUIRED_PAIRS (3): 失败, 退出 [is_ok=0; break]
// k) working_set = new_set
// l) 若 is_ok: 成功退出 (在循环顶部, 而非 sigma/nb 检查处)
// 3. 达到最大迭代: 保留为候选 (IPV 容错, 原实现视为成功)
//
// 关键修复 :
// - 移除 initial_lt 参数 (从不用 2 对解析解)
// - HALT_SIGMA / nb==0 设 is_ok 标志, 不立即返回
// - find_percentile 用四舍五入 (floor(num*perc+0.5))
// - is_ok 在循环顶部退出 (在 calc_trans 重拟合后)
// ===========================================================================
IterTransResult iter_trans_inner(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& initial_set,
    const std::vector<CandidateMatch>& all_candidates,
    const IPVSolverParams& params,
    bool recalc_flag)
{
    IterTransResult result;
    result.sigma = 1e9;
    result.n_iterations = 0;
    result.converged = false;
    result.success = false;
    result.lt = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, false};

    // 常量定义
    // 注: 坐标系为角秒 (StarPoint.x/y 是角秒坐标, 原实现是像素)
    const int    MAX_ITERS         = 5;       // AT_MATCH_MAXITER
    const double PERCENTILE        = 0.35;    // AT_MATCH_PERCENTILE
    const double SIGMA_CLIP_FACTOR = 10.0;    // AT_MATCH_NSIGMA
    const double MAX_DIST          = 100.0;   // AT_MATCH_MAXDIST (原实现 50px, IPV 100 角秒 ≈ 50px * 2"/px)
    const double HALT_SIGMA        = 0.1;     // AT_MATCH_HALTSIGMA (dist² 单位, 对应 dist<0.316")
    const int    REQUIRED_PAIRS    = 3;       // AT_MATCH_REQUIRE_LINEAR
    const int    N_START           = 6;       // AT_MATCH_STARTN_LINEAR
    const double ABS_CLIP_FACTOR   = 5.0;     // IPV 双阈值: tau * 5

    const double tau = params.ransac_inlier_threshold_arcsec;
    const int M = (int)all_candidates.size();

    // 1. 初始 working_set (RECALC_YES 用全部, RECALC_NO 用前 6 对)
    std::vector<MatchPair> working_set;
    if (recalc_flag) {
        working_set = initial_set;
    } else {
        for (int i = 0; i < N_START && i < (int)initial_set.size(); ++i) {
            working_set.push_back(initial_set[i]);
        }
    }

    if ((int)working_set.size() < REQUIRED_PAIRS) {
        g_ransac_logger.warnf("iter_trans_inner: 初始 working_set=%d < %d, 失败",
                              (int)working_set.size(), REQUIRED_PAIRS);
        return result;
    }

    g_ransac_logger.infof("iter_trans_inner: 开始 (recalc=%d, initial=%d, working=%d, candidates=%d, tau=%.2f)",
                          recalc_flag ? 1 : 0, (int)initial_set.size(),
                          (int)working_set.size(), M, tau);

    // 2. sigma-clip 迭代
    // 关键: dist2 只对 working_set 计算 (不扩展到全部 M 候选)
    // nr = nbright (working_set 大小), dist2 只算 nr 个
    // 全量匹配由 atMatchLists 负责, iter_trans 只负责精化 working_set
    bool is_ok = false;  // is_ok 标志
    const double max_dist2 = MAX_DIST * MAX_DIST;

    for (int iter = 0; iter < MAX_ITERS; ++iter) {
        result.n_iterations = iter + 1;
        int nr = (int)working_set.size();

        // a) 拟合线性 TRANS (始终用 fit_linear_trans, 对应 calc_trans)
        LinearTrans lt = fit_linear_trans(U, W, working_set);
        if (!lt.valid) {
            g_ransac_logger.warnf("iter_trans_inner iter=%d: fit_linear_trans 失败 (奇异矩阵)", iter);
            return result;
        }
        result.lt = lt;

        // b) 计算 working_set 对的残差平方 (只算 nr 个, 不扩展到 M)
        // for (i=0; i<nr; i++) dist2[i] = |a_prime[i] - B[i]|²
        std::vector<double> dist2(nr);
        for (int k = 0; k < nr; ++k) {
            double r = residual_px(U, W, lt,
                                   working_set[k].u,
                                   working_set[k].w);
            dist2[k] = r * r;
        }

        // c) 绝对阈值剔除 (dist2 > max_dist2)
        std::vector<bool> bad(nr, false);
        int nb_abs = 0;
        for (int k = 0; k < nr; ++k) {
            if (dist2[k] > max_dist2) {
                bad[k] = true;
                nb_abs++;
            }
        }

        // d) sigma = 35% 百分位 (find_percentile: 四舍五入)
        std::vector<double> good_d2;
        good_d2.reserve(nr);
        for (int k = 0; k < nr; ++k) {
            if (!bad[k]) good_d2.push_back(dist2[k]);
        }
        if (good_d2.empty()) {
            g_ransac_logger.warnf("iter_trans_inner iter=%d: working_set 所有对残差 > %.1f 角秒, 失败",
                                  iter, MAX_DIST);
            return result;
        }
        std::sort(good_d2.begin(), good_d2.end());
        // index = floor(num * perc + 0.5) - 四舍五入
        int idx_35 = std::min((int)good_d2.size() - 1,
                              (int)std::floor(good_d2.size() * PERCENTILE + 0.5));
        idx_35 = std::max(0, idx_35);
        double sigma = good_d2[idx_35];
        // 注: 6 对最小二乘不会产生 sigma=0 (除非完全共线, fit_linear_trans 会返回 invalid)
        result.sigma = sigma;

        // e) HALT_SIGMA 检查 (设 is_ok=true, 不立即退出!)
        // 源码: if (sigma <= halt_sigma) { is_ok = 1; /* break; 注释掉 */ }
        if (sigma <= HALT_SIGMA) {
            is_ok = true;
            g_ransac_logger.infof("iter_trans_inner iter=%d: sigma=%.6f <= %.1f, 标记 is_ok (继续相对阈值)",
                                  iter, sigma, HALT_SIGMA);
        }

        // f) 相对阈值剔除 (IPV 双阈值, 保留优势: min((tau*5)², 10*sigma))
        // 原实现只有 10*sigma; IPV 加 abs_thresh 防止 sigma 膨胀时误剔
        double rel_thresh_sq = SIGMA_CLIP_FACTOR * sigma;
        double abs_thresh_sq = (tau * ABS_CLIP_FACTOR) * (tau * ABS_CLIP_FACTOR);
        double clip_thresh_sq = std::min(abs_thresh_sq, rel_thresh_sq);
        int nb_rel = 0;
        for (int k = 0; k < nr; ++k) {
            if (!bad[k] && dist2[k] > clip_thresh_sq) {
                bad[k] = true;
                nb_rel++;
            }
        }
        int nb = nb_abs + nb_rel;

        g_ransac_logger.infof("iter_trans_inner iter=%d: nr=%d, sigma_35%%=%.6f 角秒², "
                              "clip_thresh=%.4f (abs_sq=%.1f, rel_sq=%.4f), nb_abs=%d, nb_rel=%d, nb=%d, is_ok=%d",
                              iter, nr, sigma, clip_thresh_sq,
                              abs_thresh_sq, rel_thresh_sq, nb_abs, nb_rel, nb, is_ok ? 1 : 0);

        // g) nb == 0 检查 (设 is_ok=true, 不立即退出!)
        // 源码: if (nb == 0) { is_ok = 1; /* break; 注释掉 */ }
        if (nb == 0) {
            is_ok = true;
        }

        // h) 收集剩余星对 (从 working_set 中保留好的)
        std::vector<MatchPair> new_set;
        new_set.reserve(nr);
        for (int k = 0; k < nr; ++k) {
            if (!bad[k]) {
                new_set.push_back(working_set[k]);
            }
        }
        nr = (int)new_set.size();

        // i) nr < REQUIRED_PAIRS -> 失败 (is_ok=0; break)
        if (nr < REQUIRED_PAIRS) {
            g_ransac_logger.warnf("iter_trans_inner iter=%d: nr=%d < %d, 失败 (不兜底)",
                                  iter, nr, REQUIRED_PAIRS);
            result.success = false;
            return result;
        }

        // j) working_set = 剩余星对 (calc_trans(nr, ...) 重拟合)
        working_set = std::move(new_set);

        // k) is_ok -> 成功退出 (iters_so_far++; if (is_ok) break)
        // 注意: 这里在 working_set 更新后退出, 下次循环会用新的 working_set 重拟合
        // 即 is_ok 时仍然重拟合一次
        // 严格遵循: is_ok 时重拟合一次再退出
        if (is_ok) {
            // 重拟合一次 (calc_trans(nr, ...))
            LinearTrans final_lt = fit_linear_trans(U, W, working_set);
            if (final_lt.valid) {
                result.lt = final_lt;
            }
            result.converged = true;
            result.success = true;
            result.inliers = working_set;
            g_ransac_logger.infof("iter_trans_inner iter=%d: 收敛退出 (is_ok=1), inliers=%d, sigma=%.6f",
                                  iter, (int)result.inliers.size(), sigma);
            break;
        }
    }

    // 3. 达到最大迭代未收敛 (视为成功, 用最后结果)
    if (!is_ok && (int)working_set.size() >= REQUIRED_PAIRS) {
        result.success = true;
        result.converged = false;
        result.inliers = working_set;
        g_ransac_logger.infof("iter_trans_inner: 达到最大迭代 %d 未收敛, sigma=%.6f 角秒², inliers=%d (保留为候选)",
                              MAX_ITERS, result.sigma, (int)result.inliers.size());
    }
    return result;
}

// ===========================================================================
// at_match_lists: 用 TRANS 变换 U, 在 W 中找 radius 内最近邻
//
// 算法:
// 1. 对每个 U[u], 用 lt 变换得到 (pred_x, pred_y)
// 2. 在 W 中找最近邻 (radius 内)
// 3. 收集所有 (u, w, dist) 对
// 4. 按距离升序排序
// 5. 贪心分配 w (避免一个 w 匹配多个 u)
// ===========================================================================
std::vector<MatchPair> at_match_lists(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const LinearTrans& lt,
    double radius)
{
    std::vector<MatchPair> result;
    if (!lt.valid) return result;

    const int N_U = (int)U.size();
    const int N_W = (int)W.size();
    if (N_U == 0 || N_W == 0) return result;

    struct UWDist {
        int    u;
        int    w;
        double dist;
    };
    std::vector<UWDist> pairs;
    pairs.reserve(N_U);

    // 1. 对每个 u 找最近邻 w (在 radius 内)
    for (int u = 0; u < N_U; ++u) {
        const double ux = U[u].x, uy = U[u].y;
        const double pred_x = lt.a00 * ux + lt.a01 * uy + lt.tx;
        const double pred_y = lt.a10 * ux + lt.a11 * uy + lt.ty;

        int    best_w    = -1;
        double best_dist = 1e18;
        for (int w = 0; w < N_W; ++w) {
            const double dx = W[w].x - pred_x;
            const double dy = W[w].y - pred_y;
            const double d  = dx * dx + dy * dy;  // 用平方距离避免 sqrt
            if (d < best_dist) {
                best_dist = d;
                best_w    = w;
            }
        }
        if (best_w >= 0 && best_dist < radius * radius) {
            pairs.push_back({u, best_w, std::sqrt(best_dist)});
        }
    }

    // 4. 按距离升序排序
    std::sort(pairs.begin(), pairs.end(),
              [](const UWDist& a, const UWDist& b) { return a.dist < b.dist; });

    // 5. 贪心分配 w (避免一个 w 匹配多个 u)
    std::vector<bool> w_used(N_W, false);
    result.reserve(pairs.size());
    for (const auto& p : pairs) {
        if (w_used[p.w]) continue;
        w_used[p.w] = true;
        result.push_back({p.u, p.w});
    }

    g_ransac_logger.infof("at_match_lists: N_U=%d, N_W=%d, radius=%.2f px, "
                          "candidates=%zu, matched=%zu",
                          N_U, N_W, radius, pairs.size(), result.size());
    return result;
}

} // anonymous namespace

PROSACResult iter_trans_verify(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<CandidateMatch>& candidates,
    const IPVSolverParams& params)
{
    PROSACResult result;
    result.transform     = {1.0, 0.0, 0.0, 0.0, false};
    result.rms           = 1e9;
    result.n_inliers     = 0;
    result.n_iterations  = 0;
    result.score         = 0.0;
    result.success       = false;

    // 1. 按 vote 降序排序候选对 (top_vote_getters 降序)
    std::vector<CandidateMatch> sorted_cands = candidates;
    std::sort(sorted_cands.begin(), sorted_cands.end(),
              [](const CandidateMatch& a, const CandidateMatch& b) {
                  return a.vote > b.vote;
              });

    const int M = (int)sorted_cands.size();
    if (M < 3) {
        g_ransac_logger.warnf("iter_trans_verify: 候选数 %d < 3", M);
        return result;
    }

    // 2. 多组采样 (IPV 优势, 保留): 3 组, 每组 6 对
    // 单组 top 6 对, 若包含错配则失败; IPV 3 组提高成功率
    // 移除原 2 对预验证 (C(20,2)=190 组合), 直接用 6 对最小二乘
    const int N_GROUPS = 3;
    const int N_START  = 6;  // AT_MATCH_STARTN_LINEAR

    g_ransac_logger.infof("iter_trans_verify: 开始 (M=%d, N_GROUPS=%d, N_START=%d, tau=%.2f 角秒, s=[%.3f,%.3f])",
                          M, N_GROUPS, N_START, params.ransac_inlier_threshold_arcsec,
                          params.s_min, params.s_max);

    IterTransResult best_result;
    bool has_best = false;
    int total_iterations = 0;
    int best_inliers = 0;

    for (int g = 0; g < N_GROUPS; ++g) {
        int start = g * N_START;
        if (start + N_START > M) {
            // 候选不足以凑齐第 g 组
            g_ransac_logger.infof("iter_trans_verify: 组 %d 起始 %d + %d > M=%d, 跳过",
                                  g, start, N_START, M);
            break;
        }

        // 构造 initial_set: 第 g 组的 6 对 (vote 排名 start 到 start+5)
        std::vector<MatchPair> initial_set;
        initial_set.reserve(N_START);
        for (int i = start; i < start + N_START; ++i) {
            initial_set.push_back({sorted_cands[i].u_idx, sorted_cands[i].w_idx});
        }

        g_ransac_logger.infof("iter_trans_verify: 组 %d (vote 排名 %d-%d), vote=[%.2f~%.2f]",
                              g, start, start + N_START - 1,
                              sorted_cands[start].vote, sorted_cands[start + N_START - 1].vote);

        // 调用 iter_trans_inner (无 initial_lt)
        IterTransResult itr = iter_trans_inner(U, W, initial_set,
                                               sorted_cands, params, false);
        total_iterations += itr.n_iterations;

        if (itr.success) {
            // 选优逻辑 (IPV 优势): 内点数优先, sigma 次之
            bool better = false;
            if (!has_best) {
                better = true;
            } else if ((int)itr.inliers.size() > best_inliers) {
                better = true;
            } else if ((int)itr.inliers.size() == best_inliers && itr.sigma < best_result.sigma) {
                better = true;
            }
            if (better) {
                best_result = itr;
                best_inliers = (int)itr.inliers.size();
                has_best = true;
                g_ransac_logger.infof("iter_trans_verify 组 %d 成为最优: inliers=%d, sigma=%.6f 角秒², "
                                      "n_iter=%d, converged=%d",
                                      g, best_inliers, itr.sigma, itr.n_iterations,
                                      itr.converged ? 1 : 0);
            }
        }

        // 早停: 内点 >= 15 且 sigma < 5.0 (足够好的解)
        if (has_best && best_inliers >= 15 && best_result.sigma < 5.0) {
            g_ransac_logger.infof("iter_trans_verify: 早停 (inliers=%d, sigma=%.6f)",
                                  best_inliers, best_result.sigma);
            break;
        }
    }

    g_ransac_logger.infof("iter_trans_verify: 多组采样完成, has_best=%d, best_inliers=%d, best_sigma=%.6f 角秒²",
                          has_best ? 1 : 0, best_inliers,
                          has_best ? best_result.sigma : -1.0);

    if (!has_best) {
        g_ransac_logger.warnf("iter_trans_verify: 所有 %d 组采样均失败", N_GROUPS);
        result.n_iterations = total_iterations;
        return result;
    }

    // 3. atMatchLists 全量匹配 (用最优 TRANS) [P3 吸纳]
    const double tau = params.ransac_inlier_threshold_arcsec;
    std::vector<MatchPair> matched = at_match_lists(U, W, best_result.lt, tau);

    if ((int)matched.size() < 3) {
        g_ransac_logger.warnf("iter_trans_verify: 全量匹配对 %d < 3, 失败",
                              (int)matched.size());
        result.n_iterations = total_iterations;
        return result;
    }

    // 4. atRecalcTrans 二轮精化 (用匹配对重新 iter_trans) [P3 吸纳]
    // recalc_flag=true: 用全部匹配对 (不是只有 6 对)
    std::vector<CandidateMatch> matched_as_cands;
    matched_as_cands.reserve(matched.size());
    for (const auto& mp : matched) {
        CandidateMatch cm;
        cm.u_idx      = mp.u;
        cm.w_idx      = mp.w;
        cm.vote       = 100.0;
        cm.confidence = 1.0;
        matched_as_cands.push_back(cm);
    }

    IterTransResult refined = iter_trans_inner(U, W, matched, matched_as_cands,
                                               params, true);
    total_iterations += refined.n_iterations;

    LinearTrans final_lt = best_result.lt;
    if (refined.success) {
        final_lt = refined.lt;
        // 5. 全量匹配 (第二轮, 用精化后的 TRANS)
        matched = at_match_lists(U, W, final_lt, tau);
        g_ransac_logger.infof("iter_trans_verify: 二轮精化成功 sigma=%.6f→%.6f 角秒², "
                              "matched=%d (第二轮)",
                              best_result.sigma, refined.sigma, (int)matched.size());
    } else {
        g_ransac_logger.warn("iter_trans_verify: 二轮精化失败, 保留一轮结果");
    }

    if ((int)matched.size() < 3) {
        g_ransac_logger.warnf("iter_trans_verify: 二轮匹配对 %d < 3, 失败",
                              (int)matched.size());
        result.n_iterations = total_iterations;
        return result;
    }

    // 6. 转换线性 TRANS → SimTransform
    // 相似变换是仿射变换的特例:
    // a00 = s*cos(θ), a01 = -s*sin(θ)
    // a10 = s*sin(θ), a11 = s*cos(θ)
    // s = sqrt(|a00*a11 - a01*a10|), θ = atan2(a10, a00)
    double det = final_lt.a00 * final_lt.a11 - final_lt.a01 * final_lt.a10;
    double s_est = std::sqrt(std::abs(det));
    double theta_est = std::atan2(final_lt.a10, final_lt.a00);

    SimTransform sim_tf;
    sim_tf.s     = s_est;
    sim_tf.theta = theta_est;
    // 用质心法计算平移 (与 Umeyama 风格一致)
    // W = s·R(θ)·U + t ⇒ t = mean(W) - s·R(θ)·mean(U)
    if (!matched.empty()) {
        double mu_Ux = 0, mu_Uy = 0, mu_Wx = 0, mu_Wy = 0;
        for (const auto& mp : matched) {
            mu_Ux += U[mp.u].x; mu_Uy += U[mp.u].y;
            mu_Wx += W[mp.w].x; mu_Wy += W[mp.w].y;
        }
        mu_Ux /= matched.size(); mu_Uy /= matched.size();
        mu_Wx /= matched.size(); mu_Wy /= matched.size();
        const double cos_t = std::cos(theta_est);
        const double sin_t = std::sin(theta_est);
        sim_tf.tx = mu_Wx - s_est * (cos_t * mu_Ux - sin_t * mu_Uy);
        sim_tf.ty = mu_Wy - s_est * (sin_t * mu_Ux + cos_t * mu_Uy);
    } else {
        sim_tf.tx = final_lt.tx;
        sim_tf.ty = final_lt.ty;
    }
    sim_tf.valid = true;

    // 7. Umeyama 精化 (内点 >= 4 时) [保留 IPV 优势]
    if ((int)matched.size() >= 4) {
        SimTransform ume = umeyama_estimate(U, W, matched);
        if (ume.valid && ume.s >= params.s_min && ume.s <= params.s_max) {
            g_ransac_logger.infof("iter_trans_verify Umeyama 精化: s=%.6f→%.6f, "
                                  "theta=%.4f→%.4f deg",
                                  s_est, ume.s,
                                  theta_est * 57.295779513082323,
                                  ume.theta * 57.295779513082323);
            sim_tf = ume;
        } else if (ume.valid) {
            g_ransac_logger.warnf("iter_trans_verify Umeyama 尺度 %.4f 越界 [%.2f,%.2f], 保留线性 TRANS",
                                  ume.s, params.s_min, params.s_max);
        }
    }

    // 8. 尺度约束检查 [保留 IPV 优势]
    bool s_in_range = (sim_tf.s >= params.s_min && sim_tf.s <= params.s_max);

    // 9. 计算 RMS (用最终 sim_tf)
    double sum_sq = 0.0;
    const double cos_t = std::cos(sim_tf.theta);
    const double sin_t = std::sin(sim_tf.theta);
    for (const auto& mp : matched) {
        const double x_pred = sim_tf.s * (cos_t * U[mp.u].x - sin_t * U[mp.u].y) + sim_tf.tx;
        const double y_pred = sim_tf.s * (sin_t * U[mp.u].x + cos_t * U[mp.u].y) + sim_tf.ty;
        const double rx = W[mp.w].x - x_pred;
        const double ry = W[mp.w].y - y_pred;
        sum_sq += rx * rx + ry * ry;
    }
    double rms = (matched.size() > 0) ? std::sqrt(sum_sq / matched.size()) : 1e9;

    // 10. 组装结果
    result.transform    = sim_tf;
    result.inliers      = matched;
    result.rms          = rms;
    result.n_inliers    = (int)matched.size();
    result.n_iterations = total_iterations;
    result.score        = s_in_range ? (double)matched.size() / (1.0 + rms) : 0.0;
    result.success      = s_in_range && (int)matched.size() >= 4 && rms < 50.0 && sim_tf.valid;

    g_ransac_logger.infof("iter_trans_verify: 完成 inliers=%d, RMS=%.4f 角秒, "
                          "s=%.4f (range[%.2f,%.2f] %s), theta=%.4f deg, "
                          "n_iter=%d (一轮=%d+二轮=%d), sigma_best=%.6f 角秒², "
                          "score=%.4f, success=%d",
                          result.n_inliers, rms, sim_tf.s,
                          params.s_min, params.s_max, s_in_range ? "OK" : "FAIL",
                          sim_tf.theta * 57.295779513082323,
                          total_iterations,
                          best_result.n_iterations,
                          refined.success ? refined.n_iterations : 0,
                          best_result.sigma,
                          result.score, result.success ? 1 : 0);

    return result;
}

} // namespace ipv
