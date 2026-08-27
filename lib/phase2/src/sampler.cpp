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
#include <chrono>
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

#if defined(P2_ENABLE_OPENMP) && defined(_OPENMP)
#include <omp.h>
#include <atomic>
#include <mutex>
#include <thread>
#endif

#define NOMINMAX
#if defined(_WIN32) && defined(__has_include)
#if __has_include(<windows.h>)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#include <excpt.h>
#endif
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#include <excpt.h>
#endif

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

// （K_CORR_DOMAIN 选项 B）：k_corr 标定表（control_median_mc /
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

// 从帧 HiPS properties 解析 Drizzle provenance
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
    double kcorr = kControlCorrDefault;     //：per-frame k_corr
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

// CON-010 复归：cfitsio / aio_hips 并发文件读在本机 gcc 14 上非线程安全，
// 多 worker 并发 aio_hips_* 会导致 _IO_fread SIGSEGV（TSan 于 sampler.cpp 误报
// std::set::count 竞态，真正崩溃在 _IO_fread）。故将所有 aio_hips 的 open/read
// 用单一全局 mutex 串行化（计算仍并行），消除崩溃且不改变每 cell 结果。
static std::mutex g_aio_mu;

int read_tile_pair(AioHipsDataset* sig, AioHipsDataset* sup,
                   std::uint64_t tile_ipix, TilePair* out) {
    if (!sig || !sup || !out) return 2;
    std::lock_guard<std::mutex> lk(g_aio_mu);   // 串行化 cfitsio 读，避免并发 _IO_fread
    out->ok = false;
    try {
        out->signal.resize((size_t)kTileWidth * kTileWidth);
        out->support.resize((size_t)kTileWidth * kTileWidth);
    } catch (...) {
        return 3;
    }
    if (aio_hips_read_tile_f32(sig, tile_ipix, out->signal.data()) != 0)
        return 1;
    if (aio_hips_read_tile_f32(sup, tile_ipix, out->support.data()) != 0)
        return 1;
    out->ok = true;
    return 0;
}

#if defined(_WIN32) && defined(_MSC_VER)
static int seh_filter(unsigned long code, const char* where, char* err, std::size_t err_size) {
    if (err && err_size) std::snprintf(err, err_size, "SEH 0x%08lX at %s (AV outside try/catch)", code, where ? where : "?");
    std::fprintf(stderr, "[sampler] SEH 0x%08lX at %s\n", code, where ? where : "?");
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

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
    c.cpu_workers = 0;                  // 0=auto：默认构建 P2_ENABLE_OPENMP=OFF => 实际串行(=1)
    return c;
}

std::uint64_t p2_frame_id(const char* hips_path) {
    if (!hips_path || !*hips_path) return 0;
    try {
    // 科学产品稳定身份——关键元数据 + signal/support 像素 +
    // SNR catalogue 内容（canonical SHA-256）。
    // 复制/重命名/换根目录不变；任何科学 payload 变化 → 改变。
    // 性能：增量 Sha256 流式 update，禁止 500MB std::string 堆积
    // （修复 O(payload²) 二次方拷贝）。
    AioHipsDataset* d = aio_hips_open(hips_path, AIO_HIPS_RD_SIGNAL);
    if (!d) return 0;
    astrocs::crypto::Sha256 sha;
    char buf[8192];
    if (aio_hips_get_properties(d, buf, (int)sizeof(buf)) == 0) {
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
            const std::string seg = std::string(k) + "=" +
                        (it == props.end() ? std::string() : it->second) + ";";
            sha.update(seg.data(), seg.size());
        }
    }
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
            const std::string pre = std::to_string(t) + "=";
            sha.update(pre.data(), pre.size());
            sha.update(tile_buf.data(), tile_buf.size() * sizeof(float));
            const char semi = ';';
            sha.update(&semi, 1);
        }
    }
    aio_hips_close(d);
    AioHipsDataset* sp = aio_hips_open(hips_path, AIO_HIPS_RD_SUPPORT);
    if (sp) {
        for (std::uint64_t t : tiles) {
            if (aio_hips_read_tile_f32(sp, t, tile_buf.data()) == 0) {
                const std::string pre = "S" + std::to_string(t) + "=";
                sha.update(pre.data(), pre.size());
                sha.update(tile_buf.data(), tile_buf.size() * sizeof(float));
                const char semi = ';';
                sha.update(&semi, 1);
            }
        }
        aio_hips_close(sp);
    }
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
                const std::string pre = std::to_string(i) + ":";
                sha.update(pre.data(), pre.size());
                const auto fmt = [](double v) {
                    std::ostringstream os;
                    os << std::setprecision(
                              std::numeric_limits<double>::max_digits10)
                       << v;
                    return os.str();
                };
                const std::string seg = fmt(ra[i]) + "," + fmt(dec[i]) + "," +
                           fmt(snr[i]) + "," + std::to_string(qf[i]) + ";";
                sha.update(seg.data(), seg.size());
            }
        }
        aio_hips_close(sn);
    }
    const std::string hex = sha.final_hex();
    std::uint64_t id = 0;
    for (int i = 0; i < 16; ++i) {
        id <<= 4;
        const char c = hex[(std::size_t)i];
        id += (c <= '9') ? (std::uint64_t)(c - '0')
                         : (std::uint64_t)(c - 'a' + 10);
    }
    return id;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[p2_frame_id] exception %s path=%s\n", e.what(), hips_path ? hips_path : "(null)"); std::fflush(stderr);
        return 0;
    } catch (...) {
        std::fprintf(stderr, "[p2_frame_id] unknown exception path=%s\n", hips_path ? hips_path : "(null)"); std::fflush(stderr);
        return 0;
    }
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

static int p2_sample_controls_impl(
                      const P2CoverageResult* coverage,
                      const char* const* hips_paths,
                      const std::uint64_t* frame_ids_in,
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
    P2SamplerConfig cfg = p2_sampler_default_config();
    if (cfg_in) cfg = *cfg_in;
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
        cfg.control_k_corr = kControlCorrDefault;
    if (cfg.control_grid_per_tile < 1) cfg.control_grid_per_tile = 8;
    if (cfg.patch_radius_leaf < 0) cfg.patch_radius_leaf = 2;
    if (cfg.min_samples < 1) cfg.min_samples = 5;
    if (cfg.snr_search_radius_deg <= 0.0) cfg.snr_search_radius_deg = 0.05;

    const std::uint64_t n_frames = coverage->n_inputs;
    std::vector<std::uint64_t> fid_cache(n_frames);
    if (frame_ids_in) {
        for (std::uint64_t i = 0; i < n_frames; ++i) fid_cache[i] = frame_ids_in[i];
    } else {
        for (std::uint64_t i = 0; i < n_frames; ++i)
            fid_cache[i] = p2_frame_id(hips_paths[i]);
    }
    for (std::uint64_t i = 0; i < n_frames; ++i) {
        if (fid_cache[i] == 0) {
            if (err && err_size)
                std::snprintf(err, err_size,
                              "frame_id 0 invalid (hash/open failed) frame %llu path=%s",
                              (unsigned long long)i,
                              hips_paths[i] ? hips_paths[i] : "(null)");
            std::fprintf(stderr, "[sampler] frame_id 0 invalid frame %llu path=%s\n",
                         (unsigned long long)i,
                         hips_paths[i] ? hips_paths[i] : "(null)");
            std::fflush(stderr);
            return 1;
        }
    }
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
        // （K_CORR_DOMAIN 选项 B）：读帧 Drizzle provenance → k_corr
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

    // 第一遍：每个 union cell 的 64 controls（hotfix：串行；
    // 保留 tile 级复用——每 cell 覆盖帧的 signal/support tile 只读一次，
    // 消除 64× 重读；OpenMP 默认关闭，可用 -DP2_ENABLE_OPENMP=ON 显式开启）。
    const std::uint64_t n_union = coverage->n_union_cells;
    // 边界：714*64=45696，32 帧候选约 1.4M；reserve 前检查溢出
    if (n_union > (std::size_t)1e6) {
        if (err && err_size) std::snprintf(err, err_size, "n_union too large %llu", (unsigned long long)n_union);
        for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
        return 1;
    }
    try {
    // 诊断：coverage 后、cells.resize 前、首 tile 读前 立即落盘
    std::fprintf(stderr, "[sampler] enter n_union=%llu grid=%d n_frames=%llu target_order=%d\n",
        (unsigned long long)n_union, grid, (unsigned long long)n_frames, coverage->target_order);
    std::fflush(stderr);
    if ((std::size_t)n_union * (std::size_t)grid * grid > (std::size_t)200 * 1000 * 1000) {
        if (err && err_size) std::snprintf(err, err_size, "cells too large %llu", (unsigned long long)n_union * (std::size_t)grid * grid);
        for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
        return 1;
    }
    try { cells.resize(n_union * (std::size_t)grid * grid); } catch (const std::exception& e) {
        if (err && err_size) std::snprintf(err, err_size, "cells resize failed: %s", e.what());
        std::fprintf(stderr, "[sampler] cells resize failed: %s\n", e.what()); std::fflush(stderr);
        for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
        return 1;
    }
    std::fprintf(stderr, "[sampler] cells resized %zu, first tile read check\n", cells.size()); std::fflush(stderr);
    // 首 tile 预读校验（空/范围 sớm, 避免首 tile AV 静默）
    if (n_union > 0) {
        const std::uint64_t t0ip = coverage->union_cells[0].ipix;
        const std::uint64_t npix_check = 12ULL * ((std::uint64_t)1 << (2u * (unsigned)coverage->target_order));
        if (t0ip >= npix_check) {
            if (err && err_size) std::snprintf(err, err_size, "tile ipix out of range %llu >= %llu (order %d)", (unsigned long long)t0ip, (unsigned long long)npix_check, coverage->target_order);
            for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
            return 1;
        }
        if (n_frames > 0 && sig[0]) {
            std::fprintf(stderr, "[sampler] first tile %llu probe (frames with tile: ", (unsigned long long)t0ip); std::fflush(stderr);
            for (std::uint64_t i = 0; i < std::min<std::uint64_t>(n_frames, 4); ++i) std::fprintf(stderr, "%d ", frames[i].tiles.count(t0ip)?1:0);
            std::fprintf(stderr, ")\n"); std::fflush(stderr);
        }
    }
    std::uint64_t sum_catalog_veto = 0;
    std::uint64_t sum_insufficient_support = 0;

    const auto t0 = std::chrono::steady_clock::now();
    std::uint64_t progress = 0;
#if defined(_WIN32) && defined(_MSC_VER)
    __try {
#endif
    // ============ CON-004 parallel sampler first pass ============
    // worker reader：serial 复用 setup 打开的共享句柄；并行每 worker 独立句柄
    // （cfitsio 同句柄并发读非线程安全 => 禁跨线程共享；禁全局 critical(aio_read)）。
    struct SamplerReader {
        const char* const* paths = nullptr;
        AioHipsDataset* const* shared_sig = nullptr;
        AioHipsDataset* const* shared_sup = nullptr;
        std::size_t n = 0;
        bool own = false;
        std::vector<AioHipsDataset*> csig, csup;
        void init_shared(AioHipsDataset* const* s, AioHipsDataset* const* p, std::size_t n_) { shared_sig = s; shared_sup = p; n = n_; own = false; }
        void init_own(const char* const* p, std::size_t n_) { paths = p; n = n_; own = true; csig.assign(n, nullptr); csup.assign(n, nullptr); }
        AioHipsDataset* sig(std::size_t f) { if (!own) return shared_sig[f]; if (!csig[f]) { std::lock_guard<std::mutex> lk(g_aio_mu); csig[f] = aio_hips_open(paths[f], AIO_HIPS_RD_SIGNAL); } return csig[f]; }
        AioHipsDataset* sup(std::size_t f) { if (!own) return shared_sup[f]; if (!csup[f]) { std::lock_guard<std::mutex> lk(g_aio_mu); csup[f] = aio_hips_open(paths[f], AIO_HIPS_RD_SUPPORT); } return csup[f]; }
        void close_all() { if (!own) return; for (AioHipsDataset* p : csig) if (p) aio_hips_close(p); for (AioHipsDataset* p : csup) if (p) aio_hips_close(p); csig.clear(); csup.clear(); }
    };

    // per-cell body：串行与并行共用（杜绝双份漂移）。返回 0 或错误码(1=pairs resize OOM)。
    auto pass1_cell = [&](std::uint64_t c, SamplerReader& rdr,
                         std::uint64_t& cv, std::uint64_t& ci) -> int {
        cv = 0; ci = 0;
        const std::uint64_t tile_ipix = coverage->union_cells[c].ipix;
        {
            const std::uint64_t npix = 12ULL * ((std::uint64_t)1 << (2u * (unsigned)coverage->target_order));
            if (tile_ipix >= npix) {
                std::fprintf(stderr, "[sampler] skip out-of-range tile %llu >= %llu at c=%llu\n", (unsigned long long)tile_ipix, (unsigned long long)npix, (unsigned long long)c); std::fflush(stderr);
                for (int gy = 0; gy < grid; ++gy) for (int gx = 0; gx < grid; ++gx) {
                    const std::size_t idx = (std::size_t)c * grid * grid + (std::size_t)(gy * grid + gx);
                    if (idx < cells.size()) { cells[idx].tile = -1; }
                }
                return 0;
            }
        }
        std::vector<std::uint64_t> cov_frames;
        for (std::uint64_t i = 0; i < n_frames; ++i)
            if (frames[i].tiles.count(tile_ipix)) cov_frames.push_back(i);

        std::vector<TilePair> pairs;
        if (!cov_frames.empty()) {
            try { pairs.resize(cov_frames.size()); } catch (...) { return 1; }
            for (std::size_t fi = 0; fi < cov_frames.size(); ++fi) {
                if (!rdr.sig(cov_frames[fi]) || !rdr.sup(cov_frames[fi])) {
                    std::fprintf(stderr, "[sampler] null dataset at frame %llu tile %llu\n", (unsigned long long)cov_frames[fi], (unsigned long long)tile_ipix); std::fflush(stderr);
                    continue;
                }
                const int rc = read_tile_pair(rdr.sig(cov_frames[fi]), rdr.sup(cov_frames[fi]), tile_ipix, &pairs[fi]);
                if (rc != 0) {
                    pairs[fi].ok = false;
                    std::fprintf(stderr, "[sampler] read_tile_pair failed rc=%d frame=%llu tile=%llu\n", rc, (unsigned long long)cov_frames[fi], (unsigned long long)tile_ipix); std::fflush(stderr);
                }
            }
        }

        for (int gy = 0; gy < grid; ++gy) {
            for (int gx = 0; gx < grid; ++gx) {
                const int cx = gx * cell_side + cell_side / 2;
                const int cy = gy * cell_side + cell_side / 2;
                const std::uint64_t center_local = astrocs::healpix::xy_to_nested_local((unsigned)cx, (unsigned)cy, (unsigned)kTileShift);
                const std::uint64_t center_leaf = leaf_of_tile(tile_ipix, leaf_shift) + center_local;
                double ra_deg = 0.0, dec_deg = 0.0;
                astrocs::healpix::pix2ang_nest(1u << (unsigned)(coverage->target_order + leaf_shift), center_leaf, ra_deg, dec_deg);

                CellStat cs;
                cs.ra = ra_deg; cs.dec = dec_deg; cs.leaf = center_leaf;
                cs.tile = (int)tile_ipix; cs.gx = gx; cs.gy = gy;
                std::uint64_t local_veto = 0, local_insupp = 0;
                for (std::size_t fi = 0; fi < cov_frames.size(); ++fi) {
                    const std::uint64_t frame_id = cov_frames[fi];
                    const TilePair& tp = pairs[fi];
                    if (!tp.ok || tp.signal.empty() || tp.support.empty() ||
                        tp.signal.size() < (size_t)kTileWidth * kTileWidth ||
                        tp.support.size() < (size_t)kTileWidth * kTileWidth) {
                        ++local_insupp;
                        cs.frames.push_back((int)frame_id);
                        cs.m.push_back(0); cs.mad.push_back(0); cs.bfrac.push_back(0); cs.unc.push_back(0);
                        cs.snr.push_back(0); cs.sup.push_back(0); cs.cvar.push_back(0); cs.civar.push_back(0);
                        cs.n_total.push_back(0); cs.n_retained.push_back(0); cs.snr_avail.push_back(0); cs.qual.push_back(0);
                        cs.accepted.push_back(false); cs.reason.push_back(1);
                        continue;
                    }
                    if (cx < 0 || cy < 0 || cx >= kTileWidth || cy >= kTileWidth) {
                        ++local_insupp;
                        cs.frames.push_back((int)frame_id);
                        cs.m.push_back(0); cs.mad.push_back(0); cs.bfrac.push_back(0); cs.unc.push_back(0);
                        cs.snr.push_back(0); cs.sup.push_back(0); cs.cvar.push_back(0); cs.civar.push_back(0);
                        cs.n_total.push_back(0); cs.n_retained.push_back(0); cs.snr_avail.push_back(0); cs.qual.push_back(0);
                        cs.accepted.push_back(false); cs.reason.push_back(1);
                        continue;
                    }
                    std::vector<double> vals;
                    vals.reserve(400);
                    double sup_sum = 0.0;
                    std::uint32_t n_valid = 0;
                    for (int dy = -r; dy <= r; ++dy) {
                        for (int dx = -r; dx <= r; ++dx) {
                            const int x = cx + dx;
                            const int y = cy + dy;
                            if (x < 0 || y < 0 || x >= kTileWidth || y >= kTileWidth) continue;
                            const std::uint64_t z = astrocs::healpix::xy_to_nested_local((unsigned)x, (unsigned)y, (unsigned)kTileShift);
                            const std::uint64_t fi_idx = astrocs::healpix::nested_local_to_fits_index(z, (unsigned)kTileShift, kTileWidth);
                            if (fi_idx >= tp.signal.size() || fi_idx >= tp.support.size()) continue;
                            const float s = tp.signal[(size_t)fi_idx];
                            const float sp = tp.support[(size_t)fi_idx];
                            if (!std::isfinite(s)) continue;
                            if (!std::isfinite(sp) || sp <= 0.0f) continue;
                            vals.push_back(s);
                            sup_sum += sp;
                            ++n_valid;
                        }
                    }
                    const int n_total = (int)vals.size();
                    if (n_total < cfg.min_samples) {
                        ++local_insupp;
                        cs.frames.push_back((int)frame_id);
                        cs.m.push_back(0); cs.mad.push_back(0); cs.bfrac.push_back(0); cs.unc.push_back(0);
                        cs.snr.push_back(0); cs.sup.push_back(0); cs.cvar.push_back(0); cs.civar.push_back(0);
                        cs.n_total.push_back(n_total); cs.n_retained.push_back(0); cs.snr_avail.push_back(0); cs.qual.push_back(0);
                        cs.accepted.push_back(false); cs.reason.push_back(1);
                        continue;
                    }
                    double m0 = median_of(vals);
                    {
                        double s0 = 0.0;
                        {
                            std::vector<double> dev;
                            dev.reserve(vals.size());
                            for (double v : vals) dev.push_back(std::fabs(v - m0));
                            s0 = 1.4826 * median_of(std::move(dev));
                        }
                        std::vector<double> ret = vals;
                        for (int it = 0; it < cfg.background_clip_iters; ++it) {
                            std::vector<double> nr;
                            nr.reserve(ret.size());
                            for (double v : ret) if (v <= m0 + cfg.background_clip_sigma * s0) nr.push_back(v);
                            if ((int)nr.size() < cfg.min_samples) break;
                            const double nm = median_of(nr);
                            if (std::fabs(nm - m0) < 1e-12 * std::max(std::fabs(m0), 1e-12)) { ret = nr; break; }
                            m0 = nm;
                            ret = std::move(nr);
                            std::vector<double> dev2;
                            dev2.reserve(ret.size());
                            for (double v : ret) dev2.push_back(std::fabs(v - m0));
                            const double s1 = 1.4826 * median_of(std::move(dev2));
                            if (s1 <= 0.0) break;
                            s0 = s1;
                        }
                        const double y = m0;
                        const double sigma = (s0 > 0.0) ? s0 : 1e-12;
                        int nbright = 0;
                        for (double v : vals) if (v > y + cfg.background_contamination_sigma * sigma) ++nbright;
                        const double bfrac = (double)nbright / (double)n_total;
                        const int n_retained = (int)ret.size();
                        cs.frames.push_back((int)frame_id);
                        cs.m.push_back(y);
                        cs.mad.push_back(sigma);
                        cs.bfrac.push_back(bfrac);
                        const double n_ret = std::max((double)n_retained, 1.0);
                        const double kcorr_f = (frames[frame_id].kcorr > 0.0) ? frames[frame_id].kcorr : cfg.control_k_corr;
                        const double cvar = kcorr_f * kPiHalf * sigma * sigma / n_ret;
                        cs.cvar.push_back(cvar);
                        cs.civar.push_back(cvar > 0.0 ? 1.0 / cvar : 0.0);
                        cs.unc.push_back(std::sqrt(cvar));
                        cs.sup.push_back(n_valid ? sup_sum / (double)n_valid : 0.0);
                        cs.n_total.push_back(n_total);
                        cs.n_retained.push_back(n_retained);
                        int veto = 0;
                        if (cfg.background_catalog_veto && !frames[frame_id].snr.empty() && frame_snr_med[frame_id] > 0.0) {
                            const double thr = 10.0 * frame_snr_med[frame_id];
                            const double rad = 0.012;
                            if (snr_idx[frame_id].any_above(thr, ra_deg, dec_deg, rad)) veto = 1;
                        }
                        double snr_val = 1.0;
                        int snr_avail = 0;
                        std::uint32_t qual = 0;
                        if (!frames[frame_id].snr.empty()) {
                            std::vector<double> near_snr;
                            snr_idx[frame_id].query(ra_deg, dec_deg, cfg.snr_search_radius_deg, &near_snr, &qual);
                            if (!near_snr.empty()) { snr_val = median_of(std::move(near_snr)); snr_avail = 1; }
                            else { snr_val = frame_snr_med_exact[frame_id]; }
                        } else { snr_val = 0.0; }
                        cs.snr.push_back(snr_val);
                        cs.snr_avail.push_back(snr_avail);
                        cs.qual.push_back(qual);
                        cs.accepted.push_back(veto == 0);
                        cs.reason.push_back(veto ? 5 : 0);
                        if (veto) ++local_veto;
                    }
                }
                const std::size_t idx = (std::size_t)c * grid * grid + (std::size_t)(gy * grid + gx);
                cells[idx] = std::move(cs);
                cv += local_veto;
                ci += local_insupp;
            }
        }
        return 0;
    };

    // worker 数：0=auto => hardware_concurrency；1 或非 OpenMP 构建 => 串行（默认行为不变）。
    // 仅在 OpenMP 构建下计算/使用（避免 OFF 构建 unused 警告）。
#if defined(P2_ENABLE_OPENMP) && !defined(_MSC_VER)
    const int workers = (cfg.cpu_workers > 0) ? cfg.cpu_workers : (int)std::max(1u, std::thread::hardware_concurrency());
    const bool par = (workers > 1);
#endif

#if defined(P2_ENABLE_OPENMP) && !defined(_MSC_VER)
    if (par) {
        std::atomic<int> pass1_fail{0};
        #pragma omp parallel num_threads(workers)
        {
            SamplerReader rdr; rdr.init_own(hips_paths, n_frames);
            #pragma omp for schedule(dynamic) reduction(+:sum_catalog_veto, sum_insufficient_support)
            for (std::uint64_t c = 0; c < n_union; ++c) {
                std::uint64_t cv = 0, ci = 0;
                if (pass1_cell(c, rdr, cv, ci) != 0) { pass1_fail.store(1); continue; }
                sum_catalog_veto += cv;
                sum_insufficient_support += ci;
            }
            rdr.close_all();
        }
        if (pass1_fail.load()) {
            if (err && err_size) std::snprintf(err, err_size, "pass1 pairs resize failed (parallel)");
            for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
            return 1;
        }
    } else
#endif
    {
        SamplerReader rdr; rdr.init_shared(sig.data(), sup.data(), n_frames);
        for (std::uint64_t c = 0; c < n_union; ++c) {
            std::uint64_t cv = 0, ci = 0;
            if (pass1_cell(c, rdr, cv, ci) != 0) {
                if (err && err_size) std::snprintf(err, err_size, "pairs resize failed at c=%llu", (unsigned long long)c);
                for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
                return 1;
            }
            sum_catalog_veto += cv;
            sum_insufficient_support += ci;
            ++progress;
            if (progress % 100 == 0 || progress == n_union) {
                const auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - t0).count();
                std::fprintf(stderr, "[sampler] progress %llu/%llu tiles (%.1fs)\n", (unsigned long long)progress, (unsigned long long)n_union, elapsed);
                std::fflush(stderr);
            }
        }
    }
#if defined(_WIN32) && defined(_MSC_VER)
    } __except(seh_filter(GetExceptionCode(), "sampler first pass", err, err_size)) {
        std::fprintf(stderr, "[sampler] SEH caught in first pass, err=%s\n", err ? err : "");
        std::fflush(stderr);
        for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
        return 1;
    }
#endif
    // 空覆盖占位：control_id 仍需覆盖所有 grid
    control_id = cells.size();
    stats.rejected_catalog_veto += sum_catalog_veto;
    stats.rejected_insufficient_support += sum_insufficient_support;
    // 补偿：空覆盖 tiles 对应 cells 无 frames，跳过即等于未处理，已占位

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

    // obs/ctrl vector 容量上限拒绝（防 OOM；不至于 714*64*32 规模撑爆）
    if (cells.size() > (std::size_t)200 * 1000 * 1000) {
        if (err && err_size) std::snprintf(err, err_size, "cells too large %zu", cells.size());
        std::fprintf(stderr, "[sampler] cells too large %zu\n", cells.size()); std::fflush(stderr);
        for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
        return 1;
    }
    // stats 补偿：candidate 为几何覆盖总数；rejected 计数已在各阶段递增，此处仅补 candidate
    // 避免对 retained/support 重复递增（曾导致 double-count）
    stats.candidate_observations = 0;
    for (const auto& cs : cells) {
        // 防止单 cell frames 异常膨胀导致 stats 溢出
        if (cs.frames.size() > 10000) {
            std::fprintf(stderr, "[sampler] skip oversized cs.frames %zu at tile %d\n", cs.frames.size(), cs.tile); std::fflush(stderr);
            continue;
        }
        stats.candidate_observations += cs.frames.size();
    }
    } catch (const std::exception& e) {
        if (err && err_size) std::snprintf(err, err_size, "sampler exception: %s", e.what());
        for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
        return 1;
    } catch (...) {
        if (err && err_size) std::snprintf(err, err_size, "sampler unknown exception");
        for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
        return 1;
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
    return p2_sample_controls_impl(coverage, hips_paths, nullptr, cfg_in,
                                   out_obs, out_capacity, out_n_obs,
                                   out_n_controls, out_stats, out_controls,
                                   ctrl_capacity, err, err_size);
}

int p2_sample_controls_cached(const P2CoverageResult* coverage,
                              const char* const* hips_paths,
                              const std::uint64_t* frame_ids,
                              const P2SamplerConfig* cfg_in,
                              P2ControlObservation* out_obs,
                              std::uint64_t out_capacity,
                              std::uint64_t* out_n_obs,
                              std::uint64_t* out_n_controls,
                              P2SampleStats* out_stats,
                              P2ControlNode* out_controls,
                              std::uint64_t ctrl_capacity,
                              char* err, std::size_t err_size) {
    return p2_sample_controls_impl(coverage, hips_paths, frame_ids, cfg_in,
                                   out_obs, out_capacity, out_n_obs,
                                   out_n_controls, out_stats, out_controls,
                                   ctrl_capacity, err, err_size);
}

} // extern "C"
