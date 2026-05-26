#include "sdet_detector.h"
#include "sdet_log.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <queue>
#include <unordered_map>

static const float SDET_PI_OVER_4 = 0.7853981633974483f;

void sdet_get_structure_map(StarDetectorInternal* sd, const float* image, int w, int h, float* out_map) {
    auto t0 = std::chrono::high_resolution_clock::now();
    int n = w * h;
    const auto& P = sd->params;

    std::vector<float> buf1(n), smap(n);

    const float* s1 = image;
    if (P.hotPixelFilterRadius > 0) {
        sdet_median_filter(image, buf1.data(), w, h, P.hotPixelFilterRadius);
        s1 = buf1.data();
        auto t1 = std::chrono::high_resolution_clock::now();
        sdet_log(SDET_LOG_DEBUG, "DETECTOR", "Hot pixel filter (radius=%d): %.1f ms",
               P.hotPixelFilterRadius,
               std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    sdet_atrous_decompose_layer2(s1, smap.data(), w, h, 4);

    delete[] sd->raw_detail;
    sd->raw_detail = new float[n];
    std::memcpy(sd->raw_detail, smap.data(), n * sizeof(float));

    float max_val = -1e30f;
    for (int i = 0; i < n; i++) {
        if (smap[i] > max_val) max_val = smap[i];
    }
    for (int i = 0; i < n; i++) {
        if (smap[i] < 0.0f) smap[i] = 0.0f;
    }
    if (max_val <= 0.0f) max_val = 1.0f;
    sdet_truncate_and_rescale(smap.data(), n, 0.0f, max_val);

    {
        auto t3 = std::chrono::high_resolution_clock::now();
        sdet_log(SDET_LOG_DEBUG, "DETECTOR", "ATrous Linear3 layer2 detail: %.1f ms",
               std::chrono::duration<double, std::milli>(t3 - t0).count());
    }

    sdet_dilate_box(smap.data(), buf1.data(), w, h, 1);
    {
        auto t4 = std::chrono::high_resolution_clock::now();
        sdet_log(SDET_LOG_DEBUG, "DETECTOR", "Dilate (3x3): %.1f ms",
               std::chrono::duration<double, std::milli>(t4 - t0).count());
    }

    std::memcpy(out_map, smap.data(), n * sizeof(float));

    auto tf = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_INFO, "DETECTOR", "Structure map complete: %.1f ms",
           std::chrono::duration<double, std::milli>(tf - t0).count());
}

void sdet_get_local_maxima_map(const float* map, float* out_maxima, int w, int h, float upper_limit) {
    sdet_local_maxima_map(map, out_maxima, w, h, 1, upper_limit);

    int total_maxima = 0;
    int n = w * h;
    for (int i = 0; i < n; i++) {
        if (out_maxima[i] > 0.0f) total_maxima++;
    }

    sdet_log(SDET_LOG_INFO, "DETECTOR", "Local maxima: %d points", total_maxima);
}

int sdet_find_connected_components(const float* binary_map, int w, int h,
                                    ConnectedComponent** out_components, int* out_count) {
    auto t0 = std::chrono::high_resolution_clock::now();

    std::vector<uint8_t> visited(w * h, 0);
    std::vector<ConnectedComponent> components;

    struct Span { int y, xleft, xright; };

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            if (binary_map[idx] == 0.0f || visited[idx]) continue;

            ConnectedComponent cc;
            cc.x0 = w; cc.y0 = h; cc.x1 = 0; cc.y1 = 0;
            cc.count = 0;

            std::vector<Span> stack;
            int xleft = x;
            while (xleft > 0 && binary_map[y * w + xleft - 1] != 0.0f && !visited[y * w + xleft - 1])
                xleft--;
            int xright = x;
            while (xright < w - 1 && binary_map[y * w + xright + 1] != 0.0f && !visited[y * w + xright + 1])
                xright++;
            stack.push_back({y, xleft, xright});

            while (!stack.empty()) {
                Span s = stack.back();
                stack.pop_back();

                for (int xi = s.xleft; xi <= s.xright; xi++) {
                    int si = s.y * w + xi;
                    if (visited[si]) continue;
                    visited[si] = 1;
                    cc.px.push_back(xi);
                    cc.py.push_back(s.y);
                    cc.count++;
                    if (xi < cc.x0) cc.x0 = xi;
                    if (s.y < cc.y0) cc.y0 = s.y;
                    if (xi > cc.x1) cc.x1 = xi;
                    if (s.y > cc.y1) cc.y1 = s.y;
                }

                for (int dy : {-1, 1}) {
                    int ny = s.y + dy;
                    if (ny < 0 || ny >= h) continue;
                    int xi = s.xleft;
                    while (xi <= s.xright) {
                        while (xi <= s.xright && (binary_map[ny * w + xi] == 0.0f || visited[ny * w + xi]))
                            xi++;
                        if (xi > s.xright) break;

                        int nxleft = xi;
                        while (nxleft > 0 && binary_map[ny * w + nxleft - 1] != 0.0f && !visited[ny * w + nxleft - 1])
                            nxleft--;
                        int nxright = xi;
                        while (nxright < w - 1 && binary_map[ny * w + nxright + 1] != 0.0f && !visited[ny * w + nxright + 1])
                            nxright++;

                        stack.push_back({ny, nxleft, nxright});
                        xi = nxright + 1;
                    }
                }
            }

            components.push_back(std::move(cc));
        }
    }

    *out_count = (int)components.size();
    if (*out_count == 0) {
        *out_components = nullptr;
        return 0;
    }

    *out_components = (ConnectedComponent*)malloc((size_t)*out_count * sizeof(ConnectedComponent));
    for (int i = 0; i < *out_count; i++) {
        new (*out_components + i) ConnectedComponent(std::move(components[i]));
    }

    auto tf = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_INFO, "DETECTOR", "Connected components: %d found (%.1f ms)",
           *out_count, std::chrono::duration<double, std::milli>(tf - t0).count());

    return *out_count;
}

void sdet_get_star_parameters(const float* image, int w, int h, ConnectedComponent* cc, DetectedStarInternal* star) {
    memset(star, 0, sizeof(DetectedStarInternal));

    if (cc->count <= 0 || cc->x0 < 0 || cc->y0 < 0 || cc->x1 > w || cc->y1 > h) return;

    int rw = cc->x1 - cc->x0;
    int rh = cc->y1 - cc->y0;
    if (rw <= 0 || rh <= 0) return;

    float bkg = 0.0f, sigma = 0.0f;
    {
        int delta = 4;
        int ox0 = std::max(0, cc->x0 - delta);
        int oy0 = std::max(0, cc->y0 - delta);
        int ox1 = std::min(w, cc->x1 + delta);
        int oy1 = std::min(h, cc->y1 + delta);

        std::vector<float> annulus;
        annulus.reserve((ox1 - ox0) * (oy1 - oy0));
        for (int yy = oy0; yy < oy1; yy++) {
            for (int xx = ox0; xx < ox1; xx++) {
                if (xx >= cc->x0 && xx < cc->x1 && yy >= cc->y0 && yy < cc->y1) continue;
                annulus.push_back(image[yy * w + xx]);
            }
        }

        if (annulus.size() >= 4) {
            bkg = sdet_robust_median(annulus.data(), (int)annulus.size());
            sigma = sdet_robust_mad(annulus.data(), (int)annulus.size());
        } else {
            std::vector<float> vals;
            for (int yy = oy0; yy < oy1; yy++) {
                for (int xx = ox0; xx < ox1; xx++) {
                    vals.push_back(image[yy * w + xx]);
                }
            }
            if (vals.size() >= 4) {
                bkg = sdet_robust_median(vals.data(), (int)vals.size());
                sigma = sdet_robust_mad(vals.data(), (int)vals.size());
            }
        }
    }

    if (sigma <= 0.0f) sigma = 1e-6f;

    double sum_wx = 0.0, sum_wy = 0.0, sum_w = 0.0;
    float threshold = bkg + 1.5f * sigma;
    for (int yy = cc->y0; yy < cc->y1; yy++) {
        for (int xx = cc->x0; xx < cc->x1; xx++) {
            float val = image[yy * w + xx];
            if (val > threshold) {
                float w_val = val - bkg;
                sum_wx += (double)xx * (double)w_val;
                sum_wy += (double)yy * (double)w_val;
                sum_w += (double)w_val;
            }
        }
    }

    double cx, cy;
    if (sum_w > 0.0) {
        cx = sum_wx / sum_w;
        cy = sum_wy / sum_w;
    } else {
        cx = (cc->x0 + cc->x1) * 0.5;
        cy = (cc->y0 + cc->y1) * 0.5;
    }

    float peak = 0.0f;
    {
        std::vector<float> above_bkg;
        for (int yy = cc->y0; yy < cc->y1; yy++) {
            for (int xx = cc->x0; xx < cc->x1; xx++) {
                float val = image[yy * w + xx];
                if (val > bkg) above_bkg.push_back(val);
            }
        }
        if (!above_bkg.empty()) {
            std::sort(above_bkg.rbegin(), above_bkg.rend());
            int n_top = std::min(5, (int)above_bkg.size());
            float sum_top = 0.0f;
            for (int i = 0; i < n_top; i++) sum_top += above_bkg[i];
            peak = sum_top / n_top;
        }
    }

    float kurtosis = 0.0f;
    {
        double m2 = 0.0, m4 = 0.0;
        int cnt = 0;
        for (int yy = cc->y0; yy < cc->y1; yy++) {
            for (int xx = cc->x0; xx < cc->x1; xx++) {
                float val = image[yy * w + xx] - bkg;
                m2 += (double)val * (double)val;
                m4 += (double)val * (double)val * (double)val * (double)val;
                cnt++;
            }
        }
        if (cnt > 0 && m2 > 0.0) {
            m2 /= cnt;
            m4 /= cnt;
            kurtosis = (float)(m4 / (m2 * m2));
        }
    }

    float flux = 0.0f;
    for (int j = 0; j < cc->count; j++) {
        float val = image[cc->py[j] * w + cc->px[j]] - bkg;
        if (val > 0.0f) flux += val;
    }

    float snr = (sigma > 0.0f) ? (peak - bkg) / sigma : 0.0f;

    star->cx = cx;
    star->cy = cy;
    star->peak = peak;
    star->background = bkg;
    star->sigma = sigma;
    star->flux = flux;
    star->area = cc->count;
    star->kurtosis = kurtosis;
    star->snr = snr;
    star->rect_x0 = cc->x0;
    star->rect_y0 = cc->y0;
    star->rect_x1 = cc->x1;
    star->rect_y1 = cc->y1;
}

int sdet_apply_filters(DetectedStarInternal* stars, int count, int w, int h) {
    std::vector<uint8_t> deleted(count, 0);

    int f_size = 0, f_border = 0;

    for (int i = 0; i < count; i++) {
        DetectedStarInternal& s = stars[i];

        if ((s.rect_x1 - s.rect_x0) <= 1 || (s.rect_y1 - s.rect_y0) <= 1) {
            deleted[i] = 1; f_size++; continue;
        }

        if (s.rect_x0 <= 0 || s.rect_y0 <= 0 || s.rect_x1 >= w || s.rect_y1 >= h) {
            deleted[i] = 1; f_border++; continue;
        }
    }

    int j = 0;
    for (int i = 0; i < count; i++) {
        if (!deleted[i]) {
            if (j != i) stars[j] = stars[i];
            j++;
        }
    }

    sdet_log(SDET_LOG_INFO, "DETECTOR", "Filters: %d/%d passed (size=%d border=%d)",
           j, count, f_size, f_border);
    return j;
}

int sdet_compute_auto_min_structure_size(DetectedStarInternal* stars, int count) {
    if (count == 0) return 1;

    std::vector<float> areas;
    areas.reserve(count);
    for (int i = 0; i < count; i++) {
        if (stars[i].area > 0) areas.push_back((float)stars[i].area);
    }
    if (areas.empty()) return 1;

    std::sort(areas.begin(), areas.end());
    if (areas.size() == 1) return (int)areas[0];

    std::vector<float> diffs;
    diffs.reserve(areas.size() - 1);
    for (size_t i = 1; i < areas.size(); i++) {
        diffs.push_back(areas[i] - areas[i - 1]);
    }

    float bandwidth = sdet_robust_median(diffs.data(), (int)diffs.size());
    if (bandwidth < 1.0f) bandwidth = 1.0f;

    std::vector<std::vector<float>> clusters;
    clusters.push_back({areas[0]});
    for (size_t i = 1; i < areas.size(); i++) {
        if (areas[i] - areas[i - 1] > bandwidth) {
            clusters.push_back({});
        }
        clusters.back().push_back(areas[i]);
    }

    for (auto& c : clusters) {
        if ((int)c.size() >= 3) {
            return (int)c.front();
        }
    }

    if (clusters.size() > 1) {
        return (int)clusters[1].front();
    }

    return (int)areas.front();
}

int sdet_deduplicate_stars(DetectedStarInternal* stars, int count, int min_size) {
    if (count == 0) return 0;

    const int grid_size = 2;

    struct GridKey {
        int gx, gy;
        bool operator==(const GridKey& o) const { return gx == o.gx && gy == o.gy; }
    };
    struct GridKeyHash {
        size_t operator()(const GridKey& k) const {
            return (size_t)k.gx * 1000003ULL + (size_t)k.gy;
        }
    };

    std::unordered_map<GridKey, std::vector<int>, GridKeyHash> grid;
    for (int i = 0; i < count; i++) {
        int gx = (int)stars[i].cx / grid_size;
        int gy = (int)stars[i].cy / grid_size;
        grid[{gx, gy}].push_back(i);
    }

    std::vector<int> order(count);
    for (int i = 0; i < count; i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return stars[a].flux > stars[b].flux;
    });

    std::vector<uint8_t> deleted(count, 0);

    for (int idx : order) {
        if (deleted[idx]) continue;

        int gx = (int)stars[idx].cx / grid_size;
        int gy = (int)stars[idx].cy / grid_size;

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                auto it = grid.find({gx + dx, gy + dy});
                if (it == grid.end()) continue;

                for (int j : it->second) {
                    if (j == idx || deleted[j]) continue;
                    double ddx = stars[idx].cx - stars[j].cx;
                    double ddy = stars[idx].cy - stars[j].cy;
                    double dist2 = ddx * ddx + ddy * ddy;
                    if (dist2 <= 1.0) {
                        deleted[j] = 1;
                    }
                }
            }
        }
    }

    for (int i = 0; i < count; i++) {
        if (!deleted[i] && stars[i].area < min_size) {
            deleted[i] = 1;
        }
    }

    int j = 0;
    for (int i = 0; i < count; i++) {
        if (!deleted[i]) {
            if (j != i) stars[j] = stars[i];
            j++;
        }
    }

    sdet_log(SDET_LOG_INFO, "DETECTOR", "Deduplication: %d/%d stars remain", j, count);
    return j;
}
