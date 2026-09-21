// SCI-B 仓内实测驱动：直接链接生产源 snr_science.cpp（只读，不改一行），
// 逐参数调用 snr_source_snr_f64，输出与 Python 镜像对拍。
// 编译：code/build_prod_driver.sh（产物落 run/SCI-402/，不入库）。
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include "snr_estimator.h"

int main(int argc, char** argv) {
    if (argc < 8) {
        std::fprintf(stderr, "usage: %s F_adu sigma_px sigma_sky_adu gain rn_e half fwhm_px [zp]\n", argv[0]);
        return 2;
    }
    SnrSourceParams p;
    std::memset(&p, 0, sizeof(p));
    p.flux_adu          = std::atof(argv[1]);
    p.sigma_px          = std::atof(argv[2]);
    p.sigma_sky_adu     = std::atof(argv[3]);
    p.gain_e_per_adu    = std::atof(argv[4]);
    p.read_noise_e      = std::atof(argv[5]);
    p.profile_half_px   = std::atoi(argv[6]);
    p.fwhm_px           = std::atof(argv[7]);
    p.aperture_radius_px = 0.0;
    p.n_sky             = 0.0;
    p.zero_point_mag    = (argc > 8) ? std::atof(argv[8]) : 0.0;

    SnrSourceResult r;
    const int rc = snr_source_snr_f64(&p, &r);
    double f5 = 0.0, m5 = std::nan("");
    snr_frame_depth_f64(&r, p.zero_point_mag, &f5, &m5);
    std::printf("rc=%d\n", rc);
    std::printf("snr_optimal=%.17g\n", r.snr_optimal);
    std::printf("sigma_f_optimal_adu=%.17g\n", r.sigma_f_optimal_adu);
    std::printf("snr_aperture=%.17g\n", r.snr_aperture);
    std::printf("sigma_f_aperture_adu=%.17g\n", r.sigma_f_aperture_adu);
    std::printf("enclosed_fraction=%.17g\n", r.enclosed_fraction);
    std::printf("aperture_correction=%.17g\n", r.aperture_correction);
    std::printf("n_pix=%.17g\n", r.n_pix);
    std::printf("sum_p2=%.17g\n", r.sum_p2);
    std::printf("snr_peak=%.17g\n", r.snr_peak);
    std::printf("flux5_adu=%.17g\n", r.flux5_adu);
    std::printf("m5_mag=%.17g\n", r.m5_mag);
    return 0;
}
