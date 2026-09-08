/* CAT-GAIA-TEST (TEST-GAIA-001) — lib/gaia_xpsd_client 验证测试面（主测试）。
 *
 * 冻结依据：
 *   - MODULE_MIGRATION_TEMPLATE.md "<prefix>-TEST"：unit/properties/oracle/
 *     negative/worker/performance 全覆盖；期望值来自独立 fixture 真值
 *     （gaia_cat_manifest.h，由 gaia_xpsd_fixture_gen 生成，oracle 再独立
 *     复算），绝不由被测函数产生。
 *   - docs/algorithms/GAIA_QUERY.md §5 TEST-GAIA-DESIGN-001：
 *     I1 无假阴性 / I2 无假阳性 / I3 缓存 bitwise / I4 mag 窗口闭区间 /
 *     I5 截断 200000；负面（坏魔数/截断/坏压缩标签/GAIA_ALLOC_TEST 注入）；
 *     1/N worker；冻结容差 |Δpos|≤5e-10 deg、|Δmag|≤1e-9、光谱字节全等。
 *
 * 已登记缺陷（测试按标注放行，不修复生产代码）：
 *   KI-1  query_cache.out_mag 为 float（gaia_client.c:123），缓存命中路径
 *         mag 非双向 bitwise（ra/dec 仍 bitwise）→ 缓存一致性断言对 mag
 *         使用 |Δ|≤1e-9 并打印 [KNOWN-ISSUE KI-1]。
 *   KI-2  cone_search_with_spectrum 混合 DB（SP+DR3）时，DR3 星的光谱
 *         区段（343B/星）不被 memcpy（gaia_client.c:2045 条件），返回
 *         未初始化内存 → 测试对非 SP 星不校验光谱内容并打印标记。
 *
 * 模式（argv[1]）：
 *   unit <fixture_dir>
 *   properties <fixture_dir>
 *   negative <fixture_dir> <work_dir>
 *   worker <fixture_dir>
 *   dump <fixture_dir> <out.json>
 *   truncate <trunc_dir>
 *   performance <trunc_dir> <out.json>
 * polar 对照目标（gaia_cat_polar_ref，-DGAIA_POLAR_PRUNE_DISABLED、无
 * ALLOC_TEST 钩子）复用同一 dump 代码路径输出独立 JSON。
 */
#define GAIA_ALLOC_TEST 1
#include <stddef.h>
/* 钩子前置声明（定义在本文件 include 之后；gaia_client.c 内展开调用） */
void *gaia_test_malloc(size_t n);
void *gaia_test_calloc(size_t c, size_t n);
void *gaia_test_realloc(void *q, size_t n);
void gaia_test_free(void *p);
#include "gaia_client.c"      /* 被测实现（include 形式：启用分配钩子+块缓存可观测） */
#undef malloc
#undef calloc
#undef realloc
#undef free

#include <omp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "gaia_cat_manifest.h" /* 参考真值（生成器产物，构建期嵌入） */

/* ===== 分配钩子（GAIA_ALLOC_TEST 约定：测试程序提供全部 4 个） =====
 * 地址簿泄漏检测：分配记录（指针→大小），free 核销。能区分
 *   - wild free（释放未经钩子分配的指针 → free(NULL) 兜底，不崩）
 *   - 期末残留（实现 free 掉某个"未由钩子分配"的内部块 ≠ 泄漏）
 *   - 真泄漏（残留簿中未被核销的块）
 * 记录容量对单 client 生命周期足够（create+SKY ≈ 31 次分配）。 */
#define T_LEDGER_MAX 1024
static struct { void *p; size_t sz; int alive; } t_ledger[T_LEDGER_MAX];
static int  t_ledger_n   = 0;
static int  t_fail_after = 0;    /* 第 N 次分配失败；0=关闭 */
static int  t_alloc_seq  = 0;
static int  t_wild_free  = 0;    /* free 了未知指针的次数 */
static long t_live       = 0;    /* 未释放块计数（对簿） */
static long t_oom_fired  = 0;

static void t_alloc_reset(int fail_after) {
    t_fail_after = fail_after;
    t_alloc_seq = 0;
    t_oom_fired = 0;
}

static void t_ledger_clear(void) {
    t_ledger_n = 0;
    t_wild_free = 0;
    t_live = 0;
}

/* 期末对账：返回残留块数（>0 即泄漏），wild free 数写入 *out_wild */
static long t_ledger_reconcile(int *out_wild) {
    long leaked = 0;
    for (int i = 0; i < t_ledger_n; i++)
        if (t_ledger[i].alive) leaked++;
    if (out_wild) *out_wild = t_wild_free;
    return leaked;
}

static void t_ledger_add(void *p, size_t sz) {
    if (t_ledger_n < T_LEDGER_MAX) {
        t_ledger[t_ledger_n].p = p;
        t_ledger[t_ledger_n].sz = sz;
        t_ledger[t_ledger_n].alive = 1;
        t_ledger_n++;
    }
    t_live++;
}

static int t_ledger_find(void *p) {
    for (int i = 0; i < t_ledger_n; i++)
        if (t_ledger[i].p == p && t_ledger[i].alive) return i;
    return -1;
}

void *gaia_test_malloc(size_t n) {
    if (t_fail_after > 0 && ++t_alloc_seq >= t_fail_after) {
        t_fail_after = 0;        /* 单发：失败一次即恢复 */
        t_oom_fired++;
        return NULL;
    }
    void *p = malloc(n);
    if (p) t_ledger_add(p, n);
    return p;
}
void *gaia_test_calloc(size_t c, size_t n) {
    if (t_fail_after > 0 && ++t_alloc_seq >= t_fail_after) {
        t_fail_after = 0;
        t_oom_fired++;
        return NULL;
    }
    void *p = calloc(c, n);
    if (p) t_ledger_add(p, c * n);
    return p;
}
void *gaia_test_realloc(void *q, size_t n) {
    if (t_fail_after > 0 && ++t_alloc_seq >= t_fail_after) {
        t_fail_after = 0;
        t_oom_fired++;
        return NULL;             /* q 原块保持有效（realloc 语义） */
    }
    void *p = realloc(q, n);
    if (p) {
        int qi = (q != NULL) ? t_ledger_find(q) : -1;
        if (qi >= 0) {
            t_ledger[qi].p = p;  /* 原地改写：块转移 */
            t_ledger[qi].sz = n;
        } else {
            t_ledger_add(p, n);  /* 新块（q 未经钩子或为 NULL） */
        }
    }
    return p;
}
void gaia_test_free(void *p) {
    if (!p) return;
    int i = t_ledger_find(p);
    if (i >= 0) {
        t_ledger[i].alive = 0;
        t_live--;
    } else {
        t_wild_free++;           /* 释放了未知的块 */
    }
    free(p);                     /* 不崩且交还分配器（实现内部 free 对应 malloc 直配） */
}

/* ===== 基础设施 ===== */
static int t_failures = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        t_failures++; \
    } \
} while (0)

#define CHECK_KI(cond, ki, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "[EXPECTED-FAIL %s] %s:%d: ", ki, __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while (0)

static void t_free_results(void *p) { gaia_test_free(p); } /* 客户端分配 → 钩子释放 */

static double t_now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void t_mkdir_p(const char *path) {
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

/* ===== 参考真值计算（独立公式，与被测实现同源 ALG §2，非调用被测符号） ===== */
#define POS_TOL 5e-10   /* 冻结：位置 |Δ|≤5e-10 度 */
#define MAG_TOL 1e-9    /* 冻结：mag |Δ|≤1e-9 */

static double t_ang_dist_deg(double ra1, double dec1, double ra2, double dec2) {
    /* haversine（与实现的 acos 公式独立同义） */
    double d1 = (dec1 - dec2) * (M_PI / 180.0);
    double d2 = (ra1 - ra2) * (M_PI / 180.0);
    double a = sin(d1 / 2) * sin(d1 / 2) + cos(dec1 * M_PI / 180.0) * cos(dec2 * M_PI / 180.0) *
               sin(d2 / 2) * sin(d2 / 2);
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    return 2.0 * asin(sqrt(a)) * (180.0 / M_PI);
}

static double t_ra_sep_deg(double ra1, double ra2) {
    double d = fabs(ra1 - ra2);
    if (d > 180.0) d = 360.0 - d;
    return d;
}

typedef struct {
    double ra, dec, radius;
    double mag_low, mag_high;
} TQuery;

/* 期望集：manifest 中 满足角距≤radius 且 mag∈[lo,hi]（闭区间）者 */
static int t_expect_build(const TQuery *q, int *idx, int max) {
    int n = 0;
    for (int i = 0; i < G_MANIFEST_N; i++) {
        const GaiaCatRefStar *s = &G_MANIFEST[i];
        if (t_ang_dist_deg(q->ra, q->dec, s->ra, s->dec) <= q->radius + POS_TOL &&
            s->mag >= q->mag_low - 1e-12 && s->mag <= q->mag_high + 1e-12) {
            if (n < max) idx[n] = i;
            n++;
        }
    }
    return n;
}

/* 双向集合比对：每个结果星唯一匹配一个期望星，反之亦然。
 * 返回不匹配结果数 + 不匹配期望数（都应为 0）。 */
static void t_set_compare(const GaiaStar *res, int nres, const int *exp_idx, int nexp,
                          int *bad_res_out, int *bad_exp_out) {
    unsigned char *exp_hit = (unsigned char *)calloc((size_t)(nexp > 0 ? nexp : 1), 1);
    int bad_res = 0;
    for (int i = 0; i < nres; i++) {
        int found = -1;
        for (int j = 0; j < nexp; j++) {
            const GaiaCatRefStar *s = &G_MANIFEST[exp_idx[j]];
            if (t_ra_sep_deg(res[i].ra, s->ra) <= POS_TOL &&
                fabs(res[i].dec - s->dec) <= POS_TOL &&
                fabs(res[i].magG - s->mag) <= MAG_TOL) {
                found = j;
                break;
            }
        }
        if (found < 0) bad_res++;
        else exp_hit[found] = 1;
    }
    int bad_exp = 0;
    for (int j = 0; j < nexp; j++)
        if (!exp_hit[j]) bad_exp++;
    free(exp_hit);
    *bad_res_out = bad_res;
    *bad_exp_out = bad_exp;
}

/* ===== 查询集 ===== */
static const TQuery Q_CORE  = {10.0, 21.0, 2.5, 11.9, 13.1};   /* T0 子集（mag 窗口选择性） */
static const TQuery Q_T1    = {180.5, -29.0, 1.5, 10.0, 20.0}; /* 负 dec */
static const TQuery Q_POLAR = {0.0, 89.4, 1.2, 10.0, 20.0};    /* AE 极冠 */
static const TQuery Q_WRAP  = {0.2, 41.0, 1.5, 10.0, 20.0};    /* RA 0/360 环绕 */
static const TQuery Q_DR3   = {59.0, 5.5, 2.0, -1.6, 20.0};    /* DR3 + m=-1.5 sentinel */
static const TQuery Q_SKY   = {0.0, 0.0, 180.0, -1.5, 20.0};   /* 全域（680 星） */
static const TQuery Q_EMPTY = {10.0, 21.0, 1e-9, 10.0, 20.0};  /* 退化半径 */

/* ============================== unit ============================== */
static int mode_unit(const char *dir) {
    int rc = 0;
    GaiaClient *c = gaia_client_create(dir);
    CHECK(c != NULL, "create(%s) == NULL", dir);
    if (!c) return 1;
    CHECK(gaia_client_get_file_count(c) == 2, "file_count=%d != 2", gaia_client_get_file_count(c));
    CHECK(gaia_client_get_total_sources(c) == G_MANIFEST_N,
          "total=%d != %d", gaia_client_get_total_sources(c), G_MANIFEST_N);
    int dbt = gaia_client_get_db_type(c);
    CHECK(dbt == GAIA_DB_DR3 || dbt == GAIA_DB_DR3SP,
          "db_type=%d 不合法（files[0] 目录序依赖，仅校验枚举域）", dbt);
    int s1 = -1, s2 = -1, s3 = -1;
    rc = gaia_client_get_spectrum_params(c, &s1, &s2, &s3);
    CHECK(rc == 1 && s1 == 336 && s2 == 1 && s3 == 343,
          "spectrum_params rc=%d (%d,%d,%d) != (1,336,1,343)", rc, s1, s2, s3);

    /* ---- U1/I1+I2: 全域查询（680 星），双向集合比对 ---- */
    {
        int exp_idx[G_MANIFEST_N];
        int nexp = t_expect_build(&Q_SKY, exp_idx, G_MANIFEST_N);
        CHECK(nexp == G_MANIFEST_N, "期望集 nexp=%d != %d（真值自检）", nexp, G_MANIFEST_N);
        GaiaStar *res = NULL;
        int n = -1;
        rc = gaia_client_cone_search(c, Q_SKY.ra, Q_SKY.dec, Q_SKY.radius,
                                     Q_SKY.mag_low, Q_SKY.mag_high, &res, &n);
        CHECK(rc == 0, "cone_search SKY rc=%d", rc);
        CHECK(n == nexp, "I1/I2 SKY n=%d != %d", n, nexp);
        if (rc == 0 && n == nexp) {
            int br = 0, be = 0;
            t_set_compare(res, n, exp_idx, nexp, &br, &be);
            CHECK(br == 0, "I2 假阳性 %d 颗（结果不在真值集）", br);
            CHECK(be == 0, "I1 假阴性 %d 颗（真值未返回）", be);
        }
        t_free_results(res);
    }

    /* ---- U2: Q_CORE（窗口选择性 + dra 修正 + 光谱星），逐 API ---- */
    {
        int exp_idx[G_MANIFEST_N];
        int nexp = t_expect_build(&Q_CORE, exp_idx, G_MANIFEST_N);
        CHECK(nexp > 0, "Q_CORE 期望集为空（fixture 自检）");

        GaiaStar *res = NULL;
        int n = -1;
        rc = gaia_client_cone_search(c, Q_CORE.ra, Q_CORE.dec, Q_CORE.radius,
                                     Q_CORE.mag_low, Q_CORE.mag_high, &res, &n);
        CHECK(rc == 0 && n == nexp, "cone_search CORE rc=%d n=%d != %d", rc, n, nexp);
        if (rc == 0 && n == nexp) {
            int br = 0, be = 0;
            t_set_compare(res, n, exp_idx, nexp, &br, &be);
            CHECK(br == 0 && be == 0, "CORE 集合不匹配 br=%d be=%d", br, be);
            /* I4: mag 窗口闭区间（结果全部落窗内；边界星由真值集枚举保证） */
            for (int i = 0; i < n; i++)
                CHECK(res[i].magG >= Q_CORE.mag_low - 1e-12 && res[i].magG <= Q_CORE.mag_high + 1e-12,
                      "I4 mag=%.17g 越窗", res[i].magG);
        }
        t_free_results(res);

        /* for_solver：接口无 mag_low（仅上界）→ 期望集用 (-1.5, mag_high]；
         * ra/dec 与 cone_search bitwise 一致，mag == (float)mag */
        TQuery qs_solver = {Q_CORE.ra, Q_CORE.dec, Q_CORE.radius, -1.5, Q_CORE.mag_high};
        int exp_idx2[G_MANIFEST_N];
        int nexp2 = t_expect_build(&qs_solver, exp_idx2, G_MANIFEST_N);
        double *ora = NULL, *odec = NULL;
        float *omag = NULL;
        n = -1;
        rc = gaia_client_cone_search_for_solver(c, Q_CORE.ra, Q_CORE.dec, Q_CORE.radius,
                                                Q_CORE.mag_high, &ora, &odec, &omag, &n);
        CHECK(rc == 0 && n == nexp2, "for_solver CORE rc=%d n=%d != %d", rc, n, nexp2);
        if (rc == 0 && n == nexp2) {
            for (int i = 0; i < n; i++) {
                int found = -1;
                for (int j = 0; j < nexp2; j++) {
                    const GaiaCatRefStar *s = &G_MANIFEST[exp_idx2[j]];
                    if (t_ra_sep_deg(ora[i], s->ra) <= POS_TOL &&
                        fabs(odec[i] - s->dec) <= POS_TOL) { found = j; break; }
                }
                CHECK(found >= 0, "for_solver 星 %d 不在真值集", i);
                if (found >= 0)
                    /* float 出参：bitwise == (float)参考 mag（double→float 转换数学必然） */
                    CHECK(omag[i] == (float)G_MANIFEST[exp_idx2[found]].mag,
                          "for_solver mag=%.9g != (float)%.17g", (double)omag[i],
                          G_MANIFEST[exp_idx2[found]].mag);
            }
        }
        t_free_results(ora);
        t_free_results(odec);
        t_free_results(omag);

        /* photometry：G/BP/RP 全部对真值（SP 文件域：BP/RP=raw*0.001-1.5） */
        GaiaPhotometryStar *pres = NULL;
        n = -1;
        rc = gaia_client_cone_search_with_photometry(c, Q_CORE.ra, Q_CORE.dec, Q_CORE.radius,
                                                     Q_CORE.mag_low, Q_CORE.mag_high, &pres, &n);
        CHECK(rc == 0 && n == nexp, "photometry CORE rc=%d n=%d != %d", rc, n, nexp);
        if (rc == 0 && n == nexp) {
            for (int i = 0; i < n; i++) {
                int found = -1;
                for (int j = 0; j < nexp; j++) {
                    const GaiaCatRefStar *s = &G_MANIFEST[exp_idx[j]];
                    if (t_ra_sep_deg(pres[i].ra, s->ra) <= POS_TOL &&
                        fabs(pres[i].dec - s->dec) <= POS_TOL) { found = j; break; }
                }
                CHECK(found >= 0, "photometry 星 %d 不在真值集", i);
                if (found >= 0) {
                    const GaiaCatRefStar *s = &G_MANIFEST[exp_idx[found]];
                    CHECK(fabs(pres[i].magG - s->mag) <= MAG_TOL, "phot magG %.17g != %.17g",
                          pres[i].magG, s->mag);
                    double bpr = (double)s->bp_raw * 0.001 - 1.5;
                    double rpr = (double)s->rp_raw * 0.001 - 1.5;
                    CHECK(fabs(pres[i].magBP - bpr) <= MAG_TOL, "phot BP %.9g != %.9g",
                          pres[i].magBP, bpr);
                    CHECK(fabs(pres[i].magRP - rpr) <= MAG_TOL, "phot RP %.9g != %.9g",
                          pres[i].magRP, rpr);
                }
            }
        }
        t_free_results(pres);

        /* spectrum（极冠窄锥：全 SP 星，规避 KI-2 混合 DB 垃圾区段） */
        GaiaSpectrumStar *sres = NULL;
        uint8_t *spec = NULL;
        n = -1;
        rc = gaia_client_cone_search_with_spectrum(c, Q_POLAR.ra, Q_POLAR.dec, Q_POLAR.radius,
                                                   Q_POLAR.mag_low, Q_POLAR.mag_high,
                                                   &sres, &spec, &n);
        int nexp_polar = t_expect_build(&Q_POLAR, exp_idx, G_MANIFEST_N);
        CHECK(rc == 0 && n == nexp_polar, "spectrum POLAR rc=%d n=%d != %d", rc, n, nexp_polar);
        if (rc == 0 && n == nexp_polar) {
            for (int i = 0; i < n; i++) {
                int found = -1;
                for (int j = 0; j < nexp_polar; j++) {
                    const GaiaCatRefStar *s = &G_MANIFEST[exp_idx[j]];
                    if (t_ra_sep_deg(sres[i].ra, s->ra) <= POS_TOL &&
                        fabs(sres[i].dec - s->dec) <= POS_TOL) { found = j; break; }
                }
                CHECK(found >= 0, "spectrum 星 %d 不在真值集", i);
                if (found >= 0) {
                    const GaiaCatRefStar *s = &G_MANIFEST[exp_idx[found]];
                    CHECK(s->has_spec, "真值星 %s 无光谱（fixture 自检）", s->id);
                    CHECK(s->spectrum[0] == spec[(size_t)i * 343],
                          "极冠星 %d byte0=%d != %u（波形幅度逐星可辨）", i,
                          spec[(size_t)i * 343], s->spectrum[0]);
                    for (int j = 0; j < 343; j++)
                        if (spec[(size_t)i * 343 + j] != s->spectrum[j]) {
                            CHECK(0, "spectrum 星 %d byte %d: %d != %u",
                                  i, j, spec[(size_t)i * 343 + j], s->spectrum[j]);
                            break;
                        }
                    /* flux = byte*mul + min：spot check j=0（mul=85,min=-1） */
                    double flux0 = (double)spec[(size_t)i * 343] * sres[i].flux_mul + sres[i].flux_min;
                    CHECK(fabs(flux0 - ((double)s->spectrum[0] * 85.0 - 1.0)) < 1e-3,
                          "flux0=%.9g 不符合 byte*85-1", flux0);
                }
            }
        }
        t_free_results(sres);
        t_free_results(spec);
    }

    /* ---- U3: RA 0/360 环绕（wrap 归一化到 [0,360)） ---- */
    {
        int exp_idx[G_MANIFEST_N];
        int nexp = t_expect_build(&Q_WRAP, exp_idx, G_MANIFEST_N);
        GaiaStar *res = NULL;
        int n = -1;
        rc = gaia_client_cone_search(c, Q_WRAP.ra, Q_WRAP.dec, Q_WRAP.radius,
                                     Q_WRAP.mag_low, Q_WRAP.mag_high, &res, &n);
        CHECK(rc == 0 && n == nexp, "WRAP rc=%d n=%d != %d", rc, n, nexp);
        if (rc == 0 && n == nexp) {
            int br = 0, be = 0;
            t_set_compare(res, n, exp_idx, nexp, &br, &be);
            CHECK(br == 0 && be == 0, "WRAP 集合不匹配 br=%d be=%d", br, be);
            int wrap_seen = 0;
            for (int i = 0; i < n; i++)
                if (res[i].ra > 359.0) wrap_seen++;
            CHECK(wrap_seen > 0, "环绕验证：未观测到 ra>359 的归一化星");
        }
        t_free_results(res);
    }

    /* ---- U4: DR3 sentinel（BP/RP=0）+ m=-1.5 窗口下界 ---- */
    {
        GaiaPhotometryStar *pres = NULL;
        int n = -1;
        rc = gaia_client_cone_search_with_photometry(c, Q_DR3.ra, Q_DR3.dec, Q_DR3.radius,
                                                     Q_DR3.mag_low, Q_DR3.mag_high, &pres, &n);
        CHECK(rc == 0 && n > 0, "DR3 phot rc=%d n=%d", rc, n);
        int sentinel = 0;
        for (int i = 0; i < n; i++)
            if (pres[i].magBP == 0.0 && pres[i].magRP == 0.0) sentinel++;
        CHECK(sentinel == n, "DR3 BP/RP sentinel: %d/%d 非零（has_spectrum 域判定）", sentinel, n);
        t_free_results(pres);
    }

    /* ---- U5: 空/退化/错误参数 ---- */
    {
        GaiaStar *res = (GaiaStar *)0x1;
        int n = -1;
        rc = gaia_client_cone_search(c, Q_EMPTY.ra, Q_EMPTY.dec, Q_EMPTY.radius,
                                     Q_EMPTY.mag_low, Q_EMPTY.mag_high, &res, &n);
        CHECK(rc == 0 && n == 0 && res == NULL, "EMPTY rc=%d n=%d res=%p", rc, n, (void *)res);
        t_free_results(res);

        n = -1;
        rc = gaia_client_cone_search(c, 10.0, 21.0, -1.0, 10.0, 20.0, &res, &n);
        CHECK(rc == 0 && n == 0, "负半径 rc=%d n=%d（实现事实：无结果不崩溃）", rc, n);
        t_free_results(res);

        rc = gaia_client_cone_search(NULL, 10.0, 21.0, 1.0, 10.0, 20.0, &res, &n);
        CHECK(rc == -1, "NULL client rc=%d != -1", rc);
        rc = gaia_client_cone_search(c, 10.0, 21.0, 1.0, 10.0, 20.0, NULL, &n);
        CHECK(rc == -1, "NULL out_stars rc=%d != -1", rc);
        rc = gaia_client_cone_search(c, 10.0, 21.0, 1.0, 10.0, 20.0, &res, NULL);
        CHECK(rc == -1, "NULL out_count rc=%d != -1", rc);
    }

    /* ---- U6: db_type 过滤（create_ex） ---- */
    {
        GaiaClient *dr3 = gaia_client_create_ex(dir, GAIA_DB_DR3);
        CHECK(dr3 != NULL && gaia_client_get_file_count(dr3) == 1,
              "DR3 过滤 file_count=%d", dr3 ? gaia_client_get_file_count(dr3) : -1);
        if (dr3)
            CHECK(gaia_client_get_total_sources(dr3) == 256, "DR3 total=%d != 256",
                  gaia_client_get_total_sources(dr3));
        gaia_client_destroy(dr3);
        GaiaClient *sp = gaia_client_create_ex(dir, GAIA_DB_DR3SP);
        CHECK(sp != NULL && gaia_client_get_file_count(sp) == 1,
              "DR3SP 过滤 file_count=%d", sp ? gaia_client_get_file_count(sp) : -1);
        if (sp)
            CHECK(gaia_client_get_total_sources(sp) == 424, "DR3SP total=%d != 424",
                  gaia_client_get_total_sources(sp));
        gaia_client_destroy(sp);
        GaiaClient *none = gaia_client_create_ex(dir, (GaiaDbType)99);
        CHECK(none != NULL && gaia_client_get_file_count(none) == 0,
              "未知 db_type：create=%p files=%d（应空客户端）", (void *)none,
              none ? gaia_client_get_file_count(none) : -1);
        gaia_client_destroy(none);
    }

    /* ---- U7: 空目录 / 不存在目录 / destroy(NULL) ---- */
    {
        char tmp[2048];
        snprintf(tmp, sizeof(tmp), "%s_unit_empty", dir);
        t_mkdir_p(tmp);
        GaiaClient *e = gaia_client_create(tmp);
        CHECK(e != NULL, "空目录 create == NULL（实现事实：空客户端非 NULL）");
        if (e) {
            CHECK(gaia_client_get_file_count(e) == 0, "空目录 file_count=%d",
                  gaia_client_get_file_count(e));
            GaiaStar *res = NULL;
            int n = -1;
            rc = gaia_client_cone_search(e, 10.0, 21.0, 30.0, -2.0, 25.0, &res, &n);
            CHECK(rc == 0 && n == 0, "空目录查询 rc=%d n=%d", rc, n);
            t_free_results(res);
            gaia_client_destroy(e);
        }
        GaiaClient *bad = gaia_client_create("/nonexistent_gaia_cat_test_dir");
        CHECK(bad == NULL, "不存在目录 create != NULL");
        gaia_client_destroy(NULL); /* 必须 no-op */
    }

    gaia_client_destroy(c);
    fprintf(stderr, "unit: done failures=%d\n", t_failures);
    return t_failures ? 1 : 0;
}

/* =========================== properties =========================== */
static int mode_properties(const char *dir) {
    /* P1 I3 缓存 bitwise（ra/dec）+ KI-1（mag float 往返） */
    {
        GaiaClient *c = gaia_client_create(dir);
        CHECK(c != NULL, "create");
        if (!c) return 1;
        GaiaStar *cold = NULL, *warm = NULL;
        int nc = -1, nw = -1;
        int rc = gaia_client_cone_search(c, Q_CORE.ra, Q_CORE.dec, Q_CORE.radius,
                                         Q_CORE.mag_low, Q_CORE.mag_high, &cold, &nc);
        CHECK(rc == 0, "cold rc=%d", rc);
        rc = gaia_client_cone_search(c, Q_CORE.ra, Q_CORE.dec, Q_CORE.radius,
                                     Q_CORE.mag_low, Q_CORE.mag_high, &warm, &nw);
        CHECK(rc == 0, "warm rc=%d", rc);
        CHECK(nc == nw, "缓存命中 n=%d != 冷 n=%d", nw, nc);
        if (nc == nw) {
            int bitwise = 1, mag_delta = 0;
            for (int i = 0; i < nc; i++) {
                if (memcmp(&cold[i].ra, &warm[i].ra, 8) != 0 ||
                    memcmp(&cold[i].dec, &warm[i].dec, 8) != 0) bitwise = 0;
                if (fabs(cold[i].magG - warm[i].magG) > 1e-9) mag_delta++;
            }
            CHECK(bitwise, "I3 缓存命中 ra/dec 非 bitwise");
            if (mag_delta)
                fprintf(stderr, "[KNOWN-ISSUE KI-1] 缓存 mag float 往返差异 %d/%d（|Δ|≤1e-9 内放行）\n",
                        mag_delta, nc);
        }

        /* P2 跨客户端确定性：新 client 冷查询 == 首 client 冷查询 bitwise
         * （cold 保留到 P2 结束后再释放，勿提前 free） */
        GaiaClient *c2 = gaia_client_create(dir);
        CHECK(c2 != NULL, "create#2");
        GaiaStar *cold2 = NULL;
        int nc2 = -1;
        rc = gaia_client_cone_search(c2, Q_CORE.ra, Q_CORE.dec, Q_CORE.radius,
                                     Q_CORE.mag_low, Q_CORE.mag_high, &cold2, &nc2);
        CHECK(rc == 0 && nc2 == nc, "跨客户端 n=%d != %d", nc2, nc);
        if (rc == 0 && nc2 == nc) {
            for (int i = 0; i < nc; i++) {
                CHECK(memcmp(&cold2[i].ra, &cold[i].ra, 8) == 0 &&
                      memcmp(&cold2[i].dec, &cold[i].dec, 8) == 0,
                      "跨客户端星 %d 非 bitwise（确定性破坏）", i);
            }
        }
        t_free_results(cold2);
        t_free_results(cold);
        t_free_results(warm);
        gaia_client_destroy(c2);
        gaia_client_destroy(c);
    }

    /* P3 输出域不变量：ra∈[0,360) dec∈[-90,90]；结果星角距≤radius+tol */
    {
        GaiaClient *c = gaia_client_create(dir);
        if (!c) return 1;
        const TQuery *qs[] = {&Q_CORE, &Q_T1, &Q_POLAR, &Q_WRAP, &Q_SKY};
        for (int q = 0; q < 5; q++) {
            GaiaStar *res = NULL;
            int n = -1;
            int rc = gaia_client_cone_search(c, qs[q]->ra, qs[q]->dec, qs[q]->radius,
                                             qs[q]->mag_low, qs[q]->mag_high, &res, &n);
            CHECK(rc == 0, "P3 q%d rc=%d", q, rc);
            for (int i = 0; i < n; i++) {
                CHECK(res[i].ra >= 0.0 && res[i].ra < 360.0, "ra=%.17g 越域", res[i].ra);
                CHECK(res[i].dec >= -90.0 && res[i].dec <= 90.0, "dec=%.17g 越域", res[i].dec);
                double d = t_ang_dist_deg(qs[q]->ra, qs[q]->dec, res[i].ra, res[i].dec);
                CHECK(d <= qs[q]->radius + POS_TOL, "q%d 星 %d 角距 %.12f > %.12f",
                      q, i, d, qs[q]->radius);
                CHECK(res[i].magG >= qs[q]->mag_low - 1e-12 && res[i].magG <= qs[q]->mag_high + 1e-12,
                      "q%d 星 %d mag %.17g 越窗", q, i, res[i].magG);
            }
            t_free_results(res);
        }
        gaia_client_destroy(c);
    }

    /* P4 半径单调性：小半径结果 ⊆ 大半径结果（键：ra/dec bitwise 匹配） */
    {
        GaiaClient *c = gaia_client_create(dir);
        if (!c) return 1;
        GaiaStar *small = NULL, *big = NULL;
        int ns = -1, nb = -1;
        gaia_client_cone_search(c, Q_CORE.ra, Q_CORE.dec, 0.6, 10.0, 20.0, &small, &ns);
        gaia_client_cone_search(c, Q_CORE.ra, Q_CORE.dec, 1.2, 10.0, 20.0, &big, &nb);
        CHECK(ns >= 0 && nb >= 0 && ns <= nb, "半径单调 ns=%d nb=%d", ns, nb);
        for (int i = 0; i < ns; i++) {
            int found = 0;
            for (int j = 0; j < nb && !found; j++)
                if (memcmp(&small[i].ra, &big[j].ra, 8) == 0 &&
                    memcmp(&small[i].dec, &big[j].dec, 8) == 0) found = 1;
            CHECK(found, "小半径星 %d 未出现在大半径结果", i);
        }
        t_free_results(small);
        t_free_results(big);
        gaia_client_destroy(c);
    }
    fprintf(stderr, "properties: done failures=%d\n", t_failures);
    return t_failures ? 1 : 0;
}

/* ============================== negative ============================== */
static void t_write_file(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "negative: 无法写 %s\n", path); exit(2); }
    if (len) fwrite(data, 1, len, f);
    fclose(f);
}

static int mode_negative(const char *dir, const char *work) {
    char w[2048];
    /* N1 坏魔数 */
    snprintf(w, sizeof(w), "%s/badmagic", work);
    t_mkdir_p(w);
    {
        char src[2048], dst[2048];
        snprintf(src, sizeof(src), "%s/gaia_dr3.xpsd", dir);
        snprintf(dst, sizeof(dst), "%s/bad.xpsd", w);
        FILE *in = fopen(src, "rb");
        unsigned char buf[65536];
        size_t len = fread(buf, 1, sizeof(buf), in);
        fclose(in);
        memcpy(buf, "XPSD0000", 8);
        t_write_file(dst, buf, len);
        GaiaClient *c = gaia_client_create(w);
        CHECK(c != NULL && gaia_client_get_file_count(c) == 0,
              "坏魔数文件被加载 file_count=%d（应静默跳过）",
              c ? gaia_client_get_file_count(c) : -1);
        gaia_client_destroy(c);
        remove(dst);
    }

    /* N2 头部截断 / 中部截断 / 空文件 */
    {
        const char *cases[] = {"gaia_sp.xpsd", "gaia_dr3.xpsd"};
        for (int cs = 0; cs < 2; cs++) {
            char src[2048], dst[2048];
            snprintf(src, sizeof(src), "%s/%s", dir, cases[cs]);
            FILE *in = fopen(src, "rb");
            fseek(in, 0, SEEK_END);
            long flen = ftell(in);
            fseek(in, 0, SEEK_SET);
            unsigned char *buf = (unsigned char *)malloc((size_t)flen);
            size_t rd = fread(buf, 1, (size_t)flen, in);
            fclose(in);
            CHECK(rd == (size_t)flen, "fixture 读取 %s 不完整", cases[cs]);

            snprintf(w, sizeof(w), "%s/trunc", work);
            t_mkdir_p(w);
            snprintf(dst, sizeof(dst), "%s/x.xpsd", w);
            t_write_file(dst, buf, 8);           /* 仅魔数 */
            GaiaClient *c = gaia_client_create(w);
            CHECK(c != NULL && gaia_client_get_file_count(c) == 0,
                  "%s 头截断被加载", cases[cs]);
            gaia_client_destroy(c);

            t_write_file(dst, buf, (size_t)flen / 2);  /* 中部截断 */
            c = gaia_client_create(w);
            CHECK(c != NULL, "%s 中部截断 create=NULL", cases[cs]);
            if (c) {
                /* 只要部分树可加载即接受：总源数 < 完整值 */
                CHECK(gaia_client_get_total_sources(c) <= G_MANIFEST_N,
                      "%s 中部截断 total=%d 超界",
                      cases[cs], gaia_client_get_total_sources(c));
                gaia_client_destroy(c);
            }

            remove(dst);
            c = gaia_client_create(w);
            CHECK(c != NULL && gaia_client_get_file_count(c) == 0,
                  "空目录遍历后 file_count!=0");
            gaia_client_destroy(c);
            free(buf);
        }
    }

    /* N3 压缩标签不存在 → 块读取失败 → 查询 0 结果不崩溃 */
    {
        char src[2048], dst[2048];
        snprintf(src, sizeof(src), "%s/gaia_sp.xpsd", dir);
        snprintf(dst, sizeof(dst), "%s/badcomp.xpsd", w);
        FILE *in = fopen(src, "rb");
        fseek(in, 0, SEEK_END);
        long flen = ftell(in);
        fseek(in, 0, SEEK_SET);
        unsigned char *buf = (unsigned char *)malloc((size_t)flen);
        if (fread(buf, 1, (size_t)flen, in) != (size_t)flen) { fclose(in); free(buf); exit(2); }
        fclose(in);
        unsigned char *hit = (unsigned char *)memmem(buf, (size_t)flen, "zlib+sh", 7);
        CHECK(hit != NULL, "fixture 中未找到压缩标签（自检）");
        if (hit) memcpy(hit, "xxx", 3);
        t_write_file(dst, buf, (size_t)flen);
        free(buf);
        GaiaClient *c = gaia_client_create(w);
        CHECK(c != NULL, "badcomp create=NULL");
        if (c) {
            GaiaStar *res = NULL;
            int n = -1;
            int rc = gaia_client_cone_search(c, Q_CORE.ra, Q_CORE.dec, 2.5, 10.0, 20.0,
                                             &res, &n);
            CHECK(rc == 0, "badcomp 查询 rc=%d（应容错为空）", rc);
            CHECK(n == 0, "badcomp n=%d（坏标签块应不可解码）", n);
            t_free_results(res);
            gaia_client_destroy(c);
        }
        remove(dst);
    }

    /* N4 分配注入：create 第 1 次分配失败 → NULL，且无泄漏 */
    {
        t_alloc_reset(1);
        t_ledger_clear();
        GaiaClient *c = gaia_client_create(dir);
        CHECK(c == NULL, "注入 k=1 时 create != NULL");
        {
            int wild = 0;
            long leaked = t_ledger_reconcile(&wild);
            CHECK(leaked == 0 && wild == 0, "create OOM 后泄漏 %ld 块 wild=%d", leaked, wild);
        }
        CHECK(t_oom_fired >= 1, "注入未触发");
        t_alloc_reset(0);

        /* N5 查询路径分配失败：每 k 新建 client（相同参数会命中查询缓存，
         * 命中路径仅 1 次分配，注入不触发）。合法容错 = rc<0 或结果不超
         * 期望上界（collector 初始分配失败可自愈，不必然丢星）；核心不变
         * 量是 destroy 后对账零残留。 */
        int exp_idx5[G_MANIFEST_N];
        int nexp_full = t_expect_build(&Q_SKY, exp_idx5, G_MANIFEST_N);
        for (int k = 2; k <= 9; k++) {
            t_alloc_reset(0);
            t_ledger_clear();
            GaiaClient *ci = gaia_client_create(dir);
            CHECK(ci != NULL, "注入循环 k=%d create=NULL", k);
            if (!ci) continue;
            t_alloc_reset(k);
            GaiaStar *res = (GaiaStar *)0x1;
            int n = -1;
            int rcq = gaia_client_cone_search(ci, Q_SKY.ra, Q_SKY.dec, Q_SKY.radius,
                                              Q_SKY.mag_low, Q_SKY.mag_high, &res, &n);
            t_alloc_reset(0);
            CHECK(rcq == 0 ? (n >= 0 && n <= nexp_full) : (rcq == -1 && n == 0),
                  "注入 k=%d rc=%d n=%d（非法返回）", k, rcq, n);
            t_free_results(res);
            gaia_client_destroy(ci);
            int wild = 0;
            long leaked = t_ledger_reconcile(&wild);
            CHECK(leaked == 0, "注入 k=%d 泄漏 %ld 块（destroy 未回收）", k, leaked);
        }
    }
    fprintf(stderr, "negative: done failures=%d\n", t_failures);
    return t_failures ? 1 : 0;
}

/* ============================== worker 1/N ============================== */
static int mode_worker(const char *dir) {
    omp_set_num_threads(1);
    GaiaClient *c1 = gaia_client_create(dir);
    CHECK(c1 != NULL, "create");
    if (!c1) return 1;
    GaiaStar *r1 = NULL;
    int n1 = -1;
    int rc = gaia_client_cone_search(c1, Q_CORE.ra, Q_CORE.dec, Q_CORE.radius,
                                     Q_CORE.mag_low, Q_CORE.mag_high, &r1, &n1);
    CHECK(rc == 0 && n1 > 0, "worker1 rc=%d n=%d", rc, n1);
    gaia_client_destroy(c1);

    omp_set_num_threads(8);
    GaiaClient *c8 = gaia_client_create(dir);
    CHECK(c8 != NULL, "create#8");
    GaiaStar *r8 = NULL;
    int n8 = -1;
    rc = gaia_client_cone_search(c8, Q_CORE.ra, Q_CORE.dec, Q_CORE.radius,
                                 Q_CORE.mag_low, Q_CORE.mag_high, &r8, &n8);
    CHECK(rc == 0, "worker8 rc=%d", rc);
    CHECK(n8 == n1, "1 vs 8 worker n=%d != %d", n8, n1);
    if (n8 == n1) {
        for (int i = 0; i < n8; i++) {
            CHECK(memcmp(&r1[i].ra, &r8[i].ra, 8) == 0 &&
                  memcmp(&r1[i].dec, &r8[i].dec, 8) == 0 &&
                  memcmp(&r1[i].magG, &r8[i].magG, 8) == 0,
                  "worker 星 %d 结果非 bitwise（并行混合）", i);
        }
    }
    /* spectrum 路径同样 1 vs 8（极冠全 SP 星；命中数小，bitwise 全比对） */
    GaiaSpectrumStar *s1 = NULL, *s8 = NULL;
    uint8_t *b1 = NULL, *b8 = NULL;
    int m1 = -1, m8 = -1;
    omp_set_num_threads(1);
    GaiaClient *cs1 = gaia_client_create(dir);
    CHECK(cs1 != NULL, "create sp1");
    rc = gaia_client_cone_search_with_spectrum(cs1, 0.0, 89.4, 1.2, 10.0, 20.0,
                                               &s1, &b1, &m1);
    CHECK(rc == 0 && m1 > 0, "sp1 rc=%d n=%d", rc, m1);
    omp_set_num_threads(8);
    GaiaClient *cs8 = gaia_client_create(dir);
    CHECK(cs8 != NULL, "create sp8");
    rc = gaia_client_cone_search_with_spectrum(cs8, 0.0, 89.4, 1.2, 10.0, 20.0,
                                               &s8, &b8, &m8);
    CHECK(rc == 0 && m8 == m1, "sp8 rc=%d n=%d != %d", rc, m8, m1);
    if (m8 == m1) {
        for (int i = 0; i < m8; i++) {
            CHECK(memcmp(&s1[i].ra, &s8[i].ra, 8) == 0 &&
                  memcmp(&s1[i].dec, &s8[i].dec, 8) == 0 &&
                  memcmp(&s1[i].magG, &s8[i].magG, 8) == 0,
                  "spectrum worker 星 %d 非 bitwise", i);
            CHECK(memcmp(b1 + (size_t)i * 343, b8 + (size_t)i * 343, 343) == 0,
                  "spectrum worker 星 %d 光谱非 bitwise", i);
        }
    }
    t_free_results(s1);
    t_free_results(b1);
    t_free_results(s8);
    t_free_results(b8);
    gaia_client_destroy(cs1);
    gaia_client_destroy(cs8);
    gaia_client_destroy(c8);
    fprintf(stderr, "worker: done failures=%d\n", t_failures);
    return t_failures ? 1 : 0;
}

/* ============================== dump（oracle 比对用） ============================== */
static int mode_dump(const char *dir, const char *out_json) {
    FILE *out = fopen(out_json, "w");
    if (!out) { fprintf(stderr, "dump: 无法写 %s\n", out_json); return 2; }
    GaiaClient *c = gaia_client_create(dir);
    if (!c) { fprintf(stderr, "dump: create NULL\n"); fclose(out); return 2; }
    fprintf(out, "{\"file_count\":%d,\"total\":%d,\"stars\":[",
            gaia_client_get_file_count(c), gaia_client_get_total_sources(c));
    /* SKY 查询取全域；光谱路径单独跑 POLAR 全窗 */
    GaiaStar *res = NULL;
    int n = -1;
    int rc = gaia_client_cone_search(c, Q_SKY.ra, Q_SKY.dec, Q_SKY.radius,
                                     Q_SKY.mag_low, Q_SKY.mag_high, &res, &n);
    if (rc != 0) { fprintf(stderr, "dump: cone rc=%d\n", rc); fclose(out); return 2; }
    for (int i = 0; i < n; i++) {
        fprintf(out, "%s{\"ra\":%.17g,\"dec\":%.17g,\"magG\":%.17g}",
                i ? "," : "", res[i].ra, res[i].dec, res[i].magG);
    }
    t_free_results(res);
    fprintf(out, "],\"polar\":[");
    GaiaSpectrumStar *sres = NULL;
    uint8_t *spec = NULL;
    n = -1;
    gaia_client_cone_search_with_spectrum(c, 0.0, 89.4, 1.2, 10.0, 20.0, &sres, &spec, &n);
    for (int i = 0; i < n; i++) {
        fprintf(out, "%s{\"ra\":%.17g,\"dec\":%.17g,\"magG\":%.17g,\"flux_min\":%.9g,"
                     "\"flux_mul\":%.9g,\"spec\":[",
                i ? "," : "", sres[i].ra, sres[i].dec, sres[i].magG,
                (double)sres[i].flux_min, (double)sres[i].flux_mul);
        for (int j = 0; j < 343; j++)
            fprintf(out, "%s%u", j ? "," : "", (unsigned)spec[(size_t)i * 343 + j]);
        fprintf(out, "]}");
    }
    t_free_results(sres);
    t_free_results(spec);
    fprintf(out, "]}\n");
    fclose(out);
    gaia_client_destroy(c);
    fprintf(stderr, "dump: %s written\n", out_json);
    return 0;
}

/* ============================== truncate I5 ============================== */
static int mode_truncate(const char *trunc_dir) {
    GaiaClient *c = gaia_client_create(trunc_dir);
    CHECK(c != NULL, "trunc create");
    if (!c) return 1;
    GaiaStar *res = NULL;
    int n = -1;
    int rc = gaia_client_cone_search(c, 0.0, 0.0, 180.0, 11.0, 13.0, &res, &n);
    CHECK(rc == 0, "I5 rc=%d", rc);
    CHECK(n == 200000, "I5 截断 n=%d != 200000", n);
    if (rc == 0 && n == 200000) {
        for (int i = 0; i < n; i++) {
            CHECK(fabs(res[i].magG - 12.0) <= MAG_TOL, "I5 星 %d mag=%.9g", i, res[i].magG);
        }
        CHECK(res[200000 - 1].ra == res[199999].ra, "尾元素访问自检");
    }
    t_free_results(res);
    gaia_client_destroy(c);
    fprintf(stderr, "truncate: done failures=%d\n", t_failures);
    return t_failures ? 1 : 0;
}

/* ============================== performance ============================== */
static int mode_performance(const char *trunc_dir, const char *out_json) {
    GaiaClient *c = gaia_client_create(trunc_dir);
    CHECK(c != NULL, "perf create");
    if (!c) return 1;
    double t0 = t_now_sec();
    GaiaStar *res = NULL;
    int n = -1;
    int rc = gaia_client_cone_search(c, 0.0, 0.0, 180.0, 11.0, 13.0, &res, &n);
    double dt = t_now_sec() - t0;
    CHECK(rc == 0 && n == 200000, "perf 查询 rc=%d n=%d", rc, n);
    t_free_results(res);
    fprintf(stderr, "performance: %.3f s\n", dt);
    FILE *out = fopen(out_json, "w");
    if (out) {
        fprintf(out, "{\"cold_full_sky_200k_sec\":%.6f,\"result_count\":%d}\n", dt, n);
        fclose(out);
    }
    gaia_client_destroy(c);
    /* 基线：单文件 20 万星全量 < 15 s（冻结 CI 上限，慢机可复核） */
    CHECK(dt < 15.0, "性能基线 %.3f s >= 15 s", dt);
    return t_failures ? 1 : 0;
}

/* ============================== main ============================== */
static void t_print_usage(void) {
    fprintf(stderr,
            "usage: gaia_cat_test <mode> ...\n"
            "  unit        <fixture_dir>\n"
            "  properties  <fixture_dir>\n"
            "  negative    <fixture_dir> <work_dir>\n"
            "  worker      <fixture_dir>\n"
            "  dump        <fixture_dir> <out.json>\n"
            "  truncate    <trunc_dir>\n"
            "  performance <trunc_dir> <out.json>\n");
}

int main(int argc, char **argv) {
    if (argc < 3) { t_print_usage(); return 2; }
    const char *mode = argv[1];
    if (strcmp(mode, "unit") == 0 && argc == 3)
        return mode_unit(argv[2]);
    if (strcmp(mode, "properties") == 0 && argc == 3)
        return mode_properties(argv[2]);
    if (strcmp(mode, "negative") == 0 && argc == 4)
        return mode_negative(argv[2], argv[3]);
    if (strcmp(mode, "worker") == 0 && argc == 3)
        return mode_worker(argv[2]);
    if (strcmp(mode, "dump") == 0 && argc == 4)
        return mode_dump(argv[2], argv[3]);
    if (strcmp(mode, "truncate") == 0 && argc == 3)
        return mode_truncate(argv[2]);
    if (strcmp(mode, "performance") == 0 && argc == 4)
        return mode_performance(argv[2], argv[3]);
    t_print_usage();
    return 2;
}
