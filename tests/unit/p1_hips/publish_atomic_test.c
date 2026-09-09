/* publish_atomic_test.c - HiPS 原子发布与临时目录清理验证面 (AIO-002)
 *
 * 对齐先例: tests/unit/p1_hips/adapter_test.c (P1-HIPS-IMPL host api 构造/
 * tile 流) + tests/unit/aio_abi_tests.cpp (AIO-001 fault 注入必败口径)。
 *
 * 被测面:
 *   - lib/hips/src/aio_publish.cpp (publish.h v1 合同, 4 原语唯一实现):
 *     units 组 — 正向/边界/错误/幂等 (RAII 语义: discard 恒收敛)。
 *   - module_entry.cpp write_product 事务面: atomic 组 —
 *     正向发布 (staging→fsync→原子 promote→out_dir 完整树)、
 *     cancel 中断 (staging 丢弃 → 目标根无 partial)、
 *     kill 中断 (fork+SIGKILL 驻留 staging 期 → 目标根无 partial + 残留
 *     自愈)、注入必败 (p1_stage_create_fail/p1_fsync_fail/p1_promote_fail/
 *     p1_discard_noop; 红锚: 旧行为直写时 out_dir 含半成品树, 断言翻红)。
 *
 * 验收锚 (AIO-002): 正常/取消/ENOSPC(fsync 暴露面)/kill 后无 partial;
 * 临时目录 RAII (残留不累积, 下次 stage_create 确定性自愈);
 * 全部注入名必败 (无恒 PASS 占位)。
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/hips/types.h"
#include "astrocs/hips/publish.h"
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
               g_case, __FILE__, __LINE__, (hay) ? (hay) : "(null)", needle); \
        g_fail++; \
    } \
} while (0)

static void set_case(const char* c) {
    snprintf(g_case, sizeof(g_case), "%s", c);
    printf("case: %s\n", c);
}

/* ═══════════════════ fs helper ═══════════════════ */

static void mkdir_p(const char* path) {
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

static int path_exists(const char* p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static int path_is_dir(const char* p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static void msleep_(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void touch_file(const char* p, const char* content) {
    FILE* f = fopen(p, "wb");
    if (!f) return;
    if (content && content[0]) fputs(content, f);
    fclose(f);
}

static void rm_rf(const char* path) {
    DIR* d = opendir(path);
    if (d) {
        struct dirent* e;
        char child[1024];
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                continue;
            snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
            struct stat st;
            if (lstat(child, &st) == 0 && S_ISDIR(st.st_mode)) rm_rf(child);
            else unlink(child);
        }
        closedir(d);
        rmdir(path);
    } else {
        unlink(path);
    }
}

/* staging 残留探测 (AIO-002 RAII 锚: 目标根旁 .<base>.hips_staging.tmp) */
static int stage_exists(const char* out_dir) {
    char parent[1024], base[512], stage[1200];
    const char* slash = strrchr(out_dir, '/');
    if (slash) {
        snprintf(parent, sizeof(parent), "%.*s", (int)(slash - out_dir),
                 out_dir);
        snprintf(base, sizeof(base), "%s", slash + 1);
    } else {
        snprintf(parent, sizeof(parent), ".");
        snprintf(base, sizeof(base), "%s", out_dir);
    }
    if (base[0] == '\0') snprintf(base, sizeof(base), "hips");
    snprintf(stage, sizeof(stage), "%s/.%s%s", parent, base,
             ASTROCS_HIPS_STAGE_BASENAME);
    return path_exists(stage);
}

/* partial 判定: out_dir 根内不得出现任何 HiPS 子产品/manifest (原子性:
 * 失败后目标根保持旧态或不存在; staging 兄弟目录由 stage_exists 单独判) */
static int target_has_partial(const char* out_dir) {
    static const char* const kForbidden[] = {
        "signal", "support", "variance", "ivar", "snr",
        "manifest.json"
    };
    for (size_t i = 0; i < sizeof(kForbidden) / sizeof(kForbidden[0]); ++i) {
        char p[1200];
        snprintf(p, sizeof(p), "%s/%s", out_dir, kForbidden[i]);
        if (path_exists(p)) return 1;
    }
    return 0;
}

/* ═══════════════════ host api 构造 (adapter_test.c 同款) ═══════════════════ */

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
static void exec_release_ok(void* ud, uint32_t n) { (void)ud; (void)n; }

/* 计数型 cancel: 前 skip_after 次查询返回 0, 之后恒 1 (tile 间触发) */
typedef struct { int calls; int skip_after; } cancel_cnt;
static int exec_cancel_counted(void* ud) {
    cancel_cnt* c = (cancel_cnt*)ud;
    return ++c->calls > c->skip_after ? 1 : 0;
}

static const acs_allocator_v1 k_alloc = {
    { (uint32_t)sizeof(acs_allocator_v1), ACS_ABI_VERSION_V1 },
    halloc, hfree, NULL
};
static const acs_executor_v1 k_exec_ok = {
    { (uint32_t)sizeof(acs_executor_v1), ACS_ABI_VERSION_V1 },
    4, 2, exec_acquire_ok, exec_release_ok, NULL
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

/* ═══════════════════ tile 流 (adapter_test.c 确定性同款: 4 tile) ═══════════════════ */

static const uint64_t kParents[4] = { 0, 1, 6, 11 };

static const char kB64Tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static uint64_t b64_encoded_len(uint64_t n) { return ((n + 2) / 3) * 4; }

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
        dst[o++] = kB64Tab[(v >> 18) & 63]; dst[o++] = kB64Tab[(v >> 12) & 63];
        dst[o++] = '='; dst[o++] = '=';
    } else if (rem == 2) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8);
        dst[o++] = kB64Tab[(v >> 18) & 63]; dst[o++] = kB64Tab[(v >> 12) & 63];
        dst[o++] = kB64Tab[(v >> 6) & 63];  dst[o++] = '=';
    }
    dst[o] = '\0';
}

/* 输出 manifest 缓冲上限 (4 tile flags=7 无 SNR; 探测口径同 adapter_test) */
static char g_out_buf[8192];

/* 组装 execute 输入 (4 tile; flags=7 signal/support/snr) 并执行一次事务。
 * 返回 execute 状态码; out_dir 写入 out_dir 参数。 */
static acs_status run_write_product(const acs_module_api_v1* api,
                                    const acs_host_api_v1* host,
                                    const char* out_dir,
                                    acs_error_info_v1* err) {
    char cfg[1400];
    snprintf(cfg, sizeof(cfg),
             "{\"op\":\"write_product\",\"out_dir\":\"%s\",\"nside\":512,"
             "\"tile_width\":512,\"data_type\":0,\"flags\":7}",
             out_dir);
    acs_module_instance_v1* inst = NULL;
    acs_status st = api->create(api,
        (acs_str_v1){ sizeof(acs_str_v1), ACS_ABI_VERSION_V1,
                      cfg, strlen(cfg) },
        host, &inst, err);
    if (st != ACS_OK) return st;

    /* 平面: float32 产品 (dtype_sz=4, types.h HIPS_M_KEY 尺寸合同)
     * 每 tile W² 元素 × 4B = 1MiB; 4 tile = 4MiB/平面 (base64 ~5.6MB)。 */
    const uint64_t W = 512, elems = W * W, dtype_sz = 4;
    const uint64_t plane_bytes = 4 * elems * dtype_sz;
    uint8_t* flux = (uint8_t*)malloc((size_t)plane_bytes);
    uint8_t* cov = (uint8_t*)malloc((size_t)plane_bytes);
    if (!flux || !cov) { free(flux); free(cov); api->destroy(inst); return ACS_ERR_NOMEM; }
    for (uint64_t t = 0; t < 4; ++t) {
        for (uint64_t i = 0; i < elems; ++i) {
            /* 确定性伪数据: 字节型生成 → float32 重解释 (位型填充) */
            uint32_t bits = (uint32_t)(0x3D800000u + (i & 0xFFFFu) * 3u +
                                       t * 17u);   /* ~0.0625..0.375 */
            uint32_t cbits = 0x3E800000u + (uint32_t)((i * 3 + t) & 0xFFFFu);
            memcpy(flux + (t * elems + i) * 4, &bits, 4);
            memcpy(cov + (t * elems + i) * 4, &cbits, 4);
        }
    }
    /* base64 分段拼装 (2×4MiB → ~11.2MB 文本; 手工分段)。
     * chunk_b64 上界: chunk=3*1024 字节 → 编码 4096 字符 + NUL = 4097。 */
    static char man[12582912];
    char* w = man;
    w += snprintf(w, (size_t)(sizeof(man) - (size_t)(w - man)),
        "{\"n_tiles\":4,\"tile_width\":512,\"leaf_order\":9,"
        "\"width\":512,"
        "\"parent_ipix\":[0,1,6,11],"
        "\"flux_base64\":\"");
    static char chunk_b64[4104];
    /* 分块编码 (每块 3 的倍数字节 → 无 '=' 填充, 拼接等价整段编码) */
    {
        const uint64_t total = plane_bytes;
        const uint64_t chunk = 3 * 1024;
        for (uint64_t off = 0; off < total; off += chunk) {
            uint64_t n = (total - off < chunk) ? (total - off) : chunk;
            b64_encode(flux + off, n, chunk_b64);
            size_t bl = strlen(chunk_b64);
            memcpy(w, chunk_b64, bl);
            w += bl;
            *w = '\0';
        }
    }
    w += snprintf(w, (size_t)(sizeof(man) - (size_t)(w - man)),
        "\",\"coverage_base64\":\"");
    {
        const uint64_t total = plane_bytes;
        const uint64_t chunk = 3 * 1024;
        for (uint64_t off = 0; off < total; off += chunk) {
            uint64_t n = (total - off < chunk) ? (total - off) : chunk;
            b64_encode(cov + off, n, chunk_b64);
            size_t bl = strlen(chunk_b64);
            memcpy(w, chunk_b64, bl);
            w += bl;
            *w = '\0';
        }
    }
    w += snprintf(w, (size_t)(sizeof(man) - (size_t)(w - man)), "\"}");

    acs_strbuf_v1 ob = { sizeof(acs_strbuf_v1), ACS_ABI_VERSION_V1,
                         g_out_buf, sizeof(g_out_buf), 0 };
    st = api->execute(inst,
        (acs_str_v1){ sizeof(acs_str_v1), ACS_ABI_VERSION_V1,
                      man, (uint64_t)(w - man) },
        (acs_str_v1){ sizeof(acs_str_v1), ACS_ABI_VERSION_V1,
                      cfg, strlen(cfg) },
        &ob, err);
    api->destroy(inst);
    free(flux);
    free(cov);
    return st;
}

/* ═══════════════════ units 组: 4 原语直接 ABI ═══════════════════ */

static void run_units(const char* root) {
    char dir[1024], stage[1200];

    /* u1: create 正向 + 路径回填 + 段词法 */
    set_case("units_stage_create");
    snprintf(dir, sizeof(dir), "%s/units/target_a", root);
    mkdir_p(dir);
    EXPECT(aio_publish_stage_create_v1(dir, stage, sizeof(stage)) ==
           AIO_PUBLISH_OK);
    EXPECT(path_is_dir(stage));
    EXPECT(strstr(stage, ASTROCS_HIPS_STAGE_BASENAME) != NULL);
    EXPECT(strncmp(stage, dir, strlen(dir)) == 0 ||
           strstr(stage, "/.") != NULL);
    /* staging 必须是 out_dir 的兄弟 (父目录内), 不在其内部 */
    {
        char parent_probe[1100];
        snprintf(parent_probe, sizeof(parent_probe), "%s/%s",
                 stage, "self_nested_probe");
        (void)parent_probe;
        EXPECT(strstr(stage, dir) == NULL);   /* 兄弟, 非嵌套 */
    }

    /* u2: create 自愈 — 预置同名残留 (垃圾文件) → 再 create 必清 */
    set_case("units_stage_create_selfheal");
    {
        char junk[1300];
        snprintf(junk, sizeof(junk), "%s/stale_junk.bin", stage);
        touch_file(junk, "stale");
        EXPECT(aio_publish_stage_create_v1(dir, stage, sizeof(stage)) ==
               AIO_PUBLISH_OK);
        EXPECT(path_is_dir(stage));
        snprintf(junk, sizeof(junk), "%s/stale_junk.bin", stage);
        EXPECT(!path_exists(junk));           /* 残留垃圾被自愈清除 */
    }

    /* u3: create 负向 (NULL/空/容量不足) */
    set_case("units_stage_create_negative");
    EXPECT(aio_publish_stage_create_v1(NULL, stage, sizeof(stage)) ==
           AIO_PUBLISH_ERR_PARAM);
    EXPECT(aio_publish_stage_create_v1("", stage, sizeof(stage)) ==
           AIO_PUBLISH_ERR_PARAM);
    EXPECT(aio_publish_stage_create_v1(dir, NULL, sizeof(stage)) ==
           AIO_PUBLISH_ERR_PARAM);
    EXPECT(aio_publish_stage_create_v1(dir, stage, 4) ==
           AIO_PUBLISH_ERR_PARAM);

    /* u4: fsync 正向 + 计数; 树不存在 → IO */
    set_case("units_tree_fsync");
    {
        char f1[1300], sub[1300];
        snprintf(sub, sizeof(sub), "%s/Norder0/Dir0", stage);
        mkdir_p(sub);
        snprintf(f1, sizeof(f1), "%s/properties", stage);
        touch_file(f1, "k=v\n");
        snprintf(f1, sizeof(f1), "%s/Norder0/Dir0/Npix0.fits", stage);
        touch_file(f1, "FITS");
        uint64_t nf = 0, nb = 0;
        EXPECT(aio_publish_tree_fsync_v1(stage, &nf, &nb) == AIO_PUBLISH_OK);
        EXPECT(nf == 2);
        EXPECT(nb == 4 + 4);                  /* "k=v\n"=4 + "FITS"=4 */
        EXPECT(aio_publish_tree_fsync_v1(stage, NULL, NULL) == AIO_PUBLISH_OK);
        snprintf(f1, sizeof(f1), "%s/missing_dir_xyz", root);
        EXPECT(aio_publish_tree_fsync_v1(f1, NULL, NULL) == AIO_PUBLISH_ERR_IO);
        EXPECT(aio_publish_tree_fsync_v1(NULL, NULL, NULL) ==
               AIO_PUBLISH_ERR_PARAM);
    }

    /* u5: promote 正向 (空 out_dir 被替换) + 目标非空拒绝 + stage 缺失拒绝 */
    set_case("units_promote");
    {
        char out[1024];
        snprintf(out, sizeof(out), "%s/units/target_promoted", root);
        rm_rf(out);
        EXPECT(aio_publish_promote_v1(out, stage) == AIO_PUBLISH_OK);
        EXPECT(path_is_dir(out));
        EXPECT(!path_exists(stage));          /* staging 原子消失 (rename) */
        /* promote 后产物在 out */
        char p[1300];
        snprintf(p, sizeof(p), "%s/properties", out);
        EXPECT(path_exists(p));
        /* 再 promote: stage 已不存在 → STATE */
        EXPECT(aio_publish_promote_v1(out, stage) == AIO_PUBLISH_ERR_STATE);
        /* 目标非空 → STATE (唯一目标语义) */
        EXPECT(aio_publish_stage_create_v1(out, stage, sizeof(stage)) ==
               AIO_PUBLISH_OK);
        EXPECT(aio_publish_promote_v1(out, stage) == AIO_PUBLISH_ERR_STATE);
        /* stage 完好 (拒绝不损 staging) → discard 收口 */
        EXPECT(path_is_dir(stage));
        EXPECT(aio_publish_stage_discard_v1(out) == AIO_PUBLISH_OK);
        EXPECT(!path_exists(stage));
    }

    /* u6: discard 幂等 + 负向 + 注入假清有效性证明 (p1_discard_noop) */
    set_case("units_discard");
    snprintf(dir, sizeof(dir), "%s/units/target_d", root);
    mkdir_p(dir);
    EXPECT(aio_publish_stage_create_v1(dir, stage, sizeof(stage)) ==
           AIO_PUBLISH_OK);
    EXPECT(path_is_dir(stage));
    /* 幂等: 不存在也 OK */
    EXPECT(aio_publish_stage_discard_v1(dir) == AIO_PUBLISH_OK);
    EXPECT(!path_exists(stage));
    EXPECT(aio_publish_stage_discard_v1(dir) == AIO_PUBLISH_OK);
    EXPECT(aio_publish_stage_discard_v1(NULL) == AIO_PUBLISH_ERR_PARAM);
    /* 注入假清: 返回 OK 但残留仍在 → 证明常规 discard 真实删除
     * (注入必败口径: 该注入使"清空断言"翻红, 不存在恒 PASS 占位) */
    EXPECT(aio_publish_stage_create_v1(dir, stage, sizeof(stage)) ==
           AIO_PUBLISH_OK);
    setenv("ASTROCS_HIPS_PUBLISH_FAULT", "p1_discard_noop", 1);
    EXPECT(aio_publish_stage_discard_v1(dir) == AIO_PUBLISH_OK);
    unsetenv("ASTROCS_HIPS_PUBLISH_FAULT");
    EXPECT(path_is_dir(stage));               /* 假清生效 → 残留仍在 */
    EXPECT(aio_publish_stage_discard_v1(dir) == AIO_PUBLISH_OK);
    EXPECT(!path_exists(stage));
}

/* ═══════════════════ atomic 组: write_product 事务面 ═══════════════════ */

static void run_atomic_success(const char* root) {
    set_case("atomic_success_publish");
    char out[1024];
    snprintf(out, sizeof(out), "%s/atomic/success/target", root);
    rm_rf(out);                                /* 幂等重跑: 从零态开始 */
    mkdir_p(out);                              /* 空目标 (唯一目标语义) */
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    const acs_module_api_v1* api = NULL;
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status st = run_write_product(api, &host, out, &err);
    EXPECT(st == ACS_OK);
    char p[1200];
    snprintf(p, sizeof(p), "%s/manifest.json", out);
    EXPECT(path_exists(p));                    /* writer 完成标记 (原子树内) */
    snprintf(p, sizeof(p), "%s/signal/Norder0/Dir0/Npix0.fits", out);
    EXPECT(path_exists(p));
    EXPECT(!stage_exists(out));                /* staging 已 promote 消失 */
    /* 输出 manifest 报告 out_dir (发布目标) */
    EXPECT_STR(g_out_buf, "\"out_dir\":");
    EXPECT_STR(g_out_buf, "artifacts");
}

static void run_atomic_fsync_fail(const char* root) {
    set_case("atomic_fault_fsync_fail");
    char out[1024];
    snprintf(out, sizeof(out), "%s/atomic/fsync_fail/target", root);
    rm_rf(out);
    mkdir_p(out);
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    const acs_module_api_v1* api = NULL;
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);
    setenv("ASTROCS_HIPS_PUBLISH_FAULT", "p1_fsync_fail", 1);
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status st = run_write_product(api, &host, out, &err);
    unsetenv("ASTROCS_HIPS_PUBLISH_FAULT");
    EXPECT(st == ACS_ERR_IO);                  /* ENOSPC 收敛面 → IO */
    EXPECT_STR(err.message_utf8, "publish failed");
    EXPECT(target_has_partial(out) == 0);      /* 无 partial (红锚: 旧行为
                                                * 直写 → 树存在 → 翻红) */
    EXPECT(!stage_exists(out));                /* staging RAII 丢弃 */
}

static void run_atomic_promote_fail(const char* root) {
    set_case("atomic_fault_promote_fail");
    char out[1024];
    snprintf(out, sizeof(out), "%s/atomic/promote_fail/target", root);
    rm_rf(out);
    mkdir_p(out);
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    const acs_module_api_v1* api = NULL;
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);
    setenv("ASTROCS_HIPS_PUBLISH_FAULT", "p1_promote_fail", 1);
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status st = run_write_product(api, &host, out, &err);
    unsetenv("ASTROCS_HIPS_PUBLISH_FAULT");
    EXPECT(st == ACS_ERR_IO);
    EXPECT(target_has_partial(out) == 0);
    EXPECT(!stage_exists(out));
}

static void run_atomic_stage_fail(const char* root) {
    set_case("atomic_fault_stage_create_fail");
    char out[1024];
    snprintf(out, sizeof(out), "%s/atomic/stage_fail/target", root);
    rm_rf(out);
    mkdir_p(out);
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    const acs_module_api_v1* api = NULL;
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);
    setenv("ASTROCS_HIPS_PUBLISH_FAULT", "p1_stage_create_fail", 1);
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status st = run_write_product(api, &host, out, &err);
    unsetenv("ASTROCS_HIPS_PUBLISH_FAULT");
    EXPECT(st == ACS_ERR_IO);
    EXPECT_STR(err.message_utf8, "staging create failed");
    EXPECT(target_has_partial(out) == 0);      /* 未触数据平面 */
    EXPECT(!stage_exists(out));
}

static void run_atomic_cancel_mid(const char* root) {
    set_case("atomic_cancel_mid_transaction");
    char out[1024];
    snprintf(out, sizeof(out), "%s/atomic/cancel_mid/target", root);
    rm_rf(out);
    mkdir_p(out);
    acs_host_api_v1 host;
    static cancel_cnt cc = { 0, 1 };           /* tile 间第 2 次查询触发取消 */
    cc.calls = 0;
    static const acs_cancel_v1 k_cancel = {
        { (uint32_t)sizeof(acs_cancel_v1), ACS_ABI_VERSION_V1 },
        exec_cancel_counted, &cc
    };
    host_fill(&host, &k_alloc, &k_exec_ok, &k_cancel);
    const acs_module_api_v1* api = NULL;
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status st = run_write_product(api, &host, out, &err);
    EXPECT(st == ACS_ERR_CANCELLED);
    EXPECT(cc.calls > 1);                      /* 取消确实发生在事务中 */
    EXPECT(target_has_partial(out) == 0);      /* 取消后无 partial (红锚) */
    EXPECT(!stage_exists(out));                /* staging 确定性丢弃 */
}

#ifndef _WIN32
/* kill 中断: 子进程驻留 staging 期 (p1_stage_slow_write 注入) 被 SIGKILL →
 * 目标根无 partial; staging 残留 → 下次事务自愈 (RAII 收口)。 */
static void run_atomic_kill(const char* root) {
    set_case("atomic_kill_no_partial_and_selfheal");
    char out[1024];
    snprintf(out, sizeof(out), "%s/atomic/kill/target", root);
    rm_rf(out);
    mkdir_p(out);

    pid_t pid = fork();
    EXPECT(pid >= 0);
    if (pid == 0) {
        /* 子进程: 注入 slow → 驻留 staging 期; SIGKILL 由父进程施加 */
        setenv("ASTROCS_HIPS_PUBLISH_FAULT", "p1_stage_slow_write", 1);
        acs_host_api_v1 host;
        host_fill(&host, &k_alloc, &k_exec_ok, NULL);
        const acs_module_api_v1* api = NULL;
        if (astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) != ACS_OK ||
            !api) _exit(2);
        acs_error_info_v1 err;
        memset(&err, 0, sizeof(err));
        run_write_product(api, &host, out, &err);
        _exit(0);                              /* 未被 kill = 时序异常 */
    }
    /* 轮询等待子进程进入 staging 期 (asan/-O0 慢 10x, 固定延时不确定;
     * slow 注入窗口 600ms, 轮询步进 50ms 上限 5s 覆盖之) */
    int entered_stage = 0;
    for (int i = 0; i < 100; ++i) {
        msleep_(50);
        if (stage_exists(out)) { entered_stage = 1; break; }
    }
    EXPECT(entered_stage);
    kill(pid, SIGKILL);
    int wst = 0;
    waitpid(pid, &wst, 0);
    EXPECT(WIFSIGNALED(wst) && WTERMSIG(wst) == SIGKILL);

    /* kill 后: 目标根无 partial (半成品 tile 只在 staging 兄弟目录内);
     * staging 残留 (进程死亡, RAII 不可达 — 预期形态) */
    EXPECT(target_has_partial(out) == 0);
    EXPECT(stage_exists(out));

    /* 自愈: 下一次事务 stage_create 确定性清残留 → 正常发布 → 完整树 */
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    const acs_module_api_v1* api = NULL;
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status st = run_write_product(api, &host, out, &err);
    EXPECT(st == ACS_OK);
    EXPECT(!stage_exists(out));                /* 残留已自愈 */
    char p[1200];
    snprintf(p, sizeof(p), "%s/manifest.json", out);
    EXPECT(path_exists(p));
}
#endif /* !_WIN32 */

static void run_atomic_overwrite_nonempty_reject(const char* root) {
    set_case("atomic_target_nonempty_reject");
    char out[1024], p[1200];
    snprintf(out, sizeof(out), "%s/atomic/nonempty/target", root);
    rm_rf(out);
    mkdir_p(out);
    snprintf(p, sizeof(p), "%s/user_data.bin", out);
    touch_file(p, "precious");
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    const acs_module_api_v1* api = NULL;
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status st = run_write_product(api, &host, out, &err);
    EXPECT(st == ACS_ERR_IO);                  /* 发布门失败 (promote STATE) */
    EXPECT_STR(err.message_utf8, "no partial tree");
    snprintf(p, sizeof(p), "%s/user_data.bin", out);
    EXPECT(path_exists(p));                    /* 既有产物原样保留 */
    snprintf(p, sizeof(p), "%s/manifest.json", out);
    EXPECT(!path_exists(p));                   /* 未发布 */
    EXPECT(!stage_exists(out));                /* staging 已丢弃 */
}

static void run_atomic_stale_junk_selfheal(const char* root) {
    set_case("atomic_stale_stage_junk_selfheal");
    char out[1024], junk[1300];
    snprintf(out, sizeof(out), "%s/atomic/stale/target", root);
    rm_rf(out);
    mkdir_p(out);
    /* 手工预置 staging 残留 + 垃圾文件 (kill 同形态) */
    EXPECT(aio_publish_stage_create_v1(out, (char[1200]){0}, 1200) ==
           AIO_PUBLISH_OK);
    {
        char stage[1200];
        const char* slash = strrchr(out, '/');
        snprintf(stage, sizeof(stage), "%.*s/%s%s", (int)(slash - out), out,
                 slash + 1, ASTROCS_HIPS_STAGE_BASENAME);
        snprintf(junk, sizeof(junk), "%s/stale.bin", stage);
        touch_file(junk, "garbage");
    }
    acs_host_api_v1 host;
    host_fill(&host, &k_alloc, &k_exec_ok, NULL);
    const acs_module_api_v1* api = NULL;
    EXPECT(astrocs_module_query_v1(ACS_ABI_VERSION_V1, &host, &api) == ACS_OK);
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status st = run_write_product(api, &host, out, &err);
    EXPECT(st == ACS_OK);
    snprintf(junk, sizeof(junk), "%s/../stale", out);
    (void)junk;
    /* 垃圾文件不在发布树 (残留被自愈删除) */
    snprintf(junk, sizeof(junk), "%s/stale.bin", out);
    EXPECT(!path_exists(junk));
    EXPECT(!stage_exists(out));
    snprintf(junk, sizeof(junk), "%s/manifest.json", out);
    EXPECT(path_exists(junk));
}

/* ═══════════════════ main ═══════════════════ */

int main(int argc, char** argv) {
    const char* mode = (argc > 1) ? argv[1] : "all";
    const char* root = (argc > 2) ? argv[2] : "./run/t_hips_publish";

    if (strcmp(mode, "units") == 0 || strcmp(mode, "all") == 0) {
        char uroot[1100];
        snprintf(uroot, sizeof(uroot), "%s/units_root", root);
        mkdir_p(uroot);
        run_units(uroot);
    }
    if (strcmp(mode, "atomic") == 0 || strcmp(mode, "all") == 0) {
        char aroot[1100];
        snprintf(aroot, sizeof(aroot), "%s/atomic_root", root);
        mkdir_p(aroot);
        run_atomic_success(aroot);
        run_atomic_fsync_fail(aroot);
        run_atomic_promote_fail(aroot);
        run_atomic_stage_fail(aroot);
        run_atomic_cancel_mid(aroot);
#ifndef _WIN32
        run_atomic_kill(aroot);
#endif
        run_atomic_overwrite_nonempty_reject(aroot);
        run_atomic_stale_junk_selfheal(aroot);
    }

    printf(g_fail ? "AIO_HIPS_PUBLISH_TEST: FAIL (%d)\n"
                  : "AIO_HIPS_PUBLISH_TEST: PASS (%d)\n", g_fail);
    return g_fail ? 1 : 0;
}
