// Issue #92 —— 真实 Gaia DR3SP 锥形搜索取星 dump。
//
// 复刻生产 `pc_api.cpp:run_with_gaia_impl` 的查询口径:
//   锥心 = (crval1, crval2), 半径 = fov_radius_deg, mag_min = 6.0,
//   mag_max 自适应 {12,13,14,15,16}, 第一次 n>=2000 即停 (否则取最后一档)。
// 只读真实 star catalog; 输出 ra,dec,magG CSV 供投影探针/独立 oracle 使用。
//
// 用法: gaia_cone_dump <gaia_dir> <ra> <dec> <radius_deg> <mag_min> <out_csv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include "gaia_client.h"
}

int main(int argc, char** argv) {
    if (argc < 7) {
        std::fprintf(stderr, "usage: %s <gaia_dir> <ra> <dec> <radius> <mag_min> <out_csv>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
    const double ra = std::atof(argv[2]);
    const double dec = std::atof(argv[3]);
    const double radius = std::atof(argv[4]);
    const double mag_min = std::atof(argv[5]);
    const std::string out_csv = argv[6];

    GaiaClient* client = gaia_client_create(dir.c_str());
    if (!client) { std::fprintf(stderr, "gaia_client_create failed: %s\n", dir.c_str()); return 3; }

    static const double mag_max_arr[] = {12.0, 13.0, 14.0, 15.0, 16.0};
    GaiaSpectrumStar* stars = nullptr;
    uint8_t* spectra = nullptr;
    int n = 0;
    double used_mag_max = 0.0;
    for (int i = 0; i < 5; ++i) {
        const double mm = mag_max_arr[i];
        const int rc = gaia_client_cone_search_with_spectrum(
            client, ra, dec, radius, mag_min, mm, &stars, &spectra, &n);
        if (rc != 0) {
            std::fprintf(stderr, "cone_search rc=%d mag_max=%.1f\n", rc, mm);
            if (stars) { std::free(stars); stars = nullptr; }
            if (spectra) { std::free(spectra); spectra = nullptr; }
            gaia_client_destroy(client);
            return 4;
        }
        std::fprintf(stderr, "adaptive: mag_max=%.1f n_gaia=%d\n", mm, n);
        used_mag_max = mm;
        if (n >= 2000 || i == 4) break;
        if (stars) { std::free(stars); stars = nullptr; }
        if (spectra) { std::free(spectra); spectra = nullptr; }
    }
    if (!stars || n <= 0) {
        std::fprintf(stderr, "no stars (n=%d)\n", n);
        gaia_client_destroy(client);
        return 5;
    }
    FILE* f = std::fopen(out_csv.c_str(), "w");
    if (!f) { std::free(stars); if (spectra) std::free(spectra); gaia_client_destroy(client); return 3; }
    std::fprintf(f, "# query ra=%.12f dec=%.12f radius=%.12f mag_min=%.3f mag_max=%.3f n=%d\n",
                 ra, dec, radius, mag_min, used_mag_max, n);
    std::fprintf(f, "ra,dec,magG\n");
    for (int i = 0; i < n; ++i)
        std::fprintf(f, "%.10f,%.10f,%.6f\n", stars[i].ra, stars[i].dec, stars[i].magG);
    std::fclose(f);
    std::fprintf(stderr, "wrote %d stars -> %s (mag_max=%.1f)\n", n, out_csv.c_str(), used_mag_max);
    std::free(stars);
    if (spectra) std::free(spectra);
    gaia_client_destroy(client);
    return 0;
}
