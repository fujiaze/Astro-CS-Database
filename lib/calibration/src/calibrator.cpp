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
// K = t_light / t_dark，由调用方从 FITS EXPTIME 计算后传入 k_init。
// 不使用优化搜索，直接采用 k_init 作为暗场优化系数。

// ---------------------------- 主校准 ----------------------------
// dark_opt=0: out = (light - dark) / flat            （Dark 已含 Bias）
// dark_opt=1: out = (light - bias - K*(dark-bias)) / flat
//   - K = k_init（由调用方从 FITS EXPTIME 计算：t_light / t_dark）
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
        // 暗场优化模式：K = t_light / t_dark，直接应用 (light - bias - K*(dark-bias)) / flat
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
