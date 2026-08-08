// gaia_sanitize_driver.c - ASan/UBSan 驱动: 真实 XPSD 解析 (DR3SP parser)
// 用法: <exe> <GaiaDR3SP_dir>
#include "gaia_client.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    GaiaClient* c = gaia_client_create_ex(argv[1], GAIA_DB_DR3SP);
    if (!c) { fprintf(stderr, "create fail\n"); return 3; }
    fprintf(stderr, "files=%d sources=%lld\n", gaia_client_get_file_count(c),
            (long long)gaia_client_get_total_sources(c));
    // 查询一组坐标 (覆盖不同 face/极区)
    double ra[] = {272.908258995422, 10.0};
    double dec[] = {-23.5926042853775, 89.9};
    GaiaSpectrumStar* stars = NULL;
    uint8_t* spectra = NULL;
    int* match = NULL;
    int n = 0;
    int rc = gaia_client_query_spectrum_by_coords(c, ra, dec, 2, 5.0, -5.0, 20.0,
                                                  &stars, &spectra, &match, &n);
    if (rc != 0) { fprintf(stderr, "query rc=%d\n", rc); return 4; }
    double flux_min_sum = 0.0;
    for (int i = 0; i < n; ++i) {
        flux_min_sum += stars[i].flux_min + stars[i].flux_mul * 100.0;
    }
    printf("GAIA_SANITIZE_OK matched=%d flux_probe=%.6e\n", n, flux_min_sum);
    if (stars) free(stars);
    if (spectra) free(spectra);
    if (match) free(match);
    gaia_client_destroy(c);
    return 0;
}
