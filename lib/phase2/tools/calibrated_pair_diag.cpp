// lib/phase2/tools/calibrated_pair_diag.cpp — P8-2 只读证据 helper
//
// 链接 production Phase2 库 + AIO，对银心 真实产物计算 calibrated frame
// pair 诊断：
// - p2_upm_open() 读取生产 UPM（upm_sparse.json）；
// - p2_frame_id() 取得真实 frame id；
// - p2_upm_calibrate_block() 对 overlap leaf 求 calibrated signal；
// - 不复制 UPM 数学公式（全部走 production API）。
//
// 输出 machine-readable calibrated_pair_metrics.json。
//
// 用法:
// calibrated_pair_diag <panel1.hips> <panel2.hips> <panel3.hips>
// <mosaic.hips> <mosaic_upm.json> <output.json>

#include "astro/phase2/upm.h"
#include "astro/phase2/sampler.h"

#include "healpix/healpix_core.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" {
#include "aio_hips_reader.h"
}

namespace {

constexpr int kTileWidth = 512;
constexpr int kTileShift = 9;
constexpr int kLeafOrder = 7;
constexpr double kSeamBandDeg = 0.25;
constexpr double kSmallFloor = 1e-5;  // median/lowfreq 允许的小量（固定）

double median_of(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

double mad_of(const std::vector<double>& v, double med) {
    std::vector<double> dev;
    dev.reserve(v.size());
    for (double x : v) dev.push_back(std::fabs(x - med));
    return median_of(std::move(dev));
}

double robust_sigma(const std::vector<double>& v, double med) {
    return 1.4826 * mad_of(v, med);
}

double p95_abs(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    std::vector<double> a;
    a.reserve(v.size());
    for (double x : v) a.push_back(std::fabs(x));
    const std::size_t idx = (std::size_t)(0.95 * (double)(a.size() - 1));
    std::nth_element(a.begin(), a.begin() + idx, a.end());
    return a[idx];
}

double max_abs(const std::vector<double>& v) {
    double m = 0.0;
    for (double x : v) m = std::max(m, std::fabs(x));
    return m;
}

struct LowFreq {
    double p95_abs = 0.0;
    double max_abs = 0.0;
    int n_bins = 0;
};

LowFreq low_freq(const std::vector<double>& ra, const std::vector<double>& dec,
                 const std::vector<double>& d, double bin_deg = 0.5) {
    LowFreq lf;
    std::unordered_map<std::int64_t, std::vector<double>> bins;
    for (std::size_t i = 0; i < d.size(); ++i) {
        if (!std::isfinite(d[i])) continue;
        const std::int64_t rb = (std::int64_t)std::floor(ra[i] / bin_deg);
        const std::int64_t db = (std::int64_t)std::floor(dec[i] / bin_deg);
        bins[rb * 100000 + db].push_back(d[i]);
    }
    std::vector<double> meds;
    meds.reserve(bins.size());
    for (auto& kv : bins) meds.push_back(median_of(std::move(kv.second)));
    lf.n_bins = (int)meds.size();
    lf.p95_abs = p95_abs(meds);
    lf.max_abs = max_abs(meds);
    return lf;
}

struct PairStats {
    std::string key;
    std::size_t n_shared_tiles = 0;
    std::size_t n = 0;
    double boundary_dec = 0.0;
    std::vector<double> ra, decs, raw, cal;

    nlohmann::json summary() const {
        const double rmed = median_of(raw);
        const double cmed = median_of(cal);
        const double rmed_abs = std::fabs(rmed);
        const double cmed_abs = std::fabs(cmed);
        const double rsigma = robust_sigma(raw, rmed);
        const double csigma = robust_sigma(cal, cmed);
        const LowFreq lf_r = low_freq(ra, decs, raw);
        const LowFreq lf_c = low_freq(ra, decs, cal);

        std::vector<double> r_seam, c_seam, r_int, c_int;
        r_seam.reserve(n / 4);
        c_seam.reserve(n / 4);
        r_int.reserve(n);
        c_int.reserve(n);
        for (std::size_t i = 0; i < raw.size(); ++i) {
            const bool in_seam =
                std::fabs(decs[i] - boundary_dec) <= kSeamBandDeg;
            (in_seam ? r_seam : r_int).push_back(raw[i]);
            (in_seam ? c_seam : c_int).push_back(cal[i]);
        }
        const double r_seam_med = median_of(r_seam);
        const double c_seam_med = median_of(c_seam);
        const double r_int_med = median_of(r_int);
        const double c_int_med = median_of(c_int);

        const bool med_ok = cmed_abs <= rmed_abs + kSmallFloor;
        const bool lf_ok = lf_c.p95_abs <= lf_r.p95_abs * 1.05 + kSmallFloor;
        const bool improved = lf_c.p95_abs < lf_r.p95_abs;

        nlohmann::json j;
        j["n_shared_tiles"] = n_shared_tiles;
        j["n_overlap_pixels"] = n;
        j["boundary_dec_deg"] = boundary_dec;
        j["seam_band_deg"] = kSeamBandDeg;
        j["raw_diff"] = {
            {"median", rmed},
            {"median_abs", rmed_abs},
            {"mad", mad_of(raw, rmed)},
            {"robust_sigma", rsigma},
            {"p95_abs", p95_abs(raw)},
            {"max_abs", max_abs(raw)},
            {"low_frequency_p95_abs", lf_r.p95_abs},
            {"low_frequency_max_abs", lf_r.max_abs},
            {"low_frequency_bins", lf_r.n_bins},
            {"seam_band_median", r_seam_med},
            {"seam_band_robust_sigma", robust_sigma(r_seam, r_seam_med)},
            {"interior_median", r_int_med},
            {"interior_robust_sigma", robust_sigma(r_int, r_int_med)},
        };
        j["cal_diff"] = {
            {"median", cmed},
            {"median_abs", cmed_abs},
            {"mad", mad_of(cal, cmed)},
            {"robust_sigma", csigma},
            {"p95_abs", p95_abs(cal)},
            {"max_abs", max_abs(cal)},
            {"low_frequency_p95_abs", lf_c.p95_abs},
            {"low_frequency_max_abs", lf_c.max_abs},
            {"low_frequency_bins", lf_c.n_bins},
            {"seam_band_median", c_seam_med},
            {"seam_band_robust_sigma", robust_sigma(c_seam, c_seam_med)},
            {"interior_median", c_int_med},
            {"interior_robust_sigma", robust_sigma(c_int, c_int_med)},
        };
        j["verdict"] = {
            {"calibrated_median_not_worse", med_ok},
            {"calibrated_lowfreq_not_worse_5pct", lf_ok},
            {"low_frequency_improved", improved},
            {"small_floor", kSmallFloor},
        };
        return j;
    }
};

} // namespace

int main(int argc, char** argv) {
    if (argc != 7) {
        std::fprintf(stderr,
                     "usage: calibrated_pair_diag <panel1.hips> <panel2.hips> "
                     "<panel3.hips> <mosaic.hips> <mosaic_upm.json> "
                     "<output.json>\n");
        return 2;
    }
    const char* paths[3] = {argv[1], argv[2], argv[3]};
    const char* mosaic_dir = argv[4];
    const char* model_path = argv[5];
    const char* out_path = argv[6];

    void* model = nullptr;
    if (p2_upm_open(model_path, &model) != 0) {
        std::fprintf(stderr, "p2_upm_open failed: %s\n", model_path);
        return 3;
    }
    P2ModelInfo info{};
    p2_upm_info(model, &info);

    // 真实 frame id（production payload identity）
    std::uint64_t fids[3];
    for (int i = 0; i < 3; ++i) fids[i] = p2_frame_id(paths[i]);

    // 打开 signal/support（3 panels + mosaic）
    AioHipsDataset* sig[3] = {nullptr, nullptr, nullptr};
    AioHipsDataset* sup[3] = {nullptr, nullptr, nullptr};
    for (int i = 0; i < 3; ++i) {
        sig[i] = aio_hips_open(paths[i], AIO_HIPS_RD_SIGNAL);
        sup[i] = aio_hips_open(paths[i], AIO_HIPS_RD_SUPPORT);
        if (!sig[i] || !sup[i]) {
            std::fprintf(stderr, "open %s failed: %s\n", paths[i],
                         aio_hips_reader_last_error());
            return 4;
        }
    }
    AioHipsDataset* mos_sig = aio_hips_open(mosaic_dir, AIO_HIPS_RD_SIGNAL);
    AioHipsDataset* mos_sup = aio_hips_open(mosaic_dir, AIO_HIPS_RD_SUPPORT);
    if (!mos_sig || !mos_sup) {
        std::fprintf(stderr, "open mosaic %s failed: %s\n", mosaic_dir,
                     aio_hips_reader_last_error());
        return 4;
    }

    // 每帧 tile 集合
    std::vector<std::unordered_set<std::uint64_t>> tiles(3);
    for (int i = 0; i < 3; ++i) {
        const int n = aio_hips_tile_count(sig[i]);
        for (int t = 0; t < n; ++t) {
            std::uint64_t ip = 0;
            if (aio_hips_tile_ipix(sig[i], t, &ip) == 0) tiles[i].insert(ip);
        }
    }

    PairStats pairs[2] = {{"panel1-panel2"}, {"panel2-panel3"}};
    const int idx[2][2] = {{0, 1}, {1, 2}};

    std::vector<float> si_buf, sj_buf, ui_buf, uj_buf, sm_buf, um_buf;
    const std::size_t tile_n = (std::size_t)kTileWidth * kTileWidth;
    si_buf.resize(tile_n);
    sj_buf.resize(tile_n);
    ui_buf.resize(tile_n);
    uj_buf.resize(tile_n);
    sm_buf.resize(tile_n);
    um_buf.resize(tile_n);

    for (int p = 0; p < 2; ++p) {
        const int i = idx[p][0], j = idx[p][1];
        PairStats& ps = pairs[p];
        std::vector<std::uint64_t> shared;
        for (std::uint64_t t : tiles[i])
            if (tiles[j].count(t)) shared.push_back(t);
        ps.n_shared_tiles = shared.size();

        for (std::uint64_t t : shared) {
            const bool ok_i =
                aio_hips_read_tile_f32(sig[i], t, si_buf.data()) == 0 &&
                aio_hips_read_tile_f32(sup[i], t, ui_buf.data()) == 0;
            const bool ok_j =
                aio_hips_read_tile_f32(sig[j], t, sj_buf.data()) == 0 &&
                aio_hips_read_tile_f32(sup[j], t, uj_buf.data()) == 0;
            const bool ok_m =
                aio_hips_read_tile_f32(mos_sig, t, sm_buf.data()) == 0 &&
                aio_hips_read_tile_f32(mos_sup, t, um_buf.data()) == 0;
            if (!ok_i || !ok_j || !ok_m) continue;

            std::vector<std::uint64_t> leaves;
            std::vector<double> raw_i, raw_j;
            leaves.reserve(tile_n);
            raw_i.reserve(tile_n);
            raw_j.reserve(tile_n);
            for (std::size_t k = 0; k < tile_n; ++k) {
                const float si = si_buf[k], sj = sj_buf[k];
                const float ui = ui_buf[k], uj = uj_buf[k];
                const float sm = sm_buf[k], um = um_buf[k];
                if (!std::isfinite(si) || !std::isfinite(sj) ||
                    !std::isfinite(sm) || ui <= 0.0f || uj <= 0.0f ||
                    um <= 0.0f)
                    continue;
                const std::uint64_t local =
                    astrocs::healpix::fits_index_to_nested_local(
                        (std::uint64_t)k, (std::uint32_t)kTileShift,
                        (std::uint32_t)kTileWidth);
                leaves.push_back((t << (2u * (unsigned)kTileShift)) | local);
                raw_i.push_back((double)si);
                raw_j.push_back((double)sj);
            }
            if (leaves.empty()) continue;
            std::vector<double> cal_i(leaves.size()), cal_j(leaves.size());
            if (p2_upm_calibrate_block(model, fids[i], leaves.data(),
                                       raw_i.data(), cal_i.data(),
                                       leaves.size()) != 0 ||
                p2_upm_calibrate_block(model, fids[j], leaves.data(),
                                       raw_j.data(), cal_j.data(),
                                       leaves.size()) != 0) {
                std::fprintf(stderr, "calibrate_block failed\n");
                return 5;
            }
            for (std::size_t k = 0; k < leaves.size(); ++k) {
                double ra = 0, dec = 0;
                astrocs::healpix::pix2ang_nest(
                    1u << (unsigned)(kLeafOrder + kTileShift), leaves[k], ra,
                    dec);
                ps.ra.push_back(ra);
                ps.decs.push_back(dec);
                ps.raw.push_back(raw_i[k] - raw_j[k]);
                ps.cal.push_back(cal_i[k] - cal_j[k]);
            }
        }
        ps.n = ps.raw.size();
        ps.boundary_dec = median_of(ps.decs);
        std::fprintf(stderr,
                     "[calpair] %s: shared=%zu px=%zu boundary_dec=%.4f "
                     "raw_med=%.3e cal_med=%.3e lf_raw=%.3e lf_cal=%.3e\n",
                     ps.key.c_str(), ps.n_shared_tiles, ps.n, ps.boundary_dec,
                     median_of(ps.raw), median_of(ps.cal),
                     low_freq(ps.ra, ps.decs, ps.raw).p95_abs,
                     low_freq(ps.ra, ps.decs, ps.cal).p95_abs);
    }

    nlohmann::json out;
    out["_description"] =
        "V8 P8-2: 真实 calibrated frame pair 诊断（production "
        "p2_upm_open + p2_upm_calibrate_block；calibrated = raw - C(frame,leaf)；"
        "非 Python 重写 evaluator）";
    out["model"] = {
        {"path", model_path},
        {"model_hash", std::string(info.model_hash)},
        {"control_count", info.control_count},
        {"observation_count", info.observation_count},
        {"component_count", info.component_count},
    };
    out["frames"] = {
        {"panel1", fids[0]}, {"panel2", fids[1]}, {"panel3", fids[2]}};
    out["inputs"] = {paths[0], paths[1], paths[2]};
    out["mosaic"] = mosaic_dir;
    out["pairs"] = {
        {"panel1-panel2", pairs[0].summary()},
        {"panel2-panel3", pairs[1].summary()},
    };
    out["conclusion"] = {
        {"note",
         "若 calibrated median/lowfreq 不显著恶化且至少一个 pair 有可测改善，"
         "则真实校准改善成立；否则如实说明，不用 mosaic residual 替代"},
    };

    std::ofstream f(out_path);
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", out_path);
        return 6;
    }
    f << out.dump(2);
    f.close();
    std::fprintf(stderr, "[calpair] written %s\n", out_path);

    p2_upm_close(model);
    for (int i = 0; i < 3; ++i) {
        if (sig[i]) aio_hips_close(sig[i]);
        if (sup[i]) aio_hips_close(sup[i]);
    }
    aio_hips_close(mos_sig);
    aio_hips_close(mos_sup);
    return 0;
}
