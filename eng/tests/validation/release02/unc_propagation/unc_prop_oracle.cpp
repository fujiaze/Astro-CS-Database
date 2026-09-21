// UNC-PROP audit oracle (v3): quantify J_out C_theta J_out^T on the normalized
// pixel and compare the weight the code applies (raw Phase1 ivar) with the
// correctly propagated normalized variance, plus a g_k suppression scan.
//
// MA UPM: y_k(p) = g_k s(p) + b_k ; gauge g_ref=1, b_ref=0 (ALG-P2S-UPM.1).
// Reference-path normalization (FIX-GK): corrected_k = (y_k - b_k)/g_k.
//   J_out   = d corrected_k / d theta_free
//   Var_correct = sigma_y^2/g_k^2 + J_out C_theta J_out^T   (FZ-FORMULA-COV-PROP)
//   Var_used    = sigma_y^2                                (raw Phase1 ivar; no /g^2, no param)
// Production scheme B (SD-22) has no g: Var_correct = sigma_y^2 + J_out C_theta J_out^T.
#include "astro/phase2/upm.h"
#include <cmath>
#include <cstdio>
#include <vector>

struct Case { const char* name; int P; double s_lo; double s_hi; double sig0; double sig1; double g1; };

// returns weights via out params
static void run_case(const Case& c, double* w0, double* w1_corr, double* w1_used,
                     double* w1_corr_prod, double* pj_ref_out, double* pj_prod_out) {
  std::vector<double> s_true((std::size_t)c.P);
  for (int p = 0; p < c.P; ++p)
    s_true[(std::size_t)p] = c.s_lo + (c.s_hi - c.s_lo) * p / (c.P - 1.0);
  const std::uint64_t f0 = 10, f1 = 20;
  const double b1 = 10.0;
  std::vector<P2UpmMaObservation> obs;
  for (int p = 0; p < c.P; ++p) {
    P2UpmMaObservation a{};
    a.frame_id = f0; a.control_id = (std::uint64_t)p;
    a.value = s_true[(std::size_t)p]; a.control_ivar = 1.0 / (c.sig0 * c.sig0);
    obs.push_back(a);
    P2UpmMaObservation b{};
    b.frame_id = f1; b.control_id = (std::uint64_t)p;
    b.value = c.g1 * s_true[(std::size_t)p] + b1;
    b.control_ivar = 1.0 / (c.sig1 * c.sig1);
    obs.push_back(b);
  }
  P2UpmMaConfig cfg{};
  cfg.min_frames = 2; cfg.rank_rtol = 1e-10; cfg.kappa_max = 1e9;
  cfg.gauge_mode = 0; cfg.allow_additive_only_single_frame = 0;
  cfg.c_in_has_unrepresented_shared_terms = 0;
  cfg.huber_delta = 1.345; cfg.max_iterations = 100; cfg.tolerance = 1e-6;
  cfg.sigma_floor = 1e-3; cfg.zero_anchor_weight = 0.0;
  cfg.k_corr = 1.4; cfg.k_corr_applicability_domain = "phase2_control_cell";
  cfg.k_corr_calibration_run_id = "audit";
  void* model = nullptr;
  const int rc = p2_upm_ma_build(obs.data(), obs.size(), &cfg, &model);
  if (rc != 0 || !model) { std::printf("[%s] BUILD rc=%d\n", c.name, rc); *w0 = *w1_corr = *w1_used = *w1_corr_prod = 0; return; }
  P2UpmMaInfo info{};
  p2_upm_ma_info(model, &info);
  const std::size_t np = (std::size_t)info.n_params;
  std::vector<double> Cth(np * np, 0.0);
  p2_upm_ma_param_cov(model, Cth.data(), np);
  const std::size_t F = 2, full_n = (std::size_t)c.P + 2 * F;
  std::vector<int> f2f(full_n, -1);
  {
    std::vector<char> rem(full_n, 0);
    rem[(std::size_t)c.P + 0] = 1; rem[(std::size_t)c.P + F + 0] = 1;
    int nx = 0;
    for (std::size_t i = 0; i < full_n; ++i) if (!rem[i]) f2f[i] = nx++;
  }
  auto jcj = [&](const std::vector<double>& J) {
    double s = 0.0;
    for (std::size_t a = 0; a < np; ++a)
      for (std::size_t b = 0; b < np; ++b) s += J[a] * Cth[a * np + b] * J[b];
    return s;
  };
  const double y1 = c.g1 * s_true[0] + b1;
  double b1v = 0.0, g1v = 1.0;
  p2_upm_ma_solution(model, f1, 0, &g1v, &b1v, nullptr);
  const double corr1 = (y1 - b1v) / g1v;
  std::vector<double> Jr(np, 0.0), Jb(np, 0.0);
  if (f2f[(std::size_t)c.P + 1] >= 0) Jr[(std::size_t)f2f[(std::size_t)c.P + 1]] = -corr1 / g1v;
  if (f2f[(std::size_t)c.P + F + 1] >= 0) Jr[(std::size_t)f2f[(std::size_t)c.P + F + 1]] = -1.0 / g1v;
  if (f2f[(std::size_t)c.P + F + 1] >= 0) Jb[(std::size_t)f2f[(std::size_t)c.P + F + 1]] = -1.0;
  const double pj_ref = jcj(Jr), pj_prod = jcj(Jb);
  const double vraw1 = c.sig1 * c.sig1, vraw0 = c.sig0 * c.sig0;
  const double vc_ref = vraw1 / (g1v * g1v) + pj_ref;
  const double vc_prod = vraw1 + pj_prod;
  *w0 = 1.0 / vraw0; *w1_corr = 1.0 / vc_ref; *w1_used = 1.0 / vraw1;
  *w1_corr_prod = 1.0 / vc_prod; *pj_ref_out = pj_ref; *pj_prod_out = pj_prod;
  std::printf("\n[%s] P=%d kappa=%.3e g1=%.4f sigma0=%.3f sigma1=%.3f\n",
              c.name, c.P, info.kappa, g1v, c.sig0, c.sig1);
  std::printf("  J Cth J^T (ref path, /g included)=%.6e  (prod, no g)=%.6e\n", pj_ref, pj_prod);
  std::printf("  ref path Var_correct=%.6f (raw/g2=%.6f + param=%.6f) Var_used=%.6f\n",
              vc_ref, vraw1 / (g1v * g1v), pj_ref, vraw1);
  std::printf("  prod     Var_correct=%.6f (raw=%.6f + param=%.6f)     Var_used=%.6f\n",
              vc_prod, vraw1, pj_prod, vraw1);
  std::printf("  w0(ref)=%.6f  w1_correct=%.6f  w1_used=%.6f  used/correct=%.4f (pure 1/g^2=%.4f)\n",
              *w0, *w1_corr, *w1_used, (*w1_used) / (*w1_corr), 1.0 / (g1v * g1v));
  std::printf("  ORDER ref path: correct w1/w0=%.4f used w1/w0=%.4f  FLIP=%s\n",
              (*w1_corr) / (*w0), (*w1_used) / (*w0),
              ((*w1_corr) / (*w0) > 1.0 && (*w1_used) / (*w0) < 1.0) ? "YES" : "no");
  std::printf("  prod path: correct w1/w0=%.4f used/correct=%.4f\n",
              (*w1_corr_prod) / (*w0), (*w1_used) / (*w1_corr_prod));
  p2_upm_ma_close(model);
}

int main() {
  double a, b, c, d, e, f;
  run_case({"A well-conditioned P=40", 40, 50.0, 500.0, 2.0, 2.6, 2.0}, &a, &b, &c, &d, &e, &f);
  run_case({"B medium P=12",         12, 50.0, 300.0, 2.0, 2.6, 2.0}, &a, &b, &c, &d, &e, &f);
  run_case({"C ill-conditioned P=4",  4, 100.0, 143.0, 2.0, 2.6, 2.0}, &a, &b, &c, &d, &e, &f);
  run_case({"D mild g=1.25 P=40",    40, 50.0, 500.0, 2.0, 2.2, 1.25}, &a, &b, &c, &d, &e, &f);
  std::printf("\n=== g_k suppression scan (P=40 well-conditioned; sigma0=2.0 sigma1=2.6) ===\n");
  std::printf("  %6s %12s %12s %12s %12s\n", "g1", "w1_correct", "w1_used", "used/corr", "1/g^2");
  for (double g : {1.10, 1.25, 1.50, 2.00, 3.00}) {
    char nm[64]; std::snprintf(nm, sizeof(nm), "scan g=%.2f", g);
    run_case({nm, 40, 50.0, 500.0, 2.0, 2.6, g}, &a, &b, &c, &d, &e, &f);
  }
  return 0;
}
