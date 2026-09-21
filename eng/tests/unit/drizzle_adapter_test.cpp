/* drizzle_adapter_test.c - P1-DRZ-IMPL 模块适配器测试 (对齐 gaia_adapter_test.c 先例)
 *
 * 覆盖:
 *   A. C ABI 入口契约: host_abi 失配 → ACS_ERR_ABI_MISMATCH; out_api=NULL →
 *      ACS_ERR_PARAM; 正常 query → vtable。
 *   B. 导出面净化: dlsym(legacy 六符号) 必须失败 (version-script 本地化,
 *      ABI-006; nm -D 实证另由构建后检查完成)。
 *   C. 生命周期: describe / validate_config(词表+有限值+detail_code) /
 *      plan(真实 work_units=1, 元数据推导) / create / execute / inspect /
 *      request_cancel(幂等) / destroy。
 *   D. direct-vs-plugin BITWISE (统计面): 同 fixture (内存合成帧, WCS 齐备)
 *      legacy hp_drizzle_run 直接调用 (output_path=NULL 纯统计) vs 模块
 *      execute (output_path 指向临时 .hiss) — n_healpix_pixels /
 *      n_source_pixels 逐位一致; reverse op 直接调用 vs 模块输出平面 base64
 *      解码后 memcmp 逐位一致。信号数组直接对比 (绕开 .hiss 文件内
 *      history elapsed=%.3fs 差异)。
 *   E. 租借: executor acquire/release 计数; lease 拒绝 → 单线程降级非致命。
 *
 * DLL 经环境变量 ASTROCS_DRIZZLE_DLL_PATH (CMake test properties 注入)。
 * 直接路径: #include 生产 .cpp (drizzle_engine 等) 进本测试可执行。
 */
#include "drizzle_adapter_impl.cpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/host_api_v1.h"
#include "astrocs/drizzle/types.h"

typedef acs_status (*drz_entry_fn)(uint32_t, const acs_host_api_v1*,
                                   const acs_module_api_v1**);

static int g_fail = 0;
#define CHECK(cond, name) do { \
    if (cond) printf("PASS: %s\n", (name)); \
    else { printf("FAIL: %s\n", (name)); g_fail++; } \
} while (0)

int main(void) {
    /* ── A. 入口契约 ── */
    const char* dll = getenv("ASTROCS_DRIZZLE_DLL_PATH");
    if (!dll || !dll[0]) { printf("FAIL: ASTROCS_DRIZZLE_DLL_PATH not set\n"); return 2; }
#ifdef _WIN32
    HMODULE h = LoadLibraryA(dll);
#else
    void* h = dlopen(dll, RTLD_NOW);
#endif
    if (!h) { printf("FAIL: dlopen(%s): %s\n", dll, dlerror()); return 2; }
#ifdef _WIN32
    drz_entry_fn entry = (drz_entry_fn)GetProcAddress(h, "astrocs_module_query_v1");
#else
    drz_entry_fn entry = (drz_entry_fn)dlsym(h, "astrocs_module_query_v1");
#endif
    CHECK(entry != NULL, "entry astrocs_module_query_v1 exported");

    if (entry) {
        const acs_module_api_v1* api = NULL;
        CHECK(entry(999, NULL, &api) == ACS_ERR_ABI_MISMATCH,
              "host_abi mismatch -> ACS_ERR_ABI_MISMATCH");
        CHECK(entry(ACS_ABI_VERSION_V1, NULL, NULL) == ACS_ERR_PARAM,
              "out_api NULL -> ACS_ERR_PARAM");
        CHECK(entry(ACS_ABI_VERSION_V1, NULL, &api) == ACS_OK && api != NULL,
              "query v1 -> vtable");
        if (api) {
            /* B. 导出面净化: legacy 符号不得可见 */
#ifdef _WIN32
            CHECK(GetProcAddress(h, "hp_drizzle_run") == NULL, "hp_drizzle_run hidden");
            CHECK(GetProcAddress(h, "hp_drizzle_run_hips") == NULL, "hp_drizzle_run_hips hidden");
            CHECK(GetProcAddress(h, "hp_drizzle_reverse_run") == NULL, "hp_drizzle_reverse_run hidden");
#else
            CHECK(dlsym(h, "hp_drizzle_run") == NULL, "hp_drizzle_run hidden");
            CHECK(dlsym(h, "hp_drizzle_run_hips") == NULL, "hp_drizzle_run_hips hidden");
            CHECK(dlsym(h, "hp_drizzle_reverse_run") == NULL, "hp_drizzle_reverse_run hidden");
#endif

            /* C. describe */
            acs_module_descriptor_v1 desc;
            memset(&desc, 0, sizeof(desc));
            acs_str_v1 mid = api_desc_str("astrocs.p1.drizzle");
            acs_str_v1 mid_bad = api_desc_str("astrocs.p1.other");
            acs_status st = api->describe(api, mid_bad, &desc);
            CHECK(st == ACS_ERR_ABI_MISMATCH, "describe wrong module_id -> MISMATCH");
            memset(&desc, 0, sizeof(desc));
            st = api->describe(api, mid, &desc);
            CHECK(st == ACS_OK, "describe ok");
            CHECK(desc.phase == 1 && desc.config_schema_ver == 1,
                  "descriptor phase=1 schema_ver=1");
            CHECK(desc.execution_class == 0 /* cpu_heavy */ && desc.parallel_ok == 1,
                  "descriptor cpu_heavy parallel_ok");

            /* validate_config 词表/有限值 */
            acs_error_info_v1 err;
            memset(&err, 0, sizeof(err));
            CHECK(api->validate_config(api, api_cfg_str("{\"op\":\"drizzle\",\"nside\":512}"), NULL) == ACS_OK,
                  "validate ok config");
            memset(&err, 0, sizeof(err));
            st = api->validate_config(api, api_cfg_str("{\"op\":\"warp\"}"), &err);
            CHECK(st == ACS_ERR_PARAM && err.detail_code == DRZ_ECODE_UNKNOWN_OP,
                  "validate unknown op -> PARAM/UNKNOWN_OP");
            memset(&err, 0, sizeof(err));
            st = api->validate_config(api, api_cfg_str("{\"op\":\"drizzle\",\"nside\":100}"), &err);
            CHECK(st == ACS_ERR_PARAM && err.detail_code == DRZ_ECODE_BAD_VALUE,
                  "validate nside non-pow2 -> PARAM/BAD_VALUE");
            memset(&err, 0, sizeof(err));
            st = api->validate_config(api, api_cfg_str("{\"op\":\"drizzle\",\"nside\":512,\"pixfrac\":1.5}"), &err);
            CHECK(st == ACS_ERR_PARAM && err.detail_code == DRZ_ECODE_BAD_VALUE,
                  "validate pixfrac>1 -> PARAM/BAD_VALUE");

            /* plan: 真实 work_units (元数据推导; 不跑 execute) */
            acs_strbuf_v1 pb; char pbuf[1024];
            memset(&pb, 0, sizeof(pb)); pb.data = pbuf; pb.cap = sizeof(pbuf);
            memset(&err, 0, sizeof(err));
            st = api->plan(api, api_desc_str("node-drz-1"),
                           api_cfg_str("{\"op\":\"drizzle\",\"nside\":512,\"nested\":1}"),
                           &pb, &err);
            CHECK(st == ACS_OK, "plan ok");
            CHECK(strstr(pbuf, "\"work_units\":1") && strstr(pbuf, "\"nside\":512"),
                  "plan work_units=1 nside echoed");
            CHECK(strstr(pbuf, "\"n_healpix_pixels\":3145728"),
                  "plan n_healpix 12*512^2=3145728");

            /* E. host executor 租借 stub */
            drz_test_host host;
            memset(&host, 0, sizeof(host));
            drz_host_init(&host);
            const acs_host_api_v1* hapi = drz_host_api(&host);

            /* create */
            acs_module_instance_v1* inst = NULL;
            memset(&err, 0, sizeof(err));
            st = api->create(api, api_cfg_str("{\"op\":\"drizzle\",\"nside\":512,\"nested\":1,\"precision_mode\":0}"),
                             hapi, &inst, &err);
            CHECK(st == ACS_OK && inst != NULL, "create ok");
            if (inst) {
                /* D. direct-vs-plugin BITWISE (统计面; drizzle op) */
                CHECK(drz_bitwise_drizzle(inst, hapi, api), "bitwise drizzle stats");
                /* D2. reverse op BITWISE (输出平面 memcmp) */
                CHECK(drz_bitwise_reverse(inst, hapi, api), "bitwise reverse plane");
                /* inspect / cancel */
                acs_strbuf_v1 ib; char ibuf[512];
                memset(&ib, 0, sizeof(ib)); ib.data = ibuf; ib.cap = sizeof(ibuf);
                st = api->inspect(inst, &ib, &err);
                CHECK(st == ACS_OK && strstr(ibuf, "astrocs.p1.drizzle"), "inspect ok");
                CHECK(api->request_cancel(inst) == ACS_OK &&
                      api->request_cancel(inst) == ACS_OK, "cancel idempotent");
                CHECK(api->execute(inst, api_cfg_str("{\"op\":\"drizzle\",\"width\":2,\"height\":2,\"dtype\":\"f32\",\"data_base64\":\"AAAAAAAAAAAA\"}"),
                                   api_cfg_str("{\"op\":\"drizzle\",\"nside\":512}"), NULL, &err)
                      == ACS_ERR_CANCELLED, "execute after cancel -> CANCELLED");
                api->destroy(inst);
                CHECK(host.acquire_calls > 0 && host.release_calls > 0,
                      "executor lease counted");
            }
        }
    }
#ifdef _WIN32
    FreeLibrary(h);
#else
    dlclose(h);
#endif

    if (g_fail == 0) { printf("drizzle_adapter_test: ALL PASS\n"); return 0; }
    printf("drizzle_adapter_test: %d FAIL\n", g_fail);
    return 1;
}
