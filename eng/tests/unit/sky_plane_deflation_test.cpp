// eng/tests/unit/sky_plane_deflation_test.cpp
//
// #80 P0-A1 — 生产罚法方程求解器（deflation）单元测试。
//
// 被测 = lib/algorithms/coverage/src/sky_plane.cpp 内的 sky_penalized_solve
// （经 acsd_sky_plane_penalized_solve_for_test 接缝驱动，**同一编译产物、同一实现**）。
//
// 判据与对拍来源（不许来自被测实现自身）：
//   · 期望的 D 秩/零空间维数来自 S3-R 独立 Python 结果
//     （实验/innovation-closure/S3R_SCIENCE_REPORT_2026-10-18.md：
//      完整 7×6 → n_free 42 / rank 38 / nullity 4；双 3×3 岛 → 18/10/8；
//      缺整行 → 35/27/8；单缺点 → 41/37/4）；本测试用**独立的**全主元 Gauss
//      消元在 D 上重新求秩（不复用生产任何分解）。
//   · 参考解 = 独立实现的全主元 Gauss 消元（与 chol_spd/deflation 无关）。
//   · 「旧 Q-only 逆修正」按代数定义在本文件内独立实现，用于证明其残差非零
//     （修复前反例）而生产 deflation 残差在算术地板上。
//   · 出现/不出现某个 status 由上述独立读数判定，不由实现自报。
//
// 用法: sky_plane_deflation_test [--dump <生产 matrix dump json>] [--out <readings.json>]
// 退出码 = 失败断言数（0 = 全绿）。
#include "sky_plane_test_hooks.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

int g_fail = 0;
json g_readings = json::array();

void check(bool ok, const std::string& what, const std::string& detail = "") {
    if (!ok) {
        ++g_fail;
        std::fprintf(stderr, "FAIL: %s  %s\n", what.c_str(), detail.c_str());
    } else {
        std::fprintf(stderr, "ok  : %s  %s\n", what.c_str(), detail.c_str());
    }
}

// ---- 独立线性代数（不引用被测实现）----------------------------------------
// 全主元 Gauss 消元求秩（容差 = rtol·max|a|）。
int gauss_rank(std::vector<double> M, int rows, int cols, double rtol = 1e-11) {
    double mx = 0.0;
    for (double v : M) mx = std::max(mx, std::fabs(v));
    if (mx == 0.0) return 0;
    const double tol = rtol * mx;
    int rank = 0;
    std::vector<int> col_used(cols, 0);
    for (int step = 0; step < std::min(rows, cols); ++step) {
        int br = -1, bc = -1;
        double best = tol;
        for (int i = step; i < rows; ++i)
            for (int j = 0; j < cols; ++j) {
                if (col_used[static_cast<std::size_t>(j)]) continue;
                const double v = std::fabs(M[static_cast<std::size_t>(i) * cols + j]);
                if (v > best) { best = v; br = i; bc = j; }
            }
        if (br < 0) break;
        col_used[static_cast<std::size_t>(bc)] = 1;
        if (br != step)
            for (int j = 0; j < cols; ++j)
                std::swap(M[static_cast<std::size_t>(step) * cols + j],
                          M[static_cast<std::size_t>(br) * cols + j]);
        const double piv = M[static_cast<std::size_t>(step) * cols + bc];
        for (int i = 0; i < rows; ++i) {
            if (i == step) continue;
            const double f = M[static_cast<std::size_t>(i) * cols + bc] / piv;
            if (f == 0.0) continue;
            for (int j = 0; j < cols; ++j)
                M[static_cast<std::size_t>(i) * cols + j] -=
                    f * M[static_cast<std::size_t>(step) * cols + j];
        }
        ++rank;
    }
    return rank;
}

// 全主元 Gauss 消元解方阵（独立参考解）。
bool gauss_solve(std::vector<double> A, std::vector<double> b, int n,
                 std::vector<double>* x, double* rel_resid) {
    std::vector<int> perm(n);
    for (int i = 0; i < n; ++i) perm[i] = i;
    for (int k = 0; k < n; ++k) {
        int br = k, bc = k;
        double best = 0.0;
        for (int i = k; i < n; ++i)
            for (int j = k; j < n; ++j) {
                const double v = std::fabs(A[static_cast<std::size_t>(i) * n + j]);
                if (v > best) { best = v; br = i; bc = j; }
            }
        if (best == 0.0) return false;
        if (br != k) {
            for (int j = 0; j < n; ++j) std::swap(A[static_cast<std::size_t>(k) * n + j],
                                                  A[static_cast<std::size_t>(br) * n + j]);
            std::swap(b[static_cast<std::size_t>(k)], b[static_cast<std::size_t>(br)]);
        }
        if (bc != k) {
            for (int i = 0; i < n; ++i) std::swap(A[static_cast<std::size_t>(i) * n + k],
                                                  A[static_cast<std::size_t>(i) * n + bc]);
            std::swap(perm[static_cast<std::size_t>(k)], perm[static_cast<std::size_t>(bc)]);
        }
        const double piv = A[static_cast<std::size_t>(k) * n + k];
        for (int i = k + 1; i < n; ++i) {
            const double f = A[static_cast<std::size_t>(i) * n + k] / piv;
            if (f == 0.0) continue;
            for (int j = k; j < n; ++j)
                A[static_cast<std::size_t>(i) * n + j] -=
                    f * A[static_cast<std::size_t>(k) * n + j];
            b[static_cast<std::size_t>(i)] -= f * b[static_cast<std::size_t>(k)];
        }
    }
    std::vector<double> y(n, 0.0);
    for (int i = n - 1; i >= 0; --i) {
        double s = b[static_cast<std::size_t>(i)];
        for (int j = i + 1; j < n; ++j)
            s -= A[static_cast<std::size_t>(i) * n + j] * y[static_cast<std::size_t>(j)];
        y[static_cast<std::size_t>(i)] = s / A[static_cast<std::size_t>(i) * n + i];
    }
    x->assign(n, 0.0);
    for (int i = 0; i < n; ++i) (*x)[static_cast<std::size_t>(perm[static_cast<std::size_t>(i)])] =
        y[static_cast<std::size_t>(i)];
    *rel_resid = 0.0;
    return true;
}

double matvec_resid_inf(const std::vector<double>& A, int n,
                        const std::vector<double>& b, const std::vector<double>& x,
                        double* denom) {
    double rn = 0.0, an = 0.0, xn = 0.0, bn = 0.0;
    for (int i = 0; i < n; ++i) {
        double row = 0.0, s = 0.0;
        for (int j = 0; j < n; ++j) {
            row += std::fabs(A[static_cast<std::size_t>(i) * n + j]);
            s += A[static_cast<std::size_t>(i) * n + j] * x[static_cast<std::size_t>(j)];
        }
        an = std::max(an, row);
        bn = std::max(bn, std::fabs(b[static_cast<std::size_t>(i)]));
        xn = std::max(xn, std::fabs(x[static_cast<std::size_t>(i)]));
        rn = std::max(rn, std::fabs(b[static_cast<std::size_t>(i)] - s));
    }
    *denom = an * xn + bn;
    return rn;
}

// 生产 D 的三点二阶差分规则（与 sky_plane.cpp add_stencil 同规则，独立转写）。
// active[y*nx+x] != 0 表示该 full 节点自由。返回 D（行主序）与对应的 free 索引表。
struct StencilOperator {
    std::vector<int> free_of_full;
    std::vector<int> coord_x, coord_y;   // 每 free 节点的 (ix, iy)
    std::vector<double> D;               // rows × n_free
    int n_free = 0, rows = 0;
};

StencilOperator build_d(const std::vector<int>& active, int nx, int ny) {
    StencilOperator op;
    op.free_of_full.assign(static_cast<std::size_t>(nx * ny), -1);
    for (int i = 0; i < nx * ny; ++i)
        if (active[static_cast<std::size_t>(i)]) {
            op.free_of_full[static_cast<std::size_t>(i)] = op.n_free++;
            op.coord_x.push_back(i % nx);
            op.coord_y.push_back(i / nx);
        }
    const int n = op.n_free;
    auto fof = [&](int full) { return op.free_of_full[static_cast<std::size_t>(full)]; };
    std::vector<std::vector<double>> rws;
    for (int y = 0; y < ny; ++y)
        for (int x = 0; x + 2 < nx; ++x) {
            const int a = fof(y * nx + x), b = fof(y * nx + x + 1), c = fof(y * nx + x + 2);
            if (a < 0 || b < 0 || c < 0) continue;
            std::vector<double> r(static_cast<std::size_t>(n), 0.0);
            r[static_cast<std::size_t>(a)] += 1.0;
            r[static_cast<std::size_t>(b)] += -2.0;
            r[static_cast<std::size_t>(c)] += 1.0;
            rws.push_back(std::move(r));
        }
    for (int x = 0; x < nx; ++x)
        for (int y = 0; y + 2 < ny; ++y) {
            const int a = fof(y * nx + x), b = fof((y + 1) * nx + x), c = fof((y + 2) * nx + x);
            if (a < 0 || b < 0 || c < 0) continue;
            std::vector<double> r(static_cast<std::size_t>(n), 0.0);
            r[static_cast<std::size_t>(a)] += 1.0;
            r[static_cast<std::size_t>(b)] += -2.0;
            r[static_cast<std::size_t>(c)] += 1.0;
            rws.push_back(std::move(r));
        }
    op.rows = static_cast<int>(rws.size());
    op.D.assign(static_cast<std::size_t>(op.rows) * n, 0.0);
    for (int i = 0; i < op.rows; ++i)
        for (int j = 0; j < n; ++j)
            op.D[static_cast<std::size_t>(i) * n + j] = rws[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
    return op;
}

// 独立实现「旧 Q-only 逆修正」（修复前生产公式），用于反例读数：
//   B_a = (A + αQQᵀ)⁻¹ b ；G = Qᵀ H Q ；t = G⁻¹(Qᵀb − Qᵀ H B_a)；B_old = B_a + Q t
std::vector<double> old_q_only_correction(const std::vector<double>& A,
                                         const std::vector<double>& H, int n,
                                         const std::vector<double>& Q, int mq,
                                         double alpha, const std::vector<double>& b) {
    std::vector<double> Aa = A;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            double s = 0.0;
            for (int k = 0; k < mq; ++k)
                s += Q[static_cast<std::size_t>(i) * mq + k] * Q[static_cast<std::size_t>(j) * mq + k];
            Aa[static_cast<std::size_t>(i) * n + j] += alpha * s;
        }
    std::vector<double> Ba, dummy;
    double rr = 0.0;
    gauss_solve(Aa, b, n, &Ba, &rr);
    std::vector<double> G(static_cast<std::size_t>(mq) * mq, 0.0), rhs(static_cast<std::size_t>(mq), 0.0);
    for (int p = 0; p < mq; ++p) {
        for (int q = 0; q < mq; ++q) {
            double s = 0.0;
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    s += Q[static_cast<std::size_t>(i) * mq + p] * H[static_cast<std::size_t>(i) * n + j] *
                         Q[static_cast<std::size_t>(j) * mq + q];
            G[static_cast<std::size_t>(p) * mq + q] = s;
        }
        double s = 0.0;
        for (int i = 0; i < n; ++i) {
            s += Q[static_cast<std::size_t>(i) * mq + p] * b[static_cast<std::size_t>(i)];
            for (int j = 0; j < n; ++j)
                s -= Q[static_cast<std::size_t>(j) * mq + p] * H[static_cast<std::size_t>(j) * n + i] *
                     Ba[static_cast<std::size_t>(i)];
        }
        rhs[static_cast<std::size_t>(p)] = s;
    }
    std::vector<double> t;
    gauss_solve(G, rhs, mq, &t, &rr);
    std::vector<double> bold = Ba;
    for (int i = 0; i < n; ++i) {
        double s = 0.0;
        for (int k = 0; k < mq; ++k) s += Q[static_cast<std::size_t>(i) * mq + k] * t[static_cast<std::size_t>(k)];
        bold[static_cast<std::size_t>(i)] += s;
    }
    return bold;
}

// 候选惩罚零空间基：各 stencil 连通块的多项式列 {1, x, y, xy} + 修正 Gram-Schmidt
// （与生产 sky_plane.cpp 的构造规则一致，但此处独立转写，用于构造测试输入）。
std::vector<double> build_q(const StencilOperator& op, int* out_mq,
                            std::vector<int>* comp_of_free) {
    const int n = op.n_free;
    std::vector<int> uf(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) uf[static_cast<std::size_t>(i)] = i;
    std::function<int(int)> find_root = [&](int x) {
        while (uf[static_cast<std::size_t>(x)] != x) x = uf[static_cast<std::size_t>(x)];
        return x;
    };
    auto unite = [&](int a, int b) {
        const int ra = find_root(a), rb = find_root(b);
        if (ra != rb) uf[static_cast<std::size_t>(rb)] = ra;
    };
    // 用 D 的非零列（每个 stencil 行恰有 3 个非零列）重建连通性。
    for (int i = 0; i < op.rows; ++i) {
        int cols[3], nc = 0;
        for (int j = 0; j < n; ++j)
            if (op.D[static_cast<std::size_t>(i) * n + j] != 0.0 && nc < 3)
                cols[nc++] = j;
        for (int k = 1; k < nc; ++k) unite(cols[0], cols[k]);
    }
    comp_of_free->assign(static_cast<std::size_t>(n), -1);
    std::vector<int> root_to_comp(static_cast<std::size_t>(n), -1);
    int ncomp = 0;
    for (int i = 0; i < n; ++i) {
        const int r = find_root(i);
        if (root_to_comp[static_cast<std::size_t>(r)] < 0) root_to_comp[static_cast<std::size_t>(r)] = ncomp++;
        (*comp_of_free)[static_cast<std::size_t>(i)] = root_to_comp[static_cast<std::size_t>(r)];
    }
    std::vector<std::vector<double>> cols;
    for (int c = 0; c < ncomp; ++c)
        for (int k = 0; k < 4; ++k) {
            std::vector<double> v(static_cast<std::size_t>(n), 0.0);
            for (int i = 0; i < n; ++i) {
                if ((*comp_of_free)[static_cast<std::size_t>(i)] != c) continue;
                const double x = static_cast<double>(op.coord_x[static_cast<std::size_t>(i)]);
                const double y = static_cast<double>(op.coord_y[static_cast<std::size_t>(i)]);
                v[static_cast<std::size_t>(i)] = (k == 0) ? 1.0 : (k == 1) ? x : (k == 2) ? y : x * y;
            }
            double orig = 0.0;
            for (double z : v) orig += z * z;
            orig = std::sqrt(orig);
            for (const auto& q : cols) {
                double pr = 0.0;
                for (int i = 0; i < n; ++i) pr += q[static_cast<std::size_t>(i)] * v[static_cast<std::size_t>(i)];
                for (int i = 0; i < n; ++i) v[static_cast<std::size_t>(i)] -= pr * q[static_cast<std::size_t>(i)];
            }
            double nrm = 0.0;
            for (double z : v) nrm += z * z;
            nrm = std::sqrt(nrm);
            if (orig > 0.0 && nrm > 1e-9 * orig) {
                for (double& z : v) z /= nrm;
                cols.push_back(std::move(v));
            }
        }
    *out_mq = static_cast<int>(cols.size());
    std::vector<double> Q(static_cast<std::size_t>(n) * static_cast<std::size_t>(*out_mq), 0.0);
    for (int k = 0; k < *out_mq; ++k)
        for (int i = 0; i < n; ++i)
            Q[static_cast<std::size_t>(i) * (*out_mq) + k] = cols[static_cast<std::size_t>(k)][static_cast<std::size_t>(i)];
    return Q;
}

std::vector<double> matmul_pp(const std::vector<double>& D, int rows, int n) {
    std::vector<double> P(static_cast<std::size_t>(n) * n, 0.0);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            double s = 0.0;
            for (int r = 0; r < rows; ++r)
                s += D[static_cast<std::size_t>(r) * n + i] * D[static_cast<std::size_t>(r) * n + j];
            P[static_cast<std::size_t>(i) * n + j] = s;
        }
    return P;
}

std::uint64_t prng_state = 88172645463325252ull;
double prng() {
    prng_state ^= prng_state << 13;
    prng_state ^= prng_state >> 7;
    prng_state ^= prng_state << 17;
    return static_cast<double>(prng_state % 1000003ull) / 1000003.0 - 0.5;
}

// 一个 case 的完整读数（用于 JSON 输出与跨语言对拍）。
json run_case(const std::string& name, const std::vector<int>& active, int nx, int ny,
              bool make_singular_on_kerD, bool expect_full_rank_D, int exp_n_free,
              int exp_rankD, int exp_nullity) {
    json out;
    out["case"] = name;
    out["grid"] = {nx, ny};
    StencilOperator op = build_d(active, nx, ny);
    const int n = op.n_free;
    const int rankD = gauss_rank(op.D, op.rows, n);
    out["n_free"] = n;
    out["D_rows"] = op.rows;
    out["D_rank_independent"] = rankD;
    out["D_nullity_independent"] = n - rankD;
    if (exp_n_free >= 0) {
        check(n == exp_n_free, name + ": n_free matches S3-R", std::to_string(n));
        check(rankD == exp_rankD, name + ": rank(D) matches S3-R", std::to_string(rankD));
        check(n - rankD == exp_nullity, name + ": nullity(D) matches S3-R",
              std::to_string(n - rankD));
    }
    (void)expect_full_rank_D;
    int mq = 0;
    std::vector<int> comp;
    std::vector<double> Q = build_q(op, &mq, &comp);
    out["mq"] = mq;
    // D Q ≈ 0 与 QᵀQ = I（独立核对锚基确实是惩罚零空间基）
    double dq = 0.0;
    for (int r = 0; r < op.rows; ++r)
        for (int k = 0; k < mq; ++k) {
            double s = 0.0;
            for (int j = 0; j < n; ++j)
                s += op.D[static_cast<std::size_t>(r) * n + j] * Q[static_cast<std::size_t>(j) * mq + k];
            dq = std::max(dq, std::fabs(s));
        }
    double qtq = 0.0;
    for (int a = 0; a < mq; ++a)
        for (int b = 0; b < mq; ++b) {
            double s = 0.0;
            for (int i = 0; i < n; ++i)
                s += Q[static_cast<std::size_t>(i) * mq + a] * Q[static_cast<std::size_t>(i) * mq + b];
            qtq = std::max(qtq, std::fabs(s - (a == b ? 1.0 : 0.0)));
        }
    out["DQ_max_abs"] = dq;
    out["QtQ_minus_I_max_abs"] = qtq;
    check(dq < 1e-12, name + ": D Q == 0", std::to_string(dq));
    check(qtq < 1e-12, name + ": Q^T Q == I", std::to_string(qtq));

    std::vector<double> P = matmul_pp(op.D, op.rows, n);
    const double lam = 0.5;
    // SPD 数据矩阵：H = XᵀX/n + 0.3 I（定种子 ⇒ 可复现）
    std::vector<double> H(static_cast<std::size_t>(n) * n, 0.0);
    {
        const int nd = n + 9;
        std::vector<double> X(static_cast<std::size_t>(nd) * n);
        for (double& v : X) v = prng();
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                double s = 0.0;
                for (int r = 0; r < nd; ++r)
                    s += X[static_cast<std::size_t>(r) * n + i] * X[static_cast<std::size_t>(r) * n + j];
                H[static_cast<std::size_t>(i) * n + j] = s / nd + ((i == j) ? 0.3 : 0.0);
            }
    }
    if (make_singular_on_kerD) {
        // 把 Q 方向的数据曲率**精确**去掉：H ← (I − QQᵀ) H (I − QQᵀ)，
        // 使 A = H + λP 在 span(Q) 上恰好奇异（λP Q = 0）。
        std::vector<double> HQ(static_cast<std::size_t>(n) * n, 0.0);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                double s = H[static_cast<std::size_t>(i) * n + j];
                for (int k = 0; k < mq; ++k) {
                    double pr = 0.0;
                    for (int a = 0; a < n; ++a)
                        pr += Q[static_cast<std::size_t>(a) * mq + k] * H[static_cast<std::size_t>(a) * n + j];
                    s -= Q[static_cast<std::size_t>(i) * mq + k] * pr;
                }
                HQ[static_cast<std::size_t>(i) * n + j] = s;
            }
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                double s = HQ[static_cast<std::size_t>(i) * n + j];
                for (int k = 0; k < mq; ++k) {
                    double pr = 0.0;
                    for (int a = 0; a < n; ++a)
                        pr += HQ[static_cast<std::size_t>(i) * n + a] * Q[static_cast<std::size_t>(a) * mq + k];
                    s -= pr * Q[static_cast<std::size_t>(j) * mq + k];
                }
                H[static_cast<std::size_t>(i) * n + j] = s;
            }
    }
    std::vector<double> A(static_cast<std::size_t>(n) * n, 0.0);
    double alpha = 0.0;
    for (int i = 0; i < n; ++i) {
        if (P[static_cast<std::size_t>(i) * n + i] > alpha) alpha = P[static_cast<std::size_t>(i) * n + i];
        for (int j = 0; j < n; ++j)
            A[static_cast<std::size_t>(i) * n + j] = H[static_cast<std::size_t>(i) * n + j] + lam * P[static_cast<std::size_t>(i) * n + j];
    }
    alpha *= lam;
    out["lambda"] = lam;
    out["alpha"] = alpha;
    std::vector<double> xtrue(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) xtrue[static_cast<std::size_t>(i)] = prng();
    std::vector<double> b(static_cast<std::size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        double s = 0.0;
        for (int j = 0; j < n; ++j) s += A[static_cast<std::size_t>(i) * n + j] * xtrue[static_cast<std::size_t>(j)];
        b[static_cast<std::size_t>(i)] = s;
    }
    // 后向误差门（Higham 型 k·n·eps，k=8；比生产的 τ_floor = max(n_obs,n_free)·ε 更紧）
    const double gate = 8.0 * static_cast<double>(std::max(n, 1)) *
                        std::numeric_limits<double>::epsilon();

    // 导出算子本体（供独立 Python oracle 在同一 A,H,b,Q 上对拍；不是期望值来源）
    out["ops"] = {{"A", A}, {"H", H}, {"b", b}, {"Q", Q}, {"alpha", alpha},
                  {"lambda", lam}, {"gate", gate}, {"nx", nx}, {"ny", ny},
                  {"D", op.D}, {"D_rows", op.rows}, {"mq", mq}};
    auto run = [&](int force_anchor, const char* tag) {
        std::vector<double> B(static_cast<std::size_t>(n), 0.0);
        AcsdSkyDeflationDiag dg{};
        const int rc = acsd_sky_plane_penalized_solve_for_test(
            A.data(), n, b.data(), Q.data(), mq, alpha, gate, force_anchor, B.data(), &dg);
        double denom = 0.0;
        const double res = matvec_resid_inf(A, n, b, B, &denom);
        json r;
        r["tag"] = tag;
        r["rc"] = rc;
        r["status"] = dg.status;
        r["branch"] = dg.branch;
        r["chol_fail_col"] = dg.chol_fail_col;
        r["chol_fail_pivot"] = dg.chol_fail_pivot;
        r["reported_backward_error"] = dg.residual_rel;
        r["recomputed_residual_inf_over_denom"] = (denom > 0.0) ? res / denom : -1.0;
        if (rc == 0) {
            json bx = json::array();
            for (int i = 0; i < n; ++i) bx.push_back(B[static_cast<std::size_t>(i)]);
            r["B"] = std::move(bx);
        }
        return r;
    };
    out["solve_direct"] = run(0, "direct");
    out["solve_forced_anchor"] = run(1, "forced_anchor");

    // 独立参考解：全主元 Gauss 解 A x = b（A SPD 时唯一）
    std::vector<double> xref;
    double rr = 0.0;
    const bool solved = gauss_solve(A, b, n, &xref, &rr);
    out["independent_gauss_solved"] = solved;
    if (solved) {
        double denom = 0.0;
        out["independent_gauss_residual_inf_over_denom"] =
            matvec_resid_inf(A, n, b, xref, &denom) / denom;
        json bx = json::array();
        for (double v : xref) bx.push_back(v);
        out["B_independent_gauss"] = std::move(bx);
    }
    // 修复前公式（Q-only）在同一 A,b 上的残差（反例）
    if (solved) {
        std::vector<double> bold = old_q_only_correction(A, H, n, Q, mq, alpha, b);
        double denom = 0.0;
        out["old_q_only_residual_inf_over_denom"] =
            matvec_resid_inf(A, n, b, bold, &denom) / denom;
    }
    // 参数偏差：forced-anchor vs 直接解（A SPD 时必须逐位级一致）
    if (out["solve_direct"]["rc"] == 0 && out["solve_forced_anchor"]["rc"] == 0) {
        const auto& bd = out["solve_direct"]["B"];
        const auto& ba = out["solve_forced_anchor"]["B"];
        double db = 0.0, sb = 0.0;
        for (int i = 0; i < n; ++i) {
            const double a = bd[static_cast<std::size_t>(i)].get<double>();
            const double c = ba[static_cast<std::size_t>(i)].get<double>();
            db = std::max(db, std::fabs(a - c));
            sb = std::max(sb, std::fabs(a));
        }
        out["param_rel_diff_direct_vs_forced_anchor"] = (sb > 0.0) ? db / sb : db;
    }
    g_readings.push_back(out);
    return out;
}

// 生产 dump 的真矩阵（可选）：直接驱动 hook，用独立读数判定 status 是否自洽。
void run_dump_case(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "SKIP: cannot open dump %s\n", path.c_str());
        return;
    }
    std::string text;
    char buf[65536];
    std::size_t k;
    while ((k = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, k);
    std::fclose(f);
    json d = json::parse(text);
    const int n = d["grid"]["n_free"].get<int>();
    auto flat = [&](const char* key) {
        std::vector<double> v;
        for (const auto& x : d[key]["values_row_major"]) v.push_back(x.get<double>());
        return v;
    };
    std::vector<double> A = flat("H_solve");
    std::vector<double> Hred = flat("H_red");
    std::vector<double> b = flat("rhs");
    std::vector<double> Q = flat("Q_penalty_nullspace");
    const int mq = d["grid"]["mq_penalty_nullspace_candidate"].get<int>();
    const double alpha = d["alpha_anchor"].get<double>();
    const double gate = d["solve"]["backward_error_gate"].get<double>();
    json out;
    out["case"] = "production_abc_real_iter0";
    out["dump_path"] = path;
    out["n_free"] = n;
    out["mq"] = mq;
    out["alpha"] = alpha;
    out["gate"] = gate;
    out["n_stencil_components"] = d["grid"]["n_stencil_components"];
    // 独立重建 D（用 dump 的 stencil 行索引）并验证 A == H_red + λ P
    const double lam = d["lambda_numerical"].get<double>();
    std::vector<std::vector<int>> rows;
    for (const auto& r : d["stencil_rows_free_index"]) {
        std::vector<int> t;
        for (const auto& x : r) t.push_back(x.get<int>());
        rows.push_back(t);
    }
    std::vector<double> P(static_cast<std::size_t>(n) * n, 0.0);
    for (const auto& r : rows) {
        const int ii[3] = {r[0], r[1], r[2]};
        const double v[3] = {1.0, -2.0, 1.0};
        for (int a = 0; a < 3; ++a)
            for (int b2 = 0; b2 < 3; ++b2)
                P[static_cast<std::size_t>(ii[a]) * n + ii[b2]] += v[a] * v[b2];
    }
    double rebuild_max_abs = 0.0, arel = 0.0;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            const double want = Hred[static_cast<std::size_t>(i) * n + j] + lam * P[static_cast<std::size_t>(i) * n + j];
            const double got = A[static_cast<std::size_t>(i) * n + j];
            rebuild_max_abs = std::max(rebuild_max_abs, std::fabs(want - got));
            arel = std::max(arel, std::fabs(got));
        }
    out["A_rebuild_from_Hred_plus_lambdaP_max_abs"] = rebuild_max_abs;
    out["A_rebuild_relative"] = (arel > 0.0) ? rebuild_max_abs / arel : rebuild_max_abs;
    check(rebuild_max_abs <= 1e-9 * std::max(arel, 1e-300),
          "production dump: H_solve == H_red + lambda*D^T D", std::to_string(rebuild_max_abs));
    // 独立秩诊断：A 的对称谱（用幂法/正交迭代太慢；此处用 Gauss 秩 + 主元读数）
    const int rankA = gauss_rank(A, n, n, 1e-12);
    out["A_rank_independent_rtol1e-12"] = rankA;
    std::vector<double> B(static_cast<std::size_t>(n), 0.0);
    AcsdSkyDeflationDiag dg{};
    const int rc = acsd_sky_plane_penalized_solve_for_test(A.data(), n, b.data(), Q.data(),
                                                           mq, alpha, gate, 0, B.data(), &dg);
    out["prod_rc"] = rc;
    out["prod_status"] = dg.status;
    out["prod_branch"] = dg.branch;
    out["prod_chol_fail_col"] = dg.chol_fail_col;
    out["prod_chol_fail_pivot"] = dg.chol_fail_pivot;
    out["prod_reported_backward_error"] = dg.residual_rel;
    // 自洽性：只有过门才允许 rc==0；rc!=0 时必须不产生可信解。
    out["ops"] = {{"A", A}, {"H", Hred}, {"b", b}, {"Q", Q}, {"alpha", alpha},
                  {"lambda", lam}, {"gate", gate}, {"mq", mq},
                  {"stencil_rows", rows}};
    check((rc == 0) == (dg.status == 0 || dg.status == 1),
          "production dump: rc 与 status 自洽");
    check(rc == 0 || !(dg.residual_rel == dg.residual_rel) || dg.residual_rel > gate,
          "production dump: rc!=0 时不返回可信解");
    g_readings.push_back(out);
}

}  // namespace

int main(int argc, char** argv) {
    std::string dump_path, out_path;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) dump_path = argv[++i];
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) out_path = argv[++i];
    }
    // S3-R 独立结果（实验/innovation-closure/S3R_SCIENCE_REPORT_2026-10-18.md）
    {
        std::vector<int> a(42, 1);
        run_case("full_7x6", a, 7, 6, false, true, 42, 38, 4);
    }
    {
        std::vector<int> a(42, 1);
        for (int x = 0; x < 7; ++x) a[static_cast<std::size_t>(3 * 7 + x)] = 0;   // 缺整行
        run_case("missing_full_row_7x6", a, 7, 6, false, false, 35, 27, 8);
    }
    {
        std::vector<int> a(21, 0);
        for (int y = 0; y < 3; ++y)
            for (int x : {0, 1, 2, 4, 5, 6}) a[static_cast<std::size_t>(y * 7 + x)] = 1;
        run_case("two_3x3_islands_7x3", a, 7, 3, false, false, 18, 10, 8);
    }
    {
        std::vector<int> a(42, 1);
        a[static_cast<std::size_t>(3 * 7 + 3)] = 0;   // 单缺点
        run_case("single_hole_7x6", a, 7, 6, false, true, 41, 37, 4);
    }
    // 精确奇异（在 ker(D) 方向上无数据曲率）⇒ 锚定可分解但 (I−αR) 奇异 ⇒ 具名失败
    {
        std::vector<int> a(42, 1);
        json o = run_case("full_7x6_singular_on_kerD", a, 7, 6, true, true, 42, 38, 4);
        check(o["solve_direct"]["status"].get<int>() != 1,
              "singular-on-kerD: 直接分支不得报 OK_WOODBURY");
        check(o["solve_forced_anchor"]["status"].get<int>() == 3,
              "singular-on-kerD: deflation 必须报 NOT_RESOLVABLE(3)",
              std::to_string(o["solve_forced_anchor"]["status"].get<int>()));
        check(o["solve_forced_anchor"]["rc"].get<int>() == 1,
              "singular-on-kerD: 不产生可信解");
    }
    // SPD + 强制锚定分支：deflation 必须精确复现直接解（旧 Q-only 公式的修复证据）
    {
        std::vector<int> a(42, 1);
        json o = run_case("full_7x6_spd_forced_anchor", a, 7, 6, false, true, 42, 38, 4);
        check(o["solve_direct"]["status"].get<int>() == 0, "SPD: 直接分支 OK_DIRECT");
        check(o["solve_forced_anchor"]["status"].get<int>() == 1,
              "SPD: 强制锚定分支 OK_WOODBURY");
        check(o["solve_forced_anchor"]["recomputed_residual_inf_over_denom"].get<double>() < 1e-12,
              "SPD: deflation 原方程后向误差 ≤ 1e-12",
              std::to_string(o["solve_forced_anchor"]["recomputed_residual_inf_over_denom"].get<double>()));
        check(o["param_rel_diff_direct_vs_forced_anchor"].get<double>() < 1e-10,
              "SPD: deflation 参数与直接解相对差 ≤ 1e-10",
              std::to_string(o["param_rel_diff_direct_vs_forced_anchor"].get<double>()));
        const double old_r = o["old_q_only_residual_inf_over_denom"].get<double>();
        const double new_r = o["solve_forced_anchor"]["recomputed_residual_inf_over_denom"].get<double>();
        check(old_r > 1e-4, "反例：旧 Q-only 公式在同一 A,b 上残差 > 1e-4",
              std::to_string(old_r));
        check(old_r > 1e4 * std::max(new_r, 1e-300),
              "修复：deflation 残差比旧 Q-only 公式小 ≥ 1e4 倍",
              std::to_string(new_r) + " vs " + std::to_string(old_r));
        check(o["solve_direct"]["recomputed_residual_inf_over_denom"].get<double>() < 1e-12,
              "SPD: 直接分支后向误差 ≤ 1e-12");
    }
    if (!dump_path.empty()) run_dump_case(dump_path);

    json outj;
    outj["schema"] = "acsd.sky-plane-deflation-test/v1";
    outj["n_cases"] = g_readings.size();
    outj["failures"] = g_fail;
    outj["cases"] = g_readings;
    if (!out_path.empty()) {
        std::FILE* f = std::fopen(out_path.c_str(), "wb");
        if (f) {
            const std::string s = outj.dump(2);
            std::fwrite(s.data(), 1, s.size(), f);
            std::fclose(f);
        }
    }
    std::fprintf(stderr, "sky_plane_deflation_test: %zu cases, %d failing assertions\n",
                 g_readings.size(), g_fail);
    return g_fail;
}
