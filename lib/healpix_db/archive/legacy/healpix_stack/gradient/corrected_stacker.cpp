// corrected_stacker.cpp - 阶段3 梯度校正叠加实现
//
// 实现 spec §3.5:
//   1. 预计算: 每帧每个 ipix 的 g_i(ra,dec) → g_cache (避免 sigma-clip 重复评估)
//   2. 第一遍: SNR² 加权累加 corrected = pixel - g_i
//   3. sigma-clip 迭代: 剔除 |corrected - mean| > sigma × std
//   4. 输出: mean = sum_w / weight
//
// 复用 hp_stack_hiss 的 SNR² 加权 sigma-clip 逻辑, 增加梯度校正

#include "corrected_stacker.h"
#include "healpix_core.h"  // healpix::HealpixCore

#include <cmath>
#include <limits>
#include <algorithm>
#include <unordered_map>

namespace gradient {

// ============================================================================
// 构造/析构
// ============================================================================
CorrectedStacker::CorrectedStacker() {}
CorrectedStacker::~CorrectedStacker() {}

// ============================================================================
// pix2ang: ipix → (ra_deg, dec_deg)
// ============================================================================
void CorrectedStacker::pix2radec(int64_t ipix, int nside, bool nested,
                                  double* ra_deg, double* dec_deg) {
    healpix::HealpixCore core(nside, nested);
    core.pix2radec(ipix, ra_deg, dec_deg);
}

// ============================================================================
// stack: 梯度校正 + SNR² 加权 sigma-clip 叠加
// ============================================================================
int CorrectedStacker::stack(const FrameData* frames, int n_frames,
                             const SplineModel* models, int n_models,
                             int nside, bool nested,
                             const CorrectedStackParams& params,
                             StackResult& out) {
    // 重置输出
    out.ipix.clear();
    out.mean.clear();
    out.count.clear();
    out.weight.clear();
    out.nside = nside;
    out.nested = nested;

    // 输入校验
    if (n_frames <= 0 || !frames) {
        error_msg_ = "stack: empty input (n_frames<=0 or null)";
        return 1;
    }
    if (nside <= 0 || (nside & (nside - 1)) != 0) {
        error_msg_ = "stack: invalid nside (must be power of 2)";
        return 2;
    }
    bool has_models = (models != nullptr && n_models > 0);

    // 检查帧数据非空
    bool any_data = false;
    for (int f = 0; f < n_frames; ++f) {
        if (!frames[f].ipix.empty() && frames[f].ipix.size() == frames[f].pixel.size()) {
            any_data = true;
            break;
        }
    }
    if (!any_data) {
        error_msg_ = "stack: all frames empty or size mismatch";
        return 3;
    }

    // 创建 HEALPix 核心 (复用)
    healpix::HealpixCore core(nside, nested);

    // ========================================================================
    // 1. 预计算: 每帧每个 ipix 的 g_i(ra,dec)
    //    g_cache[f][k] = g_{frame_f}(ra_k, dec_k)
    //    避免 sigma-clip 重复评估 (O(n_pix × n_ctrl) → 一次性)
    //
    //    防 extrapolation 爆炸 (小 FOV 球谐病态):
    //    评估控制点上的 g_i, 计算 max|g_ctrl|。
    //    像素 |g_i| 超过 10×max|g_ctrl| 视为爆炸, clamp 到边界。
    //    (保守策略: 只处理明显爆炸值, 不影响正常外推)
    // ========================================================================
    std::vector<std::vector<double>> g_cache(n_frames);
    if (has_models) {
        for (int f = 0; f < n_frames; ++f) {
            const int n_pix = (int)frames[f].ipix.size();
            g_cache[f].resize(n_pix, 0.0);

            // 查找该帧的 SplineModel
            int model_idx = frames[f].frame_id;
            if (model_idx < 0 || model_idx >= n_models) model_idx = f;
            if (model_idx >= 0 && model_idx < n_models && models[model_idx].valid) {
                const SplineModel& model = models[model_idx];

                // 批量评估所有像素
                std::vector<double> ra_arr(n_pix), dec_arr(n_pix);
                for (int k = 0; k < n_pix; ++k) {
                    core.pix2radec(frames[f].ipix[k], &ra_arr[k], &dec_arr[k]);
                }
                spline_.evaluateBatch(model,
                                   ra_arr.data(), dec_arr.data(),
                                   n_pix, g_cache[f].data());

                // ---- 保守 clamp: 只处理明显爆炸值 ----
                const int n_ctrl = (int)model.ctrl_ra_deg.size();
                if (n_ctrl >= 5) {
                    // 评估控制点上的 g_i
                    std::vector<double> g_ctrl(n_ctrl);
                    spline_.evaluateBatch(model,
                                       model.ctrl_ra_deg.data(),
                                       model.ctrl_dec_deg.data(),
                                       n_ctrl, g_ctrl.data());

                    double g_ctrl_max_abs = 0.0;
                    for (int k = 0; k < n_ctrl; ++k) {
                        if (std::isfinite(g_ctrl[k]) && std::fabs(g_ctrl[k]) > g_ctrl_max_abs) {
                            g_ctrl_max_abs = std::fabs(g_ctrl[k]);
                        }
                    }
                    // 爆炸阈值: 10× max|g_ctrl| + 10 ADU
                    double explode_limit = 10.0 * g_ctrl_max_abs + 10.0;
                    double lo = -explode_limit;
                    double hi =  explode_limit;

                    for (int k = 0; k < n_pix; ++k) {
                        if (!std::isfinite(g_cache[f][k])) {
                            g_cache[f][k] = 0.0;
                        } else if (g_cache[f][k] < lo) {
                            g_cache[f][k] = lo;
                        } else if (g_cache[f][k] > hi) {
                            g_cache[f][k] = hi;
                        }
                    }
                }
            }
            // 如果 model 无效, g_cache 全 0 (不校正)
        }
    }

    // ========================================================================
    // 2. 建立全局 ipix → index 映射
    // ========================================================================
    std::unordered_map<int64_t, int> ipix_to_idx;
    std::vector<int64_t> all_ipix;

    for (int f = 0; f < n_frames; ++f) {
        const int n_pix = (int)frames[f].ipix.size();
        for (int k = 0; k < n_pix; ++k) {
            int64_t ip = frames[f].ipix[k];
            if (ipix_to_idx.find(ip) == ipix_to_idx.end()) {
                ipix_to_idx[ip] = (int)all_ipix.size();
                all_ipix.push_back(ip);
            }
        }
    }

    const int n_unique = (int)all_ipix.size();
    if (n_unique == 0) {
        error_msg_ = "stack: no unique ipix";
        return 3;
    }

    // ========================================================================
    // 3. 分配累加数组
    //    weight_arr:   Σ(SNR²)           — SNR² 权重累加
    //    sum_w_arr:    Σ(corrected × SNR²) — 加权和
    //    sum_wsq_arr:  Σ(corrected² × SNR²) — 加权平方和
    //    count_arr:    Σ(1)              — 帧数
    // ========================================================================
    std::vector<double> weight_arr(n_unique, 0.0);
    std::vector<double> sum_w_arr(n_unique, 0.0);
    std::vector<double> sum_wsq_arr(n_unique, 0.0);
    std::vector<double> count_arr(n_unique, 0.0);

    // 第一遍累加
    for (int f = 0; f < n_frames; ++f) {
        const int n_pix = (int)frames[f].ipix.size();
        const bool has_snr = !frames[f].snr.empty();
        for (int k = 0; k < n_pix; ++k) {
            const int64_t ip = frames[f].ipix[k];
            auto it = ipix_to_idx.find(ip);
            if (it == ipix_to_idx.end()) continue;
            const int idx = it->second;

            // 梯度校正
            const double g_i = (has_models && !g_cache[f].empty()) ? g_cache[f][k] : 0.0;
            const double corrected = (double)frames[f].pixel[k] - g_i;

            // SNR² 权重 (无 SNR 时等权=1.0)
            const double snr_val = has_snr ? (double)frames[f].snr[k] : 1.0;
            const double w = snr_val * snr_val;

            weight_arr[idx]   += w;
            sum_w_arr[idx]    += corrected * w;
            sum_wsq_arr[idx]  += corrected * corrected * w;
            count_arr[idx]    += 1.0;
        }
    }

    // ========================================================================
    // 4. sigma-clip 迭代
    //    GAP-017: 根据 use_winsorized 切换:
    //      - false (默认): 普通 SNR²加权 sigma-clip (保持原逻辑)
    //      - true: Winsorized sigma clip (按分位数缩尾后计算稳健 mean/std)
    // ========================================================================
    if (params.use_winsorized) {
        // ---- Winsorized sigma clip ----
        // 每轮对每个 ipix 收集未剔除帧的 corrected 值, 排序后取分位数缩尾,
        // 用缩尾后的样本计算 mean/std, 再用原值判断 |x-mean| > sigma*std 剔除
        // masked[f][k]=1 表示该 (frame,pixel) 已被剔除, 不再参与后续统计
        std::vector<std::vector<uint8_t>> masked(n_frames);
        for (int f = 0; f < n_frames; ++f) {
            masked[f].assign(frames[f].ipix.size(), 0);
        }

        // 校验分位数范围 [0, 1]
        double low_pct  = params.winsorize_low_pct;
        double high_pct = params.winsorize_high_pct;
        if (low_pct  < 0.0 || low_pct  > 1.0) low_pct  = 0.05;
        if (high_pct < 0.0 || high_pct > 1.0) high_pct = 0.95;
        if (low_pct >= high_pct) { low_pct = 0.05; high_pct = 0.95; }

        for (int iter = 0; iter < params.max_iter; ++iter) {
            // 4.1 收集每个 ipix 的未剔除 corrected 值
            std::vector<std::vector<double>> vals_per_ipix(n_unique);
            for (int f = 0; f < n_frames; ++f) {
                const int n_pix = (int)frames[f].ipix.size();
                for (int k = 0; k < n_pix; ++k) {
                    if (masked[f][k]) continue;
                    const int64_t ip = frames[f].ipix[k];
                    auto it = ipix_to_idx.find(ip);
                    if (it == ipix_to_idx.end()) continue;
                    const int idx = it->second;
                    const double g_i = (has_models && !g_cache[f].empty()) ? g_cache[f][k] : 0.0;
                    const double corrected = (double)frames[f].pixel[k] - g_i;
                    vals_per_ipix[idx].push_back(corrected);
                }
            }

            // 4.2 计算每个 ipix 的 winsorized mean/std
            std::vector<double> mean_arr(n_unique, 0.0);
            std::vector<double> std_arr(n_unique, 0.0);
            for (int i = 0; i < n_unique; ++i) {
                auto& vals = vals_per_ipix[i];
                if (vals.size() < 2) continue;
                std::sort(vals.begin(), vals.end());
                size_t n = vals.size();
                size_t low_idx  = (size_t)(low_pct  * n);
                size_t high_idx = (size_t)(high_pct * n);
                if (low_idx  >= n) low_idx  = 0;
                if (high_idx >= n) high_idx = n - 1;
                double low_val  = vals[low_idx];
                double high_val = vals[high_idx];

                // 缩尾: 把低于 low_val 的值替换为 low_val, 高于 high_val 的替换为 high_val
                double sum = 0.0;
                for (double v : vals) {
                    double wv = v;
                    if (wv < low_val)  wv = low_val;
                    if (wv > high_val) wv = high_val;
                    sum += wv;
                }
                double mean = sum / n;
                double var = 0.0;
                for (double v : vals) {
                    double wv = v;
                    if (wv < low_val)  wv = low_val;
                    if (wv > high_val) wv = high_val;
                    var += (wv - mean) * (wv - mean);
                }
                mean_arr[i] = mean;
                std_arr[i]  = std::sqrt(var / n);
            }

            // 4.3 扫描剔除超出 sigma*winsorized_std 的点 (用原 corrected 值判断)
            bool any_clip = false;
            for (int f = 0; f < n_frames; ++f) {
                const int n_pix = (int)frames[f].ipix.size();
                const bool has_snr = !frames[f].snr.empty();
                for (int k = 0; k < n_pix; ++k) {
                    if (masked[f][k]) continue;
                    const int64_t ip = frames[f].ipix[k];
                    auto it = ipix_to_idx.find(ip);
                    if (it == ipix_to_idx.end()) continue;
                    const int idx = it->second;

                    if (count_arr[idx] <= 1.0 || std_arr[idx] <= 0.0) continue;

                    const double g_i = (has_models && !g_cache[f].empty()) ? g_cache[f][k] : 0.0;
                    const double corrected = (double)frames[f].pixel[k] - g_i;
                    const double dev = std::fabs(corrected - mean_arr[idx]);

                    if (dev > params.sigma * std_arr[idx]) {
                        // 剔除: 更新累加器 + masked
                        const double snr_val = has_snr ? (double)frames[f].snr[k] : 1.0;
                        const double w = snr_val * snr_val;
                        weight_arr[idx]   -= w;
                        sum_w_arr[idx]    -= corrected * w;
                        sum_wsq_arr[idx]  -= corrected * corrected * w;
                        count_arr[idx]    -= 1.0;
                        masked[f][k] = 1;
                        any_clip = true;
                    }
                }
            }

            if (!any_clip) break;  // 收敛
        }
    } else {
        // ---- 普通 SNR²加权 sigma-clip (原逻辑) ----
        for (int iter = 0; iter < params.max_iter; ++iter) {
            // 计算加权 mean 和 std
            std::vector<double> mean_arr(n_unique, 0.0);
            std::vector<double> std_arr(n_unique, 0.0);
            bool any_clip = false;

            for (int i = 0; i < n_unique; ++i) {
                if (weight_arr[i] > 0.0 && count_arr[i] > 1.0) {
                    mean_arr[i] = sum_w_arr[i] / weight_arr[i];
                    double var = sum_wsq_arr[i] / weight_arr[i] - mean_arr[i] * mean_arr[i];
                    if (var < 0.0) var = 0.0;
                    std_arr[i] = std::sqrt(var);
                }
            }

            // 重新扫描, 剔除离群值
            for (int f = 0; f < n_frames; ++f) {
                const int n_pix = (int)frames[f].ipix.size();
                const bool has_snr = !frames[f].snr.empty();
                for (int k = 0; k < n_pix; ++k) {
                    const int64_t ip = frames[f].ipix[k];
                    auto it = ipix_to_idx.find(ip);
                    if (it == ipix_to_idx.end()) continue;
                    const int idx = it->second;

                    if (count_arr[idx] <= 1.0 || std_arr[idx] <= 0.0) continue;

                    const double g_i = (has_models && !g_cache[f].empty()) ? g_cache[f][k] : 0.0;
                    const double corrected = (double)frames[f].pixel[k] - g_i;
                    const double dev = std::fabs(corrected - mean_arr[idx]);

                    if (dev > params.sigma * std_arr[idx]) {
                        // 剔除
                        const double snr_val = has_snr ? (double)frames[f].snr[k] : 1.0;
                        const double w = snr_val * snr_val;
                        weight_arr[idx]   -= w;
                        sum_w_arr[idx]    -= corrected * w;
                        sum_wsq_arr[idx]  -= corrected * corrected * w;
                        count_arr[idx]    -= 1.0;
                        any_clip = true;
                    }
                }
            }

            if (!any_clip) break;  // 收敛
        }
    }

    // ========================================================================
    // 5. 输出: mean = sum_w / weight
    // ========================================================================
    out.ipix = all_ipix;
    out.mean.resize(n_unique);
    out.count = count_arr;
    out.weight = weight_arr;

    for (int i = 0; i < n_unique; ++i) {
        if (weight_arr[i] > 0.0 && count_arr[i] > 0.0) {
            out.mean[i] = sum_w_arr[i] / weight_arr[i];
        } else {
            out.mean[i] = 0.0;
        }
    }

    return 0;
}

} // namespace gradient
