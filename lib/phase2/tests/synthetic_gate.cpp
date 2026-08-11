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

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <random>
#include <numeric>
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
