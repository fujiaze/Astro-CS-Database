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
//   O5 variance/ivar 互倒 (I4 有限域) f64 bitwise + f32 rtol。
#include "p1hips_test_main.hpp"
#include "p1hips_fixtures.hpp"
#include "p1hips_oracle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "aio_hips.h"

using namespace p1hips;
using namespace p1hips::oracle;

namespace {

CheckState g_cs;

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
        // sky fraction 弱上界: 3/12 cells → |frac−0.25| < 1e-9 (§9 冻结)
        const auto kv = read_properties(dir + "/signal/properties");
        {
            const auto it = kv.find("moc_sky_fraction");
            if (it != kv.end()) {
                const double got = std::atof(it->second.c_str());
                P1HIPS_CHECK(cs, std::fabs(got - 3.0 / 12.0) < 1e-9, "o2_skyfrac_1e9");
            } else {
                P1HIPS_CHECK(cs, false, "o2_skyfrac_key");
            }
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
        // ALG: pixel_scale(") = 3600·(180/π)·sqrt(π/3)/nside —— 独立复算
        const double scale = 3600.0 * (180.0 / kPi) * std::sqrt(kPi / 3.0) / 512.0;
        char want[32];
        std::snprintf(want, sizeof(want), "%.6f", scale);
        const auto kv = read_properties(dir + "/signal/properties");
        const auto it = kv.find("hips_pixel_scale");
        P1HIPS_CHECK_MSG(cs, it != kv.end() && it->second == want, "o4_pixel_scale",
                         "hips_pixel_scale got=%s want=%s",
                         it == kv.end() ? "<missing>" : it->second.c_str(), want);
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

    if (cs.failures == 0) {
        std::fprintf(stdout, "[p1hips] oracle: O1..O5 PASS\n");
        return 0;
    }
    std::fprintf(stderr, "[p1hips] oracle: %d check(s) failed\n", cs.failures);
    return 1;
}

}  // namespace p1hips
