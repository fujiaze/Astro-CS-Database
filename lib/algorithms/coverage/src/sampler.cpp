// lib/algorithms/coverage/src/sampler.cpp — Phase2 稀疏光度控制点采样器
//
// 语义（冻结；权威 = docs/science/algorithms/PHASE2_SAMPLER.md）：
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
#include <atomic>
#include <mutex>
#include <thread>


// NOMINMAX 只由下面的平台块（已用 #ifndef 守卫）定义。此处原先另有一行**无守卫**的
// `#define NOMINMAX`，而构建系统已在编译命令行定义该宏 ⇒ MSVC C4005「宏重定义」。
// NOMINMAX 是 windows.h 专用宏（非 Windows 侧无意义），故删除无守卫那一行即可，
// 平台块内的定义与守卫保持原样；宏的最终取值不变，零行为变化。
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

// （K_CORR_DOMAIN 选项 B，scale 维 RETIRED）：k_corr 标定表
// （kcorr_matrix_test 实测，pixfrac × 源像素角尺度双线性插值）。
// 矩阵：scale 300"/600" 两档 × pixfrac 0.5/0.8/1.0。
// **域外不得 clamp**：域外静默 clamp 会把「恒取 300 档」固化成合同，并把一个
// 与帧无关的表值当成逐帧标定——生产真帧源像素角尺度 0.9586″/px 远在标定域
// [300,600]″ 之外。域外一律返回冻结默认 kControlCorrDefault（1.4 ≥ 实证
// 1.3883，保守侧），调用方 p2_sample_controls 另打 stderr 标。
double kcorr_lookup(double pixfrac, double scale_arcsec) {
    static const double pf_grid[3] = {0.5, 0.8, 1.0};
    static const double sc_grid[2] = {300.0, 600.0};
    static const double k[2][3] = {
        {1.2112, 1.3925, 1.4980},   // 300"/px
        {2.3958, 2.8971, 3.2035},   // 600"/px
    };
    // 域外（含 NaN/未知）：显式回退冻结默认，禁止 clamp 静默饱和。
    if (!(scale_arcsec >= sc_grid[0] && scale_arcsec <= sc_grid[1]))
        return kControlCorrDefault;
    const double pf = std::clamp(pixfrac, 0.5, 1.0);
    const double sc = scale_arcsec;
    // pixfrac 网格 {0.5,0.8,1.0} 非均匀：两段各自归一化的线性插值，
    // 不得按 [0.5,1.0] 均匀网格取列（F2 角点须精确等于表值）。
    const bool lo = (pf <= pf_grid[1]);
    const std::size_t c0 = lo ? 0u : 1u;
    const std::size_t c1 = lo ? 1u : 2u;
    const double fx = (pf - pf_grid[c0]) / (pf_grid[c1] - pf_grid[c0]);
    const double w0 = 1.0 - fx;
    const double r0 =   // scale 网格 {300,600} 均匀：权重式保证端点 exact
        (sc_grid[1] - sc) / (sc_grid[1] - sc_grid[0]);
    const double k_r0 = k[0][c0] * w0 + k[0][c1] * fx;   // scale 行插值
    const double k_r1 = k[1][c0] * w0 + k[1][c1] * fx;
    return k_r0 * r0 + k_r1 * (1.0 - r0);               // pixfrac 列插值
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
            if (k == "ACSD_DRIZZLE_PIXFRAC") {
                const double vf = std::atof(v.c_str());
                if (vf > 0.0 && vf <= 1.0) *pixfrac = vf;
            } else if (k == "ACSD_DRIZZLE_SCALE_ARCSEC") {
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
    double kcorr = kControlCorrDefault;     // per-frame k_corr
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

// PERF-401：CON-010 的全局读锁已撤销。原 _IO_fread SIGSEGV 的成因是跨线程共享
// 同一个 fitsfile*；现行读路径每次调用各自 open/read/close，句柄只在本线程栈上，
// 且 cfitsio 以 _REENTRANT 构建（FptrTable 与错误栈由 cfitsio 自带 Fitsio_Lock
// 保护，READONLY 打开不共享句柄 —— 依据见 lib/infrastructure/aio/src/
// aio_cfitsio_mutex.h 头注释）。读路径因此零共享可变状态，tile 读真正并行；
// 每 cell 结果逐位不变（同一文件、同一读序、同一归约序）。取锁点改用计数式
// aio::CfitsioLockGuard，锁等待进 resource_timeseries.csv 的 lock_wait_ns 列。
int read_tile_pair(AioHipsDataset* sig, AioHipsDataset* sup,
                   std::uint64_t tile_ipix, TilePair* out) {
    if (!sig || !sup || !out) return 2;
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
[[maybe_unused]] static int seh_filter(unsigned long code, const char* where, char* err, std::size_t err_size) {
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
    const auto mid_it = v.begin() + static_cast<std::ptrdiff_t>(mid);
    std::nth_element(v.begin(), mid_it, v.end());
    if (n % 2 == 1) return v[mid];
    const double a = v[mid];
    // nth_element 后 [begin, mid) 全部 ≤ v[mid]；偶数下中位数为
    // [begin, mid) 的最大值（排序后 v[mid-1]）。min_element 是错误的
    // （P0-01：会污染 y_ik / MAD / SNR 邻域中位数）。
    const double b = *std::max_element(v.begin(), mid_it);
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
            if (acsd::healpix::angular_distance_deg(ra_c, dec_c, ra[s],
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
                acsd::healpix::angular_distance_deg(ra_c, dec_c, ra[s],
                                                       dec[s]) <= radius_deg)
                return true;
        }
        return false;
    }
};

// 晕掩膜证伪停用：整分支只注释不删除（性能优化与 export slim 保留）。
// RERUN2/RERUN3 对比洞为次因，缝主因为 M1/M4 混合 pedestal 差。
// task-4 双通道掩膜与 veto helper（匿名命名空间；纯内存判定，无 I/O）。
// (a) 普通星阈值+8连通 veto：patch 内亮像素（v > y+contam_sigma*sigma）的
// 8-连通最大分量像素数 >= min_samples 即否决（reason=6）。patch 以行主序
// H×W=2r+1 网格给出（vals 按 (dy+r)*W+(dx+r)，越界/无效已在收集时丢弃，
// 以 NaN 占位标记缺失）。与 sky_plane p2_star_mask_caps_simple_bright
// 同数学定义（阈值/8连通/min 像素数），此处输出 veto 判决，那边输出帽。
#if 0
// [晕掩膜证伪停用：simple veto helper 原实现保留，见 #else 分支留桩]
bool simple_mask_connected_veto(const std::vector<double>& grid, int w,
                                double y, double sigma, double contam_sigma,
                                int min_samples) {
    if (w <= 0 || grid.empty() || min_samples < 1) return false;
    if (!(sigma > 0.0) || !std::isfinite(sigma)) return false;
    if (!(contam_sigma > 0.0) || !std::isfinite(contam_sigma)) return false;
    if (!std::isfinite(y)) return false;
    if ((int)grid.size() % w != 0) return false;
    const int h = (int)grid.size() / w;
    const double thr = y + contam_sigma * sigma;
    const std::size_t n = grid.size();
    std::vector<char> bright(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const double v = grid[i];
        if (std::isfinite(v) && v > thr) bright[i] = 1;
    }
    std::vector<char> seen(n, 0);
    std::vector<std::size_t> stack;
    for (int iy = 0; iy < h; ++iy) {
        for (int ix = 0; ix < w; ++ix) {
            const std::size_t s0 = (std::size_t)(iy * w + ix);
            if (!bright[s0] || seen[s0]) continue;
            stack.clear();
            stack.push_back(s0);
            seen[s0] = 1;
            std::size_t cnt = 0;
            std::size_t head = 0;
            while (head < stack.size()) {
                const std::size_t s = stack[head++];
                ++cnt;
                if ((int)cnt >= min_samples) return true;  // 早停
                const int sy = (int)(s / (std::size_t)w);
                const int sx = (int)(s % (std::size_t)w);
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        const int nx = sx + dx;
                        const int ny = sy + dy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        const std::size_t ns = (std::size_t)(ny * w + nx);
                        if (!bright[ns] || seen[ns]) continue;
                        seen[ns] = 1;
                        stack.push_back(ns);
                    }
                }
            }
        }
    }
    return false;
}

// (b) Gaia 晕帽命中判定：候选 (ra,dec) 是否落在任一晕帽内（含边界）。
// 晕帽由调用方传入的 P2GaiaHaloCaps 按星等定半径现场求出
// （p2_gaia_halo_radius_px / fallback），半径像素→度换算与
// p2_star_mask_caps_gaia_halo 同式。无晕帽（null/n==0）→ false。
// P2GaiaHaloCaps 定义在 sky_plane.h（sampler.h 已包含该头）。
bool gaia_halo_hit(const P2GaiaHaloCaps* halos, double ra_c, double dec_c) {
    if (!halos || halos->n == 0) return false;
    if (!halos->ra_deg || !halos->dec_deg || !halos->mag_g) return false;
    if (!std::isfinite(ra_c) || !std::isfinite(dec_c)) return false;
    if (!(halos->anchor_pixel_scale_arcsec > 0.0) ||
        !std::isfinite(halos->anchor_pixel_scale_arcsec))
        return false;
    double ratio = halos->pixel_scale_ratio;
    if (!(ratio > 0.0) || !std::isfinite(ratio)) ratio = 1.0;
    const double d2r = 3.14159265358979323846 / 180.0;
    const double r = ra_c * d2r, d = dec_c * d2r;
    const double sd = std::sin(d), cd = std::cos(d);
    for (std::uint64_t i = 0; i < halos->n; ++i) {
        const double hra = halos->ra_deg[i];
        const double hdec = halos->dec_deg[i];
        const double mag = halos->mag_g[i];
        if (!std::isfinite(hra) || !std::isfinite(hdec) ||
            !std::isfinite(mag))
            continue;
        double r_px = 0.0;
        if (p2_gaia_halo_radius_px(mag, halos->mag_thresh, halos->r8,
                                         halos->a, halos->r_min, halos->r_max,
                                         &r_px) != 0)
            continue;
        if (halos->use_fallback) {
            double rfb = 0.0;
            if (p2_gaia_halo_radius_fallback_px(
                    r_px, halos->fallback_factor, halos->fallback_r_max,
                    &rfb) != 0)
                continue;
            r_px = rfb;
        }
        const double rad_deg =
            r_px * halos->anchor_pixel_scale_arcsec / 3600.0 / ratio;
        if (!(rad_deg > 0.0) || !std::isfinite(rad_deg)) continue;
        const double c0 = hra * d2r, d0 = hdec * d2r;
        const double rad = rad_deg * d2r;
        double c = std::sin(d0) * sd + std::cos(d0) * cd * std::cos(r - c0);
        c = std::min(1.0, std::max(-1.0, c));
        if (std::acos(c) <= rad) return true;
    }
    return false;
}
#else
// [晕掩膜证伪停用留桩：原实现见 #if 0 分支；停用期间恒 false，veto 调用点同步注释]
[[maybe_unused]] bool simple_mask_connected_veto(const std::vector<double>&, int,
                                double, double, double,
                                int) {
    return false;
}

// [晕掩膜证伪停用留桩：原实现见 #if 0 分支；停用期间恒 false，veto 调用点同步注释]
[[maybe_unused]] bool gaia_halo_hit(const P2GaiaHaloCaps*, double, double) {
    return false;
}
#endif

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
    c.star_mask_snr_factor = 10.0;      // 与 catalog veto 同口径
    c.star_mask_radius_deg = 0.012;     // 与 veto 半径同口径
    c.cpu_workers = 1;                  // 默认 1(串行 reference); 生产由 p2_session 传 lease
    // 普通星简单掩膜沿用既有两键（CONFIG 普通星组），无新增字段。
    // Gaia 晕组（CONFIG 掩膜三组键文法；PHASE2_SAMPLER.md §5.9）。
    c.halo_mag_thresh = 8.0;
    c.halo_r8 = 150.0;
    c.halo_a = 1.5;
    c.halo_r_min = 30.0;
    c.halo_r_max = 300.0;
    // 缝回退组（CONFIG 掩膜三组键文法；PHASE2_SAMPLER.md §5.10）。
    c.seam_fallback_factor = 1.5;
    c.seam_fallback_r_max = 450.0;
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
    acsd::crypto::Sha256 sha;
    char buf[8192];
    /* properties 是身份载荷的构成部分 (本函数头注: 「关键元数据 + signal/
     * support 像素 + SNR catalogue 内容」)。读取失败 (返回 -1) 时若静默
     * 跳过, 仅元数据 (filter/exptime/creator_did…) 不同的两帧会拿到同一个
     * id, 且与末尾 catch 的 0 哨兵语义不一致 —— sampler.h 的
     * p2_sample_controls_cached 契约明写
     * 「0 视为非法 (p2_frame_id 失败哨兵), 实现将直接拒绝」⇒ fail-closed。*/
    if (aio_hips_get_properties(d, buf, (int)sizeof(buf)) != 0) {
        aio_hips_close(d);
        return 0;
    }
    {
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
        /* signal tile 读取失败 = 身份载荷缺失。静默跳过会产出「部分内容的
         * id」, 与有效帧碰撞且调用方无从分辨 ⇒ fail-closed（d 在本循环
         * 之后关闭, 此处提前返回须自行关闭）。 */
        if (aio_hips_read_tile_f32(d, t, tile_buf.data()) != 0) {
            aio_hips_close(d);
            return 0;
        }
        {
            const std::string pre = std::to_string(t) + "=";
            sha.update(pre.data(), pre.size());
            sha.update(tile_buf.data(), tile_buf.size() * sizeof(float));
            const char semi = ';';
            sha.update(&semi, 1);
        }
    }
    aio_hips_close(d);
    AioHipsDataset* sp = aio_hips_open(hips_path, AIO_HIPS_RD_SUPPORT);
    /* support 面与 signal 面同权：sampler.h 的冻结清单把 support tile 像素列为
     * 身份载荷的构成部分，因此**打开失败**与读取失败是同一类「载荷缺失」。
     * 旧的 `if (sp)` 无 else：打开失败时整段静默跳过，产出「部分内容的 id」，
     * 与有效帧碰撞且调用方无从分辨（sampler.h 规定 0 才是失败哨兵）⇒ 与上面
     * signal 面的 open 臂同风格 fail-closed。 */
    if (!sp) return 0;
    for (std::uint64_t t : tiles) {
        /* 同 signal 面: support tile 读取失败同样 fail-closed。 */
        if (aio_hips_read_tile_f32(sp, t, tile_buf.data()) != 0) {
            aio_hips_close(sp);
            return 0;
        }
        {
            const std::string pre = "S" + std::to_string(t) + "=";
            sha.update(pre.data(), pre.size());
            sha.update(tile_buf.data(), tile_buf.size() * sizeof(float));
            const char semi = ';';
            sha.update(&semi, 1);
        }
    }
    aio_hips_close(sp);
    AioHipsDataset* sn = aio_hips_open(hips_path, AIO_HIPS_RD_SNR);
    /* SNR 面与 signal/support **不同权**：它是可选子产品。证据链：
     * aio_hips.h 的 AIO_HIPS_PRODUCT_SNR 是 flags 位，「未被 flags 选中的通道
     * 不落盘」；Phase1 写侧 module_entry.cpp 的 flags 来自配置，且 SNR 点集
     * 只在 rows.n_snr>0 时写入。⇒ 「目录不存在 ⇒ open 返回 NULL」是**合法态**，
     * 本函数下面的采样实现也按同一口径 `if (snr)` 容忍它。所以这里**不能**照搬
     * signal 面的 fail-closed：那会把「合法地没有 SNR 面」判成失败。
     * 真正需要 fail-closed 的是**读失败**（下面 got<0 那道），不是「面缺失」。 */
    if (sn) {
        const int maxn = 1 << 20;
        std::vector<double> ra(maxn), dec(maxn), snr(maxn);
        std::vector<std::uint32_t> qf(maxn);
        const int got = aio_hips_read_snr_catalog(
            sn, ra.data(), dec.data(), snr.data(), nullptr, qf.data(),
            nullptr, maxn);
        /* 返回值语义（aio_hips_reader.h / aio_hips_reader.cpp 的实现）：
         * >=0 = 实际点数（0 = 目录为空，**合法**）；<0 = 读取失败。
         * 旧的 `if (got > 0)` 把「读失败」与「合法空目录」并成同一条静默跳过 ⇒
         * 这里分开：读失败 fail-closed（载荷缺失），空目录照常哈希。 */
        if (got < 0) {
            aio_hips_close(sn);
            return 0;
        }
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
            const std::string seg = fmt(ra[static_cast<size_t>(i)]) + "," +
                       fmt(dec[static_cast<size_t>(i)]) + "," +
                       fmt(snr[static_cast<size_t>(i)]) + "," +
                       std::to_string(qf[static_cast<size_t>(i)]) + ";";
            sha.update(seg.data(), seg.size());
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
    return 1.482602218505602 * median_of(std::move(v));
}

static int p2_sample_controls_impl(
                      const P2CoverageResult* coverage,
                      const char* const* hips_paths,
                      const std::uint64_t* frame_ids_in,
                      const P2SamplerConfig* cfg_in,
                      const P2GaiaHaloCaps* halos_in,
                      P2ControlObservation* out_obs,
                      std::uint64_t out_capacity,
                      std::uint64_t* out_n_obs,
                      std::uint64_t* out_n_controls,
                      P2SampleStats* out_stats,
                      P2ControlNode* out_controls,
                      std::uint64_t ctrl_capacity,
                      P2SkySample* out_sky,
                      std::uint64_t sky_capacity,
                      std::uint64_t* out_n_sky,
                      P2StarMaskCap* out_mask,
                      std::uint64_t mask_capacity,
                      std::uint64_t* out_n_mask,
                      char* err, std::size_t err_size) {
    if (!coverage || !hips_paths || !out_n_obs || !out_n_controls) {
        if (err && err_size) std::snprintf(err, err_size, "bad args");
        return 1;
    }
    *out_n_obs = 0;
    *out_n_controls = 0;
    if (out_n_sky) *out_n_sky = 0;
    if (out_n_mask) *out_n_mask = 0;
    // [晕掩膜证伪停用：halos_in 暂停消费（simple(6)/halo(7) 分支已注释），保留形参与 with_halos 入口]
    (void)halos_in;
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
    if (!(cfg.star_mask_snr_factor > 0.0)) cfg.star_mask_snr_factor = 10.0;
    if (!(cfg.star_mask_radius_deg > 0.0)) cfg.star_mask_radius_deg = 0.012;
    // task-4 双通道掩膜与 veto 修补（CONFIG 新节口径；与默认值同源）：
    // halo_mag_thresh 须有限（NaN/Inf→默认 8.0）；halo_r8/r_min/r_max 须>0 有限；
    // halo_a 须>1 有限；r_min<=r_max 否则双双回默认；fallback_factor 须>=1 有限；
    // fallback_r_max 须>0 有限。修补只回退非法值，不吞合法显式值。
    if (!std::isfinite(cfg.halo_mag_thresh)) cfg.halo_mag_thresh = 8.0;
    if (!(cfg.halo_r8 > 0.0) || !std::isfinite(cfg.halo_r8))
        cfg.halo_r8 = 150.0;
    if (!(cfg.halo_a > 1.0) || !std::isfinite(cfg.halo_a))
        cfg.halo_a = 1.5;
    if (!(cfg.halo_r_min > 0.0) || !std::isfinite(cfg.halo_r_min) ||
        !(cfg.halo_r_max > 0.0) || !std::isfinite(cfg.halo_r_max) ||
        cfg.halo_r_min > cfg.halo_r_max) {
        cfg.halo_r_min = 30.0;
        cfg.halo_r_max = 300.0;
    }
    if (!(cfg.seam_fallback_factor >= 1.0) ||
        !std::isfinite(cfg.seam_fallback_factor))
        cfg.seam_fallback_factor = 1.5;
    if (!(cfg.seam_fallback_r_max > 0.0) ||
        !std::isfinite(cfg.seam_fallback_r_max))
        cfg.seam_fallback_r_max = 450.0;
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
    // PERF-P2 S2.2（sampler pass1 读路径）：逐帧 setup（open + tile 枚举 +
    // SNR catalogue 读 + ivar 探测 open）按帧并行。位精确论证：
    //   · 每帧只写自己下标的 sig[i]/sup[i]/ivr[i]/frames[i]（disjoint 写）；
    //   · frames[i].tiles 是 std::set<ipix>，插入顺序无关（有序容器，内容恒同）；
    //   · frames[i].snr_* 向量由单次 read_snr_catalog 回填后按 got 截断（与串行同值）；
    //   · kcorr 由本帧 properties 纯函数得出（frame_drizzle_provenance 只读本帧）；
    //   · 失败语义：按帧下标升序取首个失败（= 串行首个失败），错误串逐字一致；
    //     串行 reference = workers=1。stderr 的 k_corr 域外回退行按帧序重排后输出
    //     （内容逐行一致，顺序恒为帧升序 ⇒ 与串行逐字节一致）。
    //   · aio_hips_open/read 系调用方各自 open/read/close 零共享可变状态
    //     （read_tile_pair 注释 PERF-401 同款契约），跨帧并发安全。
    std::vector<AioHipsDataset*> sig(n_frames, nullptr);
    std::vector<AioHipsDataset*> sup(n_frames, nullptr);
    std::vector<AioHipsDataset*> ivr(n_frames, nullptr);   // ivar 产品 (可缺)
    std::vector<FrameData> frames(n_frames);
    {
        const int setup_workers =
            (cfg.cpu_workers > 0) ? cfg.cpu_workers : 1;
        const bool setup_par =
            (setup_workers > 1 && n_frames > 1);
        std::vector<int> setup_rc(n_frames, 0);
        std::vector<std::string> setup_err(n_frames);
        std::vector<std::string> setup_note(n_frames);
        auto setup_frame = [&](std::uint64_t i) {
            sig[i] = aio_hips_open(hips_paths[i], AIO_HIPS_RD_SIGNAL);
            sup[i] = aio_hips_open(hips_paths[i], AIO_HIPS_RD_SUPPORT);
            if (!sig[i] || !sup[i]) {
                setup_rc[i] = 1;
                setup_err[i] = std::string("open frame ") +
                              std::to_string((unsigned long long)i) +
                              " failed: " + aio_hips_reader_last_error();
                if (sig[i]) { aio_hips_close(sig[i]); sig[i] = nullptr; }
                if (sup[i]) { aio_hips_close(sup[i]); sup[i] = nullptr; }
                return;
            }
        // （K_CORR_DOMAIN 选项 B，scale 维已退役）：读帧 Drizzle provenance。
        // 仅当 scale 落在标定域 [300,600]″ 才取逐帧标定值；域外/未知一律保留
        // frames[i].kcorr=0（→ 既有回退链 cfg.control_k_corr，默认即冻结 1.4）
        // 并显式打标 —— 禁止把 300″ 档 clamp 值当生产逐帧 k_corr。
        {
            double pf = 0.0, sc = 0.0;
            frame_drizzle_provenance(hips_paths[i], &pf, &sc);
            const bool in_domain = (sc >= 300.0 && sc <= 600.0);
            if (pf > 0.0 && in_domain) {
                frames[i].kcorr = kcorr_lookup(pf, sc);
            } else if (pf > 0.0) {
                // 并行确定性：stderr 行先攒入 setup_note，join 后按帧序输出
                // （内容与串行逐行一致，顺序恒为帧升序）。
                char nbuf[256];
                std::snprintf(nbuf, sizeof(nbuf),
                              "[sampler] k_corr 域外回退: frame=%llu "
                              "src_pixel_scale=%.6f\"/px ∉ [300,600] → 冻结默认 "
                              "%.4f（scale 维退役，禁 clamp）",
                              (unsigned long long)i, sc,
                              (double)cfg.control_k_corr);
                setup_note[i] = nbuf;
            }
        }
        const int n = aio_hips_tile_count(sig[i]);
        for (int t = 0; t < n; ++t) {
            std::uint64_t ip = 0;
            if (aio_hips_tile_ipix(sig[i], t, &ip) == 0)
                frames[i].tiles.insert(ip);
        }
        // SNR catalogue（质量/可信度场，不参与空间基函数）
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
        };  // setup_frame 结束（disjoint 写；join 后按帧序合并）
        if (setup_par) {
            std::atomic<std::uint64_t> setup_next{0};
            std::vector<std::thread> setup_pool;
            setup_pool.reserve((std::size_t)setup_workers);
            for (int sw = 0; sw < setup_workers; ++sw) {
                setup_pool.emplace_back([&]() {
                    for (;;) {
                        const std::uint64_t ii =
                            setup_next.fetch_add(1);
                        if (ii >= n_frames) break;
                        setup_frame(ii);
                    }
                });
            }
            for (auto& th : setup_pool) th.join();
        } else {
            for (std::uint64_t i = 0; i < n_frames; ++i) setup_frame(i);
        }
        // 失败语义：按帧下标升序取首个失败（= 串行首个失败）。
        for (std::uint64_t i = 0; i < n_frames; ++i) {
            if (!setup_note[i].empty()) {
                std::fprintf(stderr, "%s\n", setup_note[i].c_str());
                std::fflush(stderr);
            }
        }
        for (std::uint64_t i = 0; i < n_frames; ++i) {
            if (setup_rc[i] != 0) {
                if (err && err_size)
                    std::snprintf(err, err_size, "%s",
                                  setup_err[i].c_str());
                for (std::uint64_t j = 0; j < n_frames; ++j) {
                    if (sig[j]) aio_hips_close(sig[j]);
                    if (sup[j]) aio_hips_close(sup[j]);
                }
                return 1;
            }
        }
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
    // P0-08 天光采样点/星点掩膜输出（声明在 try 外，供函数尾部输出）
    const bool want_sky = (out_n_sky != nullptr);
    const bool want_mask = (out_n_mask != nullptr);
    std::vector<P2SkySample> sky;
    std::vector<P2StarMaskCap> mask;
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
        std::vector<int> reason;                  // 0=ok 1..7=原因（6=simple mask，7=gaia halo）
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
    // 消除 64× 重读；并行由 std::thread + Runtime lease 驱动。
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
    if ((std::size_t)n_union * (std::size_t)grid * (std::size_t)grid > (std::size_t)200 * 1000 * 1000) {
        if (err && err_size) std::snprintf(err, err_size, "cells too large %llu", (unsigned long long)((std::size_t)n_union * (std::size_t)grid * (std::size_t)grid));
        for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
        return 1;
    }
    try { cells.resize(n_union * (std::size_t)grid * (std::size_t)grid); } catch (const std::exception& e) {
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
    std::uint64_t sum_simple_mask = 0;
    std::uint64_t sum_gaia_halo = 0;
    std::uint64_t sum_insufficient_support = 0;

    const auto t0 = std::chrono::steady_clock::now();
    std::uint64_t progress = 0;
#if defined(_WIN32) && defined(_MSC_VER)
    // MSVC: __try/__except(SEH) 与 C++ 对象展开互斥(C2712), 改用 C++ try/catch(...)+/EHa
    // catch(...) 在 /EHa 下同样捕获结构化异常(AV), 保留崩溃保护意图。
    try {
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
        AioHipsDataset* sig(std::size_t f) { if (!own) return shared_sig[f]; if (!csig[f]) csig[f] = aio_hips_open(paths[f], AIO_HIPS_RD_SIGNAL); return csig[f]; }
        AioHipsDataset* sup(std::size_t f) { if (!own) return shared_sup[f]; if (!csup[f]) csup[f] = aio_hips_open(paths[f], AIO_HIPS_RD_SUPPORT); return csup[f]; }
        void close_all() { if (!own) return; for (AioHipsDataset* p : csig) if (p) aio_hips_close(p); for (AioHipsDataset* p : csup) if (p) aio_hips_close(p); csig.clear(); csup.clear(); }
    };

    // per-cell body：串行与并行共用（杜绝双份漂移）。返回 0 或错误码(1=pairs resize OOM)。
    auto pass1_cell = [&](std::uint64_t c, SamplerReader& rdr,
                         std::uint64_t& cv, std::uint64_t& ci,
                         std::uint64_t& csm, std::uint64_t& ch) -> int {
        cv = 0; ci = 0; csm = 0; ch = 0;   // 本 cell 输出计数（入口清零）；调用方须用独立局部量接收后累加
        const std::uint64_t tile_ipix = coverage->union_cells[c].ipix;
        {
            const std::uint64_t npix = 12ULL * ((std::uint64_t)1 << (2u * (unsigned)coverage->target_order));
            if (tile_ipix >= npix) {
                std::fprintf(stderr, "[sampler] skip out-of-range tile %llu >= %llu at c=%llu\n", (unsigned long long)tile_ipix, (unsigned long long)npix, (unsigned long long)c); std::fflush(stderr);
                for (int gy = 0; gy < grid; ++gy) for (int gx = 0; gx < grid; ++gx) {
                    const std::size_t idx = (std::size_t)c * (std::size_t)grid * (std::size_t)grid +
                                             (std::size_t)(gy * grid + gx);
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
                const std::uint64_t center_local = acsd::healpix::xy_to_nested_local((unsigned)cx, (unsigned)cy, (unsigned)kTileShift);
                const std::uint64_t center_leaf = leaf_of_tile(tile_ipix, leaf_shift) + center_local;
                double ra_deg = 0.0, dec_deg = 0.0;
                acsd::healpix::pix2ang_nest(1u << (unsigned)(coverage->target_order + leaf_shift), center_leaf, ra_deg, dec_deg);

                CellStat cs;
                cs.ra = ra_deg; cs.dec = dec_deg; cs.leaf = center_leaf;
                cs.tile = (int)tile_ipix; cs.gx = gx; cs.gy = gy;
                std::uint64_t local_veto = 0, local_insupp = 0;
                std::uint64_t local_simple = 0, local_halo = 0;
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
                    // task-4：普通星阈值+8连通 veto 用行主序网格（W=2r+1；
                    // 越界/无效以 NaN 占位，与 vals 过滤口径一致）。
                    const int pw = 2 * r + 1;
                    std::vector<double> patch_grid;
                    try {
                        patch_grid.assign((std::size_t)pw * (std::size_t)pw,
                                          std::numeric_limits<double>::quiet_NaN());
                    } catch (...) {
                        return 1;
                    }
                    double sup_sum = 0.0;
                    std::uint32_t n_valid = 0;
                    for (int dy = -r; dy <= r; ++dy) {
                        for (int dx = -r; dx <= r; ++dx) {
                            const int x = cx + dx;
                            const int y = cy + dy;
                            const std::size_t gi =
                                (std::size_t)(dy + r) * (std::size_t)pw +
                                (std::size_t)(dx + r);
                            if (x < 0 || y < 0 || x >= kTileWidth || y >= kTileWidth) continue;
                            const std::uint64_t z = acsd::healpix::xy_to_nested_local((unsigned)x, (unsigned)y, (unsigned)kTileShift);
                            const std::uint64_t fi_idx = acsd::healpix::nested_local_to_fits_index(z, (unsigned)kTileShift, kTileWidth);
                            if (fi_idx >= tp.signal.size() || fi_idx >= tp.support.size()) continue;
                            const float s = tp.signal[(size_t)fi_idx];
                            const float sp = tp.support[(size_t)fi_idx];
                            if (!std::isfinite(s)) continue;
                            if (!std::isfinite(sp) || sp <= 0.0f) continue;
                            vals.push_back(s);
                            // patch_grid 与 vals 同过滤口径：有效才落值。
                            if (gi < patch_grid.size())
                                patch_grid[gi] = (double)s;
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
                            s0 = 1.482602218505602 * median_of(std::move(dev));
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
                            const double s1 = 1.482602218505602 * median_of(std::move(dev2));
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
                        int veto = 0;       // 0=ok 5=catalog 6=simple 7=halo
                        int veto_reason = 0;
                        if (cfg.background_catalog_veto && !frames[frame_id].snr.empty() && frame_snr_med[frame_id] > 0.0) {
                            // CONFORM-FIX-B-005：catalog veto 必须消费
                            // cfg.star_mask_snr_factor / cfg.star_mask_radius_deg
                            // （sampler.h:56-59 声明、sampler.cpp:318-319/:513-514
                            // 已默认并修补；star_mask 路径 :1133/:1148 已是同款
                            // 消费）。旧实现在此硬编码 10.0 / 0.012 ⇒ 调用方设
                            // cfg 只改变 star_mask、不改变 veto（同文件同一物理量
                            // 两套口径），DISP-P2SMP-004 的「未入配置面」缺口
                            // 实际以「入了但不消费」形态残留。
                            const double thr =
                                cfg.star_mask_snr_factor * frame_snr_med[frame_id];
                            const double rad = cfg.star_mask_radius_deg;
                            if (snr_idx[frame_id].any_above(thr, ra_deg, dec_deg, rad)) { veto = 1; veto_reason = 5; }
                        }
                        // task-4 分支 2：普通星阈值+8连通区 veto（§5.8 第二口径）。
                        // 先后：catalog veto 未命中才执行；互斥 else-if 链。
                        // 阈值复用 background_contamination_sigma，
                        // 最小连通像素数复用 min_samples，不另立配置键。
                        // 晕掩膜证伪停用（保留 catalog(5)；simple(6)/halo(7) 整分支注释，
                        // 只注释不删除；性能优化与 export slim 保留）：
                        // RERUN2/RERUN3 对比洞为次因，缝主因为 M1/M4 混合 pedestal 差。
#if 0
                        if (!veto) {
                            if (simple_mask_connected_veto(
                                    patch_grid, pw, y, sigma,
                                    cfg.background_contamination_sigma,
                                    cfg.min_samples)) {
                                veto = 1;
                                veto_reason = 6;
                            }
                        }
#endif
                        // task-4 分支 3：Gaia 晕帽 veto（§5.9 星等定半径）。
                        // 先后：catalog/simple 均未命中才执行；晕帽由调用方经
                        // P2GaiaHaloCaps 传入（无晕帽=null/0→静默跳过）；
                        // 回退半径（use_fallback）由调用方在缝验收失败→全量重跑
                        // 时置位，本模块不自动放大。
                        // 晕掩膜证伪停用（同上；只注释不删除）。
#if 0
                        if (!veto) {
                            if (gaia_halo_hit(halos_in, ra_deg, dec_deg)) {
                                veto = 1;
                                veto_reason = 7;
                            }
                        }
#endif
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
                        // reason 编码扩展：0=ok 1=support 2=retained 3=tolerance
                        // 4=contamination 5=catalog veto 6=simple mask 7=gaia halo。
                        cs.reason.push_back(veto ? veto_reason : 0);
                        if (veto) {
                            ++local_veto;
                            if (veto_reason == 6) ++local_simple;
                            if (veto_reason == 7) ++local_halo;
                        }
                    }
                }
                const std::size_t idx = (std::size_t)c * (std::size_t)grid * (std::size_t)grid +
                                         (std::size_t)(gy * grid + gx);
                cells[idx] = std::move(cs);
                cv += local_veto;
                ci += local_insupp;
                csm += local_simple;
                ch += local_halo;
            }
        }
        return 0;
    };

    // worker 数：来自 Runtime lease(p2_session 传 cfg.cpu_workers=budget.max_workers)。
    // 无 hardware_concurrency(模块不得自行开线程); 1 => 串行 reference。
    // 生产默认 N-worker 并行(std::thread, 跨 Linux/MSVC 一致); OpenMP 条件已移除。
    const int workers = (cfg.cpu_workers > 0) ? cfg.cpu_workers : 1;
    const bool par = (workers > 1);

    if (par) {
        std::atomic<int> pass1_fail{0};
        std::atomic<std::uint64_t> a_veto{0}, a_insuff{0};
        std::atomic<std::uint64_t> a_simple{0}, a_halo{0};
        std::atomic<std::uint64_t> next_c{0};
        std::vector<std::thread> pool;
        pool.reserve((std::size_t)workers);
        for (int w = 0; w < workers; ++w) {
            pool.emplace_back([&]() {
                SamplerReader rdr; rdr.init_own(hips_paths, n_frames);
                std::uint64_t cv = 0, ci = 0, csm = 0, chl = 0;   // 本 worker 的跨 cell 累加器
                for (;;) {
                    const std::uint64_t c = next_c.fetch_add(1);
                    if (c >= n_union) break;
                    // pass1_cell 的 cv/ci 是本 cell 输出（入口清零）⇒ 必须用
                    // 每次调用独立的局部量接收后再累加（复用累加器只剩最后一个 cell 计数）。
                    std::uint64_t cell_cv = 0, cell_ci = 0, cell_cs = 0, cell_ch = 0;
                    const int cell_rc = pass1_cell(c, rdr, cell_cv, cell_ci, cell_cs, cell_ch); cv += cell_cv; ci += cell_ci; csm += cell_cs; chl += cell_ch;
                    if (cell_rc != 0) { pass1_fail.store(1); break; }
                }
                a_veto.fetch_add(cv);
                a_insuff.fetch_add(ci);
                a_simple.fetch_add(csm);
                a_halo.fetch_add(chl);
                rdr.close_all();
            });
        }
        for (auto& th : pool) th.join();
        sum_catalog_veto += a_veto.load();
        sum_insufficient_support += a_insuff.load();
        sum_simple_mask += a_simple.load();
        sum_gaia_halo += a_halo.load();
        if (pass1_fail.load()) {
            if (err && err_size) std::snprintf(err, err_size, "pass1 pairs resize failed (parallel)");
            for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
            return 1;
        }
    } else
    {
        SamplerReader rdr; rdr.init_shared(sig.data(), sup.data(), n_frames);
        for (std::uint64_t c = 0; c < n_union; ++c) {
            std::uint64_t cv = 0, ci = 0, csm = 0, chl = 0;
            if (pass1_cell(c, rdr, cv, ci, csm, chl) != 0) {
                if (err && err_size) std::snprintf(err, err_size, "pairs resize failed at c=%llu", (unsigned long long)c);
                for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
                return 1;
            }
            sum_catalog_veto += cv;
            sum_insufficient_support += ci;
            sum_simple_mask += csm;
            sum_gaia_halo += chl;
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
    } catch (...) {
        // /EHa: catch(...) 亦捕获结构化异常(AV); _exception_code 仅限 __except, 此处取不到 code
        if (err && err_size)
            std::snprintf(err, err_size, "SEH/C++ exception at %s (AV outside try/catch)", "sampler first pass");
        std::fprintf(stderr, "[sampler] exception caught in first pass\n");
        std::fflush(stderr);
        for (std::uint64_t i = 0; i < n_frames; ++i) { if (sig[i]) aio_hips_close(sig[i]); if (sup[i]) aio_hips_close(sup[i]); if (ivr[i]) aio_hips_close(ivr[i]); }
        return 1;
    }
#endif
    // 空覆盖占位：control_id 仍需覆盖所有 grid
    control_id = cells.size();
    stats.rejected_catalog_veto += sum_catalog_veto;
    stats.rejected_simple_mask += sum_simple_mask;
    stats.rejected_gaia_halo += sum_gaia_halo;
    stats.rejected_insufficient_support += sum_insufficient_support;
    // 补偿：空覆盖 tiles 对应 cells 无 frames，跳过即等于未处理，已占位

    // 第二遍：Stage C DBE-like 局部 tolerance gate
    const int nr = cfg.background_neighbor_radius;
    // 按 tile 分组，邻域只遍历同 tile cells（替代全 cells 扫描）
    std::map<int, std::vector<std::size_t>> tile_cells;
    for (std::size_t ci = 0; ci < cells.size(); ++ci)
        tile_cells[cells[ci].tile].push_back(ci);
    std::vector<std::size_t> tile_cell_list;  // 预取同 tile 列表（当前 cell）
    // pass2 单 cell 体：串行/并行共用（杜绝双份漂移）。只写 cells[ci] 自身的
    // accepted/reason；读侧仅消费 pass1 已冻结字段与同 tile 只读量（m/n_total/
    // frames/gx/gy），与写集不交 ⇒ 按 cell 区间切分无数据竞争。局部计数经引用
    // 返回，调用方（串行直加 / 并行 worker-local 聚合后按序合并）负责合并。
    auto pass2_cell = [&](std::size_t ci, std::uint64_t& l_tol,
                            std::uint64_t& l_con, std::uint64_t& l_ret) {
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
            const double S = 1.482602218505602 * dev[dev.size() / 2];
            if (cs.m[fi] > B + cfg.background_tolerance * S) {
                cs.accepted[fi] = false;
                cs.reason[fi] = 3;
                ++l_tol;
            } else if (cs.bfrac[fi] > cfg.background_max_contamination) {
                cs.accepted[fi] = false;
                cs.reason[fi] = 4;
                ++l_con;
            } else if ((double)cs.n_retained[fi] <
                       cfg.background_min_retained_fraction *
                           (double)cs.n_total[fi]) {
                cs.accepted[fi] = false;
                cs.reason[fi] = 2;
                ++l_ret;
            }
        }
    };
    // pass2 并行（workers 由同一 Runtime lease 驱动；pass1 线程池已 join，
    // 无重叠；UPM 是独立函数调用，函数作用域内建池+join，不共享/不私建
    // 全局池 ⇒ 与 UPM 任务不撞车）。写集按 cell 区间正交、计数 worker-local
    // 聚合后加总（整数加法交换律 ⇒ 位精确）；workers=1 恒走串行 reference。
    {
        const int workers23 = (cfg.cpu_workers > 0) ? cfg.cpu_workers : 1;
        const bool par23 = (workers23 > 1);
        if (par23) {
            std::uint64_t sum_tol = 0, sum_con = 0, sum_ret = 0;
            std::mutex m23;
            std::atomic<std::uint64_t> next_ci{0};
            const std::size_t n_cells = cells.size();
            std::vector<std::thread> pool23;
            pool23.reserve((std::size_t)workers23);
            for (int w = 0; w < workers23; ++w) {
                pool23.emplace_back([&]() {
                    std::uint64_t lt = 0, lc = 0, lr = 0;
                    for (;;) {
                        const std::uint64_t ci =
                            next_ci.fetch_add(1);
                        if (ci >= n_cells) break;
                        pass2_cell((std::size_t)ci, lt, lc, lr);
                    }
                    std::lock_guard<std::mutex> lk(m23);
                    sum_tol += lt;
                    sum_con += lc;
                    sum_ret += lr;
                });
            }
            for (auto& th : pool23) th.join();
            stats.rejected_bright_tolerance += sum_tol;
            stats.rejected_high_contamination += sum_con;
            stats.rejected_insufficient_retained += sum_ret;
        } else {
            for (std::size_t ci = 0; ci < cells.size(); ++ci) {
                std::uint64_t lt = 0, lc = 0, lr = 0;
                pass2_cell(ci, lt, lc, lr);
                stats.rejected_bright_tolerance += lt;
                stats.rejected_high_contamination += lc;
                stats.rejected_insufficient_retained += lr;
            }
        }
    }

    // 第三遍：≥2 帧 clean 的 control 才输出观测；
    // 同时输出 sky_samples（每帧全部 clean 采样点，**含单帧区**，P0-08）。
    // 并行：按 cell 区间切分，worker-local obs/sky 分片 + 计数，结束后按 worker
    // 序依次拼接（obs/sky 元素顺序与串行 ci 递增恒同 ⇒ 位精确）。ivar 读用
    // worker 独立只读句柄（cfitsio 同句柄非线程安全 ⇒ 禁共享 ivr[]；串行路径
    // 继续用共享句柄，逐位不变）。
    struct SamplerIvarReader {
        const char* const* paths = nullptr;
        AioHipsDataset* const* shared = nullptr;
        std::size_t n = 0;
        bool own = false;
        std::vector<AioHipsDataset*> cds;
        void init_shared(AioHipsDataset* const* s, std::size_t n_) {
            shared = s; n = n_; own = false;
        }
        void init_own(const char* const* p, std::size_t n_) {
            paths = p; n = n_; own = true; cds.assign(n, nullptr);
        }
        AioHipsDataset* get(std::size_t f) {
            if (!own) return shared[f];
            if (!cds[f]) cds[f] = aio_hips_open(paths[f], AIO_HIPS_RD_IVAR);
            return cds[f];
        }
        void close_all() {
            if (!own) return;
            for (AioHipsDataset* p : cds) if (p) aio_hips_close(p);
            cds.clear();
        }
    };
    auto pass3_cell = [&](std::size_t ci, SamplerIvarReader& ivrdr,
                          std::vector<P2ControlObservation>& l_obs,
                          std::vector<P2SkySample>& l_sky,
                          std::uint64_t& l_acc_ctl,
                          std::uint64_t& l_ovl,
                          std::uint64_t& l_lt2,
                          std::uint64_t& l_acc_obs,
                          std::uint64_t& l_cand_obs) {
        const CellStat& cs = cells[ci];
        int nclean = 0;
        for (std::size_t fi = 0; fi < cs.frames.size(); ++fi)
            if (cs.accepted[fi]) ++nclean;
        if (nclean == 0) return;
        ++l_acc_ctl;
        if (nclean >= 2) ++l_ovl;
        for (std::size_t fi = 0; fi < cs.frames.size(); ++fi) {
            if (!cs.accepted[fi]) {
                // DISP-P2SMP-002: 本循环**不再**累加
                // rejected_insufficient_retained —— 第二遍（:1049-1055）已在
                // 置 reason=2 的同一分支内对该帧计数一次；此处再计即为双计数
                // （统计面偏差：candidate = accepted + Σrejected 恒等式被破坏）。
                continue;
            }
            if (want_sky) {
                P2SkySample sk{};
                sk.frame_id = fid_cache[static_cast<std::size_t>(cs.frames[fi])];
                sk.control_id = static_cast<std::uint64_t>(ci);
                sk.ra_deg = cs.ra;
                sk.dec_deg = cs.dec;
                sk.value = cs.m[fi];
                sk.variance = cs.cvar[fi];
                // 点 SNR = 该点背景估计自身的信噪比 |value|/σ；无局部 SNR
                // 时置 NO_LOCAL_SNR 标记（帧级回退由消费方决定）。
                sk.snr = (cs.unc[fi] > 0.0) ? std::fabs(cs.m[fi]) / cs.unc[fi] : 0.0;
                sk.flags = cs.snr_avail[fi] ? P2_SKY_FLAG_NONE : P2_SKY_FLAG_NO_LOCAL_SNR;
                l_sky.push_back(sk);
            }
            if (nclean < 2) {
                ++l_lt2;
                continue;
            }
            P2ControlObservation o{};
            o.frame_id = fid_cache[static_cast<std::size_t>(cs.frames[fi])];
            o.control_id = (std::uint64_t)ci;
            o.leaf_ipix = cs.leaf;
            o.ra_deg = cs.ra;
            o.dec_deg = cs.dec;
            o.value = cs.m[fi];
            o.uncertainty = cs.unc[fi];
            // P2b-3: snr=0 stub 修正。局部 catalogue SNR 不可用时
            // 如实写该 control 点自身信噪比 |value|/uncertainty（点 SNR），
            // 不再写 0；snr_available 仍如实标记 catalogue 可用性（0=无局部星点，
            // 见 upm.h:51-54），本字段不参与 science 权重（SCI-UPM-WEIGHT-001）。
            o.snr = cs.snr_avail[fi]
                        ? cs.snr[fi]
                        : ((cs.unc[fi] > 0.0 && std::isfinite(cs.unc[fi]))
                               ? std::fabs(cs.m[fi]) / cs.unc[fi]
                               : 0.0);
            o.snr_available = cs.snr_avail[fi];
            // control estimator 的
            // 统计方差/逆方差（patch median；含 Drizzle 协方差 k_corr）。
            o.control_variance = cs.cvar[fi];
            o.control_ivar = cs.civar[fi];
            // P2b-3: ivar=0 stub 修正。帧 ivar 产品可用时用其
            // 单 leaf 值；缺失时改用该观测的**有效逆方差** 1/uncertainty²
            // (= control_ivar = 1/control_variance，控制估计器方差)，不再写 0。
            // science 权重仍只认 control_ivar（upm.h:40-47 / SCI-UPM-WEIGHT-001）；
            // 本字段仅诊断/下游方差面读取，禁止冒充 Var(control estimator) 之外的语义。
            o.ivar = (cs.civar[fi] > 0.0 && std::isfinite(cs.civar[fi]))
                         ? cs.civar[fi]
                         : 0.0;
            {
                AioHipsDataset* iv = ivr[static_cast<std::size_t>(cs.frames[fi])];
                if (iv) {
                    float v = 0.0f;
                    if (aio_hips_read_leaf_f32(iv, cs.leaf, &v) == 0 &&
                        std::isfinite(v) && v > 0.0f) {
                        o.ivar = (double)v;
                    }
                }
            }
            {
                AioHipsDataset* iv = ivrdr.get(static_cast<std::size_t>(cs.frames[fi]));
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
            l_obs.push_back(o);
            ++l_acc_obs;
            ++l_cand_obs;
        }
    };
    // pass3 调度：串行 reference（workers=1）与并行（worker 区间 + 按序拼接）。
    // obs 递增恒等式（:1504-1511 重算 candidate）不受本遍 obs 累加影响。
    {
        const int workers3 = (cfg.cpu_workers > 0) ? cfg.cpu_workers : 1;
        const bool par3 = (workers3 > 1);
        if (par3) {
            struct Shard3 {
                std::vector<P2ControlObservation> obs;
                std::vector<P2SkySample> sky;
                std::uint64_t acc_ctl = 0, ovl = 0, lt2 = 0;
                std::uint64_t acc_obs = 0, cand_obs = 0;
            };
            std::vector<Shard3> shards((std::size_t)workers3);
            std::vector<std::thread> pool3;
            pool3.reserve((std::size_t)workers3);
            for (int w = 0; w < workers3; ++w) {
                pool3.emplace_back([&, w]() {
                    SamplerIvarReader rdr;
                    rdr.init_own(hips_paths, (std::size_t)n_frames);
                    Shard3& sh = shards[(std::size_t)w];
                    const std::uint64_t n_cells = cells.size();
                    const std::uint64_t b =
                        (n_cells * (std::uint64_t)w) / (std::uint64_t)workers3;
                    const std::uint64_t e =
                        (n_cells * (std::uint64_t)(w + 1)) / (std::uint64_t)workers3;
                    for (std::uint64_t ci = b; ci < e; ++ci)
                        pass3_cell((std::size_t)ci, rdr, sh.obs, sh.sky,
                                   sh.acc_ctl, sh.ovl, sh.lt2,
                                   sh.acc_obs, sh.cand_obs);
                    rdr.close_all();
                });
            }
            for (auto& th : pool3) th.join();
            for (int w = 0; w < workers3; ++w) {
                Shard3& sh = shards[(std::size_t)w];
                obs.insert(obs.end(), sh.obs.begin(), sh.obs.end());
                if (want_sky)
                    sky.insert(sky.end(), sh.sky.begin(), sh.sky.end());
                stats.accepted_controls += sh.acc_ctl;
                stats.overlap_controls += sh.ovl;
                stats.rejected_lt_two_clean_frames += sh.lt2;
                stats.accepted_observations += sh.acc_obs;
                stats.candidate_observations += sh.cand_obs;
            }
        } else {
            SamplerIvarReader rdr;
            rdr.init_shared(ivr.data(), (std::size_t)n_frames);
            for (std::size_t ci = 0; ci < cells.size(); ++ci) {
                std::uint64_t acc_ctl = 0, ovl = 0, lt2 = 0;
                std::uint64_t acc_obs = 0, cand_obs = 0;
                const std::size_t obs0 = obs.size();
                const std::size_t sky0 = sky.size();
                pass3_cell(ci, rdr, obs, sky,
                           acc_ctl, ovl, lt2, acc_obs, cand_obs);
                (void)obs0; (void)sky0;
                stats.accepted_controls += acc_ctl;
                stats.overlap_controls += ovl;
                stats.rejected_lt_two_clean_frames += lt2;
                stats.accepted_observations += acc_obs;
                stats.candidate_observations += cand_obs;
            }
        }
    }

    // 星点掩膜（P0-08）：跨帧去重的球面圆帽（位置量化 1e-4 deg）。
    if (want_mask) {
        std::map<std::pair<long long, long long>, double> uniq;
        for (std::uint64_t i = 0; i < n_frames; ++i) {
            const FrameData& fr = frames[i];
            if (fr.snr.empty()) continue;
            const double thr = cfg.star_mask_snr_factor * frame_snr_med[i];
            for (std::size_t j = 0; j < fr.snr.size(); ++j) {
                if (!std::isfinite(fr.snr[j]) || !(fr.snr[j] > thr)) continue;
                const long long qra = std::llround(fr.snr_ra[j] * 1e4);
                const long long qdec = std::llround(fr.snr_dec[j] * 1e4);
                const std::pair<long long, long long> key(qra, qdec);
                auto it = uniq.find(key);
                if (it == uniq.end() || fr.snr[j] > it->second) uniq[key] = fr.snr[j];
            }
        }
        mask.reserve(uniq.size());
        for (const auto& kv : uniq) {
            P2StarMaskCap cap{};
            cap.ra_deg = static_cast<double>(kv.first.first) * 1e-4;
            cap.dec_deg = static_cast<double>(kv.first.second) * 1e-4;
            cap.radius_deg = cfg.star_mask_radius_deg;
            cap.kind = P2_STAR_MASK_STAR;
            mask.push_back(cap);
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
    if (out_n_sky) *out_n_sky = sky.size();
    if (out_sky) {
        const std::uint64_t n = std::min(sky_capacity, (std::uint64_t)sky.size());
        for (std::uint64_t i = 0; i < n; ++i) out_sky[i] = sky[(size_t)i];
    }
    if (out_n_mask) *out_n_mask = mask.size();
    if (out_mask) {
        const std::uint64_t n = std::min(mask_capacity, (std::uint64_t)mask.size());
        for (std::uint64_t i = 0; i < n; ++i) out_mask[i] = mask[(size_t)i];
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
    return p2_sample_controls_impl(coverage, hips_paths, nullptr, cfg_in, nullptr,
                                   out_obs, out_capacity, out_n_obs,
                                   out_n_controls, out_stats, out_controls,
                                   ctrl_capacity,
                                   nullptr, 0, nullptr, nullptr, 0, nullptr,
                                   err, err_size);
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
    return p2_sample_controls_impl(coverage, hips_paths, frame_ids, cfg_in, nullptr,
                                   out_obs, out_capacity, out_n_obs,
                                   out_n_controls, out_stats, out_controls,
                                   ctrl_capacity,
                                   nullptr, 0, nullptr, nullptr, 0, nullptr,
                                   err, err_size);
}

int p2_sample_controls_cached_with_halos(
                              const P2CoverageResult* coverage,
                              const char* const* hips_paths,
                              const std::uint64_t* frame_ids,
                              const P2SamplerConfig* cfg_in,
                              const P2GaiaHaloCaps* halos,
                              P2ControlObservation* out_obs,
                              std::uint64_t out_capacity,
                              std::uint64_t* out_n_obs,
                              std::uint64_t* out_n_controls,
                              P2SampleStats* out_stats,
                              P2ControlNode* out_controls,
                              std::uint64_t ctrl_capacity,
                              char* err, std::size_t err_size) {
    return p2_sample_controls_impl(coverage, hips_paths, frame_ids, cfg_in, halos,
                                   out_obs, out_capacity, out_n_obs,
                                   out_n_controls, out_stats, out_controls,
                                   ctrl_capacity,
                                   nullptr, 0, nullptr, nullptr, 0, nullptr,
                                   err, err_size);
}

/* RETIRED 2026-09-25 (CHK-PROD-WIRING W1): p2_sample_sky 定义已删（全仓零消费者；同能力变体入口保留在产）。 */

/* RETIRED 2026-09-25 (CHK-PROD-WIRING W1): p2_sample_sky_cached 定义已删（全仓零消费者；同能力变体入口保留在产）。 */

// ===========================================================================
// V6 目标态：空间模型求值 / 空间摘要 / 帧级标量降级门
// 冻结锚见 sampler.h V6 节。纯函数：无 I/O、无全局态、不产生任何权重。
// 缺失/NaN 一律 fail-closed（MISSING_NODE / UNAVAILABLE），禁止零填。
// ===========================================================================

static void p2_spatial_set_err(char* err, std::size_t err_size, const char* msg) {
    if (err == nullptr || err_size == 0) return;
    std::snprintf(err, err_size, "%s", msg ? msg : "");
}

static bool p2_spatial_model_wellformed(const P2SpatialGridModel* m) {
    if (m == nullptr || m->value == nullptr || m->valid == nullptr) return false;
    if (m->nx < 2 || m->ny < 2) return false;
    if (!(m->step_deg > 0.0) || !std::isfinite(m->step_deg)) return false;
    if (!std::isfinite(m->ra0_deg) || !std::isfinite(m->dec0_deg)) return false;
    return true;
}

int p2_spatial_model_eval(const P2SpatialGridModel* model,
                          double ra_deg, double dec_deg,
                          P2SpatialEval* out) {
    if (out == nullptr) return 1;
    out->status = P2_SPATIAL_INVALID_MODEL;
    out->value = std::numeric_limits<double>::quiet_NaN();
    out->n_used_nodes = 0;
    p2_spatial_set_err(out->err, sizeof(out->err), "");
    if (!p2_spatial_model_wellformed(model)) {
        out->status = P2_SPATIAL_INVALID_MODEL;
        p2_spatial_set_err(out->err, sizeof(out->err),
                           "spatial_model_eval: invalid model (null/nx<2/ny<2/step<=0)");
        return 1;
    }
    const double fx = (ra_deg - model->ra0_deg) / model->step_deg;
    const double fy = (dec_deg - model->dec0_deg) / model->step_deg;
    const double xmax = (double)(model->nx - 1);
    const double ymax = (double)(model->ny - 1);
    if (!std::isfinite(fx) || !std::isfinite(fy) || fx < 0.0 || fx > xmax ||
        fy < 0.0 || fy > ymax) {
        out->status = P2_SPATIAL_OUT_OF_DOMAIN;
        p2_spatial_set_err(out->err, sizeof(out->err),
                           "spatial_model_eval: output position outside model "
                           "domain (no extrapolation; fail-closed)");
        return 1;
    }
    // 取包含 fx 的格子 [ix0, ix0+1]；右/上边界钳到最后一格
    // （ix0 <= nx-2），使边界精确取边节点，仍不外插。
    const std::uint64_t ix0 =
        (std::uint64_t)std::min(std::floor(fx), xmax - 1.0);
    const std::uint64_t iy0 =
        (std::uint64_t)std::min(std::floor(fy), ymax - 1.0);
    const double tx = fx - (double)ix0;
    const double ty = fy - (double)iy0;
    const std::uint64_t ix1 = ix0 + 1;
    const std::uint64_t iy1 = iy0 + 1;
    const std::size_t i00 = (std::size_t)(iy0 * model->nx + ix0);
    const std::size_t i10 = (std::size_t)(iy0 * model->nx + ix1);
    const std::size_t i01 = (std::size_t)(iy1 * model->nx + ix0);
    const std::size_t i11 = (std::size_t)(iy1 * model->nx + ix1);
    const bool support_ok =
        model->valid[i00] != 0 && model->valid[i10] != 0 &&
        model->valid[i01] != 0 && model->valid[i11] != 0 &&
        std::isfinite(model->value[i00]) && std::isfinite(model->value[i10]) &&
        std::isfinite(model->value[i01]) && std::isfinite(model->value[i11]);
    if (!support_ok) {
        out->status = P2_SPATIAL_MISSING_NODE;
        p2_spatial_set_err(out->err, sizeof(out->err),
                           "spatial_model_eval: bilinear support node missing/NaN "
                           "(no zero-fill, no constant fallback)");
        return 1;
    }
    const double v00 = model->value[i00];
    const double v10 = model->value[i10];
    const double v01 = model->value[i01];
    const double v11 = model->value[i11];
    const double v = (1.0 - tx) * (1.0 - ty) * v00 +
                     tx * (1.0 - ty) * v10 +
                     (1.0 - tx) * ty * v01 +
                     tx * ty * v11;
    out->status = P2_SPATIAL_OK;
    out->value = v;
    out->n_used_nodes = 4;
    return 0;
}

int p2_spatial_model_summary(const P2SpatialGridModel* model,
                             P2SpatialSummary* out,
                             char* err, std::size_t err_size) {
    if (out == nullptr) return 1;
    std::memset(out, 0, sizeof(*out));
    if (!p2_spatial_model_wellformed(model)) {
        p2_spatial_set_err(err, err_size,
                           "spatial_model_summary: invalid model");
        return 1;
    }
    const std::uint64_t n = model->nx * model->ny;
    std::vector<double> vals;
    vals.reserve((std::size_t)n);
    for (std::uint64_t k = 0; k < n; ++k) {
        const std::size_t s = (std::size_t)k;
        if (model->valid[s] == 0) continue;   // 缺失跳过（不参与统计），非零填
        const double v = model->value[s];
        if (!std::isfinite(v)) {
            p2_spatial_set_err(err, err_size,
                               "spatial_model_summary: valid node has non-finite value "
                               "(fail-closed)");
            return 1;
        }
        vals.push_back(v);
    }
    if (vals.empty()) {
        p2_spatial_set_err(err, err_size,
                           "spatial_model_summary: no valid nodes");
        return 1;
    }
    std::sort(vals.begin(), vals.end());
    const std::size_t m = vals.size();
    auto quantile = [&](double p) -> double {
        double idx = std::floor(p * (double)(m - 1));
        if (idx < 0.0) idx = 0.0;
        std::size_t i = (std::size_t)idx;
        if (i >= m) i = m - 1;
        return vals[i];
    };
    out->n_nodes = n;
    out->n_valid = (std::uint64_t)m;
    out->p05 = quantile(0.05);
    out->p50 = quantile(0.50);
    out->p95 = quantile(0.95);
    double maxdev = 0.0;
    double sse = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        const double d = vals[i] - out->p50;
        const double ad = std::fabs(d);
        if (ad > maxdev) maxdev = ad;
        sse += d * d;
    }
    out->max_systematic_deviation = maxdev;
    out->model_error = std::sqrt(sse / (double)m);
    out->sampling_coverage = (double)m / (double)n;
    return 0;
}

int p2_scalar_degrade_gate(const P2ScalarGateThresholds* th,
                           const P2ScalarSummaryInput* s,
                           P2ScalarDegradeVerdict* out_verdict,
                           char* err, std::size_t err_size) {
    if (th == nullptr || s == nullptr || out_verdict == nullptr) return 2;
    auto fail = [&](P2ScalarDegradeVerdict v, const char* msg) -> int {
        *out_verdict = v;
        if (err != nullptr && err_size > 0) {
            std::snprintf(err, err_size, "%s", msg ? msg : "");
        }
        return 1;
    };
    if (!th->thresholds_declared) {
        return fail(P2_SCALAR_UNAVAILABLE,
                    "scalar_degrade: thresholds not declared (residual/trend and "
                    "power-loss thresholds are SO-07 PENDING_OWNER_SIGNOFF); "
                    "fail-closed, keep map/model/control points");
    }
    if (!(th->residual_trend_max > 0.0) || !std::isfinite(th->residual_trend_max) ||
        !(th->power_loss_max > 0.0) || !std::isfinite(th->power_loss_max)) {
        return fail(P2_SCALAR_UNAVAILABLE,
                    "scalar_degrade: invalid thresholds (must be finite and >0; "
                    "no relaxation permitted)");
    }
    if (!s->summary_complete) {
        return fail(P2_SCALAR_UNAVAILABLE,
                    "scalar_degrade: scalar summary incomplete (p05/p50/p95, max "
                    "systematic deviation, sampling coverage, model error and "
                    "applicability domain all required)");
    }
    if (!std::isfinite(s->p05) || !std::isfinite(s->p50) || !std::isfinite(s->p95) ||
        !(s->p05 <= s->p50) || !(s->p50 <= s->p95)) {
        return fail(P2_SCALAR_UNAVAILABLE,
                    "scalar_degrade: invalid quantiles (finite p05<=p50<=p95 required)");
    }
    if (!std::isfinite(s->sampling_coverage) || s->sampling_coverage < 0.0 ||
        s->sampling_coverage > 1.0) {
        return fail(P2_SCALAR_UNAVAILABLE,
                    "scalar_degrade: sampling_coverage outside [0,1]");
    }
    if (!std::isfinite(s->model_error) || s->model_error < 0.0) {
        return fail(P2_SCALAR_UNAVAILABLE, "scalar_degrade: invalid model_error");
    }
    if (!std::isfinite(s->max_systematic_deviation) ||
        s->max_systematic_deviation < 0.0) {
        return fail(P2_SCALAR_UNAVAILABLE,
                    "scalar_degrade: invalid max_systematic_deviation");
    }
    if (s->applicability_domain[0] == '\0') {
        return fail(P2_SCALAR_UNAVAILABLE,
                    "scalar_degrade: empty applicability_domain");
    }
    if (!std::isfinite(s->spatial_residual_p95) || !std::isfinite(s->spatial_trend) ||
        !std::isfinite(s->power_loss)) {
        return fail(P2_SCALAR_UNAVAILABLE,
                    "scalar_degrade: non-finite gate statistics");
    }
    if (s->spatial_residual_p95 > th->residual_trend_max ||
        s->spatial_trend > th->residual_trend_max) {
        return fail(P2_SCALAR_RETAIN_SPATIAL,
                    "scalar_degrade: spatial residual/trend gate FAILED -> keep "
                    "map/model/control points");
    }
    if (s->power_loss > th->power_loss_max) {
        return fail(P2_SCALAR_RETAIN_SPATIAL,
                    "scalar_degrade: power-loss gate FAILED -> keep "
                    "map/model/control points");
    }
    *out_verdict = P2_SCALAR_ALLOWED;
    if (err != nullptr && err_size > 0) err[0] = '\0';
    return 0;
}

} // extern "C"

// ── 测试接缝 (ALG-P2-SMP-001 §11.3 F2) ──
// 仅供内部测试驱动匿名命名空间的 kcorr_lookup：与生产同一实现、同一冻结
// 表值；不进入任何产品入口、不改变导出符号面（static 库内部）。
extern "C" double acsd_phase2_kcorr_lookup_for_test(double pixfrac,
                                                       double scale_arcsec) {
    return kcorr_lookup(pixfrac, scale_arcsec);
}
