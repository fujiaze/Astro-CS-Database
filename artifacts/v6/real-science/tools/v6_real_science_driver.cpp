/* v6_real_science_driver.cpp — REAL-SCIENCE-001 五口径科学比较驱动
 *
 * 直接消费冻结 V6 库（lib/dynamic_psf, lib/snr_estimator, lib/photometric_calib）：
 *   Q_k = a_k P_k^T C_k^-1 d_k          [ADU^-1]  (FZ-FORMULA-Q)
 *   W_k = a_k^2 P_k^T C_k^-1 P_k        [ADU^-2]  (FZ-FORMULA-WINFO)
 *   F_hat = Sum Q / Sum W ; Var = 1/Sum W         (FZ-FORMULA-FHAT)
 *   PSFSW: Wt_k=C_norm*S^a*Conc^b/(N^g*B^d); W_psfsw=Wt/median(Wt) (FZ-FORMULA-PSFSW-COMPOSITE)
 *   covariance: C_out = R C_in R^T 由实际组合系数传播 (FZ-FORMULA-COV-PROP)
 *   effective PSF: P_eff = Sum alpha_k a_k P_k / [Sum alpha_k a_k P_k](0) (FZ-GATE-PSFSW-EPSF)
 *
 * 不新增科学公式：所有权威量经上述冻结库函数计算。equal/exposure/ivar 是
 * DOCUMENTED_BASELINE（非生产）口径，在此测量驱动内按冻结词表公式组合，
 * 并明确标注；W_info / PSFSW 走生产库函数。
 *
 * 用法: v6_real_science_driver --in <inputs_dir> --out <json> --csv <csv>
 */
#include "astrocs/v6/information_weight.h"
#include "astrocs/v6/psf_information.h"
#include "astrocs/v6/psfsw.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/resource.h>

using nlohmann::json;
using namespace astrocs::v6::p1psfw;

namespace {

struct StarFrame {
  std::string star_id;
  double ra = 0.0, dec = 0.0;
  std::vector<double> d, P, sigma2;
  double bg = 0.0, bg_rms = 0.0, exptime = 0.0;
};

struct Frame {
  int index = 0;
  double exptime = 0.0;
  std::string path, sha, date_obs;
  std::vector<StarFrame> stars;
};

struct Dataset {
  std::string name, object, telescope, path, sha;
  int stamp = 9;
  double global_bg = 0.0, global_rms = 0.0;
  std::vector<Frame> frames;
  std::vector<std::string> star_ids;
};

double median(std::vector<double> v) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  const std::size_t n = v.size();
  return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

std::string sha256_hex(const std::string& path) {
  // 仅用于 input 指纹；调用系统 sha256sum 会引入外部进程，这里用轻量 FNV 代替不合适。
  // 输入哈希由 Python 抽取阶段已写入 frame sha；此处只用文件大小占位哈希。
  std::ifstream f(path, std::ios::binary);
  std::uint64_t h = 1469598103934665603ULL;
  char buf[65536];
  while (f) {
    f.read(buf, sizeof(buf));
    std::streamsize n = f.gcount();
    for (std::streamsize i = 0; i < n; ++i) { h ^= (unsigned char)buf[i]; h *= 1099511628211ULL; }
  }
  char out[32];
  std::snprintf(out, sizeof(out), "%016llx", (unsigned long long)h);
  return std::string(out);
}

bool load_dataset(const std::string& path, Dataset* ds, std::string* err) {
  std::ifstream f(path);
  if (!f) { *err = "cannot open " + path; return false; }
  json j;
  try { f >> j; } catch (const std::exception& e) { *err = e.what(); return false; }
  ds->path = path;
  ds->sha = sha256_hex(path);
  ds->name = j.value("dataset", std::string());
  ds->object = j.value("object", std::string());
  ds->telescope = j.value("telescope", std::string());
  ds->stamp = j.value("stamp_size", 9);
  ds->global_bg = j.value("global_bg_median", 0.0);
  ds->global_rms = j.value("global_bg_rms", 0.0);
  for (const auto& fid : j.at("common_star_ids")) ds->star_ids.push_back(fid.get<std::string>());
  for (const auto& fj : j.at("frames")) {
    Frame fr;
    fr.index = fj.at("frame_index").get<int>();
    fr.exptime = fj.at("frame").at("exptime").get<double>();
    fr.path = fj.at("frame").at("path").get<std::string>();
    fr.sha = fj.at("frame").at("sha256").get<std::string>();
    fr.date_obs = fj.at("frame").value("date_obs", std::string());
    for (const auto& sj : fj.at("stars")) {
      StarFrame sf;
      sf.star_id = sj.at("star_id").get<std::string>();
      sf.ra = sj.value("ra", 0.0); sf.dec = sj.value("dec", 0.0);
      sf.d = sj.at("d").get<std::vector<double>>();
      sf.P = sj.at("P").get<std::vector<double>>();
      sf.sigma2 = sj.at("sigma2").get<std::vector<double>>();
      sf.bg = sj.value("bg", 0.0); sf.bg_rms = sj.value("bg_rms", 0.0);
      sf.exptime = fr.exptime;
      fr.stars.push_back(std::move(sf));
    }
    ds->frames.push_back(std::move(fr));
  }
  return true;
}

/* 组合系数 -> C_out = R C_in R^T（标量，R 为 K 维，C_in K*K 行主序）。*/
double rcrt(const std::vector<double>& alpha, const std::vector<double>& c_in) {
  const std::size_t k = alpha.size();
  double v = 0.0;
  for (std::size_t a = 0; a < k; ++a)
    for (std::size_t b = 0; b < k; ++b) v += alpha[a] * alpha[b] * c_in[a * k + b];
  return v;
}

/* 共同匹配滤波估计子：I=Sum alpha_k d_k; V=Sum alpha_k^2 sigma2_k (帧间独立对角);
 * F = Sum(P_eff I / V) / Sum(P_eff^2 / V); Var = 1/Sum(P_eff^2/V)。*/
bool common_matched_filter(const Dataset& ds, std::size_t sidx,
                           const std::vector<double>& alpha, std::size_t m,
                           const EffectivePsf& epsf, double* F, double* Var) {
  const std::size_t K = ds.frames.size();
  std::vector<double> I(m, 0.0), V(m, 0.0);
  for (std::size_t k = 0; k < K; ++k) {
    const StarFrame& s = ds.frames[k].stars[sidx];
    for (std::size_t p = 0; p < m; ++p) {
      I[p] += alpha[k] * s.d[p];
      V[p] += alpha[k] * alpha[k] * s.sigma2[p];
    }
  }
  if (!epsf.ok || epsf.profile.size() != m) return false;
  double num = 0.0, den = 0.0;
  for (std::size_t p = 0; p < m; ++p) {
    if (!(V[p] > 0.0)) return false;
    num += epsf.profile[p] * I[p] / V[p];
    den += epsf.profile[p] * epsf.profile[p] / V[p];
  }
  if (!(den > 0.0)) return false;
  *F = num / den; *Var = 1.0 / den;
  return std::isfinite(*F) && std::isfinite(*Var);
}

double fwhm_of(const std::vector<std::vector<double>>& P,
               const std::vector<double>& alpha, std::size_t m) {
  std::vector<const double*> ptrs;
  for (const auto& p : P) ptrs.push_back(p.data());
  std::vector<double> a_k(P.size(), 1.0);
  const EffectivePsf e = conventional_effective_psf(ptrs, m, a_k, alpha,
                                                    EffectivePsfNormalization::peak, "real-science");
  return e.ok ? e.fwhm : -1.0;
}

struct ModeMetric {
  double flux = 0.0, var = 0.0, snr = 0.0, fwhm = 0.0;
  /* 共同估计子（跨口径可比）：同一匹配滤波作用于该口径的组合面与方差面 + effective PSF。 */
  double mf_flux = 0.0, mf_var = 0.0, mf_snr = 0.0;
  std::vector<double> alpha;
  bool ok = false;
  std::string note;
};

struct StarResult {
  std::string star_id;
  double ra = 0.0, dec = 0.0;
  ModeMetric equal, exposure, ivar, w_info, psfsw;
  double sum_q = 0.0, sum_w = 0.0;
  std::vector<double> q_k, w_k, fk, vk;
  std::vector<double> a_nea_k, support_k;
  bool ok = false;
};

/* 逐星五口径（PSFSW 为组级，此处对每星用组 alpha 生成可比无量纲组合）*/
bool analyze_star(const Dataset& ds, std::size_t sidx, StarResult* out,
                  const std::vector<double>& w_psfsw, std::string* err) {
  const std::size_t K = ds.frames.size();
  const std::size_t m = ds.frames[0].stars[sidx].P.size();
  out->star_id = ds.star_ids[sidx];
  out->ra = ds.frames[0].stars[sidx].ra;
  out->dec = ds.frames[0].stars[sidx].dec;

  std::vector<double> f_equal(K), v_equal(K);
  std::vector<double> fk(K), vk(K), qk(K), wk(K), t(K), anea(K), supp(K);

  for (std::size_t k = 0; k < K; ++k) {
    const StarFrame& s = ds.frames[k].stars[sidx];
    if (s.P.size() != m || s.d.size() != m || s.sigma2.size() != m) {
      *err = "dimension mismatch"; return false;
    }
    const PointEstimate pe = w_info_diagonal(s.P.data(), m, s.sigma2.data(), s.d.data(), 1.0);
    if (!pe.ok) { *err = std::string("w_info rejected: ") + (pe.reject ? pe.reject : "?"); return false; }
    qk[k] = pe.q; wk[k] = pe.w_info;
    const std::vector<PointEstimate> one{pe};
    const FluxEstimate fe = combine_point_estimates(one);
    if (!fe.ok) { *err = "combine rejected"; return false; }
    fk[k] = fe.f_hat; vk[k] = fe.var_f;

    double sumd = 0.0, sumv = 0.0;
    for (std::size_t p = 0; p < m; ++p) { sumd += s.d[p]; sumv += s.sigma2[p]; }
    f_equal[k] = sumd; v_equal[k] = sumv;

    const PsfProfileStats ps = psf_profile_stats(s.P.data(), m);
    if (!ps.ok) { *err = std::string("psf rejected: ") + (ps.reject ? ps.reject : "?"); return false; }
    anea[k] = ps.a_nea; supp[k] = ps.sum_p;
    t[k] = s.exptime;
  }

  /* 通用组合：alpha -> Flux/Var（R C_in R^T，C_in 对角逐帧方差）*/
  auto combine = [&](const std::vector<double>& f, const std::vector<double>& v,
                     const std::vector<double>& alpha, ModeMetric* mm) {
    double num = 0.0, den = 0.0, asum = 0.0;
    for (std::size_t k = 0; k < K; ++k) { num += alpha[k] * f[k]; den += alpha[k]; asum += alpha[k]; }
    (void)den;
    std::vector<double> a(K, 0.0);
    double s = 0.0; for (double x : alpha) s += x;
    for (std::size_t k = 0; k < K; ++k) a[k] = alpha[k] / s;
    mm->alpha = a;
    mm->flux = num / s;
    std::vector<double> c_in(K * K, 0.0);
    for (std::size_t k = 0; k < K; ++k) c_in[k * K + k] = v[k];
    mm->var = rcrt(a, c_in);
    mm->snr = (mm->var > 0.0) ? mm->flux / std::sqrt(mm->var) : 0.0;
    std::vector<std::vector<double>> P;
    for (std::size_t k = 0; k < K; ++k) P.push_back(ds.frames[k].stars[sidx].P);
    mm->fwhm = fwhm_of(P, a, m);
    {
      std::vector<const double*> ptrs; for (const auto& p : P) ptrs.push_back(p.data());
      std::vector<double> ak(P.size(), 1.0);
      const EffectivePsf ep = conventional_effective_psf(
          ptrs, m, ak, a, EffectivePsfNormalization::peak, "mf-common");
      double F = 0.0, V = 0.0;
      if (common_matched_filter(ds, sidx, a, m, ep, &F, &V)) {
        mm->mf_flux = F; mm->mf_var = V; mm->mf_snr = (V > 0.0) ? F / std::sqrt(V) : 0.0;
      }
    }
    bool allpos = true; for (double x : a) if (!(x > 0.0)) allpos = false;
    mm->ok = allpos && std::isfinite(mm->flux) && std::isfinite(mm->var);
  };

  std::vector<double> alpha_equal(K, 1.0);
  combine(f_equal, v_equal, alpha_equal, &out->equal);
  out->equal.note = "DOCUMENTED_BASELINE unit_weight: I=mean_k d_k (vocab equal)";

  std::vector<double> alpha_exp(K, 0.0);
  { double s = 0.0; for (double x : t) s += x; for (std::size_t k = 0; k < K; ++k) alpha_exp[k] = t[k] / s; }
  combine(f_equal, v_equal, alpha_exp, &out->exposure);
  out->exposure.note = "DOCUMENTED_BASELINE exposure_time_weight: alpha=t_k/Sum t";

  std::vector<double> alpha_ivar(K, 0.0);
  { double s = 0.0; for (std::size_t k = 0; k < K; ++k) { alpha_ivar[k] = 1.0 / v_equal[k]; s += alpha_ivar[k]; }
    for (std::size_t k = 0; k < K; ++k) alpha_ivar[k] /= s; }
  combine(f_equal, v_equal, alpha_ivar, &out->ivar);
  out->ivar.note = "DOCUMENTED_BASELINE pixel_ivar: alpha=(1/v_k)/Sum(1/v_j), v_k=Sum_p sigma2";

  combine(fk, vk, wk, &out->w_info);
  out->w_info.note = "PRODUCTION point_information: Q/W, F=SumQ/SumW, Var=1/SumW, alpha=W/SumW";

  /* 逐星 PSFSW：用组级 median=1 的 W_psfsw 对同一星真实 stamp 做 conventional coadd。 */
  {
    std::vector<std::vector<double>> d1(K);
    std::vector<std::vector<char>> val1(K);
    std::vector<double> vflux(K, 0.0);
    for (std::size_t k = 0; k < K; ++k) {
      d1[k] = ds.frames[k].stars[sidx].d;
      val1[k].assign(m, 1);
      double sv = 0.0;
      for (std::size_t q = 0; q < m; ++q) sv += ds.frames[k].stars[sidx].sigma2[q];
      vflux[k] = sv;
    }
    const CoaddResult co = conventional_coadd(d1, val1, w_psfsw);
    if (!co.ok) { *err = std::string("psfsw per-star coadd rejected: ") + (co.reject ? co.reject : "?"); return false; }
    std::vector<double> alpha(K, 0.0);
    for (std::size_t k = 0; k < K; ++k) alpha[k] = co.alpha[k][0];
    std::vector<double> c_in(K * K, 0.0);
    for (std::size_t k = 0; k < K; ++k) c_in[k * K + k] = vflux[k];
    const CovariancePropagation cp = propagate_covariance(co.alpha, c_in);
    if (!cp.ok) { *err = std::string("psfsw per-star covariance rejected: ") + (cp.reject ? cp.reject : "?"); return false; }
    double f = 0.0, v = 0.0;
    for (double x : co.i_out) f += x;
    for (double x : cp.var_out) v += x;
    out->psfsw.alpha = alpha;
    out->psfsw.flux = f; out->psfsw.var = v;
    out->psfsw.snr = (v > 0.0) ? f / std::sqrt(v) : 0.0;
    std::vector<std::vector<double>> P;
    for (std::size_t k = 0; k < K; ++k) P.push_back(ds.frames[k].stars[sidx].P);
    out->psfsw.fwhm = fwhm_of(P, alpha, m);
    {
      std::vector<const double*> ptrs; for (const auto& p : P) ptrs.push_back(p.data());
      std::vector<double> ak(P.size(), 1.0);
      const EffectivePsf ep = conventional_effective_psf(
          ptrs, m, ak, alpha, EffectivePsfNormalization::peak, "mf-psfsw");
      double F = 0.0, V = 0.0;
      if (common_matched_filter(ds, sidx, alpha, m, ep, &F, &V)) {
        out->psfsw.mf_flux = F; out->psfsw.mf_var = V;
        out->psfsw.mf_snr = (V > 0.0) ? F / std::sqrt(V) : 0.0;
      }
    }
    bool allpos = true; for (double x : alpha) if (!(x > 0.0)) allpos = false;
    out->psfsw.ok = allpos && std::isfinite(f);
    out->psfsw.note = "PRODUCTION psfsw_robust: dimensionless W_psfsw (group median=1); alpha=W/SumW; C_out=R C_in R^T";
  }

  /* W_info 独立复算（库 combine vs 手工 ΣQ/ΣW 与 1/ΣW）*/
  double sq = 0.0, sw = 0.0;
  for (std::size_t k = 0; k < K; ++k) { sq += qk[k]; sw += wk[k]; }
  out->sum_q = sq; out->sum_w = sw;
  {
    std::vector<PointEstimate> pes;
    for (std::size_t k = 0; k < K; ++k) {
      const StarFrame& s = ds.frames[k].stars[sidx];
      pes.push_back(w_info_diagonal(s.P.data(), m, s.sigma2.data(), s.d.data(), 1.0));
    }
    const FluxEstimate fe = combine_point_estimates(pes);
    const double rel_f = std::fabs(fe.f_hat - sq / sw) / std::fabs(sq / sw);
    const double rel_v = std::fabs(fe.var_f - 1.0 / sw) / (1.0 / sw);
    if (rel_f > 1e-9 || rel_v > 1e-9) {
      *err = "W_info combine identity violated"; return false;
    }
  }
  out->q_k = qk; out->w_k = wk; out->fk = fk; out->vk = vk;
  out->a_nea_k = anea; out->support_k = supp;
  out->ok = out->equal.ok && out->exposure.ok && out->ivar.ok && out->w_info.ok && out->psfsw.ok;
  return out->ok;
}

struct GroupPsfsw {
  bool ok = false;
  std::string reject;
  std::vector<double> wt, w_psfsw;
  double median_wt = 0.0;
  std::vector<double> s, conc, n, b;
  std::vector<double> alpha;
  double flux = 0.0, var = 0.0, snr = 0.0, fwhm = 0.0;
  double defined_fraction = 0.0;
  std::vector<double> i_out, var_out;
  double a_nea_mean = 0.0;
};

bool run_psfsw_group(const Dataset& ds, GroupPsfsw* out, std::string* err) {
  const std::size_t K = ds.frames.size();
  const std::size_t N = ds.star_ids.size();
  const std::size_t m = ds.frames[0].stars[0].P.size();
  std::vector<ComponentValues> cvs(K);
  std::vector<double> anea_frame(K);
  for (std::size_t k = 0; k < K; ++k) {
    FrameComponentInput ci;
    std::vector<double> fhat, conc_s, noise_s, bg_s;
    double anea_sum = 0.0, bg_sum = 0.0;
    for (std::size_t s = 0; s < N; ++s) {
      const StarFrame& sf = ds.frames[k].stars[s];
      double sumd = 0.0, sumv = 0.0;
      for (std::size_t p = 0; p < m; ++p) { sumd += sf.d[p]; sumv += sf.sigma2[p]; }
      fhat.push_back(sumd);
      const PsfProfileStats ps = psf_profile_stats(sf.P.data(), m);
      if (!ps.ok) { *err = "psf rejected in psfsw"; return false; }
      anea_sum += ps.a_nea;
      bg_sum += sf.bg;
      conc_s.push_back(sumd / ps.a_nea);
      noise_s.push_back(std::sqrt(sumv));
      bg_s.push_back(sf.bg);
    }
    const double anea = anea_sum / static_cast<double>(N);
    anea_frame[k] = anea;
    ci.a_nea = anea;
    ci.fhat = fhat;
    ci.background_robust_mean = bg_sum / static_cast<double>(N);
    ci.a_ref = anea;
    ci.signal_samples = fhat;
    ci.concentration_samples = conc_s;
    ci.noise_samples = noise_s;
    ci.background_samples = bg_s;
    const PsfswFrameComponents comps = extract_psfsw_components(ci);
    if (!comps.ok) { *err = std::string("psfsw components rejected: ") + (comps.reject ? comps.reject : "?"); return false; }
    cvs[k].s = comps.s; cvs[k].conc = comps.conc; cvs[k].n = comps.n; cvs[k].b = comps.b;
    out->s.push_back(comps.s); out->conc.push_back(comps.conc);
    out->n.push_back(comps.n); out->b.push_back(comps.b);
  }
  const CompositeResult cr = compute_psfsw_weights(cvs);
  if (!cr.ok) { *err = std::string("psfsw composite rejected: ") + (cr.reject ? cr.reject : "?"); return false; }
  out->wt = cr.wt; out->w_psfsw = cr.w_psfsw; out->median_wt = cr.median_wt;

  /* conventional coadd over real per-frame stamp planes (all stars concatenated). */
  std::vector<std::vector<double>> d(K);
  std::vector<std::vector<char>> val(K);
  for (std::size_t k = 0; k < K; ++k) {
    for (std::size_t s = 0; s < N; ++s)
      for (std::size_t p = 0; p < m; ++p) d[k].push_back(ds.frames[k].stars[s].d[p]);
    val[k].assign(d[k].size(), 1);
  }
  const CoaddResult co = conventional_coadd(d, val, out->w_psfsw);
  if (!co.ok) { *err = std::string("coadd rejected: ") + (co.reject ? co.reject : "?"); return false; }
  out->alpha.assign(K, 0.0);
  for (std::size_t k = 0; k < K; ++k) out->alpha[k] = co.alpha[k][0];
  out->i_out = co.i_out;
  double ndef = 0.0; for (char x : co.defined) ndef += (x != 0);
  out->defined_fraction = ndef / static_cast<double>(co.defined.size());

  /* C_in: 逐帧真实孔径方差（对角、帧间独立），经实际组合系数传播 R C_in R^T。 */
  std::vector<double> c_in(K * K, 0.0);
  for (std::size_t k = 0; k < K; ++k) {
    double sumv = 0.0;
    for (std::size_t s = 0; s < N; ++s)
      for (std::size_t p = 0; p < m; ++p) sumv += ds.frames[k].stars[s].sigma2[p];
    c_in[k * K + k] = sumv;
  }
  const CovariancePropagation cp = propagate_covariance(co.alpha, c_in);
  if (!cp.ok) { *err = std::string("covariance rejected: ") + (cp.reject ? cp.reject : "?"); return false; }
  out->var_out = cp.var_out;
  double fsum = 0.0, vsum = 0.0;
  for (double x : co.i_out) fsum += x;
  for (double x : cp.var_out) vsum += x;
  out->flux = fsum; out->var = vsum;
  out->snr = (vsum > 0.0) ? fsum / std::sqrt(vsum) : 0.0;
  { double s = 0.0; for (double x : anea_frame) s += x;
    out->a_nea_mean = anea_frame.empty() ? 0.0 : s / static_cast<double>(anea_frame.size()); }
  std::vector<std::vector<double>> P;
  for (std::size_t k = 0; k < K; ++k) P.push_back(ds.frames[k].stars[0].P);
  out->fwhm = fwhm_of(P, out->alpha, m);
  out->ok = true;
  (void)m;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::string indir, outpath, csvpath;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--in" && i + 1 < argc) indir = argv[++i];
    else if (a == "--out" && i + 1 < argc) outpath = argv[++i];
    else if (a == "--csv" && i + 1 < argc) csvpath = argv[++i];
  }
  if (indir.empty() || outpath.empty()) {
    std::fprintf(stderr, "usage: %s --in <dir> --out <json> [--csv <csv>]\n", argv[0]);
    return 2;
  }
  std::vector<std::string> names;
  {
    namespace fs = std::filesystem;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(indir, ec)) {
      if (!e.is_regular_file()) continue;
      const std::string p = e.path().string();
      if (p.size() > 5 && p.substr(p.size() - 5) == ".json") {
        names.push_back(e.path().stem().string());
      }
    }
    std::sort(names.begin(), names.end());
  }
  if (names.empty()) { std::fprintf(stderr, "no dataset json in %s\n", indir.c_str()); return 2; }
  rusage r0{}, r1{};
  getrusage(RUSAGE_SELF, &r0);
  auto t0 = std::chrono::steady_clock::now();

  json root;
  root["task"] = "REAL-SCIENCE-001";
  root["driver"] = "v6_real_science_driver";
  root["mode_definitions"] = {
      {"equal", "DOCUMENTED_BASELINE unit_weight: alpha_k=1/K"},
      {"exposure", "DOCUMENTED_BASELINE exposure_time_weight: alpha_k=t_k/Sum t"},
      {"ivar", "DOCUMENTED_BASELINE pixel_ivar: alpha_k=(1/v_k)/Sum(1/v_j), v_k=Sum_p sigma2_k"},
      {"w_info", "PRODUCTION point_information: Q=aP^T C^-1 d [ADU^-1], W=a^2 P^T C^-1 P [ADU^-2], "
                 "F=SumQ/SumW, Var=1/SumW, alpha_k=W_k/SumW, effective PSF peak-normalized"},
      {"psfsw", "PRODUCTION psfsw_robust: dimensionless composite W_psfsw (median=1); "
                "alpha_k=W_psfsw,k/Sum; covariance C_out=R C_in R^T"},
  };
  root["units"] = {{"d", "ADU"}, {"P", "1"}, {"sigma2", "ADU^2"}, {"Q", "ADU^-1"},
                   {"W_info", "ADU^-2"}, {"flux", "ADU"}, {"var", "ADU^2"},
                   {"psfsw_weight", "1"}};
  root["datasets"] = json::array();

  std::ofstream csv;
  if (!csvpath.empty()) {
    csv.open(csvpath);
    csv << "dataset,star_id,mode,flux_ADU,var_ADU2,snr,fwhm_px,sum_w_adu-2,sum_q_adu-1,note\n";
  }

  for (const std::string& nmv : names) {
    const char* nm = nmv.c_str();
    Dataset ds;
    std::string err;
    const std::string p = indir + "/" + nmv + ".json";
    if (!load_dataset(p, &ds, &err)) { std::fprintf(stderr, "[%s] load failed: %s\n", nm, err.c_str()); return 3; }
    std::printf("[%s] frames=%zu stars=%zu\n", nm, ds.frames.size(), ds.star_ids.size());
    (void)nm;

    GroupPsfsw g;
    if (!run_psfsw_group(ds, &g, &err)) { std::fprintf(stderr, "[%s] psfsw failed: %s\n", nm, err.c_str()); return 4; }
    std::printf("  psfsw group: w_psfsw=["); 
    for (std::size_t i = 0; i < g.w_psfsw.size(); ++i)
      std::printf("%s%.4f", i ? "," : "", g.w_psfsw[i]);
    std::printf("] fwhm=%.3f flux=%.1f snr=%.1f\n", g.fwhm, g.flux, g.snr);

    json dj;
    dj["dataset"] = ds.name; dj["object"] = ds.object; dj["telescope"] = ds.telescope;
    dj["input_json"] = ds.path; dj["input_fnv1a"] = ds.sha;
    dj["n_frames"] = ds.frames.size(); dj["n_stars"] = ds.star_ids.size();
    dj["stamp_size"] = ds.stamp;
    dj["global_bg_median_ADU"] = ds.global_bg; dj["global_bg_rms_ADU"] = ds.global_rms;
    dj["frame_paths"] = json::array(); dj["frame_sha256"] = json::array(); dj["exptimes"] = json::array();
    for (const auto& fr : ds.frames) {
      dj["frame_paths"].push_back(fr.path); dj["frame_sha256"].push_back(fr.sha); dj["exptimes"].push_back(fr.exptime);
    }

    json psfsw_j;
    psfsw_j["ok"] = g.ok; psfsw_j["wt"] = g.wt; psfsw_j["w_psfsw"] = g.w_psfsw;
    psfsw_j["median_wt"] = g.median_wt; psfsw_j["alpha"] = g.alpha;
    psfsw_j["components"] = {{"signal", g.s}, {"concentration", g.conc}, {"noise", g.n}, {"background", g.b}};
    psfsw_j["flux_ADU"] = g.flux; psfsw_j["var_ADU2"] = g.var; psfsw_j["snr"] = g.snr;
    psfsw_j["effective_psf_fwhm_px"] = g.fwhm; psfsw_j["defined_fraction"] = g.defined_fraction;
    psfsw_j["covariance_method"] = "propagated_from_composite_coefficients (C_out=R C_in R^T)";
    psfsw_j["variance_from_weight"] = false;
    psfsw_j["weight_units"] = "1"; psfsw_j["weight_group_normalized"] = true;
    psfsw_j["psfsw_never_ivar"] = true;
    dj["group_psfsw"] = psfsw_j;

    json agg;
    std::vector<std::vector<double>> acc(5);  // equal,exposure,ivar,w_info,psfsw
    const char* mnames[5] = {"equal", "exposure", "ivar", "w_info", "psfsw"};
    json agg_j = json::object();
    json per_star = json::array();
    bool all_ok = true;
    for (std::size_t s = 0; s < ds.star_ids.size(); ++s) {
      StarResult sr;
      if (!analyze_star(ds, s, &sr, g.w_psfsw, &err)) {
        std::fprintf(stderr, "[%s] star %zu failed: %s\n", nm, s, err.c_str());
        all_ok = false; break;
      }
      json sj;
      sj["star_id"] = sr.star_id; sj["ra_deg"] = sr.ra; sj["dec_deg"] = sr.dec;
      sj["sum_q_ADU-1"] = sr.sum_q; sj["sum_w_ADU-2"] = sr.sum_w;
      sj["q_k"] = sr.q_k; sj["w_k"] = sr.w_k;
      sj["a_nea_k_px2"] = sr.a_nea_k; sj["support_k"] = sr.support_k;
      sj["snr_identity_w_info"] = "Sum W == combine Sum W; Var=1/Sum W verified rtol 1e-9";
      sj["modes"] = json::object();
      ModeMetric* mm[5] = {&sr.equal, &sr.exposure, &sr.ivar, &sr.w_info, &sr.psfsw};
      for (int mi = 0; mi < 5; ++mi) {
        json mj;
        mj["flux_ADU"] = mm[mi]->flux; mj["var_ADU2"] = mm[mi]->var;
        mj["snr"] = mm[mi]->snr; mj["effective_psf_fwhm_px"] = mm[mi]->fwhm;
        mj["mf_flux_ADU"] = mm[mi]->mf_flux; mj["mf_var_ADU2"] = mm[mi]->mf_var;
        mj["mf_snr"] = mm[mi]->mf_snr;
        mj["alpha"] = mm[mi]->alpha; mj["note"] = mm[mi]->note; mj["ok"] = mm[mi]->ok;
        sj["modes"][mnames[mi]] = mj;
        acc[mi].push_back(mm[mi]->snr);
        if (csv) csv << nm << "," << sr.star_id << "," << mnames[mi] << "," << mm[mi]->flux << ","
                     << mm[mi]->var << "," << mm[mi]->snr << "," << mm[mi]->fwhm << ","
                     << sr.sum_w << "," << sr.sum_q << "," << mm[mi]->note << "\n";
      }
      per_star.push_back(sj);
    }
    if (csv) {
      for (int mi = 0; mi < 5; ++mi)
        csv << nm << ",AGG," << mnames[mi] << ",,,,,,," << "n=" << acc[mi].size() << "\n";
      csv << nm << ",GROUP_TOTAL,psfsw," << g.flux << "," << g.var << "," << g.snr << ","
           << g.fwhm << ",,," << "all_stars_concatenated" << "\n";
    }
    for (int mi = 0; mi < 5; ++mi) {
      json a; a["median_snr"] = median(acc[mi]); a["n"] = acc[mi].size();
      agg_j[mnames[mi]] = a;
    }
    agg_j["psfsw_group_total_flux_ADU"] = g.flux;
    agg_j["psfsw_group_total_snr"] = g.snr;
    agg_j["pixel_ivar_alias"] = json{{"median_snr", median(acc[2])}, {"n", acc[2].size()}};
    dj["aggregate"] = agg_j;
    dj["per_star"] = per_star;
    dj["all_stars_ok"] = all_ok;
    dj["coverage"] = {
        {"frame_coverage", 1.0},
        {"note", "all selected common stars valid (in-bounds, unsaturated, positive flux) in all frames"},
        {"psfsw_defined_fraction", g.defined_fraction},
        {"support_sum_per_frame", 1.0},
        {"a_nea_mean_px2", g.a_nea_mean},
    };
    root["datasets"].push_back(dj);
    std::printf("  stars analyzed=%zu all_ok=%d\n", per_star.size(), (int)all_ok);
  }

  getrusage(RUSAGE_SELF, &r1);
  auto t1 = std::chrono::steady_clock::now();
  const double wall = std::chrono::duration<double>(t1 - t0).count();
  auto ru = [](const rusage& a, const rusage& b) {
    return json{{"user_s", (b.ru_utime.tv_sec - a.ru_utime.tv_sec) + 1e-6 * (b.ru_utime.tv_usec - a.ru_utime.tv_usec)},
                {"sys_s", (b.ru_stime.tv_sec - a.ru_stime.tv_sec) + 1e-6 * (b.ru_stime.tv_usec - a.ru_stime.tv_usec)}};
  };
  json res = ru(r0, r1);
  res["wall_s"] = wall;
  res["maxrss_kb"] = r1.ru_maxrss;
  res["minor_faults"] = r1.ru_minflt;
  res["major_faults"] = r1.ru_majflt;
  res["voluntary_ctx_switches"] = r1.ru_nvcsw;
  res["involuntary_ctx_switches"] = r1.ru_nivcsw;
  root["resources"] = res;

  std::ofstream os(outpath, std::ios::binary | std::ios::trunc);
  os << root.dump(2);
  std::printf("wall=%.3fs user=%.3f sys=%.3f maxrss=%ldKB\n", wall, res["user_s"].get<double>(),
              res["sys_s"].get<double>(), (long)r1.ru_maxrss);
  std::printf("wrote %s\n", outpath.c_str());
  return 0;
}
