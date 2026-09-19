// ============================================================================
// p1_qf_oracle.cpp — RELEASE-02 FIX-P1 (P1-2) 独立 Oracle
//
// 目的: 证明质量位感知入口 pc_calibrate_simple_with_gaia_f64_v2_qf（pc_api.cpp,
//   非 C ABI 导出）确实把 quality_flags 送达 pc::StarMatcher::cleanAndScale
//   （star_matcher.cpp:428-438 的 PC_QF_SATURATED 有效域过滤）。
//
// 场景: 9 颗匹配星, 8 颗内点 (r 有散布 ⇒ S>0), 1 颗**饱和**且 r 偏移 +0.25 dex
//   （在 Tukey c·S 有效域内 ⇒ 无质量位时不会被 IRLS 剔除, 会拉动 location）。
//   ① 传 nullptr  → 离群饱和星进入拟合, |location| 被抬高;
//   ② 传 PC_QF_SATURATED 质量位 → 该星被有效域过滤, location 回到内点位置。
//   红/绿: 若 _qf 把 quality_flags 丢成 nullptr（修复前语义）→ ①② 无差异 → 必红。
//
// 编译: 见 build_qf_oracle.sh（生产源 + p1phot gaia 桩, 零被测域修改）。
// ============================================================================
#include "pc_api_qf.h"
#include "photometric_calib.h"

#include "p1phot_gaia_stub.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
void check(bool ok, const std::string& msg) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", msg.c_str());
  if (!ok) ++g_fail;
}

constexpr int kW = 256, kH = 256;
constexpr double kCd = 1.0 / 3600.0;  // 1 arcsec/px

// 一次 _qf 调用; 返回 rc, 填 out_scale / diag。
int run_qf(p1phot::stub::GaiaClient* client,
           const std::vector<double>& cx, const std::vector<double>& cy,
           const std::vector<double>& flux, const std::vector<int>& status,
           const std::vector<uint32_t>& qf, int n,
           const std::vector<double>& fw, const std::vector<double>& ft,
           const std::vector<double>& spec_wl,
           double* out_scale, PhotometricDiag* out_diag) {
  std::vector<double> pixels(static_cast<size_t>(kW) * kH, 100.0);
  std::vector<double> outp(static_cast<size_t>(kW) * kH, 0.0);
  int n_matched = -1;
  double sigma = 0.0;
  const uint32_t* qfp = qf.empty() ? nullptr : qf.data();
  return pc_calibrate_simple_with_gaia_f64_v2_qf(
      reinterpret_cast<void*>(client),
      0.0, 0.0, 0.05, 10.0, 16.0,
      fw.data(), ft.data(), static_cast<int>(fw.size()),
      nullptr, nullptr, 0,
      spec_wl.data(), static_cast<int>(spec_wl.size()),
      pixels.data(), kW, kH,
      cx.data(), cy.data(), flux.data(), status.data(), n,
      nullptr, nullptr,
      0.0, 0.0, 0.0, 0.0, kCd, 0.0, 0.0, kCd,
      0, nullptr, nullptr, nullptr, nullptr,
      outp.data(), &n_matched, out_scale, &sigma, out_diag, qfp);
}

}  // namespace

int main() {
  std::printf("== P1-2 quality-flag oracle (9 stars, 1 saturated outlier) ==\n");
  const int n = 9;
  // 光谱网格 + 平谱 (所有 byte=100, flux_min=0, flux_mul=1)
  std::vector<double> spec_wl;
  for (int i = 0; i < 343; ++i) spec_wl.push_back(336.0 + 2.0 * i);
  std::vector<double> fw, ft;
  for (int wl = 300; wl <= 1100; wl += 50) { fw.push_back(wl); ft.push_back(1.0); }

  // 像素位置 + Gaia 位置 (WCS: pixel(x,y) → sky(x/3600, y/3600) deg)
  std::vector<double> cx, cy, ra, dec;
  for (int i = 0; i < n; ++i) {
    const double x = 20.0 + 7.0 * i, y = 30.0 + 5.0 * i;
    cx.push_back(x); cy.push_back(y);
    ra.push_back(x / 3600.0); dec.push_back(y / 3600.0);
  }
  // 第 0 颗为饱和离群: r_0 = +0.25 dex; 其余内点散布 ±0.11 dex
  std::vector<double> r_true(n, 0.0);
  r_true[0] = 0.25;
  const double inl[8] = {-0.10, -0.07, -0.04, -0.01, 0.02, 0.05, 0.08, 0.11};
  for (int i = 0; i < 8; ++i) r_true[i + 1] = inl[i];

  // 桩星表: 平谱, 未知绝对 F_syn (由 reference_flux 读出)
  p1phot::stub::FakeClientConfig cfg;
  cfg.wl_start = 336; cfg.wl_step = 2; cfg.wl_count = 343;
  std::vector<std::vector<uint8_t>> spectra(n, std::vector<uint8_t>(343, 100));
  for (int i = 0; i < n; ++i) {
    p1phot::stub::FakeStar s;
    s.ra = ra[i]; s.dec = dec[i]; s.magG = 0.0;
    s.flux_min = 0.0f; s.flux_mul = 1.0f; s.spectrum = spectra[i];
    cfg.stars.push_back(s);
  }
  p1phot::stub::GaiaClient* client = p1phot::stub::create(cfg);

  // pass 1: 读 F_syn (reference_flux)
  std::vector<int> status(n, 0);
  std::vector<double> flux1(n, 1.0);
  std::vector<uint32_t> noqf;
  double scale1 = 0.0; PhotometricDiag d1; std::memset(&d1, 0, sizeof(d1));
  std::vector<double> pixels(static_cast<size_t>(kW) * kH, 100.0);
  std::vector<double> outp(static_cast<size_t>(kW) * kH, 0.0);
  std::vector<PcMatchRecord> recs(n);
  int nm1 = -1; double sg1 = 0.0;
  const int rc1 = pc_calibrate_simple_with_gaia_f64_v2_qf(
      reinterpret_cast<void*>(client), 0.0, 0.0, 0.05, 10.0, 16.0,
      fw.data(), ft.data(), static_cast<int>(fw.size()), nullptr, nullptr, 0,
      spec_wl.data(), static_cast<int>(spec_wl.size()), pixels.data(), kW, kH,
      cx.data(), cy.data(), flux1.data(), status.data(), n,
      nullptr, recs.data(),
      0.0, 0.0, 0.0, 0.0, kCd, 0.0, 0.0, kCd,
      0, nullptr, nullptr, nullptr, nullptr,
      outp.data(), &nm1, &scale1, &sg1, &d1, nullptr);
  check(rc1 == 0 && nm1 == n, "pass1 rc=0 且全部匹配 (n_matched=9)");
  std::vector<double> fsyn(n, 0.0);
  bool fsyn_ok = true;
  for (int i = 0; i < n; ++i) {
    fsyn[i] = recs[i].reference_flux;
    if (!(std::isfinite(fsyn[i]) && fsyn[i] > 0.0)) fsyn_ok = false;
  }
  check(fsyn_ok, "pass1 读出全部 F_syn>0 (用于构造已知 r)");
  if (!fsyn_ok) { std::printf("ORACLE_FAIL (fsyn)\n"); return 1; }

  // pass 2 输入: F_instr = 10^{r_true}·F_syn
  std::vector<double> flux(n);
  for (int i = 0; i < n; ++i) flux[i] = std::pow(10.0, r_true[i]) * fsyn[i];

  // ① nullptr: 饱和离群星进入拟合
  double scale_a = 0.0; PhotometricDiag da; std::memset(&da, 0, sizeof(da));
  const int rca = run_qf(client, cx, cy, flux, status, noqf, n, fw, ft, spec_wl,
                         &scale_a, &da);
  // ② 质量位: 第 0 颗标记 PC_QF_SATURATED (1u<<1)
  std::vector<uint32_t> qf(n, 0u); qf[0] = (1u << 1);
  double scale_b = 0.0; PhotometricDiag db; std::memset(&db, 0, sizeof(db));
  const int rcb = run_qf(client, cx, cy, flux, status, qf, n, fw, ft, spec_wl,
                         &scale_b, &db);

  const double loc_a = -std::log10(scale_a);
  const double loc_b = -std::log10(scale_b);
  std::printf("  ① 无质量位: rc=%d location=%+.6f dex scale=%.8f rejected_quality=%d\n",
              rca, loc_a, scale_a, da.rejected_quality);
  std::printf("  ② 有质量位: rc=%d location=%+.6f dex scale=%.8f rejected_quality=%d\n",
              rcb, loc_b, scale_b, db.rejected_quality);

  check(rca == 0 && rcb == 0, "两次调用 rc=0");
  check(da.rejected_quality == 0, "① 无质量位: 饱和星未被质量位剔除 (rejected_quality=0)");
  check(db.rejected_quality == 1, "② 有质量位: 饱和星被剔除 (rejected_quality=1)");
  check(std::fabs(loc_b) < std::fabs(loc_a),
        "② 剔除后 location 更接近内点中心 (|loc_b|<|loc_a|)");
  check(std::fabs(loc_a - loc_b) > 1e-3,
        "红/绿判别力: 有无质量位结果显著不同 (>1e-3 dex)");

  p1phot::stub::destroy(client);
  std::printf(g_fail == 0 ? "ORACLE_PASS\n" : "ORACLE_FAIL (%d)\n", g_fail);
  return g_fail == 0 ? 0 : 1;
}
