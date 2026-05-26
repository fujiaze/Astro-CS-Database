#include "sdet_image.h"
#include "sdet_log.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>

namespace {

inline float sdet_mirror_fetch(const float* src, int w, int h, int x, int y) {
    if (x < 0) x = -x - 1;
    if (x >= w) x = 2 * w - x - 1;
    if (y < 0) y = -y - 1;
    if (y >= h) y = 2 * h - y - 1;
    x = std::max(0, std::min(x, w - 1));
    y = std::max(0, std::min(y, h - 1));
    return src[y * w + x];
}

}

void sdet_gaussian_filter_separable(const float* src, float* dst, int w, int h, double sigma) {
    if (sigma <= 0.5) {
        if (src != dst) std::memcpy(dst, src, (size_t)w * h * sizeof(float));
        return;
    }

    int radius = (int)std::ceil(3.0 * sigma);
    int ksize = 2 * radius + 1;
    std::vector<double> kernel(ksize);
    double sum = 0.0;
    for (int i = 0; i < ksize; i++) {
        double d = i - radius;
        kernel[i] = std::exp(-0.5 * d * d / (sigma * sigma));
        sum += kernel[i];
    }
    for (int i = 0; i < ksize; i++) kernel[i] /= sum;

    std::vector<float> tmp((size_t)w * h);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double val = 0.0;
            for (int k = -radius; k <= radius; k++) {
                val += (double)sdet_mirror_fetch(src, w, h, x + k, y) * kernel[k + radius];
            }
            tmp[y * w + x] = (float)val;
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double val = 0.0;
            for (int k = -radius; k <= radius; k++) {
                val += (double)sdet_mirror_fetch(tmp.data(), w, h, x, y + k) * kernel[k + radius];
            }
            dst[y * w + x] = (float)val;
        }
    }
}

void sdet_median_filter_3x3(const float* src, float* dst, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float p[9];
            int idx = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    p[idx++] = sdet_mirror_fetch(src, w, h, x + dx, y + dy);
                }
            }

            #define SW(a,b) if(p[a]>p[b]) std::swap(p[a],p[b])
            SW(1,2); SW(4,5); SW(7,8);
            SW(0,1); SW(3,4); SW(6,7);
            SW(1,2); SW(4,5); SW(7,8);
            SW(0,3); SW(5,8); SW(4,7);
            SW(3,6); SW(1,4); SW(2,5);
            SW(4,7); SW(4,2); SW(6,4);
            SW(4,2);
            #undef SW

            dst[y * w + x] = p[4];
        }
    }
}

void sdet_median_filter_5x5(const float* src, float* dst, int w, int h) {
    std::vector<float> buf(25);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = 0;
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    buf[idx++] = sdet_mirror_fetch(src, w, h, x + dx, y + dy);
                }
            }
            std::nth_element(buf.begin(), buf.begin() + 12, buf.end());
            dst[y * w + x] = buf[12];
        }
    }
}

void sdet_median_filter(const float* src, float* dst, int w, int h, int radius) {
    if (radius == 1) {
        sdet_median_filter_3x3(src, dst, w, h);
        return;
    }
    if (radius == 2) {
        sdet_median_filter_5x5(src, dst, w, h);
        return;
    }

    int ksize = 2 * radius + 1;
    int n = ksize * ksize;
    std::vector<float> buf(n);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = 0;
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    buf[idx++] = sdet_mirror_fetch(src, w, h, x + dx, y + dy);
                }
            }
            std::nth_element(buf.begin(), buf.begin() + n / 2, buf.end());
            dst[y * w + x] = buf[n / 2];
        }
    }
}

namespace {

void sdet_morph_op_box(const float* src, float* dst, int w, int h, int radius, bool is_dilate) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float val = is_dilate ? -1e30f : 1e30f;
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    float v = sdet_mirror_fetch(src, w, h, x + dx, y + dy);
                    if (is_dilate) val = std::max(val, v);
                    else val = std::min(val, v);
                }
            }
            dst[y * w + x] = val;
        }
    }
}

void sdet_morph_op_circle(const float* src, float* dst, int w, int h, int radius, bool is_dilate) {
    std::vector<std::pair<int,int>> offsets;
    float r2 = (float)(radius * radius);
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= r2) {
                offsets.push_back({dx, dy});
            }
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float val = is_dilate ? -1e30f : 1e30f;
            for (auto& off : offsets) {
                float v = sdet_mirror_fetch(src, w, h, x + off.first, y + off.second);
                if (is_dilate) val = std::max(val, v);
                else val = std::min(val, v);
            }
            dst[y * w + x] = val;
        }
    }
}

}

void sdet_dilate_box(const float* src, float* dst, int w, int h, int radius) {
    sdet_morph_op_box(src, dst, w, h, radius, true);
}

void sdet_erode_box(const float* src, float* dst, int w, int h, int radius) {
    sdet_morph_op_box(src, dst, w, h, radius, false);
}

void sdet_dilate_circle(const float* src, float* dst, int w, int h, int radius) {
    sdet_morph_op_circle(src, dst, w, h, radius, true);
}

void sdet_erode_circle(const float* src, float* dst, int w, int h, int radius) {
    sdet_morph_op_circle(src, dst, w, h, radius, false);
}

void sdet_truncate_and_rescale(float* data, int n, float min_val, float max_val) {
    float range = max_val - min_val;
    if (range <= 0.0f) {
        for (int i = 0; i < n; i++) data[i] = 0.0f;
        return;
    }
    for (int i = 0; i < n; i++) {
        float v = data[i];
        if (v < min_val) v = min_val;
        if (v > max_val) v = max_val;
        data[i] = (v - min_val) / range;
    }
}

void sdet_local_maxima_map(const float* src, float* dst, int w, int h, int radius, float limit) {
    std::vector<float> dilated((size_t)w * h);

    std::vector<std::pair<int,int>> se;
    float r2 = (float)(radius * radius);
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx == 0 && dy == 0) continue;
            if (dx * dx + dy * dy <= r2) {
                se.push_back({dx, dy});
            }
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float val = -1e30f;
            for (auto& off : se) {
                float v = sdet_mirror_fetch(src, w, h, x + off.first, y + off.second);
                if (v > val) val = v;
            }
            dilated[y * w + x] = val;
        }
    }

    for (int i = 0; i < w * h; i++) {
        dst[i] = (dilated[i] < src[i] && src[i] < limit) ? 1.0f : 0.0f;
    }
}

float sdet_robust_median(const float* data, int n) {
    if (n <= 0) return 0.0f;
    std::vector<float> tmp(data, data + n);
    std::nth_element(tmp.begin(), tmp.begin() + n / 2, tmp.end());
    if (n % 2 == 0) {
        float a = tmp[n / 2 - 1];
        float b = tmp[n / 2];
        std::nth_element(tmp.begin(), tmp.begin() + n / 2 - 1, tmp.begin() + n / 2);
        a = tmp[n / 2 - 1];
        return (a + b) * 0.5f;
    }
    return tmp[n / 2];
}

float sdet_robust_mad(const float* data, int n) {
    if (n <= 0) return 0.0f;
    float med = sdet_robust_median(data, n);
    std::vector<float> deviations(n);
    for (int i = 0; i < n; i++) {
        deviations[i] = std::fabs(data[i] - med);
    }
    float mad = sdet_robust_median(deviations.data(), n);
    return mad * 1.4826f;
}

void sdet_downsample(const float* src, int sw, int sh, float* dst, int dw, int dh) {
    float x_ratio = (float)sw / (float)dw;
    float y_ratio = (float)sh / (float)dh;

    for (int dy = 0; dy < dh; dy++) {
        float sy0 = dy * y_ratio;
        float sy1 = (dy + 1) * y_ratio;
        int iy0 = (int)sy0;
        int iy1 = (int)std::ceil(sy1);
        iy1 = std::min(iy1, sh);

        for (int dx = 0; dx < dw; dx++) {
            float sx0 = dx * x_ratio;
            float sx1 = (dx + 1) * x_ratio;
            int ix0 = (int)sx0;
            int ix1 = (int)std::ceil(sx1);
            ix1 = std::min(ix1, sw);

            double sum = 0.0;
            int count = 0;
            for (int y = iy0; y < iy1; y++) {
                for (int x = ix0; x < ix1; x++) {
                    sum += (double)src[y * sw + x];
                    count++;
                }
            }
            dst[dy * dw + dx] = (count > 0) ? (float)(sum / count) : 0.0f;
        }
    }
}

void sdet_upsample_bilinear(const float* src, int sw, int sh, float* dst, int dw, int dh) {
    float x_ratio = (float)(sw - 1) / (float)(dw - 1);
    float y_ratio = (float)(sh - 1) / (float)(dh - 1);

    for (int dy = 0; dy < dh; dy++) {
        float sy = dy * y_ratio;
        int iy0 = (int)sy;
        int iy1 = std::min(iy0 + 1, sh - 1);
        float fy = sy - iy0;

        for (int dx = 0; dx < dw; dx++) {
            float sx = dx * x_ratio;
            int ix0 = (int)sx;
            int ix1 = std::min(ix0 + 1, sw - 1);
            float fx = sx - ix0;

            float v00 = src[iy0 * sw + ix0];
            float v10 = src[iy0 * sw + ix1];
            float v01 = src[iy1 * sw + ix0];
            float v11 = src[iy1 * sw + ix1];

            float top = v00 * (1.0f - fx) + v10 * fx;
            float bot = v01 * (1.0f - fx) + v11 * fx;
            dst[dy * dw + dx] = top * (1.0f - fy) + bot * fy;
        }
    }
}

void sdet_atrous_b3v_filter(const float* src, float* dst, int w, int h, int scale) {
    static const float b3v[5] = {1.0f/16.0f, 1.0f/4.0f, 3.0f/8.0f, 1.0f/4.0f, 1.0f/16.0f};
    int step = 1 << scale;

    std::vector<float> tmp((size_t)w * h);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double val = 0.0;
            for (int k = -2; k <= 2; k++) {
                int sx = x + k * step;
                val += (double)sdet_mirror_fetch(src, w, h, sx, y) * b3v[k + 2];
            }
            tmp[y * w + x] = (float)val;
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double val = 0.0;
            for (int k = -2; k <= 2; k++) {
                int sy = y + k * step;
                val += (double)sdet_mirror_fetch(tmp.data(), w, h, x, sy) * b3v[k + 2];
            }
            dst[y * w + x] = (float)val;
        }
    }
}

void sdet_iterative_sigma_clip(const float* data, int n,
                               float clip_sigma, int max_rounds,
                               float* out_med, float* out_mad) {
    if (n <= 0) {
        *out_med = 0.0f;
        *out_mad = 0.0f;
        return;
    }

    std::vector<float> buf(data, data + n);

    float prev_med = sdet_robust_median(buf.data(), (int)buf.size());
    float prev_mad = sdet_robust_mad(buf.data(), (int)buf.size());

    sdet_log(SDET_LOG_DEBUG, "SIGMA_CLIP", "Round 0: med=%.6f mad=%.6f pixels=%d", prev_med, prev_mad, (int)buf.size());

    for (int round = 1; round <= max_rounds; round++) {
        float threshold = prev_med + clip_sigma * prev_mad;

        std::vector<float> clipped;
        clipped.reserve(buf.size());
        int clipped_count = 0;
        for (size_t i = 0; i < buf.size(); i++) {
            if (buf[i] > threshold) {
                clipped_count++;
            } else {
                clipped.push_back(buf[i]);
            }
        }

        if ((int)clipped.size() < 100) {
            sdet_log(SDET_LOG_DEBUG, "SIGMA_CLIP", "Round %d: only %d pixels left (< 100), stopping", round, (int)clipped.size());
            *out_med = prev_med;
            *out_mad = prev_mad;
            return;
        }

        float med = sdet_robust_median(clipped.data(), (int)clipped.size());
        float mad = sdet_robust_mad(clipped.data(), (int)clipped.size());

        sdet_log(SDET_LOG_DEBUG, "SIGMA_CLIP", "Round %d: med=%.6f mad=%.6f clipped=%d/%d (%.1f%%)",
               round, med, mad, clipped_count, (int)buf.size(),
               100.0 * clipped_count / buf.size());

        float med_change = std::fabs(med - prev_med) / (std::fabs(prev_med) + 1e-30f);
        float mad_change = std::fabs(mad - prev_mad) / (std::fabs(prev_mad) + 1e-30f);

        if (med_change < 0.001f && mad_change < 0.001f) {
            sdet_log(SDET_LOG_DEBUG, "SIGMA_CLIP", "Converged at round %d: med_change=%.4f%% mad_change=%.4f%%",
                   round, med_change * 100.0f, mad_change * 100.0f);
            *out_med = med;
            *out_mad = mad;
            return;
        }

        prev_med = med;
        prev_mad = mad;
        buf.swap(clipped);
    }

    sdet_log(SDET_LOG_DEBUG, "SIGMA_CLIP", "Reached max_rounds=%d: med=%.6f mad=%.6f", max_rounds, prev_med, prev_mad);
    *out_med = prev_med;
    *out_mad = prev_mad;
}

void sdet_extract_lowfreq_atrous(const float* src, float* dst, int w, int h, int downsample_factor, int n_scales) {
    int dw = (w + downsample_factor - 1) / downsample_factor;
    int dh = (h + downsample_factor - 1) / downsample_factor;
    int dn = dw * dh;

    std::vector<float> small(dn), c1(dn), c2(dn);

    sdet_downsample(src, w, h, small.data(), dw, dh);

    sdet_atrous_b3v_filter(small.data(), c1.data(), dw, dh, 0);

    if (n_scales >= 2) {
        sdet_atrous_b3v_filter(c1.data(), c2.data(), dw, dh, 1);
        sdet_upsample_bilinear(c2.data(), dw, dh, dst, w, h);
    } else {
        sdet_upsample_bilinear(c1.data(), dw, dh, dst, w, h);
    }
}

void sdet_atrous_linear3_filter(const float* src, float* dst, int w, int h, int scale) {
    static const float lin3[3] = {0.25f, 0.5f, 0.25f};
    int step = 1 << scale;

    std::vector<float> tmp((size_t)w * h);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double val = 0.0;
            for (int k = -1; k <= 1; k++) {
                int sx = x + k * step;
                val += (double)sdet_mirror_fetch(src, w, h, sx, y) * lin3[k + 1];
            }
            tmp[y * w + x] = (float)val;
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double val = 0.0;
            for (int k = -1; k <= 1; k++) {
                int sy = y + k * step;
                val += (double)sdet_mirror_fetch(tmp.data(), w, h, x, sy) * lin3[k + 1];
            }
            dst[y * w + x] = (float)val;
        }
    }
}

void sdet_atrous_decompose_layer2(const float* src, float* dst, int w, int h, int n_layers) {
    size_t n = (size_t)w * h;

    std::vector<float> c0(src, src + n);
    std::vector<float> c1(n), c2(n), c3(n), c4(n);

    float* layers[5] = { c0.data(), c1.data(), c2.data(), c3.data(), c4.data() };

    sdet_atrous_linear3_filter(layers[0], layers[1], w, h, 0);
    sdet_log(SDET_LOG_DEBUG, "ATROUS", "Layer 0->1 done (scale=1)");

    sdet_atrous_linear3_filter(layers[1], layers[2], w, h, 1);
    sdet_log(SDET_LOG_DEBUG, "ATROUS", "Layer 1->2 done (scale=2)");

    if (n_layers >= 3) {
        sdet_atrous_linear3_filter(layers[2], layers[3], w, h, 2);
        sdet_log(SDET_LOG_DEBUG, "ATROUS", "Layer 2->3 done (scale=4)");
    }

    if (n_layers >= 4) {
        sdet_atrous_linear3_filter(layers[3], layers[4], w, h, 3);
        sdet_log(SDET_LOG_DEBUG, "ATROUS", "Layer 3->4 done (scale=8)");
    }

    for (size_t i = 0; i < n; i++) {
        dst[i] = layers[1][i] - layers[2][i];
    }

    sdet_log(SDET_LOG_INFO, "ATROUS", "ATrous Linear3 decompose: %d layers, output=w1-w2 (layer 2 detail)", n_layers);
}
