// P1-HIPS-TEST · negative 组 (§9 负面矩阵逐行 + 错误码/last_error 合同)
//
// 覆盖 (HIPS_WRITER.md §9 负面矩阵): nside<512 / tile_width≠512 /
// 非法 dtype / 越位 flags / parent_ipix≥12·4^K / width≠512 / var_num NULL /
// 全无效 variance tile (−5) / FITS 路径不可写 (−4/−5/−6/−7) /
// prov pixfrac>1 (rc=2) / 重复 finalize (−2, I9) / NULL 句柄域 /
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
#include <cstring>
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

    // --- N5: provenance 域 (pixfrac>1/≤0 → 2; scale<0 → 2; NULL ps → 1)
    {
        const std::string dir = make_tmp_dir("n5");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "n5_begin");
        P1HIPS_CHECK_EQ(cs, aio_hips_set_drizzle_provenance(nullptr, 0.5, 1.0), 1);
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_set_drizzle_provenance(ps, 1.5, 1.0), 2);
            P1HIPS_CHECK_EQ(cs, aio_hips_set_drizzle_provenance(ps, 0.0, 1.0), 2);
            P1HIPS_CHECK_EQ(cs, aio_hips_set_drizzle_provenance(ps, 0.5, -1.0), 2);
            P1HIPS_CHECK_EQ(cs, aio_hips_set_drizzle_provenance(ps, 0.5, 0.1), 0);
            aio_hips_abort(ps);
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

    // --- DP (SCI-F3-001 增补): setter 参数域 / finalize 双向守卫 /
    //     write_diag_tile 值域 / verify 违反面逐条必败
    {
        const int dp_rc = test_diag_prov_negative();
        if (dp_rc != 0) cs.failures += 1;
    }

    if (cs.failures == 0) {
        std::fprintf(stdout, "[p1hips] negative: N1..N9 + DP-N1..DP-N4 PASS\n");
        return 0;
    }
    std::fprintf(stderr, "[p1hips] negative: %d check(s) failed\n", cs.failures);
    return 1;
}

}  // namespace p1hips
