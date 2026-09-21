// P1-HIPS-TEST · oracle 组 (期望值由独立 oracle 生成, 非被测函数)
//
// 合同锚 HIPS_WRITER.md §9: "oracle 不调用被测函数、不复制同一实现;
// 可用解析解、高精度朴素实现或许可隔离的测试参考库"。
//   O1 FITS 排列全量对拍: fixture NESTED 注入逐像素唯一值 → oracle
//      独立位解交织重排 (local_to_fits_index=(511−x)·512+y, DATA_SEMANTICS
//      §3) → 与磁盘 FITS 读回逐像素 bitwise (f64) —— 期望值完全由
//      oracle 生成;
//   O2 MOC UNIQ 独立式 (IVOA MOC 1.1 uniq=4·4^order+ipix>>2Δ) + MOCORDER;
//   O3 SNR cell 归属弱 oracle: 独立球面角距离 + order-K cell 角尺度
//      解析上界 (不调用被测/共享地址生成);
//   O4 hips_pixel_scale 独立解析复算 (%.6f 双侧一致);
//   O5 variance/ivar 互倒 (I4 有限域) f64 bitwise + f32 rtol;
//   O6 moc_sky_fraction 序列化判别面: 非平凡点 1/12 (N=1, K=0) 上
//      回程精确 + §9 <1e-9 绝对容差 + properties↔manifest 逐字符一致
//      (修复前 std::to_string 6dp 在 1/12 点超容差 333 倍且两面字面量分叉)。
#include "p1hips_test_main.hpp"
#include "p1hips_fixtures.hpp"
#include "p1hips_oracle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "aio_hips.h"

using namespace p1hips;
using namespace p1hips::oracle;

namespace {

CheckState g_cs;

// ---------------------------------------------------------------------------
// O7 (FIX-403 / GAP_AUDIT G3-3 / DISP-HIPS-009) 深层级 hierarchy 累加 oracle
// ---------------------------------------------------------------------------
// 判别面构造 (为什么这样取参数):
//   hierarchy 父 cell 的累加次数 = 4^dk (dk = K−k), 与叶 tile 数无关 —— 单个
//   叶 tile 在 dk 层就向同一父像素贡献 4^dk 次顺序累加; dk=9 即 2^18 次。
//   故 fixture 取 K=10 (nside=2^19) 覆盖 dk=1..10, 2 个叶 tile (parent 0/1,
//   同一 order-0 祖先 A=0) 使 s 偏移路径亦被覆盖。
//   非退化前提: 常数/可精确表示的 sig·area 在 f32 下累加亦精确 (恒绿无判别
//   力) ⇒ 必须用满尾数的异构 sig/cov (splitmix64 固定 seed); cov∈[0.15,0.40)
//   保证 Σa/A_cell_k ≤ 2·0.40 < 1, 不触 support 钳制, 使"父子层积分通量"可由
//   发布面 sig·sup·A_cell 重建 (否则编码限使守恒式不成立)。
constexpr std::uint32_t O7_LEAF_ORDER = 19;                     // nside = 2^19
constexpr std::uint32_t O7_TILE_ORDER = O7_LEAF_ORDER - 9u;     // K = 10
constexpr std::uint32_t O7_NSIDE = 1u << O7_LEAF_ORDER;

struct O7Fix {
    std::vector<float> flux_f;
    std::vector<float> area_f;
    AstroSphereTileView view{};
};

inline O7Fix o7_make_tile(std::uint64_t parent_ipix, std::uint64_t seed) {
    O7Fix f;
    aio_hips_tile_view_abi_init(&f.view);
    const double a_cell = fix_a_cell_sr(O7_NSIDE);
    SplitMix64 rng(seed);
    f.flux_f.resize(FIX_NPIX);
    f.area_f.resize(FIX_NPIX);
    for (std::size_t i = 0; i < FIX_NPIX; ++i) {
        const double cov = 0.15 + 0.25 * rng.unit();   // 覆盖因子 (未钳制)
        const double sig = 0.5 + 2.5 * rng.unit();     // 满尾数异构面亮度
        const double a = cov * a_cell;
        f.area_f[i] = (float)a;
        f.flux_f[i] = (float)(sig * a);
    }
    f.view.parent_ipix = parent_ipix;
    f.view.leaf_order = O7_LEAF_ORDER;
    f.view.width = 512;
    f.view.data_type = AIO_HIPS_FLOAT32;
    f.view.flux_sum = f.flux_f.data();
    f.view.covered_area = f.area_f.data();
    f.view.valid_mask = nullptr;
    f.view.var_num_sum = nullptr;
    return f;
}

// vector 迁移后重绑 view 裸指针 (禁止悬垂)
inline void o7_rebind(std::vector<O7Fix>& v) {
    for (auto& f : v) {
        f.view.flux_sum = f.flux_f.data();
        f.view.covered_area = f.area_f.data();
    }
}

inline std::string o7_tile_path(const std::string& root, const char* product,
                                std::uint32_t order, std::uint64_t ipix) {
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s/%s/Norder%u/Dir%llu/Npix%llu.fits",
                  root.c_str(), product, (unsigned)order,
                  (unsigned long long)((ipix / 10000ULL) * 10000ULL),
                  (unsigned long long)ipix);
    return std::string(buf);
}

// 进程 RSS (kB); 非 Linux → -1 (仅作记录, 不参与判据)
inline long o7_rss_kb() {
#if defined(__linux__)
    FILE* f = std::fopen("/proc/self/status", "rb");
    if (!f) return -1;
    char line[256];
    long kb = -1;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "VmRSS:", 6) == 0) {
            kb = std::strtol(line + 6, nullptr, 10);
            break;
        }
    }
    std::fclose(f);
    return kb;
#else
    return -1;
#endif
}

// rss_after_writes != nullptr 时回填"两个叶 tile 写完、finalize 之前"的 RSS
// (此刻 K 个祖先累加器已全部分配, 是内存增量的观测点)。
inline bool o7_write_product(const std::string& dir, const std::vector<O7Fix>& tiles,
                             std::string& err, long* rss_after_writes = nullptr) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        dir.c_str(), O7_NSIDE, 512, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
        "ivo://astrocs/test/p1hips", "o7", nullptr, 0.0, "2026-09-20T00:00:00Z", 0);
    if (!ps) { err = aio_hips_last_error(); return false; }
    for (const auto& t : tiles) {
        const int rc = aio_hips_write_signal_support_tile(ps, &t.view);
        if (rc != 0) { err = aio_hips_last_error(); return false; }
    }
    if (rss_after_writes) *rss_after_writes = o7_rss_kb();
    const int frc = aio_hips_finalize(ps);
    if (frc != 0) { err = aio_hips_last_error(); return false; }
    return true;
}

struct O7Result {
    double dev_sig[11] = {0.0};   // 索引 = dk (1..10): max 相对偏差 (发布 vs oracle)
    double dev_sup[11] = {0.0};
    double cons[11] = {0.0};      // 逐层积分通量守恒 |F_k − F_leaf| / F_leaf
    double leaf_flux = 0.0;
    bool nodata_ok = true;        // 无贡献父 cell 必须 sig=NaN / sup=0 (DISP-HIPS-011)
    bool read_ok = true;
};

// 独立 f64 参考实现 (不调用生产 symbol, 不复制源码): 由**发布叶 tile** 读回的
// signal + fixture 输入未钳制真实覆盖面积, 按 HEALPix NESTED 四叉定义
//   order-(k+9) 父 cell z 收 s 固定时高 2·dk 位相同的 order-(K+9) 叶像素,
//   z = ((s << 18) | i) >> 2·dk,  s = parent_ipix & (4^dk − 1)
// 独立重算每层父 tile 逐像素期望值, 与发布 hierarchy tile 对拍; 同时给出逐层
// 积分通量守恒 (发布面重建 Σ_z sig·sup·A_cell_k vs 叶级 Σ sig·a)。
inline O7Result o7_oracle(const std::string& dir, const std::vector<O7Fix>& tiles) {
    O7Result r;
    const std::size_t n = FIX_NPIX;
    const std::size_t nt = tiles.size();
    std::vector<std::vector<float>> lsig(nt);
    for (std::size_t t = 0; t < nt; ++t) {
        TileFits lf;
        if (!read_tile_fits(o7_tile_path(dir, "signal", O7_TILE_ORDER,
                                         tiles[t].view.parent_ipix), lf) ||
            lf.bitpix != -32 || lf.pix_f.size() != n) {
            r.read_ok = false;
            return r;
        }
        lsig[t] = lf.pix_f;
    }
    // 叶级积分通量 (f64, tile 序 × 像素序 = 生产同序)
    for (std::size_t t = 0; t < nt; ++t) {
        for (std::size_t i = 0; i < n; ++i) {
            const double sv = (double)lsig[t][local_to_fits_index(i)];
            const double a = (double)tiles[t].area_f[i];
            if (!(a > 0.0) || !std::isfinite(sv)) continue;
            r.leaf_flux += sv * a;
        }
    }
    for (int k = (int)O7_TILE_ORDER - 1; k >= 0; --k) {
        const int dk = (int)O7_TILE_ORDER - k;
        const std::uint64_t span = (1ULL << (2 * dk)) - 1ULL;
        std::vector<double> fs(n, 0.0), as(n, 0.0);
        for (std::size_t t = 0; t < nt; ++t) {
            const std::uint64_t s = tiles[t].view.parent_ipix & span;
            for (std::size_t i = 0; i < n; ++i) {
                const double sv = (double)lsig[t][local_to_fits_index(i)];
                const double a = (double)tiles[t].area_f[i];
                if (!(a > 0.0) || !std::isfinite(sv)) continue;
                const std::size_t z =
                    (std::size_t)(((s << 18) | (std::uint64_t)i) >> (2 * dk));
                fs[z] += sv * a;
                as[z] += a;
            }
        }
        const std::uint32_t nside_k = 1u << (k + 9);
        const double a_cell_k = fix_a_cell_sr(nside_k);
        TileFits psig, psup;
        if (!read_tile_fits(o7_tile_path(dir, "signal", (std::uint32_t)k, 0), psig) ||
            !read_tile_fits(o7_tile_path(dir, "support", (std::uint32_t)k, 0), psup) ||
            psig.bitpix != -32 || psup.bitpix != -32 ||
            psig.pix_f.size() != n || psup.pix_f.size() != n) {
            r.read_ok = false;
            return r;
        }
        double ms = 0.0, mp = 0.0, fsum = 0.0;
        bool fsum_bad = false;
        for (std::size_t z = 0; z < n; ++z) {
            const std::size_t fi = (std::size_t)local_to_fits_index(z);
            const double gs = (double)psig.pix_f[fi];
            const double gp = (double)psup.pix_f[fi];
            double ws = std::numeric_limits<double>::quiet_NaN(), wp = 0.0;
            if (as[z] > 0.0 && std::isfinite(fs[z])) {
                ws = fs[z] / as[z];
                wp = as[z] / a_cell_k;
                if (wp > 1.0) wp = 1.0;
            } else if (!std::isnan(gs) || gp != 0.0) {
                r.nodata_ok = false;   // 无贡献父 cell 必须 NaN/0
            }
            if (!std::isfinite(ws)) continue;
            // 非有限发布值不得静默算作 0 偏差 (判据非退化)
            const double d = std::isfinite(gs)
                                 ? std::fabs(gs - ws) / std::fabs(ws)
                                 : std::numeric_limits<double>::infinity();
            if (d > ms) ms = d;
            const double dp = std::isfinite(gp)
                                  ? ((wp > 0.0) ? std::fabs(gp - wp) / wp
                                                : (gp == 0.0 ? 0.0
                                                             : std::numeric_limits<double>::infinity()))
                                  : std::numeric_limits<double>::infinity();
            if (dp > mp) mp = dp;
            if (std::isfinite(gs) && std::isfinite(gp)) fsum += gs * gp * a_cell_k;
            else fsum_bad = true;
        }
        r.dev_sig[dk] = ms;
        r.dev_sup[dk] = mp;
        r.cons[dk] = (r.leaf_flux > 0.0 && !fsum_bad)
                         ? std::fabs(fsum - r.leaf_flux) / r.leaf_flux
                         : std::numeric_limits<double>::infinity();
    }
    return r;
}

inline void o7_set_f32_accum_env(bool on) {
#if defined(_WIN32)
    _putenv_s("ASTROCS_HIPS_HIER_FAULT", on ? "f32_accum" : "");
#else
    if (on) setenv("ASTROCS_HIPS_HIER_FAULT", "f32_accum", 1);
    else    unsetenv("ASTROCS_HIPS_HIER_FAULT");
#endif
}

}  // namespace

namespace p1hips {

int test_oracle() {
    CheckState& cs = g_cs;
    cs.failures = 0;
    cs.fault_reported = false;

    // --- O1: FITS 排列全量对拍 (f64, 期望由 oracle 重排生成)
    {
        const std::string dir = make_tmp_dir("o1");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://astrocs/test/p1hips", "o1", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "o1_begin");
        // fixture: NESTED local i 注入解析唯一值 v(i)=sin(i)*1e3+i (可逆)
        FixViewF64 fx;
        fx.flux_sum.resize(FIX_NPIX);
        fx.covered_area.assign(FIX_NPIX, fix_a_cell_sr(FIX_NSIDE) * 0.5);
        fx.valid_mask.assign(FIX_NPIX, 1);
        for (std::size_t i = 0; i < FIX_NPIX; ++i)
            fx.flux_sum[i] = std::sin((double)i) * 1000.0 + (double)i;
        aio_hips_tile_view_abi_init(&fx.view);   // ABI 自描述 (V11-N-01)
        fx.view.parent_ipix = 0;
        fx.view.leaf_order = FIX_LEAF_ORDER;
        fx.view.width = FIX_WIDTH;
        fx.view.data_type = AIO_HIPS_FLOAT64;
        fx.view.flux_sum = fx.flux_sum.data();
        fx.view.covered_area = fx.covered_area.data();
        fx.view.valid_mask = fx.valid_mask.data();
        fx.view.var_num_sum = nullptr;
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        TileFits sig;
        if (read_tile_fits(dir + "/signal/Norder0/Dir0/Npix0.fits", sig)) {
            // oracle 期望: signal = flux/area, 经独立重排映射到 FITS 序
            std::vector<double> want(FIX_NPIX, 0.0);
            for (std::size_t i = 0; i < FIX_NPIX; ++i)
                want[local_to_fits_index(i)] = fx.flux_sum[i] / fx.covered_area[i];
            bool bit_ok = true;
            std::size_t first_bad = (std::size_t)-1;
            for (std::size_t fi = 0; fi < want.size(); ++fi) {
                if (!bits_eq_d(sig.pix_d[fi], want[fi])) {
                    bit_ok = false;
                    first_bad = fi;
                    break;
                }
            }
            P1HIPS_CHECK_MSG(cs, bit_ok, "o1_fits_mapping_bitwise",
                             "FITS 排列 oracle 对拍失败 (首错 fi=%zu)", first_bad);
            // 双向自洽 (oracle 内部性质: local→fits→local 幂等)
            bool inv_ok = true;
            for (std::uint64_t i = 0; i < FIX_NPIX; i += 997ULL) {
                if (fits_index_to_local(local_to_fits_index(i)) != i) { inv_ok = false; break; }
            }
            P1HIPS_CHECK(cs, inv_ok, "o1_mapping_involution");
        } else {
            P1HIPS_CHECK(cs, false, "o1_readback");
        }
    }

    // --- O2: MOC UNIQ 独立式 + MOCORDER (order K=0, 3 cells)
    {
        const std::string dir = make_tmp_dir("o2");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL, "ivo://astrocs/test/p1hips", "o2",
            nullptr, 0.0, nullptr, 0);
        if (ps) {
            for (std::uint64_t p : {7ULL, 3ULL, 11ULL}) {
                FixViewF64 fx = fix_hips_a_tile(p, 10.0, 0.5, 0.0, true, false);
                P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            }
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        } else {
            P1HIPS_CHECK(cs, false, "o2_begin");
        }
        std::vector<std::uint64_t> uniq;
        if (read_moc_uniq(dir + "/signal/Moc.fits", uniq)) {
            std::vector<std::uint64_t> want;
            for (std::uint64_t p : {7ULL, 3ULL, 11ULL}) want.push_back(moc_uniq(p, 0, 0));
            std::sort(uniq.begin(), uniq.end());
            std::sort(want.begin(), want.end());
            P1HIPS_CHECK(cs, uniq == want, "o2_moc_uniq_exact");
        } else {
            P1HIPS_CHECK(cs, false, "o2_moc_read");
        }
        // sky fraction: 3/12 cells → |frac−0.25| < 1e-9 + 回程精确 + 双面一致。
        // 3/12=0.25 是可精确表示的有限小数 ⇒ 对序列化精度无判别力; 非平凡
        // 判别点 (1/12) 见 O6。此处的双面逐字符一致面在修复前必红 (properties
        // 6dp "0.250000" vs manifest 8dp "0.25000000")。
        const auto kv = read_properties(dir + "/signal/properties");
        {
            const auto it = kv.find("moc_sky_fraction");
            if (it != kv.end()) {
                const double got = std::strtod(it->second.c_str(), nullptr);
                P1HIPS_CHECK(cs, std::fabs(got - 3.0 / 12.0) < 1e-9, "o2_skyfrac_1e9");
                P1HIPS_CHECK(cs, got == 3.0 / 12.0, "o2_skyfrac_roundtrip_exact");
            } else {
                P1HIPS_CHECK(cs, false, "o2_skyfrac_key");
            }
            std::string mtxt, mlit;
            const bool mok = read_manifest(dir + "/manifest.json", mtxt) &&
                             manifest_get(mtxt, "moc_sky_fraction", mlit);
            P1HIPS_CHECK_MSG(cs, it != kv.end() && mok && it->second == mlit,
                             "o2_skyfrac_double_face",
                             "properties/manifest 字面量分叉 (prop=%s manifest=%s)",
                             it != kv.end() ? it->second.c_str() : "(missing)",
                             mok ? mlit.c_str() : "(missing)");
        }
    }

    // --- O3: SNR cell 归属弱 oracle (独立角距离 + cell 尺寸上界)
    {
        const std::string dir = make_tmp_dir("o3");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64, AIO_HIPS_PRODUCT_SNR,
            "ivo://astrocs/test/p1hips", "o3", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "o3_begin");
        std::vector<FixSnrPointF> pts = fix_hips_d_snr_points(777u, 200);
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(ps, pts.data(), (int)pts.size()), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        // 每文件 (cell) 内两两角距离 ≤ 解析上界; 同点不跨文件
        std::vector<std::string> files;
        walk_dir(dir + "/snr/Norder0", files);
        P1HIPS_CHECK(cs, !files.empty(), "o3_tiles_exist");
        const double ub_deg = snr_cell_diameter_deg_ub(0);   // K=0 → 12 cells
        bool dist_ok = true;
        for (const auto& f : files) {
            std::vector<SnrRow> rows;
            if (!read_snr_tsv(f, rows)) { dist_ok = false; break; }
            for (std::size_t a = 0; a < rows.size(); ++a)
                for (std::size_t b = a + 1; b < rows.size(); ++b) {
                    const double d = ang_dist_deg(rows[a].ra, rows[a].dec,
                                                  rows[b].ra, rows[b].dec);
                    if (d > ub_deg) { dist_ok = false; break; }
                }
        }
        P1HIPS_CHECK_MSG(cs, dist_ok, "o3_snr_cell_bound",
                         "SNR cell 内点对角距离超解析上界 %.3f°", ub_deg);
    }

    // --- O4: hips_pixel_scale 独立解析复算
    {
        const std::string dir = make_tmp_dir("o4");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL, "ivo://astrocs/test/p1hips", "o4",
            nullptr, 0.0, nullptr, 0);
        if (ps) {
            FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, false);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        } else {
            P1HIPS_CHECK(cs, false, "o4_begin");
        }
        // M2b-B-03: IVOA REC-HIPS-1.0 §4.4.1 定义 hips_pixel_scale 单位为**度**。
        // 独立解析复算: pixel_scale(deg) = (180/π)·sqrt(π/3)/nside。
        const double scale = (180.0 / kPi) * std::sqrt(kPi / 3.0) / 512.0;
        char want[32];
        std::snprintf(want, sizeof(want), "%.6f", scale);
        const auto kv = read_properties(dir + "/signal/properties");
        const auto it = kv.find("hips_pixel_scale");
        P1HIPS_CHECK_MSG(cs, it != kv.end() && it->second == want, "o4_pixel_scale",
                         "hips_pixel_scale got=%s want=%s",
                         it == kv.end() ? "<missing>" : it->second.c_str(), want);
        // 负例 (可判红): 角秒口径 (3600×) 必须**不**成立, 否则单位断言无判别力。
        char want_arcsec[32];
        std::snprintf(want_arcsec, sizeof(want_arcsec), "%.6f", scale * 3600.0);
        P1HIPS_CHECK_MSG(cs, !(it != kv.end() && it->second == want_arcsec),
                         "o4_pixel_scale_neg", "hips_pixel_scale 仍是角秒口径 %s",
                         want_arcsec);
    }

    // --- O5: variance/ivar 互倒 (I4): f64 bitwise + f32 rtol
    {
        const std::string d64 = make_tmp_dir("o5a");
        AioHipsProductSet* ps = aio_hips_product_begin(
            d64.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR,
            "ivo://astrocs/test/p1hips", "o5", nullptr, 0.0, nullptr, 0);
        if (ps) {
            FixViewF64 fx = fix_hips_e_half_var_tile(0, 10.0, 0.5, 1.5);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_variance_tile(ps, &fx.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        } else {
            P1HIPS_CHECK(cs, false, "o5_begin_f64");
        }
        TileFits var, ivar;
        if (read_tile_fits(d64 + "/variance/Norder0/Dir0/Npix0.fits", var) &&
            read_tile_fits(d64 + "/ivar/Norder0/Dir0/Npix0.fits", ivar)) {
            bool inv_ok = true, nan_ok = true;
            for (std::size_t i = 0; i < var.pix_d.size(); ++i) {
                const double v = var.pix_d[i], iv = ivar.pix_d[i];
                if (std::isnan(v)) {
                    if (!std::isnan(iv)) { nan_ok = false; break; }   // I3 一致
                    continue;
                }
                // f64: ivar == 1/variance bitwise (生产同式独立复算)
                if (!bits_eq_d(iv, 1.0 / v)) { inv_ok = false; break; }
            }
            P1HIPS_CHECK(cs, inv_ok, "o5_ivar_inverse_bitwise");
            P1HIPS_CHECK(cs, nan_ok, "o5_nan_pair");
        } else {
            P1HIPS_CHECK(cs, false, "o5_readback");
        }
    }

    // --- O6: moc_sky_fraction 序列化精度判别面 (M2b-H-01): 非平凡点 1/12
    //   N=1 cell @ K=0 ⇒ 数学真值 1/12 (十进制非有限小数)。
    //   (a) 回程精确 strtod(literal)==1/12 (要求 ≥17 有效位);
    //   (b) §9 冻结容差 |literal − 1/12| < 1e-9 (6dp 偏 3.333e-7 = 333 倍);
    //   (c) 双面一致 properties == manifest.json 逐字符。
    {
        const std::string dir = make_tmp_dir("o6");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL, "ivo://astrocs/test/p1hips", "o6",
            nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "o6_begin");
        if (ps) {
            FixViewF64 fx = fix_hips_a_tile(4, 10.0, 0.5, 0.0, true, false);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        const double want = 1.0 / 12.0;   // 独立解析真值 (N=1 cell, K=0)
        const auto kv = read_properties(dir + "/signal/properties");
        const auto it = kv.find("moc_sky_fraction");
        P1HIPS_CHECK(cs, it != kv.end(), "o6_skyfrac_key");
        if (it != kv.end()) {
            const double got = std::strtod(it->second.c_str(), nullptr);
            P1HIPS_CHECK_MSG(cs, std::fabs(got - want) < 1e-9, "o6_skyfrac_1e9_nontrivial",
                             "1/12 点容差失败: literal=%s got=%.17g err=%.3e",
                             it->second.c_str(), got, std::fabs(got - want));
            P1HIPS_CHECK_MSG(cs, got == want, "o6_skyfrac_roundtrip_exact",
                             "字面量非 round-trip 精确: literal=%s got=%.17g want=%.17g",
                             it->second.c_str(), got, want);
            std::string mtxt, mlit;
            const bool mok = read_manifest(dir + "/manifest.json", mtxt) &&
                             manifest_get(mtxt, "moc_sky_fraction", mlit);
            P1HIPS_CHECK_MSG(cs, mok && mlit == it->second, "o6_skyfrac_double_face",
                             "properties/manifest 字面量分叉 (prop=%s manifest=%s)",
                             it->second.c_str(), mok ? mlit.c_str() : "(missing)");
        }
    }

    // --- O7 (FIX-403): 深层级 hierarchy f64 累加 oracle dk=1..10 + 负例保护
    //   修复前: f32 产品走 float 累加器 ⇒ dk 越大偏差越大 (dk=9 实测 2.5e-3);
    //   修复后: 累加恒在 f64, 落盘按声明位深量化一次 ⇒ 偏差只剩 f32 存储舍入。
    //   合同容差 (HIPS_WRITER.md §9 冻结): hierarchy 通路 rtol=1e-6。
    //   负例: ASTROCS_HIPS_HIER_FAULT=f32_accum 强制复现修复前 f32 累加 ⇒ 同一
    //   判据必须判红 (否则注入面失效/判据退化, 本条自身判红)。
    {
        std::vector<O7Fix> tiles;
        tiles.reserve(2);
        tiles.push_back(o7_make_tile(0, 0x0F1C40301ULL));
        tiles.push_back(o7_make_tile(1, 0x0F1C40302ULL));
        o7_rebind(tiles);

        // (a) 正常路径
        const std::string dir = make_tmp_dir("o7");
        std::string err;
        const long rss0 = o7_rss_kb();
        long rss1 = -1;
        const bool wok = o7_write_product(dir, tiles, err, &rss1);
        P1HIPS_CHECK_MSG(cs, wok, "o7_write", "产品写出失败: %s", err.c_str());
        // 内存增量评估 (FIX-403 步骤 3): 累加器按"每被填充祖先 cell 一整张
        // 512×512"分配 ⇒ f32→f64 增量 = 每 cell 3 通道 × 262144 × 4 B = +3.146 MB。
        // 本 fixture 被填充祖先 cell = K = 10 (每层 A=0 一个) ⇒ +31.5 MB。
        // 实测与产品级外推见 run/FIX-403/REPORT.md §2 (如实登记: 该增量与
        // "覆盖区叶级稠密面"同量级偏大, 只在"全天空叶级稠密面"口径下远小于)。
        {
            const long cells = (long)O7_TILE_ORDER;   // 每层 1 个 (A=0)
            const long b_f32 = cells * 3L * (long)FIX_NPIX * 4L;
            const long b_f64 = cells * 3L * (long)FIX_NPIX * 8L;
            std::printf("[o7-mem] ancestor_cells=%ld f32_track=%ld B f64_track=%ld B "
                        "delta=+%ld B (%.3f MB) rss0=%ld kB rss1=%ld kB rss_delta=%ld kB\n",
                        cells, b_f32, b_f64, b_f64 - b_f32,
                        (double)(b_f64 - b_f32) / 1048576.0, rss0, rss1,
                        (rss0 > 0 && rss1 > 0) ? (rss1 - rss0) : -1L);
            // 下界断言 (非退化): 观测点 RSS 增量必须 ≥ 解析 f64 轨增量的 50%
            // —— 证明累加器确实被分配 (注入/回退到"零分配"实现即判红);
            // 分配器/页粒度噪声留 2× 余量, 跨上下文实测 49~80 MB (阈值 15.7 MB)。
            if (rss0 > 0 && rss1 > 0)
                P1HIPS_CHECK_MSG(cs,
                                 (rss1 - rss0) * 1024L >= (b_f64 - b_f32) / 2L,
                                 "o7_mem_f64_accumulator_present",
                                 "RSS 增量 %ld kB 低于 f64 累加器解析增量的 50%% (%ld B)",
                                 rss1 - rss0, (b_f64 - b_f32) / 2L);
        }
        O7Result base;
        if (wok) {
            base = o7_oracle(dir, tiles);
            P1HIPS_CHECK(cs, base.read_ok, "o7_readback");
        } else {
            base.read_ok = false;
        }
        if (base.read_ok) {
            for (int dk = 1; dk <= 10; ++dk)
                std::printf("[o7] dk=%02d dev_sig=%.6e dev_sup=%.6e cons=%.6e\n",
                            dk, base.dev_sig[dk], base.dev_sup[dk], base.cons[dk]);
            std::printf("[o7] leaf_flux=%.17g\n", base.leaf_flux);
            double worst_sig = 0.0, worst_sup = 0.0, worst_cons = 0.0;
            for (int dk = 1; dk <= 10; ++dk) {
                if (base.dev_sig[dk] > worst_sig) worst_sig = base.dev_sig[dk];
                if (base.dev_sup[dk] > worst_sup) worst_sup = base.dev_sup[dk];
                if (base.cons[dk] > worst_cons) worst_cons = base.cons[dk];
            }
            P1HIPS_CHECK_MSG(cs, worst_sig <= 1e-6, "o7_dk1_10_sig_contract_rtol1e6",
                             "hierarchy signal 偏差超合同容差 rtol=1e-6 (worst=%.6e @dk=1..10)",
                             worst_sig);
            P1HIPS_CHECK_MSG(cs, worst_sup <= 1e-6, "o7_dk1_10_sup_contract_rtol1e6",
                             "hierarchy support 偏差超合同容差 rtol=1e-6 (worst=%.6e)",
                             worst_sup);
            // 累加在 f64 时, 偏差只能来自落盘 f32 单次舍入 (2^-24 = 5.96e-8)
            P1HIPS_CHECK_MSG(cs, worst_sig <= 1e-7, "o7_dk1_10_sig_storage_only",
                             "偏差超 f32 存储单次舍入界 (worst=%.6e > 1e-7) —— 累加未在 f64",
                             worst_sig);
            P1HIPS_CHECK_MSG(cs, base.dev_sig[9] <= 1e-6 && base.dev_sig[10] <= 1e-6,
                             "o7_dk9_dk10_within_tol",
                             "dk=9 偏差 %.6e / dk=10 偏差 %.6e 超合同容差 1e-6",
                             base.dev_sig[9], base.dev_sig[10]);
            // 全层级通量守恒 (父子层积分通量一致; 发布面重建)
            P1HIPS_CHECK_MSG(cs, worst_cons <= 1e-6, "o7_flux_conservation_all_levels",
                             "逐层积分通量守恒超容差 (worst=%.6e)", worst_cons);
            P1HIPS_CHECK_MSG(cs, base.cons[9] <= 1e-6 && base.cons[10] <= 1e-6,
                             "o7_dk9_dk10_flux_conservation",
                             "dk=9 cons=%.6e dk=10 cons=%.6e 超 1e-6",
                             base.cons[9], base.cons[10]);
            P1HIPS_CHECK(cs, base.nodata_ok, "o7_nodata_nan_sup0");
        }

        // (b) 负例保护: 强制 f32 累加路径 → 判据必须判红
        o7_set_f32_accum_env(true);
        const std::string dir_n = make_tmp_dir("o7n");
        std::string err_n;
        const bool wok_n = o7_write_product(dir_n, tiles, err_n);
        o7_set_f32_accum_env(false);
        P1HIPS_CHECK_MSG(cs, wok_n, "o7_neg_write", "注入路径写出失败: %s", err_n.c_str());
        O7Result inj;
        if (wok_n) {
            inj = o7_oracle(dir_n, tiles);
            P1HIPS_CHECK(cs, inj.read_ok, "o7_neg_readback");
        } else {
            inj.read_ok = false;
        }
        if (inj.read_ok) {
            for (int dk = 1; dk <= 10; ++dk)
                std::printf("[o7-neg] dk=%02d dev_sig=%.6e dev_sup=%.6e cons=%.6e\n",
                            dk, inj.dev_sig[dk], inj.dev_sup[dk], inj.cons[dk]);
            const double neg_dk9 = inj.dev_sig[9];
            const double neg_worst = (inj.dev_sig[9] > inj.dev_sig[10]) ? inj.dev_sig[9]
                                                                        : inj.dev_sig[10];
            P1HIPS_CHECK_MSG(cs, neg_worst > 1e-6, "o7_neg_forced_f32_must_be_red",
                             "强制 f32 累加路径未使判据判红 (dk=9/10 worst=%.6e ≤ 1e-6) "
                             "—— 注入面失效或判据退化", neg_worst);
            P1HIPS_CHECK_MSG(cs, neg_dk9 > 1e-5, "o7_neg_dk9_legacy_magnitude",
                             "dk=9 强制 f32 偏差仅 %.6e, 未复现修复前量级 (≥1e-5)",
                             neg_dk9);
            const double neg_cons = (inj.cons[9] > inj.cons[10]) ? inj.cons[9]
                                                                 : inj.cons[10];
            P1HIPS_CHECK_MSG(cs, neg_cons > 1e-6, "o7_neg_conservation_must_be_red",
                             "强制 f32 累加下 dk=9/10 通量守恒仍未判红 (worst=%.6e)",
                             neg_cons);
        }
    }

    if (cs.failures == 0) {
        std::fprintf(stdout, "[p1hips] oracle: O1..O7 PASS\n");
        return 0;
    }
    std::fprintf(stderr, "[p1hips] oracle: %d check(s) failed\n", cs.failures);
    return 1;
}

}  // namespace p1hips
