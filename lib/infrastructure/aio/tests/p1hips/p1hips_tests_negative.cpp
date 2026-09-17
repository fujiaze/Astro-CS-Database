// P1-HIPS-TEST · negative 组 (§9 负面矩阵逐行 + 错误码/last_error 合同)
//
// 覆盖 (HIPS_WRITER.md §9 负面矩阵): nside<512 / tile_width≠512 /
// 非法 dtype / 越位 flags / parent_ipix≥12·4^K / width≠512 / var_num NULL /
// 全无效 variance tile (−5) / FITS 路径不可写 (−4/−5/−6/−7) /
// prov pixfrac(0,1] + scale 闭域 (NaN/Inf/0/负/>824.5167388361774″, 均 rc=2
// 且必须 set_error 点名原因) / 重复 finalize (−2, I9) / NULL 句柄域 /
// write_snr NULL+域 / last_error 非空语义。
// 批次 R hardening 面 (astro_image_io 域 57 处 malloc 0 静默) 由既有
// hardening 测试覆盖, 本组不重复 (负面注入从 ABI 合同层出发)。
#include "p1hips_test_main.hpp"
#include "p1hips_fixtures.hpp"
#include "p1hips_oracle.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <system_error>
#include <string>

#include "aio_hips.h"

using namespace p1hips;
using namespace p1hips::oracle;

namespace {

CheckState g_cs;

bool is_root() { return ::geteuid() == 0; }

// 预建只读目录 (chmod 500) —— 非 root 下 fopen 失败
bool make_ro_dir(const std::string& path) {
    if (::mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) return false;
    return ::chmod(path.c_str(), 0500) == 0;
}

// AIO-001/M9-G-6: 递归检查产品目录内是否有 *.tmp.* 残留 (原子落盘失败路径必须清理)。
// 返回 true = 发现残留 (测试应失败)。
bool has_tmp_leftover(const std::string& root) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return false;
    for (const auto& e : std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec)) {
        const std::string name = e.path().filename().string();
        if (name.find(".tmp.") != std::string::npos) return true;
    }
    return false;
}

}  // namespace

namespace p1hips {

// SCI-F3-001: DATA-UNC-001 §30.2/§30.3 AIO 通道负向组 (定义于
// p1hips_tests_diag_prov.cpp); 并入既有 negative 组 ⇒ 零新增 CTest 目标。
int test_diag_prov_negative();

int test_negative() {
    CheckState& cs = g_cs;
    cs.failures = 0;
    cs.fault_reported = false;

    // --- N1: product_begin 参数域 → NULL (§9 行 1-4) + last_error 非空
    {
        const std::string dir = make_tmp_dir("n1");
        AioHipsProductSet* ps = nullptr;
        const char* last = nullptr;

        ps = aio_hips_product_begin(nullptr, FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
                                    AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t",
                                    nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_begin_null_outdir");
        last = aio_hips_last_error();
        P1HIPS_CHECK(cs, last && last[0] != '\0', "n1_last_error_nonempty");

        ps = aio_hips_product_begin("", FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
                                    AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t",
                                    nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_begin_empty_outdir");

        ps = aio_hips_product_begin(dir.c_str(), 256, 512, AIO_HIPS_FLOAT64,
                                    AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t",
                                    nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_nside_lt512");

        ps = aio_hips_product_begin(dir.c_str(), 511, 512, AIO_HIPS_FLOAT64,
                                    AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t",
                                    nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_nside_511");

        // M8d-A-01/AIO-001: nside 必须是 2 的幂 (ALG-HIPS-001 (1a) "nside=2^K>=512")。
        // 非 2 的幂会被 ilog2_u64 向下取整静默夹逼并据此写出与调用方声明不一致的
        // NSIDE/A_cell/leaf_order —— 属 ASTROCS_DESIGN §9 禁止的"看似完整产品"。
        const std::uint32_t kNsideNotPow2[] = {600u, 513u, 768u, 1000u, 1536u};
        for (std::uint32_t bad_nside : kNsideNotPow2) {
            ps = aio_hips_product_begin(dir.c_str(), bad_nside, 512, AIO_HIPS_FLOAT64,
                                        AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t",
                                        nullptr, 0.0, nullptr, 0);
            P1HIPS_CHECK(cs, ps == nullptr, "n1_nside_not_pow2");
            if (ps) aio_hips_abort(ps);
        }
        // 上界: 2^29 合法, 2^30 拒绝 (leaf_order<=29 ⇒ MOC tile_order<=20)。
        ps = aio_hips_product_begin(dir.c_str(), 1u << 30, 512, AIO_HIPS_FLOAT64,
                                    AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t",
                                    nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_nside_gt_2p29");
        if (ps) aio_hips_abort(ps);

        ps = aio_hips_product_begin(dir.c_str(), FIX_NSIDE, 256, AIO_HIPS_FLOAT64,
                                    AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t",
                                    nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_tilewidth_256");

        ps = aio_hips_product_begin(dir.c_str(), FIX_NSIDE, 1024, AIO_HIPS_FLOAT64,
                                    AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t",
                                    nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_tilewidth_1024");

        ps = aio_hips_product_begin(dir.c_str(), FIX_NSIDE, 512, 2,
                                    AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t",
                                    nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_dtype_2");

        ps = aio_hips_product_begin(dir.c_str(), FIX_NSIDE, 512, -1,
                                    AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t",
                                    nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_dtype_minus1");

        // 越位 flags: 高于 ALL_V20 (127) 的位未定义。
        // 32/64 (nrej/nused) 自 SCI-F3-001 起为合法位 (DATA-UNC-001 §30.2),
        // 未定义位上界随之抬到 127。
        ps = aio_hips_product_begin(dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
                                    128, "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_flags_bit7");
        // 32/64 现在合法 (nrej/nused 诊断平面位) —— begin 必须接受
        ps = aio_hips_product_begin(dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
                                    AIO_HIPS_PRODUCT_NREJ | AIO_HIPS_PRODUCT_NUSED,
                                    "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "n1_flags_diag_bits_accepted");
        if (ps) aio_hips_abort(ps);

        ps = aio_hips_product_begin(dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
                                    -1, "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps == nullptr, "n1_flags_negative");
    }

    // --- N2: write_signal_support_tile 域 (width≠512 → −2; dtype/leaf_order
    //     不匹配 → −2; parent_ipix≥12·4^K → −3; NULL → −1)
    {
        const std::string dir = make_tmp_dir("n2");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "n2_begin");
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(nullptr, nullptr), -1);
            FixViewF64 good = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, false);

            FixViewF64 bad_width = good;
            bad_width.view.width = 256;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &bad_width.view), -2);

            FixViewF64 bad_order = good;
            bad_order.view.leaf_order = 8;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &bad_order.view), -2);

            FixViewF64 bad_dtype = good;
            bad_dtype.view.data_type = AIO_HIPS_FLOAT32;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &bad_dtype.view), -2);

            FixViewF64 bad_parent = good;
            bad_parent.view.parent_ipix = 12;   // ≥ 12·4^0
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &bad_parent.view), -3);

            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, nullptr), -1);
            aio_hips_abort(ps);
        }
    }

    // --- N3: write_variance_tile 域 (var_num NULL → −2; width/order/dtype →
    //     −3; parent 越界 → −4; 全无效 → −5; NULL → −1)
    {
        const std::string dir = make_tmp_dir("n3");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR,
            "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "n3_begin");
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_write_variance_tile(nullptr, nullptr), -1);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_variance_tile(ps, nullptr), -1);

            FixViewF64 no_var = fix_hips_a_tile(0, 10.0, 0.5, 1.0, true, false);
            no_var.view.var_num_sum = nullptr;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_variance_tile(ps, &no_var.view), -2);

            FixViewF64 bad_width = fix_hips_a_tile(0, 10.0, 0.5, 1.0, true, true);
            bad_width.view.width = 128;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_variance_tile(ps, &bad_width.view), -3);

            FixViewF64 bad_parent = fix_hips_a_tile(12, 10.0, 0.5, 1.0, true, true);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_variance_tile(ps, &bad_parent.view), -4);

            FixViewF64 all_inv = fix_hips_f_all_invalid_var_tile(0);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_variance_tile(ps, &all_inv.view), -5);

            aio_hips_abort(ps);
        }
    }

    // --- N4: FITS 路径不可写 → write_signal rc=−4 / write_variance rc=−6
    //     (§9 行: FITS 路径不可写 −4/−5/−6/−7; 信号 FITS 失败先于 support)
    {
        if (is_root()) {
            std::fprintf(stdout, "[p1hips] negative: N4 跳过 (root 权限绕过只读目录)\n");
        } else {
            const std::string dir = make_tmp_dir("n4");
            P1HIPS_CHECK(cs, make_ro_dir(dir + "/signal"), "n4_mkro_signal");
            AioHipsProductSet* ps = aio_hips_product_begin(
                dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
                AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
                "ivo://t", "t", nullptr, 0.0, nullptr, 0);
            P1HIPS_CHECK(cs, ps != nullptr, "n4_begin");
            if (ps) {
                FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, false);
                P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), -4);
                aio_hips_abort(ps);
            }
            // variance FITS 不可写 → −6
            const std::string dir2 = make_tmp_dir("n4b");
            P1HIPS_CHECK(cs, make_ro_dir(dir2 + "/variance"), "n4_mkro_variance");
            AioHipsProductSet* ps2 = aio_hips_product_begin(
                dir2.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
                AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR,
                "ivo://t", "t", nullptr, 0.0, nullptr, 0);
            if (ps2) {
                FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 1.0, true, true);
                P1HIPS_CHECK_EQ(cs, aio_hips_write_variance_tile(ps2, &fx.view), -6);
                aio_hips_abort(ps2);
            }
            // ivar FITS 不可写 (variance 可写) → −7
            const std::string dir3 = make_tmp_dir("n4c");
            P1HIPS_CHECK(cs, make_ro_dir(dir3 + "/ivar"), "n4_mkro_ivar");
            AioHipsProductSet* ps3 = aio_hips_product_begin(
                dir3.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
                AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR,
                "ivo://t", "t", nullptr, 0.0, nullptr, 0);
            if (ps3) {
                FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 1.0, true, true);
                P1HIPS_CHECK_EQ(cs, aio_hips_write_variance_tile(ps3, &fx.view), -7);
                aio_hips_abort(ps3);
            }
        }
    }

    // --- N5: drizzle provenance 合法域 (M9-F-3)
    //   pixfrac ∉ (0,1] → 2; scale: !isfinite → 2、!(x>0) → 2、
    //   x > ACS_HIPS_MAX_FRAME_SCALE_ARCSEC → 2; 每次拒绝必须 set_error 并点名
    //   具体原因 (ENGINEERING_SPEC:137 状态码 + 结构化诊断)。
    //   通道语义 = 全或无: 接受后两键必须齐备落盘 (禁"接受 0 但静默不写键")。
    {
        const std::string dir = make_tmp_dir("n5");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "n5_begin");
        P1HIPS_CHECK_EQ(cs, aio_hips_set_drizzle_provenance(nullptr, 0.5, 1.0), 1);
        if (ps) {
            const double nan_v = std::numeric_limits<double>::quiet_NaN();
            const double inf_v = std::numeric_limits<double>::infinity();
            auto rejected = [&](double pf, double sc, const char* token, const char* tag) {
                P1HIPS_CHECK_EQ(cs, aio_hips_set_drizzle_provenance(ps, pf, sc), 2);
                const char* e = aio_hips_last_error();
                P1HIPS_CHECK_MSG(cs, e && std::strstr(e, token) != nullptr, tag,
                                 "拒绝必须 set_error 点名原因 (%s); last_error=%s",
                                 token, e ? e : "(null)");
            };
            rejected(1.5, 1.0, "pixfrac", "n5_pixfrac_gt1");
            rejected(0.0, 1.0, "pixfrac", "n5_pixfrac_zero");
            rejected(0.5, nan_v, "非有限", "n5_scale_nan");
            rejected(0.5, inf_v, "非有限", "n5_scale_inf");
            rejected(0.5, 0.0, "必须 > 0", "n5_scale_zero");
            rejected(0.5, -1.0, "必须 > 0", "n5_scale_negative");
            rejected(0.5, ACS_HIPS_MAX_FRAME_SCALE_ARCSEC + 1.0, "超出物理域",
                     "n5_scale_above_upper");
            // 上界本身在闭域内 (上界不是"留白"): 必须被接受
            P1HIPS_CHECK_EQ(cs,
                            aio_hips_set_drizzle_provenance(
                                ps, 0.5, ACS_HIPS_MAX_FRAME_SCALE_ARCSEC), 0);
            // 接受后两键齐备落盘 (禁静默缺键)
            P1HIPS_CHECK_EQ(cs, aio_hips_set_drizzle_provenance(ps, 0.5, 0.1), 0);
            FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, false);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
            const auto kv = read_properties(dir + "/signal/properties");
            P1HIPS_CHECK_MSG(cs, kv.count("ASTROCS_DRIZZLE_PIXFRAC") &&
                                     kv.count("ASTROCS_DRIZZLE_SCALE_ARCSEC"),
                             "n5_all_or_none_keys",
                             "provenance 接受后必须两键齐备 (pixfrac=%d scale=%d)",
                             (int)kv.count("ASTROCS_DRIZZLE_PIXFRAC"),
                             (int)kv.count("ASTROCS_DRIZZLE_SCALE_ARCSEC"));
        }
    }

    // --- N6: 重复 finalize → −2 (I9); finalize(NULL) → −1
    //     (经失败路径构造: SNR 目录只读 → finalize rc=−6, 句柄不释放)
    {
        if (is_root()) {
            std::fprintf(stdout, "[p1hips] negative: N6 跳过 (root 权限绕过只读目录)\n");
        } else {
            const std::string dir = make_tmp_dir("n6");
            P1HIPS_CHECK(cs, make_ro_dir(dir + "/snr"), "n6_mkro_snr");
            AioHipsProductSet* ps = aio_hips_product_begin(
                dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
                AIO_HIPS_PRODUCT_SNR, "ivo://t", "t", nullptr, 0.0, nullptr, 0);
            P1HIPS_CHECK(cs, ps != nullptr, "n6_begin");
            if (ps) {
                std::vector<FixSnrPointF> pts = fix_hips_d_snr_points(42u, 4);
                P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(ps, pts.data(), (int)pts.size()), 0);
                const int f1 = aio_hips_finalize(ps);
                P1HIPS_CHECK_MSG(cs, f1 == -6, "n6_finalize_fail_rc",
                                 "SNR 目录只读 finalize 期望 −6, got %d", f1);
                // I9: 失败句柄二次 finalize → −2
                const int f2 = aio_hips_finalize(ps);
                P1HIPS_CHECK_EQ(cs, f2, -2);
                aio_hips_abort(ps);
            }
        }
        P1HIPS_CHECK_EQ(cs, aio_hips_finalize(nullptr), -1);
    }

    // --- N7: write_snr_points 域 (NULL pts+n>0 → −1; n=0 → 0; NULL ps → −1)
    {
        const std::string dir = make_tmp_dir("n7");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64, AIO_HIPS_PRODUCT_SNR,
            "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "n7_begin");
        P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(nullptr, nullptr, 5), -1);
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(ps, nullptr, 5), -1);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(ps, nullptr, 0), 0);
            std::vector<FixSnrPointF> pts = fix_hips_d_snr_points(1u, 2);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(ps, pts.data(), (int)pts.size()), 0);
            aio_hips_abort(ps);
        }
    }

    // --- N8: abort 域 (NULL → 0; 正常句柄 abort → 0, 句柄随即失效不再复用)
    {
        P1HIPS_CHECK_EQ(cs, aio_hips_abort(nullptr), 0);
        const std::string dir = make_tmp_dir("n8");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_abort(ps), 0);
            // I10: abort 后句柄失效 — 悬垂复用为 UB, 以不再复用为合同
            // (DISP-HIPS-001 已登记 abort 无文件回滚; 不注入悬垂调用)
        } else {
            P1HIPS_CHECK(cs, false, "n8_begin");
        }
    }

    // --- N9: 错误码/last_error 语义一致性 (负面返回后 last_error 可读)
    {
        const std::string dir = make_tmp_dir("n9");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        if (ps) {
            FixViewF64 bad_parent = fix_hips_a_tile(99, 10.0, 0.5, 0.0, true, false);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &bad_parent.view), -3);
            const char* msg = aio_hips_last_error();
            P1HIPS_CHECK(cs, msg && std::strstr(msg, "parent") != nullptr, "n9_last_error_text");
            aio_hips_abort(ps);
        } else {
            P1HIPS_CHECK(cs, false, "n9_begin");
        }
    }

    // --- N10 (AIO-001/M9-G-6): properties 是 IVOA+provenance 唯一文本载体,
    //     其落盘失败必须传播 (修复前与 write_properties 同为 void/静默 return:
    //     fopen 失败即 return、fprintf/fclose 不查 ⇒ finalize 照常返回 0,
    //     产出缺 properties 的"完整"产品, 违反 ASTROCS_DESIGN §9)。
    //     同时断言原子落盘纪律: 失败路径不得残留 .tmp.* 临时文件。
    {
        // 注入面: ASTROCS_HIPS_PROV_FAULT=properties_write_fail (测试专用等价缺陷面,
        // 与既有 missing_key/value_drift/sentinel/skip_write/shortcut 五个注入点同机制)。
        const char* prev = std::getenv("ASTROCS_HIPS_PROV_FAULT");
        const std::string prev_s = prev ? std::string(prev) : std::string();
#ifdef _WIN32
        _putenv_s("ASTROCS_HIPS_PROV_FAULT", "properties_write_fail");
#else
        ::setenv("ASTROCS_HIPS_PROV_FAULT", "properties_write_fail", 1);
#endif
        const std::string dir = make_tmp_dir("n10");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        if (ps) {
            FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, true);
            const int wrc = aio_hips_write_signal_support_tile(ps, &fx.view);
            P1HIPS_CHECK_MSG(cs, wrc == 0, "n10_write_tile_ok",
                             "signal 数据 tile 写入期望 0, got %d", wrc);
            const int frc = aio_hips_finalize(ps);
            // 修复前: properties 与 FITS 同一子产品路径均为静默/void
            // (write_properties fopen 失败即 return、fprintf/fclose 不查) ⇒ 即使
            // properties 写不出, finalize 仍按成功返回 0 —— 产出缺 IVOA/provenance
            // 唯一文本载体的"完整"产品, 违反 ASTROCS_DESIGN §9。
            // 修复后: properties 原子写失败 → 子产品失败码 −3 上报。
            P1HIPS_CHECK_MSG(cs, frc == -3, "n10_finalize_propagates_properties_failure",
                             "properties 不可写 finalize 期望 −3, got %d", frc);
            // 失败路径不留临时文件 (原子落盘纪律: 无半成品、无 *.tmp.* 残留)
            P1HIPS_CHECK(cs, !has_tmp_leftover(dir), "n10_no_tmp_leftover");
            aio_hips_abort(ps);
        } else {
            P1HIPS_CHECK(cs, false, "n10_begin");
        }
        // 恢复注入面 (后续用例必须回到未注入语义)
        if (!prev_s.empty()) {
#ifdef _WIN32
            _putenv_s("ASTROCS_HIPS_PROV_FAULT", prev_s.c_str());
#else
            ::setenv("ASTROCS_HIPS_PROV_FAULT", prev_s.c_str(), 1);
#endif
        } else {
#ifdef _WIN32
            _putenv_s("ASTROCS_HIPS_PROV_FAULT", "");
#else
            ::unsetenv("ASTROCS_HIPS_PROV_FAULT");
#endif
        }
    }

    // --- N10b: 输出结构 AioHipsVerifyReport 的 ABI fail-closed (LEDGER-P1 追加项)
    //   ABI 校验必须**先于**任何产品级检查: 否则旧调用方 (零头) 会先拿到
    //   "-1 参数无效/properties 缺失", ABI 不匹配被掩盖 = 假绿。
    {
        const std::string nodir = make_tmp_dir("n10babi") + "/absent";
        // (a) 合法 ABI 头 ⇒ 走常规失败路径 (不得是 -9)
        AioHipsVerifyReport rep{};
        rep.struct_size = (uint32_t)sizeof(AioHipsVerifyReport);
        rep.abi_version = (uint32_t)AIO_HIPS_VERIFY_REPORT_ABI_VERSION;
        P1HIPS_CHECK_MSG(cs,
                         aio_hips_verify_product_set(nodir.c_str(), &rep) !=
                             AIO_HIPS_ABI_MISMATCH,
                         "n10b_abi_ok_not_rejected", "合法 ABI 头不得判 -9");
        // (b) 旧调用方 (struct_size/abi_version 均 0) ⇒ -9 且 last_error 点名 ABI
        //   **本行必须保持零头**: 这是"旧调用方"的负例本体, 不得被调用方
        //   批量补头脚本"顺手修好"(实测踩过一次 ⇒ 负例变假绿)。
        AioHipsVerifyReport old_rep{};   // NOLINT: 故意不初始化 ABI 头
        old_rep.struct_size = 0;
        old_rep.abi_version = 0;
        P1HIPS_CHECK_EQ(cs, aio_hips_verify_product_set(nodir.c_str(), &old_rep),
                        AIO_HIPS_ABI_MISMATCH);
        {
            const char* m = aio_hips_last_error();
            P1HIPS_CHECK_MSG(cs, m && std::strstr(m, "ABI") != nullptr,
                             "n10b_last_error", "ABI 拒绝必须点名 ABI; last_error=%s",
                             m ? m : "(null)");
        }
        // (c) 尺寸不符 ⇒ -9
        AioHipsVerifyReport bad{};
        bad.struct_size = (uint32_t)sizeof(AioHipsVerifyReport) - 4u;
        bad.abi_version = (uint32_t)AIO_HIPS_VERIFY_REPORT_ABI_VERSION;
        P1HIPS_CHECK_EQ(cs, aio_hips_verify_product_set(nodir.c_str(), &bad),
                        AIO_HIPS_ABI_MISMATCH);
        // (d) 版本不符 ⇒ -9
        bad = rep;
        bad.abi_version = (uint32_t)AIO_HIPS_VERIFY_REPORT_ABI_VERSION + 1u;
        P1HIPS_CHECK_EQ(cs, aio_hips_verify_product_set(nodir.c_str(), &bad),
                        AIO_HIPS_ABI_MISMATCH);
    }

    // --- N11: 跨边界结构 ABI fail-closed (V11-N-01; ASTROCS_DESIGN §7.3)
    //   无 ABI 头的旧调用方 (struct_size/abi_version=0) 与错尺寸/错版本必须在
    //   公共入口被拒绝 (AIO_HIPS_ABI_MISMATCH=-9) 且 last_error 点名 ABI;
    //   合法 ABI 头不得被误拒 (双向排除)。修复前这些调用会被"按盲步长"读取:
    //   40B C 结构 vs 32B 镜像语义 ⇒ 静默错位写数据 + 越界读。
    {
        const std::string dir = make_tmp_dir("n11abi");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "n11abi_begin");
        if (ps) {
            FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, false);
            // (a) 合法 ABI 头: 必须放行
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            // (b) 旧调用方 (零初始化 = struct_size/abi_version 均 0): fail-closed
            AstroSphereTileView old_view = fx.view;
            old_view.struct_size = 0;
            old_view.abi_version = 0;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &old_view),
                            AIO_HIPS_ABI_MISMATCH);
            {
                const char* m = aio_hips_last_error();
                P1HIPS_CHECK_MSG(cs, m && std::strstr(m, "ABI") != nullptr,
                                 "n11abi_view_last_error",
                                 "ABI 拒绝必须点名 ABI; last_error=%s",
                                 m ? m : "(null)");
            }
            // (c) 尺寸不符 (旧 40B 布局调用方按新头读到的 struct_size)
            AstroSphereTileView bad = fx.view;
            bad.struct_size = (uint32_t)sizeof(AstroSphereTileView) - 4u;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &bad),
                            AIO_HIPS_ABI_MISMATCH);
            // (d) 版本不符 (variance 入口同款)
            bad = fx.view;
            bad.abi_version = (uint32_t)AIO_HIPS_TILE_VIEW_ABI_VERSION + 1u;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_variance_tile(ps, &bad),
                            AIO_HIPS_ABI_MISMATCH);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &bad),
                            AIO_HIPS_ABI_MISMATCH);
            // (e) SNR 点数组: 逐元素校验 (第 2 元素起模拟旧 32B 镜像步长错位)
            std::vector<FixSnrPointF> pts = fix_hips_d_snr_points(11u, 3);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(ps, pts.data(), 3), 0);
            pts[1].struct_size = 0;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(ps, pts.data(), 3),
                            AIO_HIPS_ABI_MISMATCH);
            pts[1].struct_size = (uint32_t)sizeof(AioHipsSnrPoint);
            pts[2].abi_version = 7;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(ps, pts.data(), 3),
                            AIO_HIPS_ABI_MISMATCH);
            pts[2].abi_version = AIO_HIPS_SNR_POINT_ABI_VERSION;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(ps, pts.data(), 3), 0);
            aio_hips_abort(ps);   // 本组只测入口拒绝面, 不 finalize
        }
        // (f) 诊断平面视图同款
        {
            const std::string d2 = make_tmp_dir("n11abi_diag");
            AioHipsProductSet* p2 = aio_hips_product_begin(
                d2.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT32,
                AIO_HIPS_PRODUCT_NREJ, "ivo://t", "t", nullptr, 0.0, nullptr, 0);
            if (p2) {
                std::vector<std::int32_t> z((std::size_t)FIX_NPIX, 0);
                AioHipsDiagTileView dv{};
                dv.parent_ipix = 0;
                dv.leaf_order = FIX_LEAF_ORDER;
                dv.width = 512;
                dv.nrej = z.data();
                dv.nused = nullptr;
                aio_hips_diag_tile_view_abi_init(&dv);
                P1HIPS_CHECK_EQ(cs, aio_hips_write_diag_tile(p2, &dv), 0);
                AioHipsDiagTileView bad_dv = dv;
                bad_dv.struct_size = 0;
                P1HIPS_CHECK_EQ(cs, aio_hips_write_diag_tile(p2, &bad_dv),
                                AIO_HIPS_ABI_MISMATCH);
                aio_hips_abort(p2);
            } else {
                P1HIPS_CHECK(cs, false, "n11abi_diag_begin");
            }
        }
        // (g) legacy aio_hips_write: AioHipsTile 数组同样逐元素 fail-closed
        {
            std::vector<float> sig((std::size_t)FIX_NPIX, 1.0f);
            std::vector<std::uint8_t> sup((std::size_t)FIX_NPIX, 255u);
            AioHipsTile t{};
            t.parent_ipix = 0;
            t.depth = FIX_LEAF_ORDER;
            t.signal = sig.data();
            t.support = sup.data();
            aio_hips_tile_abi_init(&t);
            const std::string d3 = make_tmp_dir("n11abi_legacy");
            P1HIPS_CHECK_EQ(cs, aio_hips_write(d3.c_str(), FIX_NSIDE, 512, &t, 1,
                                               AIO_HIPS_FLOAT32, nullptr, 0,
                                               "ivo://t", "t", 0), 0);
            AioHipsTile bad_t = t;
            bad_t.struct_size = 0;
            const std::string d4 = make_tmp_dir("n11abi_legacy_bad");
            P1HIPS_CHECK_EQ(cs, aio_hips_write(d4.c_str(), FIX_NSIDE, 512, &bad_t, 1,
                                               AIO_HIPS_FLOAT32, nullptr, 0,
                                               "ivo://t", "t", 0),
                            AIO_HIPS_ABI_MISMATCH);
        }
    }

    // --- DP (SCI-F3-001 增补): setter 参数域 / finalize 双向守卫 /
    //     write_diag_tile 值域 / verify 违反面逐条必败
    {
        const int dp_rc = test_diag_prov_negative();
        if (dp_rc != 0) cs.failures += 1;
    }

    if (cs.failures == 0) {
        std::fprintf(stdout, "[p1hips] negative: N1..N11 + DP-N1..DP-N4 PASS\n");
        return 0;
    }
    std::fprintf(stderr, "[p1hips] negative: %d check(s) failed\n", cs.failures);
    return 1;
}

}  // namespace p1hips
