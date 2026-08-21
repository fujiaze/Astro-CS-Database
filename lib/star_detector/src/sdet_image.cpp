#include "sdet_image.h"
#include "sdet_log.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <omp.h>

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

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        int x = 0;
        for (; x < radius && x < w; x++) {
            double val = 0.0;
            for (int k = -radius; k <= radius; k++) {
                val += (double)sdet_mirror_fetch(src, w, h, x + k, y) * kernel[k + radius];
            }
            tmp[y * w + x] = (float)val;
        }
        for (; x + radius < w; x++) {
            double val = 0.0;
            for (int k = -radius; k <= radius; k++) {
                val += (double)src[y * w + (x + k)] * kernel[k + radius];
            }
            tmp[y * w + x] = (float)val;
        }
        for (; x < w; x++) {
            double val = 0.0;
            for (int k = -radius; k <= radius; k++) {
                val += (double)sdet_mirror_fetch(src, w, h, x + k, y) * kernel[k + radius];
            }
            tmp[y * w + x] = (float)val;
        }
    }

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        if (y < radius || y + radius >= h) {
            for (int x = 0; x < w; x++) {
                double val = 0.0;
                for (int k = -radius; k <= radius; k++) {
                    val += (double)sdet_mirror_fetch(tmp.data(), w, h, x, y + k) * kernel[k + radius];
                }
                dst[y * w + x] = (float)val;
            }
        } else {
            for (int x = 0; x < w; x++) {
                double val = 0.0;
                for (int k = -radius; k <= radius; k++) {
                    val += (double)tmp[(y + k) * w + x] * kernel[k + radius];
                }
                dst[y * w + x] = (float)val;
            }
        }
    }
}

void sdet_gaussian_blur_yvv(const float* src, float* dst, int w, int h, double sigma) {
    if (sigma <= 0.5 || w < 4 || h < 4) {
        sdet_gaussian_filter_separable(src, dst, w, h, sigma);
        return;
    }

    // === calculateYvVFactors (gauss.cc:139-171) ===
    double q;
    if (sigma < 2.5) {
        q = 3.97156 - 4.14554 * std::sqrt(1.0 - 0.26891 * sigma);
    } else {
        q = 0.98711 * sigma - 0.96330;
    }
    double b0 = 1.57825 + 2.44413 * q + 1.4281 * q * q + 0.422205 * q * q * q;
    double b1 = 2.44413 * q + 2.85619 * q * q + 1.26661 * q * q * q;
    double b2 = -1.4281 * q * q - 1.26661 * q * q * q;
    double b3 = 0.422205 * q * q * q;
    double B = 1.0 - (b1 + b2 + b3) / b0;
    b1 /= b0;
    b2 /= b0;
    b3 /= b0;

    // Triggs-Sdika 边界条件矩阵 M
    double M[3][3];
    M[0][0] = -b3 * b1 + 1.0 - b3 * b3 - b2;
    M[0][1] = (b3 + b1) * (b2 + b3 * b1);
    M[0][2] = b3 * (b1 + b3 * b2);
    M[1][0] = b1 + b3 * b2;
    M[1][1] = -(b2 - 1.0) * (b2 + b3 * b1);
    M[1][2] = -(b3 * b1 + b3 * b3 + b2 - 1.0) * b3;
    M[2][0] = b3 * b1 + b2 + b1 * b1 - b2 * b2;
    M[2][1] = b1 * b2 + b3 * b2 * b2 - b1 * b3 * b3 - b3 * b3 * b3 - b3 * b2 + b3;
    M[2][2] = b3 * (b1 + b3 * b2);

    // M 归一化 (gaussHorizontalSse/gaussVerticalSse: gauss.cc:603-607)
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            M[i][j] *= (1.0 + b2 + (b1 - b3) * b3);
            M[i][j] /= (1.0 + b1 - b2 + b3) * (1.0 - b1 - b2 - b3);
        }
    }

    const float Bf = (float)B, b1f = (float)b1, b2f = (float)b2, b3f = (float)b3;
    float Mf[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            Mf[i][j] = (float)M[i][j];

    std::vector<float> tmp((size_t)w * h);

    // === 水平方向前向+反向递归 (gaussHorizontalSse 标量: gauss.cc:683-707) ===
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        const float* srow = src + (size_t)y * w;
        float* trow = tmp.data() + (size_t)y * w;

        trow[0] = srow[0] * (Bf + b1f + b2f + b3f);
        trow[1] = Bf * srow[1] + b1f * trow[0] + srow[0] * (b2f + b3f);
        trow[2] = Bf * srow[2] + b1f * trow[1] + b2f * trow[0] + b3f * srow[0];
        for (int x = 3; x < w; x++) {
            trow[x] = Bf * srow[x] + b1f * trow[x - 1] + b2f * trow[x - 2] + b3f * trow[x - 3];
        }

        float sW = srow[w - 1];
        float temp2Wm1 = sW + Mf[0][0] * (trow[w - 1] - sW) + Mf[0][1] * (trow[w - 2] - sW) + Mf[0][2] * (trow[w - 3] - sW);
        float temp2W   = sW + Mf[1][0] * (trow[w - 1] - sW) + Mf[1][1] * (trow[w - 2] - sW) + Mf[1][2] * (trow[w - 3] - sW);
        float temp2Wp1 = sW + Mf[2][0] * (trow[w - 1] - sW) + Mf[2][1] * (trow[w - 2] - sW) + Mf[2][2] * (trow[w - 3] - sW);

        trow[w - 1] = temp2Wm1;
        trow[w - 2] = Bf * trow[w - 2] + b1f * trow[w - 1] + b2f * temp2W + b3f * temp2Wp1;
        trow[w - 3] = Bf * trow[w - 3] + b1f * trow[w - 2] + b2f * trow[w - 1] + b3f * temp2W;

        for (int x = w - 4; x >= 0; x--) {
            trow[x] = Bf * trow[x] + b1f * trow[x + 1] + b2f * trow[x + 2] + b3f * trow[x + 3];
        }
    }

    // === 垂直方向前向+反向递归 (gaussVerticalSse 标量: gauss.cc:872-897) ===
    #pragma omp parallel for schedule(static)
    for (int x = 0; x < w; x++) {
        dst[0 * w + x] = tmp[0 * w + x] * (Bf + b1f + b2f + b3f);
        dst[1 * w + x] = Bf * tmp[1 * w + x] + b1f * dst[0 * w + x] + tmp[0 * w + x] * (b2f + b3f);
        dst[2 * w + x] = Bf * tmp[2 * w + x] + b1f * dst[1 * w + x] + b2f * dst[0 * w + x] + b3f * tmp[0 * w + x];
        for (int y = 3; y < h; y++) {
            dst[(size_t)y * w + x] = Bf * tmp[(size_t)y * w + x] + b1f * dst[(size_t)(y - 1) * w + x] + b2f * dst[(size_t)(y - 2) * w + x] + b3f * dst[(size_t)(y - 3) * w + x];
        }

        float sH = tmp[(size_t)(h - 1) * w + x];
        float temp2Hm1 = sH + Mf[0][0] * (dst[(size_t)(h - 1) * w + x] - sH) + Mf[0][1] * (dst[(size_t)(h - 2) * w + x] - sH) + Mf[0][2] * (dst[(size_t)(h - 3) * w + x] - sH);
        float temp2H   = sH + Mf[1][0] * (dst[(size_t)(h - 1) * w + x] - sH) + Mf[1][1] * (dst[(size_t)(h - 2) * w + x] - sH) + Mf[1][2] * (dst[(size_t)(h - 3) * w + x] - sH);
        float temp2Hp1 = sH + Mf[2][0] * (dst[(size_t)(h - 1) * w + x] - sH) + Mf[2][1] * (dst[(size_t)(h - 2) * w + x] - sH) + Mf[2][2] * (dst[(size_t)(h - 3) * w + x] - sH);

        dst[(size_t)(h - 1) * w + x] = temp2Hm1;
        dst[(size_t)(h - 2) * w + x] = Bf * dst[(size_t)(h - 2) * w + x] + b1f * dst[(size_t)(h - 1) * w + x] + b2f * temp2H + b3f * temp2Hp1;
        dst[(size_t)(h - 3) * w + x] = Bf * dst[(size_t)(h - 3) * w + x] + b1f * dst[(size_t)(h - 2) * w + x] + b2f * dst[(size_t)(h - 1) * w + x] + b3f * temp2H;

        for (int y = h - 4; y >= 0; y--) {
            dst[(size_t)y * w + x] = Bf * dst[(size_t)y * w + x] + b1f * dst[(size_t)(y + 1) * w + x] + b2f * dst[(size_t)(y + 2) * w + x] + b3f * dst[(size_t)(y + 3) * w + x];
        }
    }
}

// ============================================================================
// sdet_gaussian_blur_yvv_d - FP64 变体 (, PREC-108): double 图像, 无 float 降级
// 与 float 版逻辑一致, 系数/缓冲/递归全部 double
// ============================================================================
void sdet_gaussian_blur_yvv_d(const double* src, double* dst, int w, int h, double sigma) {
    if (sigma <= 0.5 || w < 4 || h < 4) {
        // 退化路径: 直接复制 (浮点高斯在该参数下近似恒等)
        for (int64_t i = 0; i < (int64_t)w * h; i++) dst[i] = src[i];
        return;
    }

    double q;
    if (sigma < 2.5) {
        q = 3.97156 - 4.14554 * std::sqrt(1.0 - 0.26891 * sigma);
    } else {
        q = 0.98711 * sigma - 0.96330;
    }
    double b0 = 1.57825 + 2.44413 * q + 1.4281 * q * q + 0.422205 * q * q * q;
    double b1 = 2.44413 * q + 2.85619 * q * q + 1.26661 * q * q * q;
    double b2 = -1.4281 * q * q - 1.26661 * q * q * q;
    double b3 = 0.422205 * q * q * q;
    double B = 1.0 - (b1 + b2 + b3) / b0;
    b1 /= b0;
    b2 /= b0;
    b3 /= b0;

    double M[3][3];
    M[0][0] = -b3 * b1 + 1.0 - b3 * b3 - b2;
    M[0][1] = (b3 + b1) * (b2 + b3 * b1);
    M[0][2] = b3 * (b1 + b3 * b2);
    M[1][0] = b1 + b3 * b2;
    M[1][1] = -(b2 - 1.0) * (b2 + b3 * b1);
    M[1][2] = -(b3 * b1 + b3 * b3 + b2 - 1.0) * b3;
    M[2][0] = b3 * b1 + b2 + b1 * b1 - b2 * b2;
    M[2][1] = b1 * b2 + b3 * b2 * b2 - b1 * b3 * b3 - b3 * b3 * b3 - b3 * b2 + b3;
    M[2][2] = b3 * (b1 + b3 * b2);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            M[i][j] *= (1.0 + b2 + (b1 - b3) * b3);
            M[i][j] /= (1.0 + b1 - b2 + b3) * (1.0 - b1 - b2 - b3);
        }
    }

    std::vector<double> tmp((size_t)w * h);

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        const double* srow = src + (size_t)y * w;
        double* trow = tmp.data() + (size_t)y * w;

        trow[0] = srow[0] * (B + b1 + b2 + b3);
        trow[1] = B * srow[1] + b1 * trow[0] + srow[0] * (b2 + b3);
        trow[2] = B * srow[2] + b1 * trow[1] + b2 * trow[0] + b3 * srow[0];
        for (int x = 3; x < w; x++) {
            trow[x] = B * srow[x] + b1 * trow[x - 1] + b2 * trow[x - 2] + b3 * trow[x - 3];
        }

        double sW = srow[w - 1];
        double temp2Wm1 = sW + M[0][0] * (trow[w - 1] - sW) + M[0][1] * (trow[w - 2] - sW) + M[0][2] * (trow[w - 3] - sW);
        double temp2W   = sW + M[1][0] * (trow[w - 1] - sW) + M[1][1] * (trow[w - 2] - sW) + M[1][2] * (trow[w - 3] - sW);
        double temp2Wp1 = sW + M[2][0] * (trow[w - 1] - sW) + M[2][1] * (trow[w - 2] - sW) + M[2][2] * (trow[w - 3] - sW);

        trow[w - 1] = temp2Wm1;
        trow[w - 2] = B * trow[w - 2] + b1 * trow[w - 1] + b2 * temp2W + b3 * temp2Wp1;
        trow[w - 3] = B * trow[w - 3] + b1 * trow[w - 2] + b2 * trow[w - 1] + b3 * temp2W;

        for (int x = w - 4; x >= 0; x--) {
            trow[x] = B * trow[x] + b1 * trow[x + 1] + b2 * trow[x + 2] + b3 * trow[x + 3];
        }
    }

    #pragma omp parallel for schedule(static)
    for (int x = 0; x < w; x++) {
        dst[0 * w + x] = tmp[0 * w + x] * (B + b1 + b2 + b3);
        dst[1 * w + x] = B * tmp[1 * w + x] + b1 * dst[0 * w + x] + tmp[0 * w + x] * (b2 + b3);
        dst[2 * w + x] = B * tmp[2 * w + x] + b1 * dst[1 * w + x] + b2 * dst[0 * w + x] + b3 * tmp[0 * w + x];
        for (int y = 3; y < h; y++) {
            dst[(size_t)y * w + x] = B * tmp[(size_t)y * w + x] + b1 * dst[(size_t)(y - 1) * w + x] + b2 * dst[(size_t)(y - 2) * w + x] + b3 * dst[(size_t)(y - 3) * w + x];
        }

        double sH = tmp[(size_t)(h - 1) * w + x];
        double temp2Hm1 = sH + M[0][0] * (dst[(size_t)(h - 1) * w + x] - sH) + M[0][1] * (dst[(size_t)(h - 2) * w + x] - sH) + M[0][2] * (dst[(size_t)(h - 3) * w + x] - sH);
        double temp2H   = sH + M[1][0] * (dst[(size_t)(h - 1) * w + x] - sH) + M[1][1] * (dst[(size_t)(h - 2) * w + x] - sH) + M[1][2] * (dst[(size_t)(h - 3) * w + x] - sH);
        double temp2Hp1 = sH + M[2][0] * (dst[(size_t)(h - 1) * w + x] - sH) + M[2][1] * (dst[(size_t)(h - 2) * w + x] - sH) + M[2][2] * (dst[(size_t)(h - 3) * w + x] - sH);

        dst[(size_t)(h - 1) * w + x] = temp2Hm1;
        dst[(size_t)(h - 2) * w + x] = B * dst[(size_t)(h - 2) * w + x] + b1 * dst[(size_t)(h - 1) * w + x] + b2 * temp2H + b3 * temp2Hp1;
        dst[(size_t)(h - 3) * w + x] = B * dst[(size_t)(h - 3) * w + x] + b1 * dst[(size_t)(h - 2) * w + x] + b2 * dst[(size_t)(h - 1) * w + x] + b3 * temp2H;

        for (int y = h - 4; y >= 0; y--) {
            dst[(size_t)y * w + x] = B * dst[(size_t)y * w + x] + b1 * dst[(size_t)(y + 1) * w + x] + b2 * dst[(size_t)(y + 2) * w + x] + b3 * dst[(size_t)(y + 3) * w + x];
        }
    }
}

void sdet_median_filter_3x3(const float* src, float* dst, int w, int h) {
    #pragma omp parallel for schedule(static)
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
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        std::vector<float> buf(25);
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
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        std::vector<float> buf(n);
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
    #pragma omp parallel for schedule(static)
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

    #pragma omp parallel for schedule(static)
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
    size_t n = (size_t)w * h;

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

    std::vector<float> dilated(n);

    #pragma omp parallel for schedule(static)
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

    for (size_t i = 0; i < n; i++) {
        dst[i] = (dilated[i] < src[i] && src[i] < limit) ? 1.0f : 0.0f;
    }
}

float sdet_robust_median(const float* data, int n) {
    if (n <= 0) return 0.0f;
    std::vector<float> tmp(data, data + n);
    std::nth_element(tmp.begin(), tmp.begin() + n / 2, tmp.end());
    if (n % 2 == 0) {
        float a;
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
    return mad * 1.4826f; // MAD→σ 1.4826 (Gaussian, NOISE_MODEL.md / STAR_PSF_ALGORITHMS.md)
}

// ============================================================================
// sdet_robust_median_d / sdet_robust_mad_d - FP64 变体 (, PREC-108)
// ============================================================================
double sdet_robust_median_d(const double* data, int n) {
    if (n <= 0) return 0.0;
    std::vector<double> tmp(data, data + n);
    std::nth_element(tmp.begin(), tmp.begin() + n / 2, tmp.end());
    if (n % 2 == 0) {
        double a;
        double b = tmp[n / 2];
        std::nth_element(tmp.begin(), tmp.begin() + n / 2 - 1, tmp.begin() + n / 2);
        a = tmp[n / 2 - 1];
        return (a + b) * 0.5;
    }
    return tmp[n / 2];
}

double sdet_robust_mad_d(const double* data, int n) {
    if (n <= 0) return 0.0;
    double med = sdet_robust_median_d(data, n);
    std::vector<double> deviations(n);
    for (int i = 0; i < n; i++) {
        deviations[i] = std::fabs(data[i] - med);
    }
    double mad = sdet_robust_median_d(deviations.data(), n);
    return mad * 1.4826; // MAD→σ 1.4826 (Gaussian, NOISE_MODEL.md)
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

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        int x = 0;
        for (; x < 2 * step && x < w; x++) {
            double val = 0.0;
            for (int k = -2; k <= 2; k++) {
                int sx = x + k * step;
                val += (double)sdet_mirror_fetch(src, w, h, sx, y) * b3v[k + 2];
            }
            tmp[y * w + x] = (float)val;
        }
        for (; x + 2 * step < w; x++) {
            double val = 0.0;
            for (int k = -2; k <= 2; k++) {
                int sx = x + k * step;
                val += (double)src[y * w + sx] * b3v[k + 2];
            }
            tmp[y * w + x] = (float)val;
        }
        for (; x < w; x++) {
            double val = 0.0;
            for (int k = -2; k <= 2; k++) {
                int sx = x + k * step;
                val += (double)sdet_mirror_fetch(src, w, h, sx, y) * b3v[k + 2];
            }
            tmp[y * w + x] = (float)val;
        }
    }

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        if (y < 2 * step || y + 2 * step >= h) {
            for (int x = 0; x < w; x++) {
                double val = 0.0;
                for (int k = -2; k <= 2; k++) {
                    int sy = y + k * step;
                    val += (double)sdet_mirror_fetch(tmp.data(), w, h, x, sy) * b3v[k + 2];
                }
                dst[y * w + x] = (float)val;
            }
        } else {
            for (int x = 0; x < w; x++) {
                double val = 0.0;
                for (int k = -2; k <= 2; k++) {
                    int sy = y + k * step;
                    val += (double)tmp[sy * w + x] * b3v[k + 2];
                }
                dst[y * w + x] = (float)val;
            }
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

    for (int round = 0; round < max_rounds; round++) {
        float med = sdet_robust_median(buf.data(), (int)buf.size());
        float mad = sdet_robust_mad(buf.data(), (int)buf.size());

        sdet_log(SDET_LOG_DEBUG, "SIGMA_CLIP", "Round %d: med=%.6f mad=%.6f pixels=%d", round, med, mad, (int)buf.size());

        float threshold = med + clip_sigma * mad;
        std::vector<float> clipped;
        clipped.reserve(buf.size());
        for (size_t i = 0; i < buf.size(); i++) {
            if (buf[i] <= threshold) clipped.push_back(buf[i]);
        }
        if ((int)clipped.size() < 10) break;
        buf.swap(clipped);
    }

    *out_med = sdet_robust_median(buf.data(), (int)buf.size());
    *out_mad = sdet_robust_mad(buf.data(), (int)buf.size());
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

#if 0
void sdet_atrous_linear3_filter(const float* src, float* dst, int w, int h, int scale) {
    static const float lin3[3] = {0.25f, 0.5f, 0.25f};
    int step = 1 << scale;

    std::vector<float> tmp((size_t)w * h);

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        int x = 0;
        for (; x < step && x < w; x++) {
            double val = 0.0;
            for (int k = -1; k <= 1; k++) {
                int sx = x + k * step;
                val += (double)sdet_mirror_fetch(src, w, h, sx, y) * lin3[k + 1];
            }
            tmp[y * w + x] = (float)val;
        }
        for (; x + step < w; x++) {
            double val = 0.0;
            for (int k = -1; k <= 1; k++) {
                int sx = x + k * step;
                val += (double)src[y * w + sx] * lin3[k + 1];
            }
            tmp[y * w + x] = (float)val;
        }
        for (; x < w; x++) {
            double val = 0.0;
            for (int k = -1; k <= 1; k++) {
                int sx = x + k * step;
                val += (double)sdet_mirror_fetch(src, w, h, sx, y) * lin3[k + 1];
            }
            tmp[y * w + x] = (float)val;
        }
    }

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        if (y < step || y + step >= h) {
            for (int x = 0; x < w; x++) {
                double val = 0.0;
                for (int k = -1; k <= 1; k++) {
                    int sy = y + k * step;
                    val += (double)sdet_mirror_fetch(tmp.data(), w, h, x, sy) * lin3[k + 1];
                }
                dst[y * w + x] = (float)val;
            }
        } else {
            for (int x = 0; x < w; x++) {
                double val = 0.0;
                for (int k = -1; k <= 1; k++) {
                    int sy = y + k * step;
                    val += (double)tmp[sy * w + x] * lin3[k + 1];
                }
                dst[y * w + x] = (float)val;
            }
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
#endif

static void sdet_sigma_clip_med_of_rounds(const float* data, int n,
                                          float clip_sigma, int max_rounds,
                                          float* out_med, float* out_mad) {
    if (n <= 0) {
        *out_med = 0.0f;
        *out_mad = 0.0f;
        return;
    }

    std::vector<float> buf(data, data + n);
    std::vector<float> meds, mads;

    for (int round = 0; round < max_rounds; round++) {
        float med = sdet_robust_median(buf.data(), (int)buf.size());
        float mad = sdet_robust_mad(buf.data(), (int)buf.size());
        meds.push_back(med);
        mads.push_back(mad);

        float threshold = med + clip_sigma * mad;
        std::vector<float> clipped;
        clipped.reserve(buf.size());
        for (size_t i = 0; i < buf.size(); i++) {
            if (buf[i] <= threshold) clipped.push_back(buf[i]);
        }
        if ((int)clipped.size() < 10) break;
        buf.swap(clipped);
    }

    *out_med = sdet_robust_median(meds.data(), (int)meds.size());
    *out_mad = sdet_robust_median(mads.data(), (int)mads.size());
}

void sdet_dynamic_regional_background(const float* src, float* out_detail, int w, int h,
                                       int block_size, int block_overlap,
                                       int sub_block_size, int sub_block_overlap,
                                       float clip_sigma, int max_rounds,
                                       int fill_radius) {
    auto t0 = omp_get_wtime();
    size_t n = (size_t)w * h;

    sdet_log(SDET_LOG_INFO, "DYN_BG", "Dynamic regional background: %dx%d block=%d overlap=%d sub=%d sub_overlap=%d sigma=%.2f rounds=%d fill=%d",
             w, h, block_size, block_overlap, sub_block_size, sub_block_overlap, clip_sigma, max_rounds, fill_radius);

    float* med_map = new float[n]();
    float* mad_map = new float[n]();
    float* wt = new float[n]();

    int step = block_size - block_overlap;
    int n_bx = std::max(1, (w - block_overlap + step - 1) / step);
    int n_by = std::max(1, (h - block_overlap + step - 1) / step);
    int n_blocks = n_bx * n_by;

    // 预计算所有块的med/mad和类型
    struct BlockInfo { int x0, y0, x1, y1; float med, mad; bool is_normal; };
    std::vector<BlockInfo> blocks(n_blocks);
    #pragma omp parallel for schedule(dynamic) collapse(2)
    for (int by = 0; by < n_by; by++) {
        for (int bx = 0; bx < n_bx; bx++) {
            int x0 = bx * step;
            int y0 = by * step;
            int x1 = std::min(x0 + block_size, w);
            int y1 = std::min(y0 + block_size, h);

            int bn = (y1 - y0) * (x1 - x0);
            std::vector<float> block(bn);
            int idx = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++)
                    block[idx++] = src[yy * w + xx];

            float med, mad;
            sdet_sigma_clip_med_of_rounds(block.data(), bn, clip_sigma, max_rounds, &med, &mad);

            double mean_val = 0;
            for (int i = 0; i < bn; i++) mean_val += block[i];
            mean_val /= bn;

            double ratio = mean_val / (med + 1e-30f);
            bool is_normal = (ratio >= 0.707) && (ratio <= 1.0 / 0.707);

            int bi = by * n_bx + bx;
            blocks[bi] = {x0, y0, x1, y1, med, mad, is_normal};
        }
    }

    // 异常块的子块精细化
    struct SubBlockInfo { int x0, y0, x1, y1; float med, mad; };
    std::vector<std::vector<SubBlockInfo>> sub_blocks(n_blocks);

    for (int bi = 0; bi < n_blocks; bi++) {
        if (blocks[bi].is_normal) continue;
        int x0 = blocks[bi].x0, y0 = blocks[bi].y0;
        int x1 = blocks[bi].x1, y1 = blocks[bi].y1;

        int sub_step = sub_block_size - sub_block_overlap;
        int n_sbx = std::max(1, (x1 - x0 - sub_block_overlap + sub_step - 1) / sub_step);
        int n_sby = std::max(1, (y1 - y0 - sub_block_overlap + sub_step - 1) / sub_step);

        sub_blocks[bi].resize((std::size_t)n_sbx * (std::size_t)n_sby);
        for (int sby = 0; sby < n_sby; sby++) {
            for (int sbx = 0; sbx < n_sbx; sbx++) {
                int sx0 = x0 + sbx * sub_step;
                int sy0 = y0 + sby * sub_step;
                int sx1 = std::min(sx0 + sub_block_size, x1);
                int sy1 = std::min(sy0 + sub_block_size, y1);

                int sbn = (sy1 - sy0) * (sx1 - sx0);
                std::vector<float> sub_block(sbn);
                int si = 0;
                for (int yy = sy0; yy < sy1; yy++)
                    for (int xx = sx0; xx < sx1; xx++)
                        sub_block[si++] = src[yy * w + xx];

                float s_med, s_mad;
                sdet_sigma_clip_med_of_rounds(sub_block.data(), sbn, clip_sigma, max_rounds, &s_med, &s_mad);

                sub_blocks[bi][sby * n_sbx + sbx] = {sx0, sy0, sx1, sy1, s_med, s_mad};
            }
        }
    }

    // 写入med_map/mad_map/wt
    for (int bi = 0; bi < n_blocks; bi++) {
        if (blocks[bi].is_normal) {
            for (int yy = blocks[bi].y0; yy < blocks[bi].y1; yy++)
                for (int xx = blocks[bi].x0; xx < blocks[bi].x1; xx++) {
                    med_map[yy * w + xx] += blocks[bi].med;
                    mad_map[yy * w + xx] += blocks[bi].mad;
                    wt[yy * w + xx] += 1.0f;
                }
        } else {
            for (const auto& sb : sub_blocks[bi]) {
                for (int yy = sb.y0; yy < sb.y1; yy++)
                    for (int xx = sb.x0; xx < sb.x1; xx++) {
                        med_map[yy * w + xx] += sb.med;
                        mad_map[yy * w + xx] += sb.mad;
                        wt[yy * w + xx] += 1.0f;
                    }
            }
        }
    }

    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < n; i++) {
        if (wt[i] > 0) {
            med_map[i] /= wt[i];
            mad_map[i] /= wt[i];
        } else {
            mad_map[i] = 1.0f;
        }
    }

    uint8_t* star_mask = new uint8_t[n]();
    size_t mask_count = 0;
    #pragma omp parallel for schedule(static) reduction(+:mask_count)
    for (size_t i = 0; i < n; i++) {
        if (src[i] > med_map[i] + clip_sigma * mad_map[i]) {
            star_mask[i] = 1;
            mask_count++;
        }
    }

    sdet_log(SDET_LOG_INFO, "DYN_BG", "Star mask: %zu / %zu pixels (%.2f%%)", mask_count, n, 100.0 * mask_count / n);

    float* background = new float[n];
    memcpy(background, src, n * sizeof(float));

    if (mask_count > 0) {
        double* sum_img = new double[n]();
        double* cnt_img = new double[n]();

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                size_t idx = y * w + x;
                double val = star_mask[idx] ? 0.0 : (double)src[idx];
                double cnt = star_mask[idx] ? 0.0 : 1.0;
                double sum_left = (x > 0) ? sum_img[idx - 1] : 0.0;
                double cnt_left = (x > 0) ? cnt_img[idx - 1] : 0.0;
                double sum_up = (y > 0) ? sum_img[idx - w] : 0.0;
                double cnt_up = (y > 0) ? cnt_img[idx - w] : 0.0;
                double sum_diag = (x > 0 && y > 0) ? sum_img[idx - w - 1] : 0.0;
                double cnt_diag = (x > 0 && y > 0) ? cnt_img[idx - w - 1] : 0.0;
                sum_img[idx] = val + sum_left + sum_up - sum_diag;
                cnt_img[idx] = cnt + cnt_left + cnt_up - cnt_diag;
            }
        }

        int r = fill_radius;
        #pragma omp parallel for schedule(static)
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                size_t idx = y * w + x;
                if (!star_mask[idx]) continue;

                int x0c = std::max(0, x - r);
                int y0c = std::max(0, y - r);
                int x1c = std::min(w, x + r + 1);
                int y1c = std::min(h, y + r + 1);

                auto rect_sum = [&](int rx0, int ry0, int rx1, int ry1) -> double {
                    double s = sum_img[(ry1 - 1) * w + (rx1 - 1)];
                    if (rx0 > 0) s -= sum_img[(ry1 - 1) * w + (rx0 - 1)];
                    if (ry0 > 0) s -= sum_img[(rx1 - 1) + (ry0 - 1) * w];
                    if (rx0 > 0 && ry0 > 0) s += sum_img[(rx0 - 1) + (ry0 - 1) * w];
                    return s;
                };
                auto rect_cnt = [&](int rx0, int ry0, int rx1, int ry1) -> double {
                    double c = cnt_img[(ry1 - 1) * w + (rx1 - 1)];
                    if (rx0 > 0) c -= cnt_img[(ry1 - 1) * w + (rx0 - 1)];
                    if (ry0 > 0) c -= cnt_img[(rx1 - 1) + (ry0 - 1) * w];
                    if (rx0 > 0 && ry0 > 0) c += cnt_img[(rx0 - 1) + (ry0 - 1) * w];
                    return c;
                };

                double s = rect_sum(x0c, y0c, x1c, y1c);
                double c = rect_cnt(x0c, y0c, x1c, y1c);

                if (c > 0.5) {
                    background[idx] = (float)(s / c);
                } else {
                    background[idx] = med_map[idx];
                }
            }
        }

        delete[] sum_img;
        delete[] cnt_img;
    }

    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < n; i++) {
        out_detail[i] = src[i] - background[i];
    }

    delete[] med_map;
    delete[] mad_map;
    delete[] wt;
    delete[] star_mask;
    delete[] background;

    auto t1 = omp_get_wtime();
    sdet_log(SDET_LOG_INFO, "DYN_BG", "Done in %.2f s", t1 - t0);
}
