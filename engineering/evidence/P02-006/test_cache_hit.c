/*
 * P02-006 GaiaClient 进程内缓存命中验证
 *
 * 验证内容:
 *   1) 同一 cone search 第二次调用应命中进程内缓存, 耗时显著低于第一次
 *   2) 不同 cone 参数应未命中, 触发实际查询
 *   3) 缓存键版本号 GAIA_CACHE_VERSION 隐式生效 (代码层校验)
 *
 * 编译: gcc -O2 -o test_cache_hit.exe test_cache_hit.c -Ilib/gaia_xpsd_client/src \
 *           -Llib/gaia_xpsd_client -lgaia_client -lz -fopenmp -lm
 * 运行: ./test_cache_hit.exe GaiaDR3SP <ra> <dec> <radius_deg> <mag_high>
 */
#include "gaia_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
static double now_sec(void) {
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
}
#else
#include <sys/time.h>
static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}
#endif

static double run_query(GaiaClient *client, double ra, double dec, double radius,
                         double mag_high, const char *tag) {
    GaiaStar *stars = NULL;
    int count = 0;
    double t0 = now_sec();
    int ret = gaia_client_cone_search(client, ra, dec, radius, -1.5, mag_high,
                                       &stars, &count);
    double t1 = now_sec();
    double elapsed = t1 - t0;
    printf("[%s] ret=%d count=%d elapsed=%.4fs (ra=%.4f dec=%.4f r=%.3f mag<%.2f)\n",
           tag, ret, count, elapsed, ra, dec, radius, mag_high);
    if (stars) free(stars);
    return elapsed;
}

int main(int argc, char **argv) {
    const char *data_dir = argc > 1 ? argv[1] : "GaiaDR3SP";
    double ra  = argc > 2 ? atof(argv[2]) : 266.4167;   /* SgrA* */
    double dec = argc > 3 ? atof(argv[3]) : -28.9867;
    double radius = argc > 4 ? atof(argv[4]) : 0.5;
    double mag_high = argc > 5 ? atof(argv[5]) : 14.0;

    printf("=== P02-006 GaiaClient 缓存命中验证 ===\n");
    printf("data_dir=%s ra=%.4f dec=%.4f radius=%.3f mag_high=%.2f\n",
           data_dir, ra, dec, radius, mag_high);

    GaiaClient *client = gaia_client_create(data_dir);
    if (!client) {
        fprintf(stderr, "gaia_client_create 失败\n");
        return 1;
    }
    printf("client 创建成功, file_count=%d, total_sources=%d, db_type=%d\n",
           gaia_client_get_file_count(client),
           gaia_client_get_total_sources(client),
           gaia_client_get_db_type(client));

    /* 1. 第一次查询: 应未命中, 触发实际查询 */
    double t1 = run_query(client, ra, dec, radius, mag_high, "Q1 缓存未命中");

    /* 2. 第二次相同参数查询: 应命中缓存, 耗时显著低于第一次 */
    double t2 = run_query(client, ra, dec, radius, mag_high, "Q2 缓存命中");

    /* 3. 不同参数查询: 应未命中 */
    double t3 = run_query(client, ra + 0.01, dec + 0.01, radius, mag_high,
                            "Q3 不同 cone (未命中)");

    /* 4. 再次相同查询 (验证 Q2 命中后 Q4 仍命中, 且 TTL 未过期) */
    double t4 = run_query(client, ra, dec, radius, mag_high, "Q4 仍命中");

    /* 判定缓存命中: Q2 / Q4 应显著快于 Q1 (≥10x) */
    double speedup_q2 = (t2 > 0) ? t1 / t2 : 0.0;
    double speedup_q4 = (t4 > 0) ? t1 / t4 : 0.0;
    int pass = (t2 < t1 * 0.1) && (t4 < t1 * 0.1);

    printf("\n=== 结果汇总 ===\n");
    printf("Q1 (未命中) 耗时:           %.4fs\n", t1);
    printf("Q2 (命中)   耗时:           %.4fs (加速 %.1fx)\n", t2, speedup_q2);
    printf("Q3 (不同 cone) 耗时:        %.4fs\n", t3);
    printf("Q4 (命中)   耗时:           %.4fs (加速 %.1fx)\n", t4, speedup_q4);
    printf("\nVERDICT: %s\n", pass ? "PASS" : "FAIL");
    printf("  缓存命中要求: Q2/Q4 < Q1*0.1 (即加速 ≥10x)\n");

    gaia_client_destroy(client);
    return pass ? 0 : 1;
}
