// ============================================================================
// eng/tests/unit/frame_response_identity_test.cpp
// ----------------------------------------------------------------------------
// Issue #106 独立源码审核 P1-2 的**最小确定性反例**测试:
// 「一个 block 内两帧不同响应 ⇒ 两张 frame_science 必须有不同的响应/施加身份」。
//
// 审核给出的反例（必须被本测试证伪或证实）:
//   block 有两帧: 帧A parametric_narrowband(656.28nm,3nm)、帧B override
//   parametric_narrowband(500nm,5nm)（或不同 QE）⇒ 两帧拟合成功后
//   `p1_phot.photscale_fit` 的响应 id 应不同, 但此前两张 `frame_science` 从
//   **组级首帧**取同一响应 id ⇒ 身份被首帧污染。
//
// 本测试覆盖（审核要求的清单）:
//   ① 两帧**不同滤镜**（measured A vs measured B）;
//   ② 两帧**同滤镜不同 QE**;
//   ③ 参数化 + measured **混合**;
//   ④ 两帧**相同响应**（id 必须相同, 且与运行次数无关）;
//   ⑤ **帧失败**（该帧无逐帧记录）⇒ 具名不可用, 不得被别的帧代入;
//   ⑥ **历史 legacy 产物**（整份无逐帧记录）⇒ 组级回退, 且标记 group_legacy;
//   ⑦ `application_id` 对**响应变化**的独立性（同 k/同输入 ⇒ 响应变 ⇒ id 变）;
//   ⑧ `p1_phot.json` 逐帧身份 与 最终 `frame_science` 身份 的**一致性**。
//
// 判据来源（不复用被测实现）:
//   · 逐帧身份由**生产拟合入口** `fit_frame_photometry` 产出（真实 Gaia DR3 XP
//     光谱 + **真实**响应曲线库）; 不是手写的假 id。
//   · 落盘/解析用**生产同一份**实现（acsd::scheduler::photometry_frame_identity.h）。
//   · frame_science 身份用**生产**函数（acsd::phase1::precision:
//     photo_response_identity / photo_application_identity）。
//
// 运行: python3 eng/tests/unit/run_frame_response_identity_test.py
// ============================================================================

#include "frame_photometry_fit.h"
#include "photometry_frame_identity.h"   // 生产同一份逐帧身份实现（调度层）
#include "wcs_transform.h"

#include "acsd/normalize_precision.h"  // 生产身份函数
#include "canonical_double.h"         // 无损浮点规范串（共享层唯一实现）

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
#include <map>
#include <string>
#include <vector>

using Json = nlohmann::json;

namespace {

int g_fail = 0;
int g_checks = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++g_fail;
}

// 生产身份函数（唯一实现）
using acsd::phase1::precision::AppliedScaleIdentity;
using acsd::phase1::precision::photo_application_identity;
using acsd::phase1::precision::photo_response_identity;

using acsd::scheduler::FrameResponseIdentity;
using acsd::scheduler::GroupResponseIdentity;

// frame_science 的两个身份字段 = 生产构造方式（与 module_adapters 的
// p1_op_drizzle 调用点同口径）; 本测试只验证**逐帧取值**是否正确。
struct FrameScienceIdentities {
    std::string response_id;
    std::string application_id;
};

FrameScienceIdentities frame_science_identities(
    const acsd::scheduler::FrameResponseResolution& res,
    const std::string& frame_key, const std::string& input_sha,
    double k_photo) {
    FrameScienceIdentities out;
    AppliedScaleIdentity ident;
    ident.frame_key = frame_key;
    ident.input_source_sha256 = input_sha;
    ident.k_photo = k_photo;
    if (res.available) {
        ident.filter_key = res.identity.filter_key;
        ident.filter_content_sha256 = res.identity.filter_content_sha256;
        ident.qe_key = res.identity.qe_key;
        ident.qe_content_sha256 = res.identity.qe_content_sha256;
        ident.response_id = res.identity.response_id;
    }
    out.application_id = photo_application_identity(ident);
    out.response_id = res.available
        ? photo_response_identity(res.identity.filter_key,
                                  res.identity.filter_content_sha256,
                                  res.identity.qe_key,
                                  res.identity.qe_content_sha256,
                                  res.identity.response_id)
        : std::string();
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    std::string gaia_dir = "gaia/GaiaDR3SP";
    std::string qe_name = "KAF-16803";
    std::string filter_a = "Baader 7nm H-alpha";
    std::string filter_b = "Astrodon E-series R";
    double ra_center = 100.0;
    double dec_center = 0.0;
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
        else if (a == "--qe") qe_name = next("--qe");
        else if (a == "--filter-a") filter_a = next("--filter-a");
        else if (a == "--filter-b") filter_b = next("--filter-b");
        else if (a == "--ra") ra_center = std::stod(next("--ra"));
        else if (a == "--dec") dec_center = std::stod(next("--dec"));
    }

    std::printf("== Issue #106 P1: 逐帧响应身份传播测试 ==\n");

    // ══════════════════════════════════════════════════════════════════════
    // 0. 真实 Gaia DR3 XP 光谱 + 合成帧（生产同口径的 FOV/投影）
    // ══════════════════════════════════════════════════════════════════════
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
    pc::WcsTransform wcs(ra_center, dec_center, crpix1, crpix2,
                         cd11, cd12, cd21, cd22, 0, nullptr, nullptr, nullptr,
                         nullptr);
    std::vector<double> pcx, pcy, pfl;
    std::vector<int> pst;
    for (int i = 0; i < n_stars; ++i) {
        double x = 0.0, y = 0.0;
        wcs.skyToPixel(stars[i].ra, stars[i].dec, x, y);
        if (!(x >= 0.0 && x < W && y >= 0.0 && y < H)) continue;
        pcx.push_back(x);
        pcy.push_back(y);
        pfl.push_back(1.0);
        pst.push_back(0);
    }
    if (pcx.size() < 4) {
        std::fprintf(stderr, "too few stars inside the synthetic frame: %zu\n",
                     pcx.size());
        gaia_client_destroy(client);
        return 2;
    }
    std::vector<double> pixels(static_cast<size_t>(W) * H, 0.0);
    std::printf("== cone search: %d real XP stars; synthetic frame: %zu PSF stars\n",
                n_stars, pcx.size());

    // 一次拟合 = 一帧（生产入口）。参数: 响应模式 + 滤镜/QE。
    struct FitCase {
        std::string label;
        std::string key;            // 帧键（p1_frame_key 口径的净化名）
        acsd::photometry::ResponseMode mode;
        std::string curve_name;     // measured: 曲线库键
        std::string qe;             // QE 曲线库键
        double center_nm = 0.0;
        double fwhm_nm = 0.0;
    };
    std::vector<FitCase> cases = {
        {"A: measured " + filter_a + " + QE " + qe_name, "frameA",
         acsd::photometry::ResponseMode::kMeasuredCurve, filter_a, qe_name, 0.0, 0.0},
        {"B: measured " + filter_b + " + QE " + qe_name + " (different filter)",
         "frameB", acsd::photometry::ResponseMode::kMeasuredCurve, filter_b, qe_name,
         0.0, 0.0},
        {"C: measured " + filter_a + " + QE Ideal QE curve (same filter, other QE)",
         "frameC", acsd::photometry::ResponseMode::kMeasuredCurve, filter_a,
         "Ideal QE curve", 0.0, 0.0},
        {"D: parametric 500/5nm + QE " + qe_name + " (mixed parametric/measured)",
         "frameD", acsd::photometry::ResponseMode::kParametricNarrowband, "",
         qe_name, 500.0, 5.0},
        {"E: parametric 656.28/3nm + QE " + qe_name + " (other center/fwhm)",
         "frameE", acsd::photometry::ResponseMode::kParametricNarrowband, "",
         qe_name, 656.28, 3.0},
        {"A2: repeat of A (determinism)", "frameA2",
         acsd::photometry::ResponseMode::kMeasuredCurve, filter_a, qe_name, 0.0, 0.0},
        // F: **旧 measured 路径**（生产形式: req.filter_name + req.response.mode
        // == kUnspecified, 即 photometry.fit.filter / 块级 filter_passband /
        // 逐帧 override.filter 的声明形态）。
        // 第一轮 #106 的分发用**归一后**的 mode 取曲线名 ⇒ 旧路径读到空的
        // curve_name ⇒ 具名失败「filter_name required」; 第二轮由**生产 E2E 实测**
        // 发现并修复。此处固化回归判据（并要求两条声明形态给出**同一**响应身份）。
        {"F: legacy measured (req.filter_name) + QE " + qe_name, "frameF",
         acsd::photometry::ResponseMode::kUnspecified, filter_a, qe_name, 0.0, 0.0},
    };

    std::map<std::string, acsd::photometry::FramePhotFitResult> results;
    for (const FitCase& c : cases) {
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
        req.qe_name = c.qe;
        req.mag_min = 6.0;
        req.mag_max = 16.0;
        req.response.mode = c.mode;
        req.response.curve_name = c.curve_name;
        req.response.center_nm = c.center_nm;
        req.response.fwhm_nm = c.fwhm_nm;
        req.response.profile = acsd::photometry::kNarrowbandProfile;
        req.response.profile_version = acsd::photometry::kNarrowbandProfileVersion;
        req.response.source = "block_inherit";
        // 旧 measured 路径的**生产声明形态**: 曲线名走 req.filter_name
        // （req.response.mode 保持 kUnspecified）。
        if (c.mode == acsd::photometry::ResponseMode::kUnspecified) {
            req.filter_name = c.curve_name;
            req.response.curve_name.clear();
        }

        const acsd::photometry::FramePhotFitResult r =
            acsd::photometry::fit_frame_photometry(req);
        if (!r.fit_ok) {
            std::fprintf(stderr, "fit failed (%s): %s\n", c.label.c_str(),
                         r.error.c_str());
            gaia_client_destroy(client);
            return 3;
        }
        std::printf("== %s: k=%.6e n=%d response_id=%s synthesis_id=%s\n",
                    c.label.c_str(), r.k_photo, r.n_matched,
                    r.response.response_id.c_str(), r.response.synthesis_id.c_str());
        results[c.key] = r;
    }
    free(stars);
    free(spectra);
    gaia_client_destroy(client);

    const auto& rA = results["frameA"];
    const auto& rB = results["frameB"];
    const auto& rC = results["frameC"];
    const auto& rD = results["frameD"];
    const auto& rE = results["frameE"];
    const auto& rA2 = results["frameA2"];
    const auto& rF = results["frameF"];

    // ══════════════════════════════════════════════════════════════════════
    // 0b. 旧 measured 路径的**回归判据**（#106 第一轮引入、第二轮修复）
    //     声明形态不同（req.filter_name vs 新对象 measured_curve）但指向同一条
    //     曲线 ⇒ **同一**响应身份; 且旧形态必须**真的跑通**（第一轮实测失败）。
    // ══════════════════════════════════════════════════════════════════════
    check(rF.fit_ok,
          "legacy measured path (req.filter_name) still produces a fitted scale");
    check(rF.response.response_id == rA.response.response_id,
          "legacy filter_name and new measured_curve declaration of the same curve"
          " give the SAME response_id");
    check(rF.k_photo == rA.k_photo,
          "legacy and new declaration forms give the same k_photo (same curve)");

    // ══════════════════════════════════════════════════════════════════════
    // 1. 生产拟合入口给出的**逐帧**响应身份互不相同（不同滤镜/不同 QE/混合）
    // ══════════════════════════════════════════════════════════════════════
    check(rA.response.response_id != rB.response.response_id,
          "different filter curves -> different per-frame response_id");
    check(rA.response.response_id != rC.response.response_id,
          "same filter, different QE -> different per-frame response_id");
    check(rA.response.response_id != rD.response.response_id,
          "measured vs parametric (mixed) -> different per-frame response_id");
    check(rD.response.response_id != rE.response.response_id,
          "different parametric center/FWHM -> different per-frame response_id");
    check(rA.response.response_id == rA2.response.response_id,
          "same response in two runs -> identical response_id (deterministic)");
    check(rA.response.synthesis_id != rA.response.response_id,
          "physical response id and synthesis model id are distinct identities");
    check(!rA.response.synthesis_id.empty() && !rD.response.synthesis_id.empty(),
          "synthesis model id is published for both measured and parametric");

    // ══════════════════════════════════════════════════════════════════════
    // 2. 逐帧记录落盘 → 解析（生产同一份实现）→ 身份（生产函数）
    // ══════════════════════════════════════════════════════════════════════
    // 组级摘要 = 首帧（frameA）的身份（与光度节点同口径: 首个拟合成功帧）。
    std::map<std::string, FrameResponseIdentity> per_frame;
    Json photscale_fit = Json::object();
    for (const auto& kv : results) {
        const auto& r = kv.second;
        FrameResponseIdentity fri = acsd::scheduler::frame_response_identity_of(
            r.response, r.filter_curve_source, r.qe_curve_source);
        per_frame[kv.first] = fri;
        Json entry = Json::object();
        entry["k_photo"] = r.k_photo;
        entry["n_matched"] = r.n_matched;
        entry["fitted"] = true;
        entry["response"] = acsd::scheduler::frame_response_identity_json(fri);
        photscale_fit[kv.first] = entry;
    }
    Json p1_phot = Json::object();
    p1_phot["schema"] = "DATA-P1-PHOTPROV-001";
    p1_phot["photscale_fit"] = photscale_fit;
    {
        // 组级摘要块（首帧）: 与光度节点同口径 + scope 标记
        Json pbi = Json::object();
        pbi["resolved_library_key"] = rA.filter_key;
        pbi["curve_source"] = rA.filter_curve_source;
        pbi["qe"] = Json{{"resolved_library_key", rA.qe_key},
                         {"curve_source", rA.qe_curve_source}};
        Json gresp = acsd::scheduler::frame_response_identity_json(per_frame["frameA"]);
        gresp["scope"] = "group_summary (first successful frame);"
                         " per-frame truth = photscale_fit[frame].response";
        pbi["response"] = gresp;
        p1_phot["passband_identity"] = pbi;
    }

    // 解析（生产实现）: 收集 + 解析规则
    const auto collected =
        acsd::scheduler::collect_frame_response_identities(p1_phot);
    const GroupResponseIdentity group =
        acsd::scheduler::parse_group_response_identity(p1_phot);
    check(collected.size() == results.size(),
          "every fitted frame yields a per-frame response identity record");
    check(group.present && group.response_id == rA.response.response_id,
          "group summary = first successful frame (summary only)");

    const std::string input_sha = std::string(64, 'a');
    const double k = 1.524215866973e-17;

    auto resolve = [&](const std::string& key) {
        return acsd::scheduler::resolve_frame_response_identity(collected, group, key);
    };

    // ① 两帧**不同滤镜** ⇒ frame_science 身份不同
    {
        const auto ra = resolve("frameA");
        const auto rb = resolve("frameB");
        const FrameScienceIdentities ia = frame_science_identities(ra, "frameA", input_sha, k);
        const FrameScienceIdentities ib = frame_science_identities(rb, "frameB", input_sha, k);
        std::printf("   A response_id=%s\n   B response_id=%s\n",
                    ia.response_id.c_str(), ib.response_id.c_str());
        check(ra.source == "per_frame" && rb.source == "per_frame",
              "both frames resolve via their own per-frame record");
        check(ia.response_id != ib.response_id,
              "two frames with different filters get different frame_science.response_id");
        check(ia.application_id != ib.application_id,
              "two frames with different filters get different frame_science.application_id");
    }
    // ② 两帧**同滤镜不同 QE** ⇒ 身份不同
    {
        const auto ra = resolve("frameA");
        const auto rc2 = resolve("frameC");
        const FrameScienceIdentities ia = frame_science_identities(ra, "frameA", input_sha, k);
        const FrameScienceIdentities ic = frame_science_identities(rc2, "frameC", input_sha, k);
        std::printf("   C response_id=%s\n", ic.response_id.c_str());
        check(ia.response_id != ic.response_id,
              "same filter but different QE -> different frame_science.response_id");
        check(ia.application_id != ic.application_id,
              "same filter but different QE -> different frame_science.application_id");
    }
    // ③ 参数化 + measured 混合 ⇒ 身份不同
    {
        const auto rd = resolve("frameD");
        const FrameScienceIdentities id = frame_science_identities(rd, "frameD", input_sha, k);
        const FrameScienceIdentities ia = frame_science_identities(resolve("frameA"), "frameA", input_sha, k);
        std::printf("   D response_id=%s\n", id.response_id.c_str());
        check(id.response_id != ia.response_id,
              "parametric and measured frames -> different frame_science.response_id");
        check(id.application_id != ia.application_id,
              "parametric and measured frames -> different frame_science.application_id");
    }
    // ④ 两帧**相同响应** ⇒ 响应身份相同; 施加身份按**帧 occurrence** 区分
    //    （规范串含 frame_key: 同一施加事实发生在不同帧 ⇒ 不同施加身份; 这是
    //     设计的逐帧语义, 不是缺陷）。
    {
        const auto ra = resolve("frameA");
        const auto ra2 = resolve("frameA2");
        const FrameScienceIdentities ia = frame_science_identities(ra, "frameA", input_sha, k);
        const FrameScienceIdentities ib = frame_science_identities(ra2, "frameA2", input_sha, k);
        const FrameScienceIdentities ia_again =
            frame_science_identities(resolve("frameA"), "frameA", input_sha, k);
        check(ia.response_id == ib.response_id,
              "two frames with the same response share the same response_id");
        check(ia.application_id == ia_again.application_id,
              "same frame + same response + same k/input -> identical application_id");
        check(ia.application_id != ib.application_id,
              "different frame occurrence -> different application_id (frame_key is part of it)");
    }
    // ⑤ 帧失败（该帧无逐帧记录）⇒ 具名不可用, **不**被别的帧代入
    {
        Json p_fail = p1_phot;
        Json fit2 = p_fail["photscale_fit"];
        fit2.erase("frameD");
        p_fail["photscale_fit"] = fit2;
        const auto coll2 =
            acsd::scheduler::collect_frame_response_identities(p_fail);
        const auto res =
            acsd::scheduler::resolve_frame_response_identity(coll2, group, "frameD");
        check(!res.available && res.source == "unavailable",
              "frame without a per-frame record -> named unavailable (not substituted)");
        check(res.reason.find("response_identity_unavailable") != std::string::npos,
              "unavailable reason is named (response_identity_unavailable)");
        const FrameScienceIdentities idf =
            frame_science_identities(res, "frameD", input_sha, k);
        check(idf.response_id.empty(),
              "unavailable frame -> frame_science.response_id stays empty (no borrowed id)");
        check(idf.application_id !=
                  frame_science_identities(resolve("frameA"), "frameA", input_sha, k)
                      .application_id,
              "unavailable frame -> application_id does not borrow another frame's response");
    }
    // ⑥ 历史 legacy 产物（整份无逐帧记录）⇒ 组级回退且标记 group_legacy
    {
        Json p_legacy = Json::object();
        p_legacy["schema"] = "DATA-P1-PHOTPROV-001";
        p_legacy["passband_identity"] = p1_phot["passband_identity"];
        const auto coll3 =
            acsd::scheduler::collect_frame_response_identities(p_legacy);
        check(coll3.empty(), "legacy artifact has no per-frame response records");
        const auto res =
            acsd::scheduler::resolve_frame_response_identity(coll3, group, "frameA");
        check(res.available && res.source == "group_legacy",
              "legacy artifact -> group-level fallback, marked group_legacy");
        check(!res.reason.empty(),
              "legacy fallback carries a named reason (not a silent substitution)");
    }
    // ⑦ application_id 对响应变化的独立性（同 k/同输入 ⇒ 响应变 ⇒ id 变）
    {
        FrameResponseIdentity mut = per_frame["frameA"];
        mut.response_id = per_frame["frameB"].response_id;
        mut.filter_key = per_frame["frameB"].filter_key;
        mut.filter_content_sha256 = per_frame["frameB"].filter_content_sha256;
        const auto res = acsd::scheduler::resolve_frame_response_identity(
            {{"frameA", mut}}, group, "frameA");
        const FrameScienceIdentities i1 =
            frame_science_identities(res, "frameA", input_sha, k);
        const FrameScienceIdentities i2 =
            frame_science_identities(resolve("frameA"), "frameA", input_sha, k);
        check(i1.application_id != i2.application_id,
              "application_id changes when only the response changes");
        check(i1.response_id != i2.response_id,
              "response_id changes when only the response changes");
    }
    // ⑧ p1_phot 逐帧身份 与 frame_science 身份 一致（逐帧贯通）
    {
        bool all_ok = true;
        for (const auto& kv : results) {
            const auto res = resolve(kv.first);
            if (!res.available) { all_ok = false; break; }
            if (res.identity.response_id != kv.second.response.response_id) {
                all_ok = false;
                break;
            }
            if (res.identity.synthesis_id != kv.second.response.synthesis_id) {
                all_ok = false;
                break;
            }
            if (res.identity.qe_key != kv.second.qe_key) { all_ok = false; break; }
            if (res.identity.filter_key != kv.second.filter_key) {
                all_ok = false;
                break;
            }
        }
        check(all_ok,
              "every frame's p1_phot identity == the identity carried into frame_science");
    }
    // ⑨ 身份是**内容**身份: 曲线文件字节摘要变化不改变身份（排版变化非物理变化）
    {
        FrameResponseIdentity mut = per_frame["frameA"];
        mut.filter_file_sha256 = std::string(64, 'e');
        mut.qe_file_sha256 = std::string(64, 'f');
        const auto res = acsd::scheduler::resolve_frame_response_identity(
            {{"frameA", mut}}, group, "frameA");
        const FrameScienceIdentities i1 =
            frame_science_identities(res, "frameA", input_sha, k);
        const FrameScienceIdentities i2 =
            frame_science_identities(resolve("frameA"), "frameA", input_sha, k);
        check(i1.response_id == i2.response_id && i1.application_id == i2.application_id,
              "identity is content-based: curve file byte digest does not enter it");
    }
    // ⑩ 无损浮点: 相邻 binary64 的中心波长不得共享响应 id
    {
        // 用生产拟合入口无法注入 nextafter 中心; 这里直接检查规范串口径:
        // 生产 id 由 frame_photometry_fit 的规范串派生, 其浮点写法为无损 hexfloat
        // （单一实现 canonical_double.h）。用同一规范串函数验证单射性。
        const double c1 = 656.28;
        const double c2 = std::nextafter(c1, 1e9);
        check(acsd::shared::canonical_double_hex(c1) !=
                  acsd::shared::canonical_double_hex(c2),
              "adjacent binary64 centers serialize differently (lossless canonical form)");
    }

    std::printf("\n== checks %d, failures %d\n", g_checks, g_fail);
    if (g_fail != 0) {
        std::fprintf(stderr, "FRAME RESPONSE IDENTITY TEST FAILED\n");
        return 1;
    }
    std::printf("FRAME RESPONSE IDENTITY TEST PASSED\n");
    return 0;
}
