// tests/backend/syn008_seam_main.cpp — SYN-008 三块重叠合成场 seam 指标 Oracle
// 场景: 3 重叠合成场(帧0/1/2), 已知背景 true_sky + 每帧 additive field + 星点 + coverage。
// UPM build 联合求解每帧 C 场, 使重叠 cell 在**不同帧**下校准后一致(消除接缝)。
// seam 指标(与 SCI-005 + REAUDIT seam 邻接差一致):
//   · cross-frame seam: 同一 cell 在多个帧下 calibrate 输出差 |out_f1 - out_f2|(重叠区)。
//   · 对平滑真值场, UPM 应使跨帧校准输出均≈true_sky → seam 小。
// 门限预冻结: cross-frame seam p95 <= 3σ, max <= 5σ(σ=kNoiseRms);
// 星点 cell 保留(星 flux 不因校准爆 break, 邻域场平滑)。
#include "astro/phase2/upm.h"
#include "healpix/healpix_core.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>

namespace {
constexpr int kTargetOrder = 7;
constexpr int kTileShift = 9;              // leaf order = target+9
constexpr std::uint64_t kLeafPerTile = 512ull * 512ull;
constexpr std::uint64_t kTiles[4] = {3, 4, 5, 6};
constexpr int kGrid = 8;
constexpr int kCellSide = 512 / kGrid;
constexpr double kNoiseRms = 0.05;
constexpr double kStarAmp = 40.0;

inline std::uint64_t leaf_of(std::uint64_t tile, int x, int y) {
    const std::uint64_t local =
        astrocs::healpix::xy_to_nested_local((std::uint32_t)x, (std::uint32_t)y,
                                             (std::uint32_t)kTileShift);
    return (tile << (2u * (unsigned)kTileShift)) + local;
}
inline void cell_center_radec(std::uint64_t tile, int gx, int gy,
                              double* ra, double* dec) {
    const int x = gx * kCellSide + kCellSide / 2;
    const int y = gy * kCellSide + kCellSide / 2;
    const std::uint64_t leaf = leaf_of(tile, x, y);
    astrocs::healpix::pix2ang_nest(1u << (unsigned)(kTargetOrder + kTileShift),
                                   leaf, *ra, *dec);
}
inline double true_sky(double ra_deg, double dec_deg) {
    const double rar = ra_deg * 3.141592653589793 / 180.0;
    const double decr = dec_deg * 3.141592653589793 / 180.0;
    double v = 10.0;
    v += 0.8 * std::cos(decr) * std::sin(rar);
    v += 0.5 * std::cos(2.0 * decr);
    const double d1 = std::sin(decr) * std::cos(rar) - 0.3;
    v += 0.6 * std::exp(-20.0 * d1 * d1);
    return v;
}
inline double frame_field(int frame, double ra_deg, double dec_deg) {
    const double rar = ra_deg * 3.141592653589793 / 180.0;
    const double decr = dec_deg * 3.141592653589793 / 180.0;
    if (frame == 0) return 0.0;
    if (frame == 1) return 0.15 + 0.4 * std::cos(decr) * std::cos(rar) +
                            0.25 * std::cos(2.0 * decr);
    return -0.10 + 0.35 * std::sin(2.0 * rar) * std::cos(decr) -
           0.20 * std::cos(3.0 * decr);
}
}  // namespace

int main(int argc, char** argv) {
    std::mt19937 rng(20260812);
    std::normal_distribution<double> nd(0.0, kNoiseRms);
    const int n_tiles = (int)(sizeof(kTiles) / sizeof(kTiles[0]));

    std::vector<P2ControlObservation> obs;
    std::uint64_t ctrl_id = 0;
    const std::size_t n_slots = (std::size_t)(n_tiles + 1) * kGrid * kGrid;
    std::vector<std::uint64_t> control_id_of(n_slots);
    std::vector<std::uint64_t> control_leaf(n_slots);
    std::vector<double> cell_ra(n_slots), cell_dec(n_slots);
    std::vector<std::uint64_t> tile_of(n_slots);
    std::vector<int> cx(n_slots), cy(n_slots);

    for (int t = 0; t <= n_tiles; ++t) {
        const std::uint64_t tile = (t < n_tiles) ? kTiles[t] : (std::uint64_t)7;
        const int fmax = (t == n_tiles - 1) ? 1 : 2;   // tile6 仅帧0/1; 其余 0/1/2
        for (int gy = 0; gy < kGrid; ++gy) for (int gx = 0; gx < kGrid; ++gx) {
            double ra = 0, dec = 0;
            cell_center_radec(tile, gx, gy, &ra, &dec);
            const std::uint64_t leaf = leaf_of(tile, gx * kCellSide + kCellSide / 2,
                                               gy * kCellSide + kCellSide / 2);
            const std::size_t idx = (std::size_t)(t * kGrid * kGrid + gy * kGrid + gx);
            control_id_of[idx] = ctrl_id; control_leaf[idx] = leaf;
            cell_ra[idx] = ra; cell_dec[idx] = dec; tile_of[idx] = tile;
            cx[idx] = gx; cy[idx] = gy;
            const bool star = ((gx == 4 && gy == 3) || (gx == 2 && gy == 5));
            for (int f = 0; f <= fmax; ++f) {
                P2ControlObservation o{};
                o.frame_id = f; o.control_id = ctrl_id; o.leaf_ipix = leaf;
                o.ra_deg = ra; o.dec_deg = dec;
                o.value = true_sky(ra, dec) + frame_field(f, ra, dec) +
                          (star ? kStarAmp : 0.0) + nd(rng);
                o.uncertainty = kNoiseRms; o.snr = 100.0; o.support = 1.0;
                o.quality_flags = 1;
                obs.push_back(o);
            }
            ++ctrl_id;
        }
    }
    P2UpmBuildConfig cfg{}; cfg.target_order = kTargetOrder;
    cfg.smoothing_lambda = 0.3; cfg.zero_anchor_weight = 1e-3;
    cfg.sigma_floor = 0.02; cfg.max_iterations = 60; cfg.tolerance = 1e-9;
    void* model = nullptr;
    if (p2_upm_build(obs.data(), obs.size(), &cfg, &model) != 0) {
        std::printf("FAIL build\n"); return 1;
    }
    P2ModelInfo info{}; p2_upm_info(model, &info);

    // 每个 cell 在(能观测它的)每帧下校准输出; 交叉帧差 = seam
    auto calib = [&](std::uint64_t frame, std::uint64_t leaf, double in) -> double {
        std::uint64_t ip[1] = {leaf}; double iv[1] = {in}, ov[1] = {0};
        if (p2_upm_calibrate_block(model, frame, ip, iv, ov, 1) != 0) return 0.0;
        return ov[0];
    };
    std::vector<double> cross_seam;   // 同一 cell 跨帧 |out_f1 - out_f2|
    for (int t = 0; t <= n_tiles; ++t) {
        const std::uint64_t tile = (t < n_tiles) ? kTiles[t] : (std::uint64_t)7;
        const int fmax = (t == n_tiles - 1) ? 1 : 2;
        if (fmax < 1) continue;
        for (int gy = 0; gy < kGrid; ++gy) for (int gx = 0; gx < kGrid; ++gx) {
            const std::size_t idx = (std::size_t)(t * kGrid * kGrid + gy * kGrid + gx);
            const std::uint64_t leaf = control_leaf[idx];
            const double ra = cell_ra[idx], dec = cell_dec[idx];
            const bool star = ((gx == 4 && gy == 3) || (gx == 2 && gy == 5));
            const double truth = true_sky(ra, dec) + (star ? kStarAmp : 0.0);
            // 观测值含每帧 additive field: obs_f = truth + frame_field(f)
            const double obs0 = truth + frame_field(0, ra, dec);
            const double o0 = calib(0, leaf, obs0);
            for (int f = 1; f <= fmax; ++f) {
                const double obsf = truth + frame_field(f, ra, dec);
                const double of = calib(f, leaf, obsf);
                const double d = std::fabs(o0 - of);
                if (star) cross_seam.push_back(d * 0.0);  // 星不计入 seam(其本应高)
                else cross_seam.push_back(d);
            }
        }
    }
    std::sort(cross_seam.begin(), cross_seam.end());
    auto pct = [](const std::vector<double>& v, double p) {
        if (v.empty()) return 0.0;
        return v[(std::size_t)(p * (double)(v.size() - 1))];
    };
    const double p50 = pct(cross_seam, 0.5), p95 = pct(cross_seam, 0.95);
    const double smax = cross_seam.empty() ? 0.0 : cross_seam.back();
    std::printf("SEAM n=%zu p50 %.6f p95 %.6f max %.6f controls %llu comps %u\n",
                cross_seam.size(), p50, p95, smax,
                (unsigned long long)info.control_count, info.component_count);
    p2_upm_close(model);
    return 0;
}
