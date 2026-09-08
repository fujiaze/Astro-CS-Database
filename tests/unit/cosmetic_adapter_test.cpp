/* cosmetic_adapter_test.cpp - P1-COS-IMPL 模块适配器测试
 * (对齐 calibration_adapter_test / drizzle_adapter_test / gaia_adapter_test 先例)
 *
 * 覆盖:
 *   A. C ABI 入口契约: host_abi 失配 → ACS_ERR_ABI_MISMATCH; out_api=NULL →
 *      ACS_ERR_PARAM; 正常 query → vtable; describe 错 id → MISMATCH;
 *      descriptor 三方一致字段 (module_id/version/build_id/sci/alg/api,
 *      phase=1 cpu_heavy parallel_ok=1)。
 *   B. 导出面净化: dlsym(legacy 4 符号) 必须失败 (AC_API= 本地化 +
 *      version-script 双保险, ABI-006); 唯一导出 astrocs_module_query_v1。
 *   C. validate_config 负例冻结 detail 码: 词表外 op=100 / 缺键=101 /
 *      类型错=102 / 范围错=103 / method 词表外=103 (DISP-COS-003 处置);
 *      空 config → NULL_CONFIG(2); 未知键/重复键 → CONFIG_SCHEMA(3)。
 *   D. plan: 两阶段 strbuf (探测→整写); work_units=w*h 数据事实;
 *      work_unit=parallel_axis="pixel"; min_workers=1; plan_version=1;
 *      cancel_support="entry"。
 *   E. execute direct-vs-plugin BITWISE: op=correct_frame (f32) 与
 *      correct_frame_f64 同 fixture 直接调 legacy AC_API vs 模块 execute
 *      输出 base64 解码后 memcmp 逐位一致; hot/cold 计数一致; NULL-master
 *      恒等通道 (键缺席 = NULL); cancel → CANCELLED 无输出可复用;
 *      budget: acquire 失败 / executor 缺失 → BUDGET+105 (cpu_heavy 禁
 *      单线程, CAL 先例); 租约 acquire/release 平衡; inspect exec_count;
 *      状态护栏 (create config 缺失 / manifest 尺寸与 dtype)。
 *
 * DLL 经环境变量 ASTROSCOS_DLL_PATH (CMake test properties 注入)。
 * direct 通道: #include 生产头 + 链 astrocs_calibration STATIC。
 * OMP 教训 (P1-CAL-INT 58d20223): 本 TU 链 astrocs_calibration (OMP 符号
 * 引用) 且注册段显式 LINKER:--no-as-needed, 防 DLL 内 GOMP 晚装载 SEGV。
 */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "astro_calibration.h"
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/host_api_v1.h"
#include "astrocs/cosmetic/types.h"

typedef acs_status (*cos_entry_fn)(uint32_t, const acs_host_api_v1*,
                                   const acs_module_api_v1**);

static int g_fail = 0;
#define CHECK(cond, name) do { \
    if (cond) printf("PASS: %s\n", (name)); \
    else { printf("FAIL: %s\n", (name)); g_fail++; } \
} while (0)

/* ── str 构造 helper ── */
static acs_str_v1 cos_str(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s; v.size = (uint64_t)strlen(s);
    return v;
}
#define cos_cfg(s) cos_str(s)

/* ── host stub (allocator + executor 计数 + cancel 通道) ── */
typedef struct {
    acs_host_api_v1 api;
    acs_executor_v1 executor;
    acs_allocator_v1 allocator;
    acs_cancel_v1   cancel;
    int acquire_calls, release_calls;
    int fail_acquire;
    int cancelled;
} cos_test_host;

static void* cos_alloc(void* user, uint64_t size, uint64_t align) {
    (void)user; (void)align;
    return size ? malloc((size_t)size) : NULL;
}
static void cos_free(void* user, void* p) { (void)user; free(p); }

static int cos_acquire(void* user, uint32_t n) {
    cos_test_host* h = (cos_test_host*)user;
    h->acquire_calls++;
    if (h->fail_acquire) return -1;
    (void)n;
    return 0;
}
static void cos_release(void* user, uint32_t n) {
    cos_test_host* h = (cos_test_host*)user;
    h->release_calls++;
    (void)n;
}
static int cos_is_cancelled(void* user) {
    cos_test_host* h = (cos_test_host*)user;
    return h->cancelled;
}

static void cos_host_init(cos_test_host* h) {
    memset(h, 0, sizeof(*h));
    h->api.head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    h->api.head.abi_version = ACS_ABI_VERSION_V1;
    h->executor.head.struct_size = (uint32_t)sizeof(acs_executor_v1);
    h->executor.head.abi_version = ACS_ABI_VERSION_V1;
    h->executor.available_cpus = 4;
    h->executor.max_workers = 4;
    h->executor.acquire = cos_acquire;
    h->executor.release = cos_release;
    h->executor.user_data = h;
    h->allocator.head.struct_size = (uint32_t)sizeof(acs_allocator_v1);
    h->allocator.head.abi_version = ACS_ABI_VERSION_V1;
    h->allocator.alloc = cos_alloc;
    h->allocator.free = cos_free;
    h->allocator.user_data = h;
    h->cancel.head.struct_size = (uint32_t)sizeof(acs_cancel_v1);
    h->cancel.head.abi_version = ACS_ABI_VERSION_V1;
    h->cancel.is_cancelled = cos_is_cancelled;
    h->cancel.user_data = h;
    h->api.executor = &h->executor;
    h->api.allocator = &h->allocator;
    h->api.cancel = &h->cancel;
}

/* host 缺 executor 变体 (BUDGET 负例) */
static void cos_host_no_executor(cos_test_host* h) {
    cos_host_init(h);
    h->api.executor = NULL;
}

/* ── fixture (P1-COS-TEST 谱系外自足; 固定值域, 无随机) ── */
typedef struct {
    int w, h;
    std::vector<float> data, dark, bias;
    std::vector<double> data64, dark64, bias64;
} cos_fix;

static void fix_build(cos_fix* fx, int w, int h) {
    fx->w = w; fx->h = h;
    const int n = w * h;
    fx->data.resize(n); fx->dark.resize(n); fx->bias.resize(n);
    for (int i = 0; i < n; ++i) {
        /* 确定性行主序光变面: 100 + 平滑梯度 + 低幅伪噪声 (无 RNG) */
        const int x = i % w, y = i / w;
        fx->data[i] = 100.0f + 0.25f * x + 0.125f * y + ((i * 7) % 5) * 0.05f;
        fx->dark[i] = 5.0f + 0.01f * x;
        fx->bias[i] = 2.0f + 0.01f * y;
    }
    /* 少量离群: 触发热 (dark 高值) / 冷 (bias 低值) 检测 */
    fx->dark[3 * w + 4] = 500.0f;
    fx->dark[(h - 2) * w + (w - 3)] = 450.0f;
    fx->bias[1 * w + 2] = -300.0f;
    fx->data64.assign(fx->data.begin(), fx->data.end());
    fx->dark64.assign(fx->dark.begin(), fx->dark.end());
    fx->bias64.assign(fx->bias.begin(), fx->bias.end());
}

/* ── base64 编码 (测试侧独立实现; RFC 4648) ── */
static std::string b64e(const uint8_t* p, size_t n) {
    static const char* T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    o.reserve(((n + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 3 <= n; i += 3) {
        const uint32_t v = ((uint32_t)p[i] << 16) | ((uint32_t)p[i + 1] << 8) | p[i + 2];
        o += T[(v >> 18) & 63]; o += T[(v >> 12) & 63];
        o += T[(v >> 6) & 63];  o += T[v & 63];
    }
    if (n - i == 1) {
        const uint32_t v = (uint32_t)p[i] << 16;
        o += T[(v >> 18) & 63]; o += T[(v >> 12) & 63]; o += "==";
    } else if (n - i == 2) {
        const uint32_t v = ((uint32_t)p[i] << 16) | ((uint32_t)p[i + 1] << 8);
        o += T[(v >> 18) & 63]; o += T[(v >> 12) & 63];
        o += T[(v >> 6) & 63];  o += "=";
    }
    return o;
}

static int b64v(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
static bool b64d(const std::string& s, std::vector<uint8_t>* out) {
    out->clear();
    uint32_t acc = 0; int bits = 0;
    for (char c : s) {
        if (c == '=') break;
        const int v = b64v(c);
        if (v < 0) return false;
        acc = (acc << 6) | (uint32_t)v; bits += 6;
        if (bits >= 8) { bits -= 8; out->push_back((uint8_t)((acc >> bits) & 0xFF)); }
    }
    return true;
}

/* manifest 构造 (f32/f64 字节平面; master 键可选) */
static std::string make_manifest(const cos_fix& fx, bool f64,
                                 const void* dark, const void* bias) {
    const int n = fx.w * fx.h;
    const uint8_t* db = f64 ? (const uint8_t*)fx.data64.data()
                            : (const uint8_t*)fx.data.data();
    const size_t bs = n * (f64 ? 8 : 4);
    std::string s = "{\"schema_version\":1,\"width\":";
    s += std::to_string(fx.w);
    s += ",\"height\":" + std::to_string(fx.h);
    s += ",\"dtype\":\"" + std::string(f64 ? "f64" : "f32") + "\"";
    s += ",\"data_base64\":\"" + b64e(db, bs) + "\"";
    if (dark) {
        s += ",\"master_dark\":\"" + b64e((const uint8_t*)dark, bs) + "\"";
    }
    if (bias) {
        s += ",\"master_bias\":\"" + b64e((const uint8_t*)bias, bs) + "\"";
    }
    s += "}";
    return s;
}

/* 从输出 manifest 提取 out_base64 / hot / cold (前缀定位; 键序冻结) */
static bool extract_out(const std::string& js, std::string* b64,
                        long long* hot, long long* cold) {
    const size_t k = js.find("\"out_base64\":\"");
    if (k == std::string::npos) return false;
    const size_t v0 = k + strlen("\"out_base64\":\"");
    const size_t v1 = js.find('"', v0);
    if (v1 == std::string::npos) return false;
    *b64 = js.substr(v0, v1 - v0);
    const size_t kh = js.find("\"hot\":");
    const size_t kc = js.find("\"cold\":");
    if (kh == std::string::npos || kc == std::string::npos) return false;
    *hot = atoll(js.c_str() + kh + 6);
    *cold = atoll(js.c_str() + kc + 7);
    return true;
}

static const char* CFG32 =
    "{\"op\":\"correct_frame\",\"width\":%d,\"height\":%d,"
    "\"hot_sigma\":5.0,\"cold_sigma\":4.0,\"method\":\"median\","
    "\"max_structure_size\":9}";
static const char* CFG64 =
    "{\"op\":\"correct_frame_f64\",\"width\":%d,\"height\":%d,"
    "\"hot_sigma\":5.0,\"cold_sigma\":4.0,\"method\":\"median\","
    "\"max_structure_size\":9}";

int main() {
    const char* dll_path = getenv("ASTROSCOS_DLL_PATH");
    if (!dll_path) { printf("FAIL: ASTROSCOS_DLL_PATH not set\n"); return 1; }

#ifdef _WIN32
    HMODULE dl = LoadLibraryA(dll_path);
#else
    void* dl = dlopen(dll_path, RTLD_NOW);
#endif
    CHECK(dl != NULL, "A: dlopen astrocs_p1_cosmetic");
    if (!dl) {
#ifdef _WIN32
        printf("  LoadLibrary error=%lu\n", (unsigned long)GetLastError());
#else
        printf("  dlerror=%s\n", dlerror());
#endif
        return 1;
    }

#ifdef _WIN32
    cos_entry_fn q = (cos_entry_fn)GetProcAddress(dl, "astrocs_module_query_v1");
#else
    cos_entry_fn q = (cos_entry_fn)dlsym(dl, "astrocs_module_query_v1");
#endif
    CHECK(q != NULL, "A: dlsym astrocs_module_query_v1");
    if (!q) return 1;

    /* ── A. 入口契约 ── */
    {
        const acs_module_api_v1* api = NULL;
        CHECK(q(ACS_ABI_VERSION_V1 + 99u, NULL, &api) == ACS_ERR_ABI_MISMATCH &&
              api == NULL,
              "A: host_abi mismatch -> ABI_MISMATCH, out=NULL");
        CHECK(q(ACS_ABI_VERSION_V1, NULL, NULL) == ACS_ERR_PARAM,
              "A: out_api NULL -> PARAM");
        CHECK(q(ACS_ABI_VERSION_V1, NULL, &api) == ACS_OK && api != NULL,
              "A: query ok -> vtable");

        acs_module_descriptor_v1 desc;
        memset(&desc, 0, sizeof(desc));
        CHECK(api->describe(api, cos_str("astrocs.p1.wrong"), &desc) ==
              ACS_ERR_ABI_MISMATCH,
              "A: describe wrong module_id -> MISMATCH");
        CHECK(api->describe(api, cos_str(ASTROCS_COS_MODULE_ID), &desc) == ACS_OK,
              "A: describe ok");
        CHECK(desc.module_id.size == strlen(ASTROCS_COS_MODULE_ID) &&
              memcmp(desc.module_id.data, ASTROCS_COS_MODULE_ID, desc.module_id.size) == 0,
              "A: descriptor module_id 三方一致");
        CHECK(desc.version.size == strlen(ASTROCS_COS_VERSION) &&
              memcmp(desc.version.data, ASTROCS_COS_VERSION, desc.version.size) == 0,
              "A: descriptor version == module.yaml module_version");
        CHECK(desc.build_id.size == strlen(ASTROCS_COS_BUILD_ID) &&
              memcmp(desc.build_id.data, ASTROCS_COS_BUILD_ID, desc.build_id.size) == 0,
              "A: descriptor build_id == P1-COS-IMPL");
        CHECK(desc.sci_id.size == strlen(ASTROCS_COS_SCI_ID) &&
              memcmp(desc.sci_id.data, ASTROCS_COS_SCI_ID, desc.sci_id.size) == 0 &&
              desc.alg_id.size == strlen(ASTROCS_COS_ALG_ID) &&
              memcmp(desc.alg_id.data, ASTROCS_COS_ALG_ID, desc.alg_id.size) == 0 &&
              desc.api_id.size == strlen(ASTROCS_COS_API_ID) &&
              memcmp(desc.api_id.data, ASTROCS_COS_API_ID, desc.api_id.size) == 0,
              "A: descriptor SCI/ALG/API 合同 ID 三方一致");
        CHECK(desc.phase == 1 && desc.execution_class == 0 &&
              desc.parallel_ok == 1 && desc.config_schema_ver == 1,
              "A: descriptor phase=1 cpu_heavy parallel_ok schema_ver=1");
    }

    /* ── B. 导出面净化 ── */
    {
        static const char* LEGACY[] = {
            "ac_correct_frame", "ac_correct_frame_f64",
            "ac_set_num_threads", "ac_version"
        };
        int all_null = 1;
        for (int i = 0; i < 4; ++i) {
#ifdef _WIN32
            void* s = (void*)GetProcAddress(dl, LEGACY[i]);
#else
            void* s = dlsym(dl, LEGACY[i]);
#endif
            if (s) { all_null = 0; printf("  leaked: %s\n", LEGACY[i]); }
        }
        CHECK(all_null, "B: dlsym 4 legacy AC_API 符号全 NULL (ABI-006)");
    }

    const acs_module_api_v1* api = NULL;
    q(ACS_ABI_VERSION_V1, NULL, &api);

    /* ── C. validate_config 负例 ── */
    {
        acs_error_info_v1 err;
        char cb[256];

        CHECK(api->validate_config(api, cos_cfg(""), &err) == ACS_ERR_PARAM &&
              err.detail_code == ACS_DIAG_ECODE_NULL_CONFIG,
              "C: empty config -> PARAM/NULL_CONFIG");

        snprintf(cb, sizeof(cb), CFG32, 16, 12);
        CHECK(api->validate_config(api, cos_cfg(cb), &err) == ACS_OK,
              "C: valid f32 config -> OK");

        snprintf(cb, sizeof(cb), CFG64, 9, 7);
        CHECK(api->validate_config(api, cos_cfg(cb), &err) == ACS_OK,
              "C: valid f64 config (奇数尺寸) -> OK");

        /* op 词表外 → detail 100 */
        snprintf(cb, sizeof(cb),
                 "{\"op\":\"detect_hot_pixels\",\"width\":16,\"height\":12,"
                 "\"hot_sigma\":5.0,\"cold_sigma\":4.0,\"method\":\"median\","
                 "\"max_structure_size\":9}");
        CHECK(api->validate_config(api, cos_cfg(cb), &err) == ACS_ERR_PARAM &&
              err.detail_code == 100,
              "C: op outside vocabulary -> PARAM/100 (词表 2 op)");

        /* 缺键 → detail 101 */
        snprintf(cb, sizeof(cb),
                 "{\"op\":\"correct_frame\",\"width\":16,\"height\":12,"
                 "\"hot_sigma\":5.0,\"cold_sigma\":4.0,\"method\":\"median\"}");
        CHECK(api->validate_config(api, cos_cfg(cb), &err) == ACS_ERR_PARAM &&
              err.detail_code == 101,
              "C: missing max_structure_size -> PARAM/101");

        /* 类型错 → detail 102 */
        snprintf(cb, sizeof(cb),
                 "{\"op\":\"correct_frame\",\"width\":\"16\",\"height\":12,"
                 "\"hot_sigma\":5.0,\"cold_sigma\":4.0,\"method\":\"median\","
                 "\"max_structure_size\":9}");
        CHECK(api->validate_config(api, cos_cfg(cb), &err) == ACS_ERR_PARAM &&
              err.detail_code == 102,
              "C: width string type -> PARAM/102");

        /* 范围错 → detail 103 */
        snprintf(cb, sizeof(cb),
                 "{\"op\":\"correct_frame\",\"width\":0,\"height\":12,"
                 "\"hot_sigma\":5.0,\"cold_sigma\":4.0,\"method\":\"median\","
                 "\"max_structure_size\":9}");
        CHECK(api->validate_config(api, cos_cfg(cb), &err) == ACS_ERR_PARAM &&
              err.detail_code == 103,
              "C: width=0 -> PARAM/103");

        /* method 词表外 (DISP-COS-003 处置: adapter 词表层拒绝) */
        snprintf(cb, sizeof(cb),
                 "{\"op\":\"correct_frame\",\"width\":16,\"height\":12,"
                 "\"hot_sigma\":5.0,\"cold_sigma\":4.0,\"method\":\"cubic\","
                 "\"max_structure_size\":9}");
        CHECK(api->validate_config(api, cos_cfg(cb), &err) == ACS_ERR_PARAM &&
              err.detail_code == 103,
              "C: method=cubic -> PARAM/103 (词表 median|bilinear)");

        /* 未知键 → CONFIG_SCHEMA */
        snprintf(cb, sizeof(cb),
                 "{\"op\":\"correct_frame\",\"width\":16,\"height\":12,"
                 "\"hot_sigma\":5.0,\"cold_sigma\":4.0,\"method\":\"median\","
                 "\"max_structure_size\":9,\"turbo\":true}");
        CHECK(api->validate_config(api, cos_cfg(cb), &err) == ACS_ERR_PARAM &&
              err.detail_code == ACS_DIAG_ECODE_CONFIG_SCHEMA,
              "C: unknown key -> PARAM/CONFIG_SCHEMA");

        /* 重复键 → CONFIG_SCHEMA */
        CHECK(api->validate_config(api, cos_cfg(
                  "{\"op\":\"correct_frame\",\"op\":\"correct_frame_f64\","
                  "\"width\":16,\"height\":12,\"hot_sigma\":5.0,"
                  "\"cold_sigma\":4.0,\"method\":\"median\","
                  "\"max_structure_size\":9}"), &err) == ACS_ERR_PARAM &&
              err.detail_code == ACS_DIAG_ECODE_CONFIG_SCHEMA,
              "C: duplicate key -> PARAM/CONFIG_SCHEMA");

        /* 嵌套 → 拒 */
        CHECK(api->validate_config(api, cos_cfg(
                  "{\"op\":\"correct_frame\",\"width\":16,\"height\":12,"
                  "\"hot_sigma\":5.0,\"cold_sigma\":4.0,\"method\":\"median\","
                  "\"max_structure_size\":9,\"extra\":{\"a\":1}}"), &err) ==
              ACS_ERR_PARAM,
              "C: nested object -> PARAM");
    }

    /* ── D. plan ── */
    {
        char cb[256];
        snprintf(cb, sizeof(cb), CFG32, 16, 12);
        acs_strbuf_v1 pb;
        memset(&pb, 0, sizeof(pb));
        acs_error_info_v1 err;
        CHECK(api->plan(api, cos_str("node-cos-1"), cos_cfg(cb), &pb, &err) ==
              ACS_OK && pb.size > 0 && pb.data == NULL,
              "D: plan probe phase -> size>0, no write");
        std::vector<char> pbuf((size_t)pb.size + 1);
        pb.data = pbuf.data(); pb.cap = pbuf.size();
        CHECK(api->plan(api, cos_str("node-cos-1"), cos_cfg(cb), &pb, &err) ==
              ACS_OK && strlen(pb.data) == pb.size,
              "D: plan commit phase -> NUL-terminated exact size");
        CHECK(strstr(pb.data, "\"work_units\":192") != NULL,
              "D: work_units=16*12=192 (数据事实)");
        CHECK(strstr(pb.data, "\"work_unit\":\"pixel\"") != NULL &&
              strstr(pb.data, "\"parallel_axis\":\"pixel\"") != NULL,
              "D: work_unit/parallel_axis=pixel");
        CHECK(strstr(pb.data, "\"min_workers\":1") != NULL &&
              strstr(pb.data, "\"plan_version\":1") != NULL &&
              strstr(pb.data, "\"cancel_support\":\"entry\"") != NULL,
              "D: min_workers=1 plan_version=1 cancel_support=entry");
    }

    /* ── E. execute direct-vs-plugin BITWISE ── */
    {
        cos_fix fx;
        fix_build(&fx, 16, 12);
        cos_fix fx2;
        fix_build(&fx2, 9, 7);

        acs_error_info_v1 err;
        char cb[256];

        /* E1: f32 带 dark+bias, direct vs plugin memcmp */
        {
            snprintf(cb, sizeof(cb), CFG32, 16, 12);
            cos_test_host host;
            cos_host_init(&host);
            acs_module_instance_v1* inst = NULL;
            CHECK(api->create(api, cos_cfg(cb), &host.api, &inst, &err) == ACS_OK &&
                  inst != NULL,
                  "E1: create f32");
            std::string man = make_manifest(fx, false,
                                            fx.dark.data(), fx.bias.data());
            acs_strbuf_v1 ob;
            memset(&ob, 0, sizeof(ob));
            std::vector<char> obuf(4 << 20);
            ob.data = obuf.data(); ob.cap = obuf.size();
            const acs_status st = api->execute(inst, cos_str(man.c_str()),
                                               cos_cfg(""), &ob, &err);
            CHECK(st == ACS_OK, "E1: execute f32 OK");

            /* direct 通道 */
            std::vector<float> out(fx.w * fx.h);
            int dh = -1, dc = -1;
            const int rc = ac_correct_frame(fx.data.data(), fx.w, fx.h,
                                            fx.dark.data(), fx.bias.data(),
                                            out.data(), 5.0f, 4.0f,
                                            AC_METHOD_MEDIAN, 9, &dh, &dc);
            CHECK(rc == AC_OK, "E1: direct ac_correct_frame OK");

            std::string b64;
            long long hot = -1, cold = -1;
            CHECK(extract_out(ob.data ? std::string(ob.data) : std::string(),
                              &b64, &hot, &cold),
                  "E1: out manifest hot/cold present");
            std::vector<uint8_t> got;
            CHECK(b64d(b64, &got) &&
                  got.size() == (size_t)(fx.w * fx.h) * 4u,
                  "E1: plugin plane decode size == w*h*4");
            CHECK(memcmp(got.data(), out.data(), got.size()) == 0,
                  "E1: BITWISE plugin == direct (f32, dark+bias)");
            CHECK(hot == dh && cold == dc,
                  "E1: hot/cold counts plugin == direct");

            /* 状态与租约 */
            CHECK(host.acquire_calls == 1 && host.release_calls == 1,
                  "E1: lease acquire/release balanced 1/1");
            char ib[256];
            acs_strbuf_v1 ibb;
            ibb.data = ib; ibb.cap = sizeof(ib); ibb.size = 0;
            CHECK(api->inspect(inst, &ibb, &err) == ACS_OK &&
                  strstr(ib, "\"exec_count\":1") != NULL,
                  "E1: inspect exec_count=1");
            /* 可重复 execute (回 CREATED) */
            ob.size = 0;
            CHECK(api->execute(inst, cos_str(man.c_str()), cos_cfg(""),
                               &ob, &err) == ACS_OK,
                  "E1: re-execute after success (CREATED 闭环)");
            api->destroy(inst);
        }

        /* E2: f64 带 dark+bias, direct vs plugin memcmp (f64 降级位型:
         * DISP-COS-004 — direct 侧同为 ac_correct_frame_f64, 位型同源) */
        {
            snprintf(cb, sizeof(cb), CFG64, 9, 7);
            cos_test_host host;
            cos_host_init(&host);
            acs_module_instance_v1* inst = NULL;
            CHECK(api->create(api, cos_cfg(cb), &host.api, &inst, &err) == ACS_OK,
                  "E2: create f64 (奇数尺寸 9x7)");
            std::string man = make_manifest(fx2, true,
                                            fx2.dark64.data(), fx2.bias64.data());
            acs_strbuf_v1 ob;
            memset(&ob, 0, sizeof(ob));
            std::vector<char> obuf(4 << 20);
            ob.data = obuf.data(); ob.cap = obuf.size();
            CHECK(api->execute(inst, cos_str(man.c_str()), cos_cfg(""),
                               &ob, &err) == ACS_OK,
                  "E2: execute f64 OK");

            std::vector<double> out(fx2.w * fx2.h);
            int dh = -1, dc = -1;
            CHECK(ac_correct_frame_f64(fx2.data64.data(), fx2.w, fx2.h,
                                       fx2.dark64.data(), fx2.bias64.data(),
                                       out.data(), 5.0, 4.0,
                                       AC_METHOD_MEDIAN, 9, &dh, &dc) == AC_OK,
                  "E2: direct ac_correct_frame_f64 OK");
            std::string b64;
            long long hot = -1, cold = -1;
            CHECK(extract_out(ob.data, &b64, &hot, &cold),
                  "E2: out manifest hot/cold present");
            std::vector<uint8_t> got;
            CHECK(b64d(b64, &got) &&
                  got.size() == (size_t)(fx2.w * fx2.h) * 8u,
                  "E2: plugin plane decode size == w*h*8");
            CHECK(memcmp(got.data(), out.data(), got.size()) == 0,
                  "E2: BITWISE plugin == direct (f64, dark+bias)");
            api->destroy(inst);
        }

        /* E3: NULL-master 恒等通道 (键缺席 = legacy NULL → 检测禁用) */
        {
            snprintf(cb, sizeof(cb), CFG32, 16, 12);
            cos_test_host host;
            cos_host_init(&host);
            acs_module_instance_v1* inst = NULL;
            CHECK(api->create(api, cos_cfg(cb), &host.api, &inst, &err) == ACS_OK,
                  "E3: create (no master keys)");
            std::string man = make_manifest(fx, false, NULL, NULL);
            acs_strbuf_v1 ob;
            memset(&ob, 0, sizeof(ob));
            std::vector<char> obuf(4 << 20);
            ob.data = obuf.data(); ob.cap = obuf.size();
            CHECK(api->execute(inst, cos_str(man.c_str()), cos_cfg(""),
                               &ob, &err) == ACS_OK,
                  "E3: execute without master keys OK");
            std::string b64;
            long long hot = -1, cold = -1;
            CHECK(extract_out(ob.data, &b64, &hot, &cold) &&
                  hot == 0 && cold == 0,
                  "E3: hot=cold=0 (检测禁用, DISP-COS-009 行为保持)");
            std::vector<uint8_t> got;
            CHECK(b64d(b64, &got) &&
                  got.size() == (size_t)(fx.w * fx.h) * 4u &&
                  memcmp(got.data(), fx.data.data(), got.size()) == 0,
                  "E3: BITWISE 恒等 out == data");
            api->destroy(inst);
        }

        /* E4: cancel → CANCELLED 无输出; 复用再 execute 成功 */
        {
            snprintf(cb, sizeof(cb), CFG32, 16, 12);
            cos_test_host host;
            cos_host_init(&host);
            host.cancelled = 1;
            acs_module_instance_v1* inst = NULL;
            CHECK(api->create(api, cos_cfg(cb), &host.api, &inst, &err) == ACS_OK,
                  "E4: create");
            std::string man = make_manifest(fx, false,
                                            fx.dark.data(), fx.bias.data());
            acs_strbuf_v1 ob;
            memset(&ob, 0, sizeof(ob));
            std::vector<char> obuf(4 << 20);
            ob.data = obuf.data(); ob.cap = obuf.size();
            obuf[0] = 'X';
            CHECK(api->execute(inst, cos_str(man.c_str()), cos_cfg(""),
                               &ob, &err) == ACS_ERR_CANCELLED &&
                  ob.size == 0 && obuf[0] == 'X',
                  "E4: cancel -> CANCELLED, out 未写 (事务性)");
            host.cancelled = 0;
            ob.size = 0;
            CHECK(api->execute(inst, cos_str(man.c_str()), cos_cfg(""),
                               &ob, &err) == ACS_OK,
                  "E4: instance reusable after cancel");
            api->destroy(inst);
        }

        /* E5: budget — executor 缺失 / acquire 失败 → BUDGET+105 (cpu_heavy
         * 禁单线程, CAL 先例) */
        {
            snprintf(cb, sizeof(cb), CFG32, 16, 12);
            cos_test_host host;
            cos_host_no_executor(&host);
            acs_module_instance_v1* inst = NULL;
            CHECK(api->create(api, cos_cfg(cb), &host.api, &inst, &err) == ACS_OK,
                  "E5: create (no executor)");
            std::string man = make_manifest(fx, false, NULL, NULL);
            acs_strbuf_v1 ob;
            memset(&ob, 0, sizeof(ob));
            std::vector<char> obuf(4 << 20);
            ob.data = obuf.data(); ob.cap = obuf.size();
            CHECK(api->execute(inst, cos_str(man.c_str()), cos_cfg(""),
                               &ob, &err) == ACS_ERR_BUDGET &&
                  err.detail_code == 105 && ob.size == 0,
                  "E5: executor missing -> BUDGET/105, no output");
            api->destroy(inst);

            cos_test_host host2;
            cos_host_init(&host2);
            host2.fail_acquire = 1;
            acs_module_instance_v1* inst2 = NULL;
            CHECK(api->create(api, cos_cfg(cb), &host2.api, &inst2, &err) == ACS_OK,
                  "E5: create (fail_acquire)");
            CHECK(api->execute(inst2, cos_str(man.c_str()), cos_cfg(""),
                               &ob, &err) == ACS_ERR_BUDGET &&
                  err.detail_code == 105 && host2.release_calls == 0,
                  "E5: acquire fail -> BUDGET/105 (禁单线程降级), 无泄漏 release");
            api->destroy(inst2);
        }

        /* E6: manifest 校验负面 — 尺寸/dtype/base64 */
        {
            snprintf(cb, sizeof(cb), CFG32, 16, 12);
            cos_test_host host;
            cos_host_init(&host);
            acs_module_instance_v1* inst = NULL;
            CHECK(api->create(api, cos_cfg(cb), &host.api, &inst, &err) == ACS_OK,
                  "E6: create");
            acs_strbuf_v1 ob;
            memset(&ob, 0, sizeof(ob));
            std::vector<char> obuf(4 << 20);
            ob.data = obuf.data(); ob.cap = obuf.size();

            CHECK(api->execute(inst, cos_str(""), cos_cfg(""),
                               &ob, &err) == ACS_ERR_PARAM,
                  "E6: empty manifest -> PARAM");
            CHECK(api->execute(inst, cos_str("{\"schema_version\":2}"),
                               cos_cfg(""), &ob, &err) == ACS_ERR_PARAM &&
                  err.detail_code == 110,
                  "E6: schema_version=2 -> PARAM/110");
            CHECK(api->execute(inst, cos_str("{\"schema_version\":1,\"width\":15,"
                                             "\"height\":12,\"dtype\":\"f32\","
                                             "\"data_base64\":\"\"}"),
                               cos_cfg(""), &ob, &err) == ACS_ERR_PARAM &&
                  err.detail_code == 111,
                  "E6: manifest width mismatch -> PARAM/111");
            CHECK(api->execute(inst, cos_str("{\"schema_version\":1,\"width\":16,"
                                             "\"height\":12,\"dtype\":\"f64\","
                                             "\"data_base64\":\"\"}"),
                               cos_cfg(""), &ob, &err) == ACS_ERR_PARAM &&
                  err.detail_code == 111,
                  "E6: manifest dtype mismatch -> PARAM/111");
            CHECK(api->execute(inst, cos_str("{\"schema_version\":1,\"width\":16,"
                                             "\"height\":12,\"dtype\":\"f32\","
                                             "\"data_base64\":\"not!b64\"}"),
                               cos_cfg(""), &ob, &err) == ACS_ERR_PARAM,
                  "E6: invalid base64 -> PARAM");
            api->destroy(inst);
        }

        /* E7: 状态护栏 — create 无 config / execute 覆盖 config */
        {
            cos_test_host host;
            cos_host_init(&host);
            acs_module_instance_v1* bad = NULL;
            CHECK(api->create(api, cos_cfg(""), &host.api, &bad, &err) ==
                  ACS_ERR_PARAM &&
                  err.detail_code == ACS_DIAG_ECODE_NULL_CONFIG && bad == NULL,
                  "E7: create empty config -> PARAM/NULL_CONFIG, out=NULL");

            char cb[256];
            snprintf(cb, sizeof(cb), CFG32, 16, 12);
            cos_test_host host2;
            cos_host_init(&host2);
            acs_module_instance_v1* inst = NULL;
            /* create 缺 config, execute 期补齐 (覆盖通道) */
            std::string mk = std::string(cb);
            CHECK(api->create(api, cos_cfg(cb), &host2.api, &inst, &err) == ACS_OK,
                  "E7: create with config");
            cos_fix fx3;
            fix_build(&fx3, 16, 12);
            std::string man = make_manifest(fx3, false, NULL, NULL);
            acs_strbuf_v1 ob;
            memset(&ob, 0, sizeof(ob));
            std::vector<char> obuf(4 << 20);
            ob.data = obuf.data(); ob.cap = obuf.size();
            /* execute 期覆盖词表外 op → 拒, 实例回 CREATED */
            CHECK(api->execute(inst, cos_str(man.c_str()),
                               cos_cfg("{\"op\":\"bogus_op\"}"),
                               &ob, &err) == ACS_ERR_PARAM &&
                  err.detail_code == 100,
                  "E7: execute-override bad op -> PARAM/100");
            ob.size = 0;
            CHECK(api->execute(inst, cos_str(man.c_str()), cos_cfg(cb),
                               &ob, &err) == ACS_OK,
                  "E7: execute-override valid config OK (回 CREATED 复用)");
            api->destroy(inst);
        }
    }

    printf("----\n%s (%d failures)\n", g_fail ? "FAILED" : "ALL PASSED", g_fail);
    return g_fail ? 1 : 0;
}
