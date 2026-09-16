// ============================================================================
// test_spec_collector_ownership.c - B4-P1-3 共址测试 (ASAN)
//
// 修复点: gaia_client.c spec_collector_push —— 修复前 spectra 扩容失败
// 路径 free(new_stars), 而 realloc 已释放旧 sc->stars 且未回写 =>
// sc->stars 悬垂, 后续 spec_collector_free double free。
// 修复: realloc 返回值立即回写 sc->stars (单一所有权); spectra 失败仅
// 返回, capacity 不提升, 下次 push 自动重试。
//
// 方法: #include 源码 + GAIA_ALLOC_TEST 包装 (docs/algorithms/GAIA_QUERY.md
// §5 冻结测试设计), 注入第 N 次 realloc 失败; ASAN 捕获 double free。
//
// 编译 (ASAN):
//   gcc -std=gnu99 -O1 -g -fsanitize=address -I../src \
//       test_spec_collector_ownership.c -o test_spec_collector_ownership
// 运行: ./test_spec_collector_ownership
// 返回: 0=通过, 非0=失败 (ASAN abort = 修复无效)
//
// 日期: 2026-09-08 (bughunt P1 batchA)
// ============================================================================

#include <stdio.h>
#include <dlfcn.h>   /* dlsym: 绕开 gaia_client.c 的分配宏重定向 */

/* 前置声明 (GAIA_ALLOC_TEST 下 gaia_client.c 只做宏重定向, 无原型) */
void *gaia_test_malloc(size_t size);
void *gaia_test_calloc(size_t nmemb, size_t size);
void *gaia_test_realloc(void *ptr, size_t size);
void gaia_test_free(void *ptr);

/* 激活分配包装: 之后 include 的 gaia_client.c 中 malloc/calloc/realloc/free
 * 全部重定向到上方声明/下方实现的 gaia_test_* (GAIA_ALLOC_TEST 契约, §5) */
#define GAIA_ALLOC_TEST 1
#include "../src/gaia_client.c"

/* ---------- 分配故障注入实现 (GAIA_ALLOC_TEST 契约) ---------- */
static long g_fail_realloc_at = -1; /* 命中序号则该次 realloc 返回 NULL (-1=不注入) */
static long g_realloc_seq = 0;      /* realloc 调用序号 (仅统计 realloc, malloc/calloc 不计) */

static void gaia_inject_reset(long fail_realloc_at) {
    g_fail_realloc_at = fail_realloc_at;
    g_realloc_seq = 0;  /* 每个 case 从下一次 realloc 重新计数 */
}

void *gaia_test_malloc(size_t size) {
    /* GAIA_ALLOC_TEST 下 gaia_client.c 的 malloc 已重定向到本函数,
     * 用 dlsym 取真实分配器, 避免宏递归栈溢出 */
    static void *(*real_malloc)(size_t) = NULL;
    if (!real_malloc) real_malloc = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
    return real_malloc(size);
}

void *gaia_test_calloc(size_t nmemb, size_t size) {
    static void *(*real_calloc)(size_t, size_t) = NULL;
    if (!real_calloc) {
        real_calloc = (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
        if (!real_calloc) { fprintf(stderr, "dlsym calloc failed\n"); abort(); }
    }
    return real_calloc(nmemb, size);
}

void *gaia_test_realloc(void *ptr, size_t size) {
    g_realloc_seq++;
    if (g_realloc_seq == g_fail_realloc_at) {
        fprintf(stderr, "[inject] realloc(seq=%ld) -> NULL (注入失败)\n", g_realloc_seq);
        return NULL;  /* 注: 注入路径不释放 ptr, 与真实 realloc 失败语义一致 */
    }
    static void *(*real_realloc)(void *, size_t) = NULL;
    if (!real_realloc) real_realloc = (void *(*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");
    return real_realloc(ptr, size);
}

void gaia_test_free(void *ptr) {
    static void (*real_free)(void *) = NULL;
    if (!real_free) real_free = (void (*)(void *))dlsym(RTLD_NEXT, "free");
    real_free(ptr);
}

/* ---------- 测试体 ---------- */

#define WL_TEST 4   /* spectrum_count=4 (非 0, 走双缓冲路径) */

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); } \
    else      { printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

int main(void) {
    printf("=== B4-P1-3: spec_collector_push realloc 所有权 共址测试 ===\n");

    /* Case A: spectra 扩容失败注入 —— 修复前此处 double free (ASAN abort) */
    {
        SpectrumStarCollector sc;
        spec_collector_init(&sc, 4, WL_TEST);
        CHECK(sc.stars && sc.spectra && sc.capacity == 4, "init: capacity=4 双缓冲就绪");

        for (int i = 0; i < 4; i++) {
            spec_collector_push(&sc, 10.0 + i, 20.0 + i, 12.0, 1.0f, 1.0f, NULL);
        }
        CHECK(sc.count == 4, "填满 capacity=4");

        /* 注入下一次 realloc 失败 (第 5 次 push 触发扩容:
         * stars realloc 先行, spectra realloc 失败) */
        /* 注入第 2 次 realloc 失败: 第 1 次 = stars 扩容(成功),
         * 第 2 次 = spectra 扩容(失败) —— 精确命中悬垂路径 */
        gaia_inject_reset(2);
        spec_collector_push(&sc, 99.0, 99.0, 12.0, 1.0f, 1.0f, NULL);
        CHECK(sc.count == 4, "扩容失败: count 不变 (星未入队, 语义保守)");
        CHECK(sc.stars != NULL, "扩容失败后 sc->stars 仍有效 (单一所有权核心断言)");
        /* 修复关键断言: sc->stars 仍是唯一有效指针 (由 ASAN 守护 ——
         * 修复前 sc->stars 悬垂, spec_collector_free 触发 double-free abort) */

        gaia_inject_reset(-1);  /* 解除注入 */
        /* 继续压入: 触发重试扩容并成功 */
        for (int i = 0; i < 8; i++) {
            spec_collector_push(&sc, 30.0 + i, 40.0 + i, 13.0, 2.0f, 3.0f, NULL);
        }
        CHECK(sc.count == 12, "解除注入后继续 push: count=12");
        CHECK(sc.stars[4].ra == 30.0 && sc.stars[11].ra == 37.0, "数据完整性: 首尾星点正确");

        spec_collector_free(&sc);
        CHECK(sc.stars == NULL && sc.spectra == NULL, "free 后指针清空 (无悬垂)");
    }

    /* Case B: stars 扩容本身失败 —— 无变化路径 (回归保护) */
    {
        SpectrumStarCollector sc;
        spec_collector_init(&sc, 2, WL_TEST);
        for (int i = 0; i < 2; i++)
            spec_collector_push(&sc, 1.0 + i, 2.0 + i, 12.0, 1.0f, 1.0f, NULL);
        gaia_inject_reset(1);  /* 注入 stars realloc 失败 */
        spec_collector_push(&sc, 50.0, 60.0, 12.0, 1.0f, 1.0f, NULL);
        CHECK(sc.count == 2, "stars realloc 失败: count 不变");
        CHECK(sc.stars != NULL, "stars realloc 失败后旧指针仍有效 (realloc 语义)");
        gaia_inject_reset(-1);
        spec_collector_push(&sc, 50.0, 60.0, 12.0, 1.0f, 1.0f, NULL);
        CHECK(sc.count == 3 && sc.stars[2].ra == 50.0, "重试扩容成功且数据正确");
        spec_collector_free(&sc);
    }

    /* Case C: spectrum_count=0 路径 (W1-GAIA-001 语义) 回归 */
    {
        SpectrumStarCollector sc;
        spec_collector_init(&sc, 2, 0);
        CHECK(sc.spectra == NULL && sc.capacity == 2, "init(0): spectra=NULL");
        for (int i = 0; i < 5; i++)
            spec_collector_push(&sc, 1.0 * i, 2.0 * i, 12.0, 1.0f, 1.0f, NULL);
        CHECK(sc.count == 5, "无光谱路径 push x5 (跨扩容)");
        spec_collector_free(&sc);
    }

    printf("=== B4-P1-3 结果: %s (%d failures) ===\n",
           g_failures == 0 ? "ALL PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
