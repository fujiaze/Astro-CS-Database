// spectrum_integrator.cpp - 光谱积分器实现
// 算法: Akima 子样条插值 + Simpson 1/3 复合积分
// 参考: python/synthetic_photometry.py (scipy.interpolate.Akima1DInterpolator + scipy.integrate.simpson)
// 日志: 输出到 stderr, 前缀 [spec_int]

#include "spectrum_integrator.h"
#include "log_macros.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

    // 计算原始斜率 m[i] = (y[i+1]-y[i])/(x[i+1]-x[i]), i=0..n-2
    std::vector<double> slope(n - 1, 0.0);
    for (int i = 0; i < n - 1; ++i) {
        double dx = x_src[i + 1] - x_src[i];
        if (dx <= 0.0) {
            std::fprintf(stderr, "[spec_int] akima: x_src 非严格递增 @%d (dx=%.6g)\n", i, dx);
            return y_dst;
        }
        slope[i] = (y_src[i + 1] - y_src[i]) / dx;
    }

    // 扩展斜率数组 ext_m[0..n+1], ext_m[j] = m[j-2]
    // ext_m[0]=m[-2], ext_m[1]=m[-1], ext_m[2..n]=m[0..n-2], ext_m[n+1]=m[n-1]
    std::vector<double> ext_m(n + 2, 0.0);
    ext_m[0] = 3.0 * slope[0] - 2.0 * slope[1];       // m[-2]
    ext_m[1] = 2.0 * slope[0] - slope[1];             // m[-1]
    for (int i = 0; i < n - 1; ++i) ext_m[2 + i] = slope[i]; // m[0..n-2]
    ext_m[n + 1] = 2.0 * slope[n - 2] - slope[n - 3]; // m[n-1]

    // 每个数据点 i 的切线 t[i]
    std::vector<double> t(n, 0.0);
    for (int i = 0; i < n; ++i) {
        double m_left2 = ext_m[i];      // m[i-2]
        double m_left1 = ext_m[i + 1];  // m[i-1]
        double m_cur   = ext_m[i + 2];  // m[i]
        double m_right = ext_m[i + 3];  // m[i+1]
        double w1 = std::fabs(m_right - m_cur);
        double w2 = std::fabs(m_left1 - m_left2);
        if (w1 + w2 == 0.0) {
            t[i] = 0.5 * (m_left1 + m_cur);
        } else {
            t[i] = (w1 * m_left1 + w2 * m_cur) / (w1 + w2);
        }
    }

    // 对每个 x_dst 做分段三次 Hermite 插值
    // 用二分找区间 [x_src[j], x_src[j+1]]
    for (int k = 0; k < m; ++k) {
        double x = x_dst[k];
        if (x < x_src[0] || x > x_src[n - 1]) {
            y_dst[k] = fill; // 范围外钳位
            continue;
        }
        // 二分查找区间 j: x_src[j] <= x <= x_src[j+1]
        int lo = 0, hi = n - 2, j = 0;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (x >= x_src[mid] && x <= x_src[mid + 1]) { j = mid; break; }
            if (x < x_src[mid]) hi = mid - 1; else lo = mid + 1;
        }
        double x0 = x_src[j], x1 = x_src[j + 1];
        double y0 = y_src[j], y1 = y_src[j + 1];
        double dx = x1 - x0;
        double s = (x - x0) / dx;
        // Hermite 基函数
        double h00 = (2.0 * s - 3.0) * s * s + 1.0;
        double h10 = ((s - 2.0) * s + 1.0) * s;
        double h01 = (-2.0 * s + 3.0) * s * s;
        double h11 = (s - 1.0) * s * s;
        y_dst[k] = h00 * y0 + h10 * dx * t[j] + h01 * y1 + h11 * dx * t[j + 1];
    }
    return y_dst;
}

// ----------------------------------------------------------------------------
// Simpson 1/3 复合积分 (末尾奇数区间用 Simpson 3/8)
// ----------------------------------------------------------------------------
double simpson_integrate(const std::vector<double>& x, const std::vector<double>& y) {
    const int n_pts = (int)x.size();
    if (n_pts < 2 || (int)y.size() < n_pts) return 0.0;

    double h = (x[n_pts - 1] - x[0]) / (n_pts - 1); // 等间距假设
    int n_int = n_pts - 1; // 区间数

    double sum = 0.0;

    if (n_int % 2 == 0) {
        // 纯 Simpson 1/3
        sum = y[0] + y[n_pts - 1];
        for (int i = 1; i < n_pts - 1; ++i) {
            sum += (i % 2 == 1 ? 4.0 : 2.0) * y[i];
        }
        return sum * h / 3.0;
    }

    // 奇数区间: 前 n_int-3 用 Simpson 1/3, 末尾 3 用 Simpson 3/8
    if (n_int >= 3) {
        int n_13 = n_int - 3; // 前 n_13 个区间用 Simpson 1/3 (必须为偶数)
        // n_13 此时为偶数
        sum = y[0] + y[n_13];
        for (int i = 1; i < n_13; ++i) {
            sum += (i % 2 == 1 ? 4.0 : 2.0) * y[i];
        }
        sum = sum * h / 3.0;

        // Simpson 3/8: 3 区间, 4 点 (y[n_13], y[n_13+1], y[n_13+2], y[n_pts-1])
        sum += (y[n_13] + 3.0 * y[n_13 + 1] + 3.0 * y[n_13 + 2] + y[n_pts - 1]) * 3.0 * h / 8.0;
        return sum;
    }

    // n_int == 1: 单区间梯形
    return 0.5 * h * (y[0] + y[1]);
}

// ----------------------------------------------------------------------------
// compute_f_syn: 单星合成流量
// ----------------------------------------------------------------------------
double compute_f_syn(
    const uint8_t* spectrum_uint8, int spectrum_count,
    const double* spectrum_wl, int wl_count,
    const double* filter_wl, const double* filter_trans, int filter_count,
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

    // uint8 -> float64, 作为 S(λ), 并乘 10^(-0.4*mag_g) 做星等归一化
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

    // 积分范围: SED 与滤光片波长交集
    double wl_min = std::max(sw.front(), fw.front());
    double wl_max = std::min(sw.back(), fw.back());
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

    // 被积函数: S(λ)·T(λ)·λ
    std::vector<double> integrand(n_points, 0.0);
    for (int i = 0; i < n_points; ++i) {
        integrand[i] = s_grid[i] * t_grid[i] * grid[i];
    }

    // Simpson 积分
    double f_syn = simpson_integrate(grid, integrand);

    // 循环内高频日志 (被 pc_api.cpp OpenMP 并行循环调用), 用 LOG_DEBUG 编译时禁用
    LOG_DEBUG("[spec_int] compute_f_syn: mag_g=%.3f, 范围=[%.1f,%.1f]nm, %d点, F_syn=%.6e",
              mag_g, wl_min, wl_max, n_points, f_syn);
    return f_syn;
}

// ----------------------------------------------------------------------------
// prepare_filter_cache: 预处理滤光片曲线, 缓存重采样结果 (Task 11)
// ----------------------------------------------------------------------------
SpectrumIntegratorCache prepare_filter_cache(
    const double* filter_wl, const double* filter_trans, int filter_count,
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

    // 2. 复制光谱波长网格作为积分网格
    cache.spectrum_wl.assign(spectrum_wl, spectrum_wl + spectrum_count);

    // 3. Akima 插值: 滤光片透过率重采样到光谱波长网格 (范围外 = 0)
    cache.filter_trans = akima_interpolate(fw, fv, cache.spectrum_wl, 0.0);

    // 4. 预计算 weighted_wl[i] = λ_i × T(λ_i)
    cache.weighted_wl.resize(spectrum_count, 0.0);
    for (int i = 0; i < spectrum_count; ++i) {
        cache.weighted_wl[i] = cache.spectrum_wl[i] * cache.filter_trans[i];
    }

    LOG_INFO("[spec_int] prepare_filter_cache: 滤光片 %zu 点 -> 光谱 %d 点, 范围 [%.1f, %.1f] nm",
             fw.size(), spectrum_count, fw.front(), fw.back());
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
    //    其中 S(λ_i) 已含 mag_factor 归一化
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

} // namespace photo_calib
