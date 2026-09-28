
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "snr_estimator.h"
int main(int argc, char** argv) {
  if (argc < 6) { std::fprintf(stderr, "usage: F_s_e B_e sigma_R_e gain fwhm [sigma_px]\n"); return 2; }
  const double F_e = std::atof(argv[1]);
  const double B_e = std::atof(argv[2]);
  const double rn_e = std::atof(argv[3]);
  const double gain = std::atof(argv[4]);
  const double fwhm = std::atof(argv[5]);
  const double sigma_px = (argc > 6) ? std::atof(argv[6]) : 0.0;  // 生产同尺度入口
  SnrSourceParams p;
  std::memset(&p, 0, sizeof(p));
  p.flux_adu = F_e / gain;                 // ADU
  p.fwhm_px = fwhm;                        // 检测块高斯 FWHM（<=0 时改用 sigma_px）
  p.sigma_px = sigma_px;                   // 本块 Moffat4 sigma（与 fwhm_px 同尺度）
  p.sigma_sky_adu = std::sqrt(B_e) / gain; // sky rms in ADU (sky Poisson only)
  p.gain_e_per_adu = gain;
  p.read_noise_e = rn_e;
  p.aperture_radius_px = 0.0;
  p.n_sky = 0.0;
  p.zero_point_mag = 0.0;
  p.profile_half_px = 0;
  SnrSourceResult r;
  const int rc = snr_source_snr_f64(&p, &r);
  if (rc != 0 || r.status != 0) { std::printf("{\"ok\":false,\"rc\":%d}\n", rc); return 1; }
  std::printf("{\"ok\":true,\"snr_optimal\":%.17g,\"sigma_f_optimal_adu\":%.17g,"
              "\"sum_p2\":%.17g,\"snr_aperture\":%.17g,\"sigma_f_aperture_adu\":%.17g,"
              "\"enclosed_fraction\":%.17g}\n",
              r.snr_optimal, r.sigma_f_optimal_adu, r.sum_p2,
              r.snr_aperture, r.sigma_f_aperture_adu, r.enclosed_fraction);
  return 0;
}
