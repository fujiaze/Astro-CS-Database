// cosmetic_corrector.cpp
// 坏点修复模块 - 天文CCD校准 (Astro Calibration)
//
// 功能：
//   在校准后的 Light 图像上检测坏点并修复。
//   检测方法：Dark全局统计检测热像素 + Bias全局统计检测冷像素。
//   连通区域大小过滤排除星点（保留 < max_structure_size 的结构）。
//   修复方法：5x5中值滤波 或 双线性插值。
//
// 设计要点：
//   - 纯 C++17 标准库 + OpenMP，不依赖外部库
//   - 核心算法不含文件 IO
//   - Dark/Bias 主帧保留坏点（校准时扣除），仅用它们定位坏点位置
//   - 连通区域标记：背景 label 0 的 size 设为 max_size，防止全图被误标记
//   - median 用 std::nth_element（O(n)），MAD = median(|v - median|)

#include "../include/astro_calibration.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <queue>
#include <cstdio>
#include <omp.h>

namespace ac {

// ======================== 内部辅助 ========================

namespace {

float median_inplace(std::vector<float>& v) {
    int n = static_cast<int>(v.size());
    if (n <= 0) return 0.0f;
    int mid = n / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (n % 2 == 1) return v[mid];
    float hi = v[mid];
    float lo = *std::max_element(v.begin(), v.begin() + mid);
    return (hi + lo) * 0.5f;
}

float compute_global_median(const float* data, int n) {
    std::vector<float> tmp(data, data + n);
    return median_inplace(tmp);
}

float compute_global_mad(const float* data, int n, float med) {
    std::vector<float> absdev(n);
    for (int i = 0; i < n; i++) absdev[i] = std::fabs(data[i] - med);
    return median_inplace(absdev);
}

} // namespace

// ======================== 连通区域过滤 ========================
// 8连通区域标记 + 大小统计，只保留 < max_size 的结构（排除星点）
// 关键：背景(label 0)的size设为max_size，防止全图被误标记
void filter_by_structure_size(char* mask, int w, int h, int max_size) {
    const int npix = w * h;
    if (npix <= 0) return;

    std::vector<int> labels(npix, 0);
    int num_features = 0;

    for (int seed = 0; seed < npix; seed++) {
        if (!mask[seed] || labels[seed] != 0) continue;

        int label = ++num_features;

        std::queue<int> q;
        q.push(seed);
        labels[seed] = label;

        while (!q.empty()) {
            int idx = q.front();
            q.pop();

            int x = idx % w;
            int y = idx / w;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    int nidx = ny * w + nx;
                    if (mask[nidx] && labels[nidx] == 0) {
                        labels[nidx] = label;
                        q.push(nidx);
                    }
                }
            }
        }
    }

    if (num_features == 0) return;

    std::vector<int> sizes(num_features + 1, 0);
    for (int i = 0; i < npix; i++) {
        if (labels[i] > 0) sizes[labels[i]]++;
    }

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < npix; i++) {
        if (labels[i] > 0 && sizes[labels[i]] >= max_size) {
            mask[i] = 0;
        }
    }
}

// ======================== 热像素检测（从Dark） ========================
// 全局统计：median + threshold_sigma * 1.4826 * MAD
// dark > threshold 的像素为候选，经结构过滤后返回
void detect_hot_pixels(const float* dark, int w, int h, char* hot_mask,
                       float threshold_sigma, int max_size) {
    int n = w * h;
    if (!dark || n <= 0) return;

    float med = compute_global_median(dark, n);
    float mad = compute_global_mad(dark, n, med);
    float sigma = 1.4826f * mad;
    float threshold = med + threshold_sigma * sigma;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        hot_mask[i] = (dark[i] > threshold) ? 1 : 0;
    }

    filter_by_structure_size(hot_mask, w, h, max_size);
}

// ======================== 冷像素检测（从Bias） ========================
// 全局统计：median - threshold_sigma * 1.4826 * MAD
// bias < threshold 的像素为候选，经结构过滤后返回
void detect_cold_pixels(const float* bias, int w, int h, char* cold_mask,
                        float threshold_sigma, int max_size) {
    int n = w * h;
    if (!bias || n <= 0) return;

    float med = compute_global_median(bias, n);
    float mad = compute_global_mad(bias, n, med);
    float sigma = 1.4826f * mad;
    float threshold = med - threshold_sigma * sigma;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        cold_mask[i] = (bias[i] < threshold) ? 1 : 0;
    }

    filter_by_structure_size(cold_mask, w, h, max_size);
}

// ======================== 插值修复 ========================
// method=0 (median): 用5x5中值滤波结果替换坏像素
// method=1 (bilinear): 用8邻居双线性插值替换坏像素
void interpolate_pixels(const float* data, const char* bad_mask, int w, int h,
                        float* out, int method) {
    int n = w * h;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        if (!bad_mask[i]) {
            out[i] = data[i];
            continue;
        }

        int x = i % w;
        int y = i / w;

        if (method == AC_METHOD_MEDIAN) {
            // 5x5中值
            std::vector<float> vals;
            vals.reserve(25);
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0) nx = -nx;
                    if (ny < 0) ny = -ny;
                    if (nx >= w) nx = 2 * w - nx - 2;
                    if (ny >= h) ny = 2 * h - ny - 2;
                    if (nx < 0) nx = 0;
                    if (ny < 0) ny = 0;
                    if (nx >= w) nx = w - 1;
                    if (ny >= h) ny = h - 1;
                    int nidx = ny * w + nx;
                    if (!bad_mask[nidx]) {
                        vals.push_back(data[nidx]);
                    }
                }
            }
            if (vals.empty()) {
                out[i] = data[i];
            } else {
                out[i] = median_inplace(vals);
            }
        } else {
            // 双线性插值：用4个方向最近的好像素做距离反比加权
            float sum_wv = 0.0f, sum_w = 0.0f;
            int dirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
            for (int d = 0; d < 4; d++) {
                int dx = dirs[d][0], dy = dirs[d][1];
                int nx = x + dx, ny = y + dy;
                int dist = 1;
                while (nx >= 0 && nx < w && ny >= 0 && ny < h && bad_mask[ny*w+nx]) {
                    nx += dx; ny += dy; dist++;
                }
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                    float weight = 1.0f / static_cast<float>(dist);
                    sum_wv += data[ny * w + nx] * weight;
                    sum_w += weight;
                }
            }
            if (sum_w > 0.0f) {
                out[i] = sum_wv / sum_w;
            } else {
                out[i] = data[i];
            }
        }
    }
}

// ======================== 主入口：坏点修复 ========================
// 检测热像素(Dark) + 冷像素(Bias) -> 合并掩码 -> 插值修复
void correct_frame(const float* data, int w, int h,
                   const float* dark, const float* bias,
                   float* out,
                   float hot_sigma, float cold_sigma,
                   int method, int max_size,
                   int* out_hot, int* out_cold) {
    int n = w * h;
    if (!data || !out || n <= 0) return;

    std::vector<char> hot_mask(n, 0);
    std::vector<char> cold_mask(n, 0);

    // 从 Dark 检测热像素
    if (dark && hot_sigma > 0.0f) {
        detect_hot_pixels(dark, w, h, hot_mask.data(), hot_sigma, max_size);
    }

    // 从 Bias 检测冷像素
    if (bias && cold_sigma > 0.0f) {
        detect_cold_pixels(bias, w, h, cold_mask.data(), cold_sigma, max_size);
    }

    // 合并掩码
    std::vector<char> all_bad(n, 0);
    int n_hot = 0, n_cold = 0;
    for (int i = 0; i < n; i++) {
        if (hot_mask[i]) n_hot++;
        if (cold_mask[i]) n_cold++;
        all_bad[i] = hot_mask[i] || cold_mask[i];
    }

    // 插值修复
    interpolate_pixels(data, all_bad.data(), w, h, out, method);

    if (out_hot) *out_hot = n_hot;
    if (out_cold) *out_cold = n_cold;
}

} // namespace ac
