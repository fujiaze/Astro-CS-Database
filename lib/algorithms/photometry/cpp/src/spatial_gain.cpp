// spatial_gain.cpp - 低阶乘性空间增益 m(x,y) 的拟合（见 spatial_gain.h 的规范依据）
//
// 求解路线（**分阶段**，与 SCI-PHOT-001 §5 的既有冻结路线成对，不重估全局项）:
//   阶段 1/2（既有，未改）: 星等一致性预过滤 → Tukey-IRLS 稳健**位置** location
//                          ⇒ k_photo = 10^(−location)，inlier 集合。
//   阶段 3（本文件）: 在 inlier 集合上拟合**零均值空间项**
//                          log10 m(x,y) = −Σ_j c_j·B̃_j(x̃,ỹ)
//                      其中 B̃ 为按**当前 Tukey 权重**中心化的基函数 ⇒
//                      星集合上 log10 m 的加权均值恒为 0（规范，见头文件）。
//   为什么不让阶段 3 重估常数项: 常数项重估会把 k_photo 移动 O(1e-9) dex，破坏
//   「关闭空间项 ⇒ 与既有实现逐位一致」的可回归性；而数学上（同一权重、同一
//   inlier 集合下）带常数列的联合解，其常数项**就是**阶段 2 的加权均值 location，
//   两者给出同一个标定面（见 ctest p1phot_spatial 的 S1 等价性断言）。
//
// 稳健化（与实验/photometric-magnitude/docs/p1-spatial-gain.md §2.3/§2.4 一致）:
//   尺度 S **一次估计后在迭代中固定**（MAD(r−L)/0.6745），与 star_matcher 的
//   固定尺度 M 估计路线（docs/algorithms/PHOTOMETRIC_FIT.md §F3）同口径，
//   因此继承同一崩溃点保证；c = 4.685 与生产冻结值同值。
//   离群剔除只在阶段 3 **收紧**（绝不把阶段 2 剔除的星放回来）。
//
// 确定性: 固定样本序、固定基函数序、无 OpenMP、无随机数、无跨样本重结合。

#include "spatial_gain.h"

#include "photometry_apply.h"                 // 基函数/求值唯一实现
#include "astro/phase2/identifiability.h"     // 仓内唯一可辨识性判据

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace astrocs {
namespace photometry {
namespace {

// 与 star_matcher.cpp 的冻结常量同值（_TUKEY_C / _MAD_SCALE / _IRLS_MAX_ITER）
constexpr double kTukeyC = 4.685;
constexpr double kMadScale = 0.6744897501960817;
constexpr int kIrlsMaxIter = 50;
// 系数增量收敛门（比标量 location 的 1e-6 更严：系数是 dex/单位坐标的量）
constexpr double kIrlsConverge = 1e-9;
// 空间项幅度合理性硬界（declared convention; 见 fit 内的逐条论证）。
// 判的是**施加到像素上的场** max|log10 m| 在视场上的取值（不是系数本身）:
// 系数大小与场幅度只有在基函数于视场上 O(1) 变化时才等价; 星区极窄时基函数在
// 星区上几乎不变, 一个"看着不大"的系数可以外推成整帧的巨修正 ⇒ 必须判场。
constexpr double kMaxSpatialAmplitudeDex = 1.0;
// 视场诊断网格的采样上限（噪声底/峰峰值诊断用；不参与拟合与像素施加）
constexpr int kFrameGridMaxSamples = 2000000;

double median_of(std::vector<double> v) {
    if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
    const std::size_t n = v.size();
    std::sort(v.begin(), v.end());
    return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

double mad_scale(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    const double med = median_of(v);
    std::vector<double> dev;
    dev.reserve(v.size());
    for (double x : v) dev.push_back(std::fabs(x - med));
    const double mad = median_of(dev);
    return (mad > 0.0) ? (mad / kMadScale) : 0.0;
}

// 在给定权重下构建 H = Σ w·B̃ B̃ᵀ（J×J, row-major）与加权中心化均值 meanB。
// 返回 false ⇔ 权重和 <= 0（退化）。
bool build_weighted_H(int order, const std::vector<double>& xt,
                      const std::vector<double>& yt, const std::vector<double>& w,
                      std::vector<double>* H, std::vector<double>* meanB,
                      std::vector<char>* col_const_out = nullptr) {
    const int J = calibration::photo_spatial_nterm(order);
    const int N = static_cast<int>(xt.size());
    H->assign(static_cast<std::size_t>(J) * J, 0.0);
    meanB->assign(static_cast<std::size_t>(J), 0.0);
    double sw = 0.0;
    double B[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    for (int i = 0; i < N; ++i) {
        calibration::photo_spatial_basis(order, xt[i], yt[i], B);
        const double wi = w[i];
        for (int j = 0; j < J; ++j) (*meanB)[j] += wi * B[j];
        sw += wi;
    }
    if (!(sw > 0.0) || !std::isfinite(sw)) return false;
    for (int j = 0; j < J; ++j) (*meanB)[j] /= sw;

    // ── 物理上恒定的基函数列 ⇒ 精确置零（**阈值无关**的离散判据）─────────────
    // 为什么必须显式做: 中心化是浮点运算, 「样本上恒定的列」得到的 B̃ 只有 ~1e-19
    // 量级的舍入残差。p2_identifiability_assess 的判据建在**列均衡**后的矩阵上
    // （消除参数单位自由度 ⇒ 与列尺度无关），因此它**看不见**这种舍入级列: 均衡后
    // 该列仍是单位列, 会判「可辨识」, 解出的系数随之爆炸（实测 |coef|=2.1e14）。
    // 本函数把这种列精确置零, 于是 H_ii == 0, 直接落入该判据文件头定义的
    // 「该参数完全没有信息」语义（n_unidentified++、κ=+inf、identifiable=0）。
    // 依据: 判据头文件 identifiability.h 的「H_ii == 0 ⇒ 无信息方向」条款 +
    //       本模块「绝不发布未被约束的场」的降级要求（spatial_gain.h 文件头）。
    std::vector<char> col_const(static_cast<std::size_t>(J), 0);
    {
        std::vector<double> bmin(static_cast<std::size_t>(J),
                                 std::numeric_limits<double>::infinity());
        std::vector<double> bmax(static_cast<std::size_t>(J),
                                 -std::numeric_limits<double>::infinity());
        for (int i = 0; i < N; ++i) {
            if (w[i] == 0.0) continue;
            calibration::photo_spatial_basis(order, xt[i], yt[i], B);
            for (int j = 0; j < J; ++j) {
                bmin[j] = std::min(bmin[j], B[j]);
                bmax[j] = std::max(bmax[j], B[j]);
            }
        }
        for (int j = 0; j < J; ++j)
            col_const[j] = (bmax[j] == bmin[j]) ? 1 : 0;   // 精确相等, 无阈值
    }
    if (col_const_out != nullptr) *col_const_out = col_const;

    std::vector<double> Bt(static_cast<std::size_t>(J), 0.0);
    for (int i = 0; i < N; ++i) {
        calibration::photo_spatial_basis(order, xt[i], yt[i], B);
        const double wi = w[i];
        if (wi == 0.0) continue;
        for (int a = 0; a < J; ++a)
            Bt[a] = col_const[a] ? 0.0 : (B[a] - (*meanB)[a]);
        for (int a = 0; a < J; ++a) {
            for (int b = 0; b <= a; ++b)
                (*H)[static_cast<std::size_t>(a) * J + b] += wi * Bt[a] * Bt[b];
        }
    }
    for (int a = 0; a < J; ++a)
        for (int b = 0; b < a; ++b)
            (*H)[static_cast<std::size_t>(b) * J + a] =
                (*H)[static_cast<std::size_t>(a) * J + b];
    return true;
}

// 可辨识性：p2_identifiability_assess（未正则化信息矩阵 H = Σ w B̃B̃ᵀ）。
// 返回 0 = 评估成功（含判红）；1 = 参数错误（H 对角为负/非有限 ⇒ 设计矩阵有缺陷）。
int assess(int order, const std::vector<double>& xt, const std::vector<double>& yt,
           const std::vector<double>& w, P2Identifiability* out) {
    std::vector<double> H, meanB;
    if (!build_weighted_H(order, xt, yt, w, &H, &meanB)) return 1;
    const int J = calibration::photo_spatial_nterm(order);
    return p2_identifiability_assess(H.data(), static_cast<std::uint64_t>(J),
                                     static_cast<std::uint64_t>(xt.size()), 0.0, out);
}

struct FitOutcome {
    std::vector<double> coef;
    std::vector<double> w;
    std::vector<double> resid;
    std::vector<double> meanB;   // 收敛时的加权中心化均值（规范中心, **必须发布**）
    std::vector<char> col_const; // 恒定基函数列标记（诊断）
    int iterations = 0;
    int n_used = 0;
    double sigma_spatial_dex = 0.0;
};

// 阶段 3 的加权 Tukey-IRLS 空间拟合的三种结局。分开「无信息方向」与「数值失败」:
//   kOk         拟合成功
//   kDeadColumn 某个基函数列在**实际使用的样本**上恒定（无信息方向）⇒ 降阶
//   kSolveFailed 数值失败（H 非正定 / 权重全零 / 非有限）⇒ fail-closed
enum class FitStatus { kOk, kDeadColumn, kSolveFailed };

FitStatus irls_fit(int order, const std::vector<double>& xt, const std::vector<double>& yt,
                   const std::vector<double>& r, double location_dex, double S,
                   FitOutcome* out) {
    const int J = calibration::photo_spatial_nterm(order);
    const int N = static_cast<int>(xt.size());
    std::vector<double> w(static_cast<std::size_t>(N), 1.0);
    std::vector<double> coef(static_cast<std::size_t>(J), 0.0);
    std::vector<double> cnew(static_cast<std::size_t>(J), 0.0);
    std::vector<double> H, meanB, g(static_cast<std::size_t>(J), 0.0);
    std::vector<double> Bt(static_cast<std::size_t>(J), 0.0);
    std::vector<double> resid(static_cast<std::size_t>(N), 0.0);
    std::vector<char> dead;
    double B[5] = {0.0, 0.0, 0.0, 0.0, 0.0};

    int iters = 0;
    for (int iter = 0; iter < kIrlsMaxIter; ++iter) {
        iters = iter + 1;
        if (!build_weighted_H(order, xt, yt, w, &H, &meanB, &dead)) return FitStatus::kSolveFailed;
        for (int j = 0; j < J; ++j)
            if (dead[static_cast<std::size_t>(j)]) return FitStatus::kDeadColumn;
        std::fill(g.begin(), g.end(), 0.0);
        for (int i = 0; i < N; ++i) {
            const double wi = w[i];
            if (wi == 0.0) continue;
            calibration::photo_spatial_basis(order, xt[i], yt[i], B);
            const double d = r[i] - location_dex;
            for (int a = 0; a < J; ++a) Bt[a] = B[a] - meanB[a];
            // 设计方程: Σ_j c_j·B̃_j ≈ d (= r − L)。因 log10 m = −Σ c B̃ = L − r_surf，
            // 这一步解出的 c 直接给出 log10 m（符号见 photometry_apply.h 的求值）。
            for (int a = 0; a < J; ++a) g[a] += wi * Bt[a] * d;
        }
        if (!spatial_gain_solve_spd(H.data(), g.data(), J, cnew.data()))
            return FitStatus::kSolveFailed;
        double max_dc = 0.0;
        for (int j = 0; j < J; ++j) {
            max_dc = std::max(max_dc, std::fabs(cnew[j] - coef[j]));
            coef[j] = cnew[j];
        }
        // 残差 r_i − surf(x_i,y_i) = (r_i − L) − Σ_j c_j·B̃_j(i)
        for (int i = 0; i < N; ++i) {
            calibration::photo_spatial_basis(order, xt[i], yt[i], B);
            double s = 0.0;
            for (int j = 0; j < J; ++j) s += coef[j] * (B[j] - meanB[j]);
            resid[i] = (r[i] - location_dex) - s;
        }
        const double cS = kTukeyC * S;
        for (int i = 0; i < N; ++i) {
            const double u = resid[i] / cS;
            const double au = std::fabs(u);
            w[i] = (au < 1.0) ? ((1.0 - u * u) * (1.0 - u * u)) : 0.0;
        }
        if (max_dc < kIrlsConverge) break;
    }
    out->coef = coef;
    out->w = w;
    out->resid = resid;
    out->meanB = meanB;
    out->iterations = iters;
    int used = 0;
    std::vector<double> res_in;
    res_in.reserve(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        if (w[i] > 0.0) { ++used; res_in.push_back(resid[i]); }
    }
    out->n_used = used;
    out->sigma_spatial_dex = mad_scale(res_in);
    out->col_const = dead;
    return (used > 0) ? FitStatus::kOk : FitStatus::kSolveFailed;
}

}  // namespace

const char* spatial_gain_status_name(SpatialGainStatus s) {
    switch (s) {
        case SpatialGainStatus::kDisabled: return "disabled";
        case SpatialGainStatus::kOk: return "ok";
        case SpatialGainStatus::kDegradedCoverage: return "degraded_coverage";
        case SpatialGainStatus::kDegradedStars: return "degraded_stars";
        case SpatialGainStatus::kDegradedRank: return "degraded_rank";
        case SpatialGainStatus::kDegradedZeroScatter: return "degraded_zero_scatter";
        case SpatialGainStatus::kDegradedSolve: return "degraded_solve";
    }
    return "unknown";
}

bool spatial_gain_solve_spd(const double* H, const double* g, int n, double* c) {
    if (H == nullptr || g == nullptr || c == nullptr || n <= 0 || n > 16) return false;
    double L[16 * 16];
    for (int i = 0; i < n * n; ++i) L[i] = 0.0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = H[static_cast<std::size_t>(i) * n + j];
            for (int k = 0; k < j; ++k)
                sum -= L[static_cast<std::size_t>(i) * n + k] *
                       L[static_cast<std::size_t>(j) * n + k];
            if (i == j) {
                if (!(sum > 0.0) || !std::isfinite(sum)) return false;
                L[static_cast<std::size_t>(i) * n + i] = std::sqrt(sum);
            } else {
                const double d = L[static_cast<std::size_t>(j) * n + j];
                if (!(d > 0.0)) return false;
                L[static_cast<std::size_t>(i) * n + j] = sum / d;
            }
        }
    }
    double y[16];
    for (int i = 0; i < n; ++i) {
        double sum = g[i];
        for (int k = 0; k < i; ++k)
            sum -= L[static_cast<std::size_t>(i) * n + k] * y[k];
        y[i] = sum / L[static_cast<std::size_t>(i) * n + i];
    }
    for (int i = n - 1; i >= 0; --i) {
        double sum = y[i];
        for (int k = i + 1; k < n; ++k)
            sum -= L[static_cast<std::size_t>(k) * n + i] * c[k];
        c[i] = sum / L[static_cast<std::size_t>(i) * n + i];
    }
    for (int i = 0; i < n; ++i)
        if (!std::isfinite(c[i])) return false;
    return true;
}

SpatialGainGrid spatial_gain_frame_grid(int width, int height, int max_samples) {
    SpatialGainGrid gr;
    if (width <= 0 || height <= 0) return gr;
    const double npix = static_cast<double>(width) * static_cast<double>(height);
    const double cap = (max_samples > 0) ? static_cast<double>(max_samples) : npix;
    int step = 1;
    if (npix > cap) {
        step = static_cast<int>(std::ceil(std::sqrt(npix / cap)));
        if (step < 1) step = 1;
    }
    gr.dx = static_cast<double>(step);
    gr.dy = static_cast<double>(step);
    gr.nx = (width + step - 1) / step;
    gr.ny = (height + step - 1) / step;
    return gr;
}

SpatialGainField fit_spatial_gain(const SpatialGainSample* s, int n,
                                  const SpatialGainParams& p) {
    SpatialGainField f;
    f.order_requested = p.order_requested;
    f.sigma_global_dex = 0.0;   // 由调用方回填（本模块不接收阶段 2 的散度，只做诊断对齐）
    // 规范字符串（必须落盘；见 spatial_gain.h 的规范自由度论证）
    f.gauge = "weighted_mean_over_fitted_stars(log10 m)=0 (Tukey weights) <=> "
              "weighted geometric mean of m over the fitted star set = 1; "
              "k_photo stays the global term (10^-location), unchanged";
    // 坐标归一化基准 = **帧几何**（与星分布无关 ⇒ 系数跨帧可比、可复现）
    f.x_ref = 0.5 * static_cast<double>(p.width - 1);
    f.y_ref = 0.5 * static_cast<double>(p.height - 1);
    f.x_scale = (p.width > 1) ? (0.5 * static_cast<double>(p.width - 1)) : 1.0;
    f.y_scale = (p.height > 1) ? (0.5 * static_cast<double>(p.height - 1)) : 1.0;

    if (p.order_requested <= 0) {
        f.order = 0;
        f.status = SpatialGainStatus::kDisabled;
        f.degraded_reason = "spatial_gain_disabled(order_requested=0)";
        return f;
    }
    const int order_req = std::min(p.order_requested, 2);
    if (s == nullptr || n <= 0 || p.width <= 0 || p.height <= 0) {
        f.order = 0;
        f.status = SpatialGainStatus::kDegradedSolve;
        f.frame_fail = true;
        f.degraded_reason = "invalid_request(null_samples_or_nonpositive_frame_geometry)";
        return f;
    }

    // ---- 有效样本（有限 x/y/r；非有限样本显式剔除，计数保留在 n_stars 之外）----
    std::vector<double> xs, ys, rs;
    xs.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (std::isfinite(s[i].x) && std::isfinite(s[i].y) && std::isfinite(s[i].r)) {
            xs.push_back(s[i].x);
            ys.push_back(s[i].y);
            rs.push_back(s[i].r);
        }
    }
    const int N = static_cast<int>(xs.size());
    f.n_stars = N;
    // 逐星样本（审计证据; 见 spatial_gain.h）——在此处填, 使任何降级路径也都带样本
    f.sample_x = xs;
    f.sample_y = ys;
    f.sample_r = rs;
    if (N < 3) {
        f.order = 0;
        f.status = SpatialGainStatus::kDegradedStars;
        f.degraded_reason = "stars_below_3(n_valid=" + std::to_string(N) + ")";
        return f;
    }

    // ---- 覆盖（§2.5）: 3×3 分块 + 逐轴包围盒展幅 ----
    {
        const int G = std::max(1, p.coverage_block_grid);
        std::vector<int> cnt(static_cast<std::size_t>(G) * G, 0);
        double xmin = xs[0], xmax = xs[0], ymin = ys[0], ymax = ys[0];
        for (int i = 0; i < N; ++i) {
            xmin = std::min(xmin, xs[i]); xmax = std::max(xmax, xs[i]);
            ymin = std::min(ymin, ys[i]); ymax = std::max(ymax, ys[i]);
            int bx = static_cast<int>(std::floor(xs[i] / p.width * G));
            int by = static_cast<int>(std::floor(ys[i] / p.height * G));
            bx = std::min(std::max(bx, 0), G - 1);
            by = std::min(std::max(by, 0), G - 1);
            cnt[static_cast<std::size_t>(by) * G + bx] += 1;
        }
        int blocks = 0;
        for (int k = 0; k < G * G; ++k)
            if (cnt[static_cast<std::size_t>(k)] >= p.coverage_min_stars_per_block) ++blocks;
        f.coverage_blocks = blocks;
        const double fx = (xmax - xmin) / static_cast<double>(p.width);
        const double fy = (ymax - ymin) / static_cast<double>(p.height);
        f.coverage_bbox_frac = std::min(fx, fy);
    }
    // 归一化坐标（只算一次）
    std::vector<double> xt(static_cast<std::size_t>(N)), yt(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        xt[i] = (xs[i] - f.x_ref) / f.x_scale;
        yt[i] = (ys[i] - f.y_ref) / f.y_scale;
    }

    // ---- 阶数阶梯：预检（无权设计几何 + N_min）----
    std::vector<double> w_unit(static_cast<std::size_t>(N), 1.0);
    int chosen = 0;
    std::string why;
    bool stars_blocked = false;
    for (int ord = order_req; ord >= 1; --ord) {
        const int nmin = (ord == 1) ? p.min_stars_order1 : p.min_stars_order2;
        if (N < nmin) {
            stars_blocked = true;
            why = "stars_below_min_order" + std::to_string(ord) +
                  "(n_valid=" + std::to_string(N) + "<n_min=" + std::to_string(nmin) + ")";
            continue;
        }
        stars_blocked = false;
        P2Identifiability id;
        const int rc = assess(ord, xt, yt, w_unit, &id);
        if (rc != 0) {
            why = "assess_error_order" + std::to_string(ord);
            continue;
        }
        if (id.identifiable) {
            chosen = ord;
            break;
        }
        why = "rank_deficient_precheck_order" + std::to_string(ord) +
              "(n_unidentified=" + std::to_string(id.n_unidentified) +
              ", kappa=" + std::to_string(id.kappa) + ")";
    }
    if (chosen == 0) {
        f.order = 0;
        f.status = stars_blocked ? SpatialGainStatus::kDegradedStars
                                 : SpatialGainStatus::kDegradedRank;
        f.degraded_reason = why;
        return f;
    }

    // ---- 覆盖门（§2.5）：放在星数/秩阶梯之后，使 degraded_reason 报**根因**
    //      （星数不足 vs 分布聚集 vs 加权秩亏），而不是被最先命中的那一项掩盖。----
    if (p.coverage_check &&
        (f.coverage_blocks < p.coverage_min_blocks ||
         f.coverage_bbox_frac < p.coverage_min_bbox_frac)) {
        char buf[192];
        std::snprintf(buf, sizeof(buf),
                      "degraded_coverage(blocks=%d<%d, bbox_frac=%.4f<%.4f)",
                      f.coverage_blocks, p.coverage_min_blocks,
                      f.coverage_bbox_frac, p.coverage_min_bbox_frac);
        f.order = 0;
        f.status = SpatialGainStatus::kDegradedCoverage;
        f.degraded_reason = buf;
        return f;
    }

    // ---- 阶段 3 固定尺度（与 star_matcher 的固定尺度 M 估计路线同口径）----
    std::vector<double> d(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) d[i] = rs[i] - p.location_dex;
    const double S = mad_scale(d);
    f.tukey_scale_dex = S;
    if (!(S > 0.0) || !std::isfinite(S)) {
        f.order = 0;
        f.status = SpatialGainStatus::kDegradedZeroScatter;
        f.degraded_reason = "zero_residual_scatter(MAD(r-location)=0)";
        return f;
    }

    // ---- 拟合 + 后检（最终 Tukey 权重的 H）；判红则继续降阶 ----
    // 结局按**最后一次尝试**归因（根因优先）：无信息方向/秩亏 ⇒ 声明降级；
    // 数值失败/幅度不可信 ⇒ fail-closed（该帧判 fail）。
    enum class FailKind { kNone, kInfo, kHard };
    FailKind last_kind = FailKind::kNone;
    for (int ord = chosen; ord >= 1; --ord) {
        FitOutcome o;
        const FitStatus st = irls_fit(ord, xt, yt, rs, p.location_dex, S, &o);
        if (st == FitStatus::kDeadColumn) {
            // 无信息方向（该阶的某个基函数列在实际使用的样本上恒定）⇒ 降阶。
            // 这不是缺陷, 是**声明过的降级**路径（fetch 判据的 H_ii==0 语义）。
            why = "constant_basis_column_order" + std::to_string(ord) +
                  "(no information on that basis direction in the fitted sample)";
            last_kind = FailKind::kInfo;
            continue;
        }
        if (st == FitStatus::kSolveFailed) {
            why = "solve_failed_order" + std::to_string(ord);
            last_kind = FailKind::kHard;
            continue;
        }
        P2Identifiability id;
        const int rc = assess(ord, xt, yt, o.w, &id);
        if (rc != 0) {
            last_kind = FailKind::kHard;
            why = "assess_error_weighted_order" + std::to_string(ord);
            continue;
        }
        f.identifiable = id.identifiable;
        f.rank_rtol_eff = id.rank_rtol_effective;
        f.kappa = id.kappa;
        f.rank_eff = id.rank_eff;
        f.n_unidentified = id.n_unidentified;
        if (!id.identifiable) {
            why = "rank_deficient_postcheck_order" + std::to_string(ord) +
                  "(n_unidentified=" + std::to_string(id.n_unidentified) +
                  ", kappa=" + std::to_string(id.kappa) + ")";
            last_kind = FailKind::kInfo;
            continue;
        }
        // ── 幅度合理性硬界（declared convention）────────────────────────────
        // 全局 k_photo 承载帧级零点（真实量级 ~1e-17）; m(x,y) 只承载**帧内**残余，
        // 其幅度在真实数据上是百分位量级（实验 §3.1 真值 pp 7%；本轮真机 3 帧实测
        // max|log10 m| ≈ 0.1 dex）。任何整帧 max|log10 m| > 1.0 dex（10× 通量）
        // 的解释都只能是求解伪迹 ⇒ 判为缺陷并 fail-closed（帧级失败作用域）。
        // 该界是**约定值**（与 c=4.685、3× 抽样允差同类），不构成效应量门。
        // 判**场**而不判系数: 星区极窄时基函数在星区上几乎不变, 一个 O(1) 的系数
        // 能外推成整帧的巨修正（S3e 负例正是这一形态）。
        {
            const SpatialGainGrid gg = spatial_gain_frame_grid(p.width, p.height,
                                                               kFrameGridMaxSamples);
            double amax = 0.0;
            for (int gy = 0; gy < gg.ny; ++gy) {
                const double y = std::min(static_cast<double>(gy) * gg.dy,
                                          static_cast<double>(p.height - 1));
                for (int gx = 0; gx < gg.nx; ++gx) {
                    const double x = std::min(static_cast<double>(gx) * gg.dx,
                                              static_cast<double>(p.width - 1));
                    const double xtv = (x - f.x_ref) / f.x_scale;
                    const double ytv = (y - f.y_ref) / f.y_scale;
                    double B[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
                    calibration::photo_spatial_basis(ord, xtv, ytv, B);
                    double lm = 0.0;
                    for (int j = 0; j < calibration::photo_spatial_nterm(ord); ++j)
                        lm -= o.coef[static_cast<std::size_t>(j)] *
                              (B[j] - o.meanB[static_cast<std::size_t>(j)]);
                    amax = std::max(amax, std::fabs(lm));
                }
            }
            if (!std::isfinite(amax) || amax > kMaxSpatialAmplitudeDex) {
                last_kind = FailKind::kHard;
                why = "implausible_spatial_amplitude_order" + std::to_string(ord) +
                      "(max|log10 m|=" + std::to_string(amax) + ">" +
                      std::to_string(kMaxSpatialAmplitudeDex) + " dex)";
                continue;
            }
        }
        // 发布：可辨识（identifiable==1）的场
        f.order = ord;
        f.status = SpatialGainStatus::kOk;
        // 规范中心：拟合样本上（最终 Tukey 权重加权）的基函数均值。**必须发布**，
        // 施加端按 B̃ = B − center 求值（否则 k_photo 的全局语义被暗改）。
        for (int j = 0; j < 5; ++j)
            f.center[j] = (j < calibration::photo_spatial_nterm(ord))
                              ? o.meanB[static_cast<std::size_t>(j)] : 0.0;
        // 基函数清单（与 photo_spatial_basis 的列序严格同序；坐标为归一化坐标，
        // 拟合中按样本 Tukey 权重中心化 ⇒ 清单只描述"哪一列"，中心化在规范里声明）
        {
          static const char* kNames[5] = {"x_norm", "y_norm", "x_norm^2",
                                          "x_norm*y_norm", "y_norm^2"};
          f.basis_names.clear();
          for (int j = 0; j < calibration::photo_spatial_nterm(ord); ++j)
              f.basis_names.push_back(kNames[j]);
        }
        for (int j = 0; j < 5; ++j) f.coef[j] = 0.0;
        for (int j = 0; j < calibration::photo_spatial_nterm(ord); ++j)
            f.coef[j] = o.coef[static_cast<std::size_t>(j)];
        f.n_used = o.n_used;
        f.n_outliers = N - o.n_used;
        f.sample_used.assign(static_cast<std::size_t>(N), 0);
        for (int i = 0; i < N; ++i)
            f.sample_used[static_cast<std::size_t>(i)] = (o.w[static_cast<std::size_t>(i)] > 0.0) ? 1 : 0;
        f.tukey_iterations = o.iterations;
        f.sigma_spatial_dex = o.sigma_spatial_dex;
        f.degraded_reason.clear();

        // ---- 诊断: 视场上的幅度统计 + 解析噪声底 ----
        // 噪声底 = S·sqrt(B̃ᵀ H⁻¹ B̃) 在视场上的**均值**（noise_floor_dex）与
        // **最大值**（noise_floor_max_dex）；B̃ 用拟合样本的加权中心化均值 center。
        const int J = calibration::photo_spatial_nterm(ord);
        std::vector<double> H, meanB;
        if (build_weighted_H(ord, xt, yt, o.w, &H, &meanB)) {
            // H⁻¹（逐列解 H·X = e_b）
            std::vector<double> Hinv(static_cast<std::size_t>(J) * J, 0.0);
            bool hok = true;
            for (int b = 0; b < J && hok; ++b) {
                std::vector<double> e(static_cast<std::size_t>(J), 0.0);
                std::vector<double> sol(static_cast<std::size_t>(J), 0.0);
                e[static_cast<std::size_t>(b)] = 1.0;
                if (!spatial_gain_solve_spd(H.data(), e.data(), J, sol.data())) {
                    hok = false;
                    break;
                }
                for (int a = 0; a < J; ++a)
                    Hinv[static_cast<std::size_t>(a) * J + b] = sol[static_cast<std::size_t>(a)];
            }
            if (hok) {
                const SpatialGainGrid gr = spatial_gain_frame_grid(p.width, p.height,
                                                                   kFrameGridMaxSamples);
                double lmin = 0.0, lmax = 0.0, sum_lm = 0.0, sumsq_lm = 0.0;
                double sum_v = 0.0, max_v = 0.0;
                long long npix = 0;
                for (int gy = 0; gy < gr.ny; ++gy) {
                    const double y = std::min(static_cast<double>(gy) * gr.dy,
                                              static_cast<double>(p.height - 1));
                    for (int gx = 0; gx < gr.nx; ++gx) {
                        const double x = std::min(static_cast<double>(gx) * gr.dx,
                                                  static_cast<double>(p.width - 1));
                        const double xtv = (x - f.x_ref) / f.x_scale;
                        const double ytv = (y - f.y_ref) / f.y_scale;
                        double B[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
                        calibration::photo_spatial_basis(ord, xtv, ytv, B);
                        double Bt[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
                        double lm = 0.0;
                        for (int j = 0; j < J; ++j) {
                            Bt[j] = B[j] - meanB[static_cast<std::size_t>(j)];
                            lm += f.coef[j] * Bt[j];
                        }
                        double v = 0.0;
                        for (int a = 0; a < J; ++a)
                            for (int b2 = 0; b2 < J; ++b2)
                                v += Bt[a] * Hinv[static_cast<std::size_t>(a) * J + b2] * Bt[b2];
                        if (npix == 0) { lmin = lm; lmax = lm; max_v = v; }
                        lmin = std::min(lmin, lm);
                        lmax = std::max(lmax, lm);
                        max_v = std::max(max_v, v);
                        sum_lm += lm;
                        sumsq_lm += lm * lm;
                        sum_v += v;
                        ++npix;
                    }
                }
                if (npix > 0) {
                    const double n = static_cast<double>(npix);
                    f.field_ptp_dex = std::fabs(lmax - lmin);
                    const double mean_lm = sum_lm / n;
                    const double var_lm = std::max(0.0, sumsq_lm / n - mean_lm * mean_lm);
                    f.field_rms_dex = std::sqrt(var_lm);
                    const double mean_v = sum_v / n;
                    if (mean_v > 0.0) f.noise_floor_dex = S * std::sqrt(mean_v);
                    if (max_v > 0.0) f.noise_floor_max_dex = S * std::sqrt(max_v);
                    // 逐系数解析标准差 sqrt(S²·(H⁻¹)_jj)（诊断：系数是否受数据约束）
                    for (int j = 0; j < J; ++j) {
                        const double djj = Hinv[static_cast<std::size_t>(j) * J + j];
                        f.coef_sigma_dex[j] = (djj > 0.0) ? (S * std::sqrt(djj)) : 0.0;
                    }
                }
            }
        }
        return f;
    }

    f.order = 0;
    f.status = (last_kind == FailKind::kHard) ? SpatialGainStatus::kDegradedSolve
                                              : SpatialGainStatus::kDegradedRank;
    // 几何充分（预检通过）却在**最后一次尝试**遭遇数值失败/幅度不可信 ⇒ 缺陷,
    // fail-closed（帧级失败作用域）；信息不足（无信息方向/秩亏）是**声明降级**。
    f.frame_fail = (last_kind == FailKind::kHard);
    f.degraded_reason = why;
    return f;
}

}  // namespace photometry
}  // namespace astrocs
