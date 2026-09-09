/* adapter_test.c - astrocs.p1.hips_writer 模块 C ABI v1 adapter 契约测试
 *
 * 对齐先例: tests/unit/phase1_session/module_adapter_test.c (九操作)
 *          lib/cosmetic/tests/adapter_budget_test.c (BUDGET/租约)
 *          lib/gaia_xpsd_client/tests/adapter_export_test.c (导出面探针)
 *          tests/unit/p1hips/* (BITWISE 对拍基线, p1hips_writer)
 *
 * 覆盖面 (S-4):
 *   1. 事务写路径端到端 (direct aio_hips_writer 与 adapter 生产产物逐字节一致;
 *      p1hips BITWISE 基线保持全绿)
 *   2. plan() 真实 work_units (禁空转; 单事务串行 max_workers=1)
 *   3. host executor 租约: 缺失/不足 → ACS_ERR_BUDGET detail 105 (禁私建线程)
 *   4. entry cancel / mid-transaction cancel → 事务化 abort (半成品树移除)
 *   5. 导出面: nm -D 唯一 astrocs_module_query_v1; dlsym legacy 九符号全 NULL
 *   6. host allocator 分配失败负面用例 (批次 R 57 处 malloc hardening 合同面)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>

#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/hips/types.h"
#include "aio_hips.h"

static int g_fail = 0;
static char g_case[128] = "(init)";

#define EXPECT(cond) do { \
    if (!(cond)) { \
        printf("FAIL [%s] %s:%d: %s\n", g_case, __FILE__, __LINE__, #cond); \
        g_fail++; \
    } \
} while (0)

#define EXPECT_STR(hay, needle) do { \
    if (!(hay) || !strstr((hay), (needle))) { \
        printf("FAIL [%s] %s:%d: \"%s\" missing \"%s\"\n", \
               g_case, __FILE__, __LINE__, (hay) ? (hay) : "(null)", (needle)); \
        g_fail++; \
    } \
} while (0)

static void set_case(const char* c) {
    snprintf(g_case, sizeof(g_case), "%s", c);
    printf("case: %s\n", c);
}

/* ═════════ host api 构造 ═════════ */

static void* halloc(void* ud, uint64_t size, uint64_t align) {
    if (ud) return NULL;               /* fail_user_data != 0 → 恒失败 */
    (void)align;
    return size ? malloc((size_t)size) : NULL;
}
static void hfree(void* ud, void* p) {
    (void)ud;
    free(p);
}

static int exec_acquire_ok(void* ud, uint32_t n) { (void)ud; (void)n; return 0; }
static int exec_acquire_fail(void* ud, uint32_t n) { (void)ud; (void)n; return 1; }
static void exec_release_ok(void* ud, uint32_t n) { (void)ud; (void)n; }

static int exec_cancelled(void* ud) { return ud ? 1 : 0; }

static const acs_allocator_v1 k_alloc = {
    { (uint32_t)sizeof(acs_allocator_v1), ACS_ABI_VERSION_V1 },
    halloc, hfree, NULL
};
static const acs_executor_v1 k_exec_ok = {
    { (uint32_t)sizeof(acs_executor_v1), ACS_ABI_VERSION_V1 },
    4, 2, exec_acquire_ok, exec_release_ok, NULL
};
static const acs_executor_v1 k_exec_fail = {
    { (uint32_t)sizeof(acs_executor_v1), ACS_ABI_VERSION_V1 },
    4, 2, exec_acquire_fail, exec_release_ok, NULL
};
static const acs_cancel_v1 k_cancel = {
    { (uint32_t)sizeof(acs_cancel_v1), ACS_ABI_VERSION_V1 },
    exec_cancelled, (void*)1
};

static void host_fill(acs_host_api_v1* h, const acs_allocator_v1* al,
                      const acs_executor_v1* ex, const acs_cancel_v1* ca) {
    memset(h, 0, sizeof(*h));
    h->head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    h->head.abi_version = ACS_ABI_VERSION_V1;
    h->allocator = al;
    h->executor = ex;
    h->cancel = ca;
}

/* ═════════ b64 编码 (测试侧; RFC 4648) ═════════ */

static const char kB64Tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static uint64_t b64_encoded_len(uint64_t n) {
    return ((n + 2) / 3) * 4;
}

static void b64_encode(const uint8_t* src, uint64_t n, char* dst) {
    uint64_t i = 0, o = 0;
    while (i + 3 <= n) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8) |
                     src[i + 2];
        dst[o++] = kB64Tab[(v >> 18) & 63];
        dst[o++] = kB64Tab[(v >> 12) & 63];
        dst[o++] = kB64Tab[(v >> 6) & 63];
        dst[o++] = kB64Tab[v & 63];
        i += 3;
    }
    uint64_t rem = n - i;
    if (rem == 1) {
        uint32_t v = (uint32_t)src[i] << 16;
        dst[o++] = kB64Tab[(v >> 18) & 63];
        dst[o++] = kB64Tab[(v >> 12) & 63];
        dst[o++] = '=';
        dst[o++] = '=';
    } else if (rem == 2) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8);
        dst[o++] = kB64Tab[(v >> 18) & 63];
        dst[o++] = kB64Tab[(v >> 12) & 63];
        dst[o++] = kB64Tab[(v >> 6) & 63];
        dst[o++] = '=';
    }
    dst[o] = '\0';
}

/* ═════════ 测试数据 (直接通道; 与 BITWISE 基线同款确定性生成) ═════════ */

/* direct 与 adapter 共用 4 tile 序列。合法域实证 (writer:456-459,424):
 * tile_order = leaf_order - 9; nside=512 → leaf_order=9 → tile_order=0 →
 * parent_ipix ∈ 0..11 (Norder0 12 像素); 取南北混合 4 像素。
 * 对拍成立条件 = 两通道 consume 同一 tile 数据流。 */
static const uint64_t kParents[4] = {
    0, 1, 6, 11
};

typedef struct {
    AioHipsProductSet* ps;
    char out_dir[512];
    uint32_t nside;
    uint32_t width;
    int data_type;
    int32_t flags;
} direct_env;

static int direct_begin(direct_env* e, const char* out_dir) {
    memset(e, 0, sizeof(*e));
    snprintf(e->out_dir, sizeof(e->out_dir), "%s", out_dir);
    e->nside = 512;
    e->width = 512;
    e->data_type = AIO_HIPS_FLOAT32;
    e->flags = AIO_HIPS_PRODUCT_ALL_V19;
    e->ps = aio_hips_product_begin(out_dir, 512, 512, AIO_HIPS_FLOAT32,
                                   AIO_HIPS_PRODUCT_ALL_V19,
                                   "ivo://astrocs/phase1",
                                   "HIPS adapter channel (P1-HIPS-IMPL adapter "
                                   "test)", NULL, 1200.0, "2026-02-11T00:00:00Z",
                                   0);
    return e->ps != NULL;
}

/* 单 tile 平面生成 (float32 通道):
 *   mask[i]      = (i % 3 != 0)
 *   flux[i]      = mask ? (2.0f + 100i) : NaN
 *   coverage[i]  = (0.25f + 10i)   (全像素; writer 按 mask 跳过)
 *   var_num[i]   = (0.5f + 7i)
 * SNR tile (point_count=256): ra=6i, dec=-60+0.1i, snr=20+0.01i, id=i,
 *   qf=100, st=7; provenance: pixfrac=0.3+0.01t, scale=0.6+0.02t。 */
static int direct_tiles(direct_env* e, int n_tiles, int t_base) {
    const uint64_t tile_elems = (uint64_t)e->width * (uint64_t)e->width;
    for (int t = 0; t < n_tiles; ++t) {
        uint8_t* mask = (uint8_t*)malloc((size_t)tile_elems);
        float* flux = (float*)malloc((size_t)tile_elems * 4);
        float* cov = (float*)malloc((size_t)tile_elems * 4);
        float* var = (float*)malloc((size_t)tile_elems * 4);
        if (!mask || !flux || !cov || !var) {
            free(mask); free(flux); free(cov); free(var);
            return 0;
        }
        for (uint64_t i = 0; i < tile_elems; ++i) {
            mask[i] = (uint8_t)((i % 3 != 0) ? 1 : 0);
            flux[i] = mask[i] ? (2.0f + (float)(100 + i)) : NAN;
            cov[i] = 0.25f + (float)(10 + i);
            var[i] = 0.5f + (float)(7 + i);
        }
        AstroSphereTileView v;
        memset(&v, 0, sizeof(v));
        v.parent_ipix = kParents[t];
        v.leaf_order = 9;
        v.width = e->width;
        v.data_type = e->data_type;
        v.flux_sum = flux;
        v.covered_area = cov;
        v.valid_mask = mask;
        v.var_num_sum = var;
        int rc = aio_hips_write_signal_support_tile(e->ps, &v);
        int rc2 = rc == 0 ? aio_hips_write_variance_tile(e->ps, &v) : 0;
        free(mask); free(flux); free(cov); free(var);
        if (rc != 0 || rc2 != 0) {
            printf("  direct_tiles t=%d parent=%llu rc=%d rc2=%d last=%s\n",
                   t, (unsigned long long)kParents[t], rc, rc2,
                   aio_hips_last_error() ? aio_hips_last_error() : "-");
            return 0;
        }
    }
    return 1;
}

static int direct_snr(direct_env* e) {
    AioHipsSnrPoint pts[256];
    for (int i = 0; i < 256; ++i) {
        pts[i].ra_deg = 6.0 * (double)i;
        pts[i].dec_deg = -60.0 + 0.1 * (double)i;
        pts[i].snr = 20.0 + 0.01 * (double)i;
        pts[i].star_id = (int64_t)i;
        pts[i].quality_flags = 100;
        pts[i].photometric_status = 7;
    }
    return aio_hips_write_snr_points(e->ps, pts, 256) == 0;
}

static int direct_prov(direct_env* e, int t) {
    return aio_hips_set_drizzle_provenance(e->ps, 0.3 + 0.01 * (double)t,
                                           0.6 + 0.02 * (double)t) == 0;
}

/* ═════════ adapter 通道: manifest 构造 ═════════ */

typedef struct {
    uint8_t* mask;     /* tile_elems */
    uint8_t* flux;     /* tile_elems*4 */
    uint8_t* cov;      /* tile_elems*4 */
    uint8_t* var;      /* tile_elems*4 */
    uint8_t* has_bits; /* n_tiles */
} tile_bytes;

static void tile_bytes_free(tile_bytes* b) {
    free(b->mask); free(b->flux); free(b->cov); free(b->var); free(b->has_bits);
    memset(b, 0, sizeof(*b));
}

/* 与 direct_tiles 逐字节同款生成 (tiles_only=0: 全通道含 SNR+prov) */
static int tile_bytes_build(tile_bytes* b, uint32_t width, int n_tiles,
                            int tiles_only) {
    memset(b, 0, sizeof(*b));
    const uint64_t tile_elems = (uint64_t)width * (uint64_t)width;
    b->mask = (uint8_t*)malloc((size_t)(n_tiles * tile_elems));
    b->flux = (uint8_t*)malloc((size_t)(n_tiles * tile_elems * 4));
    b->cov = (uint8_t*)malloc((size_t)(n_tiles * tile_elems * 4));
    b->var = (uint8_t*)malloc((size_t)(n_tiles * tile_elems * 4));
    b->has_bits = (uint8_t*)malloc((size_t)n_tiles);
    if (!b->mask || !b->flux || !b->cov || !b->var || !b->has_bits) {
        tile_bytes_free(b);
        return 0;
    }
    for (int t = 0; t < n_tiles; ++t) {
        b->has_bits[t] = 3;   /* bit0 mask=1, bit1 var=1 (全 tile) */
        for (uint64_t i = 0; i < tile_elems; ++i) {
            uint64_t o = (uint64_t)t * tile_elems + i;
            uint8_t m = (uint8_t)((i % 3 != 0) ? 1 : 0);
            b->mask[o] = m;
            float fv = m ? (2.0f + (float)(100 + i)) : NAN;
            float cv = 0.25f + (float)(10 + i);
            float vv = 0.5f + (float)(7 + i);
            memcpy(&b->flux[o * 4], &fv, 4);
            memcpy(&b->cov[o * 4], &cv, 4);
            memcpy(&b->var[o * 4], &vv, 4);
        }
    }
    (void)tiles_only;
    return 1;
}

/* manifest 组装 (顶层平铺 v1; has bits + 6 SoA SNR 平面 + provenance) */
static int manifest_build(char* buf, size_t cap, const tile_bytes* b,
                          int n_tiles, uint32_t width, int leaf_order,
                          const char* out_dir, const uint64_t* parent_ipix) {
    const uint64_t tile_elems = (uint64_t)width * (uint64_t)width;
    const uint64_t mask_len = (uint64_t)n_tiles * tile_elems;
    const uint64_t f32_len = mask_len * 4;
    char* p = buf;
    p += snprintf(p, (size_t)(buf + cap - p),
        "{\"op\":\"write_product\",\"out_dir\":\"%s\",\"nside\":512,"
        "\"tile_width\":%u,\"data_type\":0,\"flags\":31,"
        "\"creator_did\":\"ivo://astrocs/phase1\","
        "\"obs_title\":\"HIPS adapter channel (P1-HIPS-IMPL adapter test)\","
        "\"obs_filter\":null,\"exposure_s\":1200.0,"
        "\"obs_date\":\"2026-02-11T00:00:00Z\",\"moc_order\":0,"
        "\"max_workers\":0,"
        "\"n_tiles\":%d,\"width\":%u,\"leaf_order\":%d,"
        "\"parent_ipix\":[", out_dir, width, n_tiles, width, leaf_order);
    for (int t = 0; t < n_tiles; ++t)
        p += snprintf(p, (size_t)(buf + cap - p), "%s%llu", t ? "," : "",
                      (unsigned long long)parent_ipix[t]);
    p += snprintf(p, (size_t)(buf + cap - p), "%s", "],");

    /* enc 容量按最大平面 (f32_len 字节) 计, 非 u8 位图平面 (越界即堆腐蚀) */
    size_t need = b64_encoded_len(f32_len) + 1;
    char* enc = (char*)malloc(need);
    if (!enc) return 0;

    p += snprintf(p, (size_t)(buf + cap - p), "\"flux_base64\":\"");
    b64_encode(b->flux, f32_len, enc);
    p += snprintf(p, (size_t)(buf + cap - p), "%s\",", enc);

    p += snprintf(p, (size_t)(buf + cap - p), "\"coverage_base64\":\"");
    b64_encode(b->cov, f32_len, enc);
    p += snprintf(p, (size_t)(buf + cap - p), "%s\",", enc);

    p += snprintf(p, (size_t)(buf + cap - p), "\"has_mask_base64\":\"");
    b64_encode(b->has_bits, (uint64_t)n_tiles, enc);
    p += snprintf(p, (size_t)(buf + cap - p), "%s\",", enc);

    p += snprintf(p, (size_t)(buf + cap - p), "\"valid_mask_base64\":\"");
    b64_encode(b->mask, mask_len, enc);
    p += snprintf(p, (size_t)(buf + cap - p), "%s\",", enc);

    p += snprintf(p, (size_t)(buf + cap - p), "\"has_variance_base64\":\"");
    b64_encode(b->has_bits, (uint64_t)n_tiles, enc);
    p += snprintf(p, (size_t)(buf + cap - p), "%s\",", enc);

    p += snprintf(p, (size_t)(buf + cap - p), "\"var_num_base64\":\"");
    b64_encode(b->var, f32_len, enc);
    p += snprintf(p, (size_t)(buf + cap - p), "%s\",", enc);

    /* SNR SoA 六平面 (256 点) */
    double ra[256], dec[256], sv[256];
    int64_t sid[256];
    uint32_t qf[256], stt[256];
    for (int i = 0; i < 256; ++i) {
        ra[i] = 6.0 * (double)i;
        dec[i] = -60.0 + 0.1 * (double)i;
        sv[i] = 20.0 + 0.01 * (double)i;
        sid[i] = (int64_t)i;
        qf[i] = 100;
        stt[i] = 7;
    }
    p += snprintf(p, (size_t)(buf + cap - p), "\"snr_count\":256,");
    struct { const char* key; const void* p; uint64_t n; } sp[] = {
        { "snr_ra_base64",   ra,  256 * 8 },
        { "snr_dec_base64",  dec, 256 * 8 },
        { "snr_val_base64",  sv,  256 * 8 },
        { "snr_star_id_base64", sid, 256 * 8 },
        { "snr_qflags_base64", qf, 256 * 4 },
        { "snr_status_base64", stt, 256 * 4 },
    };
    for (int i = 0; i < 6; ++i) {
        b64_encode((const uint8_t*)sp[i].p, sp[i].n, enc);
        p += snprintf(p, (size_t)(buf + cap - p), "\"%s\":\"%s\"%s",
                      sp[i].key, enc, i < 5 ? "," : "");
    }
    p += snprintf(p, (size_t)(buf + cap - p),
                  ",\"prov_pixfrac\":0.3,\"prov_scale_arcsec\":0.6}");
    free(enc);
    return 1;
}

/* ═════════ 文件树断言 ═════════ */

static long long file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long long)st.st_size;
}

static void hips_mkdir_p(const char* path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* q = tmp + 1; *q; ++q) {
        if (*q == '/') {
            *q = '\0';
            mkdir(tmp, 0777);
            *q = '/';
        }
    }
    mkdir(tmp, 0777);
}

/* BITWISE 对拍键 (实际 HiPS 树布局, aio_hips_writer 冻结面):
 *   out_dir/manifest.json (P1-HIPS-TEST I8 基线) +
 *   {signal,support,variance,ivar,snr}/properties +
 *   {signal,support,variance,ivar,snr}/Moc.fits +
 *   {signal,support,variance,ivar}/metadata.fits + snr/metadata.xml +
 *   各 Norder 子目录内 tile 文件 (writer 层次输出, 集合相等 + size 相等)。
 * 对拍方式 = 递归目录列表 (文件集合相等) + 每文件 size 相等。 */
static void list_dir_rec(const char* base, const char* rel,
                         char*** list, int* n, int* cap) {
    char full[1024];
    if (rel[0]) snprintf(full, sizeof(full), "%s/%s", base, rel);
    else snprintf(full, sizeof(full), "%s", base);
    DIR* d = opendir(full);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char nrel[1024];
        snprintf(nrel, sizeof(nrel), "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
        char nfull[1024];
        snprintf(nfull, sizeof(nfull), "%s/%s", base, nrel);
        struct stat st;
        if (stat(nfull, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            list_dir_rec(base, nrel, list, n, cap);
        } else {
            if (*n >= *cap) {
                *cap = *cap ? *cap * 2 : 64;
                *list = (char**)realloc(*list, (size_t)*cap * sizeof(char*));
            }
            (*list)[*n] = strdup(nrel);
            (*n)++;
        }
    }
    closedir(d);
}

static int cmp_str(const void* a, const void* b) {
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static void assert_tree_identical(const char* a, const char* b) {
    char** la = NULL; int na = 0, ca = 0;
    char** lb = NULL; int nb = 0, cb = 0;
    list_dir_rec(a, "", &la, &na, &ca);
    list_dir_rec(b, "", &lb, &nb, &cb);
    qsort(la, (size_t)na, sizeof(char*), cmp_str);
    qsort(lb, (size_t)nb, sizeof(char*), cmp_str);
    EXPECT(na > 0);
    EXPECT(na == nb);
    int n_min = na < nb ? na : nb;
    for (int i = 0; i < n_min; ++i) {
        if (strcmp(la[i], lb[i]) != 0) {
            printf("FAIL [%s] file set differs at %d: %s vs %s\n",
                   g_case, i, la[i], lb[i]);
            g_fail++;
            continue;
        }
        char pa[1024], pb[1024];
        snprintf(pa, sizeof(pa), "%s/%s", a, la[i]);
        snprintf(pb, sizeof(pb), "%s/%s", b, la[i]);
        long long sa = file_size(pa), sb = file_size(pb);
        if (sa != sb) {
            printf("FAIL [%s] size differs: %s (%lld vs %lld)\n",
                   g_case, la[i], sa, sb);
            g_fail++;
        }
    }
    for (int i = 0; i < na; ++i) free(la[i]);
    for (int i = 0; i < nb; ++i) free(lb[i]);
    free(la); free(lb);
}

static void assert_tree_absent(const char* root) {
    char p[1024];
    snprintf(p, sizeof(p), "%s/manifest.json", root);
    EXPECT(file_size(p) < 0);           /* 未 finalize → 树 manifest 缺失 */
}

/* ═════════ manifest 输出 JSON 探针 ═════════ */

static int json_int_field(const char* s, const char* key, long long* out) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char* p = strstr(s, pat);
    if (!p) return 0;
    p += strlen(pat);
    *out = strtoll(p, NULL, 10);
    return 1;
}

/* ═════════ case 1/2: direct 通道与 adapter 通道共用 4 tile 序列 ═════════
 * 对拍成立条件 = 两通道 consume 同一 tile 数据流 (determinism 基线):
 *   parent_ipix 见 kParents (order 9 交错), 平面逐字节同款生成,
 *   SNR 256 点同款, provenance 每次调用 t 递增。 */

/* case 1: 直接通道参考产物 (legacy aio_hips_* 九导出面) */
static void run_direct_reference(const char* root) {
    set_case("direct_reference");
    direct_env e;
    EXPECT(direct_begin(&e, root));
    EXPECT(direct_tiles(&e, 4, 0));            /* t 基准 0 (prov 序列同) */
    EXPECT(direct_snr(&e));
    EXPECT(direct_prov(&e, 0));   /* 仅一次 → 0.31/0.62, 与 adapter manifest 0.3+0.01 对齐 */
    (void)e;
    EXPECT(aio_hips_finalize(e.ps) == 0);
    char p[1024];
    snprintf(p, sizeof(p), "%s/manifest.json", root);
    EXPECT(file_size(p) > 0);
    snprintf(p, sizeof(p), "%s/signal/Norder0/Dir0/Npix0.fits", root);
    EXPECT(file_size(p) > 0);
}

/* case 2: adapter 端到端 (9 操作全序; 与 case 1 同一 tile 流) */

static void run_adapter_full(const char* root, const char* ref_dir) {
    set_case("adapter_full_lifecycle");
    const acs_module_api_v1* api = NULL;
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);
    EXPECT(api != NULL);

    char mid[] = ASTROCS_HIPS_MODULE_ID;
    acs_module_descriptor_v1 desc;
    memset(&desc, 0, sizeof(desc));
    EXPECT(api->describe(api, (acs_str_v1){ sizeof(acs_str_v1),
                                           ACS_ABI_VERSION_V1, mid, strlen(mid) },
                         &desc) == ACS_OK);
    EXPECT_STR(desc.module_id.data, "astrocs.p1.hips_writer");
    EXPECT(desc.execution_class == 0);          /* cpu_heavy */
    EXPECT(desc.parallel_ok == 0);              /* 单事务串行 (HI-3) */
    EXPECT(desc.phase == 1);

    /* validate_config: 512/5 非法组合 (HI-1) */
    char bad_cfg[] = "{\"op\":\"write_product\",\"out_dir\":\"x\",\"nside\":512,"
                     "\"tile_width\":5,\"data_type\":0,\"flags\":7}";
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status vst = api->validate_config(
        api, (acs_str_v1){ sizeof(acs_str_v1), ACS_ABI_VERSION_V1,
                           bad_cfg, strlen(bad_cfg) }, &err);
    EXPECT(vst == ACS_ERR_PARAM);
    EXPECT_STR(err.message_utf8, "tile_width");
    EXPECT(err.detail_code == 101);   /* HIPS_ECODE_BAD_VALUE */

    char cfg[] = "{\"op\":\"write_product\",\"out_dir\":\"PLACEHOLDER\","
                 "\"nside\":512,\"tile_width\":512,\"data_type\":0,\"flags\":31,"
                 "\"creator_did\":\"ivo://astrocs/phase1\","
                 "\"obs_title\":\"HIPS adapter channel (P1-HIPS-IMPL adapter "
                 "test)\",\"exposure_s\":1200.0,"
                 "\"obs_date\":\"2026-02-11T00:00:00Z\",\"moc_order\":0,"
                 "\"max_workers\":0}";
    char cfg_dir[512];
    snprintf(cfg_dir, sizeof(cfg_dir), "%s", root);
    char cfg_buf[640];
    {
        char* q = strstr(cfg, "PLACEHOLDER");
        snprintf(cfg_buf, sizeof(cfg_buf), "%.*s%s%s",
                 (int)(q - cfg), cfg, cfg_dir, q + strlen("PLACEHOLDER"));
    }

    acs_module_instance_v1* inst = NULL;
    EXPECT(api->create(api,
                       (acs_str_v1){ sizeof(acs_str_v1), ACS_ABI_VERSION_V1,
                                     cfg_buf, strlen(cfg_buf) },
                       &host, &inst, &err) == ACS_OK);
    EXPECT(inst != NULL);

    /* plan: 真实 work_units (单事务串行) */
    char nbuf[2048];
    acs_strbuf_v1 pb = { sizeof(acs_strbuf_v1), ACS_ABI_VERSION_V1,
                         nbuf, sizeof(nbuf), 0 };
    acs_status pst = api->plan(api,
                               (acs_str_v1){ sizeof(acs_str_v1),
                                             ACS_ABI_VERSION_V1, mid,
                                             strlen(mid) },
                               (acs_str_v1){ sizeof(acs_str_v1),
                                             ACS_ABI_VERSION_V1, cfg_buf,
                                             strlen(cfg_buf) },
                               &pb, &err);
    EXPECT(pst == ACS_OK);
    nbuf[pb.size < sizeof(nbuf) ? pb.size : sizeof(nbuf) - 1] = '\0';
    EXPECT_STR(nbuf, "\"work_units\":1");
    EXPECT_STR(nbuf, "\"max_workers\":1");
    EXPECT_STR(nbuf, "\"plan_version\":1");
    EXPECT_STR(nbuf, "\"nside\":512");
    EXPECT_STR(nbuf, "\"memory_hierarchy_accumulator_bytes\":");
    EXPECT_STR(nbuf, "\"io_out_estimate_bytes\":");

    /* execute: 4 tile adapter 通道 → 全量产物 (与 direct 参考同一 tile 流;
     * manifest ≈ 24MB 静态缓冲: 3×(4·W²·4B) b64 ≈ 17MB 峰值) */
    tile_bytes tb;
    EXPECT(tile_bytes_build(&tb, 512, 4, 0));
    static char manifest[26 * 1024 * 1024];
    EXPECT(manifest_build(manifest, sizeof(manifest), &tb, 4, 512, 9, root,
                          kParents));
    tile_bytes_free(&tb);

    static char obuf[4096];
    acs_strbuf_v1 ob = { sizeof(acs_strbuf_v1), ACS_ABI_VERSION_V1,
                         obuf, sizeof(obuf), 0 };
    acs_status est = api->execute(inst,
                                  (acs_str_v1){ sizeof(acs_str_v1),
                                                ACS_ABI_VERSION_V1, manifest,
                                                strlen(manifest) },
                                  (acs_str_v1){ sizeof(acs_str_v1),
                                                ACS_ABI_VERSION_V1, cfg_buf,
                                                strlen(cfg_buf) },
                                  &ob, &err);
    EXPECT(est == ACS_OK);
    obuf[ob.size < sizeof(obuf) ? ob.size : sizeof(obuf) - 1] = '\0';
    EXPECT_STR(obuf, "\"n_tiles\":4");
    EXPECT_STR(obuf, "\"n_tiles_written\":4");
    EXPECT_STR(obuf, "\"n_tiles_variance\":4");
    EXPECT_STR(obuf, "\"n_snr_points\":256");
    EXPECT_STR(obuf, "\"workers\":1");
    EXPECT(strstr(obuf, "signal") != NULL);
    EXPECT(strstr(obuf, "support") != NULL);
    EXPECT(strstr(obuf, "snr") != NULL);
    EXPECT(strstr(obuf, "variance") != NULL);

    /* BITWISE: adapter 产物 vs direct 参考产物 (order_9 4 tile) */
    assert_tree_identical(ref_dir, root);

    /* inspect */
    static char ibuf[2048];
    acs_strbuf_v1 ib = { sizeof(acs_strbuf_v1), ACS_ABI_VERSION_V1,
                         ibuf, sizeof(ibuf), 0 };
    EXPECT(api->inspect(inst, &ib, &err) == ACS_OK);
    ibuf[ib.size < sizeof(ibuf) ? ib.size : sizeof(ibuf) - 1] = '\0';
    EXPECT_STR(ibuf, "\"exec_count\":1");
    EXPECT_STR(ibuf, "\"legacy_last_code\":0");
    EXPECT_STR(ibuf, "\"cancel_support\":\"entry\"");

    /* cancel 后 execute → ACS_ERR_STATE (请求已置位) */
    EXPECT(api->request_cancel(inst) == ACS_OK);
    acs_status cst = api->execute(inst,
                                  (acs_str_v1){ sizeof(acs_str_v1),
                                                ACS_ABI_VERSION_V1, manifest,
                                                strlen(manifest) },
                                  (acs_str_v1){ sizeof(acs_str_v1),
                                                ACS_ABI_VERSION_V1, cfg_buf,
                                                strlen(cfg_buf) },
                                  &ob, &err);
    EXPECT(cst == ACS_ERR_STATE);

    api->destroy(inst);
    api->destroy(NULL);   /* inst=NULL 空操作 */
}

/* ═════════ case 3: allocator 分配失败 (create 拒绝) ═════════ */

static void run_alloc_fail(void) {
    set_case("alloc_fail");
    const acs_module_api_v1* api = NULL;
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);

    /* create 实证契约 (module_entry.cpp): !host || !host->allocator →
     * ACS_ERR_ABI_MISMATCH (NULL_CALLBACK 域) + inst NULL; inst 本体由
     * calloc 分配, 不走 host allocator → allocator 缺失即 ABI 拒绝。 */
    acs_host_api_v1 fh = host;
    fh.allocator = NULL;

    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    char cfg[] = "{\"op\":\"write_product\",\"out_dir\":\"x\",\"nside\":512,"
                 "\"tile_width\":512,\"data_type\":0,\"flags\":7}";
    acs_module_instance_v1* inst = NULL;
    acs_status st = api->create(api,
                                (acs_str_v1){ sizeof(acs_str_v1),
                                              ACS_ABI_VERSION_V1, cfg,
                                              strlen(cfg) },
                                &fh, &inst, &err);
    EXPECT(st == ACS_ERR_ABI_MISMATCH);
    EXPECT(inst == NULL);
}

/* ═════════ case 4: config schema 违例 (512/5) ═════════ */

static void run_schema_reject(void) {
    set_case("schema_reject_512_5");
    const acs_module_api_v1* api = NULL;
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);

    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    char cfg[] = "{\"op\":\"write_product\",\"out_dir\":\"x\",\"nside\":512,"
                 "\"tile_width\":5,\"data_type\":0,\"flags\":7}";
    acs_module_instance_v1* inst = NULL;
    acs_status st = api->create(api,
                                (acs_str_v1){ sizeof(acs_str_v1),
                                              ACS_ABI_VERSION_V1, cfg,
                                              strlen(cfg) },
                                &host, &inst, &err);
    EXPECT(st == ACS_ERR_PARAM);
    EXPECT_STR(err.message_utf8, "tile_width");
    EXPECT(inst == NULL);
}

/* ═════════ case 5: executor 缺失 / acquire 失败 → BUDGET 105 ═════════ */

static void run_budget_missing(void) {
    set_case("budget_missing_executor");
    const acs_module_api_v1* api = NULL;
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, NULL, NULL);
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);

    char cfg[] = "{\"op\":\"write_product\",\"out_dir\":\"x\",\"nside\":512,"
                 "\"tile_width\":512,\"data_type\":0,\"flags\":7}";
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_module_instance_v1* inst = NULL;
    EXPECT(api->create(api,
                       (acs_str_v1){ sizeof(acs_str_v1), ACS_ABI_VERSION_V1,
                                     cfg, strlen(cfg) },
                       &host, &inst, &err) == ACS_OK);

    /* manifest 只带 count 位 (无 tile 平面); 取消点在 begin 前 → 不触数据 */
    char manifest[] = "{\"n_tiles\":1,\"tile_width\":512,\"leaf_order\":9,"
                      "\"parent_ipix\":[0],\"count_bit\":1}";
    static char obuf[1024];
    acs_strbuf_v1 ob = { sizeof(acs_strbuf_v1), ACS_ABI_VERSION_V1,
                         obuf, sizeof(obuf), 0 };
    acs_status st = api->execute(inst,
                                 (acs_str_v1){ sizeof(acs_str_v1),
                                               ACS_ABI_VERSION_V1, manifest,
                                               strlen(manifest) },
                                 (acs_str_v1){ sizeof(acs_str_v1),
                                               ACS_ABI_VERSION_V1, cfg,
                                               strlen(cfg) },
                                 &ob, &err);
    EXPECT(st == ACS_ERR_BUDGET);
    EXPECT(err.detail_code == 105);
    EXPECT_STR(err.message_utf8, "host executor");

    api->destroy(inst);
}

static void run_budget_acquire_fail(void) {
    set_case("budget_acquire_fail");
    const acs_module_api_v1* api = NULL;
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_fail, NULL);
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);

    char cfg[] = "{\"op\":\"write_product\",\"out_dir\":\"x\",\"nside\":512,"
                 "\"tile_width\":512,\"data_type\":0,\"flags\":7}";
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_module_instance_v1* inst = NULL;
    EXPECT(api->create(api,
                       (acs_str_v1){ sizeof(acs_str_v1), ACS_ABI_VERSION_V1,
                                     cfg, strlen(cfg) },
                       &host, &inst, &err) == ACS_OK);

    char manifest[] = "{\"n_tiles\":1,\"tile_width\":512,\"leaf_order\":9,"
                      "\"parent_ipix\":[0],\"count_bit\":1}";
    static char obuf[1024];
    acs_strbuf_v1 ob = { sizeof(acs_strbuf_v1), ACS_ABI_VERSION_V1,
                         obuf, sizeof(obuf), 0 };
    acs_status st = api->execute(inst,
                                 (acs_str_v1){ sizeof(acs_str_v1),
                                               ACS_ABI_VERSION_V1, manifest,
                                               strlen(manifest) },
                                 (acs_str_v1){ sizeof(acs_str_v1),
                                               ACS_ABI_VERSION_V1, cfg,
                                               strlen(cfg) },
                                 &ob, &err);
    EXPECT(st == ACS_ERR_BUDGET);
    EXPECT(err.detail_code == 105);
    EXPECT_STR(err.message_utf8, "lease unavailable");

    api->destroy(inst);
}

/* ═════════ case 6: entry cancel (begin 前) → 事务化中止 ═════════
 * abort 语义 (DISP-HIPS-001): 已写文件不清理, 处置归调用方 —— 断言 =
 * 未 finalize (AllSky.xlsx 缺失), 而非整树不存在。 */

static void run_cancel_not_begun(const char* root) {
    set_case("cancel_not_begun");
    const acs_module_api_v1* api = NULL;
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, &k_cancel);
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);

    char cfg_dir[512];
    snprintf(cfg_dir, sizeof(cfg_dir), "%s", root);
    char cfg_buf[640];
    char cfg[] = "{\"op\":\"write_product\",\"out_dir\":\"PLACEHOLDER\","
                 "\"nside\":512,\"tile_width\":512,\"data_type\":0,\"flags\":7}";
    {
        char* q = strstr(cfg, "PLACEHOLDER");
        snprintf(cfg_buf, sizeof(cfg_buf), "%.*s%s%s",
                 (int)(q - cfg), cfg, cfg_dir, q + strlen("PLACEHOLDER"));
    }
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_module_instance_v1* inst = NULL;
    EXPECT(api->create(api,
                       (acs_str_v1){ sizeof(acs_str_v1), ACS_ABI_VERSION_V1,
                                     cfg_buf, strlen(cfg_buf) },
                       &host, &inst, &err) == ACS_OK);

    char manifest[] = "{\"n_tiles\":1,\"tile_width\":512,\"leaf_order\":9,"
                      "\"parent_ipix\":[0],\"count_bit\":1}";
    static char obuf[1024];
    acs_strbuf_v1 ob = { sizeof(acs_strbuf_v1), ACS_ABI_VERSION_V1,
                         obuf, sizeof(obuf), 0 };
    acs_status st = api->execute(inst,
                                 (acs_str_v1){ sizeof(acs_str_v1),
                                               ACS_ABI_VERSION_V1, manifest,
                                               strlen(manifest) },
                                 (acs_str_v1){ sizeof(acs_str_v1),
                                               ACS_ABI_VERSION_V1, cfg_buf,
                                               strlen(cfg_buf) },
                                 &ob, &err);
    EXPECT(st == ACS_ERR_CANCELLED);
    assert_tree_absent(root);           /* AllSky.xlsx 缺失 = 未 finalize */

    api->destroy(inst);
}

/* ═════════ case 7: strbuf 尺寸探测 + 缓冲不足 ═════════ */

static void run_strbuf_probe(void) {
    set_case("strbuf_probe_plan");
    const acs_module_api_v1* api = NULL;
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);

    char cfg[] = "{\"op\":\"write_product\",\"out_dir\":\"x\",\"nside\":512,"
                 "\"tile_width\":512,\"data_type\":0,\"flags\":7}";
    char mid[] = ASTROCS_HIPS_MODULE_ID;
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));

    /* 探测: data=NULL, cap=0 → ACS_OK + size=所需 */
    acs_strbuf_v1 probe = { sizeof(acs_strbuf_v1), ACS_ABI_VERSION_V1,
                            NULL, 0, 0 };
    acs_status st = api->plan(api,
                              (acs_str_v1){ sizeof(acs_str_v1),
                                            ACS_ABI_VERSION_V1, mid,
                                            strlen(mid) },
                              (acs_str_v1){ sizeof(acs_str_v1),
                                            ACS_ABI_VERSION_V1, cfg,
                                            strlen(cfg) },
                              &probe, &err);
    EXPECT(st == ACS_OK);
    EXPECT(probe.data == NULL);
    uint64_t need = probe.size;
    EXPECT(need > 0);

    /* 缓冲不足 → PARAM + BUFFER_TOO_SMALL + size=所需 */
    char small[32];
    acs_strbuf_v1 sb = { sizeof(acs_strbuf_v1), ACS_ABI_VERSION_V1,
                         small, sizeof(small), 0 };
    st = api->plan(api,
                   (acs_str_v1){ sizeof(acs_str_v1), ACS_ABI_VERSION_V1, mid,
                                 strlen(mid) },
                   (acs_str_v1){ sizeof(acs_str_v1), ACS_ABI_VERSION_V1, cfg,
                                 strlen(cfg) },
                   &sb, &err);
    EXPECT(st == ACS_ERR_PARAM);
    EXPECT(sb.size == need);
    EXPECT(err.detail_code == ACS_DIAG_ECODE_BUFFER_TOO_SMALL);
}

/* ═════════ case 8: 真实 DLL 导出面探针 (dlsym; ABI-006) ═════════ */

static void run_export_probe(void) {
    set_case("export_probe_dlsym");
    const char* dll = getenv("ASTROCS_HIPS_DLL_PATH");
    if (!dll || !dll[0]) {
        printf("FAIL [%s] ASTROCS_HIPS_DLL_PATH not set\n", g_case);
        g_fail++;
        return;
    }
    void* h = dlopen(dll, RTLD_NOW);
    if (!h) {
        printf("FAIL [%s] dlopen(%s): %s\n", g_case, dll, dlerror());
        g_fail++;
        return;
    }
    /* 导出面纪律: 唯一模块入口; legacy 九导出符号不得泄漏到动态面 */
    static const char* const kLegacy[] = {
        "aio_hips_product_begin",
        "aio_hips_write_signal_support_tile",
        "aio_hips_write_variance_tile",
        "aio_hips_write_snr_points",
        "aio_hips_set_drizzle_provenance",
        "aio_hips_finalize",
        "aio_hips_abort",
        "aio_hips_write",
        "aio_hips_last_error",
    };
    for (int i = 0; i < 9; ++i) {
        void* s = dlsym(h, kLegacy[i]);
        EXPECT(s == NULL);
    }
    void* q = dlsym(h, "astrocs_module_query_v1");
    EXPECT(q != NULL);
    dlclose(h);
}

/* ═════════ main ═════════ */

int main(int argc, char** argv) {
    const char* root = (argc > 1) ? argv[1] : "./run/t_hips_adapter";
    const char* ref  = (argc > 2) ? argv[2] : "./run/t_hips_ref";

    hips_mkdir_p(root);
    hips_mkdir_p(ref);

    run_direct_reference(ref);
    run_adapter_full(root, ref);
    run_alloc_fail();
    run_schema_reject();
    run_budget_missing();
    run_budget_acquire_fail();
    {
        char cd[1024];
        snprintf(cd, sizeof(cd), "%s_cancel_not_begun", root);
        run_cancel_not_begun(cd);
    }
    run_strbuf_probe();
    run_export_probe();

    printf(g_fail ? "P1_HIPS_ADAPTER_TEST: FAIL (%d)\n"
                  : "P1_HIPS_ADAPTER_TEST: PASS (%d)\n", g_fail);
    return g_fail ? 1 : 0;
}
