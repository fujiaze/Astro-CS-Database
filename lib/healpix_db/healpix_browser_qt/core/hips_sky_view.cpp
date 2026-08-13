// ============================================================================
// hips_sky_view.cpp - V9 HiPS 2D 天空视图核心实现
// ============================================================================

#include "hips_sky_view.h"

#include "hips_browser_backend.h"
#include "healpix/healpix_core.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <omp.h>
#include <set>

namespace {

using Clock = std::chrono::high_resolution_clock;
constexpr double kPi = 3.14159265358979323846;
constexpr std::uint32_t kBackground = 0xFF14181F;  // 空区背景（深蓝灰）
constexpr std::uint32_t kBlack = 0xFF000000;
constexpr int kTileShift = 9;
constexpr std::uint64_t kTileMask = (1ULL << 18) - 1;

inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

}  // namespace

HipsSkyView::HipsSkyView() = default;

void HipsSkyView::set_backend(HipsBrowserBackend* backend) {
    bk_ = backend;
    cache_.clear();
    metrics_ = Metrics{};
    stats_ = Stats{};
}

void HipsSkyView::set_view(double center_ra, double center_dec,
                           double fov_deg, double aspect) {
    ra0_ = std::fmod(center_ra, 360.0);
    if (ra0_ < 0.0) ra0_ += 360.0;
    dec0_ = clampf((float)center_dec, -89.9f, 89.9f);
    fov_ = clampf((float)fov_deg, 0.05f, 60.0f);
    if (aspect > 0.01) aspect_ = aspect;
    // V14：Auto View 模式下 pan/zoom 重算 robust STF；Auto Global 保持
    if (auto_view_ && auto_range_) auto_range_dirty_ = true;
}

void HipsSkyView::set_size(int width, int height) {
    if (width > 4 && height > 4) {
        w_ = width;
        h_ = height;
        aspect_ = (double)w_ / (double)h_;
    }
}

void HipsSkyView::set_layer(int layer) { layer_ = layer ? 1 : 0; }

void HipsSkyView::set_stretch(const std::string& preset, bool auto_range) {
    preset_ = preset;
    auto_range_ = auto_range;
}

void HipsSkyView::set_manual_range(float lo, float hi) {
    auto_range_ = false;
    lo_ = lo;
    hi_ = (hi > lo) ? hi : (lo + 1.0f);
}

int HipsSkyView::target_order() const {
    if (!bk_) return 0;
    const int leaf = bk_->get_hips_order();  // 叶级 tile order（GC=7）
    // tile 角宽 ≈ 0.46° × 2^(7-order)；目标：每维 ~4-6 张 tile
    double o = 7.0 - std::log2(std::max(fov_, 0.1) / 1.84);
    int order = (int)std::lround(o);
    order = std::max(0, std::min(order, leaf));
    return order;
}

void HipsSkyView::evict_one() {
    if (cache_.empty()) return;
    auto it = cache_.begin();
    std::uint64_t min_stamp = it->second->stamp;
    for (auto jt = cache_.begin(); jt != cache_.end(); ++jt) {
        if (jt->second->stamp < min_stamp) {
            min_stamp = jt->second->stamp;
            it = jt;
        }
    }
    cache_.erase(it);
    ++metrics_.evictions;
}

HipsSkyView::Tile* HipsSkyView::get_tile(int order, std::uint64_t ipix) {
    if (!bk_) return nullptr;
    const auto key = std::make_pair(order, ipix);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        it->second->stamp = ++clock_;
        ++metrics_.cache_hits_total;
        ++stats_.cache_hits;
        return it->second.get();
    }
    ++metrics_.cache_misses;
    ++stats_.tiles_requested;
    if (!bk_->has_tile_at_order(order, ipix)) return nullptr;
    auto tile = std::make_shared<Tile>();
    tile->order = order;
    tile->ipix = ipix;
    auto t0 = Clock::now();
    if (bk_->read_tile_at_order(order, ipix, tile->sig, tile->sup) != 0)
        return nullptr;
    auto t1 = Clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    metrics_.decode_ms_hist.push_back(ms);
    metrics_.total_tile_reads++;
    stats_.decode_ms += ms;
    if (cache_.size() >= cache_cap_) evict_one();
    tile->stamp = ++clock_;
    ++stats_.tiles_decoded;
    auto ins = cache_.emplace(key, std::move(tile));
    return ins.first->second.get();
}

bool HipsSkyView::sample_value(double ra, double dec, float& out) {
    if (!bk_) return false;
    int order = target_order();
    std::uint64_t leaf = astrocs::healpix::ang2pix_nest(
        1u << (unsigned)(order + kTileShift), ra, dec);
    std::uint64_t tile_ipix = leaf >> 18;
    std::uint64_t local = leaf & kTileMask;
    for (int o = order; o >= 0; --o) {
        Tile* t = get_tile(o, tile_ipix);
        if (t) {
            const std::uint64_t fi =
                astrocs::healpix::nested_local_to_fits_index(
                    local, 9u, 512u);
            if (layer_ == 0) {
                out = t->sig[(size_t)fi];
            } else {
                out = t->sup[(size_t)fi];
            }
            return true;
        }
        if (o > 0) {
            ++stats_.parent_fallbacks;
            tile_ipix >>= 2;              // 父 tile
            const int po = o - 1;
            local = astrocs::healpix::ang2pix_nest(
                        1u << (unsigned)(po + kTileShift), ra, dec) &
                    kTileMask;
        }
    }
    return false;
}

int HipsSkyView::sample_at(double ra, double dec, int order, float& out) const {
    if (!bk_ || order < 0 || order > bk_->get_hips_order()) return -1;
    // 非 const 访问：get_tile 修改缓存
    HipsSkyView* self = const_cast<HipsSkyView*>(this);
    const int saved_layer = self->layer_;
    self->layer_ = 0;
    int rc = -2;
    std::uint64_t leaf = astrocs::healpix::ang2pix_nest(
        1u << (unsigned)(order + kTileShift), ra, dec);
    std::uint64_t tile_ipix = leaf >> 18;
    std::uint64_t local = leaf & kTileMask;
    Tile* t = self->get_tile(order, tile_ipix);
    if (t) {
        const std::uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
            local, 9u, 512u);
        out = t->sig[(size_t)fi];
        rc = 0;
    }
    self->layer_ = saved_layer;
    return rc;
}

int HipsSkyView::query_sky(double ra, double dec, double& sig,
                           double& sup) const {
    if (!bk_) return -1;
    return bk_->query_pixel(ra, dec, sig, sup);
}

void HipsSkyView::reset_metrics() {
    metrics_ = Metrics{};
    stats_ = Stats{};
}

// 显示拉伸：v ∈ [0,1] 原始归一化 → 预设曲线 + MTF
static float display_tone(float x, const std::string& preset, float m,
                          float c) {
    float y = x;
    if (preset == "sqrt") {
        y = std::sqrt(x);
    } else if (preset == "log") {
        y = std::log1p(c * x) / std::log1p(c);
    } else if (preset == "asinh") {
        y = std::asinh(c * x) / std::asinh(c);
    }
    return STFEngine::mtf(y, m);
}

void HipsSkyView::rasterize(std::vector<std::uint32_t>& rgba) {
    auto t0 = Clock::now();
    stats_ = Stats{};
    rgba.assign((std::size_t)w_ * (std::size_t)h_, kBackground);
    if (!bk_) return;

    const int order = target_order();
    stats_.order = order;
    const std::uint32_t nside = 1u << (unsigned)(order + kTileShift);
    const double tan_half = std::tan(fov_ * kPi / 360.0);
    const double r0 = ra0_ * kPi / 180.0;
    const double d0 = dec0_ * kPi / 180.0;
    const std::size_t npx = (std::size_t)w_ * (std::size_t)h_;

    // ---- Phase A（并行）：屏幕像素 → (ra,dec) → leaf ----
    // V14：view 未变时复用已采样 leaves（stretch-only redraw，不重新
    // sky→HEALPix 采样/FITS decode）。
    std::vector<std::uint64_t> leaves;
    const bool view_same =
        cache_w_ == w_ && cache_h_ == h_ &&
        std::fabs(cache_ra0_ - ra0_) < 1e-12 &&
        std::fabs(cache_dec0_ - dec0_) < 1e-12 &&
        std::fabs(cache_fov_ - fov_) < 1e-12;
    if (view_same && !cached_leaves_.empty() &&
        cached_leaves_.size() == npx) {
        leaves = cached_leaves_;
    } else {
    leaves.assign(npx, 0);
    {
        const int nt = omp_get_max_threads();
        std::vector<std::vector<std::uint64_t>> per_thread(nt);
#pragma omp parallel
        {
            const int tid = omp_get_thread_num();
            const int jobs = omp_get_num_threads();
            const int rows = h_ / jobs + 1;
            for (int j = tid * rows; j < h_ && j < (tid + 1) * rows; ++j) {
                const double v = 1.0 - 2.0 * (j + 0.5) / (double)h_;
                for (int i = 0; i < w_; ++i) {
                    const double u = 2.0 * (i + 0.5) / (double)w_ - 1.0;
                    const double xi = -u * tan_half * aspect_;
                    const double eta = v * tan_half;
                    const double rr = std::hypot(xi, eta);
                    double ra, dec;
                    if (rr < 1e-12) {
                        dec = d0;
                        ra = r0;
                    } else {
                        const double c = std::atan(rr);
                        const double sin_c = std::sin(c), cos_c = std::cos(c);
                        dec = std::asin(cos_c * std::sin(d0) +
                                        eta * sin_c * std::cos(d0) / rr);
                        ra = r0 + std::atan2(xi * sin_c,
                                             rr * std::cos(d0) * cos_c -
                                                 eta * std::sin(d0) * sin_c);
                    }
                    ra = std::fmod(ra, 2.0 * kPi);
                    if (ra < 0.0) ra += 2.0 * kPi;
                    leaves[(std::size_t)j * (std::size_t)w_ + (std::size_t)i] =
                        astrocs::healpix::ang2pix_nest(
                            nside, ra * 180.0 / kPi, dec * 180.0 / kPi);
                    per_thread[tid].push_back(
                        leaves[(std::size_t)j * (std::size_t)w_ +
                               (std::size_t)i] >>
                        18);
                }
            }
        }
        std::set<std::uint64_t> tiles_needed;
        for (const auto& vv : per_thread)
            tiles_needed.insert(vv.begin(), vv.end());
        // 解码（单线程；含父级回退）
        for (std::uint64_t t : tiles_needed) {
            if (strict_leaf_) {
                get_tile(order, t);  // strict-leaf：缺 tile 直接背景
            } else {
                if (!get_tile(order, t)) {
                    std::uint64_t p = t >> 2;
                    for (int o = order - 1; o >= 0 && !get_tile(o, p); --o)
                        p >>= 2;
                }
            }
        }
    }
    cached_leaves_ = leaves;
    cache_ra0_ = ra0_; cache_dec0_ = dec0_; cache_fov_ = fov_;
    cache_w_ = w_; cache_h_ = h_;
    }

    // ---- 自动范围（V14）：robust median/MAD + 亮端 clip，Auto Global ----
    float dmin = FLT_MAX, dmax = -FLT_MAX;
    std::size_t valid = 0;
    if (layer_ == 0 && auto_range_ && auto_range_dirty_) {
        std::vector<float> samples;
        for (const auto& kv : cache_) {
            const Tile& t = *kv.second;
            if (t.order != order && t.order != order - 1) continue;
            for (std::size_t k = 0; k < t.sig.size(); k += 16) {
                const float s = t.sig[k];
                if (std::isfinite(s)) samples.push_back(s);
            }
        }
        if (!samples.empty()) {
            std::sort(samples.begin(), samples.end());
            const std::size_t n = samples.size();
            // V14：robust median/MAD + 亮端迭代 clip，亮星不主导背景；
            // Auto Global 保持（auto_range_dirty_ 只在首帧/显式刷新重算）
            const float med = samples[n / 2];
            std::vector<float> dev;
            dev.reserve(n);
            for (float s : samples) dev.push_back(std::fabs(s - med));
            std::sort(dev.begin(), dev.end());
            const float mad = 1.4826f * dev[dev.size() / 2];
            std::vector<float> ret;
            for (int it = 0; it < 3; ++it) {
                ret.clear();
                for (float s : samples)
                    if (s <= med + 3.0f * mad) ret.push_back(s);
                if (ret.size() < 32u) break;
                const float nm = ret[ret.size() / 2];
                if (std::fabs(nm - med) < 1e-6f * (med + 1e-9f)) {
                    ret = samples;  // 收敛：全部保留
                    break;
                }
                // 重算 med/mad（简化：用 ret 中位数）
            }
            const float rmed = ret.empty() ? med : ret[ret.size() / 2];
            std::vector<float> rdev;
            for (float s : (ret.empty() ? samples : ret))
                rdev.push_back(std::fabs(s - rmed));
            std::sort(rdev.begin(), rdev.end());
            const float rmad = 1.4826f * rdev[rdev.size() / 2];
            dmin = rmed - 3.0f * rmad;
            dmax = rmed + 3.0f * rmad;
            if (dmax <= dmin) dmax = dmin + 1.0f;
            valid = samples.size();
            auto_range_dirty_ = false;   // Auto Global：保持标尺
        }
        stats_.data_min = dmin;
        stats_.data_max = dmax;
        stats_.valid_pixels = valid;
        lo_ = dmin;
        hi_ = dmax;
    } else if (layer_ == 0 && !auto_range_) {
        stats_.data_min = lo_;
        stats_.data_max = hi_;
    }

    // ---- Phase B（并行）：leaf → 缓存 tile → 上色 ----
    std::atomic<std::size_t> fallback_count{0};
    const STFParams sp = STFEngine::get_preset(preset_, lo_, hi_);
    const float range = (hi_ - lo_) > 1e-12f ? (hi_ - lo_) : 1.0f;
#pragma omp parallel for schedule(static)
    for (int j = 0; j < h_; ++j) {
        for (int i = 0; i < w_; ++i) {
            const std::size_t idx = (std::size_t)j * (std::size_t)w_ + (std::size_t)i;
            const std::uint64_t leaf = leaves[idx];
            const std::uint64_t tile_ipix = leaf >> 18;
            const std::uint64_t local = leaf & kTileMask;
            const auto key = std::make_pair(order, tile_ipix);
            auto it = cache_.find(key);
            const Tile* t = (it != cache_.end()) ? it->second.get() : nullptr;
            int o = order;
            std::uint64_t tp = tile_ipix;
            std::uint64_t use_local = local;
            // V10 修复：父级回退时必须用父级 nside 下的 local（低 2*(order-o)
            // 位），不能用目标 order 的 local，否则父 tile 大范围内错位采样，
            // 导致“同一数据在屏幕上多处重复出现”的孤岛/碎片。
            while (!t && o > 0 && !strict_leaf_) {
                tp >>= 2;
                --o;
                auto jt = cache_.find(std::make_pair(o, tp));
                t = (jt != cache_.end()) ? jt->second.get() : nullptr;
                if (t) {
                    ++fallback_count;
                    use_local = (leaf >> (2u * (unsigned)(order - o))) &
                                kTileMask;
                }
            }
            std::uint32_t color = kBackground;
            if (t) {
                // 最近邻采样：高倍率下保留 HiPS tile 像素网格，不掩盖真实结构
                const std::uint64_t fi =
                    astrocs::healpix::nested_local_to_fits_index(use_local, 9u,
                                                                 512u);
                if (layer_ == 0) {
                    const float val = t->sig[(size_t)fi];
                    if (std::isfinite(val)) {
                        const float x = clampf((val - lo_) / range, 0.0f, 1.0f);
                        const float tone = display_tone(x, preset_, sp.midtones,
                                                        sp.compression);
                        const std::uint32_t g =
                            (std::uint32_t)clampf(tone * 255.0f, 0.0f, 255.0f);
                        color = 0xFF000000 | (g << 16) | (g << 8) | g;
                    }
                } else {
                    const float s = clampf(t->sup[(size_t)fi], 0.0f, 1.0f);
                    const std::uint32_t g = (std::uint32_t)clampf(
                        std::sqrt(s) * 255.0f, 0.0f, 255.0f);
                    color = (s <= 0.0f)
                                ? kBlack
                                : (0xFF000000 | (g << 16) | (g << 8) | g);
                }
            }
            rgba[idx] = color;
        }
    }
    stats_.parent_fallbacks = fallback_count.load();
    auto t1 = Clock::now();
    stats_.frame_ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
}
