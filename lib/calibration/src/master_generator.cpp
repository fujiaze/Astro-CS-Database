// master_generator.cpp
// 主帧生成模块 - 天文CCD校准 (Astro Calibration)
//
// 功能：
//   对多帧CCD图像执行 sigma-clip 离群值剔除后合并，生成主帧（Master Frame）。
//   - generate_master:       通用主帧生成，用于 Master Bias / Master Dark。
//                             沿帧方向迭代计算 median 与 MAD，剔除离群值后
//                             按 mean 或 median 合并。
//   - generate_master_flat:  Flat 专用主帧生成。
//                             减 Bias -> 逐帧 median 归一化 -> sigma-clip + mean
//                             合并 -> 最终 median 归一化到 1.0，最小值裁剪 0.1。
//
// 设计要点：
//   - 纯 C++17 标准库实现，不依赖任何外部库。
//   - 核心算法不含文件 IO（日志输出到 stderr，便于分析）。
//   - 使用 OpenMP 多线程并行处理每个像素（线程本地缓冲复用，避免重复分配）。
//   - median 计算使用 std::nth_element（O(n)），MAD = median(|v - median|)，
//     sigma = 1.4826 * MAD。
//
// 编译标准：C++17
// 对应头文件：astro_calibration.h

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <chrono>
#include <vector>
#include <numeric>

#include "astro_calibration.h"

namespace ac {

// ======================== 内部工具 ========================

// 日志输出到 stderr（不涉及文件 IO），格式化输出便于运行分析
static void ac_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::fprintf(stderr, "[ac::master_gen] ");
    std::vfprintf(stderr, fmt, args);
    std::fprintf(stderr, "\n");
    va_end(args);
}

// 对缓冲区中的值求中位数（原位操作，调用后 buf 顺序会被 nth_element 打乱）
// 缓冲区应已剔除 NaN（由调用方保证）
static float median_of(std::vector<float>& buf) {
    if (buf.empty()) return NAN;
    size_t n = buf.size();
    size_t mid = n / 2;
    std::nth_element(buf.begin(), buf.begin() + mid, buf.end());
    if (n % 2 == 1) return buf[mid];
    // 偶数个：取中间两值的平均
    // nth_element 后 [begin, mid) 中所有元素 <= buf[mid]，取该区间最大值即为下中位数
    float upper = buf[mid];
    float lower = *std::max_element(buf.begin(), buf.begin() + mid);
    return (lower + upper) * 0.5f;
}

// ======================== 主帧生成（通用） ========================
// sigma-clip 离群值剔除 + median/mean 合并
// stack: [n_frames * h * w] 行优先 float32
// out:   [h * w] float32
// combine: AC_COMBINE_MEAN(0) 或 AC_COMBINE_MEDIAN(1)
void generate_master(const float* stack, int n_frames, int w, int h,
                     float* out, float sigma_low, float sigma_high,
                     int max_iter, int combine) {
    auto t0 = std::chrono::steady_clock::now();
    ac_log("generate_master: start | n_frames=%d, %dx%d, sigma_low=%.2f, sigma_high=%.2f, max_iter=%d, combine=%d(%s)",
           n_frames, w, h, sigma_low, sigma_high, max_iter, combine,
           combine == AC_COMBINE_MEDIAN ? "median" : "mean");

    const int npix = w * h;
    const float k_sigma = 1.4826f;  // MAD -> 高斯 sigma 转换系数

    if (n_frames <= 0 || npix <= 0) {
        ac_log("generate_master: invalid params (n_frames=%d, npix=%d), abort", n_frames, npix);
        return;
    }

    // 单帧直接拷贝（无需 sigma-clip）
    if (n_frames == 1) {
        std::copy(stack, stack + npix, out);
        ac_log("generate_master: single frame, copied directly");
        return;
    }

    // 并行处理每个像素：使用 parallel + for 分离形式以复用线程本地缓冲
    #pragma omp parallel
    {
        std::vector<float> vals(n_frames);  // 当前像素各帧的值（剔除后置 NaN）
        std::vector<float> work;            // median/MAD 计算的工作缓冲（复用）

        #pragma omp for
        for (int idx = 0; idx < npix; ++idx) {
            // 收集当前像素在所有帧中的值
            for (int n = 0; n < n_frames; ++n) {
                vals[n] = stack[(size_t)n * npix + idx];
            }

            // ---- sigma-clip 迭代 ----
            for (int iter = 0; iter < max_iter; ++iter) {
                // 计算非 NaN 值的 median
                work.clear();
                for (float v : vals) {
                    if (!std::isnan(v)) work.push_back(v);
                }
                if (work.empty()) break;
                float med = median_of(work);

                // 计算 MAD = median(|v - median|)
                work.clear();
                for (float v : vals) {
                    if (!std::isnan(v)) work.push_back(std::fabs(v - med));
                }
                float mad = median_of(work);
                float sigma = k_sigma * mad;
                if (sigma <= 0.0f) break;  // 无离散度，无需剔除

                // 非对称 sigma-clip：低于 median-sigma_low*sigma 或高于 median+sigma_high*sigma
                int rejected = 0;
                for (float& v : vals) {
                    if (std::isnan(v)) continue;
                    float dev = v - med;
                    if (dev < -sigma_low * sigma || dev > sigma_high * sigma) {
                        v = NAN;
                        ++rejected;
                    }
                }
                if (rejected == 0) break;  // 已收敛
            }

            // ---- 合并 ----
            if (combine == AC_COMBINE_MEDIAN) {
                work.clear();
                for (float v : vals) {
                    if (!std::isnan(v)) work.push_back(v);
                }
                out[idx] = median_of(work);
            } else {
                // mean：对非 NaN 值求平均
                int cnt = 0;
                for (float v : vals) {
                    if (!std::isnan(v)) ++cnt;
                }
                if (cnt > 0) {
                    float sum = std::accumulate(vals.begin(), vals.end(), 0.0f,
                        [](float acc, float v) {
                            return std::isnan(v) ? acc : (acc + v);
                        });
                    out[idx] = sum / static_cast<float>(cnt);
                } else {
                    out[idx] = NAN;
                }
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(t1 - t0).count();
    ac_log("generate_master: done | %.3f s", dt);
}

// ======================== 主帧生成（Flat 专用） ========================
// 减 Bias -> 逐帧归一化(median) -> sigma-clip + mean 合并 -> 再归一化
// flat_stack: [n_frames * h * w]
// bias:       [h * w] 或 NULL
// out:        [h * w]，最终 median 归一化到 1.0，最小值裁剪 0.1
void generate_master_flat(const float* flat_stack, int n_frames, int w, int h,
                          const float* bias, float* out,
                          float sigma_low, float sigma_high, int max_iter) {
    auto t0 = std::chrono::steady_clock::now();
    ac_log("generate_master_flat: start | n_frames=%d, %dx%d, bias=%s, sigma_low=%.2f, sigma_high=%.2f, max_iter=%d",
           n_frames, w, h, bias ? "yes" : "no", sigma_low, sigma_high, max_iter);

    const int npix = w * h;

    if (n_frames <= 0 || npix <= 0) {
        ac_log("generate_master_flat: invalid params (n_frames=%d, npix=%d), abort", n_frames, npix);
        return;
    }

    // 归一化后的帧缓冲
    std::vector<float> norm(static_cast<size_t>(n_frames) * npix);

    // ---- 步骤1：减 Bias + 逐帧 median 归一化（最小裁剪 0.1）----
    #pragma omp parallel for
    for (int n = 0; n < n_frames; ++n) {
        const float* src = flat_stack + static_cast<size_t>(n) * npix;
        float* dst = norm.data() + static_cast<size_t>(n) * npix;

        // 减 Bias
        if (bias) {
            for (int i = 0; i < npix; ++i) {
                dst[i] = src[i] - bias[i];
            }
        } else {
            std::copy(src, src + npix, dst);
        }

        // 计算帧 median
        std::vector<float> tmp(dst, dst + npix);
        float frame_med = median_of(tmp);
        if (std::isnan(frame_med) || frame_med == 0.0f) frame_med = 1.0f;

        // 归一化：除以帧 median，最小值裁剪 0.1
        for (int i = 0; i < npix; ++i) {
            float v = dst[i] / frame_med;
            if (v < 0.1f) v = 0.1f;
            dst[i] = v;
        }
    }
    ac_log("generate_master_flat: step1 done (bias subtraction + per-frame normalization)");

    // ---- 步骤2：sigma-clip + mean 合并 ----
    generate_master(norm.data(), n_frames, w, h, out,
                    sigma_low, sigma_high, max_iter, AC_COMBINE_MEAN);
    ac_log("generate_master_flat: step2 done (sigma-clip + mean combine)");

    // ---- 步骤3：最终 median 归一化到 1.0（最小裁剪 0.1）----
    {
        std::vector<float> tmp(out, out + npix);
        float final_med = median_of(tmp);
        if (std::isnan(final_med) || final_med == 0.0f) final_med = 1.0f;

        #pragma omp parallel for
        for (int i = 0; i < npix; ++i) {
            float v = out[i] / final_med;
            if (v < 0.1f) v = 0.1f;
            out[i] = v;
        }
    }
    ac_log("generate_master_flat: step3 done (final normalization to median=1.0)");

    auto t1 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(t1 - t0).count();
    ac_log("generate_master_flat: done | %.3f s", dt);
}

}  // namespace ac
