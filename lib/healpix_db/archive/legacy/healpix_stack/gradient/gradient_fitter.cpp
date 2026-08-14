// gradient_fitter.cpp - 阶段2 差异拟合实现 (无迭代)
//
// 实现 spec §3.4 算法:
//   1. 按 frame_id 分组样本
//   2. 计算参考场 ref(p) = Σ_{j covers p} w_j·bg_j / Σ w_j  (SNR²加权)
//   3. 差异计算 diff_i(p) = bg_i(p) - ref(p)
//   4. sigma-clip 异常值过滤
//   5. 3D 嵌入球面样条拟合 (一次性, 无 Gauss-Seidel 迭代)
//   6. gauge fixing 加权归零
//
// 暗环避免 (PMM 风格):
//   差异拟合 → 亮天体光度在 diff 中抵消 → 避免低阶拟合产生暗环
//
// 帧间重叠判断: 对帧 i 的控制点 p_i, 在帧 j (j≠i) 的控制点中暴力查找最近点 p_j,
// 若 |p_i - p_j| < match_threshold, 则 j 覆盖 p_i。
// 暴力查找复杂度 O(n_i × n_j), 对 n=500, 10帧 ~ 2.5s, 可接受。

#include "gradient_fitter.h"

#include <cmath>
#include <algorithm>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gradient {

namespace {
constexpr double DEG2RAD = M_PI / 180.0;
constexpr double ARCSEC2RAD = M_PI / (180.0 * 3600.0);

// 帧的样本分组
struct FrameSamples {
    int32_t frame_id;
    std::vector<double> ra;    // 控制点 ra (度)
    std::vector<double> dec;   // 控制点 dec (度)
    std::vector<double> bg;    // 背景中位数
    std::vector<double> snr;   // SNR-B
    std::vector<double> snr_sq; // SNR² (权重)
    SplineModel model;          // 拟合的 g_i
};
} // namespace

// ============================================================================
// 构造/析构
// ============================================================================
GradientFitter::GradientFitter() {}
GradientFitter::~GradientFitter() {}

// ============================================================================
// 球面大圆弧角 (弧度)
// ============================================================================
double GradientFitter::greatCircleDistanceRad(double ra1_deg, double dec1_deg,
                                               double ra2_deg, double dec2_deg) {
    const double dec1 = dec1_deg * DEG2RAD;
    const double dec2 = dec2_deg * DEG2RAD;
    const double dra  = (ra1_deg - ra2_deg) * DEG2RAD;
    double cos_g = std::sin(dec1) * std::sin(dec2) +
                   std::cos(dec1) * std::cos(dec2) * std::cos(dra);
    if (cos_g >  1.0) cos_g =  1.0;
    if (cos_g < -1.0) cos_g = -1.0;
    return std::acos(cos_g);
}

// ============================================================================
// 中位数计算
// ============================================================================
double GradientFitter::median(std::vector<double>& vals) {
    if (vals.empty()) return 0.0;
    std::sort(vals.begin(), vals.end());
    size_t n = vals.size();
    return (n % 2 == 0) ? (vals[n/2 - 1] + vals[n/2]) * 0.5 : vals[n/2];
}

// ============================================================================
// MAD (median absolute deviation) 计算
// ============================================================================
double GradientFitter::mad(const std::vector<double>& vals, double med) {
    if (vals.empty()) return 0.0;
    std::vector<double> abs_devs;
    abs_devs.reserve(vals.size());
    for (double v : vals) {
        abs_devs.push_back(std::fabs(v - med));
    }
    std::sort(abs_devs.begin(), abs_devs.end());
    size_t n = abs_devs.size();
    return (n % 2 == 0) ? (abs_devs[n/2 - 1] + abs_devs[n/2]) * 0.5 : abs_devs[n/2];
}

// ============================================================================
// fit: 差异拟合 (一次性, 无迭代)
// ============================================================================
int GradientFitter::fit(const SampleRow* rows, int n_rows,
                         const int32_t* frame_ids, int n_frames,
                         const FitterParams& params,
                         FitterResult& out) {
    // 重置输出
    out.models.clear();
    out.frame_ids.clear();
    out.n_clipped_per_frame.clear();
    out.fit_rms_per_frame.clear();
    out.success = false;

    // 输入校验
    if (n_rows <= 0 || !rows || n_frames <= 0 || !frame_ids) {
        error_msg_ = "fit: empty input";
        return 1;
    }

    // 1. 按 frame_id 分组样本
    std::unordered_map<int32_t, int> frame_id_to_idx;
    std::vector<FrameSamples> frames(n_frames);
    for (int f = 0; f < n_frames; ++f) {
        frames[f].frame_id = frame_ids[f];
        frames[f].model.valid = false;  // 初始化 g_i = 0
        frame_id_to_idx[frame_ids[f]] = f;
    }

    for (int r = 0; r < n_rows; ++r) {
        auto it = frame_id_to_idx.find(rows[r].frame_id);
        if (it == frame_id_to_idx.end()) continue;
        int f = it->second;
        frames[f].ra.push_back(rows[r].cp_ra);
        frames[f].dec.push_back(rows[r].cp_dec);
        frames[f].bg.push_back(rows[r].bg_median);
        frames[f].snr.push_back(rows[r].snr);
        // SNR² 权重 (spec §3.4.2: w_j = SNR_j²)
        double snr_val = static_cast<double>(rows[r].snr);
        frames[f].snr_sq.push_back(snr_val * snr_val);
    }

    // 检查有效帧
    int valid_frames = 0;
    for (int f = 0; f < n_frames; ++f) {
        if ((int)frames[f].ra.size() >= 5) {  // 样条至少需要 5 点
            valid_frames++;
        } else {
            frames[f].model.valid = false;
        }
    }
    if (valid_frames == 0) {
        error_msg_ = "fit: no valid frames (all have <5 samples)";
        return 2;
    }

    // 样条拟合参数
    SplineParams spline_params;
    spline_params.lambda = params.lambda;

    const double match_threshold_rad = params.match_threshold_arcsec * ARCSEC2RAD;

    // 2. 逐帧计算 ref(p) → diff_i(p) → sigma-clip → 样条拟合
    out.n_clipped_per_frame.resize(n_frames, 0);
    out.fit_rms_per_frame.resize(n_frames, 0.0);

    for (int i = 0; i < n_frames; ++i) {
        const int n_ctrl_i = (int)frames[i].ra.size();
        if (n_ctrl_i < 5) {
            // 无效帧, g_i = 0
            continue;
        }

        // 计算参考场 ref(p) = Σ_{j covers p} w_j·bg_j / Σ w_j  (SNR²加权)
        // 差异 diff_i(p) = bg_i(p) - ref(p)
        std::vector<double> diff_i(n_ctrl_i);
        for (int p = 0; p < n_ctrl_i; ++p) {
            const double ra_p = frames[i].ra[p];
            const double dec_p = frames[i].dec[p];

            double ref_num = 0.0;
            double ref_den = 0.0;
            for (int j = 0; j < n_frames; ++j) {
                if (j == i) continue;
                const int n_ctrl_j = (int)frames[j].ra.size();
                if (n_ctrl_j == 0) continue;

                // 在帧 j 中找最近控制点 p_j
                int nearest_idx = -1;
                double nearest_dist = 1e18;
                for (int q = 0; q < n_ctrl_j; ++q) {
                    double d = greatCircleDistanceRad(ra_p, dec_p,
                                                      frames[j].ra[q],
                                                      frames[j].dec[q]);
                    if (d < nearest_dist) {
                        nearest_dist = d;
                        nearest_idx = q;
                    }
                }

                // 如果距离 < threshold, j 覆盖 p
                if (nearest_idx >= 0 && nearest_dist < match_threshold_rad) {
                    double w_j = frames[j].snr_sq[nearest_idx];  // SNR² 权重
                    double bg_j = frames[j].bg[nearest_idx];
                    // 差异拟合: ref 不含 g_j 项 (无迭代)
                    ref_num += w_j * bg_j;
                    ref_den += w_j;
                }
            }

            double ref_p = (ref_den > 0) ? ref_num / ref_den : 0.0;
            diff_i[p] = frames[i].bg[p] - ref_p;
        }

        // 3. sigma-clip 异常值过滤 (spec §3.4.3)
        std::vector<double> diff_copy = diff_i;  // 副本用于计算 med/mad
        double med = median(diff_copy);
        double mad_val = mad(diff_i, med);
        double threshold = std::max(params.sigma_clip_factor * 1.4826 * mad_val,
                                     params.sigma_clip_floor);

        // 保留 |diff_i(p) - med| < threshold 的控制点
        std::vector<double> kept_ra, kept_dec, kept_diff, kept_snr_sq;
        int n_clipped = 0;
        for (int p = 0; p < n_ctrl_i; ++p) {
            if (std::fabs(diff_i[p] - med) < threshold) {
                kept_ra.push_back(frames[i].ra[p]);
                kept_dec.push_back(frames[i].dec[p]);
                kept_diff.push_back(diff_i[p]);
                kept_snr_sq.push_back(frames[i].snr_sq[p]);
            } else {
                n_clipped++;
            }
        }
        out.n_clipped_per_frame[i] = n_clipped;

        // 检查过滤后是否仍有足够点
        if ((int)kept_ra.size() < 5) {
            // 过滤后点数不足, 用原始 diff_i 拟合 (不过滤)
            kept_ra = frames[i].ra;
            kept_dec = frames[i].dec;
            kept_diff = diff_i;
            kept_snr_sq = frames[i].snr_sq;
        }

        // 4. 3D 嵌入球面样条拟合 (一次性, 无迭代)
        SplineModel new_model;
        int rc = spline_.fit(kept_ra.data(), kept_dec.data(),
                              kept_diff.data(),
                              kept_snr_sq.data(),
                              (int)kept_ra.size(), spline_params, new_model);
        if (rc != 0 || !new_model.valid) {
            // 拟合失败, g_i = 0
            continue;
        }

        frames[i].model = std::move(new_model);
        out.fit_rms_per_frame[i] = frames[i].model.fit_rms;
    }

    // 5. gauge fixing: 加权平均归零
    if (params.enable_gauge_fixing) {
        double total_num = 0.0;
        double total_den = 0.0;
        for (int i = 0; i < n_frames; ++i) {
            if (!frames[i].model.valid) continue;
            const int n_ctrl = (int)frames[i].ra.size();
            for (int p = 0; p < n_ctrl; ++p) {
                double w = frames[i].snr_sq[p];  // SNR² 权重
                double g = spline_.evaluate(frames[i].model,
                                             frames[i].ra[p], frames[i].dec[p]);
                total_num += w * g;
                total_den += w;
            }
        }
        if (total_den > 0) {
            double offset = total_num / total_den;
            // 从常数项 v[0] 中减去 offset
            for (int i = 0; i < n_frames; ++i) {
                if (frames[i].model.valid) {
                    frames[i].model.v[0] -= offset;
                }
            }
        }
    }

    // 填充输出
    out.frame_ids.assign(frame_ids, frame_ids + n_frames);
    out.models.resize(n_frames);
    for (int i = 0; i < n_frames; ++i) {
        out.models[i] = std::move(frames[i].model);
    }
    out.success = true;

    return 0;
}

} // namespace gradient
