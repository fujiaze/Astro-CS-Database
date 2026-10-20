// ============================================================================
// eng/tests/unit/narrowband_production_path_test.cpp
// ----------------------------------------------------------------------------
// Issue #106 (ACSD-CPP/C1) **生产路径**验证: 参数化窄带响应是否真的进入了
// `fit_frame_photometry` → `pc_calibrate_simple_with_gaia_f64_v2_qf` → F_syn
// 的生产计算链, 以及帧级响应身份是否随之改变。
//
// 本测试用**真实 Gaia DR3 XP 光谱**（仓内 gaia/GaiaDR3SP）与**真实响应曲线库**
// (config/response_curves) 构造一个合成帧:
//   1. 锥形搜索取真实 XP 光谱星; 用 TAN WCS 投影到合成帧的像元;
//   2. 合成 PSF 通量, 调 fit_frame_photometry（参数化 Hα 656.28/3nm + 真实 QE）;
//   3. 检查: 拟合**自洽**（把 F_instr 设成 F_syn·c ⇒ 残差应为常数 ⇒ 标度应为
//      10^(-log10 c) 且残差散度≈0）—— 这只有当生产 F_syn 与记录的 F_syn 同源时
//      才成立;
//   4. 同输入下再跑一次 **measured 等价矩形**（历史路径）, 逐星比较两条路径的
//      F_syn（量化 #43 在生产路径上的影响）;
//   5. 把逐星数据（XP 光谱字节 + 两条 F_syn + QE/矩形曲线）写成 JSON, 供
//      eng/tests/unit/narrowband_production_oracle.py 用 SciPy 独立复算。
//
// **不是**真实 FITS/HST 验收（真实数据节点 = #103）。
// 运行: python3 eng/tests/unit/run_narrowband_production_path_test.py
// ============================================================================

#include "frame_photometry_fit.h"
#include "wcs_transform.h"

#include "nlohmann/json.hpp"

extern "C" {
#include "gaia_client.h"
}

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

using Json = nlohmann::json;

namespace {

int g_fail = 0;
int g_checks = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_fail;
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    }
}

// 与生产 gaia_client 相同的星表标识构造（用于把逐星记录对回锥形搜索结果）:
double sep_deg(double ra1, double dec1, double ra2, double dec2) {
    const double d2r = M_PI / 180.0;
    const double s1 = std::sin(dec1 * d2r), s2 = std::sin(dec2 * d2r);
    const double c1 = std::cos(dec1 * d2r) * std::cos(dec2 * d2r);
    const double c2 = std::cos((ra1 - ra2) * d2r);
    return std::acos(std::max(-1.0, std::min(1.0, s1 * s2 + c1 * c2))) / d2r;
}

struct StarRow {
    int psf_index = -1;
    int64_t dr3sp_id = 0;
    double fsyn_parametric = 0.0;
    double fsyn_measured = 0.0;
    int status_parametric = 0;
    int status_measured = 0;
    // 真实 XP 光谱（供独立 Oracle 复算）
    std::vector<uint8_t> bytes;
    double flux_min = 0.0;
    double flux_mul = 0.0;
    double ra = 0.0;
    double dec = 0.0;
    double mag_g = 0.0;
};

}  // namespace

int main(int argc, char** argv) {
    std::string gaia_dir = "gaia/GaiaDR3SP";
    std::string out_path = "/tmp/narrowband_production_path.json";
    std::string qe_name = "KAF-16803";
    std::string rect_name = "ACSD-TEST-Ha-3nm-rectangle";
    double center_nm = 656.28;
    double fwhm_nm = 3.0;
    double ra_center = 100.0;   // deg（任意有足够恒星的方向）
    double dec_center = 0.0;    // deg
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--gaia") gaia_dir = next("--gaia");
        else if (a == "--out") out_path = next("--out");
        else if (a == "--qe") qe_name = next("--qe");
        else if (a == "--rect") rect_name = next("--rect");
        else if (a == "--ra") ra_center = std::stod(next("--ra"));
        else if (a == "--dec") dec_center = std::stod(next("--dec"));
        else if (a == "--center-nm") center_nm = std::stod(next("--center-nm"));
        else if (a == "--fwhm-nm") fwhm_nm = std::stod(next("--fwhm-nm"));
    }

    // ── 1. 真实 Gaia DR3 XP 光谱（锥形搜索）───────────────────────────────
    GaiaClient* client = gaia_client_create(gaia_dir.c_str());
    if (client == nullptr) {
        std::fprintf(stderr, "gaia_client_create failed: %s\n", gaia_dir.c_str());
        return 2;
    }
    int wl_start = 0, wl_step = 0, wl_count = 0;
    if (gaia_client_get_spectrum_params(client, &wl_start, &wl_step, &wl_count) != 1 ||
        wl_count <= 0) {
        std::fprintf(stderr, "gaia_client_get_spectrum_params failed\n");
        gaia_client_destroy(client);
        return 2;
    }
    // 与生产入口同口径的 FOV 半径（frame_photometry_fit 的公式）
    const int W = 1200, H = 1200;
    const double scale_deg_per_px = 2.0e-4;
    const double cd11 = -scale_deg_per_px, cd12 = 0.0;
    const double cd21 = 0.0, cd22 = scale_deg_per_px;
    const double crpix1 = W / 2.0 + 0.5, crpix2 = H / 2.0 + 0.5;
    double fov_radius_deg =
        scale_deg_per_px * std::sqrt(static_cast<double>(W) * W +
                                     static_cast<double>(H) * H) / 2.0 * 1.2;
    if (fov_radius_deg <= 0.0 || fov_radius_deg >= 30.0) {
        fov_radius_deg = std::min(std::max(fov_radius_deg, 1.0), 10.0);
    }
    GaiaSpectrumStar* stars = nullptr;
    uint8_t* spectra = nullptr;
    int n_stars = 0;
    const int rc = gaia_client_cone_search_with_spectrum(
        client, ra_center, dec_center, fov_radius_deg, 6.0, 16.0,
        &stars, &spectra, &n_stars);
    if (rc != 0 || n_stars <= 0 || stars == nullptr || spectra == nullptr) {
        std::fprintf(stderr, "cone search failed rc=%d n=%d\n", rc, n_stars);
        gaia_client_destroy(client);
        return 2;
    }
    std::printf("== cone search: %d real XP spectrum stars, %d XP points, "
                "fov_radius=%.5f deg\n", n_stars, wl_count, fov_radius_deg);

    // ── 2. 合成帧: 把真实星投影到像元, 作为 PSF 星 ───────────────────────
    pc::WcsTransform wcs(ra_center, dec_center, crpix1, crpix2,
                         cd11, cd12, cd21, cd22, 0, nullptr, nullptr, nullptr,
                         nullptr);
    std::vector<double> pcx, pcy, pfl;
    std::vector<int> pst;
    std::vector<int> star_index;   // PSF 行 → 锥形搜索星下标
    for (int i = 0; i < n_stars; ++i) {
        double x = 0.0, y = 0.0;
        wcs.skyToPixel(stars[i].ra, stars[i].dec, x, y);
        if (!(x >= 0.0 && x < W && y >= 0.0 && y < H)) continue;
        pcx.push_back(x);
        pcy.push_back(y);
        pfl.push_back(1.0);        // 占位; 第一遍用于取回生产 F_syn
        pst.push_back(0);          // PSF 拟合域有效
        star_index.push_back(i);
    }
    if (pcx.size() < 4) {
        std::fprintf(stderr, "too few stars inside the synthetic frame: %zu\n",
                     pcx.size());
        gaia_client_destroy(client);
        return 2;
    }
    std::printf("== synthetic frame: %d PSF stars placed\n", pcx.size());
    std::vector<double> pixels(static_cast<size_t>(W) * H, 0.0);

    // ── 3. 生产拟合入口（参数化窄带）─────────────────────────────────────
    acsd::photometry::FramePhotFitRequest req;
    req.pixels = pixels.data();
    req.width = W;
    req.height = H;
    req.psf_cx = pcx.data();
    req.psf_cy = pcy.data();
    req.psf_flux = pfl.data();
    req.psf_status = pst.data();
    req.n_psf = static_cast<int>(pcx.size());
    req.crval1 = ra_center; req.crval2 = dec_center;
    req.crpix1 = crpix1; req.crpix2 = crpix2;
    req.cd11 = cd11; req.cd12 = cd12; req.cd21 = cd21; req.cd22 = cd22;
    req.gaia_data_dir = gaia_dir;
    req.qe_name = qe_name;
    req.mag_min = 6.0;
    req.mag_max = 16.0;
    req.response.mode = acsd::photometry::ResponseMode::kParametricNarrowband;
    req.response.center_nm = center_nm;
    req.response.fwhm_nm = fwhm_nm;
    req.response.profile = acsd::photometry::kNarrowbandProfile;
    req.response.profile_version = acsd::photometry::kNarrowbandProfileVersion;
    req.response.source = "block_inherit";

    const acsd::photometry::FramePhotFitResult r1 =
        acsd::photometry::fit_frame_photometry(req);
    if (!r1.fit_ok) {
        std::fprintf(stderr, "parametric fit failed: %s (degraded=%s)\n",
                     r1.error.c_str(), r1.degraded_reason.c_str());
        gaia_client_destroy(client);
        return 3;
    }
    std::printf("== parametric fit: k_photo=%.6e n_matched=%d sigma=%.3e "
                "response_id=%s coverage=%s integration=%s\n",
                r1.k_photo, r1.n_matched, r1.sigma_residual_dex,
                r1.response.response_id.c_str(), r1.response.coverage.c_str(),
                r1.response.integration.c_str());
    check(r1.response.mode == "parametric_narrowband",
          "production response mode is parametric_narrowband");
    check(r1.response.integration == "segmented_gauss_legendre4_v1",
          "production integration is the segmented path");
    check(r1.response.coverage == "full", "production coverage is full");
    check(r1.response.knowledge == "parametric_approx",
          "production knowledge is parametric_approx (not measured)");
    check(!r1.response.response_id.empty(), "production response_id is set");
    check(r1.response.qe_key == qe_name, "production QE identity is recorded");
    check(r1.response.qe_content_sha256.size() == 64,
          "production QE ordered-content sha256 is present");
    check(r1.filter_key.empty(),
          "parametric path does NOT resolve a filter curve (no library entry needed)");

    // 逐星: 取回生产 F_syn
    std::vector<double> fsyn_param(pcx.size(), 0.0);
    std::vector<int> status_param(pcx.size(), 0);
    for (std::size_t i = 0; i < r1.stars.size() && i < pcx.size(); ++i) {
        fsyn_param[i] = r1.stars[i].reference_flux;
        status_param[i] = r1.stars[i].status;
    }
    int n_with_fsyn = 0;
    for (double v : fsyn_param) if (std::isfinite(v) && v > 0.0) ++n_with_fsyn;
    check(n_with_fsyn >= 3,
          "production produced F_syn for >=3 matched stars (" +
              std::to_string(n_with_fsyn) + ")");

    // ── 4. 自洽检验: F_instr := F_syn·c ⇒ 残差恒定 ⇒ 标度 = 10^(-log10 c) ──
    const double c_inj = 1.0e-3;
    std::vector<double> pfl2 = pfl;
    for (std::size_t i = 0; i < pfl2.size(); ++i) {
        if (std::isfinite(fsyn_param[i]) && fsyn_param[i] > 0.0) pfl2[i] = fsyn_param[i] * c_inj;
    }
    req.psf_flux = pfl2.data();
    const acsd::photometry::FramePhotFitResult r2 =
        acsd::photometry::fit_frame_photometry(req);
    if (!r2.fit_ok) {
        std::fprintf(stderr, "parametric self-consistency fit failed: %s\n",
                     r2.error.c_str());
        gaia_client_destroy(client);
        return 3;
    }
    const double scale_expect = 1.0 / c_inj;
    const double rel_scale = std::fabs(r2.k_photo - scale_expect) / scale_expect;
    std::printf("== self-consistency: injected c=%.3e ⇒ k_photo=%.6e (expect %.6e, "
                "rel=%.3e) sigma=%.3e\n", c_inj, r2.k_photo, scale_expect,
                rel_scale, r2.sigma_residual_dex);
    check(rel_scale < 1e-6,
          "injected F_instr = F_syn*c recovers k_photo = 1/c (rel<1e-6)");
    check(r2.sigma_residual_dex < 1e-6,
          "self-consistency residual scatter is ~0 (rel<1e-6)");

    // ── 5. 同输入的 measured 等价矩形（历史路径）──────────────────────────
    acsd::photometry::FramePhotFitRequest req_m = req;
    req_m.psf_flux = pfl.data();
    req_m.response.mode = acsd::photometry::ResponseMode::kMeasuredCurve;
    req_m.response.curve_name = rect_name;
    const acsd::photometry::FramePhotFitResult rm =
        acsd::photometry::fit_frame_photometry(req_m);
    if (!rm.fit_ok) {
        std::fprintf(stderr, "measured fit failed: %s\n", rm.error.c_str());
        gaia_client_destroy(client);
        return 3;
    }
    std::printf("== measured(equivalent rectangle) fit: k_photo=%.6e n_matched=%d "
                "response_id=%s integration=%s\n", rm.k_photo, rm.n_matched,
                rm.response.response_id.c_str(), rm.response.integration.c_str());
    check(rm.response.mode == "measured_curve", "measured path reports measured_curve");
    check(rm.response.integration == "simpson_xp_grid_v1",
          "measured path keeps the historical Simpson-on-XP-grid integration");
    check(rm.response.knowledge == "measured",
          "measured path knowledge is measured (≠ parametric_approx)");
    check(rm.response.response_id != r1.response.response_id,
          "parametric and measured response ids differ (not the same response)");
    std::vector<double> fsyn_meas(pcx.size(), 0.0);
    std::vector<int> status_meas(pcx.size(), 0);
    for (std::size_t i = 0; i < rm.stars.size() && i < pcx.size(); ++i) {
        fsyn_meas[i] = rm.stars[i].reference_flux;
        status_meas[i] = rm.stars[i].status;
    }

    // ── 6. 逐星数据导出（供独立 SciPy Oracle 复算）──────────────────────
    Json out;
    out["gaia_dir"] = gaia_dir;
    out["qe_name"] = qe_name;
    out["rect_name"] = rect_name;
    out["center_nm"] = center_nm;
    out["fwhm_nm"] = fwhm_nm;
    out["xp_wl_start"] = wl_start;
    out["xp_wl_step"] = wl_step;
    out["xp_wl_count"] = wl_count;
    out["k_photo_parametric"] = r1.k_photo;
    out["k_photo_measured"] = rm.k_photo;
    out["sigma_parametric"] = r1.sigma_residual_dex;
    out["response_id_parametric"] = r1.response.response_id;
    out["response_id_measured"] = rm.response.response_id;
    Json rows = Json::array();
    int n_rows = 0;
    for (std::size_t i = 0; i < pcx.size(); ++i) {
        if (!(std::isfinite(fsyn_param[i]) && fsyn_param[i] > 0.0)) continue;
        // 把 PSF 行对回锥形搜索星（位置最近者）
        const int si = star_index[i];
        StarRow row;
        row.psf_index = static_cast<int>(i);
        row.fsyn_parametric = fsyn_param[i];
        row.fsyn_measured = fsyn_meas[i];
        row.status_parametric = status_param[i];
        row.status_measured = status_meas[i];
        row.bytes.assign(spectra + static_cast<std::size_t>(si) * wl_count,
                         spectra + static_cast<std::size_t>(si + 1) * wl_count);
        row.flux_min = stars[si].flux_min;
        row.flux_mul = stars[si].flux_mul;
        row.ra = stars[si].ra;
        row.dec = stars[si].dec;
        row.mag_g = stars[si].magG;
        Json j;
        j["psf_index"] = row.psf_index;
        j["fsyn_parametric"] = row.fsyn_parametric;
        j["fsyn_measured"] = row.fsyn_measured;
        j["status_parametric"] = row.status_parametric;
        j["status_measured"] = row.status_measured;
        j["flux_min"] = row.flux_min;
        j["flux_mul"] = row.flux_mul;
        j["ra"] = row.ra;
        j["dec"] = row.dec;
        j["mag_g"] = row.mag_g;
        j["bytes"] = row.bytes;
        rows.push_back(j);
        ++n_rows;
    }
    out["stars"] = rows;
    {
        std::ofstream ofs(out_path, std::ios::binary);
        ofs << out.dump(1);
    }
    std::printf("== wrote %d per-star rows to %s\n", n_rows, out_path.c_str());
    check(n_rows >= 3, "at least 3 per-star rows exported for the oracle");

    free(stars);
    free(spectra);
    gaia_client_destroy(client);

    std::printf("== checks %d, failures %d\n", g_checks, g_fail);
    if (g_fail != 0) {
        std::fprintf(stderr, "NARROWBAND PRODUCTION PATH TEST FAILED\n");
        return 1;
    }
    std::printf("NARROWBAND PRODUCTION PATH TEST PASSED\n");
    return 0;
}
