// lib/phase2/src/sampler.cpp — Phase2 W4 稀疏光度控制点采样器
//
// 语义（ 34A532A2...B2EB308 + wiki Phase2_Unified_Photometric_Model）：
// - 控制点布置于整个 coverage union（不限于 pairwise overlap）；
// - 控制点 geometry 由 union geometry + target angular spacing 决定，
// 不由 SNR 决定（SNR 只参与观测可信度）；
// - y_ik 从实际 Phase1 HiPS signal/support 读取（AIO 唯一 I/O）；
// - patch estimator：cell 附近小型 patch，support>0 + finite 过滤，
// robust median 位置 + MAD 尺度，保留负值；
// - snr_ik 来自 Phase1 SNR Catalogue 邻近星点（不重新检测星点）。
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
#include <numeric>
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
// 冻结：Drizzle 输出像素协方差导致的 control estimator 方差放大
// （ALG-UPM-CONTROL-IVAR-001）。由 UPMW-005 control_median_mc_test 在
// 当前 Drizzle 引擎（pixfrac=0.8 生产默认）2000 实现 MC 校准：
// k_corr_empirical = 1.3883，N_eff ≈ 181 < N_retained=251。冻结保守值 1.4。
constexpr double kControlCorrDefault = 1.4;
constexpr double kPiHalf = 1.57079632679489661923;  // π/2

// V19R4（K_CORR_DOMAIN 选项 B）：k_corr 标定表（control_median_mc /
// kcorr_matrix_test 实测，pixfrac × 源像素角尺度双线性插值）。
// 矩阵：scale 300"/600" 两档 × pixfrac 0.5/0.8/1.0。
double kcorr_lookup(double pixfrac, double scale_arcsec) {
    static const double pf_grid[3] = {0.5, 0.8, 1.0};
    static const double sc_grid[2] = {300.0, 600.0};
    static const double k[2][3] = {
        {1.2112, 1.3925, 1.4980},   // 300"/px
        {2.3958, 2.8971, 3.2035},   // 600"/px
    };
    const double pf = std::clamp(pixfrac, 0.5, 1.0);
    const double sc = std::clamp(scale_arcsec, 300.0, 600.0);
    // 双线性插值
    const double wi = (pf - pf_grid[0]) / (pf_grid[2] - pf_grid[0]);
    const double i0 = wi * 2.0, i1 = i0 + 1.0;   // 网格列
    const std::size_t c0 = (std::size_t)std::min(i0, 2.0);
    const std::size_t c1 = (std::size_t)std::min(i1, 2.0);
    const double fx = i0 - (double)c0;
    const double r0 =
        (sc_grid[1] - sc) / (sc_grid[1] - sc_grid[0]);
    // 先按 scale 行插值，再按 pixfrac 列插值
    const double k_r0 = k[0][c0] + (k[0][c1] - k[0][c0]) * fx;
    const double k_r1 = k[1][c0] + (k[1][c1] - k[1][c0]) * fx;
    return k_r0 + (k_r1 - k_r0) * (1.0 - r0);
}

// 从帧 HiPS properties 解析 Drizzle provenance（V19R4）
void frame_drizzle_provenance(const char* hips_path, double* pixfrac,
                              double* scale_arcsec) {
    *pixfrac = 0.0;
    *scale_arcsec = 0.0;
    AioHipsDataset* d = aio_hips_open(hips_path, AIO_HIPS_RD_SIGNAL);
    if (!d) return;
    char buf[8192];
    if (aio_hips_get_properties(d, buf, (int)sizeof(buf)) == 0) {
        std::istringstream ss(buf);
        std::string line;
        while (std::getline(ss, line)) {
            const std::size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            const std::string k = line.substr(0, eq);
            const std::string v = line.substr(eq + 1);
            if (k == "ASTROCS_DRIZZLE_PIXFRAC") {
                const double vf = std::atof(v.c_str());
                if (vf > 0.0 && vf <= 1.0) *pixfrac = vf;
            } else if (k == "ASTROCS_DRIZZLE_SCALE_ARCSEC") {
                const double vs = std::atof(v.c_str());
                if (vs > 0.0) *scale_arcsec = vs;
            }
        }
    }
    aio_hips_close(d);
}

struct FrameData {
    std::set<std::uint64_t> tiles;          // order=K tile ipix
    std::vector<double> snr_ra, snr_dec, snr;
    std::vector<std::uint32_t> quality;     // Phase1 SNR catalogue quality
    double kcorr = kControlCorrDefault;     // V19R4：per-frame k_corr
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

// 性能：SNR catalogue 空间索引（dec 排序 + RA 窗口保守预筛，
// 最终判据与全扫描完全一致——同一 angular_distance_deg 精确调用）。
struct SnrIndex {
    std::vector<double> ra, dec, snr;
    std::vector<std::uint32_t> quality;
    std::vector<std::size_t> order;   // 按 dec 升序的星索引
    std::vector<double> dec_sorted;

    void build(const std::vector<double>& ra_, const std::vector<double>& dec_,
               const std::vector<double>& snr_,
               const std::vector<std::uint32_t>& quality_) {
        ra = ra_;
        dec = dec_;
        snr = snr_;
        quality = quality_;
        order.resize(ra.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](std::size_t a, std::size_t b) {
                      return dec[a] < dec[b];
                  });
        dec_sorted.resize(order.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            dec_sorted[i] = dec[order[i]];
    }

    // 查询 (ra,dec) 半径 radius_deg 内全部星（保守候选窗口；最终精确距离）
    // out_snr 可空；out_qual 收集半径内星的 quality OR。
    void query(double ra_c, double dec_c, double radius_deg,
               std::vector<double>* out_snr,
               std::uint32_t* out_qual) const {
        if (out_snr) out_snr->clear();
        if (out_qual) *out_qual = 0;
        if (dec.empty() || radius_deg <= 0.0) return;
        const auto lo = std::lower_bound(dec_sorted.begin(),
                                         dec_sorted.end(), dec_c - radius_deg);
        const auto hi = std::upper_bound(dec_sorted.begin(),
                                         dec_sorted.end(), dec_c + radius_deg);
        const double cos_guard =
            std::max(std::cos((std::fabs(dec_c) + radius_deg) *
                              3.14159265358979323846 / 180.0),
                     1e-4);
        const double ra_win = radius_deg / cos_guard;  // 保守 RA 窗口
        for (auto it = lo; it != hi; ++it) {
            const std::size_t s = order[(std::size_t)(it - dec_sorted.begin())];
            double dra = std::fabs(ra[s] - ra_c);
            if (dra > 180.0) dra = 360.0 - dra;
            if (dra > ra_win) continue;
            if (astrocs::healpix::angular_distance_deg(ra_c, dec_c, ra[s],
                                                       dec[s]) <= radius_deg) {
                if (out_snr) out_snr->push_back(snr[s]);
                if (out_qual) *out_qual |= quality[s];
            }
        }
    }

    // 是否存在半径内 snr > threshold 的星（veto 用，与全扫描等价）
    bool any_above(double threshold, double ra_c, double dec_c,
                   double radius_deg) const {
        if (dec.empty() || radius_deg <= 0.0) return false;
        const auto lo = std::lower_bound(dec_sorted.begin(),
                                         dec_sorted.end(), dec_c - radius_deg);
        const auto hi = std::upper_bound(dec_sorted.begin(),
                                         dec_sorted.end(), dec_c + radius_deg);
        const double cos_guard =
            std::max(std::cos((std::fabs(dec_c) + radius_deg) *
                              3.14159265358979323846 / 180.0),
                     1e-4);
        const double ra_win = radius_deg / cos_guard;
        for (auto it = lo; it != hi; ++it) {
            const std::size_t s = order[(std::size_t)(it - dec_sorted.begin())];
            double dra = std::fabs(ra[s] - ra_c);
            if (dra > 180.0) dra = 360.0 - dra;
            if (dra > ra_win) continue;
            if (snr[s] > threshold &&
                astrocs::healpix::angular_distance_deg(ra_c, dec_c, ra[s],
                                                       dec[s]) <= radius_deg)
                return true;
        }
        return false;
    }
};

} // namespace

extern "C" {

// sampler 默认配置单一来源（null cfg 与显式 cfg 同语义）。
P2SamplerConfig p2_sampler_default_config(void) {
    P2SamplerConfig c{};
    c.control_grid_per_tile = 8;
    c.patch_radius_leaf = 2;
    c.min_samples = 5;
    c.snr_search_radius_deg = 0.05;
    c.background_patch_radius = 8;
    c.background_clip_sigma = 3.0;
    c.background_clip_iters = 3;
    c.background_max_contamination = 0.20;
    c.background_contamination_sigma = 3.0;
    c.background_min_retained_fraction = 0.60;
    c.background_tolerance = 3.0;
    c.background_neighbor_radius = 2;
    c.background_catalog_veto = 1;
    c.control_k_corr = kControlCorrDefault;
    return c;
}

std::uint64_t p2_frame_id(const char* hips_path) {
    if (!hips_path || !*hips_path) return 0;
    // 科学产品稳定身份——关键元数据 + signal tile DATASUM +
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
                       P2SampleStats* out_stats,
                       P2ControlNode* out_controls,
                       std::uint64_t ctrl_capacity,
                       char* err, std::size_t err_size) {
    if (!coverage || !hips_paths || !out_n_obs || !out_n_controls) {
        if (err && err_size) std::snprintf(err, err_size, "bad args");
        return 1;
    }
    *out_n_obs = 0;
    *out_n_controls = 0;
    P2SampleStats stats{};
    // 修复：cfg 必须零初始化（此前 null 路径用栈垃圾值，默认值
    // 随机失效 → n_obs 不确定）；默认值单源 p2_sampler_default_config。
    P2SamplerConfig cfg = p2_sampler_default_config();
    if (cfg_in) cfg = *cfg_in;
    // 默认（synthetic + GC 调优后固化；BACKGROUND_SAMPLER_SPEC.md）
    if (cfg.background_patch_radius <= 0) cfg.background_patch_radius = 8;
    if (cfg.background_clip_sigma <= 0.0) cfg.background_clip_sigma = 3.0;
    if (cfg.background_clip_iters <= 0) cfg.background_clip_iters = 3;
    if (cfg.background_max_contamination <= 0.0)
        cfg.background_max_contamination = 0.20;
    if (cfg.background_contamination_sigma <= 0.0)
        cfg.background_contamination_sigma = 3.0;
    if (cfg.background_min_retained_fraction <= 0.0)
        cfg.background_min_retained_fraction = 0.60;
    if (cfg.background_tolerance <= 0.0) cfg.background_tolerance = 3.0;
    if (cfg.background_neighbor_radius <= 0)
        cfg.background_neighbor_radius = 2;
    if (cfg.control_k_corr <= 0.0)
        cfg.control_k_corr = kControlCorrDefault;   // 冻结 MC 校准值
    if (cfg.control_grid_per_tile < 1) cfg.control_grid_per_tile = 8;
    if (cfg.patch_radius_leaf < 0) cfg.patch_radius_leaf = 2;
    if (cfg.min_samples < 1) cfg.min_samples = 5;
    if (cfg.snr_search_radius_deg <= 0.0) cfg.snr_search_radius_deg = 0.05;

    const std::uint64_t n_frames = coverage->n_inputs;
    // frame_id 缓存（payload 敏感，DISCOVER 阶段一次计算）
    std::vector<std::uint64_t> fid_cache(n_frames);
    for (std::uint64_t i = 0; i < n_frames; ++i)
        fid_cache[i] = p2_frame_id(hips_paths[i]);
    const int leaf_shift = 9;  // tile 内 512×512 leaf

    // 打开每帧 signal/support/snr 并收集 tile 集合
    std::vector<AioHipsDataset*> sig(n_frames, nullptr);
    std::vector<AioHipsDataset*> sup(n_frames, nullptr);
    std::vector<AioHipsDataset*> ivr(n_frames, nullptr);   // ivar 产品 (可缺)
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
        // V19R4（K_CORR_DOMAIN 选项 B）：读帧 Drizzle provenance → k_corr
        {
            double pf = 0.0, sc = 0.0;
            frame_drizzle_provenance(hips_paths[i], &pf, &sc);
            if (pf > 0.0)
                frames[i].kcorr = (sc > 0.0)
                    ? kcorr_lookup(pf, sc)
                    : kcorr_lookup(pf, 300.0);   // scale 未知：300" 档保守
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
        // 逐像素 ivar 产品 (Drizzle 方差传播)
        // 缺失 → o.ivar=0 (UPM 权重回退 1/uncertainty², 如实降级)
        ivr[i] = aio_hips_open(hips_paths[i], AIO_HIPS_RD_IVAR);
    }

    // ================= background-clean sampler =================
    // Stage A-E（BACKGROUND_SAMPLER_SPEC.md）：
    // A 候选 patch（可配置半径，默认 17×17）
    // B 亮端迭代 sigma clipping（median/MAD）
    // C DBE-like 局部 tolerance gate（邻域粗背景 B_local）
    // D contamination fraction gate
    // E 可选 SNR catalogue veto
    // 同一 control ≥2 帧 clean 观测才进入 UPM（相对光度约束）
    std::vector<P2ControlObservation> obs;
    std::uint64_t control_id = 0;
    const int grid = cfg.control_grid_per_tile;
    const int cell_side = kTileWidth / grid;
    const int r = cfg.background_patch_radius;   // Stage A

    // 第一遍：每 (cell, frame) 候选统计
    struct CellStat {
        std::vector<int> frames;                  // 覆盖帧（frame index）
        std::vector<double> m, mad, bfrac, unc, snr, sup;
        // control estimator（patch median）统计方差/逆方差
        // control_variance = k_corr × (π/2) × sigma² / N_retained
        std::vector<double> cvar, civar;
        std::vector<int> n_total, n_retained, snr_avail;
        std::vector<std::uint32_t> qual;
        std::vector<bool> accepted;
        std::vector<int> reason;                  // 0=ok 1..5=原因
        double ra = 0, dec = 0;
        std::uint64_t leaf = 0;
        int tile = -1, gx = 0, gy = 0;
    };
    std::vector<CellStat> cells;
    // 帧级 SNR 中位数（catalogue veto 与 fallback 用）
    std::vector<double> frame_snr_med(n_frames, 0.0);
    std::vector<double> frame_snr_med_exact(n_frames, 0.0);  // median_of 语义
    std::vector<SnrIndex> snr_idx(n_frames);  // 空间索引（dec 排序）
    for (std::uint64_t i = 0; i < n_frames; ++i) {
        if (!frames[i].snr.empty()) {
            std::vector<double> cp = frames[i].snr;
            std::sort(cp.begin(), cp.end());
            frame_snr_med[i] = cp[cp.size() / 2];
            frame_snr_med_exact[i] = median_of(frames[i].snr);
            snr_idx[i].build(frames[i].snr_ra, frames[i].snr_dec,
                             frames[i].snr, frames[i].quality);
        }
    }

    for (std::uint64_t c = 0; c < coverage->n_union_cells; ++c) {
        const std::uint64_t tile_ipix = coverage->union_cells[c].ipix;
        std::vector<std::uint64_t> cov_frames;
        for (std::uint64_t i = 0; i < n_frames; ++i)
            if (frames[i].tiles.count(tile_ipix)) cov_frames.push_back(i);
        if (cov_frames.empty()) continue;

        std::vector<TilePair> pairs(cov_frames.size());
        for (std::size_t fi = 0; fi < cov_frames.size(); ++fi)
            read_tile_pair(sig[cov_frames[fi]], sup[cov_frames[fi]],
                           tile_ipix, &pairs[fi]);

        for (int gy = 0; gy < grid; ++gy) {
            for (int gx = 0; gx < grid; ++gx) {
                const int cx = gx * cell_side + cell_side / 2;
                const int cy = gy * cell_side + cell_side / 2;
                const std::uint64_t center_local =
                    astrocs::healpix::xy_to_nested_local(
                        (unsigned)cx, (unsigned)cy, (unsigned)kTileShift);
                const std::uint64_t center_leaf =
                    leaf_of_tile(tile_ipix, leaf_shift) + center_local;
                double ra_deg = 0.0, dec_deg = 0.0;
                astrocs::healpix::pix2ang_nest(
                    1u << (unsigned)(coverage->target_order + leaf_shift),
                    center_leaf, ra_deg, dec_deg);

                CellStat cs;
                cs.ra = ra_deg; cs.dec = dec_deg; cs.leaf = center_leaf;
                cs.tile = (int)tile_ipix; cs.gx = gx; cs.gy = gy;
                for (std::size_t fi = 0; fi < cov_frames.size(); ++fi) {
                    const std::uint64_t frame_id = cov_frames[fi];
                    const TilePair& tp = pairs[fi];
                    // Stage A/B：patch 收集 + 亮端迭代 clipping
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
                    const int n_total = (int)vals.size();
                    if (n_total < cfg.min_samples) {
                        ++stats.rejected_insufficient_support;
                        cs.frames.push_back((int)frame_id);
                        cs.m.push_back(0); cs.mad.push_back(0);
                        cs.bfrac.push_back(0); cs.unc.push_back(0);
                        cs.snr.push_back(0); cs.sup.push_back(0);
                        cs.cvar.push_back(0); cs.civar.push_back(0);
                        cs.n_total.push_back(n_total); cs.n_retained.push_back(0);
                        cs.snr_avail.push_back(0); cs.qual.push_back(0);
                        cs.accepted.push_back(false); cs.reason.push_back(1);
                        continue;
                    }
                    // Stage B：median/MAD + 亮端迭代 clipping
                    // 注意：收敛判定基于 retain 集 median 变化，而不是
                    // MAD 不变（梯度 patch 的 MAD 在剪星前后相同，若用
                    // MAD 判定会把星像素错误地“恢复”）。
                    double m0 = median_of(vals);
                    {
                        double s0 = 0.0;
                        {
                            std::vector<double> dev;
                            for (double v : vals)
                                dev.push_back(std::fabs(v - m0));
                            s0 = 1.4826 * median_of(std::move(dev));
                        }
                        std::vector<double> ret = vals;
                        for (int it = 0; it < cfg.background_clip_iters; ++it) {
                            std::vector<double> nr;
                            for (double v : ret)
                                if (v <= m0 + cfg.background_clip_sigma * s0)
                                    nr.push_back(v);
                            if ((int)nr.size() < cfg.min_samples) break;
                            const double nm = median_of(nr);
                            if (std::fabs(nm - m0) <
                                1e-12 * std::max(std::fabs(m0), 1e-12)) {
                                ret = nr;  // 中位数收敛
                                break;
                            }
                            m0 = nm;
                            ret = std::move(nr);
                            std::vector<double> dev2;
                            for (double v : ret)
                                dev2.push_back(std::fabs(v - m0));
                            const double s1 =
                                1.4826 * median_of(std::move(dev2));
                            if (s1 <= 0.0) break;
                            s0 = s1;
                        }
                        const double y = m0;
                        const double sigma = (s0 > 0.0) ? s0 : 1e-12;
                        // Stage D：contamination（原始 patch 亮像素占比，
                        // 相对 clip 后背景位置）
                        int nbright = 0;
                        for (double v : vals)
                            if (v > y + cfg.background_contamination_sigma *
                                            sigma)
                                ++nbright;
                        const double bfrac = (double)nbright / (double)n_total;
                        const int n_retained = (int)ret.size();
                        cs.frames.push_back((int)frame_id);
                        cs.m.push_back(y);
                        cs.mad.push_back(sigma);
                        cs.bfrac.push_back(bfrac);
                        //
                        // control estimator = patch median → 标准误
                        // SE(median) = sqrt(control_variance)；
                        // 用 N_retained（clipping 后保留样本），不是 n_total。
                        const double n_ret = std::max((double)n_retained, 1.0);
                        // V19R4：per-frame k_corr（provenance 标定；无
                        // metadata 时回退 cfg.control_k_corr 默认 1.4）
                        const double kcorr_f =
                            (frames[frame_id].kcorr > 0.0)
                                ? frames[frame_id].kcorr
                                : cfg.control_k_corr;
                        const double cvar =
                            kcorr_f * kPiHalf * sigma * sigma / n_ret;
                        cs.cvar.push_back(cvar);
                        cs.civar.push_back(cvar > 0.0 ? 1.0 / cvar : 0.0);
                        cs.unc.push_back(std::sqrt(cvar));
                        cs.sup.push_back(n_valid ? sup_sum / (double)n_valid : 0.0);
                        cs.n_total.push_back(n_total);
                        cs.n_retained.push_back(n_retained);
                        // Stage E：catalogue veto（可选；高 SNR 星点过近）
                        int veto = 0;
                        if (cfg.background_catalog_veto &&
                            !frames[frame_id].snr.empty() &&
                            frame_snr_med[frame_id] > 0.0) {
                            const double thr = 10.0 * frame_snr_med[frame_id];
                            const double rad = 0.012;  // ~ patch 尺度（度）
                            if (snr_idx[frame_id].any_above(
                                    thr, ra_deg, dec_deg, rad))
                                veto = 1;
                        }
                        // SNR 邻域（obs 字段；缺失→帧级 fallback）
                        double snr_val = 1.0;
                        int snr_avail = 0;
                        std::uint32_t qual = 0;
                        if (!frames[frame_id].snr.empty()) {
                            std::vector<double> near;
                            snr_idx[frame_id].query(
                                ra_deg, dec_deg, cfg.snr_search_radius_deg,
                                &near, &qual);
                            if (!near.empty()) {
                                snr_val = median_of(std::move(near));
                                snr_avail = 1;
                            } else {
                                // 整帧 median 回退（与全扫描 median_of 一致）
                                snr_val = frame_snr_med_exact[frame_id];
                            }
                        } else {
                            snr_val = 0.0;
                        }
                        cs.snr.push_back(snr_val);
                        cs.snr_avail.push_back(snr_avail);
                        cs.qual.push_back(qual);
                        // 暂定 accepted；Stage C tolerance 第二遍决定
                        cs.accepted.push_back(veto == 0);
                        cs.reason.push_back(veto ? 5 : 0);
                        if (veto) ++stats.rejected_catalog_veto;
                    }
                }
                cells.push_back(std::move(cs));
                ++control_id;
            }
        }
    }

    // 第二遍：Stage C DBE-like 局部 tolerance gate
    const int nr = cfg.background_neighbor_radius;
    // 按 tile 分组，邻域只遍历同 tile cells（替代全 cells 扫描）
    std::map<int, std::vector<std::size_t>> tile_cells;
    for (std::size_t ci = 0; ci < cells.size(); ++ci)
        tile_cells[cells[ci].tile].push_back(ci);
    std::vector<std::size_t> tile_cell_list;  // 预取同 tile 列表（当前 cell）
    for (std::size_t ci = 0; ci < cells.size(); ++ci) {
        CellStat& cs = cells[ci];
        const auto& same_tile = tile_cells[cs.tile];
        for (std::size_t fi = 0; fi < cs.frames.size(); ++fi) {
            if (!cs.accepted[fi] || cs.reason[fi] != 0) continue;
            // 收集同 tile 邻域候选该帧的 cleaned median
            std::vector<double> neigh;
            for (std::size_t cj : same_tile) {
                const CellStat& cn = cells[cj];
                if (std::abs(cn.gx - cs.gx) > nr ||
                    std::abs(cn.gy - cs.gy) > nr)
                    continue;
                for (std::size_t fj = 0; fj < cn.frames.size(); ++fj) {
                    if (cn.frames[fj] == cs.frames[fi] && cn.n_total[fj] > 0)
                        neigh.push_back(cn.m[fj]);
                }
            }
            // 邻域不足：回退 tile 全部候选（该帧）
            if (neigh.size() < 3) {
                neigh.clear();
                for (std::size_t cj : same_tile) {
                    const CellStat& cn = cells[cj];
                    for (std::size_t fj = 0; fj < cn.frames.size(); ++fj)
                        if (cn.frames[fj] == cs.frames[fi] && cn.n_total[fj] > 0)
                            neigh.push_back(cn.m[fj]);
                }
            }
            if (neigh.size() < 3) continue;  // 无足够邻域：不 gate（保守保留）
            std::sort(neigh.begin(), neigh.end());
            const double B = neigh[neigh.size() / 2];
            std::vector<double> dev;
            for (double v : neigh) dev.push_back(std::fabs(v - B));
            std::sort(dev.begin(), dev.end());
            const double S = 1.4826 * dev[dev.size() / 2];
            if (cs.m[fi] > B + cfg.background_tolerance * S) {
                cs.accepted[fi] = false;
                cs.reason[fi] = 3;
                ++stats.rejected_bright_tolerance;
            } else if (cs.bfrac[fi] > cfg.background_max_contamination) {
                cs.accepted[fi] = false;
                cs.reason[fi] = 4;
                ++stats.rejected_high_contamination;
            } else if ((double)cs.n_retained[fi] <
                       cfg.background_min_retained_fraction *
                           (double)cs.n_total[fi]) {
                cs.accepted[fi] = false;
                cs.reason[fi] = 2;
                ++stats.rejected_insufficient_retained;
            }
        }
    }

    // 第三遍：≥2 帧 clean 的 control 才输出观测
    for (std::size_t ci = 0; ci < cells.size(); ++ci) {
        const CellStat& cs = cells[ci];
        int nclean = 0;
        for (std::size_t fi = 0; fi < cs.frames.size(); ++fi)
            if (cs.accepted[fi]) ++nclean;
        if (nclean == 0) continue;
        ++stats.accepted_controls;
        if (nclean >= 2) ++stats.overlap_controls;
        for (std::size_t fi = 0; fi < cs.frames.size(); ++fi) {
            if (!cs.accepted[fi]) {
                if (cs.reason[fi] == 2) ++stats.rejected_insufficient_retained;
                continue;
            }
            if (nclean < 2) {
                ++stats.rejected_lt_two_clean_frames;
                continue;
            }
            P2ControlObservation o{};
            o.frame_id = fid_cache[cs.frames[fi]];
            o.control_id = (std::uint64_t)ci;
            o.leaf_ipix = cs.leaf;
            o.ra_deg = cs.ra;
            o.dec_deg = cs.dec;
            o.value = cs.m[fi];
            o.uncertainty = cs.unc[fi];
            o.snr = cs.snr[fi];
            o.snr_available = cs.snr_avail[fi];
            // control estimator 的
            // 统计方差/逆方差（patch median；含 Drizzle 协方差 k_corr）。
            o.control_variance = cs.cvar[fi];
            o.control_ivar = cs.civar[fi];
            // 控制点 ivar 取自帧 ivar 产品 (控制 leaf 处)
            // 弃用：仅诊断（单 leaf Phase1 ivar ≠ Var(control
            // estimator)），science 权重一律使用 control_ivar。
            o.ivar = 0.0;
            {
                AioHipsDataset* iv = ivr[cs.frames[fi]];
                if (iv) {
                    float v = 0.0f;
                    if (aio_hips_read_leaf_f32(iv, cs.leaf, &v) == 0 &&
                        std::isfinite(v) && v > 0.0f) {
                        o.ivar = (double)v;
                    }
                }
            }
            o.support = cs.sup[fi];
            o.quality_flags = cs.qual[fi];
            obs.push_back(o);
            ++stats.accepted_observations;
            ++stats.candidate_observations;
        }
    }

    for (std::uint64_t i = 0; i < n_frames; ++i) {
        if (sig[i]) aio_hips_close(sig[i]);
        if (sup[i]) aio_hips_close(sup[i]);
        if (ivr[i]) aio_hips_close(ivr[i]);
    }

    stats.candidate_observations = 0;
    for (const auto& cs : cells) {
        for (std::size_t fi = 0; fi < cs.frames.size(); ++fi) {
            ++stats.candidate_observations;
            if (!cs.accepted[fi]) {
                if (cs.reason[fi] == 2) ++stats.rejected_insufficient_retained;
                else if (cs.reason[fi] == 1) ++stats.rejected_insufficient_support;
            }
        }
    }
    if (out_stats) *out_stats = stats;
    *out_n_controls = control_id;
    *out_n_obs = obs.size();
    if (out_controls) {
        const std::uint64_t n = std::min(ctrl_capacity, cells.size());
        for (std::uint64_t i = 0; i < n; ++i) {
            const CellStat& cs = cells[(size_t)i];
            out_controls[i].control_id = i;
            out_controls[i].tile_ipix = (std::uint64_t)cs.tile;
            out_controls[i].gx = cs.gx;
            out_controls[i].gy = cs.gy;
            out_controls[i].ra_deg = cs.ra;
            out_controls[i].dec_deg = cs.dec;
            out_controls[i].leaf_ipix = cs.leaf;
        }
    }
    if (out_obs) {
        const std::uint64_t n = std::min(out_capacity, obs.size());
        for (std::uint64_t i = 0; i < n; ++i) out_obs[i] = obs[(size_t)i];
    }
    return 0;
}

} // extern "C"
