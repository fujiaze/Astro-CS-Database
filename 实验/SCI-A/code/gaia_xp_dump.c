/* SCI-A 实验单元 · Gaia DR3SP XP 谱导出夹具（只读，独立于生产 photometry 模块）
 *
 * 目的：把仓内本地 GaiaDR3SP XPSD 星表中某一锥内的星（位置/星等/XP 采样谱）
 *       导出为 CSV，供本实验的正向合成使用。星表访问走仓内
 *       lib/infrastructure/gaia_xpsd_client/src/gaia_client.c（唯一生产星表读取实现，
 *       本实验不复制其代码、不修改它；星表读取不是本实验的被测科学算法）。
 *
 * 记录语义（依据 lib/infrastructure/gaia_xpsd_client/README.md §3/§5，源码核对）：
 *   F(lambda) = byte[j]*flux_mul + flux_min   [W m^-2 nm^-1]
 *   lambda_j  = start_nm + j*step_nm          [nm]
 * 输出：CSV 首行 "#start_nm=.. step_nm=.. count=.. n=.."；列 ra,dec,magG,flux_min,flux_mul,b0..bN
 * 用法：gaia_xp_dump <xpsd_dir> <ra> <dec> <radius_deg> <mag_high> <out.csv>
 */
#include "gaia_client.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr, "usage: %s <xpsd_dir> <ra> <dec> <radius_deg> <mag_high> <out.csv>\n", argv[0]);
        return 2;
    }
    GaiaClient *c = gaia_client_create(argv[1]);
    if (!c) { fprintf(stderr, "[xp_dump] gaia_client_create failed for %s\n", argv[1]); return 1; }
    int start = 0, step = 0, cnt = 0;
    if (gaia_client_get_spectrum_params(c, &start, &step, &cnt) != 1) {
        fprintf(stderr, "[xp_dump] no spectrum params (not a DR3SP dir?)\n");
        gaia_client_destroy(c); return 1;
    }
    fprintf(stderr, "[xp_dump] grid start=%d step=%d count=%d\n", start, step, cnt);
    GaiaSpectrumStar *stars = NULL; uint8_t *spec = NULL; int n = 0;
    int ret = gaia_client_cone_search_with_spectrum(c, atof(argv[2]), atof(argv[3]),
                                                    atof(argv[4]), -1.5, atof(argv[5]),
                                                    &stars, &spec, &n);
    if (ret != 0) {
        fprintf(stderr, "[xp_dump] cone_search_with_spectrum ret=%d\n", ret);
        gaia_client_destroy(c); return 1;
    }
    FILE *o = fopen(argv[6], "w");
    if (!o) { fprintf(stderr, "[xp_dump] cannot open %s\n", argv[6]); gaia_client_destroy(c); return 1; }
    fprintf(o, "#start_nm=%d step_nm=%d count=%d n=%d\n", start, step, cnt, n);
    fprintf(o, "ra,dec,magG,flux_min,flux_mul");
    for (int j = 0; j < cnt; ++j) fprintf(o, ",b%d", j);
    fprintf(o, "\n");
    for (int i = 0; i < n; ++i) {
        fprintf(o, "%.8f,%.8f,%.4f,%.10g,%.10g", stars[i].ra, stars[i].dec,
                stars[i].magG, stars[i].flux_min, stars[i].flux_mul);
        for (int j = 0; j < cnt; ++j) fprintf(o, ",%u", (unsigned)spec[(size_t)i * cnt + j]);
        fprintf(o, "\n");
    }
    fclose(o);
    fprintf(stderr, "[xp_dump] n=%d -> %s\n", n, argv[6]);
    free(stars); free(spec); gaia_client_destroy(c);
    return 0;
}
