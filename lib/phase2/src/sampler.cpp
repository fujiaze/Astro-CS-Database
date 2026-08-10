// lib/phase2/src/sampler.cpp — Phase2 W4 稀疏光度控制点采样器
//
// 语义（控制包 34A532A2...B2EB308 + wiki Phase2_Unified_Photometric_Model）：
//   - 控制点布置于整个 coverage union（不限于 pairwise overlap）；
//   - 控制点 geometry 由 union geometry + target angular spacing 决定，
//     不由 SNR 决定（SNR 只参与观测可信度）；
//   - y_ik 从实际 Phase1 HiPS signal/support 读取（AIO 唯一 I/O）；
//   - patch estimator：cell 附近小型 patch，support>0 + finite 过滤，
//     robust median 位置 + MAD 尺度，保留负值；
//   - snr_ik 来自 Phase1 SNR Catalogue 邻近星点（不重新检测星点）。
#include "astro/phase2/sampler.h"

#include "healpix/healpix_core.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include "aio_hips_reader.h"
}

namespace {

constexpr int kTileWidth = 512;
constexpr int kTileShift = 9;  // log2(512)
constexpr int kSnrCatalogMax = 1 << 16;

struct FrameData {
    std::set<std::uint64_t> tiles;          // order=K tile ipix
    std::vector<double> snr_ra, snr_dec, snr;
};

inline std::uint64_t leaf_of_tile(std::uint64_t tile_ipix, int leaf_shift) {
    return tile_ipix << (2u * (unsigned)leaf_shift);
}

// 读取一帧 signal/support tile 到内存（每帧每 tile 一次）
struct TilePair {
    std::vector<float> signal;
    std::vector<float> support;
    bool ok = false;
};

int read_tile_pair(AioHipsDataset* sig, AioHipsDataset* sup,
                   std::uint64_t tile_ipix, TilePair* out) {
    out->signal.resize((size_t)kTileWidth * kTileWidth);
    out->support.resize((size_t)kTileWidth * kTileWidth);
    if (aio_hips_read_tile_f32(sig, tile_ipix, out->signal.data()) != 0)
        return 1;
    if (aio_hips_read_tile_f32(sup, tile_ipix, out->support.data()) != 0)
        return 1;
    out->ok = true;
    return 0;
}

double median_of(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const std::size_t n = v.size();
    const std::size_t mid = n / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (n % 2 == 1) return v[mid];
    const double a = v[mid];
    const double b = *std::min_element(v.begin(), v.begin() + mid);
    return 0.5 * (a + b);
}

inline std::uint64_t fnv1a64(const char* s) {
    std::uint64_t h = 14695981039346656037ULL;
    for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
        h ^= (std::uint64_t)*p;
        h *= 1099511628211ULL;
    }
    return h;
}

} // namespace

extern "C" {

std::uint64_t p2_frame_id(const char* hips_path) {
    if (!hips_path) return 0;
    return fnv1a64(hips_path);
}

int p2_sample_controls(const P2CoverageResult* coverage,
                       const char* const* hips_paths,
                       const P2SamplerConfig* cfg_in,
                       P2ControlObservation* out_obs,
                       std::uint64_t out_capacity,
                       std::uint64_t* out_n_obs,
                       std::uint64_t* out_n_controls,
                       char* err, std::size_t err_size) {
    if (!coverage || !hips_paths || !out_n_obs || !out_n_controls) {
        if (err && err_size) std::snprintf(err, err_size, "bad args");
        return 1;
    }
    *out_n_obs = 0;
    *out_n_controls = 0;
    P2SamplerConfig cfg;
    if (cfg_in) {
        cfg = *cfg_in;
    } else {
        cfg.control_grid_per_tile = 8;
        cfg.patch_radius_leaf = 2;
        cfg.min_samples = 5;
        cfg.snr_search_radius_deg = 0.05;
    }
    if (cfg.control_grid_per_tile < 1) cfg.control_grid_per_tile = 8;
    if (cfg.patch_radius_leaf < 0) cfg.patch_radius_leaf = 2;
    if (cfg.min_samples < 1) cfg.min_samples = 5;
    if (cfg.snr_search_radius_deg <= 0.0) cfg.snr_search_radius_deg = 0.05;

    const std::uint64_t n_frames = coverage->n_inputs;
    const int leaf_shift = 9;  // tile 内 512×512 leaf

    // 打开每帧 signal/support/snr 并收集 tile 集合
    std::vector<AioHipsDataset*> sig(n_frames, nullptr);
    std::vector<AioHipsDataset*> sup(n_frames, nullptr);
    std::vector<FrameData> frames(n_frames);
    for (std::uint64_t i = 0; i < n_frames; ++i) {
        sig[i] = aio_hips_open(hips_paths[i], AIO_HIPS_RD_SIGNAL);
        sup[i] = aio_hips_open(hips_paths[i], AIO_HIPS_RD_SUPPORT);
        if (!sig[i] || !sup[i]) {
            if (err && err_size)
                std::snprintf(err, err_size, "open frame %llu failed: %s",
                              (unsigned long long)i,
                              aio_hips_reader_last_error());
            for (std::uint64_t j = 0; j <= i; ++j) {
                if (sig[j]) aio_hips_close(sig[j]);
                if (sup[j]) aio_hips_close(sup[j]);
            }
            return 1;
        }
        const int n = aio_hips_tile_count(sig[i]);
        for (int t = 0; t < n; ++t) {
            std::uint64_t ip = 0;
            if (aio_hips_tile_ipix(sig[i], t, &ip) == 0)
                frames[i].tiles.insert(ip);
        }
        // SNR catalogue（质量/可信度场，不参与空间基函数）
        AioHipsDataset* snr = aio_hips_open(hips_paths[i], AIO_HIPS_RD_SNR);
        if (snr) {
            frames[i].snr_ra.resize(kSnrCatalogMax);
            frames[i].snr_dec.resize(kSnrCatalogMax);
            frames[i].snr.resize(kSnrCatalogMax);
            const int got = aio_hips_read_snr_catalog(
                snr, frames[i].snr_ra.data(), frames[i].snr_dec.data(),
                frames[i].snr.data(), nullptr, nullptr, nullptr, kSnrCatalogMax);
            frames[i].snr_ra.resize((size_t)std::max(got, 0));
            frames[i].snr_dec.resize((size_t)std::max(got, 0));
            frames[i].snr.resize((size_t)std::max(got, 0));
            aio_hips_close(snr);
        }
    }

    std::vector<P2ControlObservation> obs;
    std::uint64_t control_id = 0;
    const int grid = cfg.control_grid_per_tile;
    const int cell_side = kTileWidth / grid;
    const int r = cfg.patch_radius_leaf;

    for (std::uint64_t c = 0; c < coverage->n_union_cells; ++c) {
        const std::uint64_t tile_ipix = coverage->union_cells[c].ipix;
        // 找到覆盖该 tile 的帧
        std::vector<std::uint64_t> cov_frames;
        for (std::uint64_t i = 0; i < n_frames; ++i) {
            if (frames[i].tiles.count(tile_ipix)) cov_frames.push_back(i);
        }
        if (cov_frames.empty()) continue;

        // 读取各覆盖帧的 signal/support tile
        std::vector<TilePair> pairs(cov_frames.size());
        for (std::size_t fi = 0; fi < cov_frames.size(); ++fi) {
            read_tile_pair(sig[cov_frames[fi]], sup[cov_frames[fi]],
                           tile_ipix, &pairs[fi]);
        }

        for (int gy = 0; gy < grid; ++gy) {
            for (int gx = 0; gx < grid; ++gx) {
                const int cx0 = gx * cell_side;
                const int cy0 = gy * cell_side;
                const int cx = cx0 + cell_side / 2;
                const int cy = cy0 + cell_side / 2;
                const std::uint64_t center_local =
                    astrocs::healpix::xy_to_nested_local(
                        (unsigned)cx, (unsigned)cy, (unsigned)kTileShift);
                const std::uint64_t center_leaf =
                    leaf_of_tile(tile_ipix, leaf_shift) + center_local;
                double ra_deg = 0.0, dec_deg = 0.0;
                astrocs::healpix::pix2ang_nest(
                    1u << (unsigned)(coverage->target_order + leaf_shift),
                    center_leaf, ra_deg, dec_deg);

                for (std::size_t fi = 0; fi < cov_frames.size(); ++fi) {
                    const std::uint64_t frame_id = cov_frames[fi];
                    const TilePair& tp = pairs[fi];
                    if (!tp.ok) continue;
                    // patch 统计
                    std::vector<double> vals;
                    double sup_sum = 0.0;
                    std::uint32_t n_valid = 0;
                    for (int dy = -r; dy <= r; ++dy) {
                        for (int dx = -r; dx <= r; ++dx) {
                            const int x = cx + dx;
                            const int y = cy + dy;
                            if (x < 0 || y < 0 || x >= kTileWidth ||
                                y >= kTileWidth)
                                continue;
                            const std::uint64_t z =
                                astrocs::healpix::xy_to_nested_local(
                                    (unsigned)x, (unsigned)y,
                                    (unsigned)kTileShift);
                            const std::uint64_t fi_idx =
                                astrocs::healpix::nested_local_to_fits_index(
                                    z, (unsigned)kTileShift, kTileWidth);
                            const float s = tp.signal[(size_t)fi_idx];
                            const float sp = tp.support[(size_t)fi_idx];
                            if (!std::isfinite(s)) continue;
                            if (sp <= 0.0f) continue;
                            vals.push_back(s);
                            sup_sum += sp;
                            ++n_valid;
                        }
                    }
                    if (vals.size() < (std::size_t)cfg.min_samples) continue;
                    const double y = median_of(vals);
                    std::vector<double> dev;
                    dev.reserve(vals.size());
                    for (double v : vals) dev.push_back(std::fabs(v - y));
                    const double mad = 1.4826 * median_of(std::move(dev));
                    const double sigma = (mad > 0.0) ? mad : 1e-12;
                    const double uncertainty =
                        sigma / std::sqrt((double)vals.size());
                    // SNR：邻近星点 median（无点 → 中性 1.0）
                    double snr_val = 1.0;
                    const FrameData& fd = frames[frame_id];
                    if (!fd.snr.empty()) {
                        std::vector<double> near;
                        const double rad = cfg.snr_search_radius_deg;
                        for (std::size_t s = 0; s < fd.snr.size(); ++s) {
                            if (astrocs::healpix::angular_distance_deg(
                                    ra_deg, dec_deg, fd.snr_ra[s],
                                    fd.snr_dec[s]) <= rad)
                                near.push_back(fd.snr[s]);
                        }
                        if (!near.empty()) snr_val = median_of(std::move(near));
                    }
                    P2ControlObservation o{};
                    o.frame_id = p2_frame_id(hips_paths[frame_id]);
                    o.control_id = control_id;
                    o.leaf_ipix = center_leaf;
                    o.ra_deg = ra_deg;
                    o.dec_deg = dec_deg;
                    o.value = y;
                    o.uncertainty = uncertainty;
                    o.snr = snr_val;
                    o.support = n_valid ? sup_sum / (double)n_valid : 0.0;
                    o.quality_flags = 0;
                    obs.push_back(o);
                }
                ++control_id;
            }
        }
    }

    for (std::uint64_t i = 0; i < n_frames; ++i) {
        if (sig[i]) aio_hips_close(sig[i]);
        if (sup[i]) aio_hips_close(sup[i]);
    }

    *out_n_controls = control_id;
    *out_n_obs = obs.size();
    if (out_obs) {
        const std::uint64_t n = std::min(out_capacity, obs.size());
        for (std::uint64_t i = 0; i < n; ++i) out_obs[i] = obs[(size_t)i];
    }
    return 0;
}

} // extern "C"
