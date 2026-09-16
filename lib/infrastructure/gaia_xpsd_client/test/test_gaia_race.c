// ============================================================================
// test_gaia_race.c - B4-P1-2 共址测试: block_cache 跨线程竞争 (TSAN)
//
// 修复点: gaia_client.c block_cache —— match 类查询并行轴=坐标, 多线程
// 并发 lookup(写 last_access)/insert(淘汰 free + memcpy/哈希结构写),
// 违反 docs/algorithms/GAIA_QUERY.md §4 单写者前提 => 数据竞争/UAF。
// 修复: per-file 锁 (bc_lock) + insert 返回缓存权威副本。
//
// 本测试: 自包含生成最小 XPSD fixture (Equirectangular, DR3 无光谱,
// 4 个叶块), 8 线程并发 cone_search 反复未命中/命中/同块重插。
// TSAN 构建下修复前应报 data race, 修复后应干净。
//
// 编译 (TSAN):
//   gcc -std=gnu99 -O1 -g -fsanitize=thread -I../src test_gaia_race.c \
//       ../src/gaia_client.c -lz -lpthread -lm -o test_gaia_race
// 运行: ./test_gaia_race
// 返回: 0=通过 (功能正确且 TSAN 无 race), 非0=失败
//
// 日期: 2026-09-08 (bughunt P1 batchA)
// ============================================================================

#include "gaia_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <pthread.h>
#include <unistd.h>
#include <zlib.h>

/* ---------- 最小 XPSD fixture 生成 (Equirectangular, NOSP, zlib) ---------- */

#define N_NODES 5      /* 0=内部根(子1..4), 1..4=叶, 每叶 1 星独立压缩块 */
#define N_STARS 4
#define STAR_REC 32
#define XML_PAD 64

static void put_u32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static void put_u16(uint8_t *p, uint16_t v) { memcpy(p, &v, 2); }
static void put_u64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static void put_f64(uint8_t *p, double v)   { memcpy(p, &v, 8); }

static int write_fixture(const char *dir, const char *fname) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, fname);

    /* XML 头 (tag 搜索按子串, 空格分隔属性) */
    char xml[1024];
    int xml_len = snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\"?>\n"
        "<XpsdFile>\n"
        "<DatabaseIdentifier>GaiaDR3-race-test</DatabaseIdentifier>\n"
        "<Data magnitudeRange=\"-1.5,20.0\" position=\"0\" "
        "compression=\"zlib\" itemSize=\"4\" parameters=\"\"/>\n"
        "<Statistics totalSources=\"%d\"/>\n"
        "<Tree projection=\"Equirectangular\" center=\"180.0,0.0\" "
        "rootPosition=\"%d\" nodeCount=\"%d\"/>\n"
        "</XpsdFile>\n",
        N_STARS, 2048 - N_NODES * 48, N_NODES);
    if (xml_len <= 0 || xml_len >= (int)sizeof(xml)) return -1;

    /* 节点 blob: 根(内部, 子=1..4) + 4 叶; Equirectangular 下
     * x0/x1 = 相对 center_ra 的 deg, y = dec
     * 叶分区: rel_ra ∈ {-0.75,-0.25,0.25,0.75}, dec ∈ {-0.75,-0.25,0.25,0.75} */
    uint8_t nodes[N_NODES * 48];
    memset(nodes, 0, sizeof(nodes));
    /* 正象限 (dx/dy 为 u32 无符号偏移语义, 不可为负)
     * rel_ra: {0.25,0.25,0.75,0.75}, dec: {0.25,0.75,0.25,0.75} */
    double rel_ra[N_STARS] = {0.25, 0.25, 0.75, 0.75};
    double dec_v [N_STARS] = {0.25, 0.75, 0.25, 0.75};
    /* 根: 内部节点 */
    {
        uint8_t *n = nodes;
        put_f64(n + 0, -1.0); put_f64(n + 8, -1.0);
        put_f64(n + 16, 1.0); put_f64(n + 24, 1.0);
        put_u32(n + 32, 1); put_u32(n + 36, 2);
        put_u32(n + 40, 3); put_u32(n + 44, 4);
    }
    for (int i = 0; i < N_STARS; i++) {
        uint8_t *n = nodes + (i + 1) * 48;
        put_f64(n + 0,  (i % 2 == 0) ? 0.0 : 0.5);  /* x0 (deg, rel) */
        put_f64(n + 8,  (i < 2) ? 0.0 : 0.5);        /* y0 (deg) */
        put_f64(n + 16, (i % 2 == 0) ? 0.5 : 1.0);   /* x1 */
        put_f64(n + 24, (i < 2) ? 0.5 : 1.0);        /* y1 */
        put_u64(n + 32, 0x8000000000000000ULL);  /* bit63=leaf, block_offset 后填 */
        put_u32(n + 40, STAR_REC);           /* block_size */
        put_u32(n + 44, 0);                  /* compressed_size 后填 */
    }

    /* 星点记录 (NOSP stride 32): dx,dy u32@0/4, mag_raw u16@20, dra i16@26=0
     * inv_scale=1/(3600*1000*500): dx = rel_ra_deg * 1.8e9 */
    uint8_t recs[N_STARS][STAR_REC];
    memset(recs, 0, sizeof(recs));
    uint16_t mag_raw = (uint16_t)((12.0 + 1.5) * 1000.0);  /* magG=12 */
    for (int i = 0; i < N_STARS; i++) {
        put_u32(recs[i] + 0, (uint32_t)(rel_ra[i] * 3600.0 * 1000.0 * 500.0));
        put_u32(recs[i] + 4, (uint32_t)(dec_v[i]  * 3600.0 * 1000.0 * 500.0));
        put_u16(recs[i] + 20, mag_raw);
        /* dra_raw=0, 其余 0 */
    }

    /* zlib 压缩每星为独立叶块 */
    uLongf comp_lens[N_STARS];
    uint8_t *comp[N_STARS];
    for (int i = 0; i < N_STARS; i++) {
        uLongf bound = compressBound(STAR_REC);
        comp[i] = (uint8_t *)malloc(bound);
        comp_lens[i] = bound;
        if (compress2(comp[i], &comp_lens[i], recs[i], STAR_REC, 6) != Z_OK) return -1;
    }

    /* 布局: [0..8) magic | [8..12) header_len | [12..16) 0 | [16..) xml
     * | pad 到 2048-N_NODES*48 (nodes) | ... | 2048 (data: 块顺序排列) */
    uint32_t nodes_pos = 2048 - N_NODES * 48;
    long file_size = 2048;
    for (int i = 0; i < N_STARS; i++) file_size += comp_lens[i];

    uint8_t *buf = (uint8_t *)calloc(1, file_size);
    memcpy(buf, "XPSD0100", 8);
    put_u32(buf + 8, (uint32_t)xml_len);
    memcpy(buf + 16, xml, xml_len);
    memcpy(buf + nodes_pos, nodes, sizeof(nodes));

    long off = 2048;
    for (int i = 0; i < N_STARS; i++) {
        /* 回填叶节点 (i+1) 的 block_offset 与 compressed_size */
        uint8_t *n = buf + nodes_pos + (i + 1) * 48;
        put_u64(n + 32, 0x8000000000000000ULL | (uint64_t)off);
        put_u32(n + 44, (uint32_t)comp_lens[i]);
        memcpy(buf + off, comp[i], comp_lens[i]);
        off += comp_lens[i];
        free(comp[i]);
    }

    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); return -1; }
    fwrite(buf, 1, file_size, f);
    fclose(f);
    free(buf);
    return 0;
}

/* ---------- 并发查询驱动 ---------- */

#define N_THREADS 8
#define N_ROUNDS 60

static GaiaClient *g_client = NULL;
static volatile int g_errors = 0;
static pthread_barrier_t g_barrier;

/* 查询中心 180,0 — 4 星全部在 r<=1.06 内; 用 r=2.0 保证全命中 */
static void *query_worker(void *arg) {
    long tid = (long)arg;
    pthread_barrier_wait(&g_barrier);
    for (int r = 0; r < N_ROUNDS; r++) {
        GaiaStar *stars = NULL;
        int count = 0;
        double ra = 180.0 + (r % 4) * 0.001;   /* 微移, 缓存键不同但块相同 */
        int ret = gaia_client_cone_search(g_client, ra, 0.0, 2.0, -1.5, 20.0,
                                          &stars, &count);
        if (ret != 0) {
            fprintf(stderr, "[tid%ld] round %d: cone_search failed\n", tid, r);
            g_errors++;
            continue;
        }
        if (count != N_STARS) {
            fprintf(stderr, "[tid%ld] round %d: count=%d (expect %d)\n",
                    tid, r, count, N_STARS);
            g_errors++;
        }
        /* 功能正确性: 修复前 UAF/撕裂可能读到错块 => 星等错乱 */
        for (int i = 0; i < count; i++) {
            if (fabs(stars[i].magG - 12.0) > 1e-9) {
                fprintf(stderr, "[tid%ld] round %d: star %d magG=%.6f corrupted\n",
                        tid, r, i, stars[i].magG);
                g_errors++;
            }
        }
        free(stars);
    }
    return NULL;
}

int main(int argc, char **argv) {
    const char *dir = (argc > 1) ? argv[1] : "/tmp/gaia_race_fixture";

    /* 清空并重建 fixture 目录 */
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strlen(ent->d_name) > 5) {
                char p[600];
                snprintf(p, sizeof(p), "%s/%s", dir, ent->d_name);
                unlink(p);
            }
        }
        closedir(d);
    } else {
        mkdir(dir, 0755);
    }
    if (write_fixture(dir, "race_test.xpsd") != 0) {
        fprintf(stderr, "fixture 生成失败\n");
        return 2;
    }

    g_client = gaia_client_create(dir);
    if (!g_client) {
        fprintf(stderr, "gaia_client_create 失败 (fixture 解析不过?)\n");
        return 2;
    }

    fprintf(stderr, "[probe] file_count=%d total_sources=%d\n",
            gaia_client_get_file_count(g_client),
            gaia_client_get_total_sources(g_client));

    /* 单线程先跑一遍: 验证 fixture 语义正确 (4 星, magG=12) */
    GaiaStar *stars = NULL;
    int count = 0;
    int ret = gaia_client_cone_search(g_client, 180.0, 0.0, 2.0, -1.5, 20.0,
                                      &stars, &count);
    if (ret != 0 || count != N_STARS) {
        fprintf(stderr, "单线程验证失败: ret=%d count=%d (fixture/解析问题, 非竞争)\n",
                ret, count);
        return 2;
    }
    free(stars);

    /* 并发阶段: barrier 后 8 线程同起, 全线程并发命中/未命中/重插 */
    pthread_barrier_init(&g_barrier, NULL, N_THREADS);
    pthread_t th[N_THREADS];
    for (long i = 0; i < N_THREADS; i++)
        pthread_create(&th[i], NULL, query_worker, (void *)i);
    for (int i = 0; i < N_THREADS; i++)
        pthread_join(th[i], NULL);
    pthread_barrier_destroy(&g_barrier);

    gaia_client_destroy(g_client);

    if (g_errors != 0) {
        fprintf(stderr, "=== B4-P1-2 结果: FAIL (%d errors) ===\n", g_errors);
        return 1;
    }
    fprintf(stderr, "=== B4-P1-2 结果: ALL PASS (功能正确; race 判定看 TSAN 报告) ===\n");
    return 0;
}
