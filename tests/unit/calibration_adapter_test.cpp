/* calibration_adapter_test.cpp - P1-CAL-IMPL 模块适配器测试
 * (对齐 drizzle_adapter_test / gaia_adapter_test 先例)
 *
 * 覆盖:
 *   A. C ABI 入口契约: host_abi 失配 → ACS_ERR_ABI_MISMATCH; out_api=NULL →
 *      ACS_ERR_PARAM; 正常 query → vtable; describe 错 id → MISMATCH;
 *      descriptor 三方一致字段 (phase=1 cpu_heavy parallel_ok)。
 *   B. 导出面净化: dlsym(legacy 12 符号) 必须失败 (AC_API= 本地化 +
 *      version-script 双保险, ABI-006)。
 *   C. validate_config 负例冻结 detail 码: 词表外 op=100 / 缺键=101 /
 *      类型错=102(经 schema CONFIG_SCHEMA? 冻结为 102 时断言之) / 范围错=103;
 *      空 config → NULL_CONFIG(2)。
 *   D. plan: 两阶段 strbuf (探测→整写); work_units=数据事实 (master→frames,
 *      帧级→w*h); min_workers=1; cancel_support=entry; plan_version=1。
 *   E. execute direct-vs-plugin BITWISE: 全部 10 科学 op, 同 fixture 直接调
 *      legacy AC_API vs 模块 execute 输出 base64 解码后 memcmp 逐位一致;
 *      NULL-master 恒等通道 (键缺席 = NULL); cancel → CANCELLED 无输出可复用;
 *      budget: acquire 失败 / executor 缺失 → BUDGET+105; 租约 acquire/release
 *      平衡; inspect exec_count; 状态护栏 (create config 缺失)。
 *
 * DLL 经环境变量 ASTROCS_CAL_DLL_PATH (CMake test properties 注入)。
 * direct 通道: #include 生产头 + 链 astrocs_calibration STATIC。
 */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "astro_calibration.h"
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/host_api_v1.h"
#include "astrocs/calibration/types.h"

typedef acs_status (*cal_entry_fn)(uint32_t, const acs_host_api_v1*,
                                   const acs_module_api_v1**);

static int g_fail = 0;
#define CHECK(cond, name) do { \
    if (cond) printf("PASS: %s\n", (name)); \
    else { printf("FAIL: %s\n", (name)); g_fail++; } \
} while (0)

/* ── str 构造 helper ── */
static acs_str_v1 cal_str(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s; v.size = (uint64_t)strlen(s);
    return v;
}
#define cal_cfg(s) cal_str(s)

/* ── host stub (allocator + executor 计数 + cancel 通道) ── */
typedef struct {
    acs_host_api_v1 api;
    acs_executor_v1 executor;
    acs_allocator_v1 allocator;
    acs_cancel_v1   cancel;
    int acquire_calls, release_calls;
    int fail_acquire;
    int cancelled;
} cal_test_host;

static void* cal_alloc(void* user, uint64_t size, uint64_t align) {
    (void)user; (void)align;
    return size ? malloc((size_t)size) : NULL;
}
static void cal_free(void* user, void* p) { (void)user; free(p); }

static int cal_acquire(void* user, uint32_t n) {
    cal_test_host* h = (cal_test_host*)user;
    h->acquire_calls++;
    if (h->fail_acquire) return -1;
    (void)n;
    return 0;
}
static void cal_release(void* user, uint32_t n) {
    cal_test_host* h = (cal_test_host*)user;
    h->release_calls++;
    (void)n;
}
static int cal_is_cancelled(void* user) {
    cal_test_host* h = (cal_test_host*)user;
    return h->cancelled;
}

static void cal_host_init(cal_test_host* h) {
    memset(h, 0, sizeof(*h));
    h->api.head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    h->api.head.abi_version = ACS_ABI_VERSION_V1;
    h->executor.head.struct_size = (uint32_t)sizeof(acs_executor_v1);
    h->executor.head.abi_version = ACS_ABI_VERSION_V1;
    h->executor.available_cpus = 4;
    h->executor.max_workers = 4;
    h->executor.acquire = cal_acquire;
    h->executor.release = cal_release;
    h->executor.user_data = h;
    h->api.executor = &h->executor;
    h->allocator.head.struct_size = (uint32_t)sizeof(acs_allocator_v1);
    h->allocator.head.abi_version = ACS_ABI_VERSION_V1;
    h->allocator.alloc = cal_alloc;
    h->allocator.free = cal_free;
    h->allocator.user_data = h;
    h->api.allocator = &h->allocator;
    h->cancel.head.struct_size = (uint32_t)sizeof(acs_cancel_v1);
    h->cancel.head.abi_version = ACS_ABI_VERSION_V1;
    h->cancel.is_cancelled = cal_is_cancelled;
    h->cancel.user_data = h;
    h->api.cancel = &h->cancel;
}

/* ── base64 (RFC 4648) ── */
static void cal_b64_encode(const uint8_t* src, size_t n, std::string& out) {
    static const char* tab =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    out.clear();
    size_t i = 0;
    for (; i + 3 <= n; i += 3) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8) | src[i + 2];
        out += tab[(v >> 18) & 63]; out += tab[(v >> 12) & 63];
        out += tab[(v >> 6) & 63];  out += tab[v & 63];
    }
    size_t rem = n - i;
    if (rem == 1) {
        uint32_t v = (uint32_t)src[i] << 16;
        out += tab[(v >> 18) & 63]; out += tab[(v >> 12) & 63]; out += "==";
    } else if (rem == 2) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8);
        out += tab[(v >> 18) & 63]; out += tab[(v >> 12) & 63];
        out += tab[(v >> 6) & 63];  out += '=';
    }
}

static int cal_b64_decode(const std::string& in, std::vector<uint8_t>& out) {
    static int rev[256];
    static int init = 0;
    if (!init) {
        memset(rev, -1, sizeof(rev));
        const char* tab =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) rev[(unsigned char)tab[i]] = i;
        init = 1;
    }
    uint32_t acc = 0;
    int bits = 0;
    out.clear();
    int pad = 0;
    for (size_t i = 0; i < in.size(); ++i) {
        unsigned char c = (unsigned char)in[i];
        if (c == '=') { pad++; continue; }
        if (rev[c] < 0) return -1;
        acc = (acc << 6) | (uint32_t)rev[c];
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)((acc >> bits) & 0xFF));
        }
    }
    (void)pad;
    return 0;
}

/* ── fixture: 8x6 f32 合成平面 (确定性; 无 RNG) ── */
static const int FW = 8, FH = 6;
static const size_t FPX = (size_t)FW * FH;

static void cal_make_f32(std::vector<float>& img, float base, float scale) {
    img.resize(FPX);
    for (int y = 0; y < FH; ++y)
        for (int x = 0; x < FW; ++x) {
            const double dx = (double)x - 3.5, dy = (double)y - 2.5;
            img[(size_t)y * FW + x] = base +
                (float)(scale * (0.5 + 0.5 * std::exp(-(dx * dx + dy * dy) / 8.0)));
        }
}

static void cal_make_f64(std::vector<double>& img, double base, double scale) {
    img.resize(FPX);
    for (int y = 0; y < FH; ++y)
        for (int x = 0; x < FW; ++x) {
            const double dx = (double)x - 3.5, dy = (double)y - 2.5;
            img[(size_t)y * FW + x] = base +
                scale * (0.5 + 0.5 * std::exp(-(dx * dx + dy * dy) / 8.0));
        }
}

/* ── manifest 组装 ── */
static std::string cal_plane_b64(const void* p, size_t bytes) {
    std::string s;
    cal_b64_encode((const uint8_t*)p, bytes, s);
    return s;
}

/* config (op 词表; 与 adapter 词表一致) */
static std::string cal_cfg_calib(int f64, int dark_opt) {
    char b[256];
    snprintf(b, sizeof(b),
             "{\"op\":\"calibrate_frame%s\",\"width\":%d,\"height\":%d,"
             "\"dark_optimization\":%s,\"dark_scale_factor\":1.0,"
             "\"max_workers\":4}",
             f64 ? "_f64" : "", FW, FH, dark_opt ? "true" : "false");
    return std::string(b);
}
static std::string cal_cfg_correct(int f64) {
    char b[256];
    snprintf(b, sizeof(b),
             "{\"op\":\"correct_frame%s\",\"width\":%d,\"height\":%d,"
             "\"hot_sigma\":3.0,\"cold_sigma\":3.0,\"method\":\"median\","
             "\"max_structure_size\":4,\"max_workers\":4}",
             f64 ? "_f64" : "", FW, FH);
    return std::string(b);
}
static std::string cal_cfg_master(const char* op) {
    char b[320];
    snprintf(b, sizeof(b),
             "{\"op\":\"%s\",\"width\":%d,\"height\":%d,\"frames\":3,"
             "\"sigma_low\":2.5,\"sigma_high\":3.0,\"max_iterations\":3,"
             "\"combine\":\"mean\",\"max_workers\":4}",
             op, FW, FH);
    return std::string(b);
}

/* frame 级 manifest: data + 可选 master 平面 */
static std::string cal_man_frame(const std::vector<float>* light32,
                                 const std::vector<double>* light64,
                                 const std::vector<float>* bias32,
                                 const std::vector<double>* bias64,
                                 const std::vector<float>* dark32,
                                 const std::vector<double>* dark64,
                                 const std::vector<float>* flat32,
                                 const std::vector<double>* flat64) {
    const char* dt = (light64 || bias64 || dark64 || flat64) ? "f64" : "f32";
    std::string m = "{\"schema_version\":1,\"width\":" + std::to_string(FW) +
        ",\"height\":" + std::to_string(FH) + ",\"dtype\":\"" + dt + "\"";
    if (light32)
        m += ",\"data_base64\":\"" + cal_plane_b64(light32->data(), FPX * 4) + "\"";
    if (light64)
        m += ",\"data_base64\":\"" + cal_plane_b64(light64->data(), FPX * 8) + "\"";
    if (bias32)
        m += ",\"master_bias\":\"" + cal_plane_b64(bias32->data(), FPX * 4) + "\"";
    if (bias64)
        m += ",\"master_bias\":\"" + cal_plane_b64(bias64->data(), FPX * 8) + "\"";
    if (dark32)
        m += ",\"master_dark\":\"" + cal_plane_b64(dark32->data(), FPX * 4) + "\"";
    if (dark64)
        m += ",\"master_dark\":\"" + cal_plane_b64(dark64->data(), FPX * 8) + "\"";
    if (flat32)
        m += ",\"master_flat\":\"" + cal_plane_b64(flat32->data(), FPX * 4) + "\"";
    if (flat64)
        m += ",\"master_flat\":\"" + cal_plane_b64(flat64->data(), FPX * 8) + "\"";
    m += "}";
    return m;
}

/* master manifest: frames 数组 (3 帧) + 可选 bias 平面 */
static std::string cal_man_master(const std::vector<float>& s0,
                                  const std::vector<float>& s1,
                                  const std::vector<float>& s2,
                                  const std::vector<float>* bias) {
    std::string m = "{\"schema_version\":1,\"width\":" + std::to_string(FW) +
        ",\"height\":" + std::to_string(FH) + ",\"dtype\":\"f32\",\"frames\":[";
    m += "\"" + cal_plane_b64(s0.data(), FPX * 4) + "\",";
    m += "\"" + cal_plane_b64(s1.data(), FPX * 4) + "\",";
    m += "\"" + cal_plane_b64(s2.data(), FPX * 4) + "\"]";
    if (bias)
        m += ",\"master_bias\":\"" + cal_plane_b64(bias->data(), FPX * 4) + "\"";
    m += "}";
    return m;
}

static std::string cal_man_master_f64(const std::vector<double>& s0,
                                      const std::vector<double>& s1,
                                      const std::vector<double>& s2,
                                      const std::vector<double>* bias) {
    std::string m = "{\"schema_version\":1,\"width\":" + std::to_string(FW) +
        ",\"height\":" + std::to_string(FH) + ",\"dtype\":\"f64\",\"frames\":[";
    m += "\"" + cal_plane_b64(s0.data(), FPX * 8) + "\",";
    m += "\"" + cal_plane_b64(s1.data(), FPX * 8) + "\",";
    m += "\"" + cal_plane_b64(s2.data(), FPX * 8) + "\"]";
    if (bias)
        m += ",\"master_bias\":\"" + cal_plane_b64(bias->data(), FPX * 8) + "\"";
    m += "}";
    return m;
}

/* ── execute 便捷封装: 返回 ACS_OK 且 out 解码到 plane; 否则返回 status ── */
static acs_status cal_run(const acs_module_api_v1* api, cal_test_host* host,
                          acs_module_instance_v1* inst,
                          const std::string& manifest,
                          const std::string& config,
                          std::vector<uint8_t>* plane,
                          acs_error_info_v1* err) {
    acs_strbuf_v1 ob;
    memset(&ob, 0, sizeof(ob));
    char buf[1 << 16];
    ob.data = buf; ob.cap = sizeof(buf);
    acs_status st = api->execute(inst, cal_str(manifest.c_str()),
                                 cal_cfg(config.c_str()), &ob, err);
    if (st != ACS_OK) return st;
    if (plane) {
        /* 从输出 JSON 抽 out_base64 (固定键序, 简单扫描) */
        const char* key = "\"out_base64\":\"";
        const char* p = strstr(ob.data, key);
        if (!p) return ACS_ERR_PARAM;
        p += strlen(key);
        const char* q = strchr(p, '"');
        if (!q) return ACS_ERR_PARAM;
        std::string b64(p, (size_t)(q - p));
        if (cal_b64_decode(b64, *plane) != 0) return ACS_ERR_PARAM;
    }
    return ACS_OK;
}

int main(void) {
    const char* dll = getenv("ASTROCS_CAL_DLL_PATH");
    if (!dll || !dll[0]) { printf("FAIL: ASTROCS_CAL_DLL_PATH not set\n"); return 2; }
#ifdef _WIN32
    HMODULE h = LoadLibraryA(dll);
#else
    void* h = dlopen(dll, RTLD_NOW);
#endif
    if (!h) { printf("FAIL: dlopen(%s): %s\n", dll, dlerror()); return 2; }
#ifdef _WIN32
    cal_entry_fn entry = (cal_entry_fn)GetProcAddress(h, "astrocs_module_query_v1");
#else
    cal_entry_fn entry = (cal_entry_fn)dlsym(h, "astrocs_module_query_v1");
#endif
    CHECK(entry != NULL, "entry astrocs_module_query_v1 exported");

    if (!entry) return 2;
    const acs_module_api_v1* api = NULL;
    CHECK(entry(999, NULL, &api) == ACS_ERR_ABI_MISMATCH,
          "A: host_abi mismatch -> ACS_ERR_ABI_MISMATCH");
    CHECK(entry(ACS_ABI_VERSION_V1, NULL, NULL) == ACS_ERR_PARAM,
          "A: out_api NULL -> ACS_ERR_PARAM");
    CHECK(entry(ACS_ABI_VERSION_V1, NULL, &api) == ACS_OK && api != NULL,
          "A: query v1 -> vtable");
    if (!api) return 2;

    /* B. 导出面净化 (legacy 12 符号不可见) */
    {
        const char* syms[] = {
            "ac_generate_master_bias", "ac_generate_master_dark",
            "ac_generate_master_flat", "ac_calibrate_frame",
            "ac_correct_frame", "ac_generate_master_bias_f64",
            "ac_generate_master_dark_f64", "ac_generate_master_flat_f64",
            "ac_calibrate_frame_f64", "ac_correct_frame_f64",
            "ac_set_num_threads", "ac_version"
        };
        int hidden = 1;
        for (int i = 0; i < 12; ++i) {
#ifdef _WIN32
            if (GetProcAddress(h, syms[i]) != NULL) { hidden = 0; break; }
#else
            if (dlsym(h, syms[i]) != NULL) { hidden = 0; break; }
#endif
        }
        CHECK(hidden, "B: legacy 12 symbols hidden (ABI-006)");
    }

    /* A. describe */
    {
        acs_module_descriptor_v1 desc;
        memset(&desc, 0, sizeof(desc));
        CHECK(api->describe(api, cal_str("astrocs.p1.other"), &desc) ==
              ACS_ERR_ABI_MISMATCH, "A: describe wrong module_id -> MISMATCH");
        memset(&desc, 0, sizeof(desc));
        CHECK(api->describe(api, cal_str(ASTROCS_CAL_MODULE_ID), &desc) == ACS_OK,
              "A: describe ok");
        CHECK(desc.phase == 1 && desc.config_schema_ver == 1,
              "A: descriptor phase=1 schema_ver=1");
        CHECK(desc.execution_class == 0 && desc.parallel_ok == 1,
              "A: descriptor cpu_heavy parallel_ok");
        CHECK(desc.version.size == strlen(ASTROCS_CAL_VERSION) &&
              memcmp(desc.version.data, ASTROCS_CAL_VERSION, desc.version.size) == 0,
              "A: descriptor version 0.11.0-alpha.2");
        CHECK(desc.sci_id.size == 11 && memcmp(desc.sci_id.data, "SCI-CAL-001", 11) == 0,
              "A: descriptor sci_id");
    }

    /* C. validate_config 负例 (冻结 detail 码) */
    {
        acs_error_info_v1 err;
        memset(&err, 0, sizeof(err));
        CHECK(api->validate_config(api, cal_cfg(cal_cfg_calib(0, 0).c_str()), NULL)
              == ACS_OK, "C: validate ok config");
        memset(&err, 0, sizeof(err));
        acs_status st = api->validate_config(api, cal_cfg("{\"op\":\"warp\"}"), &err);
        CHECK(st == ACS_ERR_PARAM && err.detail_code == 100,
              "C: unknown op -> PARAM/100");
        memset(&err, 0, sizeof(err));
        st = api->validate_config(api, cal_cfg("{\"width\":8}"), &err);
        CHECK(st == ACS_ERR_PARAM && err.detail_code == 101,
              "C: missing op -> PARAM/101");
        memset(&err, 0, sizeof(err));
        st = api->validate_config(api,
              cal_cfg(cal_cfg_calib(0, 0).c_str()), &err);
        CHECK(st == ACS_OK, "C: full calibrate config ok");
        memset(&err, 0, sizeof(err));
        st = api->validate_config(api, cal_cfg(cal_cfg_master("generate_master_bias").c_str()), &err);
        CHECK(st == ACS_OK, "C: full master config ok");
        memset(&err, 0, sizeof(err));
        st = api->validate_config(api,
              cal_cfg("{\"op\":\"calibrate_frame\",\"width\":8,\"height\":6,"
                      "\"dark_optimization\":\"yes\",\"dark_scale_factor\":1.0}"), &err);
        CHECK(st == ACS_ERR_PARAM && err.detail_code == 102,
              "C: bool type mismatch -> PARAM/102");
        memset(&err, 0, sizeof(err));
        st = api->validate_config(api,
              cal_cfg("{\"op\":\"calibrate_frame\",\"width\":0,\"height\":6,"
                      "\"dark_optimization\":false,\"dark_scale_factor\":1.0}"), &err);
        CHECK(st == ACS_ERR_PARAM && err.detail_code == 103,
              "C: width=0 -> PARAM/103");
        memset(&err, 0, sizeof(err));
        st = api->validate_config(api, cal_cfg(""), &err);
        CHECK(st == ACS_ERR_PARAM && err.detail_code == ACS_DIAG_ECODE_NULL_CONFIG,
              "C: empty config -> NULL_CONFIG");
        memset(&err, 0, sizeof(err));
        st = api->validate_config(api,
              cal_cfg("{\"op\":\"calibrate_frame\",\"width\":8,\"height\":6,"
                      "\"dark_optimization\":false,\"dark_scale_factor\":1.0,"
                      "\"unknown_key\":1}"), &err);
        CHECK(st == ACS_ERR_PARAM && err.detail_code == ACS_DIAG_ECODE_CONFIG_SCHEMA,
              "C: unknown key -> CONFIG_SCHEMA");
    }

    /* D. plan 两阶段 + work_units 事实 */
    {
        acs_strbuf_v1 pb;
        char pbuf[1024];
        memset(&pb, 0, sizeof(pb));
        acs_error_info_v1 err;
        memset(&err, 0, sizeof(err));
        /* master: work_units = frames */
        acs_status st = api->plan(api, cal_str("node-1"),
                                  cal_cfg(cal_cfg_master("generate_master_bias").c_str()),
                                  &pb, &err);
        /* 两阶段语义 (strbuf_commit 冻结口径, 与 drizzle/gaia 一致):
         * data==NULL/cap==0 → ACS_OK + pb.size = 所需字节数 */
        CHECK(st == ACS_OK && pb.size > 0 && pb.size < sizeof(pbuf) && pb.data == NULL,
              "D: plan probe -> OK, size needed");
        pb.cap = pb.size;   /* 仍不分配 data → 再次探测路径 */
        memset(&pb, 0, sizeof(pb));
        pb.data = pbuf; pb.cap = sizeof(pbuf);
        st = api->plan(api, cal_str("node-1"),
                       cal_cfg(cal_cfg_master("generate_master_bias").c_str()),
                       &pb, &err);
        CHECK(st == ACS_OK, "D: plan commit ok");
        CHECK(strstr(pbuf, "\"plan_version\":1") != NULL &&
              strstr(pbuf, "\"work_units\":3") != NULL &&
              strstr(pbuf, "\"parallel_axis\":\"frame\"") != NULL &&
              strstr(pbuf, "\"min_workers\":1") != NULL &&
              strstr(pbuf, "\"cancel_support\":\"entry\"") != NULL,
              "D: master plan work_units=frames axis=frame");
        /* 帧级: work_units = w*h */
        memset(&pb, 0, sizeof(pb));
        pb.data = pbuf; pb.cap = sizeof(pbuf);
        st = api->plan(api, cal_str("node-2"),
                       cal_cfg(cal_cfg_calib(0, 0).c_str()), &pb, &err);
        CHECK(st == ACS_OK && strstr(pbuf, "\"work_units\":48") != NULL &&
              strstr(pbuf, "\"parallel_axis\":\"pixel\"") != NULL,
              "D: frame plan work_units=w*h axis=pixel");
        /* 两阶段探测后再次探测 (幂等; 同样 OK+size) */
        memset(&pb, 0, sizeof(pb));
        st = api->plan(api, cal_str("node-2"),
                       cal_cfg(cal_cfg_calib(0, 0).c_str()), &pb, &err);
        CHECK(st == ACS_OK && pb.size > 0 && pb.data == NULL,
              "D: plan probe repeatable");
    }

    /* E. direct-vs-plugin BITWISE (10 op + 恒等通道 + 状态机) */
    cal_test_host host;
    cal_host_init(&host);
    acs_error_info_v1 err;
    acs_module_instance_v1* inst = NULL;

    /* fixture 平面 (master 合成帧 base >= bias base + 余量: 减 bias 后为正,
     * 否则 legacy master_flat "median<0 reject" — 通道 fixture 数值要求) */
    std::vector<float> light, bias, dark, flat, s0, s1, s2;
    cal_make_f32(light, 100.0f, 900.0f);
    cal_make_f32(bias, 10.0f, 3.0f);
    cal_make_f32(dark, 20.0f, 5.0f);
    cal_make_f32(flat, 1.0f, 0.4f);
    cal_make_f32(s0, 30.0f, 2.0f);
    cal_make_f32(s1, 32.0f, 2.0f);
    cal_make_f32(s2, 31.0f, 3.0f);
    std::vector<double> light64, bias64, dark64, flat64, d0, d1, d2;
    cal_make_f64(light64, 100.0, 900.0);
    cal_make_f64(bias64, 10.0, 3.0);
    cal_make_f64(dark64, 20.0, 5.0);
    cal_make_f64(flat64, 1.0, 0.4);
    cal_make_f64(d0, 9.0, 2.0);
    cal_make_f64(d1, 11.0, 2.0);
    cal_make_f64(d2, 10.0, 3.0);

    /* 1) calibrate_frame (dark_opt=0) — direct */
    {
        std::vector<float> direct(FPX);
        float ak = 0.0f;
        int rc = ac_calibrate_frame(light.data(), FW, FH, dark.data(), flat.data(),
                                    bias.data(), direct.data(), 0, 1.0f, &ak);
        CHECK(rc == AC_OK, "E: direct calibrate f32 ok");
        CHECK(api->create(api, cal_cfg(cal_cfg_calib(0, 0).c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create calibrate inst");
        std::vector<uint8_t> plane;
        acs_status st = cal_run(api, &host, inst,
                                cal_man_frame(&light, NULL, &bias, NULL, &dark,
                                              NULL, &flat, NULL),
                                cal_cfg_calib(0, 0), &plane, &err);
        CHECK(st == ACS_OK, "E: plugin calibrate f32 execute ok");
        CHECK(plane.size() == FPX * 4 &&
              memcmp(plane.data(), direct.data(), FPX * 4) == 0,
              "E: calibrate f32 BITWISE (direct == plugin)");
        CHECK(host.acquire_calls == 1 && host.release_calls == 1,
              "E: lease acquire/release balanced (1/1)");
        api->destroy(inst); inst = NULL;
    }

    /* 2) calibrate_frame (dark_opt=1) */
    {
        std::vector<float> direct(FPX);
        float ak = 0.0f;
        ac_calibrate_frame(light.data(), FW, FH, dark.data(), flat.data(),
                           bias.data(), direct.data(), 1, 1.0f, &ak);
        CHECK(api->create(api, cal_cfg(cal_cfg_calib(0, 1).c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create calibrate dark_opt inst");
        std::vector<uint8_t> plane;
        acs_status st = cal_run(api, &host, inst,
                                cal_man_frame(&light, NULL, &bias, NULL, &dark,
                                              NULL, &flat, NULL),
                                cal_cfg_calib(0, 1), &plane, &err);
        CHECK(st == ACS_OK && plane.size() == FPX * 4 &&
              memcmp(plane.data(), direct.data(), FPX * 4) == 0,
              "E: calibrate f32 dark_opt=1 BITWISE");
        api->destroy(inst); inst = NULL;
    }

    /* 3) NULL-master 恒等通道 (键缺席 = NULL; DEV-2 行为保持) */
    {
        std::vector<float> direct(FPX);
        float ak = 0.0f;
        ac_calibrate_frame(light.data(), FW, FH, NULL, NULL, NULL,
                           direct.data(), 0, 1.0f, &ak);
        CHECK(api->create(api, cal_cfg(cal_cfg_calib(0, 0).c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create identity inst");
        std::vector<uint8_t> plane;
        acs_status st = cal_run(api, &host, inst,
                                cal_man_frame(&light, NULL, NULL, NULL, NULL,
                                              NULL, NULL, NULL),
                                cal_cfg_calib(0, 0), &plane, &err);
        CHECK(st == ACS_OK && plane.size() == FPX * 4 &&
              memcmp(plane.data(), direct.data(), FPX * 4) == 0,
              "E: NULL-master identity BITWISE (legacy 恒等语义保持)");
        api->destroy(inst); inst = NULL;
    }

    /* 4) correct_frame */
    {
        std::vector<float> direct(FPX);
        int nh = 0, nc = 0;
        ac_correct_frame(light.data(), FW, FH, dark.data(), bias.data(),
                         direct.data(), 3.0f, 3.0f, AC_METHOD_MEDIAN, 4, &nh, &nc);
        CHECK(api->create(api, cal_cfg(cal_cfg_correct(0).c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create correct inst");
        std::vector<uint8_t> plane;
        acs_status st = cal_run(api, &host, inst,
                                cal_man_frame(&light, NULL, NULL, NULL, &dark,
                                              NULL, NULL, NULL),
                                cal_cfg_correct(0), &plane, &err);
        CHECK(st == ACS_OK && plane.size() == FPX * 4 &&
              memcmp(plane.data(), direct.data(), FPX * 4) == 0,
              "E: correct f32 BITWISE");
        api->destroy(inst); inst = NULL;
    }

    /* 5-7) master bias/dark/flat f32 */
    {
        std::vector<float> stack(FPX * 3);
        memcpy(&stack[0], s0.data(), FPX * 4);
        memcpy(&stack[FPX], s1.data(), FPX * 4);
        memcpy(&stack[2 * FPX], s2.data(), FPX * 4);
        struct { const char* op; int use_bias; } ms[3] = {
            { "generate_master_bias", 0 },
            { "generate_master_dark", 0 },
            { "generate_master_flat", 1 }
        };
        for (int i = 0; i < 3; ++i) {
            std::vector<float> direct(FPX);
            int drc = AC_ERR_INTERNAL;
            if (i == 0)
                drc = ac_generate_master_bias(stack.data(), 3, FW, FH, direct.data(),
                                              2.5f, 3.0f, 3, AC_COMBINE_MEAN);
            else if (i == 1)
                drc = ac_generate_master_dark(stack.data(), 3, FW, FH, direct.data(),
                                              2.5f, 3.0f, 3, AC_COMBINE_MEAN);
            else
                drc = ac_generate_master_flat(stack.data(), 3, FW, FH, bias.data(),
                                              direct.data(), 2.5f, 3.0f, 3);
            char dnm[96];
            snprintf(dnm, sizeof(dnm), "E: %s direct ok (rc=0)", ms[i].op);
            CHECK(drc == AC_OK, dnm);
            CHECK(api->create(api, cal_cfg(cal_cfg_master(ms[i].op).c_str()),
                              &host.api, &inst, &err) == ACS_OK && inst,
                  "E: create master inst");
            std::vector<uint8_t> plane;
            std::string man = cal_man_master(s0, s1, s2,
                                             ms[i].use_bias ? &bias : NULL);
            acs_status st = cal_run(api, &host, inst, man,
                                    cal_cfg_master(ms[i].op), &plane, &err);
            char nm[96];
            snprintf(nm, sizeof(nm), "E: %s BITWISE", ms[i].op);
            CHECK(st == ACS_OK && plane.size() == FPX * 4 &&
                  memcmp(plane.data(), direct.data(), FPX * 4) == 0, nm);
            api->destroy(inst); inst = NULL;
        }
    }

    /* 8) calibrate_frame_f64 BITWISE */
    {
        std::vector<double> direct(FPX);
        double ak = 0.0;
        ac_calibrate_frame_f64(light64.data(), FW, FH, dark64.data(), flat64.data(),
                               bias64.data(), direct.data(), 1, 1.0, &ak);
        CHECK(api->create(api, cal_cfg(cal_cfg_calib(1, 1).c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create calibrate f64 inst");
        std::vector<uint8_t> plane;
        acs_status st = cal_run(api, &host, inst,
                                cal_man_frame(NULL, &light64, NULL, &bias64,
                                              NULL, &dark64, NULL, &flat64),
                                cal_cfg_calib(1, 1), &plane, &err);
        CHECK(st == ACS_OK && plane.size() == FPX * 8 &&
              memcmp(plane.data(), direct.data(), FPX * 8) == 0,
              "E: calibrate f64 BITWISE");
        api->destroy(inst); inst = NULL;
    }

    /* 9) correct_frame_f64 BITWISE */
    {
        std::vector<double> direct(FPX);
        int nh = 0, nc = 0;
        ac_correct_frame_f64(light64.data(), FW, FH, dark64.data(), bias64.data(),
                             direct.data(), 3.0, 3.0, AC_METHOD_MEDIAN, 4, &nh, &nc);
        CHECK(api->create(api, cal_cfg(cal_cfg_correct(1).c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create correct f64 inst");
        std::vector<uint8_t> plane;
        acs_status st = cal_run(api, &host, inst,
                                cal_man_frame(NULL, &light64, NULL, NULL,
                                              NULL, &dark64, NULL, NULL),
                                cal_cfg_correct(1), &plane, &err);
        CHECK(st == ACS_OK && plane.size() == FPX * 8 &&
              memcmp(plane.data(), direct.data(), FPX * 8) == 0,
              "E: correct f64 BITWISE");
        api->destroy(inst); inst = NULL;
    }

    /* 10) master_bias_f64 BITWISE */
    {
        std::vector<double> stack64(FPX * 3);
        memcpy(&stack64[0], d0.data(), FPX * 8);
        memcpy(&stack64[FPX], d1.data(), FPX * 8);
        memcpy(&stack64[2 * FPX], d2.data(), FPX * 8);
        std::vector<double> direct(FPX);
        ac_generate_master_bias_f64(stack64.data(), 3, FW, FH, direct.data(),
                                    2.5, 3.0, 3, AC_COMBINE_MEAN);
        CHECK(api->create(api, cal_cfg(cal_cfg_master("generate_master_bias_f64").c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create master f64 inst");
        std::vector<uint8_t> plane;
        acs_status st = cal_run(api, &host, inst,
                                cal_man_master_f64(d0, d1, d2, NULL),
                                cal_cfg_master("generate_master_bias_f64"),
                                &plane, &err);
        CHECK(st == ACS_OK && plane.size() == FPX * 8 &&
              memcmp(plane.data(), direct.data(), FPX * 8) == 0,
              "E: master_bias_f64 BITWISE");
        api->destroy(inst); inst = NULL;
    }

    /* cancel: 预置 → CANCELLED + 无输出 + 可复用 */
    {
        CHECK(api->create(api, cal_cfg(cal_cfg_calib(0, 0).c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create cancel inst");
        CHECK(api->request_cancel(inst) == ACS_OK, "E: request_cancel ok");
        host.cancelled = 1;
        acs_strbuf_v1 ob;
        memset(&ob, 0, sizeof(ob));
        char buf[4096];
        ob.data = buf; ob.cap = sizeof(buf);
        acs_status st = api->execute(inst,
                                     cal_str(cal_man_frame(&light, NULL, &bias,
                                                           NULL, &dark, NULL,
                                                           &flat, NULL).c_str()),
                                     cal_cfg(cal_cfg_calib(0, 0).c_str()),
                                     &ob, &err);
        CHECK(st == ACS_ERR_CANCELLED && ob.size == 0,
              "E: cancel -> CANCELLED, no output (事务性)");
        /* 可复用: 取消标志清零后再执行 */
        host.cancelled = 0;
        std::vector<uint8_t> plane;
        st = cal_run(api, &host, inst,
                     cal_man_frame(&light, NULL, &bias, NULL, &dark, NULL,
                                   &flat, NULL),
                     cal_cfg_calib(0, 0), &plane, &err);
        CHECK(st == ACS_OK && plane.size() == FPX * 4,
              "E: instance reusable after cancel");
        api->destroy(inst); inst = NULL;
    }

    /* budget: acquire 失败 → BUDGET/105; executor 缺失 → BUDGET/105 */
    {
        CHECK(api->create(api, cal_cfg(cal_cfg_calib(0, 0).c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create budget inst");
        host.fail_acquire = 1;
        memset(&err, 0, sizeof(err));
        acs_status st = api->execute(inst,
                                     cal_str(cal_man_frame(&light, NULL, &bias,
                                                           NULL, &dark, NULL,
                                                           &flat, NULL).c_str()),
                                     cal_cfg(cal_cfg_calib(0, 0).c_str()),
                                     NULL, &err);
        CHECK(st == ACS_ERR_BUDGET && err.detail_code == 105,
              "E: acquire fail -> BUDGET/105 (cpu_heavy 禁单线程)");
        host.fail_acquire = 0;
        api->destroy(inst); inst = NULL;

        const acs_executor_v1* saved = host.api.executor;
        host.api.executor = NULL;
        CHECK(api->create(api, cal_cfg(cal_cfg_calib(0, 0).c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create no-executor inst");
        memset(&err, 0, sizeof(err));
        st = api->execute(inst,
                          cal_str(cal_man_frame(&light, NULL, &bias, NULL,
                                                &dark, NULL, &flat, NULL).c_str()),
                          cal_cfg(cal_cfg_calib(0, 0).c_str()), NULL, &err);
        CHECK(st == ACS_ERR_BUDGET && err.detail_code == 105,
              "E: executor missing -> BUDGET/105");
        host.api.executor = saved;
        api->destroy(inst); inst = NULL;
    }

    /* inspect: exec_count 与状态 */
    {
        CHECK(api->create(api, cal_cfg(cal_cfg_calib(0, 0).c_str()),
                          &host.api, &inst, &err) == ACS_OK && inst,
              "E: create inspect inst");
        acs_strbuf_v1 ob;
        char buf[512];
        memset(&ob, 0, sizeof(ob));
        ob.data = buf; ob.cap = sizeof(buf);
        CHECK(api->inspect(inst, &ob, &err) == ACS_OK, "E: inspect ok");
        CHECK(strstr(ob.data, "\"exec_count\":0") != NULL &&
              strstr(ob.data, ASTROCS_CAL_MODULE_ID) != NULL,
              "E: inspect fresh exec_count=0");
        std::vector<uint8_t> plane;
        CHECK(cal_run(api, &host, inst,
                      cal_man_frame(&light, NULL, &bias, NULL, &dark, NULL,
                                    &flat, NULL),
                      cal_cfg_calib(0, 0), &plane, &err) == ACS_OK,
              "E: execute for inspect");
        memset(&ob, 0, sizeof(ob));
        ob.data = buf; ob.cap = sizeof(buf);
        CHECK(api->inspect(inst, &ob, &err) == ACS_OK &&
              strstr(ob.data, "\"exec_count\":1") != NULL,
              "E: inspect exec_count=1 after execute");
        api->destroy(inst); inst = NULL;
    }

    /* 状态护栏: create 无 config → PARAM/NULL_CONFIG */
    {
        memset(&err, 0, sizeof(err));
        acs_module_instance_v1* bad = NULL;
        acs_status st = api->create(api, cal_cfg(""), &host.api, &bad, &err);
        CHECK(st == ACS_ERR_PARAM &&
              err.detail_code == ACS_DIAG_ECODE_NULL_CONFIG && bad == NULL,
              "E: create empty config -> PARAM/NULL_CONFIG, out=NULL");
    }

    printf("----\n%s (%d failures)\n", g_fail ? "FAILED" : "ALL PASSED", g_fail);
    return g_fail ? 1 : 0;
}
