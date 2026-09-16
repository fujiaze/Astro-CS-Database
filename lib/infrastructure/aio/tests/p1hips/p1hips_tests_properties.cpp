// P1-HIPS-TEST · properties 组 (确定性 1/2/4 线程 + I5/I6/I7/I11 + F5 prov)
//
// 合同锚 HIPS_WRITER.md §9: 不变量 I5 (MOC cells=非空叶 cells)、
// I6 (moc_sky_fraction·4π=moc_area_sr)、I7 (hierarchy 逐阶聚合闭合,
// f64 bitwise / f32 rtol=1e-6)、I11 (F=signal×support×A_cell 有限非负);
// F2 (多 tile 聚合) + F5 (prov 完整/缺省两态) + F6 (双 dtype);
// 确定性 (模块页: tile 字节随输入与写序可复现, 1/2/4 worker 位级一致)。
//
// SCI/ALG ID 锚 (逐条核到实际文档 ID, 不臆造):
//   ALG-HIPS-001..005 — docs/algorithms/HIPS_WRITER.md: 本组逐条不变量的
//     上位算法 —— (5a) MOC UNIQ 与 moc_sky_fraction (I5/I6)、
//     (4b)(4c) hierarchy 逐阶聚合与落盘归一 (I7)、
//     (2b) 叶级归一 signal=flux/area / support=min(area/A_cell,1) 与
//     (2e) MOC 面积登记 (I11 的 sup·A_cell 域)。
//   SCI-DRZ-001 — docs/science/DRIZZLE.md (FROZEN T105, 2026-08-23):
//     §5 重建式与面亮度 S_p=F_p/D_p、§9a support=D_p→[0,1] 与
//     variance_p=sumVarNum/D_p² —— 叶/父归一式的科学层权威。
//   SCI-SCOPE-001 — docs/science/SCIENCE_SCOPE.md: HiPS 科学产品目标。
//   I6/I7/I11 的上位覆盖边界 (如实登记, 不伪造 SCI ID):
//     HIPS_WRITER.md §4 明示 hierarchy 低阶聚合"SCI 层零覆盖"、§5 (5a)
//     MOC 写出亦无独立 SCI 条目; I11 的 F=signal×support×A_cell 同式见
//     该文 §9 与 gate7_hips_validate.py。故这三条只锚 ALG-HIPS-004/005 +
//     DATA-P1-HIPS (DATA_SEMANTICS §12), 不臆造 SCI ID。
#include "p1hips_test_main.hpp"
#include "p1hips_fixtures.hpp"
#include "p1hips_oracle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <system_error>
#include <string>
#include <thread>
#include <vector>

#include "aio_hips.h"

using namespace p1hips;
using namespace p1hips::oracle;

namespace {

CheckState g_cs;

// 层次闭合 oracle (I7): 由 **fixture 输入** (未钳制真实覆盖面积 a) 独立复算
// 父 tile 期望 —— 不拿产物 support (钳后值) 当权重 (M2a-H-3 订正后的生产
// 归约语义: flux += sig·a、area += a, support≤1 只在发布时钳一次)。
// leaf: 2 个输入视图 (parent 0/1, 同父 A=0, tile_order=1→hier k=0)
// 父 local i_acc = s·4^8 + q (s=parent_ipix, q=i_leaf>>2, NESTED);
// 叶/父 FITS 读回均为行主序, 须经 local_to_fits_index 反查到 NESTED 局部。
// skip 条件 !(a>0) || !isfinite(sig) (与生产 skip 等价);
// 无贡献像素 finalize 照写 sig=NaN, sup=0 (DISP-HIPS-011 现状)。
// 判别力: 仅当存在 c=a/A_cell>1 的叶像素时, 本 oracle 才与 "support 加权"
// 旧式分离 (FIX-HIPS-G 异质覆盖即该判别面; c≤1 全域两式仅差浮点重结合)。
static const std::vector<std::uint64_t>& fits_to_local_table() {
    static std::vector<std::uint64_t> t;
    if (t.empty()) {
        t.assign(512ULL * 512ULL, 0);
        for (std::uint64_t i = 0; i < 512ULL * 512ULL; ++i)
            t[(std::size_t)local_to_fits_index(i)] = i;
    }
    return t;
}

void check_hierarchy_closure_f64(const FixViewF64& in0, const FixViewF64& in1,
                                 const TileFits& sig0, const TileFits& sig1,
                                 const TileFits& psig, const TileFits& psup) {
    CheckState& cs = g_cs;
    const double a_k0 = 4.0 * kPi / (12.0 * 512.0 * 512.0);       // k=0 父级 nside=512
    const auto& f2l = fits_to_local_table();
    bool bit_sig = true, bit_sup = true;
    for (std::size_t z_fit = 0; z_fit < psig.pix_d.size(); ++z_fit) {
        const std::uint64_t i_acc = f2l[z_fit];             // 父 local NESTED
        const std::uint64_t s_tile = i_acc >> 16;           // order-1 子单元序号
        const std::uint64_t q = i_acc & 65535ULL;           // tile 内 order-9 父
        const FixViewF64* in = nullptr;
        const TileFits* s0 = nullptr;
        if (s_tile == 0) { in = &in0; s0 = &sig0; }
        else if (s_tile == 1) { in = &in1; s0 = &sig1; }
        if (!in || in->covered_area.empty() || in->covered_area.size() < FIX_NPIX) {
            // 无数据域: sig=NaN, sup=0 (生产对空 acc 照写, DISP-HIPS-011 现状)
            if (!std::isnan(psig.pix_d[z_fit])) bit_sig = false;
            if (psup.pix_d[z_fit] != 0.0) bit_sup = false;
            continue;
        }
        // 同序左结合累加 (叶 NESTED i 递增) — bitwise 与生产一致
        double flux = 0.0, area = 0.0;
        for (int r = 0; r < 4; ++r) {
            const std::uint64_t i_leaf = 4 * q + (std::uint64_t)r;
            const std::size_t fi = (std::size_t)local_to_fits_index(i_leaf);
            const double sv = s0->pix_d[fi];              // 产物叶级 signal (f64 精确)
            const double a = in->covered_area[i_leaf];    // 输入未钳制真实覆盖面积
            // 生产 skip: !v || !(area>0) || !isfinite(flux)
            if (!(a > 0.0) || !std::isfinite(sv)) continue;
            flux += sv * a;
            area += a;
        }
        double want_sig = 0.0, want_sup = 0.0;
        if (area > 0.0 && std::isfinite(flux)) {
            want_sig = flux / area;
            want_sup = area / a_k0;
            if (want_sup > 1.0) want_sup = 1.0;
        } else {
            want_sig = std::numeric_limits<double>::quiet_NaN();
        }
        if (!bits_eq_d(psig.pix_d[z_fit], want_sig)) { bit_sig = false; break; }
        if (!bits_eq_d(psup.pix_d[z_fit], want_sup)) { bit_sup = false; break; }
    }
    P1HIPS_CHECK(cs, bit_sig, "i7_hier_sig_bitwise");
    P1HIPS_CHECK(cs, bit_sup, "i7_hier_sup_bitwise");
}

// f32 层次闭合 (rtol=1e-6, §9 f32 累加路径冻结口径)
void check_hierarchy_closure_f32(const FixViewF64& in0, const TileFits& sig0,
                                 const TileFits& psig, const TileFits& psup) {
    CheckState& cs = g_cs;
    const double a_k0 = 4.0 * kPi / (12.0 * 512.0 * 512.0);
    const auto& f2l = fits_to_local_table();
    bool ok_sig = true, ok_sup = true;
    for (std::size_t z_fit = 0; z_fit < psig.pix_f.size(); ++z_fit) {
        const std::uint64_t i_acc = f2l[z_fit];
        if ((i_acc >> 16) != 0) continue;  // 单 tile f32 场景只查 tile0 域
        const std::uint64_t q = i_acc & 65535ULL;
        double flux = 0.0, area = 0.0;
        for (int r = 0; r < 4; ++r) {
            const std::uint64_t i_leaf = 4 * q + (std::uint64_t)r;
            const std::size_t fi = (std::size_t)local_to_fits_index(i_leaf);
            const double sv = (double)sig0.pix_f[fi];       // f32 存储 = sig_n
            const double a = (double)in0.area_f[i_leaf];    // 输入未钳制真实面积
            if (!(a > 0.0) || !std::isfinite(sv)) continue;
            flux += sv * a;
            area += a;
        }
        double want_sig = 0.0, want_sup = 0.0;
        if (area > 0.0 && std::isfinite(flux)) {
            want_sig = flux / area;
            want_sup = area / a_k0;
            if (want_sup > 1.0) want_sup = 1.0;
        } else {
            want_sig = std::numeric_limits<double>::quiet_NaN();
        }
        if (!rel_close_f(psig.pix_f[z_fit], want_sig, 1e-6)) { ok_sig = false; break; }
        if (!rel_close_f(psup.pix_f[z_fit], want_sup, 1e-6)) { ok_sup = false; break; }
    }
    P1HIPS_CHECK(cs, ok_sig, "i7_hier_f32_rtol1e6");
    P1HIPS_CHECK(cs, ok_sup, "i7_hier_f32_sup_rtol1e6");
}

// N 线程各写独立产品目录 (writer 单句柄串行合同: 不共享句柄/目录)
void write_product_worker(const std::string& dir) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64, AIO_HIPS_PRODUCT_ALL,
        "ivo://astrocs/test/p1hips", "det", nullptr, 100.0,
        "2026-09-07T00:00:00", 0);
    if (!ps) return;
    FixViewF64 t0 = fix_hips_a_tile(0, 10.0, 0.5, 1.5, true, false);
    FixViewF64 t1 = fix_hips_c_edge_tile(1, 20.0, 0.5);
    aio_hips_write_signal_support_tile(ps, &t0.view);
    aio_hips_write_signal_support_tile(ps, &t1.view);
    std::vector<FixSnrPointF> pts = fix_hips_d_snr_points(20260907u, 32);
    aio_hips_write_snr_points(ps, pts.data(), (int)pts.size());
    aio_hips_finalize(ps);
}

}  // namespace

namespace p1hips {

int test_properties() {
    CheckState& cs = g_cs;
    cs.failures = 0;
    cs.fault_reported = false;

    // --- P1: 确定性 1/2/4 线程位级 (模块页: tile 字节随输入与写序可复现)
    {
        std::uint64_t d_ref = 0;
        {
            const std::string dir = make_tmp_dir("det0");
            write_product_worker(dir);
            P1HIPS_CHECK(cs, tree_digest(dir, d_ref, "properties", nullptr),
                         "p1_baseline_tree");
        }
        const int thread_set[] = {2, 4};
        for (int nt : thread_set) {
            std::vector<std::string> dirs;
            std::vector<std::thread> ths;
            for (int i = 0; i < nt; ++i) dirs.push_back(make_tmp_dir("detn"));
            for (int i = 0; i < nt; ++i)
                ths.emplace_back(write_product_worker, dirs[(std::size_t)i]);
            for (auto& t : ths) t.join();
            for (const auto& d : dirs) {
                std::uint64_t dg = 0;
                const bool ok = tree_digest(d, dg, "properties", nullptr);
                P1HIPS_CHECK_MSG(cs, ok && dg == d_ref, "p1_determinism_bits",
                                 "nt=%d 线程产品目录与单线程基线位级不一致", nt);
            }
        }
    }

    // --- P2: I5/I6 — MOC cells = 非空叶 cells + sky fraction (3-tile 产品)
    {
        const std::string dir = make_tmp_dir("p2");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://astrocs/test/p1hips", "p2", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "p2_begin");
        if (ps) {
            for (std::uint64_t p : {0ULL, 1ULL, 2ULL}) {
                FixViewF64 fx = fix_hips_a_tile(p, 10.0, 0.5, 0.0, true, false);
                P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            }
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        std::vector<std::uint64_t> uniq;
        P1HIPS_CHECK(cs, read_moc_uniq(dir + "/signal/Moc.fits", uniq), "p2_moc_read");
        // I5 独立式: order-K (K=0) cell UNIQ = 4·4^0 + c
        std::vector<std::uint64_t> want = {moc_uniq(0, 0, 0), moc_uniq(1, 0, 0), moc_uniq(2, 0, 0)};
        std::sort(uniq.begin(), uniq.end());
        P1HIPS_CHECK_MSG(cs, uniq == want, "i5_moc_cells",
                         "UNIQ 集合 {4,5,6} 期望不符");
        long moc_order = -1;
        P1HIPS_CHECK(cs, read_moc_order(dir + "/signal/Moc.fits", moc_order) && moc_order == 0,
                     "p2_moc_order");
        // I6: moc_sky_fraction 由**唯一**格式化函数产出 (properties 与
        // manifest.json 共用 ⇒ 字面量逐字符相等) 且回程精确 (≥17 有效位)。
        // 修复前 properties 用 std::to_string(6dp)、manifest 用 %.8f ⇒ 必然分叉。
        // 序列化精度的非平凡判别点见 oracle 组 O6 (1/12)。
        const auto kv = read_properties(dir + "/signal/properties");
        {
            const auto it = kv.find("moc_sky_fraction");
            std::string mtxt, mlit;
            const bool mok = read_manifest(dir + "/manifest.json", mtxt) &&
                             manifest_get(mtxt, "moc_sky_fraction", mlit);
            P1HIPS_CHECK_MSG(cs, it != kv.end() && mok && it->second == mlit,
                             "i6_sky_fraction_double_face",
                             "properties/manifest 字面量分叉 (prop=%s manifest=%s)",
                             it != kv.end() ? it->second.c_str() : "(missing)",
                             mok ? mlit.c_str() : "(missing)");
            P1HIPS_CHECK(cs, it != kv.end() &&
                                 std::strtod(it->second.c_str(), nullptr) == 3.0 / 12.0,
                         "i6_sky_fraction_roundtrip");
        }
        // AIO-001/M9-G-6 正例 (原子落盘纪律): properties 必须落在正式路径, 且
        // 临时文件+fsync+原子 rename 的临时文件不得残留。修复前直写(无 tmp)也满足
        // "无残留", 故本断言与 negative:N10 的失败传播断言成对 (一正一负)。
        {
            std::error_code ec;
            P1HIPS_CHECK(cs, std::filesystem::exists(dir + "/signal/properties", ec),
                         "n10_pos_properties_at_final_path");
            bool tmp_left = false;
            for (const auto& e : std::filesystem::recursive_directory_iterator(
                     dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
                if (e.path().filename().string().find(".tmp.") != std::string::npos) tmp_left = true;
            }
            P1HIPS_CHECK(cs, !tmp_left, "n10_pos_no_tmp_leftover");
        }
    }

    // --- P3: I7 层次闭合 f64 bitwise (nside=1024, 2 tiles 同父, F2)
    {
        const std::string dir = make_tmp_dir("p3");
        // nside=1024 → view leaf_order=10 = ilog2(nside) (与产品 ps->leaf_order 一致)
        FixViewF64 t0 = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, false,
                                        AIO_HIPS_FLOAT64, 10);
        FixViewF64 t1 = fix_hips_a_tile(1, 10.0, 0.5, 0.0, true, false,
                                        AIO_HIPS_FLOAT64, 10);
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), 1024, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://astrocs/test/p1hips", "p3", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "p3_begin");
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &t0.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &t1.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        TileFits sig0, sup0, sig1, sup1, psig, psup;
        const bool ok = read_tile_fits(dir + "/signal/Norder1/Dir0/Npix0.fits", sig0) &&
                        read_tile_fits(dir + "/support/Norder1/Dir0/Npix0.fits", sup0) &&
                        read_tile_fits(dir + "/signal/Norder1/Dir0/Npix1.fits", sig1) &&
                        read_tile_fits(dir + "/support/Norder1/Dir0/Npix1.fits", sup1) &&
                        read_tile_fits(dir + "/signal/Norder0/Dir0/Npix0.fits", psig) &&
                        read_tile_fits(dir + "/support/Norder0/Dir0/Npix0.fits", psup);
        P1HIPS_CHECK(cs, ok, "p3_readback");
        if (ok) {
            // 输入面 (未钳制真实覆盖面积) 才是 oracle 的权重来源; 产物 support
            // 是钳后发布值, 不得再当权重 (M2a-H-3)。
            check_hierarchy_closure_f64(t0, t1, sig0, sig1, psig, psup);
        }
    }

    // --- P4: I7 层次闭合 f32 rtol=1e-6 (单 tile, 父域 tile0 区间)
    {
        const std::string dir = make_tmp_dir("p4");
        FixViewF64 t0 = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, false,
                                        AIO_HIPS_FLOAT32, 10);
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), 1024, 512, AIO_HIPS_FLOAT32,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://astrocs/test/p1hips", "p4", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "p4_begin");
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &t0.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        TileFits sig0, sup0, psig, psup;
        const bool ok = read_tile_fits(dir + "/signal/Norder1/Dir0/Npix0.fits", sig0) &&
                        read_tile_fits(dir + "/support/Norder1/Dir0/Npix0.fits", sup0) &&
                        read_tile_fits(dir + "/signal/Norder0/Dir0/Npix0.fits", psig) &&
                        read_tile_fits(dir + "/support/Norder0/Dir0/Npix0.fits", psup);
        P1HIPS_CHECK(cs, ok, "p4_readback");
        if (ok) check_hierarchy_closure_f32(t0, sig0, psig, psup);
    }

    // --- P5: I11 — F=signal×support×A_cell 有限非负 (读回 U1 类 tile)
    {
        const std::string dir = make_tmp_dir("p5");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://astrocs/test/p1hips", "p5", nullptr, 0.0, nullptr, 0);
        if (ps) {
            FixViewF64 t0 = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, false);
            FixViewF64 t1 = fix_hips_c_edge_tile(1, 20.0, 0.5);
            aio_hips_write_signal_support_tile(ps, &t0.view);
            aio_hips_write_signal_support_tile(ps, &t1.view);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        TileFits sig, sup;
        if (read_tile_fits(dir + "/signal/Norder0/Dir0/Npix1.fits", sig) &&
            read_tile_fits(dir + "/support/Norder0/Dir0/Npix1.fits", sup)) {
            const double a_cell = 4.0 * kPi / (12.0 * 512.0 * 512.0);
            bool f_ok = true;
            bool neg_seen = false;  // F3 负 flux 注入必须保留 (I1 精确负值)
            for (std::size_t i = 0; i < sig.pix_d.size(); ++i) {
                const double s = sig.pix_d[i], p = sup.pix_d[i];
                if (std::isnan(s) || std::isnan(p)) continue;  // 无效像素
                if (s < 0.0) { neg_seen = true; continue; }    // F3 注入域
                const double f = s * p * a_cell;               // gate7 同式 (sup>0 域)
                if (p > 0.0 && (!std::isfinite(f) || f < 0.0)) { f_ok = false; break; }
            }
            P1HIPS_CHECK(cs, neg_seen, "i11_f3_negflux_kept");
            P1HIPS_CHECK(cs, f_ok, "i11_flux_nonneg");
        } else {
            P1HIPS_CHECK(cs, false, "p5_readback");
        }
    }

    // --- P6: F5 prov 两态 (完整/缺省)
    {
        const std::string d1 = make_tmp_dir("p6a");
        P1HIPS_CHECK_EQ(cs, write_full_f64_product(d1, AIO_HIPS_PRODUCT_SIGNAL, nullptr, true), 0);
        const auto kv1 = read_properties(d1 + "/signal/properties");
        P1HIPS_CHECK(cs, kv1.count("ASTROCS_DRIZZLE_PIXFRAC") &&
                         kv1.at("ASTROCS_DRIZZLE_PIXFRAC") == "0.700000", "f5_prov_pixfrac");
        P1HIPS_CHECK(cs, kv1.count("ASTROCS_DRIZZLE_SCALE_ARCSEC") &&
                         kv1.at("ASTROCS_DRIZZLE_SCALE_ARCSEC") == "0.3500", "f5_prov_scale");

        const std::string d2 = make_tmp_dir("p6b");
        P1HIPS_CHECK_EQ(cs, write_full_f64_product(d2, AIO_HIPS_PRODUCT_SIGNAL, nullptr, false), 0);
        const auto kv2 = read_properties(d2 + "/signal/properties");
        P1HIPS_CHECK(cs, !kv2.count("ASTROCS_DRIZZLE_PIXFRAC"), "f5_prov_absent_pixfrac");
        P1HIPS_CHECK(cs, !kv2.count("ASTROCS_DRIZZLE_SCALE_ARCSEC"), "f5_prov_absent_scale");
    }

    // --- P7: I7 判别面 (M2a-H-3) — 异质覆盖 c>1: 面积加权 vs support 加权
    //   FIX-HIPS-G: 逐像素交替 (sb=10, c=4) 与 (sb=0.1, c=1) → 每父 cell
    //   解析真值 sig=8.02、sup=min(10/4,1)=1; 旧式 (钳后 support 加权) 给 5.05
    //   ⇒ 本条在修复前必红 (单靠 c≤1 fixture 无法分离两式)。
    {
        const std::string dir = make_tmp_dir("p7");
        FixViewF64 t0 = fix_hips_g_hetero_coverage_tile(0, 10.0, 4.0, 0.1, 1.0,
                                                       AIO_HIPS_FLOAT64, 10);
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), 1024, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://astrocs/test/p1hips", "p7", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "p7_begin");
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &t0.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        TileFits sig0, psig, psup;
        const bool ok = read_tile_fits(dir + "/signal/Norder1/Dir0/Npix0.fits", sig0) &&
                        read_tile_fits(dir + "/signal/Norder0/Dir0/Npix0.fits", psig) &&
                        read_tile_fits(dir + "/support/Norder0/Dir0/Npix0.fits", psup);
        P1HIPS_CHECK(cs, ok, "p7_readback");
        if (ok) {
            FixViewF64 absent;   // 只写 tile0: s_tile==1 域无数据 (NaN/0)
            check_hierarchy_closure_f64(t0, absent, sig0, sig0, psig, psup);
            // 非平凡解析点: 父级面亮度 = 面积加权真值 8.02 (旧式 5.05)
            const std::size_t z = (std::size_t)local_to_fits_index(0);
            P1HIPS_CHECK_MSG(cs, std::fabs(psig.pix_d[z] - 8.02) < 1e-12,
                             "p7_area_weighted_signal",
                             "父级 signal 偏离面积加权真值 8.02 (got=%.17g)",
                             psig.pix_d[z]);
            P1HIPS_CHECK_MSG(cs, psup.pix_d[z] == 1.0, "p7_parent_support_clamped",
                             "父级 support 应钳到 1.0 (got=%.17g)", psup.pix_d[z]);
        }
        // 可观测钳制计数 (M2a-H-3): 叶级 c=4 像素 131072 个 (262144/2);
        // 父 tile 被叶 tile 覆盖的像素 = 262144/4 = 65536 个, 每个 Σa=10·A_leaf
        // > A_cell_k=4·A_leaf ⇒ 这 65536 个父像素全部超覆盖 (其余父像素无数据)。
        const auto kvp = read_properties(dir + "/signal/properties");
        const auto itc = kvp.find("astrocs_support_clamped_pixels");
        const auto itg = kvp.find("astrocs_coverage_gt1_pixels");
        P1HIPS_CHECK_MSG(cs, itc != kvp.end() &&
                                 std::strtoull(itc->second.c_str(), nullptr, 10) == 131072ULL,
                         "p7_leaf_clamp_count",
                         "astrocs_support_clamped_pixels=%s (want 131072)",
                         itc == kvp.end() ? "(missing)" : itc->second.c_str());
        P1HIPS_CHECK_MSG(cs, itg != kvp.end() &&
                                 std::strtoull(itg->second.c_str(), nullptr, 10) == 65536ULL,
                         "p7_hier_gt1_count",
                         "astrocs_coverage_gt1_pixels=%s (want 65536)",
                         itg == kvp.end() ? "(missing)" : itg->second.c_str());
        std::string mtxt, mv;
        const bool mok = read_manifest(dir + "/manifest.json", mtxt) &&
                         manifest_get(mtxt, "astrocs_support_clamped_pixels", mv);
        P1HIPS_CHECK_MSG(cs, mok && itc != kvp.end() && mv == itc->second,
                         "p7_manifest_clamp_count",
                         "manifest 计数与 properties 不一致 (manifest=%s)",
                         mok ? mv.c_str() : "(missing)");
    }

    if (cs.failures == 0) {
        std::fprintf(stdout, "[p1hips] properties: P1..P7 PASS\n");
        return 0;
    }
    std::fprintf(stderr, "[p1hips] properties: %d check(s) failed\n", cs.failures);
    return 1;
}

}  // namespace p1hips
