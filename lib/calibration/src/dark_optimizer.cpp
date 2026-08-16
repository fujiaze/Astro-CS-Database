// ============================================================
// dark_optimizer.cpp - 最优 Dark 系数估计（鲁棒回归 + 失败回退）
// ============================================================
// 所属模块: astro_calibration（lib/calibration）
// 规范依据: 02_FROZEN §2.3
//
// 估计模型:
// L - B = c + k * (D - B)
// 其中 L=Light, B=Bias, D=Dark（Dark 已含 Bias，D-B 为纯暗电流）
//
// 算法流程:
// 1. 背景提取：对 Light 做 sigma-clip（median + 3*1.4826*MAD）排除星点
// 2. 分区抽样：8x8 = 64 网格，每区最多 1000 个背景像素，总样本上限 50000
// 3. 鲁棒线性回归：OLS 估计 k、c → 残差 MAD 离群抑制（3*1.4826*MAD 阈值）
// → 迭代 5 轮逐步剔除离群点
// 4. 合理性判定：样本不足 / 回归残差异常 / k 值越界(k<=0 或 k>10) 任一触发即判失败
//
// 失败回退（02_FROZEN §2.3 强制要求）:
// - 最优估计失败时输出结构化诊断（hiss::Stage1Diagnostics）
// - 自动回退曝光时间比例缩放: k_t = t_light / t_dark（由调用方传入 k_init）
// - 设置 diagnostics.fell_back=1, fallback_from="OPTIMAL", fallback_to="EXPOSURE_RATIO"
// - 返回 k_init
//
// 设计说明:
// - 不得为估计 k 再执行一次完整星点检测（规范要求），故仅用 sigma-clip 粗略排除星点
// - C++17，仅依赖 STL，无外部库
// - 与 calibrator.cpp 风格一致：namespace ac + 匿名 namespace 辅助 + nth_element 求中位数
// - flat 参数当前未参与回归（模型在原始 ADU 空间，Flat 在最终校准时才除），
// 保留参数以匹配接口契约，供未来扩展（如按 flat 加权）。
// ============================================================

#include "../include/astro_calibration.h"
#include "hiss_format.h"  // hiss::Stage1Diagnostics

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>

namespace ac {

// ---------------------------- 内部辅助 ----------------------------
namespace {

// 对向量原地求中位数（std::nth_element，O(n)）。
// 调用后向量内容会被重排，调用方需自行拷贝。
// 与 calibrator.cpp 中同名实现保持一致。
float median_inplace(std::vector<float>& v) {
    int n = static_cast<int>(v.size());
    if (n <= 0) return 0.0f;
    int mid = n / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (n % 2 == 1) {
        return v[mid];
    }
    // 偶数个：取 (mid-1) 与 mid 两个次序统计量的平均。
    float hi = v[mid];
    float lo = *std::max_element(v.begin(), v.begin() + mid);
    return (hi + lo) * 0.5f;
}

// 安全字符串拷贝（固定长度 char 数组，保证 NUL 终止）
void safe_copy(char* dst, const char* src, std::size_t n) {
    if (!dst || !src || n == 0) return;
    std::size_t i = 0;
    for (; i + 1 < n && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

// 一次性填充 Stage1Diagnostics 各字段。
// success: 0=成功, <0=失败; fell_back: 0=未回退, 1=已回退
void set_diag(hiss::Stage1Diagnostics& d, int success,
              const char* stage, const char* code, const char* message,
              int fell_back, const char* from, const char* to) {
    d.success = success;
    safe_copy(d.stage, stage, sizeof(d.stage));
    safe_copy(d.code, code, sizeof(d.code));
    safe_copy(d.message, message, sizeof(d.message));
    d.fell_back = fell_back;
    safe_copy(d.fallback_from, from, sizeof(d.fallback_from));
    safe_copy(d.fallback_to, to, sizeof(d.fallback_to));
}

// 标准回退：最优估计失败 → 回退曝光时间比例
void fallback_to_exposure(hiss::Stage1Diagnostics& d,
                          const char* code, const char* message) {
    set_diag(d, -1, "DARK_OPT", code, message,
             1, "OPTIMAL", "EXPOSURE_RATIO");
}

} // namespace

// ---------------------------- 主函数 ----------------------------
// 最优 Dark 系数估计
// 返回: 最优 k 值；失败时返回 k_init 并设置 diagnostics.fell_back=1
float optimize_dark_k(const float* light, const float* bias, const float* dark,
                      const float* flat, int w, int h, float k_init,
                      hiss::Stage1Diagnostics& diagnostics) {
    // flat 当前未参与回归（模型在原始 ADU 空间），显式标记避免 -Wunused-parameter
    (void)flat;

    // ---- 0. 初始化诊断为“未回退”状态，确保任何返回路径都有合理初值 ----
    set_diag(diagnostics, 0, "DARK_OPT", "UNSET", "未执行", 0, "", "");

    // ---- 1. 参数校验 ----
    // light/bias/dark 必需；flat 可选；尺寸必须有效
    if (!light || !bias || !dark || w <= 0 || h <= 0) {
        fallback_to_exposure(diagnostics, "PARAM",
                             "input invalid (light/bias/dark null or w/h<=0)");
        return k_init;
    }
    // k_init 本身必须为正有限值，否则连回退都无法给出合理值，但仍按规范返回 k_init
    if (!(k_init > 0.0f) || !std::isfinite(k_init)) {
        fallback_to_exposure(diagnostics, "BAD_K_INIT",
                             "k_init non-positive or non-finite");
        return k_init;
    }

    const int n = w * h;

    // ---- 2. 背景提取（sigma-clip 排除星点）----
    // 规范要求：不得为估计 k 再做完整星点检测，故仅用全局 sigma-clip 粗略剔除亮结构。
    // 阈值: |light - median| <= 3 * 1.4826 * MAD
    std::vector<float> tmp(light, light + n);
    float lmed = median_inplace(tmp);           // Light 全局中位数
    for (int i = 0; i < n; ++i) {
        tmp[i] = std::fabs(light[i] - lmed);
    }
    float lmad = median_inplace(tmp);           // Light 绝对偏差中位数
    float lsigma = 1.4826f * lmad;

    std::vector<unsigned char> bg(n, 0);
    if (lsigma <= 0.0f) {
        // MAD=0：图像近常数（罕见），无法区分星点，全部视为背景
        std::fill(bg.begin(), bg.end(), static_cast<unsigned char>(1));
    } else {
        const float thr = 3.0f * lsigma;
        for (int i = 0; i < n; ++i) {
            if (std::fabs(light[i] - lmed) <= thr) bg[i] = 1;
        }
    }

    // ---- 3. 分区抽样（8x8 网格）----
    // 每区最多 PER_CELL 个背景像素，总样本上限 MAX_TOTAL。
    // 单区内若背景像素多于 PER_CELL，按等步长跨步采样以保证空间代表性。
    const int GRID = 8;
    const int PER_CELL = 1000;
    const int MAX_TOTAL = 50000;

    std::vector<float> xs, ys;  // x = D - B, y = L - B
    xs.reserve(MAX_TOTAL);
    ys.reserve(MAX_TOTAL);

    int cell_w = w / GRID;
    int cell_h = h / GRID;
    if (cell_w < 1) cell_w = 1;
    if (cell_h < 1) cell_h = 1;

    bool reached_cap = false;
    for (int gy = 0; gy < GRID && !reached_cap; ++gy) {
        for (int gx = 0; gx < GRID && !reached_cap; ++gx) {
            // 计算 cell 的像素范围（最后一行/列兜底到边界，避免漏采）
            int x0 = gx * cell_w;
            int y0 = gy * cell_h;
            int x1 = (gx == GRID - 1) ? w : (gx + 1) * cell_w;
            int y1 = (gy == GRID - 1) ? h : (gy + 1) * cell_h;

            // 先收集该 cell 内全部背景像素的索引，再做跨步抽样
            std::vector<int> cell_idx;
            cell_idx.reserve(static_cast<std::size_t>(cell_w) * cell_h);
            for (int yy = y0; yy < y1; ++yy) {
                const int row = yy * w;
                for (int xx = x0; xx < x1; ++xx) {
                    if (bg[row + xx]) cell_idx.push_back(row + xx);
                }
            }

            const int cn = static_cast<int>(cell_idx.size());
            if (cn == 0) continue;

            int taken = 0;
            if (cn <= PER_CELL) {
                // 不足上限，全部取用
                for (int j = 0; j < cn; ++j) {
                    int idx = cell_idx[j];
                    xs.push_back(dark[idx] - bias[idx]);
                    ys.push_back(light[idx] - bias[idx]);
                    ++taken;
                }
            } else {
                // 等步长跨步采样，最多取 PER_CELL 个
                int stride = cn / PER_CELL;
                if (stride < 1) stride = 1;
                for (int j = 0; j < cn && taken < PER_CELL; j += stride) {
                    int idx = cell_idx[j];
                    xs.push_back(dark[idx] - bias[idx]);
                    ys.push_back(light[idx] - bias[idx]);
                    ++taken;
                }
            }

            if (static_cast<int>(xs.size()) >= MAX_TOTAL) {
                reached_cap = true;
            }
        }
    }

    const int nsamp = static_cast<int>(xs.size());

    // ---- 4. 样本不足判定 ----
    // 样本过少无法支撑稳健回归，直接回退
    if (nsamp < 100) {
        fallback_to_exposure(diagnostics, "INSUFFICIENT_SAMPLES",
                             "background samples < 100, regression aborted");
        return k_init;
    }

    // ---- 5. 鲁棒线性回归 y = c + k*x（5 轮 MAD 离群抑制）----
    // 每轮: OLS 估计 → 计算残差 → 以 3*1.4826*MAD 为阈值剔除离群点 → 下一轮
    std::vector<unsigned char> inlier(nsamp, 1);
    float k_est = k_init;   // 回归估计结果（失败时不会被使用）
    float c_est = 0.0f;

    for (int iter = 0; iter < 5; ++iter) {
        // --- 5.1 在当前内点集上做 OLS（双精度累加，避免大样本精度损失）---
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
        int cnt = 0;
        for (int i = 0; i < nsamp; ++i) {
            if (!inlier[i]) continue;
            double xi = xs[i];
            double yi = ys[i];
            sx += xi;
            sy += yi;
            sxx += xi * xi;
            sxy += xi * yi;
            ++cnt;
        }

        if (cnt < 50) {
            // 内点数过少，回归不可靠
            fallback_to_exposure(diagnostics, "TOO_FEW_INLIERS",
                                 "inlier count < 50 during robust regression");
            return k_init;
        }

        double xm = sx / cnt;
        double ym = sy / cnt;
        // 分母 = sum((x - xm)^2) = sxx - cnt*xm*xm
        double denom = sxx - static_cast<double>(cnt) * xm * xm;
        if (denom <= 1e-12) {
            // D-B 方差近零（Dark 几乎无暗电流信号），无法由回归确定 k
            fallback_to_exposure(diagnostics, "ZERO_VARIANCE",
                                 "D-B variance ~ 0, slope undefined");
            return k_init;
        }

        double k_cur = (sxy - static_cast<double>(cnt) * xm * ym) / denom;
        double c_cur = ym - k_cur * xm;
        k_est = static_cast<float>(k_cur);
        c_est = static_cast<float>(c_cur);

        // --- 5.2 计算残差并估计其 MAD ---
        std::vector<float> resid;
        resid.reserve(cnt);
        for (int i = 0; i < nsamp; ++i) {
            if (inlier[i]) {
                resid.push_back(ys[i] - (c_est + k_est * xs[i]));
            }
        }
        float rmed = median_inplace(resid);
        for (int i = 0; i < static_cast<int>(resid.size()); ++i) {
            resid[i] = std::fabs(resid[i] - rmed);
        }
        float rmad = median_inplace(resid);
        float rsigma = 1.4826f * rmad;

        if (rsigma <= 0.0f) {
            // 残差 MAD=0：要么完美拟合（罕见），要么内点完全共线。
            // 此时离群抑制无意义，保留当前估计并结束迭代。
            break;
        }

        // --- 5.3 标记并剔除离群点（|r - rmed| > 3*1.4826*MAD）---
        const float thr = 3.0f * rsigma;
        int new_inlier = 0;
        for (int i = 0; i < nsamp; ++i) {
            if (!inlier[i]) continue;
            float r = ys[i] - (c_est + k_est * xs[i]);
            if (std::fabs(r - rmed) > thr) {
                inlier[i] = 0;
            } else {
                ++new_inlier;
            }
        }

        if (new_inlier < 50) {
            // 剔除后内点过少，回归已发散
            fallback_to_exposure(diagnostics, "TOO_FEW_INLIERS",
                                 "inlier count < 50 after outlier rejection");
            return k_init;
        }

        // 若几乎未剔除新点，认为已收敛，提前结束
        if (new_inlier == cnt) break;
    }

    // ---- 6. 回归残差异常判定 ----
    // 以最终内点集计算残差 sigma 与 y 的 sigma，若残差未显著小于 y 散布，
    // 说明线性模型未能解释数据（例如 Dark 与 Light 不相关），判为异常。
    {
        std::vector<float> yvals, rvals;
        yvals.reserve(nsamp);
        rvals.reserve(nsamp);
        for (int i = 0; i < nsamp; ++i) {
            if (!inlier[i]) continue;
            yvals.push_back(ys[i]);
            rvals.push_back(ys[i] - (c_est + k_est * xs[i]));
        }
        int cn = static_cast<int>(yvals.size());
        if (cn < 50) {
            fallback_to_exposure(diagnostics, "TOO_FEW_INLIERS",
                                 "final inlier count < 50");
            return k_init;
        }
        float ymed = median_inplace(yvals);
        for (int i = 0; i < cn; ++i) yvals[i] = std::fabs(yvals[i] - ymed);
        float ymad = median_inplace(yvals);
        float ysigma = 1.4826f * ymad;

        float rmed = median_inplace(rvals);
        for (int i = 0; i < cn; ++i) rvals[i] = std::fabs(rvals[i] - rmed);
        float rmad = median_inplace(rvals);
        float rsigma = 1.4826f * rmad;

        // 残差非有限，或残差散布 >= y 散布（模型毫无解释力），判异常
        if (!std::isfinite(rsigma) || !std::isfinite(ysigma)) {
            fallback_to_exposure(diagnostics, "RESIDUAL_NAN",
                                 "residual or y sigma non-finite");
            return k_init;
        }
        if (ysigma > 0.0f && rsigma >= ysigma) {
            fallback_to_exposure(diagnostics, "RESIDUAL_ABNORMAL",
                                 "residual sigma >= y sigma, linear model invalid");
            return k_init;
        }
    }

    // ---- 7. k 值合理性判定 ----
    // 物理上 k = t_light / t_dark 应为正且有限，过大说明回归失稳
    if (!(k_est > 0.0f) || !std::isfinite(k_est) || k_est > 10.0f) {
        fallback_to_exposure(diagnostics, "K_OUT_OF_RANGE",
                             "estimated k <= 0, non-finite, or > 10");
        return k_init;
    }

    // ---- 8. 成功 ----
    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "optimize_dark_k ok: k=%.4f (k_init=%.4f, nsamp=%d)",
                  k_est, k_init, nsamp);
    set_diag(diagnostics, 0, "DARK_OPT", "OK", msg, 0, "", "");
    return k_est;
}

} // namespace ac
