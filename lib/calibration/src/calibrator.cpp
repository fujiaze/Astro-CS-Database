// ============================================================
// calibrator.cpp - 天文 CCD 图像校准核心算法
// ============================================================
// 功能: 对单帧 Light 图像进行标准 CCD 校准（Dark / Flat / Bias），
//       支持暗场优化（黄金分割搜索最优暗场缩放因子 K）。
// 所属模块: astro_calibration（lib/astro_calibration）
//
// 实现函数（namespace ac）:
//   1. compute_mad(data, n)
//        计算中位绝对偏差 MAD：先求 median，再求 |v-median| 的 median。
//        对应 sigma = 1.4826 * MAD（本函数只返回 MAD）。
//   2. normalize_flat(flat, w, h)
//        将 Master Flat 归一化到 median=1.0，并把最小值裁剪到 0.1。
//   3. optimize_dark_scale(light, bias, dark, flat, w, h, k_init)
//        黄金分割搜索最优 K，使背景区域（去边缘10% + 去最亮5%）MAD 最小。
//        搜索范围 [0.5*k_init, 2.0*k_init]，收敛条件：区间宽度<0.001 或迭代>30。
//   4. calibrate(light, w, h, dark, flat, bias, out, dark_opt, k_init, actual_k)
//        dark_opt=0: out = (light - dark) / flat          （Dark 已含 Bias）
//        dark_opt=1: out = (light - bias - K*(dark-bias)) / flat
//        Flat 已归一化，除法前裁剪最小值 0.1；OpenMP 并行。
//
// 设计说明:
//   - 核心算法不包含任何文件 IO，仅操作内存数组，便于上层 C API 包装。
//   - C++17 标准，不依赖外部库，仅使用 STL + OpenMP。
//   - 多线程固定 16 线程（开发环境 16 核）。
//   - median 使用 std::nth_element（O(n)），黄金分割比例 0.618 / 0.382。
// ============================================================

#include "../include/astro_calibration.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <omp.h>

namespace ac {

// ---------------------------- 内部辅助 ----------------------------
namespace {

// 对向量原地求中位数（使用 std::nth_element，O(n)）。
// 调用后向量内容会被重排，调用方需自行拷贝。
float median_inplace(std::vector<float>& v) {
    int n = static_cast<int>(v.size());
    if (n <= 0) return 0.0f;
    int mid = n / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (n % 2 == 1) {
        return v[mid];
    }
    // 偶数个：取 (mid-1) 与 mid 两个次序统计量的平均。
    // nth_element(begin+mid) 后，v[mid] 为第 mid 个次序统计量，
    // [begin, begin+mid) 内均 <= v[mid]，其最大者即第 (mid-1) 个次序统计量。
    float hi = v[mid];
    float lo = *std::max_element(v.begin(), v.begin() + mid);
    return (hi + lo) * 0.5f;
}

} // namespace

// ---------------------------- MAD ----------------------------
// 计算中位绝对偏差 MAD = median( |v - median(v)| )。
// 对应高斯噪声 sigma = 1.4826 * MAD（由调用方按需换算）。
float compute_mad(const float* data, int n) {
    if (!data || n <= 0) return 0.0f;

    std::vector<float> tmp(data, data + n);
    float med = median_inplace(tmp);

    for (int i = 0; i < n; i++) {
        tmp[i] = std::fabs(data[i] - med);
    }
    return median_inplace(tmp);
}

// ---------------------------- Flat 归一化 ----------------------------
// 归一化 Master Flat 到 median = 1.0，并将最小值裁剪到 0.1。
void normalize_flat(float* flat, int w, int h) {
    if (!flat || w <= 0 || h <= 0) return;

    int n = w * h;
    std::vector<float> tmp(flat, flat + n);
    float med = median_inplace(tmp);
    if (!(med > 0.0f)) return; // median <= 0 无法归一化，保持原样

    float inv = 1.0f / med;
#pragma omp parallel for schedule(static) num_threads(16)
    for (int i = 0; i < n; i++) {
        float v = flat[i] * inv;
        if (v < 0.1f) v = 0.1f;
        flat[i] = v;
    }
}

// ---------------------------- 暗场优化 ----------------------------
// 黄金分割搜索最优 K，目标：背景区域（去边缘10% + 去最亮5%像素）MAD 最小。
// 对每个候选 K：residual = (light - bias - K*(dark-bias)) / flat，取背景 MAD。
// 搜索范围 [0.5*k_init, 2.0*k_init]，收敛：区间宽度<0.001 或迭代>30。
float optimize_dark_scale(const float* light, const float* bias, const float* dark,
                          const float* flat, int w, int h, float k_init) {
    if (!light || !bias || !dark || w <= 0 || h <= 0) return k_init;

    // 1. 中央区域（去掉每边 10% 边缘）
    int mx = static_cast<int>(w * 0.1);
    int my = static_cast<int>(h * 0.1);
    int x0 = mx, x1 = w - mx;
    int y0 = my, y1 = h - my;
    if (x1 <= x0) { x0 = 0; x1 = w; }
    if (y1 <= y0) { y0 = 0; y1 = h; }

    std::vector<int> central_idx;
    central_idx.reserve(static_cast<size_t>(x1 - x0) * (y1 - y0));
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            central_idx.push_back(y * w + x);
        }
    }
    int n_central = static_cast<int>(central_idx.size());
    if (n_central < 100) return k_init; // 背景样本过少，放弃优化

    // 2. 由 Light 计算亮度阈值：去掉最亮 5%（保留较暗的 95% 作为背景）
    std::vector<float> vals(n_central);
    for (int i = 0; i < n_central; i++) {
        vals[i] = light[central_idx[i]];
    }
    int thresh_pos = static_cast<int>(n_central * 0.95);
    if (thresh_pos >= n_central) thresh_pos = n_central - 1;
    if (thresh_pos < 0) thresh_pos = 0;
    std::nth_element(vals.begin(), vals.begin() + thresh_pos, vals.end());
    float thresh = vals[thresh_pos];

    // 3. 构建背景像素的预计算数组（避免每次迭代全图运算）
    bool has_flat = (flat != nullptr);
    std::vector<float> light_bg, bias_bg, dmb_bg, flat_bg;
    light_bg.reserve(n_central);
    bias_bg.reserve(n_central);
    dmb_bg.reserve(n_central);
    if (has_flat) flat_bg.reserve(n_central);

    for (int i = 0; i < n_central; i++) {
        int idx = central_idx[i];
        if (light[idx] <= thresh) { // 用原始 light 判定（vals 已被 nth_element 重排）
            light_bg.push_back(static_cast<float>(light[idx]));
            bias_bg.push_back(static_cast<float>(bias[idx]));
            dmb_bg.push_back(static_cast<float>(dark[idx]) - static_cast<float>(bias[idx]));
            if (has_flat) flat_bg.push_back(static_cast<float>(flat[idx]));
        }
    }
    int n_bg = static_cast<int>(light_bg.size());
    if (n_bg < 100) return k_init; // 背景样本过少

    // 4. 目标函数：给定 K，计算背景残差 MAD
    std::vector<float> residual(n_bg);
    auto objective = [&](float k) -> float {
#pragma omp parallel for schedule(static) num_threads(16)
        for (int i = 0; i < n_bg; i++) {
            float r = light_bg[i] - bias_bg[i] - k * dmb_bg[i];
            if (has_flat) {
                float f = flat_bg[i];
                if (f < 0.1f) f = 0.1f; // 与 calibrate 保持一致的最小裁剪
                r /= f;
            }
            residual[i] = r;
        }
        return compute_mad(residual.data(), n_bg);
    };

    // 5. 黄金分割搜索（0.618 法），求 MAD 最小
    const float gr = 0.618f;
    const float gc = 0.382f;
    float a = 0.5f * k_init;
    float b = 2.0f * k_init;

    // 用 c_lo/c_hi 表示两个内部试探点，避免与中央区域 int x1 冲突
    float c_lo = a + gc * (b - a); // 偏左试探点
    float c_hi = a + gr * (b - a); // 偏右试探点
    float f_lo = objective(c_lo);
    float f_hi = objective(c_hi);

    for (int iter = 0; iter < 30; iter++) {
        if ((b - a) < 0.001f) break;
        if (f_lo < f_hi) {
            // 极小值落在 [a, c_hi]，收缩右边界
            b = c_hi; c_hi = c_lo; f_hi = f_lo;
            c_lo = a + gc * (b - a);
            f_lo = objective(c_lo);
        } else {
            // 极小值落在 [c_lo, b]，收缩左边界
            a = c_lo; c_lo = c_hi; f_lo = f_hi;
            c_hi = a + gr * (b - a);
            f_hi = objective(c_hi);
        }
    }

    return 0.5f * (a + b);
}

// ---------------------------- 主校准 ----------------------------
// dark_opt=0: out = (light - dark) / flat            （Dark 已含 Bias）
// dark_opt=1: out = (light - bias - K*(dark-bias)) / flat
//   - 先调用 optimize_dark_scale 搜索最优 K（需 bias 与 dark 同时存在）
//   - 若 dark_opt=1 但缺少 bias/dark，回退为标准模式 (light - dark)/flat
void calibrate(const float* light, int w, int h,
               const float* dark, const float* flat, const float* bias,
               float* out, int dark_opt, float k_init, float* actual_k) {
    if (!light || !out || w <= 0 || h <= 0) {
        if (actual_k) *actual_k = k_init;
        return;
    }

    int n = w * h;
    float k = k_init;

    if (dark_opt == 1 && bias && dark) {
        // 暗场优化模式：搜索最优 K 后应用 (light - bias - K*(dark-bias)) / flat
        k = optimize_dark_scale(light, bias, dark, flat, w, h, k_init);
#pragma omp parallel for schedule(static) num_threads(16)
        for (int i = 0; i < n; i++) {
            float v = light[i] - bias[i] - k * (dark[i] - bias[i]);
            if (flat) v /= std::max(flat[i], 0.1f);
            out[i] = v;
        }
    } else {
        // 标准模式：Dark 已含 Bias，直接 (light - dark) / flat
        k = 1.0f;
#pragma omp parallel for schedule(static) num_threads(16)
        for (int i = 0; i < n; i++) {
            float v = light[i];
            if (dark) v -= dark[i];
            if (flat) v /= std::max(flat[i], 0.1f);
            out[i] = v;
        }
    }

    if (actual_k) *actual_k = k;
}

} // namespace ac
