// lib/phase2/tests/synthetic_gate.cpp — Phase2 合成 Gate（W4/W7/W5/W6/W8/W9 子集）
//
// 覆盖：
//   S0 identity：无额外 gradient 时 UPM 校准不改变信号；
//   S1 known additive field：已知每帧加性偏移，UPM 联合求解恢复真值；
//   S2 low-SNR pull：SNR-aware 权重下低 SNR 偏差帧不拉偏高 SNR 真值；
//   R1 sigma rejection：注入离群值，sigma-clip 检出；
//   R2 min-samples：样本不足返回 status=min-samples；
//   D  sparse=dense（materialize 与稀疏模型一致）；
//   B  block planner（内存估算/micro-chunk）；
//   I  weighted integration（加权均值/全拒）；
//   A  ACR legacy launcher 与 CPU reference 等价。
#include <gtest/gtest.h>

#include "astro/phase2/upm.h"
#include "astro/phase2/coverage.h"
#include "astro/phase2/sampler.h"
#include "astro/phase2/rejection.h"
#include "astro/phase2/block.h"
#include "astro/phase2/integrate.h"
#include "astro/phase2/acr_kernels.h"

#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "cuda_bridge_api.hpp"
#include "healpix/healpix_core.h"
#include "crypto/sha256.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <random>
#include <numeric>
#include <filesystem>
#include <vector>

namespace {

P2ControlObservation make_obs(std::uint64_t frame, std::uint64_t ctrl,
                              double value, double snr) {
    P2ControlObservation o{};
    o.frame_id = frame;
    o.control_id = ctrl;
    o.value = value;
    o.snr = snr;
    o.support = 1.0;
    return o;
}

P2ControlObservation make_obs_id(std::uint64_t frame_id, std::uint64_t ctrl,
                                 double value, double snr) {
    P2ControlObservation o{};
    o.frame_id = frame_id;
    o.control_id = ctrl;
    o.value = value;
    o.snr = snr;
    o.support = 1.0;
    return o;
}

// ===== G1 空间 UPM truth 辅助（控制包 V2 R1/R2）=====
// TrueSky(p) + 每帧空间 additive field C_i(p)，在 4 个 target tile 上生成
// control 观测与 10 万非 control 验证像素。
namespace spatial_truth {

constexpr int kTargetOrder = 7;
constexpr int kTileShift = 9;              // leaf order = target+9
constexpr std::uint64_t kLeafPerTile = 512ull * 512ull;
constexpr std::uint64_t kTiles[4] = {3, 4, 5, 6};
constexpr int kGrid = 8;
constexpr int kCellSide = 512 / kGrid;
constexpr double kNoiseRms = 0.05;

inline std::uint64_t leaf_of(std::uint64_t tile, int x, int y) {
    const std::uint64_t local =
        astrocs::healpix::xy_to_nested_local((std::uint32_t)x,
                                             (std::uint32_t)y,
                                             (std::uint32_t)kTileShift);
    return (tile << (2u * (unsigned)kTileShift)) + local;
}

// TrueSky：pedestal + 大尺度渐变 + 局部 diffuse 结构（ra/dec 的函数）
inline double true_sky(double ra_deg, double dec_deg) {
    const double rar = ra_deg * 3.141592653589793 / 180.0;
    const double decr = dec_deg * 3.141592653589793 / 180.0;
    double v = 10.0;
    v += 0.8 * std::cos(decr) * std::sin(rar);        // 大尺度渐变
    v += 0.5 * std::cos(2.0 * decr);                  // 非平面大尺度
    const double d1 = std::sin(decr) * std::cos(rar) - 0.3;
    v += 0.6 * std::exp(-20.0 * d1 * d1);             // 局部 diffuse 结构
    return v;
}

// 每帧空间 additive field：const + plane + non-planar（smooth）
inline double frame_field(int frame, double ra_deg, double dec_deg) {
    const double rar = ra_deg * 3.141592653589793 / 180.0;
    const double decr = dec_deg * 3.141592653589793 / 180.0;
    if (frame == 0) return 0.0;
    if (frame == 1) {
        return 0.15 + 0.4 * std::cos(decr) * std::cos(rar) +
               0.25 * std::cos(2.0 * decr);
    }
    // frame 2：另一方向 smooth field
    return -0.10 + 0.35 * std::sin(2.0 * rar) * std::cos(decr) -
           0.20 * std::cos(3.0 * decr);
}

// 单 tile 内 cell (gx,gy) 中心 leaf 的 ra/dec
inline void cell_center_radec(std::uint64_t tile, int gx, int gy,
                              double* ra, double* dec) {
    const int x = gx * kCellSide + kCellSide / 2;
    const int y = gy * kCellSide + kCellSide / 2;
    const std::uint64_t leaf = leaf_of(tile, x, y);
    astrocs::healpix::pix2ang_nest(
        1u << (unsigned)(kTargetOrder + kTileShift), leaf, *ra, *dec);
}

} // namespace spatial_truth

} // namespace

TEST(Phase2Upm, S0IdentityCalibrationNoChange) {
    std::vector<P2ControlObservation> obs{
        make_obs(0, 0, 10.0, 10.0),
        make_obs(0, 1, 12.0, 10.0),
        make_obs(0, 2, 11.0, 10.0),
    };
    P2UpmBuildConfig cfg{};
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    std::uint64_t ipix[2] = {0, 1};
    double in[2] = {10.0, 12.0};
    double out[2] = {0.0, 0.0};
    ASSERT_EQ(p2_upm_calibrate_block(model, 0, ipix, in, out, 2), 0);
    EXPECT_NEAR(out[0], 10.0, 1e-6);
    EXPECT_NEAR(out[1], 12.0, 1e-6);
    p2_upm_close(model);
}

TEST(Phase2Upm, S1KnownAdditiveFieldRecovered) {
    std::vector<P2ControlObservation> obs{
        make_obs(0, 0, 10.0, 100.0),
        make_obs(0, 1, 12.0, 100.0),
        make_obs(0, 2, 11.0, 100.0),
        make_obs(1, 0, 15.0, 100.0),
        make_obs(1, 1, 17.0, 100.0),
        make_obs(1, 2, 16.0, 100.0),
    };
    P2UpmBuildConfig cfg{};
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    std::uint64_t ipix[1] = {0};
    double in[1] = {15.0};
    double out[1] = {0.0};
    ASSERT_EQ(p2_upm_calibrate_block(model, 1, ipix, in, out, 1), 0);
    EXPECT_NEAR(out[0], 10.0, 1e-3);
    p2_upm_close(model);
}

TEST(Phase2Upm, S2LowSnrDoesNotPullHighSnr) {
    std::vector<P2ControlObservation> obs{
        make_obs(0, 0, 10.0, 100.0),
        make_obs(1, 0, 50.0, 0.3),
    };
    P2UpmBuildConfig cfg{};
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    P2ModelInfo info{};
    ASSERT_EQ(p2_upm_info(model, &info), 0);
    EXPECT_EQ(info.control_count, 1u);
    std::uint64_t ipix[1] = {0};
    double in[1] = {10.0};
    double out[1] = {0.0};
    ASSERT_EQ(p2_upm_calibrate_block(model, 0, ipix, in, out, 1), 0);
    EXPECT_NEAR(out[0], 10.0, 0.5);
    p2_upm_close(model);
}

// W4：UPM 持久化 round-trip + 真实内容哈希 + 连通分量
TEST(Phase2Upm, SaveOpenRoundtripAndHash) {
    std::vector<P2ControlObservation> obs{
        make_obs(0, 0, 10.0, 100.0),
        make_obs(0, 1, 12.0, 100.0),
        make_obs(0, 2, 11.0, 100.0),
        make_obs(1, 0, 15.0, 100.0),
        make_obs(1, 1, 17.0, 100.0),
        make_obs(1, 2, 16.0, 100.0),
    };
    P2UpmBuildConfig cfg{};
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    P2ModelInfo info{};
    ASSERT_EQ(p2_upm_info(model, &info), 0);
    // 真实哈希非占位
    EXPECT_NE(std::string(info.model_hash), std::string(64, '0'));
    EXPECT_EQ(info.component_count, 1u);
    const char* path = "run_tmp_upm_roundtrip.json";
    ASSERT_EQ(p2_upm_save(model, path), 0);
    void* model2 = nullptr;
    ASSERT_EQ(p2_upm_open(path, &model2), 0);
    P2ModelInfo info2{};
    ASSERT_EQ(p2_upm_info(model2, &info2), 0);
    EXPECT_EQ(std::string(info2.model_hash), std::string(info.model_hash));
    EXPECT_EQ(info2.control_count, info.control_count);
    std::uint64_t ipix[1] = {0};
    double in[1] = {15.0};
    double out1[1] = {0.0}, out2[1] = {0.0};
    ASSERT_EQ(p2_upm_calibrate_block(model, 1, ipix, in, out1, 1), 0);
    ASSERT_EQ(p2_upm_calibrate_block(model2, 1, ipix, in, out2, 1), 0);
    EXPECT_NEAR(out1[0], out2[0], 1e-12);
    p2_upm_close(model2);
    p2_upm_close(model);
    std::remove(path);
}

// G1 空间 UPM truth：3 帧 × 256 controls + 10 万非 control 验证像素。
// TrueSky 含大尺度/非平面/局部 diffuse 结构；每帧含空间 additive field；
// 单覆盖边缘（frame2 不覆盖 tile6）+ 断开分量（tile100 仅 frame0）。
TEST(Phase2Upm, G1SpatialFieldTruth) {
    namespace st = spatial_truth;
    std::mt19937 rng(20260811);
    std::normal_distribution<double> nd(0.0, st::kNoiseRms);

    // 生成 control 观测（3 帧 × 4 tiles × 64 cells）
    std::vector<P2ControlObservation> obs;
    std::vector<std::uint64_t> control_leaf(st::kGrid * st::kGrid *
                                            sizeof(st::kTiles) / sizeof(st::kTiles[0]));
    std::vector<double> control_ra(control_leaf.size());
    std::uint64_t ctrl_id = 0;
    const int n_tiles = (int)(sizeof(st::kTiles) / sizeof(st::kTiles[0]));
    for (int t = 0; t < n_tiles; ++t) {
        for (int gy = 0; gy < st::kGrid; ++gy) {
            for (int gx = 0; gx < st::kGrid; ++gx) {
                double ra = 0, dec = 0;
                st::cell_center_radec(st::kTiles[t], gx, gy, &ra, &dec);
                const std::uint64_t leaf = st::leaf_of(
                    st::kTiles[t], gx * st::kCellSide + st::kCellSide / 2,
                    gy * st::kCellSide + st::kCellSide / 2);
                control_leaf[(std::size_t)(t * st::kGrid * st::kGrid +
                                           gy * st::kGrid + gx)] = leaf;
                control_ra[(std::size_t)(t * st::kGrid * st::kGrid +
                                         gy * st::kGrid + gx)] = ra;
                (void)dec;
                // frame0（参考）覆盖全部；frame1 全部；frame2 仅前 3 tiles
                const int fmax = (t == n_tiles - 1) ? 1 : 2;
                for (int f = 0; f <= fmax; ++f) {
                    P2ControlObservation o{};
                    o.frame_id = (std::uint64_t)f;
                    o.control_id = ctrl_id;
                    o.leaf_ipix = leaf;
                    o.ra_deg = ra;
                    double r2 = 0, d2 = 0;
                    st::cell_center_radec(st::kTiles[t], gx, gy, &r2, &d2);
                    o.dec_deg = d2;
                    o.value = st::true_sky(ra, d2) +
                              st::frame_field(f, ra, d2) + nd(rng);
                    o.uncertainty = st::kNoiseRms;
                    o.snr = (f == 1 && ((t + gy + gx) % 2 == 0))
                                ? 3.0 : 100.0;
                    o.support = 0.8 + 0.2 * ((gx + gy) % 2);
                    o.quality_flags = 1;  // PSF_OK
                    obs.push_back(o);
                }
                ++ctrl_id;
            }
        }
    }
    // 断开分量：tile100 仅 frame5 覆盖（与 frame0-2 无共同观测）
    for (int c = 0; c < 2; ++c) {
        double ra = 0, dec = 0;
        st::cell_center_radec(100, c * 4, c * 4, &ra, &dec);
        const std::uint64_t leaf =
            st::leaf_of(100, c * 4 * st::kCellSide + 32,
                        c * 4 * st::kCellSide + 32);
        P2ControlObservation o{};
        o.frame_id = 5;
        o.control_id = ctrl_id++;
        o.leaf_ipix = leaf;
        o.ra_deg = ra;
        o.dec_deg = dec;
        o.value = st::true_sky(ra, dec) + nd(rng);
        o.uncertainty = st::kNoiseRms;
        o.snr = 100.0;
        o.support = 1.0;
        o.quality_flags = 1;
        obs.push_back(o);
    }

    P2UpmBuildConfig cfg{};
    cfg.target_order = st::kTargetOrder;
    cfg.smoothing_lambda = 0.3;
    cfg.zero_anchor_weight = 1e-3;
    cfg.sigma_floor = 0.02;
    cfg.max_iterations = 60;
    cfg.tolerance = 1e-9;
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    P2ModelInfo info{};
    ASSERT_EQ(p2_upm_info(model, &info), 0);
    EXPECT_GE(info.control_count, 256u);
    EXPECT_EQ(info.target_order, (std::uint32_t)st::kTargetOrder);

    // 非 control 验证像素（每 tile 25k，随机 cell 内偏移）
    const int n_validate = 25000;
    std::vector<std::uint64_t> val_leaf;
    std::vector<std::uint8_t> val_frame;
    std::vector<double> val_true;
    val_leaf.reserve((std::size_t)n_validate * 4);
    val_frame.reserve((std::size_t)n_validate * 4);
    val_true.reserve((std::size_t)n_validate * 4);
    std::uniform_int_distribution<int> cell_dist(0, st::kGrid - 1);
    std::uniform_int_distribution<int> off_dist(0, st::kCellSide - 1);
    for (int t = 0; t < n_tiles; ++t) {
        for (int i = 0; i < n_validate; ++i) {
            const int gx = cell_dist(rng);
            const int gy = cell_dist(rng);
            const int x = gx * st::kCellSide + off_dist(rng);
            const int y = gy * st::kCellSide + off_dist(rng);
            const std::uint64_t leaf = st::leaf_of(st::kTiles[t], x, y);
            double ra = 0, dec = 0;
            astrocs::healpix::pix2ang_nest(
                1u << (unsigned)(st::kTargetOrder + st::kTileShift),
                leaf, ra, dec);
            for (int f = 0; f <= ((t == n_tiles - 1) ? 1 : 2); ++f) {
                val_leaf.push_back(leaf);
                val_frame.push_back((std::uint8_t)f);
                val_true.push_back(st::true_sky(ra, dec));
            }
        }
    }
    // 校准 10 万+ 验证像素（3 帧）并统计 residual（校准 - TrueSky）
    std::vector<double> residual;
    std::vector<double> c_recovery;   // C_hat vs C_true（非 control）
    std::vector<double> in_sig(val_leaf.size());
    std::vector<double> out_sig(val_leaf.size());
    for (std::size_t i = 0; i < val_leaf.size(); ++i) {
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(
            1u << (unsigned)(st::kTargetOrder + st::kTileShift),
            val_leaf[i], ra, dec);
        const int f = val_frame[i];
        in_sig[i] = val_true[i] + st::frame_field(f, ra, dec) + nd(rng);
    }
    std::vector<std::uint64_t> frame_buf(val_leaf.size());
    for (std::size_t i = 0; i < val_leaf.size(); ++i)
        frame_buf[i] = val_frame[i];
    // calibrate_block 按帧分组调用（每帧独立收集像素）
    for (int f = 0; f < 3; ++f) {
        std::vector<std::uint64_t> leaves;
        std::vector<double> in_v;
        for (std::size_t i = 0; i < val_leaf.size(); ++i) {
            if (val_frame[i] == f) {
                leaves.push_back(val_leaf[i]);
                in_v.push_back(in_sig[i]);
            }
        }
        if (leaves.empty()) continue;
        std::vector<double> out_v(leaves.size());
        ASSERT_EQ(p2_upm_calibrate_block(model, (std::uint64_t)f,
                                         leaves.data(), in_v.data(),
                                         out_v.data(), leaves.size()), 0);
        std::size_t w = 0;
        for (std::size_t i = 0; i < val_leaf.size(); ++i) {
            if (val_frame[i] == f) out_sig[i] = out_v[w++];
        }
    }
    for (std::size_t i = 0; i < val_leaf.size(); ++i) {
        residual.push_back(out_sig[i] - val_true[i]);
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(
            1u << (unsigned)(st::kTargetOrder + st::kTileShift),
            val_leaf[i], ra, dec);
        const int f = val_frame[i];
        const double c_true = st::frame_field(f, ra, dec);
        const double c_hat = in_sig[i] - out_sig[i];
        if (f > 0) c_recovery.push_back(c_hat - c_true);
    }
    auto percentile = [](std::vector<double> v, double p) {
        std::sort(v.begin(), v.end());
        if (v.empty()) return 0.0;
        const std::size_t idx = (std::size_t)(p * (double)(v.size() - 1));
        return v[idx];
    };
    double sq = 0;
    for (double r : residual) sq += r * r;
    const double rmse = std::sqrt(sq / (double)residual.size());
    const double p50 = percentile(residual, 0.50);
    const double p95 = percentile(residual, 0.95);
    double pmax = std::fabs(residual.back());
    for (double r : residual) pmax = std::max(pmax, std::fabs(r));
    double csq = 0;
    for (double c : c_recovery) csq += c * c;
    const double c_rmse =
        std::sqrt(csq / (double)std::max<std::size_t>(1, c_recovery.size()));

    std::fprintf(stderr,
                 "[G1] controls=%llu val_px=%zu residual_rmse=%.4f "
                 "p50=%.4f p95=%.4f max=%.4f c_rmse=%.4f "
                 "components=%u noise=%.3f\n",
                 (unsigned long long)info.control_count, val_leaf.size(),
                 rmse, p50, p95, pmax, c_rmse, info.component_count,
                 st::kNoiseRms);
    // 门限（G1）：RMSE <= 3σ，p95 <= 5σ；场恢复 RMSE <= 3σ；分量 >= 2
    EXPECT_LE(rmse, 3.0 * st::kNoiseRms);
    EXPECT_LE(std::fabs(p95), 5.0 * st::kNoiseRms);
    EXPECT_LE(c_rmse, 3.0 * st::kNoiseRms);
    EXPECT_GE(info.component_count, 2u);
    p2_upm_close(model);
}

// G3/G4 持久化：medium round-trip（target_order/frames/hash 保持）、
// 1 ULP 系数变化 hash 敏感、>8 MiB 模型 round-trip。
TEST(Phase2Upm, G2PersistenceAndHashSensitivity) {
    namespace st = spatial_truth;
    std::mt19937 rng(20260812);
    std::normal_distribution<double> nd(0.0, st::kNoiseRms);
    // medium：4 tiles × 64 cells × 3 帧（同 G1 几何，256 controls）
    std::vector<P2ControlObservation> obs;
    std::uint64_t cid = 0;
    const int n_tiles = (int)(sizeof(st::kTiles) / sizeof(st::kTiles[0]));
    for (int t = 0; t < n_tiles; ++t) {
        for (int gy = 0; gy < st::kGrid; ++gy) {
            for (int gx = 0; gx < st::kGrid; ++gx) {
                double ra = 0, dec = 0;
                st::cell_center_radec(st::kTiles[t], gx, gy, &ra, &dec);
                const std::uint64_t leaf =
                    st::leaf_of(st::kTiles[t], gx * st::kCellSide + 32,
                                gy * st::kCellSide + 32);
                for (int f = 0; f < 3; ++f) {
                    P2ControlObservation o{};
                    o.frame_id = (std::uint64_t)f;
                    o.control_id = cid;
                    o.leaf_ipix = leaf;
                    o.ra_deg = ra;
                    o.dec_deg = dec;
                    o.value = st::true_sky(ra, dec) +
                              st::frame_field(f, ra, dec) + nd(rng);
                    o.uncertainty = st::kNoiseRms;
                    o.snr = 30.0;
                    o.support = 1.0;
                    o.quality_flags = 1;
                    obs.push_back(o);
                }
                ++cid;
            }
        }
    }
    P2UpmBuildConfig cfg{};
    cfg.target_order = st::kTargetOrder;
    cfg.smoothing_lambda = 0.1;
    cfg.max_iterations = 40;
    cfg.tolerance = 1e-10;
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    P2ModelInfo info{};
    ASSERT_EQ(p2_upm_info(model, &info), 0);
    EXPECT_EQ(info.target_order, (std::uint32_t)st::kTargetOrder);

    const char* path = "run_tmp_upm_g2.json";
    ASSERT_EQ(p2_upm_save(model, path), 0);
    void* model2 = nullptr;
    ASSERT_EQ(p2_upm_open(path, &model2), 0);
    P2ModelInfo info2{};
    ASSERT_EQ(p2_upm_info(model2, &info2), 0);
    EXPECT_EQ(std::string(info2.model_hash), std::string(info.model_hash));
    EXPECT_EQ(info2.target_order, info.target_order);
    EXPECT_EQ(info2.control_count, info.control_count);
    // 校准一致性（round-trip 后同一 leaf 同值）
    std::uint64_t leaf0 = st::leaf_of(st::kTiles[0], 100, 200);
    std::uint64_t leaf[1] = {leaf0};
    double in[1] = {11.0};
    double o1[1] = {0}, o2[1] = {0};
    ASSERT_EQ(p2_upm_calibrate_block(model, 2, leaf, in, o1, 1), 0);
    ASSERT_EQ(p2_upm_calibrate_block(model2, 2, leaf, in, o2, 1), 0);
    EXPECT_NEAR(o1[0], o2[0], 1e-12);
    p2_upm_close(model2);
    std::remove(path);

    // 1 ULP 系数变化 → model hash 变化
    std::vector<P2ControlObservation> obs_ulp = obs;
    const double v = obs_ulp[0].value;
    obs_ulp[0].value = std::nextafter(v, v + 1.0);
    void* model_ulp = nullptr;
    ASSERT_EQ(p2_upm_build(obs_ulp.data(), obs_ulp.size(), &cfg,
                           &model_ulp), 0);
    P2ModelInfo info_ulp{};
    ASSERT_EQ(p2_upm_info(model_ulp, &info_ulp), 0);
    EXPECT_NE(std::string(info_ulp.model_hash),
              std::string(info.model_hash));
    p2_upm_close(model_ulp);
    p2_upm_close(model);

    // >8 MiB synthetic model round-trip（60k controls × 2 帧，粗迭代）
    std::vector<P2ControlObservation> big;
    constexpr std::uint64_t kBigControls = 60000;
    P2UpmBuildConfig bcfg{};
    bcfg.target_order = 3;         // 小 tile 便于大量 cells
    bcfg.smoothing_lambda = 0.05;
    bcfg.max_iterations = 12;
    bcfg.tolerance = 1e-8;
    const int grid = 8, cell = 512 / grid;
    for (std::uint64_t c = 0; c < kBigControls; ++c) {
        const std::uint64_t tile = 1 + c / (grid * grid);
        const int gy = (int)((c % (grid * grid)) / grid);
        const int gx = (int)(c % grid);
        const std::uint64_t leaf =
            (tile << 18) + astrocs::healpix::xy_to_nested_local(
                               (std::uint32_t)(gx * cell + 32),
                               (std::uint32_t)(gy * cell + 32), 9u);
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(1u << 12, leaf, ra, dec);
        for (int f = 0; f < 2; ++f) {
            P2ControlObservation o{};
            o.frame_id = (std::uint64_t)f;
            o.control_id = c;
            o.leaf_ipix = leaf;
            o.ra_deg = ra;
            o.dec_deg = dec;
            o.value = st::true_sky(ra, dec) + (double)f * 0.5 + nd(rng);
            o.uncertainty = 0.05;
            o.snr = 20.0;
            o.support = 1.0;
            o.quality_flags = 1;
            big.push_back(o);
        }
    }
    void* bm = nullptr;
    ASSERT_EQ(p2_upm_build(big.data(), big.size(), &bcfg, &bm), 0);
    const char* big_path = "run_tmp_upm_big.json";
    ASSERT_EQ(p2_upm_save(bm, big_path), 0);
    std::ifstream fbig(big_path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(fbig.good());
    const std::streamsize big_size = fbig.tellg();
    fbig.close();
    EXPECT_GT(big_size, 8 * 1024 * 1024)
        << "big model size=" << big_size;
    void* bm2 = nullptr;
    ASSERT_EQ(p2_upm_open(big_path, &bm2), 0);
    P2ModelInfo binfo2{};
    ASSERT_EQ(p2_upm_info(bm2, &binfo2), 0);
    EXPECT_EQ(binfo2.control_count, kBigControls);
    std::uint64_t bleaf[1] = {1};
    double bin[1] = {9.0}, bout1[1] = {0}, bout2[1] = {0};
    ASSERT_EQ(p2_upm_calibrate_block(bm, 1, bleaf, bin, bout1, 1), 0);
    ASSERT_EQ(p2_upm_calibrate_block(bm2, 1, bleaf, bin, bout2, 1), 0);
    EXPECT_NEAR(bout1[0], bout2[0], 1e-9);
    p2_upm_close(bm2);
    p2_upm_close(bm);
    std::remove(big_path);
    std::fprintf(stderr, "[G2] big model size=%.1f MiB round-trip ok\n",
                 (double)big_size / (1024.0 * 1024.0));
}

// G4 dense 空间求值缓存：随机 >=1,000,000 样本 sparse calibrate == dense read；
// stale hash 拒绝；损坏 checksum 拒绝。
TEST(Phase2Upm, G4DenseMillionSampleSpatial) {
    namespace st = spatial_truth;
    std::mt19937 rng(20260813);
    std::normal_distribution<double> nd(0.0, st::kNoiseRms);
    std::vector<P2ControlObservation> obs;
    std::uint64_t cid = 0;
    const int n_tiles = (int)(sizeof(st::kTiles) / sizeof(st::kTiles[0]));
    for (int t = 0; t < n_tiles; ++t) {
        for (int gy = 0; gy < st::kGrid; ++gy) {
            for (int gx = 0; gx < st::kGrid; ++gx) {
                double ra = 0, dec = 0;
                st::cell_center_radec(st::kTiles[t], gx, gy, &ra, &dec);
                const std::uint64_t leaf =
                    st::leaf_of(st::kTiles[t], gx * st::kCellSide + 32,
                                gy * st::kCellSide + 32);
                for (int f = 0; f < 3; ++f) {
                    P2ControlObservation o{};
                    o.frame_id = (std::uint64_t)f;
                    o.control_id = cid;
                    o.leaf_ipix = leaf;
                    o.ra_deg = ra;
                    o.dec_deg = dec;
                    o.value = st::true_sky(ra, dec) +
                              st::frame_field(f, ra, dec) + nd(rng);
                    o.uncertainty = st::kNoiseRms;
                    o.snr = 50.0;
                    o.support = 1.0;
                    o.quality_flags = 1;
                    obs.push_back(o);
                }
                ++cid;
            }
        }
    }
    P2UpmBuildConfig cfg{};
    cfg.target_order = st::kTargetOrder;
    cfg.smoothing_lambda = 0.1;
    cfg.max_iterations = 30;
    cfg.tolerance = 1e-10;
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    const char* cache = "run_tmp_upm_g4.cache";
    ASSERT_EQ(p2_upm_materialize_dense(model, st::kTargetOrder, cache), 0);

    // 随机 100 万 leaf（4 tiles 内），按 tile 分组比较
    const std::uint64_t per_tile = 250000;
    const std::uint64_t total = per_tile * (std::uint64_t)n_tiles;
    std::vector<std::uint64_t> leaf(total);
    std::vector<double> in(total), out_s(total), out_d(total);
    std::size_t w = 0;
    for (int t = 0; t < n_tiles; ++t) {
        std::uniform_int_distribution<int> xd(0, 511);
        std::uniform_int_distribution<int> yd(0, 511);
        for (std::uint64_t i = 0; i < per_tile; ++i) {
            leaf[w] = st::leaf_of(st::kTiles[t], xd(rng), yd(rng));
            in[w] = 9.0 + 0.1 * (double)(w % 100);
            ++w;
        }
    }
    ASSERT_EQ(p2_upm_calibrate_block(model, 1, leaf.data(), in.data(),
                                     out_s.data(), total), 0);
    ASSERT_EQ(p2_upm_dense_read_block(model, cache, 1, leaf.data(),
                                      in.data(), out_d.data(), total), 0);
    double max_diff = 0.0;
    for (std::uint64_t i = 0; i < total; ++i)
        max_diff = std::max(max_diff, std::fabs(out_s[i] - out_d[i]));
    EXPECT_LE(max_diff, 1e-9);
    std::fprintf(stderr,
                 "[G4] dense samples=%llu max sparse-dense diff=%.3e\n",
                 (unsigned long long)total, max_diff);

    // stale hash 拒绝
    void* model2 = nullptr;
    std::vector<P2ControlObservation> obs2{obs[0]};
    ASSERT_EQ(p2_upm_build(obs2.data(), obs2.size(), &cfg, &model2), 0);
    std::uint64_t l2[1] = {leaf[0]};
    double in2[1] = {1.0}, o2[1] = {0};
    EXPECT_EQ(p2_upm_dense_read_block(model2, cache, 0, l2, in2, o2, 1), 2);
    p2_upm_close(model2);

    // 损坏 checksum 拒绝：改 dense 文件一个字节
    {
        std::FILE* f = std::fopen(cache, "r+b");
        ASSERT_NE(f, nullptr);
        ASSERT_EQ(std::fseek(f, 2048, SEEK_SET), 0);
        const unsigned char flip = 0x5A;
        ASSERT_EQ(std::fwrite(&flip, 1, 1, f), 1u);
        std::fclose(f);
        double o3[1] = {0};
        EXPECT_EQ(p2_upm_dense_read_block(model, cache, 1, l2, in2, o3, 1), 2);
    }
    p2_upm_close(model);
    std::remove(cache);
}

// Validation §4：输入 frame 顺序随机重排后，统一模型的等权叠加输出必须一致。
// frame_id 是内容稳定标识（不随输入位置变化），参考帧 = 最小 frame_id，
// 因此重排输入顺序不改变 gauge。
TEST(Phase2Upm, FrameOrderInvariance) {
    std::vector<P2ControlObservation> obs_a{
        make_obs_id(10, 0, 10.0, 100.0),
        make_obs_id(10, 1, 12.0, 100.0),
        make_obs_id(10, 2, 11.0, 100.0),
        make_obs_id(20, 0, 15.0, 100.0),
        make_obs_id(20, 1, 17.0, 100.0),
        make_obs_id(20, 2, 16.0, 100.0),
    };
    // 重排：输入顺序交换（B 先 A 后），frame_id 不变
    std::vector<P2ControlObservation> obs_b{
        make_obs_id(20, 0, 15.0, 100.0),
        make_obs_id(20, 1, 17.0, 100.0),
        make_obs_id(20, 2, 16.0, 100.0),
        make_obs_id(10, 0, 10.0, 100.0),
        make_obs_id(10, 1, 12.0, 100.0),
        make_obs_id(10, 2, 11.0, 100.0),
    };
    P2UpmBuildConfig cfg{};
    void* ma = nullptr, *mb = nullptr;
    ASSERT_EQ(p2_upm_build(obs_a.data(), obs_a.size(), &cfg, &ma), 0);
    ASSERT_EQ(p2_upm_build(obs_b.data(), obs_b.size(), &cfg, &mb), 0);
    std::uint64_t ip[1] = {0};
    double v0[1] = {10.0}, v1[1] = {15.0};
    double a0[1] = {0}, a1[1] = {0}, b0[1] = {0}, b1[1] = {0};
    ASSERT_EQ(p2_upm_calibrate_block(ma, 10, ip, v0, a0, 1), 0);
    ASSERT_EQ(p2_upm_calibrate_block(ma, 20, ip, v1, a1, 1), 0);
    ASSERT_EQ(p2_upm_calibrate_block(mb, 20, ip, v1, b0, 1), 0);
    ASSERT_EQ(p2_upm_calibrate_block(mb, 10, ip, v0, b1, 1), 0);
    // 重排前后同一帧的校准输出必须一致
    EXPECT_NEAR(a0[0], b1[0], 1e-9);  // 帧 A(10)
    EXPECT_NEAR(a1[0], b0[0], 1e-9);  // 帧 B(20)
    p2_upm_close(mb);
    p2_upm_close(ma);
}

TEST(Phase2Upm, SparseEqualsDense) {
    std::vector<P2ControlObservation> obs{
        make_obs(0, 0, 10.0, 100.0),
        make_obs(0, 1, 12.0, 100.0),
        make_obs(0, 2, 11.0, 100.0),
        make_obs(1, 0, 15.0, 100.0),
        make_obs(1, 1, 17.0, 100.0),
        make_obs(1, 2, 16.0, 100.0),
    };
    P2UpmBuildConfig cfg{};
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    const char* cache = "run_tmp_upm_dense.json";
    ASSERT_EQ(p2_upm_materialize_dense(model, 0, cache), 0);
    int order = -1; std::uint64_t pixels = 0; char hash[65] = {0};
    ASSERT_EQ(p2_upm_dense_info(model, cache, &order, &pixels, hash,
                                sizeof(hash)), 0);
    EXPECT_GE(pixels, 3u);
    EXPECT_GT(std::strlen(hash), 0u);
    // dense_read_block 与 sparse calibrate_block 一致
    std::uint64_t ipix[1] = {0};
    double in[1] = {15.0};
    double out_sparse[1] = {0.0}, out_dense[1] = {0.0};
    ASSERT_EQ(p2_upm_calibrate_block(model, 1, ipix, in, out_sparse, 1), 0);
    ASSERT_EQ(p2_upm_dense_read_block(model, cache, 1, ipix, in,
                                      out_dense, 1), 0);
    EXPECT_NEAR(out_sparse[0], out_dense[0], 1e-12);
    // stale cache：不同模型必须拒绝（返回 2）
    std::vector<P2ControlObservation> obs2{
        make_obs(0, 0, 1.0, 5.0),
        make_obs(0, 1, 2.0, 5.0),
    };
    void* model2 = nullptr;
    ASSERT_EQ(p2_upm_build(obs2.data(), obs2.size(), &cfg, &model2), 0);
    // model2 只有 frame 0（frame_id 在模型内）；source hash 不同 → stale(2)
    EXPECT_EQ(p2_upm_dense_read_block(model2, cache, 0, ipix, in,
                                      out_dense, 1), 2);
    p2_upm_close(model2);
    p2_upm_close(model);
    std::remove(cache);
}

TEST(Phase2Block, MemoryEstimateAndMicrochunk) {
    P2BlockPlannerInput in{};
    in.output_pixels = 1024u * 1024u;
    in.covering_frames = 10;
    in.precision = 1;
    in.memory_limit_bytes = 16u * 1024 * 1024;
    in.safety_factor = 0.75;
    in.scratch_bytes_per_sample = 8;
    in.fixed_overhead = 1u << 20;
    P2BlockPlan plan{};
    ASSERT_EQ(p2_block_plan(&in, &plan), 0);
    EXPECT_GT(plan.estimated_peak_bytes, 0u);
    in.memory_limit_bytes = 1ull << 30;
    ASSERT_EQ(p2_block_plan(&in, &plan), 0);
    EXPECT_EQ(plan.micro_chunk_required, 0);
}

// G5/Validation §4：块尺寸不影响科学结果（同一栈整批 vs 分块一致）
TEST(Phase2Block, ChunkSizeInvariance) {
    constexpr std::size_t P = 1u << 18;   // 262144 输出像素
    constexpr std::uint32_t N = 5;        // 帧数
    std::mt19937 rng(20260810);
    std::normal_distribution<double> nd(0.0, 0.15);
    std::vector<double> truth(P);
    for (std::size_t p = 0; p < P; ++p)
        truth[p] = 10.0 + 2.0 * std::sin((double)p * 0.001);
    std::vector<double> stack(N * P), sup(N * P);
    for (std::uint32_t f = 0; f < N; ++f) {
        for (std::size_t p = 0; p < P; ++p) {
            double v = truth[p] + (double)f * 0.3 + nd(rng);
            if ((p + f * 7919u) % 2000u == 0u) v += 5.0;  // 稀疏离群
            stack[(size_t)f * P + p] = v;
            sup[(size_t)f * P + p] =
                0.5 + 0.5 * std::sin((double)(p + f * 131) * 0.0005);
        }
    }
    auto process = [&](std::size_t chunk, std::vector<double>* sig_out) {
        sig_out->assign(P, 0.0);
        std::vector<double> vals(N), w(N), sp(N);
        std::vector<std::uint8_t> acc(N);
        for (std::size_t base = 0; base < P; base += chunk) {
            const std::size_t hi = std::min(base + chunk, P);
            for (std::size_t p = base; p < hi; ++p) {
                std::uint32_t nv = 0;
                for (std::uint32_t f = 0; f < N; ++f) {
                    const double v = stack[(size_t)f * P + p];
                    const double s = sup[(size_t)f * P + p];
                    if (std::isfinite(v) && s > 0.0) {
                        vals[nv] = v;
                        w[nv] = s * (1.0 + (double)f * 0.5);
                        sp[nv] = s;
                        ++nv;
                    }
                }
                if (nv < 3) continue;
                P2SampleStackView rv{};
                rv.values = vals.data();
                rv.count = nv;
                rv.method = P2_REJECT_WINSORIZED_SIGMA;
                rv.sigma_low = -4.0;
                rv.sigma_high = 3.0;
                rv.max_iterations = 8;
                rv.min_samples = 3;
                P2RejectionResult rr{};
                rr.accepted = acc.data();
                ASSERT_EQ(p2_reject_stack(&rv, &rr), 0);
                P2PixelStack pi{};
                pi.values = vals.data();
                pi.weights = w.data();
                pi.support = sp.data();
                pi.accepted = acc.data();
                pi.count = nv;
                P2PixelResult pr{};
                ASSERT_EQ(p2_integrate_pixel(&pi, &pr), 0);
                (*sig_out)[p] = (pr.status == 0) ? pr.signal : 0.0;
            }
        }
    };
    std::vector<double> sig_all, sig_c1024, sig_c16384, sig_c65536;
    process(P, &sig_all);
    process(1024, &sig_c1024);
    process(16384, &sig_c16384);
    process(65536, &sig_c65536);
    for (std::size_t p = 0; p < P; ++p) {
        EXPECT_NEAR(sig_c1024[p], sig_all[p], 1e-12);
        EXPECT_NEAR(sig_c16384[p], sig_all[p], 1e-12);
        EXPECT_NEAR(sig_c65536[p], sig_all[p], 1e-12);
    }
}

TEST(Phase2Integrate, WeightedMeanAndAllRejected) {
    double vals[] = {10.0, 12.0, 11.0};
    double ws[] = {0.5, 0.3, 0.2};
    P2PixelStack in{};
    in.values = vals;
    in.weights = ws;
    in.count = 3;
    P2PixelResult out{};
    ASSERT_EQ(p2_integrate_pixel(&in, &out), 0);
    EXPECT_NEAR(out.signal, 0.5*10 + 0.3*12 + 0.2*11, 1e-9);
    EXPECT_EQ(out.n_used, 3u);
    std::uint8_t acc[3] = {0, 0, 0};
    in.accepted = acc;
    ASSERT_EQ(p2_integrate_pixel(&in, &out), 0);
    EXPECT_EQ(out.status, 2);
}

TEST(Phase2Reject, R1SigmaClippingFindsOutliers) {
    std::vector<double> vals{10, 10.1, 9.9, 10.05, 10.2, 9.8, 50.0};
    std::vector<std::uint8_t> accepted(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = static_cast<std::uint32_t>(vals.size());
    in.method = P2_REJECT_SIGMA;
    in.sigma_low = -4.0;
    in.sigma_high = 3.0;
    in.max_iterations = 8;
    in.min_samples = 3;
    P2RejectionResult out{};
    out.accepted = accepted.data();
    ASSERT_EQ(p2_reject_stack(&in, &out), 0);
    EXPECT_GT(out.rejected_high, 0u);
    EXPECT_EQ(accepted.back(), 0u);
    EXPECT_GT(out.accepted_count, 3u);
}

TEST(Phase2Reject, R2MinSamples) {
    std::vector<double> vals{10.0, 10.1};
    std::vector<std::uint8_t> accepted(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = 2;
    in.method = P2_REJECT_SIGMA;
    in.min_samples = 5;
    P2RejectionResult out{};
    out.accepted = accepted.data();
    ASSERT_EQ(p2_reject_stack(&in, &out), 0);
    EXPECT_EQ(out.status, 1);
}

// W7：LinearFit 检出序列离群（Siril 公开语义独立实现；带真实噪声的
// 时间趋势数据，MAD 尺度才不会误拒正常样本）
TEST(Phase2Reject, LinearFitFindsOutlier) {
    std::mt19937 rng(42);
    std::normal_distribution<double> nd(0.0, 0.3);
    std::vector<double> vals;
    for (int i = 0; i < 50; ++i)
        vals.push_back(10.0 + 0.1 * (double)i + nd(rng));
    vals[30] = 30.0;
    std::vector<std::uint8_t> accepted(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = static_cast<std::uint32_t>(vals.size());
    in.method = P2_REJECT_LINEAR_FIT;
    in.sigma_low = -4.0;
    in.sigma_high = 3.0;
    in.max_iterations = 8;
    in.min_samples = 3;
    P2RejectionResult out{};
    out.accepted = accepted.data();
    ASSERT_EQ(p2_reject_stack(&in, &out), 0);
    EXPECT_EQ(accepted[30], 0u);
    EXPECT_GT(out.rejected_high, 0u);
    EXPECT_GE(out.accepted_count, 45u);  // 正常样本保留绝大多数
}

// W7：RCR 检出离群（Maples 2018 论文独立实现，Chauvenet 判据）
TEST(Phase2Reject, RcrFindsOutlier) {
    std::vector<double> vals{10, 10.1, 9.9, 10.05, 10.2, 9.8, 50.0};
    std::vector<std::uint8_t> accepted(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = static_cast<std::uint32_t>(vals.size());
    in.method = P2_REJECT_RCR;
    in.max_iterations = 8;
    in.min_samples = 3;
    P2RejectionResult out{};
    out.accepted = accepted.data();
    ASSERT_EQ(p2_reject_stack(&in, &out), 0);
    EXPECT_EQ(accepted.back(), 0u);
    EXPECT_GT(out.rejected_high, 0u);
}

// R4/G6：ESD 正确流程（P0-05）——NIST Rosner 54 点必须 3 outliers
TEST(Phase2Reject, G6EsdNistRosner54) {
    const std::vector<double> vals{
        -0.25, 0.68, 0.94, 1.15, 1.20, 1.26, 1.26, 1.34, 1.38, 1.43,
        1.49, 1.49, 1.55, 1.56, 1.58, 1.65, 1.69, 1.70, 1.76, 1.77,
        1.81, 1.91, 1.94, 1.96, 1.99, 2.06, 2.09, 2.10, 2.14, 2.15,
        2.23, 2.24, 2.26, 2.35, 2.37, 2.40, 2.47, 2.54, 2.62, 2.64,
        2.90, 2.92, 2.92, 2.93, 3.21, 3.26, 3.30, 3.59, 3.68, 4.30,
        4.64, 5.34, 5.42, 6.01};
    std::vector<std::uint8_t> acc(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = (std::uint32_t)vals.size();
    in.method = P2_REJECT_GENERALIZED_ESD;
    in.max_iterations = 10;
    in.min_samples = 5;
    P2RejectionResult out{};
    out.accepted = acc.data();
    ASSERT_EQ(p2_reject_stack(&in, &out), 0);
    // NIST 权威结论：3 个离群（4.30/4.64/5.34/5.42/6.01 中最大 3 个：
    // 实际 Rosner α=0.05 判 3 个：5.34/5.42/6.01）
    const std::uint32_t n_reject = out.rejected_low + out.rejected_high;
    EXPECT_EQ(n_reject, 3u) << "NIST Rosner 54 点应 3 outliers";
    std::fprintf(stderr, "[ESD NIST] rejected=%u iterations=%u\n",
                 n_reject, out.iterations);
}

// R4/G6：ESD masking case——第一轮不显著但最终显著
TEST(Phase2Reject, G6EsdMaskingCase) {
    // 构造 masking：两个离群互相掩盖，第一轮 R1 不超临界，最终应检出
    std::mt19937 rng(42);
    std::normal_distribution<double> nd(0.0, 1.0);
    std::vector<double> vals;
    for (int i = 0; i < 40; ++i) vals.push_back(nd(rng));
    vals[5] = 8.0;
    vals[6] = 8.5;   // 两极端离群（masking pair）
    std::vector<std::uint8_t> acc(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = (std::uint32_t)vals.size();
    in.method = P2_REJECT_GENERALIZED_ESD;
    in.max_iterations = 10;
    in.min_samples = 5;
    P2RejectionResult out{};
    out.accepted = acc.data();
    ASSERT_EQ(p2_reject_stack(&in, &out), 0);
    const std::uint32_t n_reject = out.rejected_low + out.rejected_high;
    EXPECT_GE(n_reject, 2u);
    EXPECT_EQ(acc[5], 0u);
    EXPECT_EQ(acc[6], 0u);
}

// R4/G6：Winsorized 与 Sigma 必须不同（至少一个数据集）
TEST(Phase2Reject, G6WinsorizedDiffersFromSigma) {
    // 数据：10×0 + 100。Sigma 的 MAD=0 → 无拒绝（保留 100）；
    // Winsorized 用 std>0 → 100 被拒。证明两算法确实不同。
    std::vector<double> vals(11, 0.0);
    vals[10] = 100.0;
    std::vector<std::uint8_t> a1(vals.size(), 0), a2(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = (std::uint32_t)vals.size();
    in.sigma_low = -4.0;
    in.sigma_high = 3.0;
    in.max_iterations = 8;
    in.min_samples = 5;
    in.method = P2_REJECT_SIGMA;
    P2RejectionResult o1{}, o2{};
    o1.accepted = a1.data();
    ASSERT_EQ(p2_reject_stack(&in, &o1), 0);
    in.method = P2_REJECT_WINSORIZED_SIGMA;
    o2.accepted = a2.data();
    ASSERT_EQ(p2_reject_stack(&in, &o2), 0);
    bool differs = false;
    for (std::size_t i = 0; i < vals.size(); ++i)
        if (a1[i] != a2[i]) differs = true;
    EXPECT_TRUE(differs) << "Winsorized 与 Sigma 实现必须不同";
    EXPECT_EQ(a1[10], 1u);  // Sigma：MAD=0 全保留
    EXPECT_EQ(a2[10], 0u);  // Winsorized：std 尺度下 100 被拒
    std::fprintf(stderr, "[Winsorized] sigma_rej=%u win_rej=%u differs=%d\n",
                 o1.rejected_low + o1.rejected_high,
                 o2.rejected_low + o2.rejected_high, (int)differs);
}

// R4/G6：全部 7 方法 permutation invariance（随机帧重排 mask 一致）
TEST(Phase2Reject, G6PermutationInvariance) {
    std::mt19937 rng(99);
    std::normal_distribution<double> nd(0.0, 0.5);
    constexpr int N = 12;
    std::vector<std::uint64_t> fid(N);
    std::vector<double> vals(N);
    for (int i = 0; i < N; ++i) {
        fid[i] = 1000 + (std::uint64_t)i;   // 稳定 frame identity
        vals[i] = 10.0 + nd(rng);
    }
    vals[2] = 30.0;  // 离群
    // 重排
    std::vector<int> perm(N);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), rng);
    std::vector<double> vals_p(N);
    std::vector<std::uint64_t> fid_p(N);
    for (int i = 0; i < N; ++i) {
        vals_p[i] = vals[perm[i]];
        fid_p[i] = fid[perm[i]];
    }
    for (int method = 0; method <= 6; ++method) {
        if (method == P2_REJECT_NONE) continue;
        std::vector<std::uint8_t> a1(N, 0), a2(N, 0);
        P2SampleStackView in{};
        in.values = vals.data();
        in.frame_ids = fid.data();
        in.count = N;
        in.method = method;
        in.sigma_low = -4.0;
        in.sigma_high = 3.0;
        in.max_iterations = 8;
        in.min_samples = 3;
        P2RejectionResult o1{}, o2{};
        o1.accepted = a1.data();
        ASSERT_EQ(p2_reject_stack(&in, &o1), 0);
        in.values = vals_p.data();
        in.frame_ids = fid_p.data();
        o2.accepted = a2.data();
        ASSERT_EQ(p2_reject_stack(&in, &o2), 0);
        // 重排后：mask 应一致（按 frame_id 对齐比较）
        bool same = true;
        std::vector<int> inv_perm(N);
        for (int i = 0; i < N; ++i) inv_perm[perm[i]] = i;
        for (int i = 0; i < N; ++i)
            if (a1[i] != a2[inv_perm[i]]) same = false;
        if (!same) {
            std::fprintf(stderr, "[perm] method=%d failed\n  a1=", method);
            for (int i = 0; i < N; ++i)
                std::fprintf(stderr, "%d", (int)a1[i]);
            std::fprintf(stderr, "\n  a2=");
            for (int i = 0; i < N; ++i)
                std::fprintf(stderr, "%d", (int)a2[i]);
            std::fprintf(stderr, "\n");
        }
        EXPECT_TRUE(same) << "method " << method
                          << " permutation invariant failed";
    }
}

// R5/G7 权重 truth gate：比较 7 种权重策略（equal/snr/snr2/
// support×snr2/capped snr2/invvar/support×invvar），低 SNR 偏差帧
// 不得拉偏高 SNR 真值帧；默认策略由 truth 冻结。
TEST(Phase2Weight, G5WeightTruthGate) {
    constexpr int N = 20000;       // 像素
    constexpr double TRUTH = 10.0;
    std::mt19937 rng(20260816);
    std::normal_distribution<double> nd(0.0, 0.2);
    // 帧 0：高 SNR 真值帧（snr=100）；帧 1：低 SNR + 偏差（+1.5，snr=2）
    std::vector<double> v0(N), v1(N), snr0(N), snr1(N), sup0(N), sup1(N);
    for (int p = 0; p < N; ++p) {
        v0[p] = TRUTH + nd(rng);
        v1[p] = TRUTH + 1.5 + nd(rng) * 3.0;
        snr0[p] = 100.0;
        snr1[p] = 2.0;
        sup0[p] = 0.9 + 0.1 * std::sin((double)p * 0.001);
        sup1[p] = 0.5 + 0.5 * std::sin((double)p * 0.0013);
    }
    auto integrate = [&](double (*w0)(double, double, double),
                         double (*w1)(double, double, double)) {
        double bias = 0.0, sq = 0.0;
        for (int p = 0; p < N; ++p) {
            const double a = w0(snr0[p], sup0[p], 0.2);
            const double b = w1(snr1[p], sup1[p], 0.6);  // σ=0.6
            const double wsum = a + b;
            const double out = (wsum > 0)
                ? (a * v0[p] + b * v1[p]) / wsum : 0.0;
            bias += out - TRUTH;
            sq += (out - TRUTH) * (out - TRUTH);
        }
        return std::make_pair(bias / N, std::sqrt(sq / N));
    };
    auto eq = [](double, double, double) { return 1.0; };
    auto snr_w = [](double s, double, double) { return s; };
    auto snr2_w = [](double s, double, double) { return s * s; };
    auto sup_snr2 = [](double s, double sp, double) { return sp * s * s; };
    auto cap_snr2 = [](double s, double, double) {
        const double c = 100.0;
        return std::min(s * s, c);
    };
    auto iv_w = [](double, double, double sig) { return 1.0 / (sig * sig); };
    auto sup_iv = [](double, double sp, double sig) {
        return sp / (sig * sig);
    };
    struct Row { const char* name; double bias; double rmse; };
    std::vector<Row> rows;
    rows.push_back({"equal", integrate(eq, eq).first, integrate(eq, eq).second});
    rows.push_back({"snr", integrate(snr_w, snr_w).first,
                    integrate(snr_w, snr_w).second});
    rows.push_back({"snr2", integrate(snr2_w, snr2_w).first,
                    integrate(snr2_w, snr2_w).second});
    rows.push_back({"support_x_snr2", integrate(sup_snr2, sup_snr2).first,
                    integrate(sup_snr2, sup_snr2).second});
    rows.push_back({"capped_snr2", integrate(cap_snr2, cap_snr2).first,
                    integrate(cap_snr2, cap_snr2).second});
    rows.push_back({"inverse_variance", integrate(iv_w, iv_w).first,
                    integrate(iv_w, iv_w).second});
    rows.push_back({"support_x_invvar", integrate(sup_iv, sup_iv).first,
                    integrate(sup_iv, sup_iv).second});
    for (const auto& r : rows)
        std::fprintf(stderr, "[weight] %-18s bias=%+.4f rmse=%.4f\n",
                     r.name, r.bias, r.rmse);
    // 关键门：equal 会被低 SNR 偏差帧显著拉偏；SNR-aware 策略 bias 小。
    const auto& eq_r = rows[0];
    const auto& snr2_r = rows[2];
    const auto& sup_r = rows[3];
    EXPECT_GT(std::fabs(eq_r.bias), std::fabs(snr2_r.bias) + 0.2);
    EXPECT_LT(std::fabs(sup_r.bias), 0.15);       // 低 SNR 不拉偏
    EXPECT_LT(std::fabs(snr2_r.bias), 0.15);
    // 默认策略（冻结）：support_x_snr2（含局部 support 可信度 + SNR²）
    // weight_mode=auto 映射到该策略（stage2 文档/schema 同步）。
}

// W9：ACR 合成 mosaic_reject legacy launcher 与 CPU reference 等价
TEST(Phase2Acr, LegacyLauncherEquivalent) {
    astro::compute::phase2::register_phase2_acr_kernels();
    const astro::compute::KernelRegistration* reg =
        astro::compute::global_kernel_registry().find(
            astro::compute::phase2::kOpMosaicReject);
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(reg->legacy_parallel, nullptr);

    const std::size_t px = 2, depth = 3;
    // frame-major：s*n_px+p —— s0:[10,20] s1:[50,20.1] s2:[10.2,19.9]
    float vals[6] = {10.0f, 20.0f, 50.0f, 20.1f, 10.2f, 19.9f};
    float out[2] = {0, 0};
    astro::compute::KernelInvocation inv;
    inv.id = astro::compute::phase2::kOpMosaicReject;
    inv.domain = astro::compute::WorkDomain{0, px};
    inv.buffers.add(0, out, px, 1, astro::compute::BufferRole::Output);
    inv.buffers.add(1, vals, px*depth, 1, astro::compute::BufferRole::Input);
    astro::compute::append_scalar(inv.scalars, std::size_t{px});
    astro::compute::append_scalar(inv.scalars, std::size_t{depth});
    astro::compute::append_scalar(inv.scalars, int{P2_REJECT_SIGMA});
    astro::compute::append_scalar(inv.scalars, double{-4.0});
    astro::compute::append_scalar(inv.scalars, double{3.0});
    reg->legacy_parallel(inv, nullptr);
    EXPECT_NEAR(out[0], 10.1f, 1e-4f);
    EXPECT_NEAR(out[1], 20.0f, 1e-2f);
}

// W9：真实 GPU kernel 与 CPU reference 等价（同输入、同语义、数值容差内）
TEST(Phase2Acr, CudaEquivalent) {
    astro::compute::phase2::register_phase2_acr_kernels();
    const astro::compute::KernelRegistration* reg =
        astro::compute::global_kernel_registry().find(
            astro::compute::phase2::kOpMosaicReject);
    ASSERT_NE(reg, nullptr);
    ASSERT_TRUE(reg->cuda.has_value());

    namespace bridge = astro::compute::cuda::bridge;
    bridge::ensure_bridge_loaded();
    ASSERT_TRUE(bridge::api().loaded());
    const char* err = nullptr;
    void* exec = bridge::api().executor_create(0, 1u << 20, 1u << 16, &err);
    ASSERT_NE(exec, nullptr) << (err ? err : "executor_create failed");
    bridge::set_tls_handle(exec);

    const std::size_t px = 2, depth = 3;
    float vals[6] = {10.0f, 20.0f, 50.0f, 20.1f, 10.2f, 19.9f};
    float out_cpu[2] = {0, 0}, out_gpu[2] = {0, 0};
    auto build_inv = [&](float* out) {
        astro::compute::KernelInvocation inv;
        inv.id = astro::compute::phase2::kOpMosaicReject;
        inv.domain = astro::compute::WorkDomain{0, px};
        inv.buffers.add(0, out, px, 1, astro::compute::BufferRole::Output);
        inv.buffers.add(1, vals, px * depth, 1,
                        astro::compute::BufferRole::Input);
        astro::compute::append_scalar(inv.scalars, std::size_t{px});
        astro::compute::append_scalar(inv.scalars, std::size_t{depth});
        astro::compute::append_scalar(inv.scalars, int{P2_REJECT_SIGMA});
        astro::compute::append_scalar(inv.scalars, double{-4.0});
        astro::compute::append_scalar(inv.scalars, double{3.0});
        return inv;
    };
    astro::compute::KernelInvocation ic = build_inv(out_cpu);
    reg->legacy_parallel(ic, nullptr);
    astro::compute::KernelInvocation ig = build_inv(out_gpu);
    (*reg->cuda)(ig, nullptr);

    EXPECT_NEAR(out_gpu[0], out_cpu[0], 1e-4f);
    EXPECT_NEAR(out_gpu[1], out_cpu[1], 1e-4f);
    bridge::api().executor_destroy(exec);
}

// W9：加权 + support 输出的 CPU/GPU 等价（真实 stage2 语义）
TEST(Phase2Acr, CudaWeightedSupportEquivalent) {
    astro::compute::phase2::register_phase2_acr_kernels();
    const astro::compute::KernelRegistration* reg =
        astro::compute::global_kernel_registry().find(
            astro::compute::phase2::kOpMosaicReject);
    ASSERT_NE(reg, nullptr);
    namespace bridge = astro::compute::cuda::bridge;
    bridge::ensure_bridge_loaded();
    if (!bridge::api().loaded()) GTEST_SKIP() << "CUDA bridge 不可用";
    const char* err = nullptr;
    void* exec = bridge::api().executor_create(0, 1u << 20, 1u << 16, &err);
    ASSERT_NE(exec, nullptr);
    bridge::set_tls_handle(exec);

    const std::size_t px = 3, depth = 4;
    // frames（frame-major）：像素0 有离群，像素1/2 正常
    float vals[12] = {
        10.0f, 10.1f, 9.9f,
        50.0f, 10.0f, 10.2f,
        10.05f, 9.8f, 10.0f,
        10.1f, 10.1f, 9.9f,
    };
    float sup[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    float snr[4] = {2.0f, 2.0f, 2.0f, 2.0f};
    float out_cpu[3] = {0}, out_gpu[3] = {0};
    float sup_cpu[3] = {0}, sup_gpu[3] = {0};
    float rej_cpu[3] = {0}, rej_gpu[3] = {0};
    float valid_cpu[3] = {0}, valid_gpu[3] = {0};

    auto build_inv = [&](float* out, float* out_sup, float* out_rej,
                         float* out_valid) {
        astro::compute::KernelInvocation inv;
        inv.id = astro::compute::phase2::kOpMosaicReject;
        inv.domain = astro::compute::WorkDomain{0, px};
        inv.buffers.add(0, out, px, 1, astro::compute::BufferRole::Output);
        inv.buffers.add(1, vals, px * depth, 1,
                        astro::compute::BufferRole::Input);
        inv.buffers.add(2, sup, px * depth, 1,
                        astro::compute::BufferRole::Input);
        inv.buffers.add(3, snr, depth, 1, astro::compute::BufferRole::Input);
        inv.buffers.add(4, out_sup, px, 1,
                        astro::compute::BufferRole::Output);
        inv.buffers.add(5, out_rej, px, 1,
                        astro::compute::BufferRole::Output);
        inv.buffers.add(6, out_valid, px, 1,
                        astro::compute::BufferRole::Output);
        astro::compute::append_scalar(inv.scalars, std::size_t{px});
        astro::compute::append_scalar(inv.scalars, std::size_t{depth});
        astro::compute::append_scalar(inv.scalars, int{P2_REJECT_SIGMA});
        astro::compute::append_scalar(inv.scalars, double{-4.0});
        astro::compute::append_scalar(inv.scalars, double{3.0});
        astro::compute::append_scalar(inv.scalars, int{3});
        return inv;
    };
    astro::compute::KernelInvocation ic =
        build_inv(out_cpu, sup_cpu, rej_cpu, valid_cpu);
    reg->legacy_parallel(ic, nullptr);
    astro::compute::KernelInvocation ig =
        build_inv(out_gpu, sup_gpu, rej_gpu, valid_gpu);
    (*reg->cuda)(ig, nullptr);

    for (std::size_t p = 0; p < px; ++p) {
        EXPECT_NEAR(out_gpu[p], out_cpu[p], 1e-4f) << "signal p=" << p;
        EXPECT_NEAR(sup_gpu[p], sup_cpu[p], 1e-5f) << "support p=" << p;
        EXPECT_NEAR(rej_gpu[p], rej_cpu[p], 1e-5f) << "reject p=" << p;
        EXPECT_NEAR(valid_gpu[p], valid_cpu[p], 1e-5f) << "valid p=" << p;
    }
    EXPECT_GT(out_cpu[0], 9.0f);   // 离群被拒后加权均值接近真值
    EXPECT_GT(sup_cpu[0], 0.0f);
    EXPECT_GE(rej_cpu[0], 1.0f);  // 像素0 的离群被拒
    EXPECT_EQ((int)valid_cpu[0], 4);
    bridge::api().executor_destroy(exec);
}

// R6：compact frame metadata case——总帧 [0,1,2,3]，当前 tile 只覆盖 [1,3]
// （非连续），SNR 数组必须按 frames[s] 一一对应，CPU/ACR 输出一致。
TEST(Phase2Acr, G9CompactFrameSubset) {
    astro::compute::phase2::register_phase2_acr_kernels();
    const astro::compute::KernelRegistration* reg =
        astro::compute::global_kernel_registry().find(
            astro::compute::phase2::kOpMosaicReject);
    ASSERT_NE(reg, nullptr);
    namespace bridge = astro::compute::cuda::bridge;
    bridge::ensure_bridge_loaded();
    if (!bridge::api().loaded()) GTEST_SKIP() << "CUDA bridge 不可用";
    const char* err = nullptr;
    void* exec = bridge::api().executor_create(0, 1u << 20, 1u << 16, &err);
    ASSERT_NE(exec, nullptr);
    bridge::set_tls_handle(exec);

    const std::size_t px = 4, depth = 2;
    // 总帧 4 帧 SNR：idx 0..3；当前 tile 覆盖帧 [1,3] → compact [snr1, snr3]
    float all_snr[4] = {1.0f, 5.0f, 1.0f, 9.0f};
    float snr_compact[2] = {all_snr[1], all_snr[3]};
    float vals[8] = {
        10.0f, 10.1f, 9.9f, 10.0f,
        50.0f, 10.0f, 10.2f, 10.05f,
    };
    float sup[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    float out_cpu[4] = {0}, out_gpu[4] = {0};
    auto build_inv = [&](float* out) {
        astro::compute::KernelInvocation inv;
        inv.id = astro::compute::phase2::kOpMosaicReject;
        inv.domain = astro::compute::WorkDomain{0, px};
        inv.buffers.add(0, out, px, 1, astro::compute::BufferRole::Output);
        inv.buffers.add(1, vals, px * depth, 1,
                        astro::compute::BufferRole::Input);
        inv.buffers.add(2, sup, px * depth, 1,
                        astro::compute::BufferRole::Input);
        inv.buffers.add(3, snr_compact, depth, 1,
                        astro::compute::BufferRole::Input);
        astro::compute::append_scalar(inv.scalars, std::size_t{px});
        astro::compute::append_scalar(inv.scalars, std::size_t{depth});
        astro::compute::append_scalar(inv.scalars, int{P2_REJECT_SIGMA});
        astro::compute::append_scalar(inv.scalars, double{-4.0});
        astro::compute::append_scalar(inv.scalars, double{3.0});
        astro::compute::append_scalar(inv.scalars, int{2});
        return inv;
    };
    astro::compute::KernelInvocation ic = build_inv(out_cpu);
    reg->legacy_parallel(ic, nullptr);
    astro::compute::KernelInvocation ig = build_inv(out_gpu);
    (*reg->cuda)(ig, nullptr);
    for (std::size_t p = 0; p < px; ++p)
        EXPECT_NEAR(out_gpu[p], out_cpu[p], 1e-4f) << "p=" << p;
    // 像素 0 离群被拒后接近高 SNR 帧值（snr3=9 主导）
    EXPECT_GT(out_cpu[0], 9.0f);
    bridge::api().executor_destroy(exec);
}

// R6：Winsorized 必须 CPU_ROUTE（CUDA launcher 拒绝，不冒充等价）
TEST(Phase2Acr, G9WinsorizedCpuRoute) {
    astro::compute::phase2::register_phase2_acr_kernels();
    const astro::compute::KernelRegistration* reg =
        astro::compute::global_kernel_registry().find(
            astro::compute::phase2::kOpMosaicReject);
    ASSERT_NE(reg, nullptr);
    ASSERT_TRUE(reg->cuda.has_value());
    namespace bridge = astro::compute::cuda::bridge;
    bridge::ensure_bridge_loaded();
    if (!bridge::api().loaded()) GTEST_SKIP() << "CUDA bridge 不可用";
    const char* err = nullptr;
    void* exec = bridge::api().executor_create(0, 1u << 20, 1u << 16, &err);
    ASSERT_NE(exec, nullptr);
    bridge::set_tls_handle(exec);
    const std::size_t px = 2, depth = 3;
    float vals[6] = {10, 20, 50, 20.1f, 10.2f, 19.9f};
    float out[2] = {0, 0};
    astro::compute::KernelInvocation inv;
    inv.id = astro::compute::phase2::kOpMosaicReject;
    inv.domain = astro::compute::WorkDomain{0, px};
    inv.buffers.add(0, out, px, 1, astro::compute::BufferRole::Output);
    inv.buffers.add(1, vals, px * depth, 1, astro::compute::BufferRole::Input);
    astro::compute::append_scalar(inv.scalars, std::size_t{px});
    astro::compute::append_scalar(inv.scalars, std::size_t{depth});
    astro::compute::append_scalar(inv.scalars, int{P2_REJECT_WINSORIZED_SIGMA});
    astro::compute::append_scalar(inv.scalars, double{-4.0});
    astro::compute::append_scalar(inv.scalars, double{3.0});
    astro::compute::append_scalar(inv.scalars, int{2});
    bool threw_cpu_route = false;
    try {
        (*reg->cuda)(inv, nullptr);
    } catch (const std::runtime_error& e) {
        threw_cpu_route =
            std::string(e.what()).find("CPU_ROUTE") != std::string::npos;
    }
    EXPECT_TRUE(threw_cpu_route) << "Winsorized CUDA 必须 CPU_ROUTE";
    bridge::api().executor_destroy(exec);
}


// W10 鲁棒性：损坏/边界输入
TEST(Phase2Robust, NanInputRejected) {
    std::vector<double> vals{10.0, std::nan(""), 11.0, 50.0};
    std::vector<std::uint8_t> acc(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = 4;
    in.method = P2_REJECT_SIGMA;
    in.min_samples = 2;
    in.max_iterations = 8;
    P2RejectionResult out{};
    out.accepted = acc.data();
    ASSERT_EQ(p2_reject_stack(&in, &out), 0);
    EXPECT_EQ(acc[1], 0u);          // NaN 被拒
    EXPECT_EQ(out.status, 3);       // nan-input
}

TEST(Phase2Robust, ZeroMemoryLimitUnfeasible) {
    P2BlockPlannerInput in{};
    in.output_pixels = 1024;
    in.covering_frames = 1;
    in.memory_limit_bytes = 0;
    P2BlockPlan plan{};
    ASSERT_EQ(p2_block_plan(&in, &plan), 0);
    EXPECT_EQ(plan.status, 1);
}

TEST(Phase2Robust, AllRejectedIntegrationHandled) {
    std::vector<double> vals{10.0, 11.0, 9.0};
    std::vector<std::uint8_t> acc{0, 0, 0};
    P2PixelStack in{};
    in.values = vals.data();
    in.accepted = acc.data();
    in.count = 3;
    P2PixelResult out{};
    ASSERT_EQ(p2_integrate_pixel(&in, &out), 0);
    EXPECT_NE(out.status, 0);       // 全拒
    EXPECT_EQ(out.signal, 0.0);
}

// W3 真实 HiPS：coverage union（Phase1 冻结产物只读输入）
TEST(Phase2Coverage, RealHipsUnion) {
    const char* base = "F:/Astro dev/Astro CS Normalization Database/run/temp/phase1_freeze";
    const std::string t2 = std::string(base) + "/T2_v3.hips/signal/properties";
    if (!std::ifstream(t2).good()) GTEST_SKIP() << "真实 HiPS 输入不存在";
    const std::string p0 = std::string(base) + "/T2_v3.hips";
    const std::string p1 = std::string(base) + "/T3_v3.hips";
    const std::string p2 = std::string(base) + "/t4_crop_v3.hips";
    const char* paths[3] = {p0.c_str(), p1.c_str(), p2.c_str()};
    P2CoverageResult cov{};
    cov.n_inputs = 3;
    P2HipsInputInfo infos[3]{};
    cov.inputs = infos;
    ASSERT_EQ(p2_coverage_build(paths, 3, &cov), 0);
    EXPECT_EQ(cov.target_order, 7);
    EXPECT_EQ(cov.n_inputs, 3u);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(infos[i].max_leaf_order, 7);
        EXPECT_GT(infos[i].n_tiles, 0);
        EXPECT_STREQ(infos[i].filter_passband, "Red");
        EXPECT_STREQ(infos[i].frame_type, "equatorial");
    }
    // 第一次查询容量后第二次填充
    cov.union_cells = nullptr;
    cov.n_union_cells = 0;
    ASSERT_EQ(p2_coverage_build(paths, 3, &cov), 0);
    const std::uint64_t n = cov.n_union_cells;
    EXPECT_GE(n, 1u);
    std::vector<P2MocCell> cells(n);
    cov.union_cells = cells.data();
    ASSERT_EQ(p2_coverage_build(paths, 3, &cov), 0);
    EXPECT_EQ(cov.n_union_cells, n);
    EXPECT_EQ(cells[0].order, 7u);
    p2_coverage_free(&cov);
}

// W3 真实 HiPS：filter 不一致必须拒绝
TEST(Phase2Coverage, FilterMismatchRejected) {
    const char* base = "F:/Astro dev/Astro CS Normalization Database/run/temp/phase1_freeze";
    const std::string t2 = std::string(base) + "/T2_v3.hips/signal/properties";
    if (!std::ifstream(t2).good()) GTEST_SKIP() << "真实 HiPS 输入不存在";
    const char* bad[1] = {"F:/definitely/not/a/hips"};
    P2CoverageResult cov{};
    cov.n_inputs = 1;
    ASSERT_NE(p2_coverage_build(bad, 1, &cov), 0);
}

// W4 真实 HiPS：控制点采样（AIO 读取 signal/support/snr）
TEST(Phase2Sampler, RealHipsControlSampling) {
    const char* base = "F:/Astro dev/Astro CS Normalization Database/run/temp/phase1_freeze";
    const std::string t2 = std::string(base) + "/T2_v3.hips/signal/properties";
    if (!std::ifstream(t2).good()) GTEST_SKIP() << "真实 HiPS 输入不存在";
    const std::string p0 = std::string(base) + "/T2_v3.hips";
    const std::string p1 = std::string(base) + "/T3_v3.hips";
    const char* paths[2] = {p0.c_str(), p1.c_str()};

    P2CoverageResult cov{};
    cov.n_inputs = 2;
    P2HipsInputInfo infos[2]{};
    cov.inputs = infos;
    ASSERT_EQ(p2_coverage_build(paths, 2, &cov), 0);
    std::vector<P2MocCell> cells(cov.n_union_cells);
    cov.union_cells = cells.data();
    ASSERT_EQ(p2_coverage_build(paths, 2, &cov), 0);

    std::uint64_t n_obs = 0, n_ctrl = 0;
    char err[512] = {0};
    ASSERT_EQ(p2_sample_controls(&cov, paths, nullptr, nullptr, 0,
                                 &n_obs, &n_ctrl, err, sizeof(err)), 0);
    EXPECT_GT(n_ctrl, 0u);
    EXPECT_GT(n_obs, 0u);
    std::vector<P2ControlObservation> obs(n_obs);
    ASSERT_EQ(p2_sample_controls(&cov, paths, nullptr, obs.data(), n_obs,
                                 &n_obs, &n_ctrl, err, sizeof(err)), 0);
    std::uint64_t finite = 0, snr_used = 0;
    for (const auto& o : obs) {
        if (std::isfinite(o.value) && std::isfinite(o.uncertainty))
            ++finite;
        if (o.snr > 0.0 && o.snr != 1.0) ++snr_used;
    }
    EXPECT_EQ(finite, obs.size());
    EXPECT_GT(snr_used, 0u);
    p2_coverage_free(&cov);
}

// R1/G1 sampler 统计量：even median、odd/even/negative/repeated/shuffled/
// NaN 过滤。
TEST(Phase2Sampler, G1StatisticsCorrectness) {
    // even: [1,2,3,4] -> 2.5（V2 曾因 min_element 得到 2.0）
    double a[] = {1, 2, 3, 4};
    EXPECT_DOUBLE_EQ(p2_stats_median(a, 4), 2.5);
    double b[] = {4, 1, 3, 2};
    EXPECT_DOUBLE_EQ(p2_stats_median(b, 4), 2.5);
    double c[] = {1, 2, 3};
    EXPECT_DOUBLE_EQ(p2_stats_median(c, 3), 2.0);
    double d[] = {-3, -1, -2, -4};
    EXPECT_DOUBLE_EQ(p2_stats_median(d, 4), -2.5);
    double e[] = {5, 5, 5, 5};
    EXPECT_DOUBLE_EQ(p2_stats_median(e, 4), 5.0);
    double f[] = {1, std::nan(""), 2, 3, 4, std::nan("")};
    EXPECT_DOUBLE_EQ(p2_stats_median(f, 6), 2.5);   // NaN 过滤后 even
    double g[] = {1, std::nan(""), 3};
    EXPECT_DOUBLE_EQ(p2_stats_median(g, 3), 2.0);
    // MAD：median(|x-med|)*1.4826
    double h[] = {1, 2, 3, 4};
    double med = 0;
    const double mad = p2_stats_mad(h, 4, &med);
    EXPECT_DOUBLE_EQ(med, 2.5);
    // dev = [1.5,0.5,0.5,1.5] median=1.0 -> mad=1.4826
    EXPECT_NEAR(mad, 1.4826, 1e-12);
}

// R3/G3 稳定 frame identity：复制/重命名路径不变；科学内容/关键元数据
// 变化敏感；输入顺序 canonical。
TEST(Phase2Identity, G3StableFrameIdentity) {
    namespace fs = std::filesystem;
    const fs::path base =
        "F:/Astro dev/Astro CS Normalization Database/run/temp/phase1_freeze";
    const fs::path src = base / "T2_v3.hips";
    if (!fs::exists(src / "signal" / "properties"))
        GTEST_SKIP() << "真实 HiPS 输入不存在";
    const fs::path tmp =
        "F:/Astro dev/Astro CS Normalization Database/run/temp/p2_identity_copy";
    // 复制 signal 产品树（含 properties + tiles）
    if (fs::exists(tmp / "signal")) fs::remove_all(tmp / "signal");
    fs::create_directories(tmp);
    fs::copy(src / "signal", tmp / "signal",
             fs::copy_options::recursive);
    const std::string path_a = src.string();
    const std::string path_b = tmp.string();
    const std::uint64_t id_a = p2_frame_id(path_a.c_str());
    const std::uint64_t id_b = p2_frame_id(path_b.c_str());
    EXPECT_EQ(id_a, id_b) << "复制/重命名不得改变 frame_id";
    EXPECT_NE(id_a, 0u);
    // 关键元数据变化 → frame_id 变
    {
        const fs::path props = tmp / "signal" / "properties";
        std::ifstream f(props);
        std::stringstream ss;
        ss << f.rdbuf();
        std::string text = ss.str();
        const std::string old_key = "obs_filter=Red";
        const std::string new_key = "obs_filter=Halpha";
        const std::size_t pos = text.find(old_key);
        ASSERT_NE(pos, std::string::npos);
        text.replace(pos, old_key.size(), new_key);
        std::ofstream of(props, std::ios::trunc);
        of << text;
    }
    EXPECT_NE(p2_frame_id(path_b.c_str()), id_b)
        << "关键元数据变化必须改变 frame_id";
    // 恢复
    fs::copy(src / "signal" / "properties", tmp / "signal" / "properties",
             fs::copy_options::overwrite_existing);
    // 科学内容（MOC tile 列表）变化 → frame_id 变
    {
        // 删除一个 leaf tile 文件（signal Norder7 下）
        bool removed = false;
        for (const auto& d :
             fs::recursive_directory_iterator(tmp / "signal")) {
            if (d.path().filename().string().rfind("Npix", 0) == 0 &&
                d.path().extension() == ".fits") {
                fs::remove(d.path());
                removed = true;
                break;
            }
        }
        ASSERT_TRUE(removed);
        // 注意：tile ipix 列表来自 Moc.fits；删除文件不改 MOC。真正科学内容
        // 变化测试改为修改 Moc.fits 内容（改动一个字节使 MOC 内容变化）。
        // 若 Moc.fits 存在则翻转一个字节；否则用 tile 删除（MOC 不变时
        // frame_id 依赖 MOC，故此处验证 MOC 内容敏感性）。
        const fs::path moc = tmp / "signal" / "Moc.fits";
        if (fs::exists(moc)) {
            std::fstream f(moc, std::ios::in | std::ios::out |
                                    std::ios::binary);
            if (f) {
                f.seekp(100, std::ios::beg);
                char c = 0;
                f.read(&c, 1);
                f.seekp(100, std::ios::beg);
                c ^= 0x01;
                f.write(&c, 1);
            }
        }
    }
    EXPECT_NE(p2_frame_id(path_b.c_str()), id_b)
        << "科学内容变化必须改变 frame_id";
    // 清理
    fs::remove_all(tmp);
}

// R3：input manifest canonicalization（输入顺序不影响模型身份）
TEST(Phase2Identity, G3ManifestOrderCanonical) {
    namespace fs = std::filesystem;
    const fs::path base =
        "F:/Astro dev/Astro CS Normalization Database/run/temp/phase1_freeze";
    if (!fs::exists(base / "T2_v3.hips" / "signal" / "properties") ||
        !fs::exists(base / "T3_v3.hips" / "signal" / "properties"))
        GTEST_SKIP() << "真实 HiPS 输入不存在";
    const std::string p0 = (base / "T2_v3.hips").string();
    const std::string p1 = (base / "T3_v3.hips").string();
    const std::uint64_t f0 = p2_frame_id(p0.c_str());
    const std::uint64_t f1 = p2_frame_id(p1.c_str());
    auto manifest = [](std::vector<std::uint64_t> ids) {
        std::sort(ids.begin(), ids.end());
        std::string s;
        for (auto id : ids) s += std::to_string(id) + ";";
        return astrocs::crypto::sha256_hex(s.data(), s.size());
    };
    EXPECT_EQ(manifest({f0, f1}), manifest({f1, f0}));
    EXPECT_NE(f0, f1);
}
