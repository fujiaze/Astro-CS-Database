// noise_model.cpp — SNR/Noise 科学重构实现
//
// 三层模型 (SNR_SCIENCE_DERIVATION.md / SNR_REDESIGN_CONTRACT.md):
// 1. PhotometricCalibrationQuality — 帧级测光定标质量 (dex/mag 单位正确化)
// 2. PsfFitQuality — 星点 PSF 拟合质量代理 (A/residual_scale)
// 3. NoiseWeightModelV1 — source-masked blank-sky 稳健方差 → ivar
//
// 设计要点:
// - 控制点来自空背景噪声, 与星亮度/星族解耦 (SNR-003/SNR-010)
// - 经验 blank-sky 为 production 基线; gain+readnoise 已知时用于
// Poisson 交叉验证 (SNR-005), 缺失时经验 fallback (SNR-014)
// - variance/ivar 传播遵循 x'=αx → var'=α²var, ivar'=ivar/α² (SNR-002)
// - 旧 (A-B)/mad 不再进入 science weight (SNR-008)

#include "snr_estimator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

static std::unordered_map<const NoiseWeightModelV1*, double> g_model_floor;

constexpr double kLn10 = 2.302585092994045684017991454684; // NOISE_ESTIMATION.md / NOISE_MODEL.md 科学定义 log10↔ln 换算
// trimmed-mean-abs-residual → Gaussian σ 换算因子:
// E[10-90% trimmed mean |r|] = 0.731673 σ (Gaussian N(0,σ²)) — 锚点: docs/science/NOISE_MODEL.md 数值精度 / PSF.md
constexpr double kTrimMeanToSigma = 0.7316727929211932; // PSF.md 0.7316728 ↔ NOISE_MODEL.md robust_residual_sigma

[[maybe_unused]] bool finite(double x) { return std::isfinite(x); }

// 稳健中位数 (输入会被重排)
double robust_median(std::vector<double>& v) {
    if (v.empty()) return 0.0;
    const std::size_t n = v.size();
    std::nth_element(v.begin(), v.begin() + n / 2, v.end());
    double med = v[n / 2];
    if (n % 2 == 0) {
        std::nth_element(v.begin(), v.begin() + n / 2 - 1, v.end());
        med = (v[n / 2 - 1] + med) * 0.5;
    }
    return med;
}

// 稳健尺度: 1.4826 × median(|x − median(x)|) — 锚点: NOISE_ESTIMATION.md σ_bg / NOISE_MODEL.md 1.4826022185
double robust_sigma(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const double med = robust_median(v);
    std::vector<double> dev;
    dev.reserve(v.size());
    for (double x : v) dev.push_back(std::fabs(x - med));
    return 1.482602218505602 * robust_median(dev);
}

bool valid_pixel(double x, double saturation) {
    if (!std::isfinite(x)) return false;
    if (saturation > 0.0 && x >= saturation) return false;
    return true;
}

// patch 内 sky 样本收集 (含 cosmic 稳健裁剪)
// 返回收集到的 sky 像素值 (经过有限/饱和/星掩膜/cosmic 裁剪)
template <typename T>
bool collect_patch_sky(const T* data, int w,
                       int x0, int x1, int y0, int y1,
                       const std::vector<uint8_t>& source_mask,
                       double saturation, double clip_sigma,
                       int min_samples, int max_rounds,
                       std::vector<double>& out_samples) {
    out_samples.clear();
    out_samples.reserve((std::size_t)(x1 - x0) * (y1 - y0));
    for (int y = y0; y < y1; ++y) {
        const std::size_t row = (std::size_t)y * (std::size_t)w;
        for (int x = x0; x < x1; ++x) {
            if (!source_mask.empty() && source_mask[row + (std::size_t)x]) continue;
            const double v = (double)data[row + (std::size_t)x];
            if (!valid_pixel(v, saturation)) continue;
            out_samples.push_back(v);
        }
    }
    if ((int)out_samples.size() < min_samples) return false;
    // cosmic / hot-pixel 稳健裁剪 (以稳健中位数为位置, 稳健 σ 为尺度)
    for (int round = 0; round < max_rounds; ++round) {
        std::vector<double> cur = out_samples;
        const double med = robust_median(cur);
        const double sig = robust_sigma(cur);
        if (sig <= 0.0) break;
        const double lo = med - clip_sigma * sig;
        const double hi = med + clip_sigma * sig;
        std::vector<double> kept;
        kept.reserve(out_samples.size());
        for (double v : out_samples)
            if (v >= lo && v <= hi) kept.push_back(v);
        if ((int)kept.size() < min_samples) return false;
        if (kept.size() == out_samples.size()) break;
        out_samples.swap(kept);
    }
    return (int)out_samples.size() >= min_samples;
}

// 模板内核: 估计 blank-sky 方差模型 (float/double 数据)
template <typename T>
int noise_model_impl(const T* data, int h, int w,
                     const float* source_mask,
                     const double* star_x, const double* star_y,
                     int n_stars,
                     const SnrNoiseModelConfig* cfg,
                     NoiseWeightModelV1* out_model) {
    if (!data || !out_model || h <= 0 || w <= 0) return 3;
    SnrNoiseModelConfig c{};
    if (cfg) {
        c = *cfg;
    } else {
        snr_noise_model_v1_default_config(&c);
    }
    std::memset(out_model, 0, sizeof(NoiseWeightModelV1));
    g_model_floor[out_model] = c.variance_floor;

    // 星点掩膜（ 冻结：fixed conservative mask）：
    // 像素在任一星点固定半径内 → source (1)；所有星统一
    // rmax = max(1, source_mask_radius_px) × max(1, mask_radius_scale)。
    // API 只接收星点坐标（无 amplitude），不按星等/振幅缩放
    // （DOCS/header 已删除假 adaptive 描述）。调用方也可直接传
    // source_mask 覆盖。
    std::vector<uint8_t> mask;
    if (source_mask) {
        mask.assign((std::size_t)h * (std::size_t)w, 0);
        for (std::size_t i = 0; i < mask.size(); ++i)
            mask[i] = source_mask[i] != 0.0f;
    } else if (star_x && star_y && n_stars > 0) {
        mask.assign((std::size_t)h * (std::size_t)w, 0);
        std::vector<double> amps;
        amps.reserve((std::size_t)n_stars);
        // 无振幅信息时统一基础半径 (调用方只传坐标)
        const double r0 = std::max(1.0, c.source_mask_radius_px);
        const double rmax = r0 * std::max(1.0, c.mask_radius_scale);
        (void)amps;
        const double r2 = rmax * rmax;
        for (int i = 0; i < n_stars; ++i) {
            if (!std::isfinite(star_x[i]) || !std::isfinite(star_y[i])) continue;
            const int cx = (int)std::lround(star_x[i]);
            const int cy = (int)std::lround(star_y[i]);
            for (int dy = -(int)rmax; dy <= (int)rmax; ++dy) {
                const int py = cy + dy;
                if (py < 0 || py >= h) continue;
                const double ddy = (double)dy;
                const int dxmax = (int)std::floor(std::sqrt(r2 - ddy * ddy));
                for (int dx = -dxmax; dx <= dxmax; ++dx) {
                    const int px = cx + dx;
                    if (px < 0 || px >= w) continue;
                    mask[(std::size_t)py * (std::size_t)w + (std::size_t)px] = 1;
                }
            }
        }
    }

    const int gx = std::max(2, c.patch_grid_x);
    const int gy = std::max(2, c.patch_grid_y);
    const double clip_sigma = std::max(1.0, c.cosmic_clip_sigma);
    const int min_samples = std::max(1, c.min_patch_samples);
    const int max_rounds = std::max(0, c.max_clip_rounds);

    std::vector<double> patch_sigma;
    std::vector<double> patch_var;
    std::vector<double> ctrl_x;
    std::vector<double> ctrl_y;
    std::vector<double> samples;
    int n_rejected = 0;

    for (int py = 0; py < gy; ++py) {
        const int y0 = (int)((std::int64_t)py * h / gy);
        const int y1 = (int)((std::int64_t)(py + 1) * h / gy);
        for (int px = 0; px < gx; ++px) {
            const int x0 = (int)((std::int64_t)px * w / gx);
            const int x1 = (int)((std::int64_t)(px + 1) * w / gx);
            if (!collect_patch_sky(data, w, x0, x1, y0, y1, mask,
                                   c.saturation_level, clip_sigma,
                                   min_samples, max_rounds, samples)) {
                ++n_rejected;
                continue;
            }
            const double sig = robust_sigma(samples);
            if (!std::isfinite(sig) || sig <= 0.0) {
                ++n_rejected;
                continue;
            }
            patch_sigma.push_back(sig);
            patch_var.push_back(sig * sig);
            ctrl_x.push_back((double)(x0 + x1) * 0.5);
            ctrl_y.push_back((double)(y0 + y1) * 0.5);
        }
    }

    out_model->n_qualified_patches = (uint32_t)patch_var.size();
    out_model->n_rejected_patches = (uint32_t)n_rejected;

    // 全局兜底: 合格 patch variance 的稳健中位数
    if (!patch_var.empty()) {
        std::vector<double> vc = patch_var;
        const double vmed = robust_median(vc);
        out_model->variance_bg_global = std::max(vmed, c.variance_floor);
        out_model->sigma_bg_global = std::sqrt(out_model->variance_bg_global);
        out_model->ivar_bg_global = 1.0 / out_model->variance_bg_global;
    } else {
        // 整帧退化兜底: 全帧 sky 样本 robust scale (忽略空间结构)
        std::vector<double> all;
        all.reserve((std::size_t)h * (std::size_t)w);
        for (int y = 0; y < h; ++y) {
            const std::size_t row = (std::size_t)y * (std::size_t)w;
            for (int x = 0; x < w; ++x) {
                if (!mask.empty() && mask[row + (std::size_t)x]) continue;
                const double v = (double)data[row + (std::size_t)x];
                if (valid_pixel(v, c.saturation_level)) all.push_back(v);
            }
        }
        if ((int)all.size() < std::max(1, min_samples / 2)) {
            out_model->degenerate = 1;
            return 1;
        }
        const double sig = robust_sigma(all);
        if (!std::isfinite(sig) || sig <= 0.0) {
            out_model->degenerate = 1;
            return 1;
        }
        out_model->sigma_bg_global = sig;
        out_model->variance_bg_global = std::max(sig * sig, c.variance_floor);
        out_model->ivar_bg_global = 1.0 / out_model->variance_bg_global;
        out_model->degenerate = 1;  // 无空间分辨, 全局兜底
        return 0;
    }

    // 控制点数组
    const std::size_t n = patch_var.size();
    out_model->n_control_points = (uint32_t)n;
    out_model->ctrl_x_px = (double*)std::malloc(n * sizeof(double));
    out_model->ctrl_y_px = (double*)std::malloc(n * sizeof(double));
    out_model->ctrl_sigma = (double*)std::malloc(n * sizeof(double));
    out_model->ctrl_variance = (double*)std::malloc(n * sizeof(double));
    out_model->ctrl_ivar = (double*)std::malloc(n * sizeof(double));
    if (!out_model->ctrl_x_px || !out_model->ctrl_y_px ||
        !out_model->ctrl_sigma || !out_model->ctrl_variance ||
        !out_model->ctrl_ivar) {
        snr_noise_model_v1_free(out_model);
        return 3;
    }
    for (std::size_t i = 0; i < n; ++i) {
        const double var = std::max(patch_var[i], c.variance_floor);
        out_model->ctrl_x_px[i] = ctrl_x[i];
        out_model->ctrl_y_px[i] = ctrl_y[i];
        out_model->ctrl_sigma[i] = patch_sigma[i];
        out_model->ctrl_variance[i] = var;
        out_model->ctrl_ivar[i] = 1.0 / var;
    }
    out_model->has_spatial_field =
        (c.enable_spatial_field && n >= 4) ? 1 : 0;
    out_model->source = 0;  // empirical blank-sky (production 基线)
    return 0;
}

}  // namespace

extern "C" {

// ============================================================================
// 1. PhotometricCalibrationQuality
// ============================================================================
SNR_API int snr_phot_cal_quality(double sigma_logflux_dex, int n_matches,
                                 PhotometricCalibrationQuality* out) {
    if (!out) return 3;
    std::memset(out, 0, sizeof(PhotometricCalibrationQuality));
    out->n_matches = n_matches;
    if (!std::isfinite(sigma_logflux_dex) || sigma_logflux_dex <= 0.0) {
        out->fit_status = 2;
        return 0;
    }
    out->sigma_logflux_dex = sigma_logflux_dex;
    out->sigma_mag         = 2.5 * sigma_logflux_dex;
    out->sigma_cal_rel     = kLn10 * sigma_logflux_dex;
    out->fit_status        = (n_matches > 0) ? 0 : 1;
    return 0;
}

// ============================================================================
// 2. PsfFitQuality
// ============================================================================
SNR_API int snr_psf_fit_quality(const double* psf, int n_stars,
                                const int64_t* star_ids,
                                const uint32_t* quality_flags,
                                PsfFitQualityRow* out) {
    if (!psf || !out || n_stars < 0) return 3;
    for (int i = 0; i < n_stars; ++i) {
        const double* row = psf + i * 9;
        PsfFitQualityRow& r = out[i];
        std::memset(&r, 0, sizeof(r));
        r.flux               = row[2];
        r.amplitude_above_bg = row[6];
        r.background         = row[1];
        r.fwhm               = row[5];
        r.eccentricity       = row[8];
        r.residual_scale     = row[7];   // 原 mad 列 → 准确语义
        const uint32_t qf = quality_flags ? quality_flags[i] : 0u;
        const double status = row[0];
        if (status != 0.0) {
            r.fit_status = 1;
        } else if (qf & (SNR_QF_SATURATED | SNR_QF_HAS_SATURATED)) {
            r.fit_status = 2;
        } else if (!std::isfinite(row[6]) || !std::isfinite(row[7]) || row[7] <= 0.0) {
            r.fit_status = 3;
        } else {
            r.fit_status = 0;
        }
        if (r.residual_scale > 0.0 && std::isfinite(r.residual_scale)) {
            r.robust_residual_sigma = r.residual_scale / kTrimMeanToSigma;
            r.q_psf = r.amplitude_above_bg / r.residual_scale;
        }
        (void)star_ids;
    }
    return 0;
}

// ============================================================================
// 3. NoiseWeightModelV1
// ============================================================================
SNR_API int snr_noise_model_v1_default_config(SnrNoiseModelConfig* cfg) {
    if (!cfg) return 3;
    std::memset(cfg, 0, sizeof(SnrNoiseModelConfig));
    cfg->patch_grid_x = 8;
    cfg->patch_grid_y = 8;
    cfg->source_mask_radius_px = 10.0;
    cfg->mask_radius_scale = 6.0;
    cfg->cosmic_clip_sigma = 5.0;
    cfg->min_patch_samples = 64;
    cfg->max_clip_rounds = 2;
    cfg->enable_spatial_field = 1;
    cfg->variance_floor = 1e-12;
    return 0;
}

SNR_API int snr_noise_model_v1(const float* data, int h, int w,
                               const float* source_mask,
                               const double* star_x, const double* star_y,
                               int n_stars,
                               const SnrNoiseModelConfig* cfg,
                               NoiseWeightModelV1* out_model) {
    try { return noise_model_impl(data, h, w, source_mask, star_x, star_y, n_stars, cfg, out_model); } catch (const std::exception& e) { (void)e; return 3; } catch (...) { return 3; }
}

SNR_API int snr_noise_model_v1_f64(const double* data, int h, int w,
                                   const float* source_mask,
                                   const double* star_x, const double* star_y,
                                   int n_stars,
                                   const SnrNoiseModelConfig* cfg,
                                   NoiseWeightModelV1* out_model) {
    try { return noise_model_impl(data, h, w, source_mask, star_x, star_y, n_stars, cfg, out_model); } catch (const std::exception& e) { (void)e; return 3; } catch (...) { return 3; }
}

namespace {



// 最小二乘平面填充内核（variance field， 冻结；float 输出）
void fill_impl(const NoiseWeightModelV1* m, int h, int w,
               float* out_variance, float* out_ivar) {
    if (m->has_spatial_field && m->n_control_points >= 4) {
        // 平滑方差场: 最小二乘平面拟合 var(x,y) = a + b·x + c·y
        // （对平滑噪声梯度统计最优；负预测 clamp 到 variance_floor）。
        double mx = 0, my = 0, mv = 0;
        const uint32_t n = m->n_control_points;
        for (uint32_t i = 0; i < n; ++i) {
            mx += m->ctrl_x_px[i];
            my += m->ctrl_y_px[i];
            mv += m->ctrl_variance[i];
        }
        mx /= (double)n; my /= (double)n; mv /= (double)n;
        double sxx = 0, sxy = 0, syy = 0, sxv = 0, syv = 0;
        for (uint32_t i = 0; i < n; ++i) {
            const double X = m->ctrl_x_px[i] - mx;
            const double Y = m->ctrl_y_px[i] - my;
            const double V = m->ctrl_variance[i] - mv;
            sxx += X * X; sxy += X * Y; syy += Y * Y;
            sxv += X * V; syv += Y * V;
        }
        double b = 0, c = 0;
        const double det = sxx * syy - sxy * sxy;
        if (std::fabs(det) > 1e-24) {
            b = (sxv * syy - syv * sxy) / det;
            c = (syv * sxx - sxv * sxy) / det;
        }
        const double a = mv - b * mx - c * my;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                // W1-NOISE-002: floor propagation via internal registry (model ptr keyed), fallback 1e-12
                double floor = 1e-12;
                auto it = g_model_floor.find(m);
                if (it != g_model_floor.end() && it->second > 0.0) floor = it->second;
                const double var = std::max(a + b * (double)x + c * (double)y, floor);
                if (out_variance) out_variance[(std::size_t)y * w + x] = (float)var;
                if (out_ivar) out_ivar[(std::size_t)y * w + x] = (float)(1.0 / var);
            }
        }
    } else {
        const double var = m->variance_bg_global;
        const double ivar = m->ivar_bg_global;
        for (std::size_t i = 0; i < (std::size_t)h * (std::size_t)w; ++i) {
            if (out_variance) out_variance[i] = (float)var;
            if (out_ivar) out_ivar[i] = (float)ivar;
        }
    }
}

}  // namespace

SNR_API int snr_noise_model_v1_fill(const NoiseWeightModelV1* model,
                                    int h, int w,
                                    float* out_variance, float* out_ivar) {
    if (!model || h <= 0 || w <= 0) return 3;
    if (!out_variance && !out_ivar) return 3;
    try { fill_impl(model, h, w, out_variance, out_ivar); } catch (const std::exception& e) { (void)e; return 3; } catch (...) { return 3; }
    return 0;
}

SNR_API void snr_noise_model_v1_free(NoiseWeightModelV1* model) {
    if (!model) return;
    g_model_floor.erase(model);
    std::free(model->ctrl_x_px);
    std::free(model->ctrl_y_px);
    std::free(model->ctrl_sigma);
    std::free(model->ctrl_variance);
    std::free(model->ctrl_ivar);
    model->ctrl_x_px = nullptr;
    model->ctrl_y_px = nullptr;
    model->ctrl_sigma = nullptr;
    model->ctrl_variance = nullptr;
    model->ctrl_ivar = nullptr;
    model->n_control_points = 0;
}

SNR_API void snr_noise_scale_law(double alpha,
                                 double* variance, double* ivar) {
    if (variance) *variance = (*variance) * alpha * alpha;
    if (ivar) {
        const double a2 = alpha * alpha;
        if (a2 > 0.0 && std::isfinite(*ivar)) *ivar = (*ivar) / a2;
    }
}

SNR_API double snr_noise_gain_variance(double signal, // NOISE_ESTIMATION.md Gain/Readnoise 仅诊断(SNR-005), NOISE_MODEL.md 诊断模型
                                       double gain_e_per_adu,
                                       double read_noise_e) {
    if (gain_e_per_adu <= 0.0) return 0.0;
    const double s = std::max(0.0, signal);
    return s / gain_e_per_adu +
           (read_noise_e * read_noise_e) /
               (gain_e_per_adu * gain_e_per_adu);
}

}  // extern "C"
