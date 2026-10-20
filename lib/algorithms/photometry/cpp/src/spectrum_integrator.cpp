// spectrum_integrator.cpp - 光谱积分器实现
// 契约锚: docs/science/algorithms/PHOTOMETRIC_FIT.md (ALG-PHOTOMETRIC-FIT-*) + docs/science/PHOTOMETRY.md (SCI-PHOT-001) ; 数值: Akima 子样条(fill=0)+Simpson 1/3 复合(奇数末3用 3/8, n==1 退梯形) — 实现锚 spectrum_integrator.h 头注释.
// 算法: Akima 子样条插值 + Simpson 1/3 复合积分（数值口径见 spectrum_integrator.h 头注释）
// 对照: scipy Akima1DInterpolator + simpson（方法对照，非代码引用）
// 日志: 输出到 stderr, 前缀 [spec_int]

#include "spectrum_integrator.h"
#include "log_macros.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace photo_calib {

// ----------------------------------------------------------------------------
// 内部工具: 排序 + 去重 (参考 python _prepare_curve)
// ----------------------------------------------------------------------------
static void prepare_curve(const std::vector<double>& wl_in,
                          const std::vector<double>& val_in,
                          std::vector<double>& wl_out,
                          std::vector<double>& val_out) {
    const int n = (int)wl_in.size();
    std::vector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;
    std::stable_sort(idx.begin(), idx.end(),
                     [&](int a, int b) { return wl_in[a] < wl_in[b]; });

    wl_out.clear();
    val_out.clear();
    wl_out.reserve(n);
    val_out.reserve(n);
    for (int i = 0; i < n; ++i) {
        double w = wl_in[idx[i]];
        if (!wl_out.empty() && w <= wl_out.back()) continue; // 严格递增, 去重
        wl_out.push_back(w);
        val_out.push_back(val_in[idx[i]]);
    }
}

// ----------------------------------------------------------------------------
// Akima 子样条插值
// ----------------------------------------------------------------------------
// 唯一实现 = AkimaSpline（下方）。它把「斜率 → 扩展斜率 → 切线」一次性算好,
// 之后可在**任意点**求值（范围外返回 fill）。
// 历史 akima_interpolate 的**逐位**行为由本类承担：A/B 判据见
// eng/tests/unit/narrowband_response_test.cpp（与重构前的实现逐位比对）。
namespace {

class AkimaSpline {
public:
    // 成功返回 true；点数 <2 或 x 非严格递增 ⇒ false（调用方具名上报）。
    bool init(const std::vector<double>& x, const std::vector<double>& y) {
        x_ = x;
        y_ = y;
        const int n = static_cast<int>(x.size());
        if (n < 2) return false;

        // 原始斜率 m[i] = (y[i+1]-y[i])/(x[i+1]-x[i]), i=0..n-2
        std::vector<double> slope(n - 1, 0.0);
        for (int i = 0; i < n - 1; ++i) {
            const double dx = x_[i + 1] - x_[i];
            if (dx <= 0.0) {
                bad_index_ = i;
                bad_dx_ = dx;
                return false;
            }
            slope[i] = (y_[i + 1] - y_[i]) / dx;
        }

        // 扩展斜率数组 ext_m[0..n+2], ext_m[j] = m[j-2]（m[-2]..m[n]）。
        // 切线 t[i] 需要 m[i-2..i+1]，i=n-1 时需要 m[n] —— 必须预留 n+3 项，
        // 否则最后一切线越界（ sanitizer 实测 heap-buffer-overflow）。
        // n==2 时仅 slope[0] 存在：边界斜率退化为复制（线性段）。
        std::vector<double> ext_m(n + 3, 0.0);
        ext_m[0] = (n >= 3) ? 3.0 * slope[0] - 2.0 * slope[1] : slope[0];
        ext_m[1] = (n >= 3) ? 2.0 * slope[0] - slope[1] : slope[0];
        for (int i = 0; i < n - 1; ++i) ext_m[2 + i] = slope[i]; // m[0..n-2]
        ext_m[n + 1] = (n >= 3)
            ? 2.0 * slope[n - 2] - slope[n - 3] : slope[n - 2];  // m[n-1]
        ext_m[n + 2] = (n >= 3)
            ? 3.0 * slope[n - 2] - 2.0 * slope[n - 3] : slope[n - 2];  // m[n]

        // 每个数据点 i 的切线 t[i]
        t_.assign(n, 0.0);
        for (int i = 0; i < n; ++i) {
            const double m_left2 = ext_m[i];      // m[i-2]
            const double m_left1 = ext_m[i + 1];  // m[i-1]
            const double m_cur   = ext_m[i + 2];  // m[i]
            const double m_right = ext_m[i + 3];  // m[i+1]
            const double w1 = std::fabs(m_right - m_cur);
            const double w2 = std::fabs(m_left1 - m_left2);
            if (w1 + w2 == 0.0) {
                t_[i] = 0.5 * (m_left1 + m_cur);
            } else {
                t_[i] = (w1 * m_left1 + w2 * m_cur) / (w1 + w2);
            }
        }
        return true;
    }

    bool ok() const { return !t_.empty(); }
    int bad_index() const { return bad_index_; }
    double bad_dx() const { return bad_dx_; }

    // 求值：范围外返回 fill（与历史 akima_interpolate 同语义）。
    double eval(double x, double fill) const {
        const int n = static_cast<int>(x_.size());
        if (x < x_[0] || x > x_[n - 1]) return fill;   // 范围外钳位
        // 二分查找区间 j: x_[j] <= x <= x_[j+1]
        int lo = 0, hi = n - 2, j = 0;
        while (lo <= hi) {
            const int mid = (lo + hi) / 2;
            if (x >= x_[mid] && x <= x_[mid + 1]) { j = mid; break; }
            if (x < x_[mid]) hi = mid - 1; else lo = mid + 1;
        }
        const double x0 = x_[j], x1 = x_[j + 1];
        const double y0 = y_[j], y1 = y_[j + 1];
        const double dx = x1 - x0;
        const double s = (x - x0) / dx;
        // Hermite 基函数
        const double h00 = (2.0 * s - 3.0) * s * s + 1.0;
        const double h10 = ((s - 2.0) * s + 1.0) * s;
        const double h01 = (-2.0 * s + 3.0) * s * s;
        const double h11 = (s - 1.0) * s * s;
        return h00 * y0 + h10 * dx * t_[j] + h01 * y1 + h11 * dx * t_[j + 1];
    }

private:
    std::vector<double> x_, y_, t_;
    int bad_index_ = -1;
    double bad_dx_ = 0.0;
};

} // namespace

std::vector<double> akima_interpolate(
    const std::vector<double>& x_src, const std::vector<double>& y_src,
    const std::vector<double>& x_dst, double fill) {
    const int n = (int)x_src.size();
    const int m = (int)x_dst.size();
    std::vector<double> y_dst(m, fill);

    if (n < 2 || m == 0) {
        if (n < 2) std::fprintf(stderr, "[spec_int] akima: 源点数 %d < 2, 返回 fill\n", n);
        return y_dst;
    }

    AkimaSpline spline;
    if (!spline.init(x_src, y_src)) {
        std::fprintf(stderr, "[spec_int] akima: x_src 非严格递增 @%d (dx=%.6g)\n",
                     spline.bad_index(), spline.bad_dx());
        return y_dst;
    }

    for (int k = 0; k < m; ++k) y_dst[k] = spline.eval(x_dst[k], fill);
    return y_dst;
}

// ----------------------------------------------------------------------------
// 光谱 nm 轴"严格等距"合法域判据
// 正本: docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md §Simpson 有效域 (Issue #12)
//
// 固定 n=N-1, h=(x_{N-1}-x_0)/n>0, eps=2^-52, M=max(|x_0|,|x_{N-1}|,h):
//     tau = min(1e-8*h, 16*eps*M)
//     forall i=1..n: x_i - x_{i-1} > 0 且 |(x_i-x_{i-1}) - h| <= tau
// 并要求坐标分辨率前提 16*eps*M <= 1e-8*h; 不满足时该轴在声明的相对判别率下
// 无法可靠识别等距 —— 具名拒绝, 不靠大 x 的机器舍入放宽容差。
// 不含不带单位的 max(1,|h|); 检查全部 n 个局部间隔, 绝不只比较首尾。
// ----------------------------------------------------------------------------
namespace {
bool spectral_grid_equidistant_nm(const std::vector<double>& x, const char** why) {
    const size_t N = x.size();
    if (N < 2) { *why = "N<2"; return false; }
    if (N > static_cast<size_t>(std::numeric_limits<int>::max())) {
        *why = "N>INT_MAX"; return false;
    }
    for (size_t i = 0; i < N; ++i) {
        if (!std::isfinite(x[i])) { *why = "x not finite"; return false; }
    }
    const int n_int = static_cast<int>(N) - 1;
    const double x0 = x[0];
    const double xN = x[N - 1];
    const double span = xN - x0;
    if (!std::isfinite(span)) { *why = "span not finite"; return false; }
    const double h = span / static_cast<double>(n_int);
    if (!(h > 0.0) || !std::isfinite(h)) { *why = "h<=0 or not finite"; return false; }
    const double eps = std::numeric_limits<double>::epsilon(); // 2^-52
    const double M = std::max(std::max(std::fabs(x0), std::fabs(xN)), h);
    const double resolution = 16.0 * eps * M;
    const double rel_tol = 1e-8 * h;
    if (!(resolution <= rel_tol)) { *why = "16*eps*M > 1e-8*h"; return false; }
    const double tau = std::min(rel_tol, resolution);
    for (int i = 1; i <= n_int; ++i) {
        const double step = x[static_cast<size_t>(i)] - x[static_cast<size_t>(i - 1)];
        if (!std::isfinite(step) || !(step > 0.0)) {
            *why = "axis not strictly increasing"; return false;
        }
        if (std::fabs(step - h) > tau) { *why = "step differs from h beyond tau"; return false; }
    }
    return true;
}
} // namespace

// ----------------------------------------------------------------------------
// Simpson 1/3 复合积分 (严格等距 nm 轴; 末尾奇数区间用 Simpson 3/8)
//
// 非法输入 (N<2、x/y 长度不等、非有限、非严格递增、非等距、坐标分辨率不足、
// 累加溢出) 一律返回 quiet NaN —— 绝不以 0 伪装成"合法零积分"; 合法网格上被积
// 函数恒零返回真 0。调用方 (cache/API 边界) 必须据非有限值判无效并具名上报。
// ----------------------------------------------------------------------------
double simpson_integrate(const std::vector<double>& x, const std::vector<double>& y) {
    const char* why = nullptr;
    if (!spectral_grid_equidistant_nm(x, &why))
        return std::numeric_limits<double>::quiet_NaN();
    if (y.size() != x.size()) return std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < y.size(); ++i) {
        if (!std::isfinite(y[i])) return std::numeric_limits<double>::quiet_NaN();
    }

    const int n_pts = static_cast<int>(x.size());
    const int n_int = n_pts - 1; // 区间数
    const double h = (x[static_cast<size_t>(n_pts) - 1] - x[0]) /
                     static_cast<double>(n_int);

    double result = std::numeric_limits<double>::quiet_NaN();

    if (n_int % 2 == 0) {
        // 纯 Simpson 1/3
        double sum = y[0] + y[static_cast<size_t>(n_pts) - 1];
        for (int i = 1; i < n_pts - 1; ++i) {
            sum += (i % 2 == 1 ? 4.0 : 2.0) * y[static_cast<size_t>(i)];
        }
        result = sum * h / 3.0;
    } else if (n_int >= 3) {
        // 奇数区间: 前 n_int-3 用 Simpson 1/3 (n_13 为偶数), 末尾 3 区间用 3/8。
        // n_int == 3 (4 点) 是退化分支: n_13 == 0 ⇒ 1/3 部分必须严格为 0
        // (历史实现在此把 y[0] 计两次, 使常数得 3.6667; 见 claim PHOT-SIMPSON-N3-001)。
        const int n_13 = n_int - 3;
        double sum = 0.0;
        if (n_13 > 0) {
            double s13 = y[0] + y[static_cast<size_t>(n_13)];
            for (int i = 1; i < n_13; ++i) {
                s13 += (i % 2 == 1 ? 4.0 : 2.0) * y[static_cast<size_t>(i)];
            }
            sum = s13 * h / 3.0;
        }
        sum += (y[static_cast<size_t>(n_13)] + 3.0 * y[static_cast<size_t>(n_13) + 1] +
                3.0 * y[static_cast<size_t>(n_13) + 2] +
                y[static_cast<size_t>(n_pts) - 1]) * 3.0 * h / 8.0;
        result = sum;
    } else {
        // n_int == 1: 单区间梯形
        result = 0.5 * h * (y[0] + y[1]);
    }
    if (!std::isfinite(result)) return std::numeric_limits<double>::quiet_NaN();
    return result;
}

// ----------------------------------------------------------------------------
// compute_f_syn: [非生产通道] 单星相对合成流量 (: 加入 CCD QE 曲线 Q(λ))
// 含 10^(-0.4*mag_g) 乘性归一; 生产定标走 compute_f_syn_cached_xpsd
// (权威: docs/science/PHOTOMETRY.md §2a, claim PHOT-FSYN-CANON-001)
// ----------------------------------------------------------------------------
double compute_f_syn(
    const uint8_t* spectrum_uint8, int spectrum_count,
    const double* spectrum_wl, int wl_count,
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    double mag_g) {

    if (spectrum_uint8 == nullptr || spectrum_wl == nullptr ||
        filter_wl == nullptr || filter_trans == nullptr) {
        std::fprintf(stderr, "[spec_int] compute_f_syn: 空指针参数\n");
        return 0.0;
    }
    if (spectrum_count <= 0 || wl_count <= 0 || filter_count <= 0) {
        std::fprintf(stderr, "[spec_int] compute_f_syn: 尺寸无效 spec=%d wl=%d filt=%d\n",
                    spectrum_count, wl_count, filter_count);
        return 0.0;
    }
    if (spectrum_count != wl_count) {
        std::fprintf(stderr, "[spec_int] compute_f_syn: spectrum_count(%d) != wl_count(%d)\n",
                    spectrum_count, wl_count);
        return 0.0;
    }
    // QE 可为 nullptr (向后兼容, Q(λ)=1.0); 若提供则 qe_trans 不可为空且 qe_count>0
    if (qe_wl != nullptr && (qe_trans == nullptr || qe_count <= 0)) {
        std::fprintf(stderr, "[spec_int] compute_f_syn: QE 参数不一致 (qe_wl 非空但 qe_trans/qe_count 无效)\n");
        return 0.0;
    }

    // [非生产通道] uint8 -> float64, 作为相对谱形, 并乘 10^(-0.4*mag_g) 做星等归一化
    // (该写法不用于生产定标: 量化参数逐星不同, 且 mag_g 因子给 r_i 注入 +0.4·G_i)
    double mag_factor = std::pow(10.0, -0.4 * mag_g);

    std::vector<double> sed_wl(spectrum_wl, spectrum_wl + wl_count);
    std::vector<double> sed_flux(wl_count, 0.0);
    for (int i = 0; i < wl_count; ++i) {
        sed_flux[i] = (double)spectrum_uint8[i] * mag_factor;
    }

    // 排序去重
    std::vector<double> sw, sf, fw, fv;
    prepare_curve(sed_wl, sed_flux, sw, sf);
    prepare_curve(std::vector<double>(filter_wl, filter_wl + filter_count),
                  std::vector<double>(filter_trans, filter_trans + filter_count),
                  fw, fv);

    if (sw.size() < 2 || fw.size() < 2) {
        std::fprintf(stderr, "[spec_int] compute_f_syn: 曲线点数不足 sed=%zu filt=%zu\n",
                    sw.size(), fw.size());
        return 0.0;
    }

    // QE 排序去重 (若有)
    std::vector<double> qw, qv;
    bool has_qe = false;
    if (qe_wl != nullptr && qe_count > 0) {
        prepare_curve(std::vector<double>(qe_wl, qe_wl + qe_count),
                      std::vector<double>(qe_trans, qe_trans + qe_count),
                      qw, qv);
        if (qw.size() >= 2) has_qe = true;
    }
    if (!has_qe) {
        LOG_INFO("[spec_int] compute_f_syn: QE curve not provided, F_syn without Q(λ)");
    }

    // 积分范围: SED 与滤光片波长交集 (QE 范围也参与, 若提供)
    double wl_min = std::max(sw.front(), fw.front());
    double wl_max = std::min(sw.back(), fw.back());
    if (has_qe) {
        wl_min = std::max(wl_min, qw.front());
        wl_max = std::min(wl_max, qw.back());
    }
    if (wl_min >= wl_max) {
        std::fprintf(stderr, "[spec_int] compute_f_syn: 波长重叠区间为空 [%.2f, %.2f]\n",
                    wl_min, wl_max);
        return 0.0;
    }

    // 等间距网格 (步长 1.0nm, 与 python _DEFAULT_WL_STEP 一致)
    const double wl_step = 1.0;
    int n_points = (int)std::round((wl_max - wl_min) / wl_step) + 1;
    if (n_points < 2) {
        std::fprintf(stderr, "[spec_int] compute_f_syn: 积分点数不足 %d\n", n_points);
        return 0.0;
    }
    std::vector<double> grid(n_points);
    for (int i = 0; i < n_points; ++i) {
        grid[i] = wl_min + i * wl_step;
    }

    // Akima 插值到网格 (范围外钳位为 0)
    std::vector<double> s_grid = akima_interpolate(sw, sf, grid, 0.0);
    std::vector<double> t_grid = akima_interpolate(fw, fv, grid, 0.0);
    std::vector<double> q_grid;
    if (has_qe) {
        q_grid = akima_interpolate(qw, qv, grid, 0.0);
    }

    // 被积函数: S(λ)·T(λ)·Q(λ)·λ (无 QE 时 Q=1.0)
    std::vector<double> integrand(n_points, 0.0);
    for (int i = 0; i < n_points; ++i) {
        double q = has_qe ? q_grid[i] : 1.0;
        integrand[i] = s_grid[i] * t_grid[i] * q * grid[i];
    }

    // Simpson 积分
    double f_syn = simpson_integrate(grid, integrand);

    // 循环内高频日志 (被 pc_api.cpp OpenMP 并行循环调用), 用 LOG_DEBUG 编译时禁用
    LOG_DEBUG("[spec_int] compute_f_syn: mag_g=%.3f, 范围=[%.1f,%.1f]nm, %d点, QE=%s, F_syn=%.6e",
              mag_g, wl_min, wl_max, n_points, has_qe ? "on" : "off", f_syn);
    return f_syn;
}

// ----------------------------------------------------------------------------
// prepare_filter_cache: 预处理滤光片+QE 曲线, 缓存重采样结果 (Task 11 +)
// ----------------------------------------------------------------------------
SpectrumIntegratorCache prepare_filter_cache(
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    const double* spectrum_wl, int spectrum_count) {

    SpectrumIntegratorCache cache;

    if (filter_wl == nullptr || filter_trans == nullptr || spectrum_wl == nullptr) {
        std::fprintf(stderr, "[spec_int] prepare_filter_cache: 空指针参数\n");
        return cache;
    }
    if (filter_count <= 0 || spectrum_count <= 0) {
        std::fprintf(stderr, "[spec_int] prepare_filter_cache: 尺寸无效 filt=%d spec=%d\n",
                    filter_count, spectrum_count);
        return cache;
    }
    // QE 可为 nullptr (向后兼容, Q(λ)=1.0); 若提供则 qe_trans 不可为空且 qe_count>0
    if (qe_wl != nullptr && (qe_trans == nullptr || qe_count <= 0)) {
        std::fprintf(stderr, "[spec_int] prepare_filter_cache: QE 参数不一致\n");
        return cache;
    }

    // 1. 滤光片曲线排序去重 (复用 prepare_curve)
    std::vector<double> fw, fv;
    prepare_curve(std::vector<double>(filter_wl, filter_wl + filter_count),
                  std::vector<double>(filter_trans, filter_trans + filter_count),
                  fw, fv);

    if (fw.size() < 2) {
        std::fprintf(stderr, "[spec_int] prepare_filter_cache: 滤光片去重后点数不足 %zu\n",
                    fw.size());
        return cache;
    }

    // 2. QE 曲线排序去重 (若有)
    std::vector<double> qw, qv;
    bool has_qe = false;
    if (qe_wl != nullptr && qe_count > 0) {
        prepare_curve(std::vector<double>(qe_wl, qe_wl + qe_count),
                      std::vector<double>(qe_trans, qe_trans + qe_count),
                      qw, qv);
        if (qw.size() >= 2) has_qe = true;
    }
    if (!has_qe) {
        LOG_INFO("[spec_int] prepare_filter_cache: QE curve not provided, F_syn without Q(λ)");
    }

    // 3. 先验证原始 nm 光谱轴 (严格等距合法域, 正本 §Simpson 有效域), 再缓存/插值:
    //    防止在非法 x 上先做二分索引/Akima, 把网格错误延后成"数值看似正常"的 F_syn。
    cache.spectrum_wl.assign(spectrum_wl, spectrum_wl + spectrum_count);
    {
        const char* why = nullptr;
        if (!spectral_grid_equidistant_nm(cache.spectrum_wl, &why)) {
            std::fprintf(stderr,
                "[spec_int] prepare_filter_cache: INVALID_SPECTRAL_GRID (%s); N=%d\n",
                why, spectrum_count);
            cache = SpectrumIntegratorCache();  // 具名失败: 空缓存 ⇒ caller 判无效
            return cache;
        }
    }

    // 4. Akima 插值: 滤光片透过率重采样到光谱波长网格 (范围外 = 0)
    cache.filter_trans = akima_interpolate(fw, fv, cache.spectrum_wl, 0.0);

    // 5. Akima 插值: QE 透过率重采样到光谱波长网格 (范围外 = 0)
    if (has_qe) {
        cache.qe_trans = akima_interpolate(qw, qv, cache.spectrum_wl, 0.0);
    }

    // 6. 预计算 weighted_wl[i] = λ_i × T(λ_i) × Q(λ_i) (无 QE 时 Q=1.0)
    cache.weighted_wl.resize(spectrum_count, 0.0);
    for (int i = 0; i < spectrum_count; ++i) {
        double q = has_qe ? cache.qe_trans[i] : 1.0;
        cache.weighted_wl[i] = cache.spectrum_wl[i] * cache.filter_trans[i] * q;
    }

    if (has_qe) {
        LOG_INFO("[spec_int] prepare_filter_cache: 滤光片 %zu 点 + QE %zu 点 -> 光谱 %d 点, "
                 "范围 F[%.1f,%.1f] Q[%.1f,%.1f] nm",
                 fw.size(), qw.size(), spectrum_count,
                 fw.front(), fw.back(), qw.front(), qw.back());
    } else {
        LOG_INFO("[spec_int] prepare_filter_cache: 滤光片 %zu 点 -> 光谱 %d 点, "
                 "范围 [%.1f, %.1f] nm (无 QE)",
                 fw.size(), spectrum_count, fw.front(), fw.back());
    }
    return cache;
}

// ----------------------------------------------------------------------------
// compute_f_syn_cached: 带缓存的 F_syn 计算 (Task 11)
// ----------------------------------------------------------------------------
double compute_f_syn_cached(
    const SpectrumIntegratorCache& cache,
    const uint8_t* spectrum_uint8, int spectrum_count,
    double mag_g) {

    if (spectrum_uint8 == nullptr) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_cached: 空指针参数\n");
        return 0.0;
    }

    const int n_pts = (int)cache.spectrum_wl.size();
    if (n_pts < 2 || (int)cache.filter_trans.size() < n_pts ||
        (int)cache.weighted_wl.size() < n_pts) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_cached: 缓存尺寸无效 wl=%zu filt=%zu wwl=%zu\n",
                    cache.spectrum_wl.size(), cache.filter_trans.size(), cache.weighted_wl.size());
        return 0.0;
    }
    if (spectrum_count != n_pts) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_cached: spectrum_count(%d) != cache(%d)\n",
                    spectrum_count, n_pts);
        return 0.0;
    }

    // 1. uint8 -> float64, S(λ) 并乘星等归一化因子
    double mag_factor = std::pow(10.0, -0.4 * mag_g);

    // 2. 被积函数 integrand[i] = S(λ_i) × T(λ_i) × λ_i = S[i] × weighted_wl[i]
    // 其中 S(λ_i) 已含 mag_factor 归一化
    std::vector<double> integrand(n_pts, 0.0);
    for (int i = 0; i < n_pts; ++i) {
        double s = (double)spectrum_uint8[i] * mag_factor;
        integrand[i] = s * cache.weighted_wl[i];
    }

    // 3. Simpson 积分
    double integral = simpson_integrate(cache.spectrum_wl, integrand);

    LOG_DEBUG("[spec_int] compute_f_syn_cached: mag_g=%.3f, %d点, F_syn=%.6e",
              mag_g, n_pts, integral);
    return integral;
}

// ----------------------------------------------------------------------------
// compute_f_syn_cached_xpsd: XPSD 官方解码变体 (PCL 语义)
// F(λ) = byte*flux_mul + flux_min (绝对谱辐照度 W*m^-2*nm^-1)
// F_syn = ∫ F(λ)·T(λ)·Q(λ)·λ dλ
// ----------------------------------------------------------------------------
double compute_f_syn_cached_xpsd(
    const SpectrumIntegratorCache& cache,
    const uint8_t* spectrum_uint8, int spectrum_count,
    double flux_min, double flux_mul) {

    // 非法输入一律 quiet NaN 具名上报, 绝不返回 0: 0 是"合法零积分"的取值,
    // 用它代表失败会把缺格/量化解码失败伪装成"零光子"。caller 以非有限值判无效。
    if (spectrum_uint8 == nullptr) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_cached_xpsd: INVALID_INTEGRAND (null spectrum)\n");
        return std::numeric_limits<double>::quiet_NaN();
    }

    const int n_pts = (int)cache.spectrum_wl.size();
    if (n_pts < 2 || (int)cache.filter_trans.size() < n_pts ||
        (int)cache.weighted_wl.size() < n_pts) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_cached_xpsd: INVALID_SPECTRAL_GRID "
                    "(缓存尺寸无效 wl=%zu filt=%zu wwl=%zu)\n",
                    cache.spectrum_wl.size(), cache.filter_trans.size(), cache.weighted_wl.size());
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (spectrum_count != n_pts) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_cached_xpsd: INVALID_SPECTRAL_GRID "
                    "(spectrum_count(%d) != cache(%d))\n", spectrum_count, n_pts);
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (!(flux_mul > 0.0) || !std::isfinite(flux_min) || !std::isfinite(flux_mul)) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_cached_xpsd: INVALID_INTEGRAND "
                    "(flux_min=%g flux_mul=%g)\n", flux_min, flux_mul);
        return std::numeric_limits<double>::quiet_NaN();
    }

    // F(λ_i) = byte_i*flux_mul + flux_min (官方 PCL 线性解码)
    std::vector<double> integrand(n_pts, 0.0);
    for (int i = 0; i < n_pts; ++i) {
        double f = (double)spectrum_uint8[i] * flux_mul + flux_min;
        integrand[i] = f * cache.weighted_wl[i];
    }

    // simpson_integrate 对非法网格/非有限 y/累加溢出返回 quiet NaN。
    double integral = simpson_integrate(cache.spectrum_wl, integrand);
    if (!std::isfinite(integral)) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_cached_xpsd: INVALID_INTEGRAND "
                    "(non-finite quadrature)\n");
        return std::numeric_limits<double>::quiet_NaN();
    }

    LOG_DEBUG("[spec_int] compute_f_syn_cached_xpsd: flux_min=%.4e flux_mul=%.4e, %d点, F_syn=%.6e",
              flux_min, flux_mul, n_pts, integral);
    return integral;
}

// ============================================================================
// 参数化窄带响应: 分段求积（Issue #106 / #43 / #104）
// ----------------------------------------------------------------------------
// 数学: 见 spectrum_integrator.h 的口径块。核心事实 = **Akima 在节点区间内是
// 三次多项式** ⇒ 段内被积函数 F(λ)·Q(λ)·λ 是 ≤7 次多项式 ⇒ 4 点
// Gauss–Legendre 对其**精确**（n 点 GL 对 ≤2n−1 次多项式精确）。
// 因此本路径的**求积误差为 0**（仅浮点舍入），误差来源被压缩为
// (a) XP 采样对 F(λ) 的重构误差, (b) XP 自身分辨率/外定标, (c) 响应形状假设。
// ============================================================================

namespace {

// 4 点 Gauss–Legendre（区间 [-1,1] 的节点与权重；高精度十进制展开）。
constexpr double kGlX[4] = {-0.86113631159405257520, -0.33998104358485626480,
                              0.33998104358485626480,  0.86113631159405257520};
constexpr double kGlW[4] = {0.34785484513745385737, 0.65214515486254614263,
                             0.65214515486254614263, 0.34785484513745385737};

} // namespace

const char* response_coverage_name(ResponseCoverage c) {
    switch (c) {
        case ResponseCoverage::kFull: return "full";
        case ResponseCoverage::kXpTruncated: return "xp_truncated";
        case ResponseCoverage::kQeTruncated: return "qe_truncated";
        case ResponseCoverage::kNoOverlap: return "no_overlap";
    }
    return "unknown";
}

ParametricBand make_parametric_band(double center_nm, double fwhm_nm) {
    ParametricBand b;
    b.center_nm = center_nm;
    b.fwhm_nm = fwhm_nm;
    if (!std::isfinite(center_nm) || !std::isfinite(fwhm_nm)) {
        b.error = "INVALID_PARAMETRIC_RESPONSE (center_nm/fwhm_nm must be finite)";
        return b;
    }
    if (!(fwhm_nm > 0.0)) {
        b.error = "INVALID_PARAMETRIC_RESPONSE (fwhm_nm must be > 0 nm)";
        return b;
    }
    const double half = fwhm_nm * 0.5;
    b.lo_nm = center_nm - half;
    b.hi_nm = center_nm + half;
    if (!(b.lo_nm > 0.0)) {
        b.error = "INVALID_PARAMETRIC_RESPONSE (center_nm - fwhm_nm/2 must be > 0 nm)";
        return b;
    }
    b.valid = true;
    return b;
}

SegmentedResponseCache prepare_segmented_response_cache(
    const ParametricBand& band,
    const double* qe_wl, const double* qe_trans, int qe_count,
    const double* spectrum_wl, int spectrum_count) {
    SegmentedResponseCache c;
    if (!band.valid) {
        c.error = band.error.empty() ? "INVALID_PARAMETRIC_RESPONSE" : band.error;
        return c;
    }
    if (spectrum_wl == nullptr || spectrum_count < 2) {
        c.error = "INVALID_SPECTRAL_GRID (null axis or <2 points)";
        return c;
    }
    c.xp_wl.assign(spectrum_wl, spectrum_wl + spectrum_count);
    for (int i = 1; i < spectrum_count; ++i) {
        if (!(c.xp_wl[static_cast<size_t>(i)] > c.xp_wl[static_cast<size_t>(i - 1)])) {
            c.error = "INVALID_SPECTRAL_GRID (axis not strictly increasing)";
            return c;
        }
    }
    c.xp_lo_nm = c.xp_wl.front();
    c.xp_hi_nm = c.xp_wl.back();
    c.band_lo_nm = band.lo_nm;
    c.band_hi_nm = band.hi_nm;

    // QE 曲线: 排序去重（与 legacy prepare_curve 同口径）。
    std::vector<double> qw, qv;
    if (qe_wl != nullptr && qe_trans != nullptr && qe_count > 0) {
        prepare_curve(std::vector<double>(qe_wl, qe_wl + qe_count),
                      std::vector<double>(qe_trans, qe_trans + qe_count),
                      qw, qv);
        if (qw.size() < 2) {
            c.error = "INVALID_QE_CURVE (<2 distinct points after dedup)";
            return c;
        }
        c.has_qe = true;
        c.qe_lo_nm = qw.front();
        c.qe_hi_nm = qw.back();
    } else if (qe_wl != nullptr || qe_trans != nullptr || qe_count != 0) {
        c.error = "INVALID_QE_CURVE (inconsistent qe_wl/qe_trans/qe_count)";
        return c;
    }

    // ── 断点集合: 响应带端点 + XP 采样点 + QE 插值节点 ────────────────────
    std::vector<double> bp;
    bp.push_back(c.band_lo_nm);
    bp.push_back(c.band_hi_nm);
    bp.push_back(c.xp_lo_nm);
    bp.push_back(c.xp_hi_nm);
    if (c.has_qe) {
        bp.push_back(c.qe_lo_nm);
        bp.push_back(c.qe_hi_nm);
    }
    for (const double x : c.xp_wl) {
        if (x > c.band_lo_nm && x < c.band_hi_nm) bp.push_back(x);
    }
    if (c.has_qe) {
        for (const double x : qw) {
            if (x > c.band_lo_nm && x < c.band_hi_nm) bp.push_back(x);
        }
    }
    std::sort(bp.begin(), bp.end());
    bp.erase(std::unique(bp.begin(), bp.end()), bp.end());

    // ── 有效积分域 = 响应带 ∩ XP 域 ∩ QE 域 ────────────────────────────────
    double lo = std::max(c.band_lo_nm, c.xp_lo_nm);
    double hi = std::min(c.band_hi_nm, c.xp_hi_nm);
    if (c.has_qe) {
        lo = std::max(lo, c.qe_lo_nm);
        hi = std::min(hi, c.qe_hi_nm);
    }
    c.overlap_lo_nm = lo;
    c.overlap_hi_nm = hi;
    if (!(hi > lo)) {
        // 无支持: 合法零（F_syn ≡ 0），不是失败。
        c.coverage = ResponseCoverage::kNoOverlap;
        c.n_segments = 0;
        c.valid = true;
        return c;
    }
    const bool band_in_xp = (c.band_lo_nm >= c.xp_lo_nm) && (c.band_hi_nm <= c.xp_hi_nm);
    const bool qe_covers = !c.has_qe ||
        ((c.band_lo_nm >= c.qe_lo_nm) && (c.band_hi_nm <= c.qe_hi_nm));
    c.coverage = !band_in_xp ? ResponseCoverage::kXpTruncated
                             : (qe_covers ? ResponseCoverage::kFull
                                          : ResponseCoverage::kQeTruncated);

    // ── QE 的 Akima 样条（与星无关, 预计算）───────────────────────────────
    AkimaSpline qe_spline;
    if (c.has_qe && !qe_spline.init(qw, qv)) {
        c.error = "INVALID_QE_CURVE (axis not strictly increasing after dedup)";
        return c;
    }

    // ── 分段 + 4 点 Gauss–Legendre 节点 ──────────────────────────────────
    std::vector<double> pts;
    for (const double b : bp) {
        if (b >= lo && b <= hi) pts.push_back(b);
    }
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const double a = pts[i];
        const double b = pts[i + 1];
        if (!(b > a)) continue;
        const double half = 0.5 * (b - a);
        const double mid = 0.5 * (a + b);
        for (int k = 0; k < 4; ++k) {
            const double lam = mid + half * kGlX[k];
            c.node_lambda.push_back(lam);
            c.node_weight.push_back(half * kGlW[k]);
            c.node_qe.push_back(c.has_qe ? qe_spline.eval(lam, 0.0) : 1.0);
        }
        ++c.n_segments;
    }
    c.breaks = pts;
    c.valid = true;
    return c;
}

double compute_f_syn_segmented_xpsd(
    const SegmentedResponseCache& cache,
    const uint8_t* spectrum_uint8, int spectrum_count,
    double flux_min, double flux_mul) {
    // 非法输入一律 quiet NaN 具名上报, 绝不返回 0（0 是"合法零积分"的取值）。
    if (!cache.valid) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_segmented_xpsd: INVALID_SPECTRAL_GRID "
                    "(cache invalid: %s)\n", cache.error.c_str());
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (spectrum_uint8 == nullptr) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_segmented_xpsd: INVALID_INTEGRAND "
                    "(null spectrum)\n");
        return std::numeric_limits<double>::quiet_NaN();
    }
    const int n_xp = static_cast<int>(cache.xp_wl.size());
    if (spectrum_count != n_xp) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_segmented_xpsd: INVALID_SPECTRAL_GRID "
                    "(spectrum_count(%d) != cache(%d))\n", spectrum_count, n_xp);
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (!(flux_mul > 0.0) || !std::isfinite(flux_min) || !std::isfinite(flux_mul)) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_segmented_xpsd: INVALID_INTEGRAND "
                    "(flux_min=%g flux_mul=%g)\n", flux_min, flux_mul);
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (cache.n_segments == 0) {
        // 无支持（响应带与有效谱域无重叠）⇒ 合法零积分。
        return 0.0;
    }

    // XPSD 官方解码: F(λ) = byte·flux_mul + flux_min (W·m⁻²·nm⁻¹)。
    std::vector<double> f(static_cast<size_t>(n_xp), 0.0);
    for (int i = 0; i < n_xp; ++i) {
        f[static_cast<size_t>(i)] =
            static_cast<double>(spectrum_uint8[i]) * flux_mul + flux_min;
    }
    AkimaSpline f_spline;
    if (!f_spline.init(cache.xp_wl, f)) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_segmented_xpsd: INVALID_SPECTRAL_GRID "
                    "(XP axis not strictly increasing)\n");
        return std::numeric_limits<double>::quiet_NaN();
    }

    // 分段 Gauss–Legendre: Σ w_k · F(λ_k)·Q(λ_k)·λ_k
    double total = 0.0;
    const size_t n_nodes = cache.node_lambda.size();
    for (size_t k = 0; k < n_nodes; ++k) {
        const double lam = cache.node_lambda[k];
        const double fv = f_spline.eval(lam, 0.0);
        total += cache.node_weight[k] * fv * cache.node_qe[k] * lam;
    }
    if (!std::isfinite(total)) {
        std::fprintf(stderr, "[spec_int] compute_f_syn_segmented_xpsd: INVALID_INTEGRAND "
                    "(non-finite quadrature)\n");
        return std::numeric_limits<double>::quiet_NaN();
    }
    return total;
}

} // namespace photo_calib
