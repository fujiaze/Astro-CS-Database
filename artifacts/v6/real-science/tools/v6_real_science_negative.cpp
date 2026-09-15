/* v6_real_science_negative.cpp — REAL-SCIENCE-001 负向/边界检查
 *
 * 用真实冻结库函数对畸形/越界输入做 fail-closed 检查，证明门能红。
 * 输出 JSONL: {"check":..,"expected_reject":..,"got_reject":..,"pass":bool}
 */
#include "astrocs/v6/information_weight.h"
#include "astrocs/v6/psf_information.h"
#include "astrocs/v6/psfsw.h"
#include "v6_runtime_contract.h"   // RUNTIME-CI-001 单源：CLI 模式路由（只读头）

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace astrocs::v6::p1psfw;

static int g_pass = 0, g_fail = 0;

static bool has_gate(const RecordValidation& v, const char* g) {
  for (const auto& f : v.findings) if (f.gate == g) return true;
  return false;
}

static void report_diff(const std::string& name, bool base_has, bool mut_has, const char* gate) {
  const bool pass = (!base_has) && mut_has;
  if (pass) ++g_pass; else ++g_fail;
  std::printf("{\"check\":\"%s\",\"gate\":\"%s\",\"base_has\":%s,\"mutant_has\":%s,\"pass\":%s}\n",
              name.c_str(), gate, base_has ? "true" : "false", mut_has ? "true" : "false",
              pass ? "true" : "false");
}

static void report_bool(const std::string& name, bool ok, const std::string& detail) {
  if (ok) ++g_pass; else ++g_fail;
  std::printf("{\"check\":\"%s\",\"ok\":%s,\"detail\":\"%s\",\"pass\":%s}\n",
              name.c_str(), ok ? "true" : "false", detail.c_str(), ok ? "true" : "false");
}

static void report(const std::string& name, bool rejected, const char* reason, bool expect_reject) {
  const bool pass = (rejected == expect_reject) || (expect_reject && rejected);
  if (pass) ++g_pass; else ++g_fail;
  std::printf("{\"check\":\"%s\",\"expect_reject\":%s,\"got_reject\":%s,\"reason\":\"%s\",\"pass\":%s}\n",
              name.c_str(), expect_reject ? "true" : "false", rejected ? "true" : "false",
              reason ? reason : "", pass ? "true" : "false");
}

int main() {
  std::vector<double> p_good = {0.1, 0.2, 0.4, 0.2, 0.1};        /* Sum=1 */
  std::vector<double> p_neg  = {0.1, -0.2, 0.7, 0.2, 0.2};       /* negative sample */
  std::vector<double> p_unnorm = {0.1, 0.2, 0.4, 0.2, 0.2};      /* Sum=1.1 */
  std::vector<double> sigma2_good(5, 4.0);
  std::vector<double> sigma2_bad(5, -1.0);
  std::vector<double> d(5, 10.0);

  {
    const PsfProfileStats s = psf_profile_stats(p_neg.data(), p_neg.size());
    report("psf_profile_reject_negative_sample", !s.ok, s.reject, true);
  }
  {
    const PsfProfileStats s = psf_profile_stats(p_unnorm.data(), p_unnorm.size());
    report("psf_profile_reject_not_normalized", !s.ok, s.reject, true);
  }
  {
    const PsfProfileStats s = psf_profile_stats(nullptr, 0);
    report("psf_profile_reject_null", !s.ok, s.reject, true);
  }
  {
    const PointEstimate e = w_info_diagonal(p_good.data(), p_good.size(), sigma2_bad.data(), d.data(), 1.0);
    report("w_info_reject_nonpositive_sigma2", !e.ok, e.reject, true);
  }
  {
    const PointEstimate e = w_info_diagonal(p_good.data(), p_good.size(), sigma2_good.data(), d.data(), 1.0);
    report("w_info_accept_valid_control", !e.ok, e.reject, false);
  }
  {
    std::vector<PointEstimate> none;
    const FluxEstimate f = combine_point_estimates(none);
    report("combine_reject_empty", !f.ok, f.reject, true);
  }
  {
    /* B=0 -> composite fail-closed (background_nonpositive_undefined_transform) */
    ComponentValues cv; cv.s = 10.0; cv.conc = 2.0; cv.n = 1.0; cv.b = 0.0;
    const CompositeResult c = compute_psfsw_weights({cv});
    report("psfsw_composite_reject_nonpositive_background", !c.ok, c.reject, true);
  }
  {
    ComponentValues cv; cv.s = 10.0; cv.conc = 2.0; cv.n = 1.0; cv.b = 3.0;
    const CompositeResult c = compute_psfsw_weights({cv});
    report("psfsw_composite_accept_valid_control", !c.ok, c.reject, false);
  }
  {
    FrameComponentInput ci; ci.a_nea = 2.0; ci.fhat = {10, 11};
    ci.background_robust_mean = 5.0; ci.a_ref = 2.0;   /* <3 stars */
    const PsfswFrameComponents c = extract_psfsw_components(ci);
    report("psfsw_reject_too_few_stars", !c.ok, c.reject, true);
  }
  {
    FrameComponentInput ci; ci.a_nea = -1.0; ci.fhat = {10, 11, 12};
    ci.background_robust_mean = 5.0; ci.a_ref = 2.0;
    const PsfswFrameComponents c = extract_psfsw_components(ci);
    report("psfsw_reject_nonpositive_anea", !c.ok, c.reject, true);
  }
  {
    std::vector<std::vector<double>> d2 = {{1, 2}, {1, 2, 3}};
    std::vector<std::vector<char>> v2 = {{1, 1}, {1, 1, 1}};
    std::vector<double> w = {1.0, 1.0};
    const CoaddResult c = conventional_coadd(d2, v2, w);
    report("coadd_reject_ragged", !c.ok, c.reject, true);
  }
  {
    std::vector<std::vector<double>> al = {{0.5, 0.5}, {0.5, 0.5}};
    std::vector<double> ci = {1.0};   /* wrong size */
    const CovariancePropagation c = propagate_covariance(al, ci);
    report("covariance_reject_wrong_size", !c.ok, c.reject, true);
  }
  {
    const std::vector<std::string>& modes = production_weight_modes();
    bool has3 = false, has_deferred = false;
    for (const auto& m : modes) {
      if (m == "point_information" || m == "surface_gls" || m == "psfsw_robust") has3 = true;
      if (m == "psf_snr_power") has_deferred = true;
    }
    report("production_modes_exclude_psf_snr_power", has3 && !has_deferred && !is_production_weight_mode("psf_snr_power"),
           "production set has 3 modes, psf_snr_power not production", true);
  }
  {
    const std::vector<std::string>& aliases = forbidden_weight_source_aliases();
    auto has = [&](const char* t) { for (const auto& a : aliases) if (a == t) return true; return false; };
    const bool ok = has("median_source_snr") && has("support") && has("coverage") && has("fwhm") && has("psfsw");
    report("forbidden_weight_aliases_present", ok, "median_source_snr/support/coverage/fwhm/psfsw", true);
  }
  {
    PsfswRecord rec;
    rec.common_star_set_id = "css"; rec.selection_function_id = "sf"; rec.independence_proof = "ext";
    rec.n_common = 5; rec.depth_gate_evaluated = true; rec.depth_gate_ok = true;
    rec.w_psfsw = {0.9, 1.1}; rec.combination_coefficient_ids = {"a", "b"};
    rec.effective_psf_id = "epsf"; rec.effective_psf_only_fwhm = false;
    rec.weight_sources = {"psfsw.signal", "psfsw.concentration", "psfsw.noise", "psfsw.background"};
    rec.components = {
        ComponentMeasure{}, ComponentMeasure{}, ComponentMeasure{}, ComponentMeasure{}};
    const char* names[4] = {"signal", "concentration", "noise", "background"};
    for (int i = 0; i < 4; ++i) { rec.components[i].name = names[i];
      rec.components[i].measurement_id = std::string("mid-") + names[i]; }
    rec.components[0].value = 10; rec.components[1].value = 2;
    rec.components[2].value = 1; rec.components[3].value = 3;
    const RecordValidation base = validate_psfsw_record(rec);
    report_diff("psfsw_record_g06_variance_from_weight", has_gate(base, "PSFSW-G06"),
                [&]{ PsfswRecord i = rec; i.variance_from_weight = true; return has_gate(validate_psfsw_record(i), "PSFSW-G06"); }(),
                "PSFSW-G06");
    report_diff("psfsw_record_g05_ivar_key", has_gate(base, "PSFSW-G05"),
                [&]{ PsfswRecord i = rec; i.produced_keys = {"psfsw.ivar"}; return has_gate(validate_psfsw_record(i), "PSFSW-G05"); }(),
                "PSFSW-G05");
    report_diff("psfsw_record_g02_flux_units", has_gate(base, "PSFSW-G02"),
                [&]{ PsfswRecord i = rec; i.weight_units = "flux^-2"; return has_gate(validate_psfsw_record(i), "PSFSW-G02"); }(),
                "PSFSW-G02");
    report_diff("psfsw_record_g07_fwhm_only_epsf", has_gate(base, "PSFSW-G07"),
                [&]{ PsfswRecord i = rec; i.effective_psf_only_fwhm = true; return has_gate(validate_psfsw_record(i), "PSFSW-G07"); }(),
                "PSFSW-G07");
    report_diff("psfsw_record_g21_legacy_integer", has_gate(base, "PSFSW-G21"),
                [&]{ PsfswRecord i = rec; i.weight_mode = "0"; return has_gate(validate_psfsw_record(i), "PSFSW-G21"); }(),
                "PSFSW-G21");
  }

  /* CLI 模式路由单源（cli/v6_runtime_contract.h）逐 token 判定。 */
  {
    using namespace astrocs::v6runtime;
    const char* prod2[] = {"point_information", "surface_gls", "psfsw_robust"};
    const char* base2[] = {"equal", "pixel_ivar"};
    const char* rej2[] = {"psf_snr_power", "auto", "support_x_snr2", "0", "bogus", ""};
    for (const char* m : prod2)
      report_bool(std::string("route2_production_") + m, route_phase2_mode(m).kind == RouteKind::kProduction, m);
    for (const char* m : base2)
      report_bool(std::string("route2_baseline_") + m, route_phase2_mode(m).kind == RouteKind::kBaseline, m);
    for (const char* m : rej2)
      report_bool(std::string("route2_reject_") + (std::strlen(m) ? m : "empty"),
                  route_phase2_mode(m).kind == RouteKind::kReject, m);
    report_bool("route_legacy_int_0_reject", route_legacy_weight_mode_int(0).kind == RouteKind::kReject, "0");
    report_bool("route_legacy_int_1_baseline", route_legacy_weight_mode_int(1).kind == RouteKind::kBaseline, "1");
    report_bool("route_legacy_int_2_baseline", route_legacy_weight_mode_int(2).kind == RouteKind::kBaseline, "2");
    const char* prod3[] = {"surface_brightness", "point_source_flux", "visualization"};
    for (const char* m : prod3)
      report_bool(std::string("route3_production_") + m, route_phase3_mode(m).kind == RouteKind::kProduction, m);
    report_bool("route3_reject_bogus", route_phase3_mode("bogus_export").kind == RouteKind::kReject, "bogus_export");
    report_bool("no_implicit_phase_chain_run", is_implicit_phase_chain({"run"}), "run");
    report_bool("phase2_run_not_chained", !is_implicit_phase_chain({"phase2", "run"}), "phase2 run");
    report_bool("phase1_run_not_chained", !is_implicit_phase_chain({"phase1", "run"}), "phase1 run");
  }

  std::printf("{\"summary\":{\"pass\":%d,\"fail\":%d}}\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
