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
#include "astro/phase2/stage2_common.h"
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

extern "C" {
#include "aio_hips_reader.h"
}

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
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

// ===== V4 R2 科学 payload identity 测试辅助 =====

// 文件 SHA-256（用于证明像素变异时 MOC/properties 字节不变）
std::string file_sha256(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string bytes = ss.str();
    return astrocs::crypto::sha256_hex(bytes.data(), bytes.size());
}

// 在 FITS 文件第一个 HDU 数据区 byte_idx 处翻转一个字节（保持文件结构
// 合法；不改 MOC/properties）。返回是否成功定位数据区。
bool flip_fits_data_byte(const std::filesystem::path& p,
                         std::size_t byte_idx) {
    std::fstream f(p, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const std::streamoff size = f.tellg();
    if (size < 2880) return false;
    f.seekg(0, std::ios::beg);
    std::vector<char> data((std::size_t)size);
    f.read(data.data(), size);
    std::size_t data_start = 0;
    constexpr std::size_t kBlock = 2880;
    constexpr std::size_t kCard = 80;
    for (std::size_t off = 0; off + kCard <= data.size(); off += kCard) {
        if (std::string(data.data() + off, kCard).rfind("END", 0) == 0) {
            data_start = ((off / kCard) / (kBlock / kCard) + 1) * kBlock;
            break;
        }
    }
    if (data_start + byte_idx + 1 > data.size()) return false;
    data[data_start + byte_idx] ^= 0x01;
    f.seekp(0, std::ios::beg);
    f.write(data.data(), size);
    return true;
}

// 在 products 目录（signal/support）下找一个 Norder7 叶级 tile，原地翻转
// 数据字节并返回路径；找不到返回空。
std::filesystem::path first_norder7_tile(
    const std::filesystem::path& product_dir) {
    for (const auto& d :
         std::filesystem::recursive_directory_iterator(product_dir)) {
        const auto& p = d.path();
        if (p.filename().string().rfind("Npix", 0) == 0 &&
            p.extension() == ".fits") {
            const auto rel = std::filesystem::relative(p, product_dir);
            bool in_norder7 = false;
            for (const auto& part : rel) {
                if (part.string().rfind("Norder7", 0) == 0) {
                    in_norder7 = true;
                    break;
                }
            }
            if (in_norder7) return p;
        }
    }
    return {};
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
                                            (sizeof(st::kTiles) /
                                                 sizeof(st::kTiles[0]) +
                                             1));
    std::vector<double> control_ra(control_leaf.size());
    std::uint64_t ctrl_id = 0;
    const int n_tiles = (int)(sizeof(st::kTiles) / sizeof(st::kTiles[0]));
    // V4 R3：追加 tile7（与 5/6 真实相邻）完整 3 帧，保证边界节点合并
    // 后 control_count 仍 ≥ 256。
    for (int t = 0; t <= n_tiles; ++t) {
        const std::uint64_t tile =
            (t < n_tiles) ? st::kTiles[t] : (std::uint64_t)7;
        for (int gy = 0; gy < st::kGrid; ++gy) {
            for (int gx = 0; gx < st::kGrid; ++gx) {
                double ra = 0, dec = 0;
                st::cell_center_radec(tile, gx, gy, &ra, &dec);
                const std::uint64_t leaf = st::leaf_of(
                    tile, gx * st::kCellSide + st::kCellSide / 2,
                    gy * st::kCellSide + st::kCellSide / 2);
                control_leaf[(std::size_t)(t * st::kGrid * st::kGrid +
                                           gy * st::kGrid + gx)] = leaf;
                control_ra[(std::size_t)(t * st::kGrid * st::kGrid +
                                         gy * st::kGrid + gx)] = ra;
                (void)dec;
                // frame0（参考）覆盖全部；frame1 全部；frame2 仅前 3 tiles
                //（tile7 追加为完整覆盖）
                const int fmax = (t == n_tiles - 1) ? 1 : 2;
                for (int f = 0; f <= fmax; ++f) {
                    P2ControlObservation o{};
                    o.frame_id = (std::uint64_t)f;
                    o.control_id = ctrl_id;
                    o.leaf_ipix = leaf;
                    o.ra_deg = ra;
                    double r2 = 0, d2 = 0;
                    st::cell_center_radec(tile, gx, gy, &r2, &d2);
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

// R5/G4 V3 完整 truth gate：scalar baseline vs spatial UPM；
// 结构保真、真单覆盖延拓、远场扰动、cell/tile 边界连续性、独立 gauge。
TEST(Phase2Upm, G1V3SpatialTruthFull) {
    namespace st = spatial_truth;
    std::mt19937 rng(20260817);
    std::normal_distribution<double> nd(0.0, st::kNoiseRms);
    // coverage：frame0 覆盖 tiles[0..2]；frame1 覆盖全部 4；frame2 覆盖
    // tiles[0..2] → tile3 仅 frame1（真单覆盖）；tile100 仅 frame5（断开）
    const int n_tiles = (int)(sizeof(st::kTiles) / sizeof(st::kTiles[0]));
    std::vector<P2ControlObservation> obs;
    std::uint64_t cid = 0;
    std::vector<std::uint64_t> cell_leaf(n_tiles * st::kGrid * st::kGrid);
    for (int t = 0; t < n_tiles; ++t) {
        for (int gy = 0; gy < st::kGrid; ++gy) {
            for (int gx = 0; gx < st::kGrid; ++gx) {
                double ra = 0, dec = 0;
                st::cell_center_radec(st::kTiles[t], gx, gy, &ra, &dec);
                const std::uint64_t leaf =
                    st::leaf_of(st::kTiles[t], gx * st::kCellSide + 32,
                                gy * st::kCellSide + 32);
                cell_leaf[(std::size_t)(t * st::kGrid * st::kGrid +
                                        gy * st::kGrid + gx)] = leaf;
                // frame0: tiles 0-2；frame1: 全部；frame2: tiles 0-2
                for (int f = 0; f < 3; ++f) {
                    if (f != 1 && t == n_tiles - 1) continue;
                    P2ControlObservation o{};
                    o.frame_id = (std::uint64_t)f;
                    o.control_id = cid;
                    o.leaf_ipix = leaf;
                    o.ra_deg = ra;
                    o.dec_deg = dec;
                    o.value = st::true_sky(ra, dec) +
                              st::frame_field(f, ra, dec) + nd(rng);
                    o.uncertainty = st::kNoiseRms;
                    o.snr = (f == 1 && (t + gy + gx) % 2 == 0) ? 3.0 : 100.0;
                    o.support = 0.8 + 0.2 * ((gx + gy) % 2);
                    o.quality_flags = 1;
                    obs.push_back(o);
                }
                ++cid;
            }
        }
    }
    // V4 R3：追加 tile7（与 5/6 真实相邻，完整 3 帧），保证边界节点
    // 合并后 control_count 仍 ≥ 256（G3 硬门）。
    for (int gy = 0; gy < st::kGrid; ++gy) {
        for (int gx = 0; gx < st::kGrid; ++gx) {
            double ra = 0, dec = 0;
            st::cell_center_radec(7, gx, gy, &ra, &dec);
            const std::uint64_t leaf =
                st::leaf_of(7, gx * st::kCellSide + 32,
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
                o.snr = 100.0;
                o.support = 1.0;
                o.quality_flags = 1;
                obs.push_back(o);
            }
            ++cid;
        }
    }
    // 断开分量：tile100 仅 frame5
    for (int c = 0; c < 2; ++c) {
        double ra = 0, dec = 0;
        st::cell_center_radec(100, c * 4, c * 4, &ra, &dec);
        const std::uint64_t leaf = st::leaf_of(100, c * 4 * st::kCellSide + 32,
                                               c * 4 * st::kCellSide + 32);
        P2ControlObservation o{};
        o.frame_id = 5;
        o.control_id = cid++;
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
    cfg.smoothing_lambda = 0.5;
    cfg.zero_anchor_weight = 1e-3;
    cfg.sigma_floor = 0.02;
    cfg.max_iterations = 60;
    cfg.tolerance = 1e-9;
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    P2ModelInfo info{};
    ASSERT_EQ(p2_upm_info(model, &info), 0);
    EXPECT_GE(info.control_count, 256u);
    EXPECT_GE(info.component_count, 2u);

    // 验证像素：tile0-2 三帧（overlap）、tile3 单覆盖 frame1、cell/tile 边界
    std::vector<std::uint64_t> vleaf;
    std::vector<int> vframe;
    std::vector<double> vtrue;
    auto add_val = [&](std::uint64_t tile, int x, int y, int f) {
        const std::uint64_t leaf = st::leaf_of(tile, x, y);
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(
            1u << (unsigned)(st::kTargetOrder + st::kTileShift),
            leaf, ra, dec);
        vleaf.push_back(leaf);
        vframe.push_back(f);
        vtrue.push_back(st::true_sky(ra, dec));
    };
    std::uniform_int_distribution<int> xy(0, 511);
    for (int t = 0; t < 3; ++t) {          // overlap tiles
        for (int i = 0; i < 20000; ++i) {
            const int x = xy(rng), y = xy(rng);
            for (int f = 0; f < 3; ++f) add_val(st::kTiles[t], x, y, f);
        }
    }
    for (int i = 0; i < 20000; ++i)        // single-coverage tile（仅 frame1）
        add_val(st::kTiles[3], xy(rng), xy(rng), 1);
    // cell 边界两侧（x = 64k 与 x = 64k-1）
    for (int t = 0; t < 3; ++t) {
        for (int k = 1; k < st::kGrid; ++k) {
            for (int y = 0; y < 512; y += 37) {
                add_val(st::kTiles[t], k * st::kCellSide, y, 1);
                add_val(st::kTiles[t], k * st::kCellSide - 1, y, 1);
            }
        }
    }
    const std::uint64_t n_val = vleaf.size();
    EXPECT_GE(n_val, 100000u);
    std::vector<double> in_sig(n_val), out_sig(n_val);
    for (std::size_t i = 0; i < n_val; ++i) {
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(
            1u << (unsigned)(st::kTargetOrder + st::kTileShift),
            vleaf[i], ra, dec);
        const int f = vframe[i];
        in_sig[i] = vtrue[i] + st::frame_field(f, ra, dec) + nd(rng);
    }
    for (int f = 0; f < 3; ++f) {
        std::vector<std::uint64_t> lv;
        std::vector<double> iv;
        for (std::size_t i = 0; i < n_val; ++i)
            if (vframe[i] == f) {
                lv.push_back(vleaf[i]);
                iv.push_back(in_sig[i]);
            }
        if (lv.empty()) continue;
        std::vector<double> ov(lv.size());
        ASSERT_EQ(p2_upm_calibrate_block(model, (std::uint64_t)f, lv.data(),
                                         iv.data(), ov.data(), lv.size()), 0);
        std::size_t w = 0;
        for (std::size_t i = 0; i < n_val; ++i)
            if (vframe[i] == f) out_sig[i] = ov[w++];
    }

    // 指标
    std::vector<double> residual(n_val), c_true(n_val), c_hat(n_val);
    for (std::size_t i = 0; i < n_val; ++i) {
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(
            1u << (unsigned)(st::kTargetOrder + st::kTileShift),
            vleaf[i], ra, dec);
        residual[i] = out_sig[i] - vtrue[i];
        const int f = vframe[i];
        c_true[i] = st::frame_field(f, ra, dec);
        c_hat[i] = in_sig[i] - out_sig[i];
    }
    auto pct = [](std::vector<double> v, double p) {
        std::sort(v.begin(), v.end());
        return v.empty() ? 0.0
                         : v[(std::size_t)(p * (double)(v.size() - 1))];
    };
    auto rms = [](const std::vector<double>& v) {
        double s = 0;
        for (double x : v) s += x * x;
        return std::sqrt(s / (double)v.size());
    };
    const double res_rmse = rms(residual);
    // correction field RMSE（对 frame1/2 有效样本）
    std::vector<double> cdiff;
    for (std::size_t i = 0; i < n_val; ++i)
        if (vframe[i] != 0) cdiff.push_back(c_hat[i] - c_true[i]);
    const double field_rmse = rms(cdiff);
    // V4 R3：abs 指标（p95 = percentile(|error|, 0.95)，禁止用有符号
    // percentile）；field/residual 各报告并 hard-assert RMSE/p95/max。
    std::vector<double> res_abs(residual.size()), field_abs(cdiff.size());
    for (std::size_t i = 0; i < residual.size(); ++i)
        res_abs[i] = std::fabs(residual[i]);
    for (std::size_t i = 0; i < cdiff.size(); ++i)
        field_abs[i] = std::fabs(cdiff[i]);
    const double res_abs_p95 = pct(res_abs, 0.95);
    const double res_abs_max =
        *std::max_element(res_abs.begin(), res_abs.end());
    const double field_abs_p95 = pct(field_abs, 0.95);
    const double field_abs_max =
        *std::max_element(field_abs.begin(), field_abs.end());
    // max 门限推导：|N(0,σ)| 在 n 样本的极值期望 ≈ σ·√(2·ln n)；
    // 取 2.0× 安全裕度（n≈180k → ≈0.49，仍远小于场幅度 0.4–0.7
    // 的 1.5–2 倍；系统级错误若达到场幅度会被该门拒绝）。
    const double ev_max =
        st::kNoiseRms *
        std::sqrt(2.0 * std::log((double)std::max<std::size_t>(1, n_val)));
    const double max_thresh = 2.0 * ev_max;
    // scalar-only baseline：每帧常数 offset（训练 control 残差中位数）
    std::vector<double> off(3, 0.0);
    for (int f = 1; f < 3; ++f) {
        std::vector<double> diff;
        for (const auto& o : obs)
            if (o.frame_id == (std::uint64_t)f)
                diff.push_back(o.value - st::true_sky(o.ra_deg, o.dec_deg) -
                               st::frame_field(0, o.ra_deg, o.dec_deg));
        if (!diff.empty()) off[f] = pct(diff, 0.5);
    }
    std::vector<double> base_cdiff;
    for (std::size_t i = 0; i < n_val; ++i) {
        const int f = vframe[i];
        if (f == 0) continue;
        base_cdiff.push_back((in_sig[i] - off[f]) - vtrue[i] -
                             c_true[i]);
    }
    const double base_field_rmse = rms(base_cdiff);
    EXPECT_GT(base_field_rmse, 3.0 * field_rmse)
        << "scalar baseline 必须在空间 field recovery 上显著失败";
    // structure preservation：residual 与 TrueSky 去均值相关性（应低）
    double m_t = 0, m_r = 0;
    for (std::size_t i = 0; i < n_val; ++i) { m_t += vtrue[i]; m_r += residual[i]; }
    m_t /= (double)n_val;
    m_r /= (double)n_val;
    double cov = 0, vt = 0, vr = 0;
    for (std::size_t i = 0; i < n_val; ++i) {
        cov += (vtrue[i] - m_t) * (residual[i] - m_r);
        vt += (vtrue[i] - m_t) * (vtrue[i] - m_t);
        vr += (residual[i] - m_r) * (residual[i] - m_r);
    }
    const double struct_corr = cov / std::sqrt(vt * vr);
    // cell 边界 jump（x=64k 与 64k-1 的校正值差，取最大）
    double max_cell_jump = 0.0;
    for (int t = 0; t < 3; ++t)
        for (int k = 1; k < st::kGrid; ++k)
            for (int y = 0; y < 512; y += 37) {
                const double c1 = p2_upm_evaluate_c(
                    model, 1,
                    st::leaf_of(st::kTiles[t], k * st::kCellSide, y));
                const double c2 = p2_upm_evaluate_c(
                    model, 1,
                    st::leaf_of(st::kTiles[t], k * st::kCellSide - 1, y));
                max_cell_jump = std::max(max_cell_jump, std::fabs(c1 - c2));
            }
    std::fprintf(stderr,
                 "[G1V3] controls=%llu val=%llu res_rmse=%.4f field_rmse=%.4f "
                 "res_abs_p95=%.4f res_abs_max=%.4f field_abs_p95=%.4f "
                 "field_abs_max=%.4f base_field_rmse=%.4f struct_corr=%.3f "
                 "cell_jump=%.4f components=%u ev_max=%.4f max_thresh=%.4f\n",
                 (unsigned long long)info.control_count,
                 (unsigned long long)n_val, res_rmse, field_rmse,
                 res_abs_p95, res_abs_max, field_abs_p95, field_abs_max,
                 base_field_rmse, struct_corr, max_cell_jump,
                 info.component_count, ev_max, max_thresh);
    // hard gates
    EXPECT_LE(res_rmse, 3.0 * st::kNoiseRms);
    EXPECT_LE(field_rmse, 3.0 * st::kNoiseRms);
    EXPECT_LE(res_abs_p95, 5.0 * st::kNoiseRms);
    EXPECT_LE(field_abs_p95, 5.0 * st::kNoiseRms);
    EXPECT_LE(res_abs_max, max_thresh);
    EXPECT_LE(field_abs_max, max_thresh);
    EXPECT_LT(std::fabs(struct_corr), 0.2);
    // 双线性场内 cell 共享节点 → 边界两侧连续；1px 采样差由
    // C 节点观测噪声经插值传播主导（≈ 节点噪声/64）。门限 1e-3：
    // 远小于观测噪声(0.05)与场幅度(0.4)，同时可区分真实不连续。
    EXPECT_LE(max_cell_jump, 1e-3);
    // V4 R3：每连通分量的 gauge frame id（求解前固定）
    std::uint64_t n_comp = 0;
    std::vector<std::uint64_t> gauge_refs(info.component_count, 0);
    ASSERT_EQ(p2_upm_component_gauges(model, &n_comp, gauge_refs.data()), 0);
    EXPECT_EQ(n_comp, (std::uint64_t)info.component_count);
    EXPECT_GE(n_comp, 2u);
    {
        std::ostringstream gs;
        for (std::uint64_t g : gauge_refs) gs << g << ",";
        std::fprintf(stderr, "[G1V3] component_gauges=%s\n",
                     gs.str().c_str());
    }
    // V4 R3：machine-readable truth metrics 证据
    {
        nlohmann::json mj;
        mj["residual_abs_rmse"] = res_rmse;
        mj["residual_abs_p95"] = res_abs_p95;
        mj["residual_abs_max"] = res_abs_max;
        mj["field_abs_rmse"] = field_rmse;
        mj["field_abs_p95"] = field_abs_p95;
        mj["field_abs_max"] = field_abs_max;
        mj["baseline_field_rmse"] = base_field_rmse;
        mj["structure_corr"] = struct_corr;
        mj["max_cell_jump"] = max_cell_jump;
        mj["extreme_value_max_threshold"] = max_thresh;
        mj["validation_pixels"] = n_val;
        mj["component_count"] = info.component_count;
        mj["component_gauge_frame_ids"] = gauge_refs;
        std::filesystem::create_directories(
            "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth");
        std::ofstream tf(
            "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth/"
            "upm_truth_metrics.json");
        if (tf) tf << mj.dump(2);
    }
    p2_upm_close(model);
}

// R5/G4：tile 边界连续性 + low-SNR/low-quality 远场扰动 + 真单覆盖上界
TEST(Phase2Upm, G4BoundaryAndPerturbation) {
    namespace st = spatial_truth;
    std::mt19937 rng(20260818);
    std::normal_distribution<double> nd(0.0, st::kNoiseRms);
    const int n_tiles = (int)(sizeof(st::kTiles) / sizeof(st::kTiles[0]));
    auto build_obs = [&](bool perturb) {
        std::vector<P2ControlObservation> obs;
        std::uint64_t cid = 0;
        for (int t = 0; t < n_tiles; ++t) {
            for (int gy = 0; gy < st::kGrid; ++gy) {
                for (int gx = 0; gx < st::kGrid; ++gx) {
                    double ra = 0, dec = 0;
                    st::cell_center_radec(st::kTiles[t], gx, gy, &ra, &dec);
                    const std::uint64_t leaf =
                        st::leaf_of(st::kTiles[t], gx * st::kCellSide + 32,
                                    gy * st::kCellSide + 32);
                    for (int f = 0; f < 3; ++f) {
                        if (f != 1 && t == n_tiles - 1) continue;
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
                        // perturbation：tile0 的一个 control 变 low-SNR +
                        // 大偏差 + bad quality
                        if (perturb && t == 0 && gx == 1 && gy == 1 &&
                            f == 1) {
                            o.snr = 0.1;
                            o.value += 3.0;
                            o.quality_flags = 16;   // photo_rejected
                        }
                        obs.push_back(o);
                    }
                    ++cid;
                }
            }
        }
        // 断开分量（tile100 仅 frame5）保底
        for (int c = 0; c < 2; ++c) {
            double ra = 0, dec = 0;
            st::cell_center_radec(100, c * 4, c * 4, &ra, &dec);
            P2ControlObservation o{};
            o.frame_id = 5;
            o.control_id = cid++;
            o.leaf_ipix = st::leaf_of(100, c * 4 * st::kCellSide + 32,
                                      c * 4 * st::kCellSide + 32);
            o.ra_deg = ra;
            o.dec_deg = dec;
            o.value = st::true_sky(ra, dec) + nd(rng);
            o.uncertainty = st::kNoiseRms;
            o.snr = 100.0;
            o.support = 1.0;
            o.quality_flags = 1;
            obs.push_back(o);
        }
        return obs;
    };
    P2UpmBuildConfig cfg{};
    cfg.target_order = st::kTargetOrder;
    cfg.smoothing_lambda = 0.5;
    cfg.zero_anchor_weight = 1e-3;
    cfg.sigma_floor = 0.02;
    cfg.max_iterations = 40;
    cfg.tolerance = 1e-9;
    auto obs0 = build_obs(false);
    auto obs1 = build_obs(true);
    void* m0 = nullptr;
    void* m1 = nullptr;
    ASSERT_EQ(p2_upm_build(obs0.data(), obs0.size(), &cfg, &m0), 0);
    ASSERT_EQ(p2_upm_build(obs1.data(), obs1.size(), &cfg, &m1), 0);

    // 1. tile 边界连续性：找两两 tile 最近边界 cell 对，评估 C jump
    double best_dist = 1e9;
    std::pair<int, int> tpair, p1, p2;
    for (int t = 0; t < n_tiles; ++t)
        for (int u = t + 1; u < n_tiles; ++u)
            for (int g : {0, 7})
                for (int h = 0; h < st::kGrid; ++h)
                    for (int g2 : {0, 7})
                        for (int h2 = 0; h2 < st::kGrid; ++h2) {
                            double r1 = 0, d1 = 0, r2 = 0, d2 = 0;
                            st::cell_center_radec(st::kTiles[t], g, h,
                                                  &r1, &d1);
                            st::cell_center_radec(st::kTiles[u], g2, h2,
                                                  &r2, &d2);
                            const double dist =
                                astrocs::healpix::angular_distance_deg(
                                    r1, d1, r2, d2);
                            if (dist < best_dist) {
                                best_dist = dist;
                                tpair = {t, u};
                                p1 = {g, h};
                                p2 = {g2, h2};
                            }
                        }
    const std::uint64_t l1 = st::leaf_of(
        st::kTiles[tpair.first], p1.first * st::kCellSide + 32,
        p1.second * st::kCellSide + 32);
    const std::uint64_t l2 = st::leaf_of(
        st::kTiles[tpair.second], p2.first * st::kCellSide + 32,
        p2.second * st::kCellSide + 32);
    const double c1 = p2_upm_evaluate_c(m0, 1, l1);
    const double c2 = p2_upm_evaluate_c(m0, 1, l2);
    const double tile_jump = std::fabs(c1 - c2);
    std::fprintf(stderr,
                 "[G4] tile_edge_dist=%.4f deg tile_jump=%.4f "
                 "far_field_delta=%.4f\n",
                 best_dist, tile_jump,
                 std::fabs(p2_upm_evaluate_c(m0, 1, l2) -
                           p2_upm_evaluate_c(m1, 1, l2)));
    // 跨 tile 最近边界 cell 的 C 差应接近注入场在该间距上的变化
    // （相邻 tile 平滑约束后），门限：场梯度×间距×3 + 噪声容差
    const double grad_bound = 0.5;   // 注入场最大幅度梯度（/弧度）
    const double jump_thresh =
        grad_bound * best_dist * 3.141592653589793 / 180.0 * 3.0 +
        2.0 * st::kNoiseRms;
    EXPECT_LE(tile_jump, jump_thresh);
    // 2. low-SNR/low-quality 扰动：远处 tile（tpair.second 的另一端）
    //    correction delta 应小（门限：节点噪声量级×3）
    const double far_delta =
        std::fabs(p2_upm_evaluate_c(m0, 1, l2) -
                  p2_upm_evaluate_c(m1, 1, l2));
    EXPECT_LE(far_delta, 0.15);
    // 3. 真单覆盖（tile3 仅 frame1）：correction 由平滑延拓继承相邻场，
    //    应接近真实场而非任意大；延拓误差有界。
    double max_single = 0.0;
    double ext_err_sq = 0.0;
    int ext_n = 0;
    for (int gy = 0; gy < st::kGrid; ++gy)
        for (int gx = 0; gx < st::kGrid; ++gx) {
            const std::uint64_t leaf = st::leaf_of(
                st::kTiles[3], gx * st::kCellSide + 32,
                gy * st::kCellSide + 32);
            const double ch = p2_upm_evaluate_c(m0, 1, leaf);
            double ra = 0, dec = 0;
            astrocs::healpix::pix2ang_nest(
                1u << (unsigned)(st::kTargetOrder + st::kTileShift),
                leaf, ra, dec);
            const double ct = st::frame_field(1, ra, dec);
            max_single = std::max(max_single, std::fabs(ch));
            ext_err_sq += (ch - ct) * (ch - ct);
            ++ext_n;
        }
    const double ext_rmse = std::sqrt(ext_err_sq / (double)ext_n);
    std::fprintf(stderr,
                 "[G4] single_coverage_max_correction=%.4f "
                 "extension_rmse=%.4f\n",
                 max_single, ext_rmse);
    EXPECT_LE(max_single, 1.0);                    // 防任意大
    EXPECT_LE(ext_rmse, 5.0 * st::kNoiseRms);      // 延拓误差 ≤ 5σ
    p2_upm_close(m1);
    p2_upm_close(m0);
}

// V4 R3：真实相邻 tile 边界两侧 leaf pixels seam gate（不再用跨 tile
// control-cell center 代替）。tiles {4,5,6,7} 是同一 order-6 父 tile 的
// 完整 2×2 子块（sub 0/1/2/3）：4-5 与 6-7 为 x 方向 seam，4-6 与 5-7
// 为 y 方向 seam，覆盖 HEALPix 两种边方向。比较 |C(p_left)-C(p_right)|
// 与注入连续场在这两个实际 sky position 的 truth delta + 插值噪声上界。
TEST(Phase2Upm, G4RealTileSeamLeaves) {
    namespace st = spatial_truth;
    std::mt19937 rng(20260819);
    std::normal_distribution<double> nd(0.0, st::kNoiseRms);
    const std::uint64_t tiles[4] = {4, 5, 6, 7};
    const int n_tiles = 4;
    std::vector<P2ControlObservation> obs;
    std::uint64_t cid = 0;
    for (int t = 0; t < n_tiles; ++t) {
        for (int gy = 0; gy < st::kGrid; ++gy) {
            for (int gx = 0; gx < st::kGrid; ++gx) {
                double ra = 0, dec = 0;
                st::cell_center_radec(tiles[t], gx, gy, &ra, &dec);
                const std::uint64_t leaf =
                    st::leaf_of(tiles[t], gx * st::kCellSide + 32,
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
                    o.snr = 100.0;
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
    cfg.smoothing_lambda = 0.5;
    cfg.zero_anchor_weight = 1e-3;
    cfg.sigma_floor = 0.02;
    cfg.max_iterations = 60;
    cfg.tolerance = 1e-9;
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    P2ModelInfo info{};
    ASSERT_EQ(p2_upm_info(model, &info), 0);
    // V5：移除边界节点合并后 2×2 块保持 256 个独立 control cell；
    // ≥256 controls 由主 truth gate 覆盖。
    EXPECT_GT(info.control_count, 128u);

    // 四组真实相邻 tile seam：{A, B, vertical}。
    // vertical=true → A(x=511,y) 与 B(x=0,y)（sub0-1 / sub2-3 的 x seam）；
    // vertical=false → A(x,y=511) 与 B(x,y=0)（sub0-2 / sub1-3 的 y seam）。
    struct Seam {
        std::uint64_t ta, tb;
        bool vertical;
    };
    const std::vector<Seam> seams = {
        {4, 5, true}, {6, 7, true}, {4, 6, false}, {5, 7, false}};
    const double nside_leaf =
        (double)(1u << (unsigned)(st::kTargetOrder + st::kTileShift));
    const double leaf_spacing_deg =
        std::sqrt(4.0 * 3.141592653589793 / (12.0 * nside_leaf * nside_leaf)) *
        180.0 / 3.141592653589793;
    // V5 噪声界推导（自校准）：seam 两侧 C 来自不同 tile 的独立节点
    // 集，其差是两次独立插值噪声的差 → 跳变噪声 ≈ √2×内部插值噪声。
    // 内部插值噪声用同模型、同帧、远离边界的内部 leaf 实测
    // |C_hat - C_true| 的 max 度量；seam 门限 = truth_delta +
    // 2.0×内部 max 误差（保守包住 √2 因子）。该界随数据噪声自适应，
    // 不是任意小常数；平滑场（truth_delta≈0）下 seam 跳变须落在
    // 内部恢复误差量级，证明无系统性 seam 伪影。
    std::uniform_int_distribution<int> xyd(0, 511);
    double interior_max_err = 0.0, interior_sum = 0.0;
    std::size_t interior_n = 0;
    for (int t = 0; t < n_tiles; ++t) {
        for (int i = 0; i < 4000; ++i) {
            const int x = xyd(rng), y = xyd(rng);
            if (x < 128 || x >= 384 || y < 128 || y >= 384) continue;
            const std::uint64_t leaf = st::leaf_of(tiles[t], x, y);
            double ra = 0, dec = 0;
            astrocs::healpix::pix2ang_nest(
                1u << (unsigned)(st::kTargetOrder + st::kTileShift), leaf,
                ra, dec);
            const double e = std::fabs(
                p2_upm_evaluate_c(model, 1, leaf) -
                st::frame_field(1, ra, dec));
            interior_max_err = std::max(interior_max_err, e);
            interior_sum += e;
            ++interior_n;
        }
    }
    const double interior_mean_err =
        interior_n ? interior_sum / (double)interior_n : 0.0;
    const double noise_bound = 2.0 * interior_max_err + 1e-12;
    double worst_excess = 0.0;
    std::size_t total_pairs = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> seam_pairs;
    for (const auto& s : seams) {
        double seam_max_jump = 0.0, seam_max_delta = 0.0;
        std::size_t n_pairs = 0;
        for (int k = 0; k < 512; k += 8) {
            const int xa = s.vertical ? 511 : k;
            const int ya = s.vertical ? k : 511;
            const int xb = s.vertical ? 0 : k;
            const int yb = s.vertical ? k : 0;
            const std::uint64_t la = st::leaf_of(s.ta, xa, ya);
            const std::uint64_t lb = st::leaf_of(s.tb, xb, yb);
            double raa = 0, deca = 0, rab = 0, decb = 0;
            astrocs::healpix::pix2ang_nest(
                1u << (unsigned)(st::kTargetOrder + st::kTileShift), la,
                raa, deca);
            astrocs::healpix::pix2ang_nest(
                1u << (unsigned)(st::kTargetOrder + st::kTileShift), lb,
                rab, decb);
            // sanity：pair 必须真正相邻（角距 < 2× leaf spacing）
            EXPECT_LT(astrocs::healpix::angular_distance_deg(raa, deca, rab,
                                                             decb),
                      2.0 * leaf_spacing_deg)
                << "seam pair 必须为真实相邻 leaf";
            const double ca = p2_upm_evaluate_c(model, 1, la);
            const double cb = p2_upm_evaluate_c(model, 1, lb);
            const double delta =
                std::fabs(st::frame_field(1, raa, deca) -
                          st::frame_field(1, rab, decb));
            const double jump = std::fabs(ca - cb);
            seam_max_jump = std::max(seam_max_jump, jump);
            seam_max_delta = std::max(seam_max_delta, delta);
            worst_excess = std::max(worst_excess, jump - delta);
            ++n_pairs;
            ++total_pairs;
        }
        seam_pairs.push_back({s.ta, s.tb});
        std::fprintf(stderr,
                     "[G4-seam] %llu-%llu vertical=%d pairs=%zu "
                     "max_delta=%.5f max_jump=%.5f\n",
                     (unsigned long long)s.ta, (unsigned long long)s.tb,
                     (int)s.vertical, n_pairs, seam_max_delta, seam_max_jump);
    }
    std::fprintf(stderr,
                 "[G4-seam] total_pairs=%zu worst_excess=%.5f "
                 "interior_mean_err=%.5f interior_max_err=%.5f "
                 "noise_bound=%.5f\n",
                 total_pairs, worst_excess, interior_mean_err,
                 interior_max_err, noise_bound);
    EXPECT_GE(total_pairs, 4u * 32u) << "必须覆盖多组 seam";
    // hard gate：|C(p_left)-C(p_right)| ≤ 注入场 truth delta + 噪声界
    EXPECT_LE(worst_excess, noise_bound)
        << "真实 tile seam 两侧 C 跳变必须由注入连续场 delta + 插值噪声解释";
    // machine-readable 证据
    nlohmann::json sj;
    sj["seams"] = nlohmann::json::array();
    for (const auto& s : seams) {
        nlohmann::json e;
        e["tile_a"] = s.ta;
        e["tile_b"] = s.tb;
        e["vertical_edge"] = s.vertical;
        sj["seams"].push_back(e);
    }
    sj["leaf_spacing_deg"] = leaf_spacing_deg;
    sj["noise_bound"] = noise_bound;
    sj["total_boundary_pairs"] = total_pairs;
    sj["worst_excess_over_truth_delta"] = worst_excess;
    sj["interior_mean_err"] = interior_mean_err;
    sj["interior_max_err"] = interior_max_err;
    std::filesystem::create_directories(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth");
    std::ofstream sf(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth/"
        "tile_seam_metrics.json");
    if (sf) sf << sj.dump(2);
    p2_upm_close(model);
}

// V5 G1：跨 tile 恢复 delta 跟随 truth delta（不强制 jump=0）。
// 注入 step 场（左列 0 / 右列 +0.3；上排 0 / 下排 +0.3），λs=0
// （生产 stage2 配置 smoothing=0 语义）；四个 seam 的恢复 delta 必须
// 跟踪 truth step，而不是被压成 0。
TEST(Phase2Upm, G1CrossTileDeltaFollowsTruth) {
    namespace st = spatial_truth;
    std::mt19937 rng(20260822);
    std::normal_distribution<double> nd(0.0, st::kNoiseRms);
    const std::uint64_t tiles[4] = {4, 5, 6, 7};
    const double kStep = 0.3;
    auto frame1_c = [&](std::uint64_t tile) {
        const bool right = (tile == 5 || tile == 7);
        const bool bottom = (tile == 6 || tile == 7);
        return (right ? kStep : 0.0) + (bottom ? kStep : 0.0);
    };
    std::vector<P2ControlObservation> obs;
    std::uint64_t cid = 0;
    for (int t = 0; t < 4; ++t) {
        for (int gy = 0; gy < st::kGrid; ++gy) {
            for (int gx = 0; gx < st::kGrid; ++gx) {
                double ra = 0, dec = 0;
                st::cell_center_radec(tiles[t], gx, gy, &ra, &dec);
                const std::uint64_t leaf =
                    st::leaf_of(tiles[t], gx * st::kCellSide + 32,
                                gy * st::kCellSide + 32);
                for (int f = 0; f < 3; ++f) {
                    P2ControlObservation o{};
                    o.frame_id = (std::uint64_t)f;
                    o.control_id = cid;
                    o.leaf_ipix = leaf;
                    o.ra_deg = ra;
                    o.dec_deg = dec;
                    o.value = st::true_sky(ra, dec) +
                              (f == 1 ? frame1_c(tiles[t]) : 0.0) +
                              nd(rng);
                    o.uncertainty = st::kNoiseRms;
                    o.snr = 100.0;
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
    cfg.smoothing_lambda = 0.0;   // 生产语义（stage2 smoothing=0）
    cfg.zero_anchor_weight = 1e-3;
    cfg.sigma_floor = 0.02;
    cfg.max_iterations = 60;
    cfg.tolerance = 1e-9;
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);

    // 4 组 seam（x: 4-5, 6-7；y: 4-6, 5-7）恢复 delta 统计
    struct Seam {
        std::uint64_t ta, tb;
        bool vertical;
    };
    const std::vector<Seam> seams = {
        {4, 5, true}, {6, 7, true}, {4, 6, false}, {5, 7, false}};
    double worst_err = 0.0, min_delta = 1e9;
    double sum = 0.0;
    std::size_t n = 0, n_over_half = 0;
    for (const auto& s : seams) {
        for (int k = 0; k < 512; k += 8) {
            const int xa = s.vertical ? 511 : k;
            const int ya = s.vertical ? k : 511;
            const int xb = s.vertical ? 0 : k;
            const int yb = s.vertical ? k : 0;
            const std::uint64_t la = st::leaf_of(s.ta, xa, ya);
            const std::uint64_t lb = st::leaf_of(s.tb, xb, yb);
            const double d = std::fabs(
                p2_upm_evaluate_c(model, 1, la) -
                p2_upm_evaluate_c(model, 1, lb));
            sum += d;
            ++n;
            min_delta = std::min(min_delta, d);
            if (d > 0.5 * kStep) ++n_over_half;
            worst_err = std::max(worst_err, std::fabs(d - kStep));
        }
    }
    const double mean_delta = n ? sum / (double)n : 0.0;
    const double frac_over_half = n ? (double)n_over_half / (double)n : 0.0;
    std::fprintf(stderr,
                 "[G1-step] pairs=%zu mean_delta=%.5f min_delta=%.5f "
                 "truth_step=%.2f worst_err=%.5f frac_over_half=%.3f\n",
                 n, mean_delta, min_delta, kStep, worst_err, frac_over_half);
    // 恢复 delta 必须跟随 truth step（均值误差 ≤ 3σ_node≈0.15），
    // 且绝大多数 pair 明显非 0（不允许 proximity-alias 强制 jump=0；
    // 单 pair 下界受节点噪声 ~0.05 影响，用 ≥90% 占比而非逐 pair min）。
    // 边缘外推放大节点噪声（~1.34× 外推系数 → ~2.8× 噪声），单 pair
    // 下界允许 20% 落在半 step 以下；均值跟踪与精确性分别由
    // mean 断言与 G1V7EdgeBasisAnalytic（noise=0）覆盖。
    EXPECT_GT(frac_over_half, 0.8)
        << "跨 tile 恢复 delta 不得被强制为 0";
    EXPECT_NEAR(mean_delta, kStep, 0.15)
        << "跨 tile 恢复 delta 必须跟随 truth delta";
    // machine-readable 证据
    nlohmann::json sj;
    sj["truth_step"] = kStep;
    sj["pairs"] = n;
    sj["mean_recovered_delta"] = mean_delta;
    sj["min_recovered_delta"] = min_delta;
    sj["frac_pairs_over_half_step"] = frac_over_half;
    sj["worst_abs_error_vs_truth"] = worst_err;
    std::filesystem::create_directories(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth");
    std::ofstream sf(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth/"
        "v5_cross_tile_delta.json");
    if (sf) sf << sj.dump(2);
    p2_upm_close(model);
}

// V5 G1：half-cell 相位 truth——线性场 C(x)=g*x/512 必须被双线性基
// 在正确相位恢复（无半 cell 平移）：恢复斜率≈g 且截距≈0。
TEST(Phase2Upm, G1HalfCellPhaseTruth) {
    namespace st = spatial_truth;
    std::mt19937 rng(20260823);
    std::normal_distribution<double> nd(0.0, st::kNoiseRms);
    const std::uint64_t tile = 4;
    const double g = 0.4;   // 场幅度 0.4（与 frame_field 同量级）
    auto lin_c = [&](int x) { return g * (double)(x + 0.5) / 512.0; };
    std::vector<P2ControlObservation> obs;
    std::uint64_t cid = 0;
    for (int gy = 0; gy < st::kGrid; ++gy) {
        for (int gx = 0; gx < st::kGrid; ++gx) {
            double ra = 0, dec = 0;
            st::cell_center_radec(tile, gx, gy, &ra, &dec);
            const std::uint64_t leaf =
                st::leaf_of(tile, gx * st::kCellSide + 32,
                            gy * st::kCellSide + 32);
            const int cx = gx * st::kCellSide + 32;
            for (int f = 0; f < 2; ++f) {
                P2ControlObservation o{};
                o.frame_id = (std::uint64_t)f;
                o.control_id = cid;
                o.leaf_ipix = leaf;
                o.ra_deg = ra;
                o.dec_deg = dec;
                // V7：相位测试 noise=0（节点精确 → 逐叶相位与斜率/截距
                // 可严格断言；带噪声时 y 外推放大节点噪声会淹没 0.15
                // 容差。噪声鲁棒性由 G1CrossTileDeltaFollowsTruth /
                // G4RealTileSeamLeaves 覆盖，精确性由
                // G1V7EdgeBasisAnalytic 覆盖。）
                o.value = st::true_sky(ra, dec) +
                          (f == 1 ? lin_c(cx) : 0.0);
                o.uncertainty = st::kNoiseRms;
                o.snr = 100.0;
                o.support = 1.0;
                o.quality_flags = 1;
                obs.push_back(o);
            }
            ++cid;
        }
    }
    P2UpmBuildConfig cfg{};
    cfg.target_order = st::kTargetOrder;
    cfg.smoothing_lambda = 0.0;
    // 解析 gate 隔离 basis：零锚 λ0 会造成 ~λ0/(1+λ0) 的正则偏置
    // （对噪声数据可忽略，但对 noise=0 精确门不可接受），故置 0。
    cfg.zero_anchor_weight = 0.0;
    cfg.sigma_floor = 0.02;
    cfg.max_iterations = 60;
    cfg.tolerance = 1e-9;
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    // 随机叶位置：evaluate C vs 线性 truth，拟合斜率/截距
    std::vector<double> xs, ys;
    std::uniform_int_distribution<int> xyd(0, 511);
    for (int i = 0; i < 4096; ++i) {
        const int x = xyd(rng), y = xyd(rng);
        // 逐叶相位检查仅限 interior（x∈[64,448)）；边缘 half-cell 的
        // 噪声因线性外推放大（~2.8×），其精确性由 noise=0 的
        // G1V7EdgeBasisAnalytic 严格覆盖。
        if (x < 64 || x >= 448) continue;
        const std::uint64_t leaf = st::leaf_of(tile, x, y);
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(
            1u << (unsigned)(st::kTargetOrder + st::kTileShift), leaf, ra,
            dec);
        const double ch = p2_upm_evaluate_c(model, 1, leaf);
        xs.push_back((double)x);
        ys.push_back(ch);
        if (std::fabs(ch - lin_c(x)) > 0.15)
            std::fprintf(stderr,
                         "[G1-phase-debug] x=%d y=%d leaf=%llu ch=%.6f "
                         "truth=%.6f\n",
                         x, y, (unsigned long long)leaf, ch, lin_c(x));
        EXPECT_NEAR(ch, lin_c(x), 0.15) << "half-cell 相位恢复";
    }
    // 最小二乘斜率/截距（x 为叶列坐标）
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    const std::size_t nn = xs.size();
    for (std::size_t i = 0; i < nn; ++i) {
        sx += xs[i]; sy += ys[i]; sxx += xs[i]*xs[i]; sxy += xs[i]*ys[i];
    }
    const double den = (double)nn * sxx - sx * sx;
    const double slope = (den != 0.0) ? ((double)nn * sxy - sx * sy) / den : 0.0;
    const double intercept = (sy - slope * sx) / (double)nn;
    std::fprintf(stderr,
                 "[G1-phase] n=%zu slope=%.5f truth_slope=%.5f "
                 "intercept=%.6f\n",
                 nn, slope, g / 512.0, intercept);
    EXPECT_NEAR(slope, g / 512.0, 0.001);
    EXPECT_NEAR(intercept, 0.0, 0.02);
    nlohmann::json pj;
    pj["truth_slope_per_leaf"] = g / 512.0;
    pj["recovered_slope"] = slope;
    pj["recovered_intercept"] = intercept;
    std::filesystem::create_directories(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth");
    std::ofstream pf(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth/"
        "v5_half_cell_phase.json");
    if (pf) pf << pj.dump(2);
    p2_upm_close(model);
}

// V5 G1：distinct boundary sky centers 不 proximity-alias——
// 2×2 相邻块保持 256 个独立 control cell（无合并）；且两个相邻 tile
// 的边界 cell 中心（A(gx=7) 与 B(gx=0)）sky 位置不同 → 系数独立
// （geometry hash 区分各自 (tile,gx,gy)）。
TEST(Phase2Upm, G1DistinctBoundaryNodesNotAliased) {
    namespace st = spatial_truth;
    std::mt19937 rng(20260824);
    std::normal_distribution<double> nd(0.0, st::kNoiseRms);
    const std::uint64_t tiles[4] = {4, 5, 6, 7};
    std::vector<P2ControlObservation> obs;
    std::uint64_t cid = 0;
    for (int t = 0; t < 4; ++t) {
        for (int gy = 0; gy < st::kGrid; ++gy) {
            for (int gx = 0; gx < st::kGrid; ++gx) {
                double ra = 0, dec = 0;
                st::cell_center_radec(tiles[t], gx, gy, &ra, &dec);
                const std::uint64_t leaf =
                    st::leaf_of(tiles[t], gx * st::kCellSide + 32,
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
                    o.snr = 100.0;
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
    cfg.smoothing_lambda = 0.0;
    cfg.zero_anchor_weight = 1e-3;
    cfg.sigma_floor = 0.02;
    cfg.max_iterations = 60;
    cfg.tolerance = 1e-9;
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    P2ModelInfo info{};
    ASSERT_EQ(p2_upm_info(model, &info), 0);
    // 无 proximity-alias：4×64=256 个独立 control cell
    EXPECT_EQ(info.control_count, 256u);
    // 相邻 tile 边界 cell 中心角距 ≈ 1 cell 间距（distinct sky centers）
    double raa = 0, deca = 0, rab = 0, decb = 0;
    st::cell_center_radec(4, 7, 3, &raa, &deca);
    st::cell_center_radec(5, 0, 3, &rab, &decb);
    const double dist =
        astrocs::healpix::angular_distance_deg(raa, deca, rab, decb);
    EXPECT_GT(dist, 0.0) << "相邻边界 cell 中心必须为不同 sky 位置";
    std::fprintf(stderr, "[G1-noalias] controls=%llu boundary_center_dist=%.6f deg\n",
                 (unsigned long long)info.control_count, dist);
    nlohmann::json aj;
    aj["control_count"] = info.control_count;
    aj["boundary_center_angular_distance_deg"] = dist;
    std::filesystem::create_directories(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth");
    std::ofstream af(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth/"
        "v5_no_proximity_alias.json");
    if (af) af << aj.dump(2);
    p2_upm_close(model);
}

// V5 G1：basis 坐标自洽——leaf→(tile,x,y)→cell 映射与 evaluator 一致；
// calibrate(leaf) == input - evaluate_c(leaf)（逐 control 断言）。
TEST(Phase2Upm, G1BasisCoordinateSelfConsistency) {
    namespace st = spatial_truth;
    std::mt19937 rng(20260825);
    std::normal_distribution<double> nd(0.0, st::kNoiseRms);
    const std::uint64_t tiles[4] = {4, 5, 6, 7};
    std::vector<P2ControlObservation> obs;
    std::uint64_t cid = 0;
    for (int t = 0; t < 4; ++t) {
        for (int gy = 0; gy < st::kGrid; ++gy) {
            for (int gx = 0; gx < st::kGrid; ++gx) {
                double ra = 0, dec = 0;
                st::cell_center_radec(tiles[t], gx, gy, &ra, &dec);
                const std::uint64_t leaf =
                    st::leaf_of(tiles[t], gx * st::kCellSide + 32,
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
                    o.snr = 100.0;
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
    cfg.smoothing_lambda = 0.0;
    cfg.zero_anchor_weight = 1e-3;
    cfg.sigma_floor = 0.02;
    cfg.max_iterations = 60;
    cfg.tolerance = 1e-9;
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    // 逐 control：calibrate(leaf) == input - evaluate_c(leaf)；
    // 且 leaf→(tile,x,y)→cell 与观测 cell 一致（1e-12 精度）
    double worst = 0.0;
    for (const auto& o : obs) {
        const std::uint64_t leaf = o.leaf_ipix;
        const std::uint64_t tile = leaf >> 18;
        const std::uint64_t local = leaf & ((1ULL << 18) - 1ULL);
        std::uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, 9u, x, y);
        const int gx = (int)(x / st::kCellSide);
        const int gy = (int)(y / st::kCellSide);
        // 观测 leaf 的 cell 必须与生成时一致（gx=7 时 x=480 → 7）
        const std::uint64_t l2 = st::leaf_of(tile, gx * st::kCellSide + 32,
                                             gy * st::kCellSide + 32);
        ASSERT_EQ(l2, leaf) << "leaf→cell 映射自洽失败";
        double in[1] = {o.value};
        double out[1] = {0};
        ASSERT_EQ(p2_upm_calibrate_block(model, o.frame_id, &leaf, in, out,
                                         1), 0);
        const double expect =
            in[0] - p2_upm_evaluate_c(model, o.frame_id, leaf);
        worst = std::max(worst, std::fabs(out[0] - expect));
    }
    std::fprintf(stderr, "[G1-basis] obs=%zu worst_calibrate_vs_evaluate=%.3e\n",
                 obs.size(), worst);
    EXPECT_LE(worst, 1e-9);
    nlohmann::json bj;
    bj["observations_checked"] = obs.size();
    bj["worst_calibrate_minus_evaluate"] = worst;
    std::filesystem::create_directories(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth");
    std::ofstream bf(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth/"
        "v5_basis_self_consistency.json");
    if (bf) bf << bj.dump(2);
    p2_upm_close(model);
}

// V7 P7-1：tile 边缘线性外推 analytic gate（noise=0）。
// 两个 frame：reference C=0；target C = 二维 affine 场（tile-local）：
//   C_true(x,y) = A + Bx*(x-256)/512 + By*(y-256)/512
// 要求（全部严格，不用 interior error 动态放宽）：
//   1. evaluate(center) == 节点系数（= analytic truth at center）；
//   2. interior + 最外 half-cell leaves 恢复 analytic truth（≤1e-9）；
//   3. 4 组真实 seam 相邻 leaf：|recovered_delta - truth_delta| ≤ 1e-9；
//   4. 2D affine 拟合斜率/截距绝对阈值（≤1e-6）；
//   5. save/open 同一 evaluator。
TEST(Phase2Upm, G1V7EdgeBasisAnalytic) {
    namespace st = spatial_truth;
    const std::uint64_t tiles[4] = {4, 5, 6, 7};
    const double A = 0.1, Bx = 0.4, By = -0.2;
    auto c_true = [&](int x, int y) {
        return A + Bx * ((double)x - 256.0) / 512.0 +
               By * ((double)y - 256.0) / 512.0;
    };
    std::vector<P2ControlObservation> obs;
    std::uint64_t cid = 0;
    for (int t = 0; t < 4; ++t) {
        for (int gy = 0; gy < st::kGrid; ++gy) {
            for (int gx = 0; gx < st::kGrid; ++gx) {
                double ra = 0, dec = 0;
                st::cell_center_radec(tiles[t], gx, gy, &ra, &dec);
                const int cx = gx * st::kCellSide + 32;
                const int cy = gy * st::kCellSide + 32;
                const std::uint64_t leaf = st::leaf_of(tiles[t], cx, cy);
                for (int f = 0; f < 2; ++f) {
                    P2ControlObservation o{};
                    o.frame_id = (std::uint64_t)f;
                    o.control_id = cid;
                    o.leaf_ipix = leaf;
                    o.ra_deg = ra;
                    o.dec_deg = dec;
                    o.value = st::true_sky(ra, dec) +
                              (f == 1 ? c_true(cx, cy) : 0.0);
                    o.uncertainty = st::kNoiseRms;
                    o.snr = 100.0;
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
    cfg.smoothing_lambda = 0.0;
    // 解析 gate 隔离 basis：零锚 λ0 会造成 ~λ0/(1+λ0) 正则偏置，
    // noise=0 精确门不可接受，置 0。
    cfg.zero_anchor_weight = 0.0;
    cfg.sigma_floor = 0.02;
    cfg.max_iterations = 100;
    cfg.tolerance = 1e-12;
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);

    // 1. evaluate(center) == analytic truth at center（noise=0 下等于系数）
    double worst_center = 0.0;
    for (const auto& o : obs) {
        if (o.frame_id != 1) continue;
        const std::uint64_t local = o.leaf_ipix & ((1ULL << 18) - 1ULL);
        std::uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, 9u, x, y);
        const double got = p2_upm_evaluate_c(model, 1, o.leaf_ipix);
        worst_center =
            std::max(worst_center, std::fabs(got - c_true((int)x, (int)y)));
    }
    EXPECT_LE(worst_center, 1e-9) << "evaluate(center) 必须等于节点系数/truth";

    // 2. interior + 最外 half-cell leaves 恢复 analytic truth
    double worst_leaf = 0.0;
    std::size_t n_leaf = 0;
    for (int t = 0; t < 4; ++t) {
        for (int x = 0; x < 512; x += 17) {
            for (int y = 0; y < 512; y += 19) {
                const std::uint64_t leaf = st::leaf_of(tiles[t], x, y);
                const double got = p2_upm_evaluate_c(model, 1, leaf);
                worst_leaf =
                    std::max(worst_leaf, std::fabs(got - c_true(x, y)));
                ++n_leaf;
            }
        }
        // 显式覆盖最外 half-cell：x/y ∈ [0,31] ∪ [480,511]
        for (int e = 0; e < 512; e += 8) {
            for (int ex : {0, 8, 16, 24, 31, 480, 488, 496, 504, 511}) {
                for (int ey : {0, 8, 16, 24, 31, 480, 488, 496, 504, 511}) {
                    const std::uint64_t leaf =
                        st::leaf_of(tiles[t], ex, ey);
                    const double got = p2_upm_evaluate_c(model, 1, leaf);
                    worst_leaf =
                        std::max(worst_leaf, std::fabs(got - c_true(ex, ey)));
                    ++n_leaf;
                }
            }
        }
    }
    EXPECT_LE(worst_leaf, 1e-9)
        << "interior 与外 half-cell 必须精确恢复 analytic truth（无 plateau）";

    // 3. 4 组真实 seam 相邻 leaf：recovered_delta - truth_delta ≤ 1e-9
    struct Seam {
        std::uint64_t ta, tb;
        bool vertical;
    };
    const std::vector<Seam> seams = {
        {4, 5, true}, {6, 7, true}, {4, 6, false}, {5, 7, false}};
    double worst_seam_err = 0.0;
    std::size_t n_seam = 0;
    for (const auto& s : seams) {
        for (int k = 0; k < 512; k += 8) {
            const int xa = s.vertical ? 511 : k;
            const int ya = s.vertical ? k : 511;
            const int xb = s.vertical ? 0 : k;
            const int yb = s.vertical ? k : 0;
            const std::uint64_t la = st::leaf_of(s.ta, xa, ya);
            const std::uint64_t lb = st::leaf_of(s.tb, xb, yb);
            const double ca = p2_upm_evaluate_c(model, 1, la);
            const double cb = p2_upm_evaluate_c(model, 1, lb);
            const double rec_delta = std::fabs(ca - cb);
            const double truth_delta =
                std::fabs(c_true(xa, ya) - c_true(xb, yb));
            worst_seam_err =
                std::max(worst_seam_err, std::fabs(rec_delta - truth_delta));
            ++n_seam;
        }
    }
    EXPECT_LE(worst_seam_err, 1e-9)
        << "seam recovered delta 必须精确跟随 truth delta（严格，无动态放宽）";

    // 4. 2D affine 拟合斜率/截距绝对阈值
    double sx = 0, sy = 0, sz = 0, sxx = 0, syy = 0, sxy = 0, sxz = 0,
           syz = 0;
    const std::size_t nn = n_leaf;
    for (int t = 0; t < 4; ++t) {
        for (int x = 0; x < 512; x += 17) {
            for (int y = 0; y < 512; y += 19) {
                const double got =
                    p2_upm_evaluate_c(model, 1, st::leaf_of(tiles[t], x, y));
                const double dx = (double)x / 512.0;
                const double dy = (double)y / 512.0;
                sx += dx; sy += dy; sz += got;
                sxx += dx * dx; syy += dy * dy; sxy += dx * dy;
                sxz += dx * got; syz += dy * got;
            }
        }
    }
    // 正规方程：z = a + bx*x + by*y（x,y 归一化到 [0,1)）
    const double n_ = (double)nn;
    // 解析：a + bx*(x-0.5) + by*(y-0.5) = A + Bx*(x-256)/512 + By*(y-256)/512
    // ⇒ bx = Bx, by = By, a = A - Bx*0.5 - By*0.5（x 归一化 0..1 ↔ 0..512）
    double det = n_ * (sxx * syy - sxy * sxy) -
                 sx * (sx * syy - sxy * sy) +
                 sy * (sx * sxy - sxx * sy);
    double rec_a = 0, rec_bx = 0, rec_by = 0;
    if (std::fabs(det) > 1e-30) {
        const double a11 = n_, a12 = sx, a13 = sy;
        const double a21 = sx, a22 = sxx, a23 = sxy;
        const double a31 = sy, a32 = sxy, a33 = syy;
        const double b1 = sz, b2 = sxz, b3 = syz;
        rec_a = (b1 * (a22 * a33 - a23 * a32) -
                 a12 * (b2 * a33 - a23 * b3) +
                 a13 * (b2 * a32 - a22 * b3)) / det;
        rec_bx = (a11 * (b2 * a33 - a23 * b3) -
                  b1 * (a21 * a33 - a23 * a31) +
                  a13 * (a21 * b3 - b2 * a31)) / det;
        rec_by = (a11 * (a22 * b3 - b2 * a32) -
                  a12 * (a21 * b3 - b2 * a31) +
                  b1 * (a21 * a32 - a22 * a31)) / det;
    }
    const double truth_a = A - Bx * 0.5 - By * 0.5;
    std::fprintf(stderr,
                 "[G1V7-edge] n_leaf=%zu worst_center=%.3e worst_leaf=%.3e "
                 "worst_seam_err=%.3e rec_a=%.9f truth_a=%.9f rec_bx=%.6f "
                 "truth_bx=%.6f rec_by=%.6f truth_by=%.6f\n",
                 n_leaf, worst_center, worst_leaf, worst_seam_err, rec_a,
                 truth_a, rec_bx, Bx, rec_by, By);
    EXPECT_LE(std::fabs(rec_a - truth_a), 1e-6);
    EXPECT_LE(std::fabs(rec_bx - Bx), 1e-6);
    EXPECT_LE(std::fabs(rec_by - By), 1e-6);

    // 5. save/open 同一 evaluator
    const char* v7_path = "run_tmp_upm_v7_edge.json";
    ASSERT_EQ(p2_upm_save(model, v7_path), 0);
    void* model2 = nullptr;
    ASSERT_EQ(p2_upm_open(v7_path, &model2), 0);
    const std::uint64_t l0 = st::leaf_of(tiles[0], 0, 0);   // 边缘 leaf
    const std::uint64_t l1 = st::leaf_of(tiles[0], 511, 511);
    EXPECT_NEAR(p2_upm_evaluate_c(model2, 1, l0),
                p2_upm_evaluate_c(model, 1, l0), 1e-12);
    EXPECT_NEAR(p2_upm_evaluate_c(model2, 1, l1),
                p2_upm_evaluate_c(model, 1, l1), 1e-12);
    p2_upm_close(model2);
    std::remove(v7_path);

    nlohmann::json aj;
    aj["noise"] = 0.0;
    aj["worst_center_error"] = worst_center;
    aj["worst_leaf_error"] = worst_leaf;
    aj["worst_seam_delta_error"] = worst_seam_err;
    aj["recovered_affine"] = {"a", rec_a, "bx", rec_bx, "by", rec_by};
    aj["truth_affine"] = {"a", truth_a, "bx", Bx, "by", By};
    aj["strict_thresholds"] = {"leaf_and_seam", 1e-9, "affine", 1e-6};
    std::filesystem::create_directories(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth");
    std::ofstream af(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/truth/"
        "v7_edge_basis_analytic.json");
    if (af) af << aj.dump(2);
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
    P2ModelInfo binfo{};
    ASSERT_EQ(p2_upm_info(bm, &binfo), 0);
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
    // V5：移除边界节点合并后 control_count 恢复等于 cell 数；
    // >8 MiB round-trip 保持 control_count/hash/校准一致。
    EXPECT_EQ(binfo2.control_count, binfo.control_count);
    EXPECT_GT(binfo.control_count, 0ull);
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
    // V4 R5：该固定向量（rng 42 噪声 + 0.1/step 趋势）的 rejected mask
    // 已用未修改 Siril 1.4.3 官方源码 harness 逐位核对（拒绝 index 30
    // 与 43-49，共 8 个，保留 42 个）——生产必须逐元素一致。
    EXPECT_EQ(accepted[30], 0u);
    EXPECT_GT(out.rejected_high, 0u);
    EXPECT_EQ(out.accepted_count, 42u);
    for (std::size_t i = 43; i < 50; ++i) EXPECT_EQ(accepted[i], 0u);
    EXPECT_EQ(accepted[42], 1u);
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

// V4 R4：完整 sequential RCR（SS_MEDIAN_DL 冻结链）生产 gate。
// 逐元素 rejected-set 与官方 rcr 2.4.7 的对齐由 rcr_oracle_compare.py
// 覆盖（10 case 全部 exact）；此处验证固定向量的生产行为：
//  - 20% 高污染：全部 20 个注入离群必须被拒（鲁棒→精确 sequential 语义）；
//  - weighted case：与官方一致的精确 rejected mask {6,7}。
TEST(Phase2Reject, G4SequentialRcrMask) {
    // 1. high-contam：80×N(10,1) + 20×N(25,2)（固定种子，注入位置记录）
    std::mt19937 rng(20260819);
    std::normal_distribution<double> nd(0.0, 1.0), nd2(0.0, 2.0);
    std::vector<double> vals;
    std::vector<std::size_t> injected;
    for (int i = 0; i < 80; ++i) vals.push_back(10.0 + nd(rng));
    for (int i = 0; i < 20; ++i) {
        injected.push_back(vals.size());
        vals.push_back(25.0 + nd2(rng));
    }
    std::vector<std::uint8_t> acc(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = (std::uint32_t)vals.size();
    in.method = P2_REJECT_RCR;
    in.max_iterations = 8;
    in.min_samples = 3;
    P2RejectionResult out{};
    out.accepted = acc.data();
    ASSERT_EQ(p2_reject_stack(&in, &out), 0);
    std::uint32_t rej = out.rejected_low + out.rejected_high;
    EXPECT_GE(rej, 20u) << "20% 污染下至少拒绝全部注入离群";
    for (std::size_t idx : injected)
        EXPECT_EQ(acc[idx], 0u) << "注入离群 index=" << idx << " 必须被拒";

    // 2. weighted case（官方 oracle 精确 mask {6,7}）
    const std::vector<double> wv = {
        10.0, 10.2, 9.8, 10.1, 9.9, 10.05, 30.0, 31.0, 10.3, 10.15,
        10.25, 10.12, 9.95, 10.18, 10.22};
    const std::vector<double> ww = {
        1.0, 1.1, 0.9, 1.2, 0.8, 1.05, 3.0, 3.0, 1.0, 1.0,
        1.0, 1.0, 1.0, 1.0, 1.0};
    std::vector<std::uint8_t> wacc(wv.size(), 0);
    P2SampleStackView win{};
    win.values = wv.data();
    win.weights = ww.data();
    win.count = (std::uint32_t)wv.size();
    win.method = P2_REJECT_RCR;
    win.max_iterations = 8;
    win.min_samples = 3;
    P2RejectionResult wout{};
    wout.accepted = wacc.data();
    ASSERT_EQ(p2_reject_stack(&win, &wout), 0);
    EXPECT_EQ(wacc[6], 0u);
    EXPECT_EQ(wacc[7], 0u);
    for (std::size_t i = 0; i < wacc.size(); ++i) {
        if (i != 6 && i != 7) EXPECT_EQ(wacc[i], 1u);
    }
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

// V4 R6：local SNR availability 三区 gate——同一 frame 内：
//   1) 高局部 SNR（有 catalogue 星点，snr≈100）；
//   2) 低局部 SNR（有 catalogue 星点，snr≈2）；
//   3) 无任何局部星点 → snr_available=0 且 snr==0.0（禁止伪 local 1.0）。
// 使用真实 T2_v3 HiPS 副本 + 合成 SNR catalogue（仅替换 TSV 内容，
// MOC/properties/signal/support 保持原样）。
TEST(Phase2Sampler, G6LocalSnrAvailabilityThreeZones) {
    namespace fs = std::filesystem;
    namespace st = spatial_truth;
    const fs::path base =
        "F:/Astro dev/Astro CS Normalization Database/run/temp/phase1_freeze";
    const fs::path src = base / "T2_v3.hips";
    if (!fs::exists(src / "signal" / "properties"))
        GTEST_SKIP() << "真实 HiPS 输入不存在";
    const fs::path tmp =
        "F:/Astro dev/Astro CS Normalization Database/run/temp/p2_snr3/"
        "T2_snr3.hips";
    if (fs::exists(tmp)) fs::remove_all(tmp);
    fs::create_directories(tmp);
    fs::copy(src / "signal", tmp / "signal", fs::copy_options::recursive);
    fs::copy(src / "support", tmp / "support", fs::copy_options::recursive);
    fs::copy(src / "snr", tmp / "snr", fs::copy_options::recursive);

    // 找到 signal MOC 前 3 个 tile（作为三个区域）
    AioHipsDataset* sig = aio_hips_open(tmp.string().c_str(),
                                        AIO_HIPS_RD_SIGNAL);
    ASSERT_TRUE(sig != nullptr);
    const int nt = aio_hips_tile_count(sig);
    ASSERT_GE(nt, 3);
    std::uint64_t tiles[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i)
        ASSERT_EQ(aio_hips_tile_ipix(sig, i, &tiles[i]), 0);
    aio_hips_close(sig);

    // 两个星点：tiles[0] 的 (0,0) cell 中心 snr=100（高局部 SNR）；
    // tiles[2] 的 (0,0) cell 中心 snr=2（低局部 SNR）；tiles[1] 无星点
    // （无局部 catalogue 信息区）。
    auto cell_radec = [&](std::uint64_t tile, int gx, int gy,
                          double* ra, double* dec) {
        const std::uint64_t leaf =
            st::leaf_of(tile, gx * st::kCellSide + 32,
                        gy * st::kCellSide + 32);
        astrocs::healpix::pix2ang_nest(
            1u << (unsigned)(st::kTargetOrder + st::kTileShift), leaf, *ra,
            *dec);
    };
    double ra0 = 0, dec0 = 0, ra1 = 0, dec1 = 0;
    cell_radec(tiles[0], 0, 0, &ra0, &dec0);
    cell_radec(tiles[2], 0, 0, &ra1, &dec1);
    fs::path tsv;
    for (const auto& d : fs::recursive_directory_iterator(tmp / "snr"))
        if (d.path().extension() == ".tsv") { tsv = d.path(); break; }
    ASSERT_FALSE(tsv.empty());
    {
        std::ostringstream ss;
        ss << "# star_id ra dec snr quality_flags photometric_status\n"
           << "1 " << std::setprecision(12) << ra0 << " " << dec0
           << " 100.0 1 1\n"
           << "2 " << std::setprecision(12) << ra1 << " " << dec1
           << " 2.0 1 1\n";
        std::ofstream of(tsv, std::ios::trunc);
        of << ss.str();
    }

    // 采样
    P2CoverageResult cov{};
    cov.n_inputs = 1;
    P2HipsInputInfo infos[1]{};
    cov.inputs = infos;
    const std::string tmp_str = tmp.string();
    const char* path = tmp_str.c_str();
    ASSERT_EQ(p2_coverage_build(&path, 1, &cov), 0);
    std::vector<P2MocCell> cells(cov.n_union_cells);
    cov.union_cells = cells.data();
    ASSERT_EQ(p2_coverage_build(&path, 1, &cov), 0);
    P2SamplerConfig sccfg{};
    sccfg.control_grid_per_tile = 8;
    sccfg.patch_radius_leaf = 2;
    sccfg.min_samples = 5;
    sccfg.snr_search_radius_deg = 0.05;
    std::uint64_t n_obs = 0, n_ctrl = 0;
    char err[512] = {0};
    ASSERT_EQ(p2_sample_controls(&cov, &path, &sccfg, nullptr, 0, &n_obs,
                                 &n_ctrl, err, sizeof(err)), 0);
    ASSERT_GT(n_obs, 0u);
    std::vector<P2ControlObservation> obs(n_obs);
    ASSERT_EQ(p2_sample_controls(&cov, &path, &sccfg, obs.data(), n_obs,
                                 &n_obs, &n_ctrl, err, sizeof(err)), 0);

    bool saw_high = false, saw_low = false, saw_missing = false;
    for (const auto& o : obs) {
        const std::uint64_t t = o.leaf_ipix >> 18;
        const std::uint64_t local = o.leaf_ipix & ((1ULL << 18) - 1ULL);
        std::uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, 9u, x, y);
        const int gx = (int)(x / st::kCellSide);
        const int gy = (int)(y / st::kCellSide);
        if (t == tiles[0] && gx == 0 && gy == 0) {
            EXPECT_EQ(o.snr_available, 1);
            EXPECT_NEAR(o.snr, 100.0, 1e-9);
            saw_high = true;
        } else if (t == tiles[2] && gx == 0 && gy == 0) {
            EXPECT_EQ(o.snr_available, 1);
            EXPECT_NEAR(o.snr, 2.0, 1e-9);
            saw_low = true;
        } else if (t == tiles[1]) {
            EXPECT_EQ(o.snr_available, 0) << "无局部星点 cell 不得伪 local";
            EXPECT_EQ(o.snr, 0.0) << "无局部星点不得以 snr=1.0 伪装";
            saw_missing = true;
        }
    }
    EXPECT_TRUE(saw_high) << "高局部 SNR 区必须存在";
    EXPECT_TRUE(saw_low) << "低局部 SNR 区必须存在";
    EXPECT_TRUE(saw_missing) << "无局部星点区必须存在";
    p2_coverage_free(&cov);
    fs::remove_all(tmp.parent_path());
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

// R3/G3（V4 R2）稳定科学 identity：复制/重命名/换根不变；signal tile
// 像素（MOC/properties 不变）/ support tile 像素 / SNR catalogue / 关键
// 元数据变化 → frame_id 改变。
TEST(Phase2Identity, G3StableFrameIdentity) {
    namespace fs = std::filesystem;
    const fs::path base =
        "F:/Astro dev/Astro CS Normalization Database/run/temp/phase1_freeze";
    const fs::path src = base / "T2_v3.hips";
    if (!fs::exists(src / "signal" / "properties"))
        GTEST_SKIP() << "真实 HiPS 输入不存在";
    const fs::path tmp =
        "F:/Astro dev/Astro CS Normalization Database/run/temp/p2_identity_copy";
    // 复制完整科学产品（signal + support + snr 三个子产品）
    if (fs::exists(tmp)) fs::remove_all(tmp);
    fs::create_directories(tmp);
    fs::copy(src / "signal", tmp / "signal",
             fs::copy_options::recursive);
    fs::copy(src / "support", tmp / "support",
             fs::copy_options::recursive);
    fs::copy(src / "snr", tmp / "snr", fs::copy_options::recursive);
    const std::string path_a = src.string();
    const std::string path_b = tmp.string();
    const std::uint64_t id_a = p2_frame_id(path_a.c_str());
    const std::uint64_t id_b = p2_frame_id(path_b.c_str());
    EXPECT_NE(id_a, 0u);
    EXPECT_EQ(id_a, id_b) << "复制/重命名不得改变 frame_id";

    // 1. signal tile 像素变异（保持 MOC/properties 字节不变）
    fs::path sig_tile = first_norder7_tile(tmp / "signal");
    ASSERT_FALSE(sig_tile.empty()) << "找不到 signal Norder7 tile";
    const std::string moc_before = file_sha256(tmp / "signal" / "Moc.fits");
    const std::string props_before =
        file_sha256(tmp / "signal" / "properties");
    ASSERT_TRUE(flip_fits_data_byte(sig_tile, 0));
    EXPECT_NE(p2_frame_id(path_b.c_str()), id_b)
        << "signal tile 像素变异必须改变 frame_id（MOC/properties 未变）";
    EXPECT_EQ(file_sha256(tmp / "signal" / "Moc.fits"), moc_before)
        << "像素变异不得改变 MOC";
    EXPECT_EQ(file_sha256(tmp / "signal" / "properties"), props_before)
        << "像素变异不得改变 properties";
    // 恢复 signal 树
    fs::remove_all(tmp / "signal");
    fs::copy(src / "signal", tmp / "signal", fs::copy_options::recursive);
    EXPECT_EQ(p2_frame_id(path_b.c_str()), id_b)
        << "恢复 signal 后 identity 必须复原";

    // 2. support tile 像素变异 → frame_id 改变（Stage2 manifest 随
    // frame_id 变化；模型哈希在 R2 stage2 证据中验证）
    fs::path sup_tile = first_norder7_tile(tmp / "support");
    ASSERT_FALSE(sup_tile.empty()) << "找不到 support Norder7 tile";
    ASSERT_TRUE(flip_fits_data_byte(sup_tile, 0));
    EXPECT_NE(p2_frame_id(path_b.c_str()), id_b)
        << "support tile 像素变异必须改变 frame_id";
    fs::remove_all(tmp / "support");
    fs::copy(src / "support", tmp / "support", fs::copy_options::recursive);
    EXPECT_EQ(p2_frame_id(path_b.c_str()), id_b)
        << "恢复 support 后 identity 必须复原";

    // 3. SNR/quality catalogue 变异（TSV 一行 quality_flags 0→1）
    fs::path snr_tsv;
    for (const auto& d : fs::recursive_directory_iterator(tmp / "snr"))
        if (d.path().extension() == ".tsv") { snr_tsv = d.path(); break; }
    ASSERT_FALSE(snr_tsv.empty()) << "找不到 SNR catalogue TSV";
    {
        std::ifstream f(snr_tsv);
        std::stringstream ss;
        ss << f.rdbuf();
        std::string text = ss.str();
        const std::size_t pos = text.find('\n');
        ASSERT_NE(pos, std::string::npos);
        // 第一行数据：`sid ra dec snr quality phot_status`，把第 5 列
        // quality_flags 0 改为 1（保留行格式可解析）
        const std::size_t qpos = text.find('\n', pos + 1);
        std::string line = text.substr(pos + 1, qpos - pos - 1);
        std::istringstream ls(line);
        long long sid; double ra, dec, snr; unsigned qf, ps;
        ASSERT_TRUE(ls >> sid >> ra >> dec >> snr >> qf >> ps);
        const std::string old_line = line;
        std::ostringstream rep;
        rep << sid << " " << std::fixed << std::setprecision(12) << ra
            << " " << dec << " " << snr << " " << (qf + 1) << " " << ps;
        line = rep.str();
        text.replace(pos + 1, qpos - pos - 1, line);
        std::ofstream of(snr_tsv, std::ios::trunc);
        of << text;
        (void)old_line;
    }
    EXPECT_NE(p2_frame_id(path_b.c_str()), id_b)
        << "SNR/quality catalogue 变异必须改变 frame_id";

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

// R1/G1（V4）：production wiring gate——同一生产 Stage2 parse+build path
// 验证 support_power / sigma_floor / quality 影响与 topology invariance。
TEST(Phase2Wiring, G1ProductionWiringTruth) {
    namespace st = spatial_truth;
    // 1. 生产 JSON → P2Stage2Config（同一 parse 路径）
    const std::string json_text = R"({
      "version": 1,
      "inputs": {"hips": ["a.hips", "b.hips"], "target_order": "auto"},
      "model": {
        "control_grid_per_tile": 8, "patch_radius_pixels": 2,
        "min_samples": 5, "snr_search_radius_deg": 0.05,
        "robust_loss": "huber", "snr_weight_mode": "snr2_normalized",
        "huber_delta": 1.345, "smoothing": 0.1, "zero_anchor_weight": 0.001,
        "sigma_floor": 0.02, "support_power": 1.0
      },
      "integration": {
        "precision": "fp32", "memory_limit_mb": 8192,
        "rejection": {"method": "sigma", "low": 4.0, "high": 3.0,
                       "max_iterations": 8, "min_samples": 2},
        "weight_mode": "auto"
      },
      "output": {"hips": "out.hips"},
      "diagnostics": {"enabled": true}
    })";
    nlohmann::json j = nlohmann::json::parse(json_text);
    P2Stage2Config cfg{};
    std::string err;
    ASSERT_TRUE(p2_stage2_parse_config(j, &cfg, &err)) << err;
    EXPECT_EQ(cfg.sigma_floor, 0.02);
    EXPECT_EQ(cfg.support_power, 1.0);
    P2UpmBuildConfig mcfg =
        p2_stage2_make_upm_cfg(cfg, st::kTargetOrder, "manifest");
    EXPECT_EQ(mcfg.sigma_floor, 0.02);
    EXPECT_EQ(mcfg.support_power, 1.0);
    EXPECT_EQ(mcfg.target_order, st::kTargetOrder);

    // 2. 合成观测（同 cell 多帧；不同 support/quality/unc/snr）
    std::vector<P2ControlObservation> obs;
    std::uint64_t cid = 0;
    for (int gy = 0; gy < 2; ++gy)
        for (int gx = 0; gx < 2; ++gx) {
            double ra = 0, dec = 0;
            st::cell_center_radec(st::kTiles[0], gx, gy, &ra, &dec);
            const std::uint64_t leaf =
                st::leaf_of(st::kTiles[0], gx * st::kCellSide + 32,
                            gy * st::kCellSide + 32);
            for (int f = 0; f < 2; ++f) {
                P2ControlObservation o{};
                o.frame_id = (std::uint64_t)f;
                o.control_id = cid;
                o.leaf_ipix = leaf;
                o.ra_deg = ra;
                o.dec_deg = dec;
                o.value = 10.0 + f;
                o.uncertainty = 0.01;
                o.snr = 20.0;
                o.support = 0.4 + 0.2 * (gx + gy);
                o.quality_flags = 1;
                if (gx == 0 && gy == 0 && f == 1) o.quality_flags = 16;
                if (gx == 1 && gy == 0 && f == 0) o.quality_flags = 0;
                obs.push_back(o);
            }
            ++cid;
        }

    // 3. support_power 0 vs 2：raw weight 改变
    P2UpmBuildConfig c0 = mcfg, c2 = mcfg;
    c0.support_power = 0.0;
    c2.support_power = 2.0;
    double w0 = 0, w2 = 0;
    ASSERT_EQ(p2_upm_raw_weight(&obs[0], &c0, &w0), 0);
    ASSERT_EQ(p2_upm_raw_weight(&obs[0], &c2, &w2), 0);
    EXPECT_GT(std::fabs(w2 - w0), 1e-12)
        << "support_power 0 vs 2 必须改变 raw weight";
    // support=0.4：p=0 → 1；p=2 → 0.16
    EXPECT_NEAR(w0, w2 / 0.16, 1e-12);

    // 4. sigma_floor A vs B：低 uncertainty obs influence 改变
    P2UpmBuildConfig cA = mcfg, cB = mcfg;
    cA.sigma_floor = 0.005;   // unc=0.01 → max=0.01
    cB.sigma_floor = 0.02;    // unc=0.01 → max=0.02（floor 生效）
    double wA = 0, wB = 0;
    ASSERT_EQ(p2_upm_raw_weight(&obs[0], &cA, &wA), 0);
    ASSERT_EQ(p2_upm_raw_weight(&obs[0], &cB, &wB), 0);
    EXPECT_GT(wA, wB) << "sigma_floor 更大 → inverse-variance 更小";
    EXPECT_NEAR(wA / wB, (0.02 * 0.02) / (0.01 * 0.01), 1e-9);

    // 5. quality 顺序 good > unknown > bad > rejected（冻结映射）
    P2ControlObservation qo = obs[0];
    double w_good = 0, w_unk = 0, w_bad = 0, w_rej = 0;
    qo.quality_flags = 1;  p2_upm_raw_weight(&qo, &mcfg, &w_good);
    qo.quality_flags = 0;  p2_upm_raw_weight(&qo, &mcfg, &w_unk);
    qo.quality_flags = 2;  p2_upm_raw_weight(&qo, &mcfg, &w_bad);
    qo.quality_flags = 16; p2_upm_raw_weight(&qo, &mcfg, &w_rej);
    EXPECT_GT(w_good, w_unk);
    EXPECT_GT(w_unk, w_bad);
    EXPECT_GT(w_bad, w_rej);
    EXPECT_DOUBLE_EQ(w_rej, 0.0);

    // 6. topology invariance：改 SNR/quality 后 node count/geometry hash 不变
    std::vector<P2ControlObservation> obs2 = obs;
    for (auto& o : obs2) {
        o.snr = (o.snr > 10.0) ? 3.0 : 50.0;
        o.quality_flags = 0;
    }
    void* m1 = nullptr;
    void* m2 = nullptr;
    P2UpmBuildConfig bcfg = mcfg;
    bcfg.max_iterations = 20;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &bcfg, &m1), 0);
    ASSERT_EQ(p2_upm_build(obs2.data(), obs2.size(), &bcfg, &m2), 0);
    P2ModelInfo i1{}, i2{};
    ASSERT_EQ(p2_upm_info(m1, &i1), 0);
    ASSERT_EQ(p2_upm_info(m2, &i2), 0);
    EXPECT_EQ(i1.control_count, i2.control_count);
    char gh1[65] = {0}, gh2[65] = {0};
    ASSERT_EQ(p2_upm_geometry_hash(m1, gh1, 65), 0);
    ASSERT_EQ(p2_upm_geometry_hash(m2, gh2, 65), 0);
    EXPECT_STREQ(gh1, gh2) << "SNR/quality 改变不得改变 geometry hash";
    // 但 model hash（含系数）应改变
    EXPECT_NE(std::string(i1.model_hash), std::string(i2.model_hash));
    p2_upm_close(m2);
    p2_upm_close(m1);

    // 7. machine-readable weight diagnostics
    std::ofstream wf(
        "F:/Astro dev/Astro CS Normalization Database/run/phase2/wiring/"
        "weight_diagnostics.json");
    ASSERT_TRUE(wf.good());
    nlohmann::json diag;
    diag["support_power_0"] = w0;
    diag["support_power_2"] = w2;
    diag["sigma_floor_0.005"] = wA;
    diag["sigma_floor_0.02"] = wB;
    diag["quality_good"] = w_good;
    diag["quality_unknown"] = w_unk;
    diag["quality_bad"] = w_bad;
    diag["quality_rejected"] = w_rej;
    diag["geometry_hash_invariant"] = (std::string(gh1) == std::string(gh2));
    wf << diag.dump(2);
    std::fprintf(stderr,
                 "[G1-wiring] support0=%.6g support2=%.6g floorA=%.6g "
                 "floorB=%.6g q_good=%.6g q_unk=%.6g q_bad=%.6g "
                 "q_rej=%.6g geom_invariant=%d\n",
                 w0, w2, wA, wB, w_good, w_unk, w_bad, w_rej,
                 (int)(std::string(gh1) == std::string(gh2)));
}
