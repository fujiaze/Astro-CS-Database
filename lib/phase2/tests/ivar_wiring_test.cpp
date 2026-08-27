// lib/phase2/tests/ivar_wiring_test.cpp — 生产 ivar wiring 集成测试
//
// 直接跑生产 Stage2（astrocs-stage2.exe），不是单独调用 reducer：
// - 3 帧合成 HiPS（signal/support/ivar 产品，单 tile）；
// - 每帧 ivar 空间 pattern 不同（A≈1、B≈4、C≈16 量级 + 空间变化）；
// - 故意制造像素级 invalid（support=0）使 eligibility compact；
// - 验证 WIRE-IVAR-001..005：
// 001 每个 eligible sample 得到自身 frame/pixel 的 ivar
// 002 invalid compact 后其余帧 ivar 不错位
// 003 期望 weighted mean 与生产输出匹配
// 004 改最后一帧 ivar 只影响该帧样本权重
// 005 帧排列置换后输出不变
#include <gtest/gtest.h>

#include "astro/phase2/upm.h"
#include "astro/phase2/sampler.h"
#include "healpix/healpix_core.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

extern "C" {
#include "aio_hips.h"
#include "aio_hips_reader.h"
}

namespace {

constexpr uint32_t kTileWidth = 512;
constexpr uint32_t kNside = 512;
constexpr uint64_t kTile = 0;
constexpr double kTruth = 10.0;

struct FrameSpec {
    std::string name;
    double phase;
    double ivar_base;     // 该帧 ivar 基础量级
    double ivar_slope;    // 空间 pattern 斜率（ivar = base + slope*(x+y)）
};

// 帧 C 空间 pattern 用常数乘子（WIRE-IVAR-004 用）
const std::vector<FrameSpec> kFrames = {
    {"A", 0.0, 1.0, 0.004},
    {"B", 1.0, 4.0, 0.010},
    {"C", 2.0, 16.0, 0.020},
};

std::string tmp_dir() {
    return "run/temp/"
           "v19r4_ivar_wiring";
}

void write_hips(const std::string& dir, const FrameSpec& spec,
                const std::vector<float>& signal,
                const std::vector<float>& support,
                const std::vector<float>& ivar) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        dir.c_str(), kNside, kTileWidth, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_ALL_V19,
        ("ivo://astrocs/test/" + spec.name).c_str(),
        ("ivar wiring frame " + spec.name).c_str(), "Red", 10.0,
        "2026-08-16", 7);
    if (!ps) {
        ADD_FAILURE() << "aio_hips_product_begin failed: "
                      << aio_hips_last_error();
        return;
    }
    AstroSphereTileView view{};
    std::memset(&view, 0, sizeof(view));
    view.parent_ipix = kTile;
    view.leaf_order = 9;    // log2(nside)=9（叶级 Norder）
    view.width = kTileWidth;
    view.data_type = AIO_HIPS_FLOAT32;
    // signal = flux_sum / covered_area；support = covered_area / A_cell
    std::vector<float> flux_sum(kTileWidth * kTileWidth, 0.0f);
    for (std::size_t i = 0; i < flux_sum.size(); ++i)
        flux_sum[i] = signal[i] * support[i];   // area 0/1
    view.flux_sum = flux_sum.data();
    view.covered_area = support.data();
    view.valid_mask = nullptr;
    if (aio_hips_write_signal_support_tile(ps, &view) != 0) {
        ADD_FAILURE() << "write_signal_support_tile failed: "
                      << aio_hips_last_error();
        aio_hips_abort(ps);
        return;
    }
    // variance = var_num_sum / covered_area²；area=1 → var_num_sum=1/ivar
    std::vector<float> var_num_sum(kTileWidth * kTileWidth, 0.0f);
    for (std::size_t i = 0; i < var_num_sum.size(); ++i)
        var_num_sum[i] = (support[i] > 0.0f && ivar[i] > 0.0f)
                             ? 1.0f / ivar[i] : 0.0f;
    view.var_num_sum = var_num_sum.data();
    if (aio_hips_write_variance_tile(ps, &view) != 0) {
        ADD_FAILURE() << "write_variance_tile failed: "
                      << aio_hips_last_error();
        aio_hips_abort(ps);
        return;
    }
    if (aio_hips_finalize(ps) != 0) {
        ADD_FAILURE() << "finalize failed: " << aio_hips_last_error();
        return;
    }
}

std::string write_config(const std::vector<std::string>& dirs,
                         const std::string& out_dir) {
    std::string cfg = tmp_dir() + "/stage2_wire.json";
    std::ofstream f(cfg);
    f << "{\"version\":1,\"inputs\":{\"hips\":[";
    for (std::size_t i = 0; i < dirs.size(); ++i) {
        if (i) f << ",";
        f << "\"" << dirs[i] << "\"";
    }
    f << "],\"target_order\":\"auto\"},\"model\":{\"robust_loss\":\"huber\","
         "\"snr_weight_mode\":\"snr2_normalized\",\"smoothing\":0.0},"
         "\"integration\":{\"precision\":\"fp32\",\"rejection\":{\"method\":"
         "\"none\"},\"weight_mode\":\"ivar\"},\"output\":{\"hips\":\""
      << out_dir << "\"},\"diagnostics\":{\"enabled\":true}}";
    f.close();
    return cfg;
}

std::string stage2_exe() {
#ifdef _WIN32
    return "astrocs-stage2.exe";
#else
    // 测试既可能在 build 目录运行，也可能在仓库根运行（如单独执行
    // ./build/linux-openmp-on/phase2_ivar_wiring）。
    for (const char* p : {"astrocs-stage2",
                          "build/linux-openmp-on/astrocs-stage2",
                          "build/linux-release/astrocs-stage2"}) {
        if (std::filesystem::exists(p)) return p;
    }
    return "astrocs-stage2";
#endif
}

// CON-006: 以固定 worker 数运行生产 stage2 CLI，用于 1T/2T 逐层差分。
int run_stage2(const std::string& cfg, int workers) {
    std::string cmd = stage2_exe() + " \"" + cfg + "\"";
    if (workers > 0) cmd += " --cpu-workers " + std::to_string(workers);
    return std::system(cmd.c_str());
}

std::vector<float> read_signal_tile(const std::string& hips) {
    // read_tile_f32 返回 FITS 顺序 buffer（一次读取，快速）
    AioHipsDataset* d = aio_hips_open(hips.c_str(), AIO_HIPS_RD_SIGNAL);
    std::vector<float> out(kTileWidth * kTileWidth, 0.0f);
    if (!d) {
        ADD_FAILURE() << "aio_hips_open failed: " << hips;
        return out;
    }
    if (aio_hips_read_tile_f32(d, kTile, out.data()) != 0) {
        ADD_FAILURE() << "read_tile_f32 failed: " << hips;
        out.clear();
    }
    aio_hips_close(d);
    return out;
}

std::vector<float> read_support_tile(const std::string& hips) {
    AioHipsDataset* d = aio_hips_open(hips.c_str(), AIO_HIPS_RD_SUPPORT);
    std::vector<float> out(kTileWidth * kTileWidth, 0.0f);
    if (!d) {
        ADD_FAILURE() << "aio_hips_open(support) failed: " << hips;
        return out;
    }
    if (aio_hips_read_tile_f32(d, kTile, out.data()) != 0) {
        ADD_FAILURE() << "read_support_tile_f32 failed: " << hips;
        out.clear();
    }
    aio_hips_close(d);
    return out;
}

// 期望加权均值：per-pixel eligible frames（support>0）按自身 ivar 加权
//calibrated = raw − C_frame(leaf)（生产 UPM 模型求值，非假设 offset=0）
std::vector<double> expected_weighted_mean(
    const std::vector<std::vector<float>>& signals,
    const std::vector<std::vector<float>>& supports,
    const std::vector<std::vector<float>>& ivars,
    const std::vector<std::string>& dirs,
    const std::string& upm_sparse_path) {
    const std::size_t n = signals[0].size();
    void* model = nullptr;
    if (p2_upm_open(upm_sparse_path.c_str(), &model) != 0) {
        ADD_FAILURE() << "p2_upm_open failed: " << upm_sparse_path;
        return std::vector<double>(n, 0.0);
    }
    std::vector<std::uint64_t> fids;
    for (const auto& d : dirs) fids.push_back(p2_frame_id(d.c_str()));
    std::vector<double> out(n, 0.0);
    // 数据数组即 NESTED 顺序；期望遍历 NESTED leaf z
    for (std::uint64_t z = 0; z < n; ++z) {
        double wsum = 0.0, vs = 0.0;
        for (std::size_t f = 0; f < signals.size(); ++f) {
            if (supports[f][(std::size_t)z] <= 0.0f) continue;
            if (!std::isfinite(signals[f][(std::size_t)z])) continue;
            const double w = (double)ivars[f][(std::size_t)z];
            if (!(w > 0.0)) continue;                  // 零/非法权重不贡献
            const std::uint64_t leaf = (kTile << 18) | z;
            const double c = p2_upm_evaluate_c(model, fids[f], leaf);
            const double cal = (double)signals[f][(std::size_t)z] - c;
            wsum += w;
            vs += w * cal;
        }
        out[(std::size_t)z] = (wsum > 0.0) ? vs / wsum : 0.0;
    }
    p2_upm_close(model);
    return out;
}

}  // namespace

TEST(Phase2IvarWiring, WireProductionStage2PerFrameIvar) {
    std::filesystem::create_directories(tmp_dir());
    const std::size_t npix = (std::size_t)kTileWidth * kTileWidth;
    std::vector<std::vector<float>> signals, supports, ivars;
    std::vector<std::string> dirs;
    for (const auto& spec : kFrames) {
        std::vector<float> sig(npix), sup(npix, 1.0f), iv(npix);
        // 数据数组按 NESTED local 索引（写入器期望 NESTED 序）
        for (std::uint64_t z = 0; z < npix; ++z) {
            std::uint32_t nx = 0, ny = 0;
            astrocs::healpix::nested_local_to_xy(z, 9u, nx, ny);
            const int x = (int)nx;
            const int y = (int)ny;
            // 确定性值：truth + 亚像素正弦（UPM offset≈0，保持帧间差异）
            sig[(std::size_t)z] =
                (float)(kTruth +
                        0.5 * std::sin(spec.phase + 0.01 * (double)x +
                                       0.02 * (double)y));
            // ivar 空间 pattern：base + slope*(x+y)
            iv[(std::size_t)z] =
                (float)(spec.ivar_base +
                        spec.ivar_slope * (double)(x + y));
        }
        // WIRE-IVAR-002：每帧制造不同 invalid 区域（按 FITS 行带定义，
        // 经 fits_index_to_nested_local 落位）
        auto invalid_fits_band = [&](int y0, int y1) {
            for (std::uint64_t z = 0; z < npix; ++z) {
                const std::uint64_t fi =
                    astrocs::healpix::nested_local_to_fits_index(
                        z, 9u, kTileWidth);
                const int fy = (int)(fi / kTileWidth);
                if (fy >= y0 && fy < y1) sup[(std::size_t)z] = 0.0f;
            }
        };
        if (spec.name == "A") invalid_fits_band(0, 64);
        else if (spec.name == "B") invalid_fits_band(192, 256);
        else invalid_fits_band(384, 448);
        const std::string dir = tmp_dir() + "/" + spec.name + ".hips";
        write_hips(dir, spec, sig, sup, iv);
        EXPECT_TRUE(std::filesystem::exists(dir + "/signal/properties"))
            << "HiPS 未生成: " << dir;
        signals.push_back(std::move(sig));
        supports.push_back(std::move(sup));
        ivars.push_back(std::move(iv));
        dirs.push_back(dir);
    }

    const std::string out1 = tmp_dir() + "/out_abc.hips";
    std::filesystem::remove_all(out1);
    const std::string cfg1 = write_config(dirs, out1);
    const int rc = run_stage2(cfg1, 1);
    ASSERT_EQ(rc, 0) << "stage2 (A,B,C) 运行失败 rc=" << rc;
    const auto got1 = read_signal_tile(out1);
    ASSERT_EQ(got1.size(), npix);

    // 期望（WIRE-IVAR-001/002/003 真值）：生产 UPM 模型求值 calibrated
    const auto expect = expected_weighted_mean(
        signals, supports, ivars, dirs, out1 + "/upm_sparse.json");

    // WIRE-IVAR-003：生产输出（FITS buffer，leaf z 在 fits(z) 处）
    // vs 期望（NESTED z）
    std::size_t n_checked = 0;
    for (std::uint64_t z = 0; z < npix; ++z) {
        const std::uint64_t fits =
            astrocs::healpix::nested_local_to_fits_index(
                (std::uint32_t)z, 9u, kTileWidth);
        const bool any_valid = supports[0][(std::size_t)z] > 0 ||
                               supports[1][(std::size_t)z] > 0 ||
                               supports[2][(std::size_t)z] > 0;
        if (!any_valid) continue;
        if (std::fabs((double)got1[(std::size_t)fits] -
                      expect[(std::size_t)z]) >
            0.05) {
            ADD_FAILURE() << "leaf=" << z
                          << " got=" << got1[(std::size_t)fits]
                          << " expected=" << expect[(std::size_t)z];
        }
        ++n_checked;
    }
    EXPECT_GT(n_checked, npix / 2);

    // CON-006: stage2 逐 pixel integration 1T/2T 逐层差分。
    // 同一合成输入分别 cpu_workers=1/=2 运行；signal/support 图层必须一致。
    const std::string out1_2t = tmp_dir() + "/out_abc_2t.hips";
    std::filesystem::remove_all(out1_2t);
    const std::string cfg1_2t = write_config(dirs, out1_2t);
    ASSERT_EQ(run_stage2(cfg1_2t, 2), 0);
    const auto got1_2t = read_signal_tile(out1_2t);
    const auto sup1 = read_support_tile(out1);
    const auto sup1_2t = read_support_tile(out1_2t);
    ASSERT_EQ(got1_2t.size(), npix);
    ASSERT_EQ(sup1.size(), npix);
    ASSERT_EQ(sup1_2t.size(), npix);
    std::size_t n_sig_diff = 0, n_sup_diff = 0;
    double max_sig_diff = 0.0, max_sup_diff = 0.0;
    for (std::size_t p = 0; p < npix; ++p) {
        const double ds = std::fabs((double)got1[p] - (double)got1_2t[p]);
        const double du = std::fabs((double)sup1[p] - (double)sup1_2t[p]);
        if (ds > 1e-4) ++n_sig_diff;
        if (du > 1e-4) ++n_sup_diff;
        max_sig_diff = std::max(max_sig_diff, ds);
        max_sup_diff = std::max(max_sup_diff, du);
    }
    EXPECT_EQ(n_sig_diff, 0u)
        << "CON-006: signal 图层 1T/2T 差数=" << n_sig_diff
        << " max=" << max_sig_diff;
    EXPECT_EQ(n_sup_diff, 0u)
        << "CON-006: support 图层 1T/2T 差数=" << n_sup_diff
        << " max=" << max_sup_diff;

    // WIRE-IVAR-004：把最后一帧（C）ivar 整体 ×4 后重跑，输出应变化；
    // 且 A/B 单独贡献的样本（C invalid 区域）不受影响。
    std::vector<std::vector<float>> ivars2 = ivars;
    for (std::size_t p = 0; p < npix; ++p) ivars2[2][p] *= 4.0f;
    std::vector<std::string> dirs2 = dirs;
    // 重写 C 帧（ivar ×4）
    {
        std::vector<float> sig = signals[2], sup = supports[2];
        const std::string dirC = tmp_dir() + "/Cx4.hips";
        write_hips(dirC, kFrames[2], sig, sup, ivars2[2]);
        dirs2[2] = dirC;
    }
    const std::string out2 = tmp_dir() + "/out_abc_x4.hips";
    std::filesystem::remove_all(out2);
    const std::string cfg2 = write_config(dirs2, out2);
    ASSERT_EQ(run_stage2(cfg2, 1), 0);
    const auto got2 = read_signal_tile(out2);
    // C-invalid 区域（A/B 两帧）输出必须不变（C 权重变化不影响 A/B）
    std::size_t n_same = 0, n_diff = 0;
    for (int y = 384; y < 448; ++y) {
        for (int x = 0; x < 512; ++x) {
            const std::size_t p = (std::size_t)y * kTileWidth + (std::size_t)x;
            // invalid 带按 FITS (x,y) 定义；两个输出 buffer 同序（FITS），
            // 同像素位置直接比较
            if (std::fabs((double)got1[p] - (double)got2[p]) > 1e-4)
                ++n_diff;
            else
                ++n_same;
        }
    }
    EXPECT_EQ(n_diff, 0u)
        << "WIRE-IVAR-004: C invalid 区域输出被 C ivar 变化污染";
    EXPECT_GT(n_same, 0u);
    // 全局输出应有变化（C 参与区域）
    std::size_t n_global_diff = 0;
    for (std::size_t z = 0; z < npix; ++z)
        if (std::fabs((double)got1[z] - (double)got2[z]) > 1e-4)
            ++n_global_diff;
    EXPECT_GT(n_global_diff, npix / 4);

    // WIRE-IVAR-005：帧排列置换（C,A,B）后科学输出不变
    std::vector<std::string> perm = {dirs[2], dirs[0], dirs[1]};
    const std::string out3 = tmp_dir() + "/out_cab.hips";
    std::filesystem::remove_all(out3);
    const std::string cfg3 = write_config(perm, out3);
    ASSERT_EQ(run_stage2(cfg3, 1), 0);
    const auto got3 = read_signal_tile(out3);
    std::size_t n_perm_diff = 0;
    double max_diff = 0.0;
    for (std::size_t p = 0; p < npix; ++p) {
        const double d = std::fabs((double)got1[p] - (double)got3[p]);
        if (d > 1e-4) ++n_perm_diff;
        max_diff = std::max(max_diff, d);
    }
    EXPECT_EQ(n_perm_diff, 0u)
        << "WIRE-IVAR-005: 帧置换改变输出 (max_diff=" << max_diff << ")";
}
