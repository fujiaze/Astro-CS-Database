// ============================================================================
// p1stardet_node_gate_test.cpp — STARDET-01 节点级门：
//   star-psf 节点的星表引导检测（权威路径）fail-closed 与"不得静默降级"
// ----------------------------------------------------------------------------
// 规范锚: ASTROCS_DESIGN.md §4.2/§2.1 + docs/plugins/algorithms_phase1/
//   03_star_detection.md §4 —— 权威路径 = 星表位置逆投影 + 只对该位置拟合；
//   全图盲检测 = 诊断/初值路径（保留，但不再是权威路径）。
// 被测面: p1_op_star_psf_precise_json（= p1_op_star_psf_impl(..., n_fit_limit=0)，
//   lib/infrastructure/scheduler/src/module_adapters.cpp）。
//
// 判据（可红可绿；"红" = 节点必须失败，"绿" = 节点必须成功）:
//   N1  catalog_guided 且未配置 gaia_data_dir            → 必红（拒绝降级）
//   N2  catalog_guided 且 gaia_data_dir = 空目录(0 .xpsd) → 必红（静默部分装载事故同族）
//   N3  catalog_guided 且 gaia_data_dir 含坏 .xpsd        → 必红（装载失败不得吞）
//   N4  catalog_guided + 可用星表但近似 WCS 不可解析      → 必红
//   N5  max_stars 越出合同域 [2万,5万]                    → 必红（禁静默夹取）
//   N6  blind_diagnostic（显式声明）                      → 必绿, detection_authoritative=false
//   N7  auto 且无星表                                      → 必绿但 detection_mode=blind_diagnostic
//                                                          且 degraded_reason 非空（留痕, 非静默）
//   N8  真实星表 + 星表位置合成的帧 + 显式近似 WCS        → 必绿, detection_mode=catalog_guided
//                                                          authoritative=true 且 n_detected>0
//   N9  同一节点配置 + 星**不在**星表位置上的帧           → 必红（0 引导拟合 ⇒ 拒绝回退盲检测）
//   N9b 同一帧 + blind_diagnostic                          → 必绿且 n_detected>0
//       （N9/N9b 成对 ⇒ 证明"权威路径退化为全图盲检测"会被判红, 判据非退化）
//   N8/N9/N9b 需要 gaia 数据集（外部只读数据集 gaia/GaiaDR3）；
//   缺失时打印 SKIP 并在结尾汇总（可用 ASTROCS_STARDET_REQUIRE_GAIA=1 把 SKIP 判红）。
//
// 节点调用不启动 astrocs CLI（进程内直调测试钩子）。
// ctest 目标名: p1stardet_node_gate
// ============================================================================

#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"

#include "gaia_client.h"
#include "wcs_tan.h"

#include "p1sess_fixtures.hpp"   // 手写最小 FITS writer（不调生产 symbol）

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

using json = nlohmann::json;

namespace {

int g_failures = 0;
int g_skips = 0;
#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (cond) { std::printf("  [PASS] %s\n", msg); }           \
        else { std::printf("  [FAIL] %s\n", msg); ++g_failures; }  \
    } while (0)

namespace fs = std::filesystem;

constexpr int kW = 512, kH = 512;

struct NodeRun {
    bool ok = false;
    std::string err;
    json man;
};

NodeRun run_node(const json& cfg) {
    NodeRun r;
    std::string man_json;
    auto rc = astrocs::core::p1_op_star_psf_precise_json(cfg.dump(), &man_json);
    r.ok = rc.ok();
    if (!r.ok) {
        r.err = rc.error().message();
    } else {
        try { r.man = json::parse(man_json); } catch (...) { r.man = json::object(); }
    }
    return r;
}

bool contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

// ── 合成帧：星点位置由调用方给出 ──────────────────────────────────────────
struct FieldSpec {
    std::vector<double> cx, cy;
    std::vector<double> amp;
    float bg = 500.0f;
    double sigma = 1.5;      // px
};
struct FieldCtx { const FieldSpec* f; };
float field_pixel(int i, void* user) {
    const FieldSpec* f = static_cast<FieldCtx*>(user)->f;
    const int x = i % kW, y = i / kW;
    double v = f->bg;
    for (std::size_t k = 0; k < f->cx.size(); ++k) {
        const double dx = (double)x + 0.5 - f->cx[k];
        const double dy = (double)y + 0.5 - f->cy[k];
        v += f->amp[k] * std::exp(-(dx * dx + dy * dy) / (2.0 * f->sigma * f->sigma));
    }
    // 确定性物理型噪声（splitmix64 → Box-Muller；固定 seed）
    const double u1 = std::max(1e-12, p1sess::SplitMix64((std::uint64_t)i * 2654435761ull + 7ull).unit());
    const double u2 = p1sess::SplitMix64((std::uint64_t)i * 40503ull + 11ull).unit();
    v += std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2) * 4.0;
    return (float)v;
}

std::string g_tmp;

std::string write_frame(const std::string& name, const FieldSpec& f, double exptime) {
    const std::string path = g_tmp + "/" + name;
    FieldCtx ctx{&f};
    if (p1sess::write_fits_file(path, kW, kH, field_pixel, &ctx, 0, exptime) != 0) {
        std::fprintf(stderr, "write_fits_file failed: %s\n", path.c_str());
        std::exit(2);
    }
    return path;
}

json base_cfg(const std::string& light) {
    return json{{"input_lights", json::array({light})},
                {"output_dir", g_tmp + "/out"}};
}

// 占位取向先验（只为让 N2/N3 的失败点落在**星表装载面**而不是取向闸门）
json dummy_wcs() {
    const double s = 2.0 / 3600.0;
    return json{{"crval1", 15.0}, {"crval2", 20.0},
                {"cd11", -s}, {"cd12", 0.0}, {"cd21", 0.0}, {"cd22", -s}};
}

// gaia 数据集定位（外部只读数据集；缺失 ⇒ N8/N9 SKIP）
std::string gaia_dir() {
    const char* env = std::getenv("ASTROCS_GAIA_DIR");
    if (env && *env) return std::string(env);
#ifdef P1STARDET_REPO_ROOT
    return std::string(P1STARDET_REPO_ROOT) + "/gaia/GaiaDR3";
#else
    return std::string();
#endif
}

// ── N1/N2/N3/N5：星表装载 fail-closed ─────────────────────────────────────
void test_catalog_fail_closed() {
    std::printf("[N1] catalog_guided 未配置 gaia_data_dir\n");
    {
        json cfg = base_cfg(g_tmp + "/missing.fits");
        cfg["star_detection"] = json{{"mode", "catalog_guided"}};
        const NodeRun r = run_node(cfg);
        CHECK(!r.ok, "n1_red: 节点必须失败（权威路径不接受无星表）");
        CHECK(contains(r.err, "requires gaia_data_dir"),
              "n1_msg: 失败原因点名 gaia_data_dir 且拒绝降级");
    }
    std::printf("[N2] catalog_guided + 空目录（0 个 .xpsd）\n");
    {
        const std::string empty_dir = g_tmp + "/gaia_empty";
        fs::create_directories(empty_dir);
        json cfg = base_cfg(g_tmp + "/missing.fits");
        cfg["star_detection"] = json{{"mode", "catalog_guided"},
                                     {"gaia_data_dir", empty_dir},
                                     {"approx_wcs", dummy_wcs()}};
        const NodeRun r = run_node(cfg);
        CHECK(!r.ok, "n2_red: 0 个 .xpsd 的目录必须失败（不得静默返回成功）");
        CHECK(contains(r.err, "gaia catalog is empty"),
              "n2_msg: 失败原因点名空星表");
    }
    std::printf("[N3] catalog_guided + 坏 .xpsd\n");
    {
        const std::string bad_dir = g_tmp + "/gaia_bad";
        fs::create_directories(bad_dir);
        std::ofstream(bad_dir + "/gdr3-1.0.0-01.xpsd", std::ios::binary)
            << "not-an-xpsd-file";
        json cfg = base_cfg(g_tmp + "/missing.fits");
        cfg["star_detection"] = json{{"mode", "catalog_guided"},
                                     {"gaia_data_dir", bad_dir},
                                     {"approx_wcs", dummy_wcs()}};
        const NodeRun r = run_node(cfg);
        CHECK(!r.ok, "n3_red: 坏 shard 必须失败（装载失败不得吞）");
        CHECK(contains(r.err, "gaia_client_create failed") ||
              contains(r.err, "gaia catalog is incomplete") ||
              contains(r.err, "gaia catalog is empty"),
              "n3_msg: 失败原因来自星表装载面");
    }
    std::printf("[N5] max_stars 越出合同域\n");
    {
        json cfg = base_cfg(g_tmp + "/missing.fits");
        cfg["star_detection"] = json{{"mode", "blind_diagnostic"}, {"max_stars", 1000}};
        const NodeRun r = run_node(cfg);
        CHECK(!r.ok, "n5_red_low: max_stars=1000 越界必红");
        CHECK(contains(r.err, "outside contract domain"), "n5_msg: 点名合同域");
        json cfg2 = base_cfg(g_tmp + "/missing.fits");
        cfg2["star_detection"] = json{{"mode", "blind_diagnostic"}, {"max_stars", 100000}};
        const NodeRun r2 = run_node(cfg2);
        CHECK(!r2.ok, "n5_red_high: max_stars=100000 越界必红");
    }
}

// ── N6/N7：显式声明与留痕 ─────────────────────────────────────────────────
void test_no_silent_degradation(const std::string& light) {
    std::printf("[N6] blind_diagnostic（显式声明的非权威路径）\n");
    {
        json cfg = base_cfg(light);
        cfg["star_detection"] = json{{"mode", "blind_diagnostic"}};
        const NodeRun r = run_node(cfg);
        CHECK(r.ok, "n6_green: 显式盲检测路径可用（保留, 未删除）");
        if (r.ok) {
            CHECK(r.man.value("detection_mode", std::string()) == "blind_diagnostic",
                  "n6_mode: manifest detection_mode=blind_diagnostic");
            CHECK(r.man.value("detection_authoritative", true) == false,
                  "n6_auth: manifest detection_authoritative=false（非权威如实登记）");
        }
    }
    std::printf("[N7] auto 且无星表 ⇒ 显式降级留痕（不是静默）\n");
    {
        json cfg = base_cfg(light);
        const NodeRun r = run_node(cfg);
        CHECK(r.ok, "n7_green: auto 无星表时仍可运行（但必须留痕）");
        if (r.ok) {
            CHECK(r.man.value("detection_mode", std::string()) == "blind_diagnostic",
                  "n7_mode: 降级后 detection_mode=blind_diagnostic");
            CHECK(r.man.value("detection_authoritative", true) == false,
                  "n7_auth: detection_authoritative=false");
            const std::string why = r.man.value("detection_degraded_reason", std::string());
            CHECK(!why.empty(), "n7_reason: degraded_reason 非空（降级必须可审计）");
        }
    }
}

// ── N8/N9：真实星表 + 显式近似 WCS（需要 gaia 数据集）─────────────────────
void test_guided_positive_and_degradation(const std::string& gdir) {
    const double ra0 = 15.0, dec0 = 20.0;      // 任意天区（星表覆盖全球）
    const double s0_arcsec = 2.0;              // ″/px
    const double s_deg = s0_arcsec / 3600.0;
    const double rot = 35.0 * M_PI / 180.0;    // 非平凡取向（检验 CD 传递）
    astrocs::phase1::WcsTan wcs;
    wcs.crpix1 = 0.5 * kW + 0.5;
    wcs.crpix2 = 0.5 * kH + 0.5;
    wcs.crval1 = ra0;
    wcs.crval2 = dec0;
    wcs.cd11 = std::cos(rot) * (-s_deg);
    wcs.cd12 = -std::sin(rot) * (-s_deg);
    wcs.cd21 = std::sin(rot) * (-s_deg);
    wcs.cd22 = std::cos(rot) * (-s_deg);

    // 星表查询（与节点同一 API 口径: mag_high = 21）
    GaiaClient* g = gaia_client_create(gdir.c_str());
    if (!g) {
        std::printf("  [SKIP] n8/n9: gaia_client_create failed (%s)\n", gdir.c_str());
        ++g_skips;
        return;
    }
    const double fov_diag_deg = std::sqrt((double)kW * kW + (double)kH * kH) * s0_arcsec / 3600.0;
    const double radius = fov_diag_deg * 0.55;
    double *ras = nullptr, *decs = nullptr;
    float* mags = nullptr;
    int n_cat = 0;
    const int qrc = gaia_client_cone_search_for_solver(g, ra0, dec0, radius, 21.0,
                                                       &ras, &decs, &mags, &n_cat);
    if (qrc != 0 || n_cat <= 0) {
        std::printf("  [SKIP] n8/n9: cone search empty (rc=%d n=%d)\n", qrc, n_cat);
        if (ras) free(ras);
        if (decs) free(decs);
        if (mags) free(mags);
        gaia_client_destroy(g);
        ++g_skips;
        return;
    }
    // 投影到像素域；取帧内星（边界 ≥4px）
    FieldSpec on_cat, off_cat;
    std::vector<double> all_x, all_y;   // 帧内**全部**星表位置（含不合成者）
    for (int i = 0; i < n_cat; ++i) {
        double x = 0.0, y = 0.0;
        wcs.sky2pix((double)ras[i], (double)decs[i], &x, &y);
        const double xi = x - 1.0, yi = y - 1.0;
        if (!std::isfinite(xi) || !std::isfinite(yi)) continue;
        if (xi < 4.0 || yi < 4.0 || xi > kW - 5.0 || yi > kH - 5.0) continue;
        all_x.push_back(xi);
        all_y.push_back(yi);
        const double m = (double)mags[i];
        if (!(m > 4.0) || !(m < 19.0)) continue;
        const double amp = 3000.0 * std::pow(10.0, -0.4 * (m - 12.0));
        if (amp < 30.0) continue;
        on_cat.cx.push_back(xi);
        on_cat.cy.push_back(yi);
        on_cat.amp.push_back(amp);
    }
    // N9 的"非星表域"帧: 星点放在**距任一帧内星表位置 ≥ 25px** 的网格点上
    // （拟合盒 R≈12 ⇒ 星表域内无任何星点 ⇒ 引导拟合必然 0 星）。
    for (double gy = 24.0; gy < kH - 24.0 && off_cat.cx.size() < 40; gy += 16.0) {
        for (double gx = 24.0; gx < kW - 24.0 && off_cat.cx.size() < 40; gx += 16.0) {
            double dmin = 1e30;
            for (std::size_t k = 0; k < all_x.size(); ++k) {
                dmin = std::min(dmin, std::hypot(gx - all_x[k], gy - all_y[k]));
            }
            if (dmin < 25.0) continue;
            off_cat.cx.push_back(gx);
            off_cat.cy.push_back(gy);
            off_cat.amp.push_back(3000.0);
        }
    }
    std::printf("      off-catalog grid points=%d (all in-frame catalog positions=%d)\n",
                (int)off_cat.cx.size(), (int)all_x.size());
    free(ras); free(decs); free(mags);
    gaia_client_destroy(g);
    std::printf("      catalog_in_cone=%d projected_in_frame=%d\n", n_cat,
                (int)on_cat.cx.size());
    if (on_cat.cx.size() < 20) {
        std::printf("  [SKIP] n8/n9: too few projected stars (%d)\n", (int)on_cat.cx.size());
        ++g_skips;
        return;
    }
    const std::string f_on = write_frame("oncat.fits", on_cat, 300.0);
    const std::string f_off = write_frame("offcat.fits", off_cat, 300.0);

    auto guided_cfg = [&](const std::string& light) {
        json cfg = base_cfg(light);
        cfg["star_detection"] = json{
            {"mode", "catalog_guided"},
            {"gaia_data_dir", gdir},
            {"max_stars", 20000},
            {"approx_wcs", json{{"crval1", wcs.crval1}, {"crval2", wcs.crval2},
                                {"cd11", wcs.cd11}, {"cd12", wcs.cd12},
                                {"cd21", wcs.cd21}, {"cd22", wcs.cd22}}}};
        cfg["wcs"] = json{{"init_source", "config"},
                          {"ra0", ra0}, {"dec0", dec0},
                          {"focal_length_mm", 206.265 / s0_arcsec},
                          {"pixel_size_um", 1.0}};
        return cfg;
    };

    std::printf("[N8] 权威路径正例（帧内星点 = 星表逆投影位置）\n");
    {
        const NodeRun r = run_node(guided_cfg(f_on));
        CHECK(r.ok, "n8_green: catalog_guided 在星表域上成功");
        if (r.ok) {
            CHECK(r.man.value("detection_mode", std::string()) == "catalog_guided",
                  "n8_mode: manifest detection_mode=catalog_guided");
            CHECK(r.man.value("detection_authoritative", false) == true,
                  "n8_auth: manifest detection_authoritative=true");
            int64_t n_det = 0;
            double rate = -1.0;
            std::ifstream in(r.man.value("sources_artifact", std::string()));
            if (in) {
                json cat;
                try { in >> cat; } catch (...) {}
                for (const auto& fr : cat.value("frames", json::array())) {
                    n_det += fr.value("n_detected", (int64_t)0);
                    if (fr.contains("guided")) {
                        rate = fr["guided"].value("fit_success_rate", -1.0);
                    }
                }
            }
            std::printf("      n_detected=%lld fit_success_rate=%.4f\n", (long long)n_det, rate);
            CHECK(n_det > 0, "n8_detected: 引导路径产出 >0 星");
        }
    }
    std::printf("[N9] 权威路径退化负例（帧内星点**不在**星表域上）\n");
    {
        const NodeRun r = run_node(guided_cfg(f_off));
        CHECK(!r.ok, "n9_red: 0 引导拟合必须 fail-closed（不得回退全图盲检测冒充成功）");
        CHECK(contains(r.err, "catalog_guided") && contains(r.err, "0/"),
              "n9_msg: 失败原因点名权威路径 0/N 且拒绝回退");
        json cfg = base_cfg(f_off);
        cfg["star_detection"] = json{{"mode", "blind_diagnostic"}};
        const NodeRun rb = run_node(cfg);
        CHECK(rb.ok, "n9b_green: 同一帧在盲检测路径上成功（判据非退化: 红/绿成对）");
        if (rb.ok) {
            int64_t n_det = 0;
            std::ifstream in(rb.man.value("sources_artifact", std::string()));
            if (in) {
                json cat;
                try { in >> cat; } catch (...) {}
                for (const auto& fr : cat.value("frames", json::array()))
                    n_det += fr.value("n_detected", (int64_t)0);
            }
            std::printf("      blind n_detected=%lld\n", (long long)n_det);
            CHECK(n_det > 0, "n9b_detected: 盲检测在同一帧上确实有源（证明 n9 的红不是恒红）");
        }
    }
}

// ── R1：真实数据（testdata 真实帧）权威路径 vs 盲检测对照 ──────────────────
// 帧: testdata/NGC55_T3_flying_dutchman/lights/
//     NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts
//     （4096², EXPTIME=600s, FOCALLEN=1877mm, XPIXSZ=9μm ⇒ s0=0.9890″/px）
// 近似 WCS 由**测试夹具**给出（该帧自身的解算 WCS 头：CRVAL/CRPIX/CD，
// TAN+SIP 的线性部分）——节点本身不读帧头 WCS（P9 裁定），此处模拟
// "来自我们自己的已解出产物"的近似 WCS 输入。
void test_real_frame(const std::string& gdir) {
#ifdef P1STARDET_REPO_ROOT
    const std::string fits = std::string(P1STARDET_REPO_ROOT) +
        "/testdata/NGC55_T3_flying_dutchman/lights/"
        "NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts";
#else
    const std::string fits;
#endif
    if (fits.empty() || !fs::exists(fits)) {
        std::printf("  [SKIP] R1: testdata 真实帧不可用\n");
        ++g_skips;
        return;
    }
    // 帧头 WCS（线性部分; SIP 未建模 ⇒ 角隅畸变是已知残差来源）
    const double crval1 = 3.7456807247193074;
    const double crval2 = -39.19679247759275;
    const double cd11 = -2.7378847720726e-06, cd12 = 0.000266298185535256;
    const double cd21 = -0.00026621612068604, cd22 = -2.7552950255393e-06;
    // orient: 0 = 显式 CD（权威取向先验）；1 = 无取向先验（只有指向 + 板尺度）；
    //         2 = rotation_deg + parity 路线（等价于显式 CD 的另一种给法）
    auto guided_cfg = [&](const std::string& light, int orient) {
        json cfg = base_cfg(light);
        json sd = json{{"mode", "catalog_guided"}, {"gaia_data_dir", gdir},
                       {"max_stars", 20000}};
        json wc = json{{"init_source", "config"}, {"ra0", crval1}, {"dec0", crval2},
                       {"focal_length_mm", 1877.0}, {"pixel_size_um", 9.0}};
        if (orient == 0) {
            sd["approx_wcs"] = json{{"crval1", crval1}, {"crval2", crval2},
                                    {"cd11", cd11}, {"cd12", cd12},
                                    {"cd21", cd21}, {"cd22", cd22}};
        } else if (orient == 2) {
            // 帧头 CD 第 1 列 ⇒ 相对"北向上/东向左"的像面旋转角; 板尺度按
            // |CD 第 1 列| 给（FOCALLEN/XPIXSZ 与之差 3%, 故用等效焦距）
            const double s_true = std::sqrt(cd11 * cd11 + cd21 * cd21);   // deg/px
            const double ct = -cd11 / s_true, st = -cd21 / s_true;
            const double rot_deg = std::atan2(st, ct) * 180.0 / M_PI;
            sd["rotation_deg"] = rot_deg;
            sd["parity"] = "pos";
            const double s0_true = s_true * 3600.0;
            wc["focal_length_mm"] = 206.265 * 9.0 / s0_true;
            std::printf("      rotation route: s0=%.4f\"/px rot=%.4f deg\n", s0_true, rot_deg);
        }
        cfg["star_detection"] = sd;
        cfg["wcs"] = wc;
        return cfg;
    };
    std::printf("[R1] 真实帧 NGC55 T3 Lum 600s（4096²）\n");
    {
        const NodeRun rg = run_node(guided_cfg(fits, 0));
        CHECK(rg.ok, "r1_guided_green: 真实帧权威路径成功");
        int64_t n_det = 0;
        double rate = -1.0, m_lim = -1.0;
        int64_t n_pred = 0, n_fit_failed = 0, n_rejected = 0, n_dropped = 0, n_in_frame = 0;
        if (rg.ok) {
            std::ifstream in(rg.man.value("sources_artifact", std::string()));
            json cat;
            if (in) { try { in >> cat; } catch (...) {} }
            for (const auto& fr : cat.value("frames", json::array())) {
                n_det += fr.value("n_detected", (int64_t)0);
                if (fr.contains("guided")) {
                    const json& g = fr["guided"];
                    rate = g.value("fit_success_rate", -1.0);
                    m_lim = g.value("limiting_mag", -1.0);
                    n_pred = g.value("n_predicted", (int64_t)0);
                    n_fit_failed = g.value("n_fit_failed", (int64_t)0);
                    n_rejected = g.value("n_rejected", (int64_t)0);
                    n_dropped = g.value("n_dropped", (int64_t)0);
                    n_in_frame = g.value("catalog_stars_in_frame", (int64_t)0);
                }
            }
        }
        std::printf("      guided: n_detected=%lld n_predicted=%lld (in_frame=%lld) "
                    "fit_success_rate=%.4f m_lim=%.3f fit_failed=%lld rejected=%lld "
                    "dropped=%lld\n",
                    (long long)n_det, (long long)n_pred, (long long)n_in_frame, rate, m_lim,
                    (long long)n_fit_failed, (long long)n_rejected, (long long)n_dropped);
        CHECK(n_det > 0, "r1_guided_detected: 真实帧引导路径产出 >0 星");
        json cfg = base_cfg(fits);
        cfg["star_detection"] = json{{"mode", "blind_diagnostic"}};
        const NodeRun rb = run_node(cfg);
        CHECK(rb.ok, "r1_blind_green: 真实帧盲检测路径成功（对照）");
        int64_t n_blind = 0;
        if (rb.ok) {
            std::ifstream in(rb.man.value("sources_artifact", std::string()));
            json cat;
            if (in) { try { in >> cat; } catch (...) {} }
            for (const auto& fr : cat.value("frames", json::array()))
                n_blind += fr.value("n_detected", (int64_t)0);
        }
        std::printf("      blind : n_detected=%lld\n", (long long)n_blind);
        CHECK(n_blind > n_det,
              "r1_contrast: 盲检测源数 >> 引导路径源数（全图连通域 vs 星表域）");
    }
    std::printf("[R2] 取向先验必需（无先验 ⇒ catalog_guided 必红）\n");
    {
        const NodeRun rn = run_node(guided_cfg(fits, 1));
        CHECK(!rn.ok, "r2_red: 无取向先验时 catalog_guided 必须 fail-closed");
        CHECK(contains(rn.err, "orientation prior"),
              "r2_msg: 失败原因点名 orientation prior 并给出两种给法");
        // auto 模式同条件 ⇒ 允许运行, 但必须显式降级留痕（不是静默假设取向）
        json cfg = guided_cfg(fits, 1);
        cfg["star_detection"]["mode"] = "auto";
        const NodeRun ra = run_node(cfg);
        CHECK(ra.ok, "r2_auto_green: auto 同条件仍可运行");
        if (ra.ok) {
            CHECK(ra.man.value("detection_mode", std::string()) == "blind_diagnostic" &&
                  ra.man.value("detection_authoritative", true) == false &&
                  !ra.man.value("detection_degraded_reason", std::string()).empty(),
                  "r2_auto_trace: auto 显式降级 + authoritative=false + 原因非空");
        }
    }
    std::printf("[R3] rotation_deg + parity 路线（等价取向先验）\n");
    {
        const NodeRun rr = run_node(guided_cfg(fits, 2));
        int64_t n_det = 0;
        if (rr.ok) {
            std::ifstream in(rr.man.value("sources_artifact", std::string()));
            json cat;
            if (in) { try { in >> cat; } catch (...) {} }
            for (const auto& fr : cat.value("frames", json::array()))
                n_det += fr.value("n_detected", (int64_t)0);
        }
        std::printf("      rotation route: ok=%d n_detected=%lld\n", (int)rr.ok, (long long)n_det);
        CHECK(rr.ok && n_det > 0,
              "r3_green: rotation_deg+parity 路线同样产出权威星表（取向先验等价）");
    }
}

}  // namespace

int main() {
    std::printf("P1STARDET-NODE-GATE (STARDET-01 星表引导检测节点级门)\n");
    g_tmp = (fs::temp_directory_path() / ("p1stardet_gate_" + std::to_string(
#ifdef _WIN32
        (long)_getpid()
#else
        (long)getpid()
#endif
        ))).string();
    fs::remove_all(g_tmp);
    fs::create_directories(g_tmp);
    fs::create_directories(g_tmp + "/out");

    // 一张可读的合成帧（盲检测路径用；固定 seed 物理型噪声）
    FieldSpec plain;
    for (int i = 0; i < 24; ++i) {
        plain.cx.push_back(30.0 + 20.0 * (i % 6) + 7.0 * ((i * 37) % 11) / 11.0);
        plain.cy.push_back(30.0 + 20.0 * (i / 6) + 7.0 * ((i * 53) % 13) / 13.0);
        plain.amp.push_back(1500.0 + 200.0 * (i % 5));
    }
    const std::string plain_path = write_frame("plain.fits", plain, 300.0);

    test_catalog_fail_closed();
    test_no_silent_degradation(plain_path);

    const std::string gdir = gaia_dir();
    const bool have_gaia = !gdir.empty() && fs::exists(gdir + "/gdr3-1.0.0-01.xpsd");
    if (have_gaia) {
        test_guided_positive_and_degradation(gdir);
        test_real_frame(gdir);
    } else {
        std::printf("  [SKIP] N8/N9/N9b: gaia 数据集不可用（ASTROCS_GAIA_DIR / "
                    "P1STARDET_REPO_ROOT/gaia/GaiaDR3）\n");
        g_skips += 3;
    }

    fs::remove_all(g_tmp);
    if (g_skips > 0) {
        std::printf("P1STARDET NODE GATE: %d check(s) skipped (external dataset)\n", g_skips);
        if (std::getenv("ASTROCS_STARDET_REQUIRE_GAIA")) {
            std::printf("P1STARDET NODE GATE FAIL (SKIP forced red by "
                        "ASTROCS_STARDET_REQUIRE_GAIA)\n");
            return 1;
        }
    }
    if (g_failures == 0) {
        std::printf("P1STARDET NODE GATE PASS\n");
        return 0;
    }
    std::printf("P1STARDET NODE GATE FAIL (%d check(s))\n", g_failures);
    return 1;
}
