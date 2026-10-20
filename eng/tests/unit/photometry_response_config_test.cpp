// ============================================================================
// eng/tests/unit/photometry_response_config_test.cpp
// ----------------------------------------------------------------------------
// Issue #106: 响应来源 one-of 的**站点解析**判据（唯一实现
// `lib/infrastructure/scheduler/src/photometry_response_config.h`）。
//
// 判据:
//   P1 参数化窄带: {mode=parametric_narrowband, center_nm, fwhm_nm[, profile,
//      profile_version]} ⇒ declared, mode=parametric_narrowband, legacy=false。
//   P2 measured 曲线: {mode=measured_curve, curve_name} ⇒ declared,
//      mode=measured_curve。
//   P3 旧字段: {filter: "<库键>"} ⇒ declared, legacy=true, mode=measured_curve。
//   P4 无声明: {} ⇒ declared=false。
//   N1 同站点 `response` + `filter` 并存 ⇒ 失败, 错误含 RESPONSE_SOURCE_CONFLICT。
//   N2 未知 mode ⇒ 失败, 具名。
//   N3 参数化缺 center_nm / fwhm_nm ⇒ 失败, 错误含 INVALID_PARAMETRIC_RESPONSE。
//   N4 fwhm<=0 / center_nm<=0 / 非有限 ⇒ 失败, INVALID_PARAMETRIC_RESPONSE。
//   N5 未知 profile / profile_version ⇒ 失败, INVALID_PARAMETRIC_RESPONSE。
//   N6 measured 缺 curve_name ⇒ 失败。
//   N7 profile_version 非整数 ⇒ 失败。
//
// 运行: python3 eng/tests/unit/run_photometry_response_config_test.py
// ============================================================================

#include "photometry_response_config.h"

#include <cmath>
#include <cstdio>
#include <string>

using acsd::scheduler::parse_response_declaration;
using acsd::scheduler::ResponseDeclaration;
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

bool has_prefix(const std::string& s, const std::string& p) {
    return s.rfind(p, 0) == 0;
}

}  // namespace

int main() {
    // ── P1 参数化窄带 ──────────────────────────────────────────────────
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"response", {{"mode", "parametric_narrowband"},
                                         {"center_nm", 656.28},
                                         {"fwhm_nm", 3.0},
                                         {"profile", "top_hat_approx"},
                                         {"profile_version", 1}}}};
        const bool ok = parse_response_declaration(site, &d, &err);
        check(ok && err.empty(), "P1 parametric site parses");
        check(d.declared && !d.legacy, "P1 declared and not legacy");
        check(d.mode == "parametric_narrowband", "P1 mode");
        check(std::fabs(d.center_nm - 656.28) < 1e-12, "P1 center_nm");
        check(std::fabs(d.fwhm_nm - 3.0) < 1e-12, "P1 fwhm_nm");
        check(d.profile == "top_hat_approx" && d.profile_version == 1, "P1 profile");
    }
    // P1b: profile/profile_version 可省略 ⇒ 取默认
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"response", {{"mode", "parametric_narrowband"},
                                         {"center_nm", 500.0},
                                         {"fwhm_nm", 5.0}}}};
        check(parse_response_declaration(site, &d, &err), "P1b minimal parametric parses");
        check(d.profile == "top_hat_approx" && d.profile_version == 1,
              "P1b default profile/version");
    }
    // ── P2 measured 曲线 ────────────────────────────────────────────────
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"response", {{"mode", "measured_curve"},
                                         {"curve_name", "Baader 7nm H-alpha"}}}};
        check(parse_response_declaration(site, &d, &err), "P2 measured site parses");
        check(d.declared && !d.legacy && d.mode == "measured_curve", "P2 mode");
        check(d.curve_name == "Baader 7nm H-alpha", "P2 curve_name");
    }
    // ── P3 旧字段 ──────────────────────────────────────────────────────
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"filter", "Baader_R"}};
        check(parse_response_declaration(site, &d, &err), "P3 legacy site parses");
        check(d.declared && d.legacy && d.mode == "measured_curve", "P3 legacy flag");
        check(d.curve_name == "Baader_R", "P3 legacy curve name");
    }
    // ── P4 无声明 ──────────────────────────────────────────────────────
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"qe_name", "KAF-16803"}};
        check(parse_response_declaration(site, &d, &err), "P4 empty site parses");
        check(!d.declared, "P4 not declared");
    }
    // ── N1 同站点并存 ⇒ RESPONSE_SOURCE_CONFLICT ───────────────────────
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"response", {{"mode", "parametric_narrowband"},
                                         {"center_nm", 656.28},
                                         {"fwhm_nm", 3.0}}},
                           {"filter", "Baader_R"}};
        const bool ok = parse_response_declaration(site, &d, &err);
        check(!ok, "N1 conflict rejected");
        check(has_prefix(err, "RESPONSE_SOURCE_CONFLICT"), "N1 error is RESPONSE_SOURCE_CONFLICT");
        check(!d.declared, "N1 output not declared (no silent choice)");
    }
    // ── N2 未知 mode ───────────────────────────────────────────────────
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"response", {{"mode", "gaussian"}}}};
        check(!parse_response_declaration(site, &d, &err), "N2 unknown mode rejected");
        check(err.find("unknown response mode") != std::string::npos, "N2 error text");
    }
    // ── N3 缺参数 ──────────────────────────────────────────────────────
    for (const char* what : {"center_nm", "fwhm_nm"}) {
        ResponseDeclaration d;
        std::string err;
        Json resp = {{"mode", "parametric_narrowband"}, {"center_nm", 656.28},
                     {"fwhm_nm", 3.0}};
        resp.erase(what);
        const Json site = {{"response", resp}};
        check(!parse_response_declaration(site, &d, &err),
              std::string("N3 missing ") + what + " rejected");
        check(has_prefix(err, "INVALID_PARAMETRIC_RESPONSE"),
              std::string("N3 missing ") + what + " error is INVALID_PARAMETRIC_RESPONSE");
    }
    // ── N4 非法数值 ────────────────────────────────────────────────────
    {
        const double bad_values[4][3] = {
            {656.28, 0.0, 1.0},     // fwhm = 0
            {656.28, -3.0, 1.0},    // fwhm < 0
            {0.0, 3.0, 1.0},        // center = 0
            {1.0, 3.0, 1.0},        // center - fwhm/2 <= 0
        };
        for (const auto& b : bad_values) {
            ResponseDeclaration d;
            std::string err;
            const Json site = {{"response", {{"mode", "parametric_narrowband"},
                                             {"center_nm", b[0]},
                                             {"fwhm_nm", b[1]}}}};
            const bool ok = parse_response_declaration(site, &d, &err);
            check(!ok, "N4 illegal numeric rejected");
            check(has_prefix(err, "INVALID_PARAMETRIC_RESPONSE"),
                  "N4 error is INVALID_PARAMETRIC_RESPONSE");
        }
    }
    // N4b: 非有限
    {
        ResponseDeclaration d;
        std::string err;
        Json site = {{"response", {{"mode", "parametric_narrowband"},
                                   {"center_nm", 656.28},
                                   {"fwhm_nm", 3.0}}}};
        site["response"]["center_nm"] = std::numeric_limits<double>::quiet_NaN();
        check(!parse_response_declaration(site, &d, &err), "N4b NaN center rejected");
        check(has_prefix(err, "INVALID_PARAMETRIC_RESPONSE"), "N4b error prefix");
    }
    // ── N5 未知 profile / version ──────────────────────────────────────
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"response", {{"mode", "parametric_narrowband"},
                                         {"center_nm", 656.28},
                                         {"fwhm_nm", 3.0},
                                         {"profile", "gauss_1nm"}}}};
        check(!parse_response_declaration(site, &d, &err), "N5 unknown profile rejected");
        check(has_prefix(err, "INVALID_PARAMETRIC_RESPONSE"), "N5 error prefix");
    }
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"response", {{"mode", "parametric_narrowband"},
                                         {"center_nm", 656.28},
                                         {"fwhm_nm", 3.0},
                                         {"profile_version", 2}}}};
        check(!parse_response_declaration(site, &d, &err), "N5b unknown version rejected");
    }
    // ── N6 measured 缺 curve_name ──────────────────────────────────────
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"response", {{"mode", "measured_curve"}}}};
        check(!parse_response_declaration(site, &d, &err), "N6 measured without curve_name rejected");
    }
    // ── N7 profile_version 非整数 ───────────────────────────────────────
    {
        ResponseDeclaration d;
        std::string err;
        const Json site = {{"response", {{"mode", "parametric_narrowband"},
                                         {"center_nm", 656.28},
                                         {"fwhm_nm", 3.0},
                                         {"profile_version", 1.5}}}};
        check(!parse_response_declaration(site, &d, &err), "N7 non-integer version rejected");
    }

    std::printf("== checks %d, failures %d\n", g_checks, g_fail);
    if (g_fail != 0) {
        std::fprintf(stderr, "PHOTOMETRY RESPONSE CONFIG TEST FAILED\n");
        return 1;
    }
    std::printf("PHOTOMETRY RESPONSE CONFIG TEST PASSED\n");
    return 0;
}
