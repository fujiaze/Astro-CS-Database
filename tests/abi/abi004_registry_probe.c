/* ABI-004 动态 registry 验收探针（C11; Linux）
 *
 * 角色: 包装 runtime/registry/module_registry.h 对真实 manifest 目录执行
 * open/check/entry/finding/close, 输出机器可读单行结果, 供
 * tests/abi/test_module_registry.py 断言。不自己判定 PASS/FAIL 语义。
 *
 * 用法:
 *   abi004_registry_probe selftest
 *   abi004_registry_probe open <manifest_abs> <yaml_root|-> <allowed_root|-> <scan|0|1>
 *     成功: OPEN_OK entries=<n> findings=<m>
 *     失败: OPEN_FAIL status=<n> detail=<n>
 *   abi004_registry_probe check <manifest_abs> <yaml_root|-> <allowed_root|-> <scan|0|1>
 *     同 open 后跑 check: CHECK_OK issues=<n>  或  OPEN_FAIL ...
 *   abi004_registry_probe dump <manifest_abs> <yaml_root|-> <allowed_root|-> <scan|0|1>
 *     输出每个 entry 一行: E <idx> <unit_id>|<kind>|<module_id>|<mid_dll>|<mid_yaml>|
 *                          <ver_dll>|<ver_yaml>|<build_dll>|<sha_reg[12]>|<sha_act[12]>|
 *                          <loaded>|<mask>|<detail>
 *     以及 findings: F <idx> kind=<kind> entry=<e> aux=<a>
 * host allocator = malloc/free。
 */
#include "module_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* t_alloc(void* ud, uint64_t size, uint64_t align) {
    (void)ud; (void)align;
    return malloc((size_t)size);
}
static void t_free(void* ud, void* p) { (void)ud; free(p); }

static acs_allocator_v1 g_alloc = {
    { (uint32_t)sizeof(acs_allocator_v1), ACS_ABI_VERSION_V1 },
    t_alloc, t_free, NULL
};

static acs_str_v1 sv(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)strlen(s) : 0;
    return v;
}

static void show_span(const char* tag, acs_str_v1 s) {
    /* 标签列: |<tag>=<值前 24 字符> —— 供 list/verify 输出实际值可机器解析 */
    printf("|%s=", tag);
    printf("%.*s", (int)(s.size > 24 ? 24 : s.size), s.data ? s.data : "");
}

static int open_registry(int argc, char** argv, acs_registry** out,
                         acs_error_info_v1* err) {
    if (argc < 4) return -2;
    const char* man = argv[0];
    const char* yr = argv[1];
    const char* ar = argv[2];
    int scan = atoi(argv[3]);

    acs_registry_options_v1 opt;
    memset(&opt, 0, sizeof(opt));
    opt.head.struct_size = (uint32_t)sizeof(acs_registry_options_v1);
    opt.head.abi_version = ACS_ABI_VERSION_V1;
    opt.manifest_abs_path = sv(man);
    opt.module_yaml_root = sv(strcmp(yr, "-") == 0 ? "" : yr);
    opt.allowed_root = sv(strcmp(ar, "-") == 0 ? "" : ar);
    opt.scan_unregistered = scan;
    opt.allocator = &g_alloc;

    memset(err, 0, sizeof(*err));
    acs_registry* r = NULL;
    acs_status st = acs_registry_open_v1(&opt, err, &r);
    if (st != ACS_OK) {
        printf("OPEN_FAIL status=%d detail=%u\n", (int)st, err->detail_code);
        return -1;
    }
    *out = r;
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage\n"); return 2; }
    if (strcmp(argv[1], "selftest") == 0) {
        if (acs_registry_self_test_v1() == ACS_OK) { printf("SELFTEST_OK\n"); return 0; }
        printf("SELFTEST_FAIL\n");
        return 1;
    }
    if (strcmp(argv[1], "open") == 0 || strcmp(argv[1], "check") == 0 ||
        strcmp(argv[1], "dump") == 0) {
        if (argc < 6) { fprintf(stderr, "usage: %s %s <manifest> <yaml_root> <root> <scan>\n",
                                argv[0], argv[1]); return 2; }
        acs_error_info_v1 err;
        acs_registry* r = NULL;
        int rc = open_registry(argc - 2, argv + 2, &r, &err);
        if (rc == -2) { printf("OPEN_FAIL status=1 detail=1 (usage)\n"); return 1; }
        if (rc != 0) return 0;   /* open 已打印失败 */
        uint32_t entries = 0, issues = 0;
        acs_registry_entry_count_v1(r, &entries);
        acs_registry_check_v1(r, &err, &issues);
        if (strcmp(argv[1], "open") == 0) {
            printf("OPEN_OK entries=%u findings=%u\n", entries, issues);
        } else if (strcmp(argv[1], "check") == 0) {
            printf("CHECK_OK entries=%u issues=%u\n", entries, issues);
            /* check 也打印 findings(机器可读), 供测试断言检测结果 */
            uint32_t i;
            for (i = 0; i < issues; ++i) {
                acs_registry_finding_v1 f;
                memset(&f, 0, sizeof(f));
                f.head.struct_size = (uint32_t)sizeof(acs_registry_finding_v1);
                f.head.abi_version = ACS_ABI_VERSION_V1;
                if (acs_registry_get_finding_v1(r, i, &f) != ACS_OK) continue;
                printf("F %u kind=%u entry=%d aux=%d\n", i, f.kind,
                       f.entry_index, f.aux_index);
            }
        } else {
            printf("DUMP_OK entries=%u findings=%u\n", entries, issues);
            uint32_t i;
            for (i = 0; i < entries; ++i) {
                acs_registry_entry_v1 e;
                memset(&e, 0, sizeof(e));
                e.head.struct_size = (uint32_t)sizeof(acs_registry_entry_v1);
                e.head.abi_version = ACS_ABI_VERSION_V1;
                if (acs_registry_get_entry_v1(r, i, &e) != ACS_OK) continue;
                printf("E %u ", i);
                show_span("unit_id", e.unit_id); show_span("kind", e.kind);
                show_span("module_id", e.module_id); show_span("mid_dll", e.module_id_dll);
                show_span("mid_yaml", e.module_id_yaml); show_span("ver_dll", e.version_dll);
                show_span("ver_yaml", e.version_yaml); show_span("build_dll", e.build_id_dll);
                show_span("sha_reg", e.sha256_registered); show_span("sha_act", e.sha256_actual);
                printf("|loaded=%u|mask=%d|detail=%u\n", e.loaded,
                       e.finding_mask, e.detail_code);
            }
            for (i = 0; i < issues; ++i) {
                acs_registry_finding_v1 f;
                memset(&f, 0, sizeof(f));
                f.head.struct_size = (uint32_t)sizeof(acs_registry_finding_v1);
                f.head.abi_version = ACS_ABI_VERSION_V1;
                if (acs_registry_get_finding_v1(r, i, &f) != ACS_OK) continue;
                printf("F %u kind=%u entry=%d aux=%d\n", i, f.kind,
                       f.entry_index, f.aux_index);
            }
        }
        acs_registry_close_v1(r);
        printf("CLOSE_OK\n");
        return 0;
    }
    fprintf(stderr, "unknown: %s\n", argv[1]);
    return 2;
}
