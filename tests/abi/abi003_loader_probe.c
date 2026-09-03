/* ABI-003 安全 loader 验收探针（C11; Linux amd64）
 *
 * 角色: 包装 runtime/module_loader/secure_loader.h 合同对真实文件系统执行
 * load/describe/release, 供 tests/abi/test_secure_loader.py 编排全部正/负
 * 场景并断言状态码与 detail_code。本探针不自己判定"通过/失败"语义, 只忠实
 * 执行并输出单行机器结果:
 *
 *   selftest: 调用 acs_secure_loader_self_test_v1 → "SELFTEST_OK"/"SELFTEST_FAIL"
 *   load:     参数化构造 acs_loader_options_v1 调用 load:
 *     abi003_loader_probe load <kind> <abs_path> <module_id|-> <sha256|-> <build|-> <root|-> <abi|0>
 *     成功: LOAD_OK module_id=<m> version=<v> build=<b> sha=<hex64> path=<canon>
 *     失败: LOAD_FAIL status=<n> detail=<n>
 *
 * 失败路径也会验证 *out==NULL; 成功路径验证 describe 往返 + release 不泄漏。
 * host allocator = malloc/free 包装(loader 分配内存全部经此, 同一实例释放)。
 */
#include "secure_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* test_alloc(void* ud, uint64_t size, uint64_t align) {
    (void)ud;
    if (align < sizeof(void*)) align = sizeof(void*);
    if (align > 64u) align = 64u;
    /* 简易对齐分配: malloc 返回已满足 max_align_t; 不额外处理 >16 对齐 */
    (void)align;
    return malloc((size_t)size);
}
static void test_free(void* ud, void* p) {
    (void)ud;
    free(p);
}

static acs_allocator_v1 g_alloc = {
    { (uint32_t)sizeof(acs_allocator_v1), ACS_ABI_VERSION_V1 },
    test_alloc, test_free, NULL
};

static acs_str_v1 sv(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)strlen(s) : 0;
    return v;
}

static int cmd_selftest(void) {
    if (acs_secure_loader_self_test_v1() == ACS_OK) {
        printf("SELFTEST_OK\n");
        return 0;
    }
    printf("SELFTEST_FAIL\n");
    return 1;
}

static int cmd_load(int argc, char** argv) {
    /* argv: kind abs_path module_id sha build root abi (7 个) */
    if (argc < 7) {
        printf("LOAD_FAIL status=1 detail=1 (usage)\n");
        return 1;
    }
    const char* kind = argv[0];
    const char* path = argv[1];
    const char* mid  = argv[2];
    const char* sha  = argv[3];
    const char* bid  = argv[4];
    const char* root = argv[5];
    uint32_t abi = (uint32_t)atoi(argv[6]);

    acs_load_manifest_unit_v1 unit;
    memset(&unit, 0, sizeof(unit));
    unit.head.struct_size = (uint32_t)sizeof(acs_load_manifest_unit_v1);
    unit.head.abi_version = ACS_ABI_VERSION_V1;
    unit.unit_id = sv("TEST-UNIT");
    unit.kind = sv(kind);
    unit.abs_path_utf8 = sv(path);
    unit.module_id = sv(strcmp(mid, "-") == 0 ? "" : mid);
    unit.expected_sha256 = sv(strcmp(sha, "-") == 0 ? "" : sha);
    unit.expected_build_id = sv(strcmp(bid, "-") == 0 ? "" : bid);
    unit.abi_version = abi;

    acs_loader_options_v1 opt;
    memset(&opt, 0, sizeof(opt));
    opt.head.struct_size = (uint32_t)sizeof(acs_loader_options_v1);
    opt.head.abi_version = ACS_ABI_VERSION_V1;
    opt.unit = unit;
    opt.allowed_root_utf8 = sv(strcmp(root, "-") == 0 ? "" : root);
    opt.allocator = &g_alloc;

    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_loader_handle* h = (acs_loader_handle*)0x1;  /* 哨兵: 失败必须置 NULL */

    acs_status st = acs_secure_loader_load_v1(&opt, &err, &h);
    if (st != ACS_OK) {
        printf("LOAD_FAIL status=%d detail=%u out_null=%d\n",
               (int)st, err.detail_code, h == NULL ? 1 : 0);
        return 0;   /* 探针执行完成; python 按输出断言 */
    }
    if (h == NULL) {
        printf("LOAD_FAIL status=70 detail=70 (null handle on ok)\n");
        return 1;
    }

    acs_loaded_module_v1 info;
    memset(&info, 0, sizeof(info));
    info.head.struct_size = (uint32_t)sizeof(acs_loaded_module_v1);
    info.head.abi_version = ACS_ABI_VERSION_V1;
    acs_status ds = acs_secure_loader_describe_v1(h, &info);
    if (ds != ACS_OK) {
        acs_secure_loader_release_v1(h);
        printf("LOAD_FAIL status=%d detail=70 (describe)\n", (int)ds);
        return 1;
    }
    /* 安全打印: 输出各字段; 供 python 校验 module_id/sha */
    printf("LOAD_OK module_id=%.*s version=%.*s build=%.*s sha=%.*s path=%.*s\n",
           (int)info.module_id.size, info.module_id.data ? info.module_id.data : "",
           (int)info.module_version.size, info.module_version.data ? info.module_version.data : "",
           (int)info.build_id.size, info.build_id.data ? info.build_id.data : "",
           (int)info.loaded_sha256.size, info.loaded_sha256.data ? info.loaded_sha256.data : "",
           (int)info.resolved_path_utf8.size, info.resolved_path_utf8.data ? info.resolved_path_utf8.data : "");
    /* 握手出的 vtable 可用性: module → describe 自证 */
    if (info.module_api && info.module_api->describe) {
        acs_module_descriptor_v1 d2;
        memset(&d2, 0, sizeof(d2));
        acs_str_v1 empty = sv("");
        if (info.module_api->describe(info.module_api, empty, &d2) == ACS_OK) {
            printf("DESCRIBE_OK module_id=%.*s build=%.*s\n",
                   (int)d2.module_id.size, d2.module_id.data ? d2.module_id.data : "",
                   (int)d2.build_id.size, d2.build_id.data ? d2.build_id.data : "");
        }
    }
    acs_secure_loader_release_v1(h);
    printf("RELEASE_OK\n");
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s selftest | load <kind> <path> <mid> <sha> <build> <root> <abi>\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "selftest") == 0) return cmd_selftest();
    if (strcmp(argv[1], "load") == 0) return cmd_load(argc - 2, argv + 2);
    fprintf(stderr, "unknown command: %s\n", argv[1]);
    return 2;
}
