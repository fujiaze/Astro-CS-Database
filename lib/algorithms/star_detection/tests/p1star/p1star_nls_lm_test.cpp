// p1star_nls_lm_test.cpp — 自研信赖域 LM 求解器的五类解析验证（GSL-REPLACE-01）
//
// 覆盖（每类都有可证伪判据, 不是恒真断言）:
//   ① 良态      : 无噪声椭圆高斯, 7 参数恢复到解析真值
//   ② 病态      : 参数量级差 1e3 + 近圆（alpha 列量级 ~1e-2）⇒ 未缩放 Gram 条件数
//                 kappa(JᵀJ) >= 1e6（判据本身可证伪）; 判据 = 求解器仍恢复全部**可观测量**
//                 （cx,cy,sx,sy,A,B）且 chi2 达数值零 —— 说明 Moré 列范数缩放是有效的
//   ③ 初值远离解: 位置偏 2px / 宽度偏 2.5 倍 / fr 反号, 仍收敛
//   ④ 残差非零  : 固定种子噪声 ⇒ 最优点残差非零; 判据 = 精确 Gauss-Newton 步的
//                 预测下降 <= 1e-6*chi2（最优性）+ A/B 与线性最小二乘闭式解一致
//   ⑤ 参数不可辨识: 圆星（r=1 ⇒ alpha 偏导恒为 0）⇒ J 秩亏; 判据 = 不产生 NaN/inf、
//                 可辨识量正确、chi2 沿 alpha 方向严格平坦（不可辨识的实测证据）
//
// 角度处理: 模型在 fr→-fr 与 alpha→alpha+π 下不变（规范等价）, 故参数对比一律用
// 可观测量 (cx,cy,sx,sy,A,B), 不比较 fr/alpha 本身。
// 失败以非零退出码 + 明确日志报告; 不使用任何外部数值库。
#include "nls_lm.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

using astrocs::star_detection::nls::Options;
using astrocs::star_detection::nls::Report;
using astrocs::star_detection::nls::Status;

struct Sample {
    double dx;
    double dy;
    double val;
};

struct Fixture {
    std::vector<Sample> s;
    size_t n() const { return s.size(); }
};

// 生产同款 7 参数母函数: x = {B, A, x0, y0, SX(=2σx²), fr, alpha}; SY = r²SX
inline void model(const double* p, double dx, double dy, double* out) {
    const double B = p[0], A = p[1], x0 = p[2], y0 = p[3];
    const double SX = std::fabs(p[4]);
    const double r = 0.5 * (std::cos(p[5]) + 1.0);
    const double SY = r * r * SX;
    const double ca = std::cos(p[6]), sa = std::sin(p[6]);
    const double tmpx = ca * (dx - x0) - sa * (dy - y0);
    const double tmpy = sa * (dx - x0) + ca * (dy - y0);
    *out = B + A * std::exp(-(tmpx * tmpx / SX + tmpy * tmpy / SY));
}

void resid(const double* x, void* ctx, double* f) {
    const Fixture* fx = static_cast<const Fixture*>(ctx);
    for (size_t k = 0; k < fx->n(); ++k) {
        double m = 0.0;
        model(x, fx->s[k].dx, fx->s[k].dy, &m);
        f[k] = m - fx->s[k].val;
    }
}

// 解析雅可比（与生产 sdet_gaussian_df 同式, 此处独立重写供自检）
void jac(const double* x, void* ctx, double* J) {
    const Fixture* fx = static_cast<const Fixture*>(ctx);
    const double A = x[1];
    const double SX = std::fabs(x[4]);
    const double r = 0.5 * (std::cos(x[5]) + 1.0);
    const double SY = r * r * SX;
    const double ca = std::cos(x[6]), sa = std::sin(x[6]);
    const double sc = std::sin(x[5]);
    for (size_t k = 0; k < fx->n(); ++k) {
        const double dx = fx->s[k].dx - x[2];
        const double dy = fx->s[k].dy - x[3];
        const double tmpx = ca * dx - sa * dy;
        const double tmpy = sa * dx + ca * dy;
        const double tmpc = std::exp(-(tmpx * tmpx / SX + tmpy * tmpy / SY));
        double* row = J + 7 * k;
        row[0] = 1.0;
        row[1] = tmpc;
        row[2] = 2.0 * A * tmpc * (tmpx / SX * ca + tmpy / SY * sa);
        row[3] = 2.0 * A * tmpc * (-tmpx / SX * sa + tmpy / SY * ca);
        row[4] = tmpc * A * (tmpx * tmpx / (SX * SX) + tmpy * tmpy / (SX * SX * r * r));
        row[5] = -A * tmpc * sc * tmpy * tmpy / SY / r;
        row[6] = 2.0 * A * tmpc * tmpx * tmpy * (1.0 / SX - 1.0 / SY);
    }
}

double chi2_at(const Fixture& fx, const double* p) {
    double c = 0.0;
    for (const Sample& s : fx.s) {
        double m = 0.0;
        model(p, s.dx, s.dy, &m);
        const double rr = m - s.val;
        c += rr * rr;
    }
    return c;
}

Fixture make_field(double B, double A, double x0, double y0, double SX, double fr,
                   double alpha, double sigma, int half, unsigned seed) {
    Fixture fx;
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> nd(0.0, 1.0);
    const double truth[7] = {B, A, x0, y0, SX, fr, alpha};
    for (int i = -half; i <= half; ++i) {
        for (int j = -half; j <= half; ++j) {
            Sample s;
            s.dx = j + 0.5;
            s.dy = i + 0.5;
            double m = 0.0;
            model(truth, s.dx, s.dy, &m);
            s.val = m + (sigma > 0.0 ? sigma * nd(rng) : 0.0);
            fx.s.push_back(s);
        }
    }
    return fx;
}

// ---- 可观测量（规范不变）: cx, cy, sx, sy, A, B ----
void observables(const double* p, double* o) {
    const double SX = std::fabs(p[4]);
    const double r = 0.5 * (std::cos(p[5]) + 1.0);
    o[0] = p[2];                      // cx
    o[1] = p[3];                      // cy
    o[2] = std::sqrt(SX * 0.5);       // sx
    o[3] = std::sqrt(SX * 0.5) * r;   // sy
    o[4] = p[1];                      // A
    o[5] = p[0];                      // B
}

double rel(double a, double b) { return std::fabs(a - b) / std::fmax(std::fabs(b), 1e-300); }

// 可观测量误差: |a-b| / max(|b|, scale_i)
// scale: cx/cy/sx/sy 取 1 px（真值可能恰为 0, 纯相对误差会退化）; A/B 取真值量级
double obs_err(const double* o, const double* t) {
    const double sc[6] = {1.0, 1.0, 1.0, 1.0, std::fabs(t[4]), std::fabs(t[5])};
    double w = 0.0;
    for (int i = 0; i < 6; ++i) w = std::fmax(w, std::fabs(o[i] - t[i]) / std::fmax(sc[i], 1e-300));
    return w;
}

// ---- 未缩放条件数 kappa(JᵀJ)（Jacobi 对称特征分解, 7x7）----
void jacobi_eig(double* A, double* eval) {
    for (int sweep = 0; sweep < 200; ++sweep) {
        double off = 0.0;
        for (int a = 0; a < 7; ++a)
            for (int b = a + 1; b < 7; ++b) off += A[a * 7 + b] * A[a * 7 + b];
        if (off < 1e-30) break;
        for (int p = 0; p < 7; ++p) {
            for (int q = p + 1; q < 7; ++q) {
                if (std::fabs(A[p * 7 + q]) < 1e-300) continue;
                const double theta = 0.5 * std::atan2(2.0 * A[p * 7 + q], A[q * 7 + q] - A[p * 7 + p]);
                const double c = std::cos(theta), s = std::sin(theta);
                for (int k = 0; k < 7; ++k) {
                    const double akp = A[k * 7 + p], akq = A[k * 7 + q];
                    A[k * 7 + p] = c * akp - s * akq;
                    A[k * 7 + q] = s * akp + c * akq;
                }
                for (int k = 0; k < 7; ++k) {
                    const double apk = A[p * 7 + k], aqk = A[q * 7 + k];
                    A[p * 7 + k] = c * apk - s * aqk;
                    A[q * 7 + k] = s * apk + c * aqk;
                }
            }
        }
    }
    for (int i = 0; i < 7; ++i) eval[i] = A[i * 7 + i];
}

double kappa_gram(const Fixture& fx, const double* p) {
    std::vector<double> J(fx.n() * 7);
    jac(p, const_cast<Fixture*>(&fx), J.data());
    double G[49];
    for (int a = 0; a < 7; ++a) {
        for (int b = 0; b < 7; ++b) {
            double s = 0.0;
            for (size_t k = 0; k < fx.n(); ++k) s += J[k * 7 + a] * J[k * 7 + b];
            G[a * 7 + b] = s;
        }
    }
    double ev[7];
    jacobi_eig(G, ev);
    double lmax = ev[0], lmin = ev[0];
    for (int i = 1; i < 7; ++i) {
        lmax = std::fmax(lmax, ev[i]);
        lmin = std::fmin(lmin, ev[i]);
    }
    return lmax / std::fmax(lmin, 1e-300);
}

int g_fail = 0;
void check(bool ok, const std::string& what) {
    std::printf("    [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++g_fail;
}

// 精确 Gauss-Newton 步的预测下降量（最优性度量, 单位与 chi2 相同）
double gn_predicted_drop(const Fixture& fx, const double* x) {
    std::vector<double> f(fx.n()), J(fx.n() * 7);
    resid(x, const_cast<Fixture*>(&fx), f.data());
    jac(x, const_cast<Fixture*>(&fx), J.data());
    double A[7][8];
    for (int a = 0; a < 7; ++a) {
        for (int b = 0; b < 7; ++b) {
            double s = 0.0;
            for (size_t k = 0; k < fx.n(); ++k) s += J[k * 7 + a] * J[k * 7 + b];
            A[a][b] = s;
        }
        double g = 0.0;
        for (size_t k = 0; k < fx.n(); ++k) g += J[k * 7 + a] * f[k];
        A[a][7] = -g;
    }
    // 加最小脊 + 高斯消元（奇异时下降量记为 0: 秩亏方向不构成下降方向）
    double scale = 0.0;
    for (int a = 0; a < 7; ++a) scale = std::fmax(scale, std::fabs(A[a][a]));
    for (int a = 0; a < 7; ++a) A[a][a] += scale * 1e-14;
    for (int c = 0; c < 7; ++c) {
        int piv = c;
        for (int rr = c + 1; rr < 7; ++rr)
            if (std::fabs(A[rr][c]) > std::fabs(A[piv][c])) piv = rr;
        for (int cc = 0; cc < 8; ++cc) std::swap(A[c][cc], A[piv][cc]);
        if (std::fabs(A[c][c]) < 1e-300) return 0.0;
        for (int rr = c + 1; rr < 7; ++rr) {
            const double fac = A[rr][c] / A[c][c];
            for (int cc = c; cc < 8; ++cc) A[rr][cc] -= fac * A[c][cc];
        }
    }
    double h[7];
    for (int a = 6; a >= 0; --a) {
        double s = A[a][7];
        for (int b = a + 1; b < 7; ++b) s -= A[a][b] * h[b];
        h[a] = s / A[a][a];
    }
    double drop = 0.0;
    for (int a = 0; a < 7; ++a) {
        double g = 0.0;
        for (size_t k = 0; k < fx.n(); ++k) g += J[k * 7 + a] * f[k];
        drop += g * h[a];
    }
    return -drop;   // gᵀh < 0 ⇒ 下降量 -gᵀh > 0
}

}  // namespace

int main() {
    std::printf("== 自研 trust-region LM 五类解析验证 ==\n");

    // ---------------- ① 良态 ----------------
    {
        std::printf("[1] 良态: 无噪声椭圆高斯, 可观测量恢复\n");
        Fixture fx = make_field(100.0, 2000.0, 0.3, -0.2, 2.88, 0.9, 0.35, 0.0, 6, 11);
        double x[7] = {90.0, 1800.0, 0.0, 0.0, 3.0, 0.8, 0.0};
        Options opt;
        Report r = astrocs::star_detection::nls::solve(&resid, &jac, &fx, fx.n(), 7, x, opt, nullptr);
        check(r.status == Status::Success, "status == Success");
        std::printf("      chi2 = %.3e (A^2=%.3e)\n", r.cost, 2000.0 * 2000.0);
        check(r.cost <= 1e-16 * 4.0e6, "chi2 达数值零 (<= 1e-16 * A^2)");
        const double truth[7] = {100.0, 2000.0, 0.3, -0.2, 2.88, 0.9, 0.35};
        double o1[6], o2[6];
        observables(x, o1);
        observables(truth, o2);
        const double worst = obs_err(o1, o2);
        std::printf("      可观测量最大相对偏差 %.3e (niter=%zu)\n", worst, r.iterations);
        check(worst <= 1e-6, "6 个可观测量相对偏差 <= 1e-6");
    }

    // ---------------- ② 病态 ----------------
    {
        std::printf("[2] 病态: 参数量级差 1e3 + 近圆 (r=0.995)\n");
        Fixture fx = make_field(1.0e2, 1.0e5, 0.4, -0.3, 18.0, 0.14153947, 0.8, 0.0, 12, 107);
        const double truth[7] = {1.0e2, 1.0e5, 0.4, -0.3, 18.0, 0.14153947, 0.8};
        const double kap = kappa_gram(fx, truth);
        std::printf("      未缩放 kappa(JᵀJ) = %.3e\n", kap);
        check(kap >= 1e6, "kappa(JᵀJ) >= 1e6（'病态'标签可证伪）");
        double x[7] = {8.0e1, 8.0e4, 0.0, 0.0, 25.0, 0.10, 0.5};
        Options opt;
        Report r = astrocs::star_detection::nls::solve(&resid, &jac, &fx, fx.n(), 7, x, opt, nullptr);
        check(r.status == Status::Success, "status == Success");
        check(std::isfinite(r.cost), "chi2 有限");
        check(r.cost <= 1e-9 * (1e5 * 1e5), "chi2 达数值零 (无噪声 ⇒ 全局最优 0)");
        double o1[6], o2[6];
        observables(x, o1);
        observables(truth, o2);
        const double worst = obs_err(o1, o2);
        std::printf("      可观测量最大相对偏差 %.3e (niter=%zu, crit=%d)\n", worst, r.iterations,
                    (int)r.criterion);
        check(worst <= 1e-5, "病态下 6 个可观测量相对偏差 <= 1e-5（Moré 缩放有效）");
    }

    // ---------------- ③ 初值远离解 ----------------
    {
        std::printf("[3] 初值远离解: 位置偏 2px / 宽度偏 2.5 倍 / fr 反号\n");
        Fixture fx = make_field(50.0, 500.0, 0.0, 0.0, 2.0, 1.2, 0.6, 0.0, 7, 31);
        double x[7] = {0.0, 100.0, 2.0, -2.0, 12.5, -1.2, -0.6};
        Options opt;
        Report r = astrocs::star_detection::nls::solve(&resid, &jac, &fx, fx.n(), 7, x, opt, nullptr);
        check(r.status == Status::Success, "status == Success");
        const double truth[7] = {50.0, 500.0, 0.0, 0.0, 2.0, 1.2, 0.6};
        double o1[6], o2[6];
        observables(x, o1);
        observables(truth, o2);
        const double worst = obs_err(o1, o2);
        std::printf("      可观测量最大相对偏差 %.3e (niter=%zu)\n", worst, r.iterations);
        check(worst <= 1e-5, "远初值下 6 个可观测量相对偏差 <= 1e-5");
        check(r.cost <= 1e-12 * 500.0 * 500.0, "chi2 达数值零");
    }

    // ---------------- ④ 残差非零（有噪声）----------------
    {
        std::printf("[4] 残差非零: sigma=8 固定种子噪声\n");
        Fixture fx = make_field(120.0, 1500.0, -0.25, 0.4, 2.4, 1.0, 0.2, 8.0, 6, 47);
        double x[7] = {100.0, 1200.0, 0.0, 0.0, 3.0, 0.9, 0.0};
        Options opt;
        Report r = astrocs::star_detection::nls::solve(&resid, &jac, &fx, fx.n(), 7, x, opt, nullptr);
        check(r.status == Status::Success, "status == Success");
        check(r.cost > 0.0, "chi2 > 0（残差非零, 判据非退化）");
        const double drop = gn_predicted_drop(fx, x);
        std::printf("      精确 GN 步预测下降 %.3e ; chi2=%.6g ; 比值 %.3e (niter=%zu)\n",
                    drop, r.cost, drop / r.cost, r.iterations);
        check(drop <= 1e-6 * r.cost, "最优性: 精确 GN 步预测下降 <= 1e-6*chi2");
        // 独立参照: 固定求解出的形状/位置, {B,A} 用线性最小二乘闭式解
        double s1 = 0, sg = 0, sgg = 0, sy = 0, sgy = 0;
        double shape[7] = {0.0, 1.0, x[2], x[3], x[4], x[5], x[6]};
        for (const Sample& s : fx.s) {
            double g = 0.0;
            model(shape, s.dx, s.dy, &g);
            s1 += 1.0; sg += g; sgg += g * g; sy += s.val; sgy += g * s.val;
        }
        const double det = s1 * sgg - sg * sg;
        const double B_ref = (sgg * sy - sg * sgy) / det;
        const double A_ref = (s1 * sgy - sg * sy) / det;
        std::printf("      A: 求解 %.6f vs 闭式 %.6f ; B: 求解 %.6f vs 闭式 %.6f\n",
                    x[1], A_ref, x[0], B_ref);
        check(rel(x[1], A_ref) <= 1e-5 && rel(x[0], B_ref) <= 1e-4,
              "A/B 与线性最小二乘闭式解一致");
    }

    // ---------------- ⑤ 参数不可辨识 ----------------
    {
        std::printf("[5] 参数不可辨识: 圆星 (r=1) ⇒ alpha 偏导恒为 0（J 秩亏）\n");
        Fixture fx = make_field(20.0, 800.0, 0.0, 0.0, 3.0, 0.0, 0.5, 0.0, 6, 59);
        double x[7] = {10.0, 700.0, 0.1, -0.1, 3.5, 0.0, 0.0};
        Options opt;
        Report r = astrocs::star_detection::nls::solve(&resid, &jac, &fx, fx.n(), 7, x, opt, nullptr);
        check(r.status == Status::Success, "status == Success（秩亏不导致失败）");
        bool finite = true;
        for (int i = 0; i < 7; ++i) finite = finite && std::isfinite(x[i]);
        check(finite, "7 参数全部有限（无 NaN/inf）");
        // alpha 列恒为 0（秩亏的直接证据）
        std::vector<double> J(fx.n() * 7);
        jac(x, &fx, J.data());
        double max_alpha_col = 0.0;
        for (size_t k = 0; k < fx.n(); ++k) max_alpha_col = std::fmax(max_alpha_col, std::fabs(J[k * 7 + 6]));
        check(max_alpha_col == 0.0, "J 的第 7 列（alpha）恒为 0 ⇒ 秩亏（判据非恒真）");
        // chi2 沿 alpha 方向平坦性。尺度取 n*A^2（模型完全错时的 chi2 量级）:
        // 圆星处 chi2 已落在浮点舍入地板（~1e-24）, 与 chi2 自身比会退化为舍入噪声之比。
        const double chi2_scale = static_cast<double>(fx.n()) * 800.0 * 800.0;
        double p2[7];
        std::memcpy(p2, x, sizeof(p2));
        double max_dev = 0.0;
        for (double d : {-1.0, -0.1, 0.1, 1.0}) {
            p2[6] = x[6] + d;
            max_dev = std::fmax(max_dev, std::fabs(chi2_at(fx, p2) - r.cost));
        }
        std::printf("      chi2 沿 alpha 最大变化 %.3e ; 尺度 n*A^2=%.3e ; chi2=%.3e\n",
                    max_dev, chi2_scale, r.cost);
        check(max_dev <= 1e-12 * chi2_scale, "chi2 沿 alpha 平坦 <= 1e-12*n*A^2（不可辨识的实测证据）");
        // 对照（反例, 判据非恒真）: 同一 alpha 扰动在非圆星 (r=0.5) 上必须显著改变 chi2
        {
            Fixture fxe = make_field(20.0, 800.0, 0.0, 0.0, 3.0, 1.5707963, 0.5, 0.0, 6, 59);
            double xe[7] = {10.0, 700.0, 0.1, -0.1, 3.5, 1.5, 0.0};
            Options o2;
            Report re = astrocs::star_detection::nls::solve(&resid, &jac, &fxe, fxe.n(), 7, xe, o2, nullptr);
            double pe[7];
            std::memcpy(pe, xe, sizeof(pe));
            pe[6] = xe[6] + 1.0;
            const double dev_e = std::fabs(chi2_at(fxe, pe) - re.cost);
            std::printf("      对照（椭圆 r=0.5）: 同一 alpha 扰动使 chi2 变化 %.3e\n", dev_e);
            check(dev_e > 1e3 * 1e-12 * chi2_scale, "对照: 椭圆星下 chi2 显著变化 ⇒ 平坦性判据非恒真");
        }
        const double truth[7] = {20.0, 800.0, 0.0, 0.0, 3.0, 0.0, 0.5};
        double o1[6], o2[6];
        observables(x, o1);
        observables(truth, o2);
        const double worst = obs_err(o1, o2);
        std::printf("      可观测量最大相对偏差 %.3e\n", worst);
        check(worst <= 1e-6, "可辨识量仍正确（相对偏差 <= 1e-6）");
    }

    std::printf("== 结果: %s (失败断言 %d) ==\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}
