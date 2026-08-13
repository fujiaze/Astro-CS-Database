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

#include "crypto/sha256.h"
#include "healpix/healpix_core.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
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
    std::vector<std::uint32_t> quality;     // Phase1 SNR catalogue quality
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
    // nth_element 后 [begin, mid) 全部 ≤ v[mid]；偶数下中位数为
    // [begin, mid) 的最大值（排序后 v[mid-1]）。min_element 是错误的
    // （P0-01：会污染 y_ik / MAD / SNR 邻域中位数）。
    const double b = *std::max_element(v.begin(), v.begin() + mid);
    return 0.5 * (a + b);
}

} // namespace

extern "C" {

std::uint64_t p2_frame_id(const char* hips_path) {
    if (!hips_path || !*hips_path) return 0;
    // R2（V4）：科学产品稳定身份——关键元数据 + signal tile DATASUM +
    // support tile DATASUM + SNR catalogue 内容（canonical SHA-256）。
    // 复制/重命名/换根目录不变；signal/support 像素 payload 或
    // SNR/quality catalogue 变化 → 改变。
    AioHipsDataset* d = aio_hips_open(hips_path, AIO_HIPS_RD_SIGNAL);
    if (!d) return 0;
    std::string payload;
    char buf[8192];
    if (aio_hips_get_properties(d, buf, (int)sizeof(buf)) == 0) {
        // 关键字段白名单（影响 Phase2 科学结果的元数据）
        const std::map<std::string, std::string> props = [&]() {
            std::map<std::string, std::string> kv;
            std::istringstream ss(buf);
            std::string line;
            while (std::getline(ss, line)) {
                const std::size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string k = line.substr(0, eq);
                    std::string v = line.substr(eq + 1);
                    auto trim = [](std::string& s) {
                        while (!s.empty() &&
                               (s.back() == ' ' || s.back() == '\r'))
                            s.pop_back();
                        while (!s.empty() && s.front() == ' ') s.erase(s.begin());
                    };
                    trim(k);
                    trim(v);
                    kv[k] = v;
                }
            }
            return kv;
        }();
        static const char* keys[] = {
            "creator_did", "obs_title", "obs_filter", "obs_exptime",
            "obs_date", "hips_order", "hips_release_date",
            "hips_pixel_scale", "moc_sky_fraction"};
        for (const char* k : keys) {
            const auto it = props.find(k);
            payload += std::string(k) + "=" +
                       (it == props.end() ? std::string() : it->second) + ";";
        }
    }
    // signal tile 数据 hash（科学 payload 指纹；MOC/properties 不变时
    // 像素变化也改变 identity）
    std::vector<std::uint64_t> tiles;
    std::vector<float> tile_buf(512ull * 512ull);
    const int n = aio_hips_tile_count(d);
    if (n > 0) {
        tiles.reserve((std::size_t)n);
        for (int i = 0; i < n; ++i) {
            std::uint64_t ip = 0;
            if (aio_hips_tile_ipix(d, i, &ip) == 0) tiles.push_back(ip);
        }
        std::sort(tiles.begin(), tiles.end());
    }
    for (std::uint64_t t : tiles) {
        if (aio_hips_read_tile_f32(d, t, tile_buf.data()) == 0) {
            payload += std::to_string(t) + "=";
            payload.append(reinterpret_cast<const char*>(tile_buf.data()),
                           tile_buf.size() * sizeof(float));
            payload += ";";
        }
    }
    aio_hips_close(d);
    // support tile 数据 hash
    AioHipsDataset* sp = aio_hips_open(hips_path, AIO_HIPS_RD_SUPPORT);
    if (sp) {
        for (std::uint64_t t : tiles) {
            if (aio_hips_read_tile_f32(sp, t, tile_buf.data()) == 0) {
                payload += "S" + std::to_string(t) + "=";
                payload.append(
                    reinterpret_cast<const char*>(tile_buf.data()),
                    tile_buf.size() * sizeof(float));
                payload += ";";
            }
        }
        aio_hips_close(sp);
    }
    // SNR/quality catalogue 内容（读全部点并序列化）
    AioHipsDataset* sn = aio_hips_open(hips_path, AIO_HIPS_RD_SNR);
    if (sn) {
        const int maxn = 1 << 20;
        std::vector<double> ra(maxn), dec(maxn), snr(maxn);
        std::vector<std::uint32_t> qf(maxn);
        const int got = aio_hips_read_snr_catalog(
            sn, ra.data(), dec.data(), snr.data(), nullptr, qf.data(),
            nullptr, maxn);
        if (got > 0) {
            for (int i = 0; i < got; ++i) {
                payload += std::to_string(i) + ":";
                const auto fmt = [](double v) {
                    std::ostringstream os;
                    os << std::setprecision(
                              std::numeric_limits<double>::max_digits10)
                       << v;
                    return os.str();
                };
                payload += fmt(ra[i]) + "," + fmt(dec[i]) + "," +
                           fmt(snr[i]) + "," + std::to_string(qf[i]) + ";";
            }
        }
        aio_hips_close(sn);
    }
    const std::string hex =
        astrocs::crypto::sha256_hex(payload.data(), payload.size());
    // 取前 16 hex → uint64（稳定、可复现）
    std::uint64_t id = 0;
    for (int i = 0; i < 16; ++i) {
        id <<= 4;
        const char c = hex[(std::size_t)i];
        id += (c <= '9') ? (std::uint64_t)(c - '0')
                         : (std::uint64_t)(c - 'a' + 10);
    }
    return id;
}

double p2_stats_median(const double* vals, std::uint64_t n) {
    if (!vals || n == 0) return 0.0;
    std::vector<double> v;
    v.reserve(n);
    for (std::uint64_t i = 0; i < n; ++i)
        if (std::isfinite(vals[i])) v.push_back(vals[i]);
    return median_of(std::move(v));
}

double p2_stats_mad(const double* vals, std::uint64_t n,
                    double* out_median) {
    if (!vals || n == 0) return 0.0;
    std::vector<double> v;
    v.reserve(n);
    for (std::uint64_t i = 0; i < n; ++i)
        if (std::isfinite(vals[i])) v.push_back(vals[i]);
    if (v.empty()) return 0.0;
    const double med = median_of(v);
    if (out_median) *out_median = med;
    for (double& x : v) x = std::fabs(x - med);
    return 1.4826 * median_of(std::move(v));
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
    // R2：frame_id 缓存（payload 敏感，DISCOVER 阶段一次计算）
    std::vector<std::uint64_t> fid_cache(n_frames);
    for (std::uint64_t i = 0; i < n_frames; ++i)
        fid_cache[i] = p2_frame_id(hips_paths[i]);
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
            frames[i].quality.resize(kSnrCatalogMax);
            const int got = aio_hips_read_snr_catalog(
                snr, frames[i].snr_ra.data(), frames[i].snr_dec.data(),
                frames[i].snr.data(), nullptr, frames[i].quality.data(),
                nullptr, kSnrCatalogMax);
            frames[i].snr_ra.resize((size_t)std::max(got, 0));
            frames[i].snr_dec.resize((size_t)std::max(got, 0));
            frames[i].snr.resize((size_t)std::max(got, 0));
            frames[i].quality.resize((size_t)std::max(got, 0));
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
                    // V4 R6：SNR = 邻近星点 median；无局部星点时
                    // snr=0.0 且 snr_available=0（禁止 1.0 伪装 unknown，
                    // 由 stage2 回退整帧 median）。
                    // quality：邻域点 quality 按位 OR（区域最坏可信度）；
                    // 无邻域点 → 0（unknown，QUALITY_FALLBACK_UNKNOWN）
                    double snr_val = 1.0;
                    int snr_avail = 0;
                    std::uint32_t qual = 0;
                    const FrameData& fd = frames[frame_id];
                    if (!fd.snr.empty()) {
                        std::vector<double> near;
                        const double rad = cfg.snr_search_radius_deg;
                        for (std::size_t s = 0; s < fd.snr.size(); ++s) {
                            if (astrocs::healpix::angular_distance_deg(
                                    ra_deg, dec_deg, fd.snr_ra[s],
                                    fd.snr_dec[s]) <= rad) {
                                near.push_back(fd.snr[s]);
                                qual |= fd.quality[s];
                            }
                        }
                        if (!near.empty()) {
                            snr_val = median_of(std::move(near));
                            snr_avail = 1;
                        } else {
                            // V12R2 (SEAM-001)：UPM 拟合层 SNR 缺失回退为
                            // 帧级 median（与 stage2 集成层 V11 R8 的
                            // frame-median fallback 同一语义）。此前此处
                            // 写 0.0，导致 snr=0 的观测 raw_w=0、参考帧在
                            // overlap control 上权重被清零，M 被非参考帧
                            // 定义而参考帧自身 C 被 gauge 固定为 0 →
                            // depth=1↔2 转换处光度标尺跳变（接缝）。
                            // 注意：snr_avail 保持 0（来源仍是 fallback）。
                            snr_val = median_of(fd.snr);
                        }
                    } else {
                        snr_val = 0.0;  // 无星表：保持 0（无可用信息）
                    }
                    P2ControlObservation o{};
                    o.frame_id = fid_cache[frame_id];
                    o.control_id = control_id;
                    o.leaf_ipix = center_leaf;
                    o.ra_deg = ra_deg;
                    o.dec_deg = dec_deg;
                    o.value = y;
                    o.uncertainty = uncertainty;
                    o.snr = snr_val;
                    o.snr_available = snr_avail;
                    o.support = n_valid ? sup_sum / (double)n_valid : 0.0;
                    o.quality_flags = qual;
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
