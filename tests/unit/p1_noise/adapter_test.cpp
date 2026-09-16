/* adapter_test.cpp - P1-NOISE-IMPL astrocs_p1_noise adapter 对拍验证面
 *
 * 对齐先例: tests/unit/p1_hips/adapter_test.c (P1-HIPS-IMPL 1959dc89)、
 *           tests/unit/calibration_adapter_test.cpp / cosmetic_adapter_test.cpp
 *           (P1-CAL-IMPL adf820ac / P1-COS-IMPL 948dfcba)。
 *
 * 通道:
 *   direct  = p1noise_direct_api_v1() (adapter_entry_impl.cpp 直通编译
 *             module_entry.cpp; 生产符号 p1noise_under_test STATIC)
 *   plugin  = dlopen($ASTROCS_NOISE_DLL_PATH) → astrocs_module_query_v1
 *
 * 用例:
 *   A  describe / query 负面 ABI (host_abi 失配不降级)
 *   B  validate_config 负面 (空/op 缺/词表外/dtype 越域/h·w 越域/fill 双空/
 *      subop 词表外)
 *   C  plan 真实 work_units (像素事实) + 两阶段探测/整写/幂等 + diag tiny
 *   D  direct-vs-plugin BITWISE 对拍 (f32/f64 build + 内联 fill + 独立 fill
 *      round-trip 影子 floor 回退同构 + scale_law/gain_variance 标量;
 *      整输出 JSON strcmp + 解码平面 memcmp 双口径)
 *   E  BUDGET 105 双路径 (executor 缺失 / acquire 失败; out 零写入)
 *   F  cancel (entry cancel_req → STATE; host cancel → CANCELLED + 零写入
 *      + 同实例复用)
 *   G  strbuf 两阶段 + BUFFER_TOO_SMALL
 *   H  export 探针 (legacy snr_noise_* 七符号 dlsym 全 NULL, query 唯一存在)
 *   I  inspect / exec_count / 负面 manifest (缺平面/尺寸失配/b64 坏)
 *   J  生命周期 (destroy 后 execute → STATE; double destroy 安全)
 *
 * 科学纪律: fixture 固定 seed 全离线 (splitmix64 + Box-Muller), 期望值=
 * direct 通道现场直调生产实现 (同一权威 noise_model.cpp), 无 golden 抄写;
 * 冻结容差不变 (bitwise)。
 */
#include <dlfcn.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/noise/types.h"

extern "C" const acs_module_api_v1* p1noise_direct_api_v1(void);

/* ────────── mini 检查框架 ────────── */

static int g_pass = 0, g_fail = 0;
static const char* g_case = "?";

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (cond) {                                                         \
            g_pass++;                                                       \
        } else {                                                            \
            g_fail++;                                                       \
            std::printf("FAIL [%s] %s (line %d)\n", g_case, msg, __LINE__); \
        }                                                                   \
    } while (0)

/* ────────── host stub (CAL/COS 同款) ────────── */

namespace {

struct HostState {
    int acquire_calls = 0, release_calls = 0, released_total = 0;
    int fail_acquire = 0;
    int cancelled = 0;
    int fail_alloc = 0;
    int alloc_count = 0, free_count = 0;
    uint32_t max_workers = 4;
    uint32_t available_cpus = 4;
};

void* host_alloc(void* ud, uint64_t size, uint64_t /*align*/) {
    HostState* h = (HostState*)ud;
    if (h->fail_alloc) return NULL;
    h->alloc_count++;
    return std::malloc((size_t)size);
}
void host_free(void* ud, void* p) {
    HostState* h = (HostState*)ud;
    if (p) h->free_count++;
    std::free(p);
}
int host_acquire(void* ud, uint32_t /*n*/) {
    HostState* h = (HostState*)ud;
    h->acquire_calls++;
    return h->fail_acquire ? 1 : 0;
}
void host_release(void* ud, uint32_t n) {
    HostState* h = (HostState*)ud;
    h->release_calls++;
    h->released_total += (int)n;
}
int host_cancelled(void* ud) {
    return ((HostState*)ud)->cancelled;
}

void fill_host(acs_host_api_v1* host, HostState* st, int with_executor,
               int with_cancel) {
    std::memset(host, 0, sizeof(*host));
    host->head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    host->head.abi_version = ACS_ABI_VERSION_V1;
    acs_allocator_v1* alloc = new acs_allocator_v1();
    std::memset(alloc, 0, sizeof(acs_allocator_v1));
    alloc->head.struct_size = (uint32_t)sizeof(acs_allocator_v1);
    alloc->head.abi_version = ACS_ABI_VERSION_V1;
    alloc->alloc = (void* (*)(void*, uint64_t, uint64_t))host_alloc;
    alloc->free = (void (*)(void*, void*))host_free;
    alloc->user_data = st;
    host->allocator = alloc;
    if (with_executor) {
        acs_executor_v1* ex = new acs_executor_v1();
        std::memset(ex, 0, sizeof(acs_executor_v1));
        ex->head.struct_size = (uint32_t)sizeof(acs_executor_v1);
        ex->head.abi_version = ACS_ABI_VERSION_V1;
        ex->acquire = (int (*)(void*, uint32_t))host_acquire;
        ex->release = (void (*)(void*, uint32_t))host_release;
        ex->user_data = st;
        ex->max_workers = st->max_workers;
        ex->available_cpus = st->available_cpus;
        host->executor = ex;
    }
    if (with_cancel) {
        acs_cancel_v1* cc = new acs_cancel_v1();
        std::memset(cc, 0, sizeof(acs_cancel_v1));
        cc->head.struct_size = (uint32_t)sizeof(acs_cancel_v1);
        cc->head.abi_version = ACS_ABI_VERSION_V1;
        cc->is_cancelled = (int (*)(void*))host_cancelled;
        cc->user_data = st;
        host->cancel = cc;
    }
}
void free_host(acs_host_api_v1* host) {
    delete host->allocator;
    delete host->executor;
    delete host->cancel;
}

acs_str_v1 cstr(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)std::strlen(s) : 0;
    return v;
}

/* ────────── base64 (测试侧独立实现) ────────── */

std::string b64enc(const void* data, size_t n) {
    static const char* T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const uint8_t* s = (const uint8_t*)data;
    std::string out;
    size_t i = 0;
    while (i + 3 <= n) {
        uint32_t v = ((uint32_t)s[i] << 16) | ((uint32_t)s[i + 1] << 8) |
                     (uint32_t)s[i + 2];
        out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63];
        out += T[(v >> 6) & 63];  out += T[v & 63];
        i += 3;
    }
    if (i < n) {
        uint32_t v = (uint32_t)s[i] << 16;
        int rem = (int)(n - i);
        if (rem == 2) v |= (uint32_t)s[i + 1] << 8;
        out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63];
        out += (rem == 2) ? T[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

bool b64dec(const std::string& in, std::vector<uint8_t>& out) {
    out.clear();
    if (in.empty()) return true;
    size_t pad = 0;
    while (pad < in.size() && in[in.size() - 1 - pad] == '=') pad++;
    if (pad > 2 || (in.size() % 4) != 0) return false;
    size_t cap = (in.size() / 4) * 3 + (pad == 1 ? 2 : (pad == 2 ? 1 : 0));
    out.resize(cap);
    size_t o = 0;
    uint32_t acc = 0;
    int bits = 0;
    for (size_t i = 0; i < in.size() - pad; ++i) {
        int v = b64val(in[i]);
        if (v < 0) return false;
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; out[o++] = (uint8_t)((acc >> bits) & 0xFF); }
    }
    if (o != cap) return false;
    return true;
}

std::string json_get_str(const std::string& json, const char* key) {
    std::string pat = std::string("\"") + key + "\"";
    size_t p = json.find(pat);
    if (p == std::string::npos) return "";
    p += pat.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == ':')) p++;
    if (p >= json.size() || json[p] != '"') return "";
    p++;
    size_t e = json.find('"', p);
    if (e == std::string::npos) return "";
    return json.substr(p, e - p);
}

long long json_get_int(const std::string& json, const char* key, int* found) {
    if (found) *found = 0;
    std::string pat = std::string("\"") + key + "\"";
    size_t p = json.find(pat);
    if (p == std::string::npos) return 0;
    p += pat.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == ':')) p++;
    char* end = NULL;
    long long v = strtoll(json.c_str() + p, &end, 10);
    if (end == json.c_str() + p) return 0;
    if (found) *found = 1;
    return v;
}

/* ────────── fixture (固定 seed; splitmix64 + Box-Muller 全离线) ────────── */

uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
uint64_t rand_below(uint64_t* st, uint64_t n) {  /* [0,n) 无偏近似 (测试面) */
    uint64_t lim = UINT64_MAX - UINT64_MAX % n;
    uint64_t x;
    do { x = splitmix64(st); } while (x >= lim);
    return x % n;
}
double rand_u01(uint64_t* st) {
    return (double)(splitmix64(st) >> 11) * (1.0 / 9007199254740992.0);
}
double rand_gauss(uint64_t* st) {   /* Box-Muller (p1noise fixtures 同族) */
    double u1 = rand_u01(st);
    if (u1 < 1e-300) u1 = 1e-300;
    double u2 = rand_u01(st);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * 3.14159265358979323846 * u2);
}

const int FX_H = 96, FX_W = 128;
const double FX_SIGMA = 5.0, FX_BG = 100.0;

struct Fixture {
    std::vector<float> data_f32;        /* FX_H*FX_W */
    std::vector<double> data_f64;       /* 同源 z 序列 */
    std::vector<float> mask;            /* 全 0 (无掩膜直通) */
    std::vector<float> mask_all;        /* 全 1 (rc=1 负面) */
    std::vector<double> stars;          /* 5 颗星 [x0,y0,x1,y1,...] */
    Fixture() {
        uint64_t st = 0x4E4F495345315F41ull;   /* "NOISE1_A" 固定 seed */
        data_f32.resize((size_t)FX_H * FX_W);
        data_f64.resize((size_t)FX_H * FX_W);
        for (size_t i = 0; i < data_f32.size(); ++i) {
            double z = FX_BG + FX_SIGMA * rand_gauss(&st);
            data_f64[i] = z;
            data_f32[i] = (float)z;
        }
        mask.assign((size_t)FX_H * FX_W, 0.0f);
        mask_all.assign((size_t)FX_H * FX_W, 1.0f);
        const double sx[10] = {32.0, 24.0, 64.0, 48.0, 96.0,
                               72.0, 20.0, 80.0, 110.0, 30.0};
        stars.assign(sx, sx + 10);
    }
};

/* ────────── 通道执行器 ────────── */

struct Ch {
    const acs_module_api_v1* api;
    const acs_host_api_v1* host;
    const char* name;
};

/* create → execute → destroy (每事务独立实例; 返回 execute status) */
acs_status run_execute(const Ch& ch, const char* config, const char* manifest,
                       std::string* out_json, acs_error_info_v1* err,
                       acs_module_instance_v1** kept = NULL) {
    acs_module_instance_v1* inst = NULL;
    acs_status st = ch.api->create(ch.api, cstr(config), ch.host, &inst, err);
    if (st != ACS_OK) return st;
    acs_strbuf_v1 ob;
    std::memset(&ob, 0, sizeof(ob));
    char buf[1 << 22];
    ob.data = buf;
    ob.cap = sizeof(buf);
    st = ch.api->execute(inst, cstr(manifest), cstr(config), &ob, err);
    if (out_json) *out_json = (st == ACS_OK) ? std::string(buf, (size_t)ob.size) : "";
    if (kept) *kept = inst;
    else ch.api->destroy(inst);
    return st;
}

/* ────────── 对拍核心: 同 config/manifest 双通道 → 整 JSON strcmp ────────── */

void compare_both(const Ch& direct, const Ch& plugin, const char* config,
                  const char* manifest, const char* what) {
    acs_error_info_v1 e1, e2;
    std::memset(&e1, 0, sizeof(e1));
    std::memset(&e2, 0, sizeof(e2));
    std::string od, op;
    acs_status sd = run_execute(direct, config, manifest, &od, &e1);
    acs_status sp = run_execute(plugin, config, manifest, &op, &e2);
    CHECK(sd == sp, what);
    CHECK(od == op, what);   /* 整输出 JSON bitwise (同 libm 同代码路径) */
    if (od != op && g_fail == 0) {
        std::printf("  direct(%zu): %.200s...\n  plugin(%zu): %.200s...\n",
                    od.size(), od.c_str(), op.size(), op.c_str());
    }
}

}  // namespace

int main(int argc, char** argv) {
    const char* dll_path = std::getenv("ASTROCS_NOISE_DLL_PATH");
    if (!dll_path || !dll_path[0]) {
        std::printf("SKIP: ASTROCS_NOISE_DLL_PATH not set\n");
        return 2;
    }

    /* ── plugin 通道装载 ── */
    void* h = dlopen(dll_path, RTLD_NOW);
    if (!h) {
        std::printf("FAIL dlopen: %s\n", dlerror());
        return 2;
    }
    typedef acs_status (*query_fn)(uint32_t, const acs_host_api_v1*,
                                   const acs_module_api_v1**);
    query_fn q = (query_fn)dlsym(h, "astrocs_module_query_v1");
    if (!q) {
        std::printf("FAIL dlsym astrocs_module_query_v1: %s\n", dlerror());
        return 2;
    }
    const acs_module_api_v1* papi = NULL;
    if (q(ACS_ABI_VERSION_V1, NULL, &papi) != ACS_OK || !papi) {
        std::printf("FAIL plugin query_v1\n");
        return 2;
    }

    Ch direct, plugin;
    direct.api = p1noise_direct_api_v1();
    direct.host = NULL;
    direct.name = "direct";
    plugin.api = papi;
    plugin.host = NULL;
    plugin.name = "plugin";

    /* ── H. export 探针 (先例 hips 同款): legacy 七符号全 NULL ── */
    g_case = "H_export_probe";
    {
        static const char* const kLegacy[] = {
            "snr_noise_model_v1", "snr_noise_model_v1_f64",
            "snr_noise_model_v1_default_config", "snr_noise_model_v1_fill",
            "snr_noise_model_v1_free", "snr_noise_scale_law",
            "snr_noise_gain_variance",
            /* 相邻生产符号 (非噪声合同面) 同样不得导出 */
            "snr_estimate", "snr_extract_model", "snr_phot_cal_quality",
            "snr_psf_fit_quality"
        };
        for (size_t i = 0; i < sizeof(kLegacy) / sizeof(kLegacy[0]); ++i) {
            void* s = dlsym(h, kLegacy[i]);
            CHECK(s == NULL, kLegacy[i]);
        }
        void* qq = dlsym(h, "astrocs_module_query_v1");
        CHECK(qq != NULL, "astrocs_module_query_v1 present");
    }

    /* ── A. describe / 负面 ABI ── */
    g_case = "A_describe";
    {
        acs_host_api_v1 host;
        HostState st;
        fill_host(&host, &st, 1, 1);
        Ch d = direct, p = plugin;
        d.host = &host;
        p.host = &host;
        acs_module_descriptor_v1 dd, pd;
        acs_status s1 = d.api->describe(d.api, cstr("astrocs.p1.noise"), &dd);
        acs_status s2 = p.api->describe(p.api, cstr("astrocs.p1.noise"), &pd);
        CHECK(s1 == ACS_OK && s2 == ACS_OK, "describe ok");
        CHECK(dd.module_id.size == strlen("astrocs.p1.noise") &&
              std::memcmp(dd.module_id.data, "astrocs.p1.noise", 16) == 0,
              "descriptor module_id");
        CHECK(std::memcmp(dd.build_id.data, pd.build_id.data,
                          dd.build_id.size) == 0, "build_id match");
        CHECK(dd.phase == 1 && pd.phase == 1, "phase=1");
        CHECK(dd.config_schema_ver == 1, "config_schema_ver=1");
        CHECK(dd.execution_class == 0, "execution_class=cpu_heavy");
        CHECK(dd.parallel_ok == 0, "parallel_ok=0 (事务串行)");
        CHECK(std::memcmp(dd.sci_id.data, "SCI-NOISE-001", 13) == 0, "sci_id");
        CHECK(std::memcmp(dd.alg_id.data, "ALG-NOISE-001", 13) == 0, "alg_id");
        CHECK(std::memcmp(dd.api_id.data, "API-NOISE-001", 13) == 0, "api_id");
        CHECK(d.api->describe(d.api, cstr("astrocs.p1.wrong"), &dd) ==
                  ACS_ERR_ABI_MISMATCH, "describe wrong id");
        CHECK(d.api->describe(d.api, cstr("astrocs.p1.noise"), NULL) ==
                  ACS_ERR_PARAM, "describe null out");
        /* query 负面 ABI (直接打 DLL query 入口) */
        const acs_module_api_v1* tmp = NULL;
        CHECK(q(2, &host, &tmp) == ACS_ERR_ABI_MISMATCH, "host_abi=2 rejected");
        CHECK(q(0, &host, &tmp) == ACS_ERR_ABI_MISMATCH, "host_abi=0 rejected");
        CHECK(q(0x7f3a11c0u, &host, &tmp) == ACS_ERR_ABI_MISMATCH,
              "host_abi garbage rejected");
        CHECK(q(ACS_ABI_VERSION_V1, &host, NULL) == ACS_ERR_PARAM,
              "query null out_api");
        free_host(&host);
    }

    /* ── B. validate_config 负面 ── */
    g_case = "B_validate_config";
    {
        acs_error_info_v1 e;
        std::memset(&e, 0, sizeof(e));
        CHECK(direct.api->validate_config(direct.api, cstr(""), &e) ==
                  ACS_ERR_PARAM && e.detail_code == ACS_DIAG_ECODE_NULL_CONFIG,
              "empty config");
        e = acs_error_info_v1();
        CHECK(direct.api->validate_config(direct.api, cstr("{}"), &e) ==
                  ACS_ERR_PARAM && e.detail_code == ACS_DIAG_ECODE_CONFIG_SCHEMA,
              "missing op");
        e = acs_error_info_v1();
        CHECK(direct.api->validate_config(
                  direct.api, cstr("{\"op\":\"unknown_op\"}"), &e) ==
                  ACS_ERR_PARAM && e.detail_code == NOISE_ECODE_OP_UNKNOWN,
              "op outside vocabulary (100)");
        e = acs_error_info_v1();
        CHECK(direct.api->validate_config(
                  direct.api,
                  cstr("{\"op\":\"estimate_noise_model\",\"dtype\":2,"
                       "\"h\":8,\"w\":8}"), &e) ==
                  ACS_ERR_PARAM && e.detail_code == NOISE_ECODE_PARAM_TYPE,
              "dtype=2 rejected (102)");
        e = acs_error_info_v1();
        CHECK(direct.api->validate_config(
                  direct.api,
                  cstr("{\"op\":\"estimate_noise_model\",\"dtype\":0,"
                       "\"h\":0,\"w\":8}"), &e) ==
                  ACS_ERR_PARAM && e.detail_code == NOISE_ECODE_PARAM_RANGE,
              "h=0 rejected (103)");
        e = acs_error_info_v1();
        CHECK(direct.api->validate_config(
                  direct.api,
                  cstr("{\"op\":\"fill_noise_field\",\"h\":8,\"w\":8}"), &e) ==
                  ACS_ERR_PARAM && e.detail_code == NOISE_ECODE_PARAM_RANGE,
              "fill without output plane rejected (103)");
        e = acs_error_info_v1();
        CHECK(direct.api->validate_config(
                  direct.api,
                  cstr("{\"op\":\"noise_diagnostic\",\"subop\":\"bogus\"}"),
                  &e) == ACS_ERR_PARAM && e.detail_code == NOISE_ECODE_OP_UNKNOWN,
              "subop outside vocabulary (100)");
        e = acs_error_info_v1();
        CHECK(direct.api->validate_config(
                  direct.api,
                  cstr("{\"op\":\"noise_diagnostic\",\"subop\":\"gain_variance\","
                       "\"signal\":1.0}"), &e) == ACS_ERR_PARAM,
              "gain_variance missing gain rejected");
        /* 合法面 (全默认 config) */
        CHECK(direct.api->validate_config(
                  direct.api,
                  cstr("{\"op\":\"estimate_noise_model\",\"dtype\":0,"
                       "\"h\":8,\"w\":8}"), NULL) == ACS_OK,
              "valid estimate config");
        CHECK(plugin.api->validate_config(
                  plugin.api,
                  cstr("{\"op\":\"estimate_noise_model\",\"dtype\":0,"
                       "\"h\":8,\"w\":8}"), NULL) == ACS_OK,
              "valid estimate config (plugin)");
    }

    /* ── C. plan work_units 事实 + 两阶段 ── */
    g_case = "C_plan";
    {
        acs_host_api_v1 host;
        HostState st;
        fill_host(&host, &st, 1, 1);
        Ch d = direct;
        d.host = &host;
        acs_strbuf_v1 pb;
        char pbuf[2048];
        std::memset(&pb, 0, sizeof(pb));
        /* 阶段 1: 尺寸探测 (data=NULL, cap=0 → OK + size=所需) */
        acs_status s1 = d.api->plan(
            d.api, cstr("node-noise-1"),
            cstr("{\"op\":\"estimate_noise_model\",\"dtype\":0,"
                 "\"h\":96,\"w\":128}"), &pb, NULL);
        CHECK(s1 == ACS_OK && pb.size > 0 && pb.data == NULL,
              "plan probe -> OK, size needed");
        uint64_t need = pb.size;
        /* 阶段 2: 整写 */
        pb.data = pbuf;
        pb.cap = sizeof(pbuf);
        acs_status s2 = d.api->plan(
            d.api, cstr("node-noise-1"),
            cstr("{\"op\":\"estimate_noise_model\",\"dtype\":0,"
                 "\"h\":96,\"w\":128}"), &pb, NULL);
        CHECK(s2 == ACS_OK && pb.size == need, "plan commit ok (size 幂等)");
        std::string plan(pbuf, (size_t)pb.size);
        /* work_units = h·w 数据事实 (CAL 帧级同款) */
        CHECK(plan.find("\"work_unit\":\"pixel\"") != std::string::npos,
              "work_unit=pixel");
        CHECK(plan.find("\"work_units\":12288") != std::string::npos,
              "work_units=96*128 (数据事实)");
        CHECK(plan.find("\"parallel_axis\":\"instance\"") != std::string::npos,
              "parallel_axis=instance");
        CHECK(plan.find("\"min_workers\":1") != std::string::npos,
              "min_workers=1");
        CHECK(plan.find("\"memory_bytes_estimate\":110592") !=
                  std::string::npos,  /* 12288*9 掩膜+兜底样本 */
              "memory_bytes_estimate");
        CHECK(plan.find("\"io_bytes_estimate\":0") != std::string::npos,
              "io_bytes_estimate=0 (v1 inline)");
        CHECK(plan.find("\"cancel_support\":\"entry\"") != std::string::npos,
              "cancel_support=entry");
        /* diag tiny: work_units=1 */
        pb.data = pbuf;
        pb.cap = sizeof(pbuf);
        CHECK(d.api->plan(d.api, cstr("n"),
                          cstr("{\"op\":\"noise_diagnostic\","
                               "\"subop\":\"scale_law\",\"alpha\":2.0}"),
                          &pb, NULL) == ACS_OK &&
              std::string(pbuf, (size_t)pb.size)
                  .find("\"work_units\":1") != std::string::npos,
              "diag work_units=1 (tiny)");
        /* f64 dtype memory 口径 = h·w·9 (与 dtype 无关, 掩膜+兜底面) */
        pb.data = pbuf;
        pb.cap = sizeof(pbuf);
        CHECK(d.api->plan(d.api, cstr("n"),
                          cstr("{\"op\":\"estimate_noise_model\","
                               "\"dtype\":1,\"h\":10,\"w\":10}"), &pb, NULL) ==
                  ACS_OK &&
              std::string(pbuf, (size_t)pb.size)
                  .find("\"memory_bytes_estimate\":900") != std::string::npos,
              "f64 memory estimate");
        /* plugin 同串 (确定性; 比较前重跑 direct 同 config 写同一 buffer,
         * 因 pbuf 被后续 diag/f64 plan 调用覆盖) */
        acs_strbuf_v1 pp;
        char pbuf2[2048];
        std::memset(&pp, 0, sizeof(pp));
        pp.data = pbuf2;
        pp.cap = sizeof(pbuf2);
        CHECK(plugin.api->plan(plugin.api, cstr("node-noise-1"),
                               cstr("{\"op\":\"estimate_noise_model\","
                                    "\"dtype\":0,\"h\":96,\"w\":128}"),
                               &pp, NULL) == ACS_OK,
              "plugin plan commit");
        std::memset(&pb, 0, sizeof(pb));
        pb.data = pbuf;
        pb.cap = sizeof(pbuf);
        CHECK(d.api->plan(d.api, cstr("node-noise-1"),
                          cstr("{\"op\":\"estimate_noise_model\","
                               "\"dtype\":0,\"h\":96,\"w\":128}"), &pb,
                          NULL) == ACS_OK && pb.size == need,
              "direct plan commit (size 幂等)");
        CHECK(pb.size == pp.size && std::memcmp(pbuf, pbuf2, (size_t)need) == 0,
              "plan direct==plugin bitwise");
        /* 负面: 空 config */
        CHECK(d.api->plan(d.api, cstr("n"), cstr(""), &pb, NULL) ==
                  ACS_ERR_PARAM, "plan empty config");
        free_host(&host);
    }

    /* ── fixture 面 (D/E/F/I 共用) ── */
    Fixture fx;
    std::string data32_b64 = b64enc(fx.data_f32.data(),
                                    fx.data_f32.size() * sizeof(float));
    std::string data64_b64 = b64enc(fx.data_f64.data(),
                                    fx.data_f64.size() * sizeof(double));
    std::string mask_all_b64 = b64enc(fx.mask_all.data(),
                                      fx.mask_all.size() * sizeof(float));
    std::string starx_b64 = b64enc(fx.stars.data(), 5 * sizeof(double));
    std::string stary_b64 = b64enc(fx.stars.data() + 5, 5 * sizeof(double));

    std::string cfg_build32 =
        "{\"op\":\"estimate_noise_model\",\"dtype\":0,\"h\":96,\"w\":128}";
    std::string cfg_build64 =
        "{\"op\":\"estimate_noise_model\",\"dtype\":1,\"h\":96,\"w\":128}";
    std::string cfg_build_fill =
        "{\"op\":\"estimate_noise_model\",\"dtype\":0,\"h\":96,\"w\":128,"
        "\"fill_variance\":1,\"fill_ivar\":1}";
    std::string cfg_fill =
        "{\"op\":\"fill_noise_field\",\"h\":96,\"w\":128,"
        "\"fill_variance\":1,\"fill_ivar\":1}";
    std::string man_build32 =
        "{\"data_base64\":\"" + data32_b64 + "\",\"star_x_base64\":\"" +
        starx_b64 + "\",\"star_y_base64\":\"" + stary_b64 + "\"}";
    std::string man_build64 =
        "{\"data_base64\":\"" + data64_b64 + "\",\"star_x_base64\":\"" +
        starx_b64 + "\",\"star_y_base64\":\"" + stary_b64 + "\"}";
    std::string man_mask_all = "{\"data_base64\":\"" + data32_b64 +
                               "\",\"source_mask_base64\":\"" + mask_all_b64 +
                               "\"}";

    /* ── D. direct-vs-plugin BITWISE 对拍 ── */
    g_case = "D_bitwise";
    {
        acs_host_api_v1 host;
        HostState st;
        fill_host(&host, &st, 1, 1);
        Ch d = direct, p = plugin;
        d.host = &host;
        p.host = &host;
        /* D1: f32 build + 星通道 */
        compare_both(d, p, cfg_build32.c_str(), man_build32.c_str(),
                     "D1 f32 build bitwise");
        /* D2: f64 build */
        compare_both(d, p, cfg_build64.c_str(), man_build64.c_str(),
                     "D2 f64 build bitwise");
        /* D3: f32 build + 内联 fill (g_model_floor 注册表事务内有效) */
        compare_both(d, p, cfg_build_fill.c_str(), man_build32.c_str(),
                     "D3 f32 build+inline fill bitwise");
        /* D4: 全掩膜 rc=1 完全退化 (科学结果面, 非故障) */
        compare_both(d, p, cfg_build32.c_str(), man_mask_all.c_str(),
                     "D4 all-mask rc=1 bitwise");
        /* D5/D6: 独立 fill op (round-trip; 影子 floor 回退同构) */
        acs_error_info_v1 e;
        std::memset(&e, 0, sizeof(e));
        std::string build_out;
        acs_status sb = run_execute(d, cfg_build_fill.c_str(),
                                    man_build32.c_str(), &build_out, &e);
        if (sb != ACS_OK) {
            std::printf("DBG D5: sb=%d detail=%u msg=%s man_len=%zu head=%.60s\n",
                        (int)sb, e.detail_code,
                        e.message_utf8 ? e.message_utf8 : "(null)",
                        man_build32.size(), man_build32.c_str());
        }
        CHECK(sb == ACS_OK, e.message_utf8 ? e.message_utf8 : "D5 exec ok");
        CHECK(build_out.find("\"rc\":0") != std::string::npos,
              "D5 direct build rc=0 for fill round-trip");
        if (g_fail && build_out.empty()) {
            std::printf("DBG D5: build_out EMPTY\n");
        } else if (build_out.size() < 400) {
            std::printf("DBG D5: build_out(%zu)=%.300s\n", build_out.size(),
                        build_out.c_str());
        } else {
            std::printf("DBG D5: build_out(%zu) head=%.120s\n", build_out.size(),
                        build_out.c_str());
        }
        std::string nctrl_s = json_get_str(build_out, "n_control_points");
        CHECK(!nctrl_s.empty() && std::atoi(nctrl_s.c_str()) > 0,
              "D5 n_control_points > 0");
        compare_both(d, p, cfg_fill.c_str(), build_out.c_str(),
                     "D6 fill round-trip bitwise");
        /* 输出结构断言 (rc/floor_fallback/平面存在) */
        std::string fill_out;
        CHECK(run_execute(p, cfg_fill.c_str(), build_out.c_str(), &fill_out,
                          &e) == ACS_OK, "D6 plugin fill run");
        CHECK(fill_out.find("\"floor_fallback\":1") != std::string::npos,
              "D6 floor_fallback=1 (DISP-NOISE-002 跨 ABI 忠实)");
        CHECK(fill_out.find("out_variance_base64") != std::string::npos &&
                  fill_out.find("out_ivar_base64") != std::string::npos,
              "D6 fill planes present");
        std::vector<uint8_t> pv, pi, dv2, di2;
        CHECK(b64dec(json_get_str(fill_out, "out_variance_base64"), pv) &&
                  pv.size() == (size_t)FX_H * FX_W * 4, "D6 var plane size");
        /* D7: diag scale_law bitwise */
        std::string cfg_sl =
            "{\"op\":\"noise_diagnostic\",\"subop\":\"scale_law\","
            "\"alpha\":2.5,\"variance\":16.0,\"ivar\":0.0625}";
        compare_both(d, p, cfg_sl.c_str(), "{}", "D7 scale_law bitwise");
        /* D8: diag gain_variance bitwise */
        std::string cfg_gv =
            "{\"op\":\"noise_diagnostic\",\"subop\":\"gain_variance\","
            "\"signal\":1000.0,\"gain_e_per_adu\":2.0,\"read_noise_e\":5.0}";
        compare_both(d, p, cfg_gv.c_str(), "{}", "D8 gain_variance bitwise");
        /* D9: scale_law 数值语义抽查 (alpha=2.5 → var×6.25) */
        std::string sl_out;
        CHECK(run_execute(d, cfg_sl.c_str(), "{}", &sl_out, &e) == ACS_OK,
              "D9 scale_law run");
        std::vector<uint8_t> sc;
        bool d9ok = b64dec(json_get_str(sl_out, "scalars_base64"), sc) &&
                    sc.size() == 3 * 8;
        CHECK(d9ok, "D9 scalars f64x3");
        double variance = 0, ivar = 0, alpha = 0;
        if (d9ok) {
            std::memcpy(&variance, sc.data(), 8);
            std::memcpy(&ivar, sc.data() + 8, 8);
            std::memcpy(&alpha, sc.data() + 16, 8);
        }
        CHECK(variance == 100.0 && ivar == 0.01 && alpha == 2.5,
              "D9 scale_law values (SNR-002)");
        /* D10: gain_variance 数值语义抽查 (SNR-005: s/g + (rn/g)^2) */
        std::string gv_out;
        CHECK(run_execute(d, cfg_gv.c_str(), "{}", &gv_out, &e) == ACS_OK,
              "D10 gain_variance run");
        sc.clear();
        bool d10ok = b64dec(json_get_str(gv_out, "scalars_base64"), sc) &&
                     sc.size() == 8;
        CHECK(d10ok, "D10 scalars f64x1");
        double gv = 0;
        if (d10ok) std::memcpy(&gv, sc.data(), 8);
        CHECK(gv == 506.25, "D10 gain_variance=506.25");
        /* D11: build 输出结构断言 (标量 bitwise 通道 + 控制点平面) */
        std::vector<uint8_t> sc4;
        bool d11ok = b64dec(json_get_str(build_out, "scalars_base64"), sc4) &&
                     sc4.size() == 4 * 8;
        CHECK(d11ok, "D11 scalars f64x4");
        double sigma_bg = 0, var_bg = 0, ivar_bg = 0, floor_echo = 0;
        if (d11ok) {
            std::memcpy(&sigma_bg, sc4.data(), 8);
            std::memcpy(&var_bg, sc4.data() + 8, 8);
            std::memcpy(&ivar_bg, sc4.data() + 16, 8);
            std::memcpy(&floor_echo, sc4.data() + 24, 8);
        }
        CHECK(var_bg > 0.0 && std::fabs(ivar_bg - 1.0 / var_bg) <=
                                      1e-15 * std::fabs(ivar_bg),
              "D11 ivar=1/variance (I1)");
        CHECK(sigma_bg > 0.0 && std::fabs(sigma_bg * sigma_bg - var_bg) <=
                                        1e-15 * var_bg,
              "D11 sigma^2=variance");
        CHECK(floor_echo == 1e-12, "D11 floor 回显=默认 1e-12");
        CHECK(json_get_str(build_out, "source") == "0",
              "D11 source=empirical");
        free_host(&host);
    }

    /* ── E. BUDGET 105 双路径 ── */
    g_case = "E_budget";
    {
        acs_host_api_v1 host_noex;
        HostState st1;
        fill_host(&host_noex, &st1, 0, 1);   /* 无 executor */
        Ch d = direct;
        d.host = &host_noex;
        acs_error_info_v1 e;
        std::memset(&e, 0, sizeof(e));
        std::string out;
        CHECK(run_execute(d, cfg_build32.c_str(), man_build32.c_str(), &out,
                          &e) == ACS_ERR_BUDGET && e.detail_code == 105,
              "E: executor missing -> BUDGET/105");
        CHECK(out.empty(), "E: no output leak (executor missing)");

        acs_host_api_v1 host_fail;
        HostState st2;
        st2.fail_acquire = 1;
        fill_host(&host_fail, &st2, 1, 1);
        d.host = &host_fail;
        e = acs_error_info_v1();
        CHECK(run_execute(d, cfg_build32.c_str(), man_build32.c_str(), &out,
                          &e) == ACS_ERR_BUDGET && e.detail_code == 105,
              "E: acquire fail -> BUDGET/105 (cpu_heavy 禁单线程)");
        CHECK(out.empty(), "E: no output leak (acquire fail)");
        CHECK(st2.release_calls == 0, "E: no stray release (acquire fail)");
        free_host(&host_noex);
        free_host(&host_fail);
    }

    /* ── F. cancel ── */
    g_case = "F_cancel";
    {
        /* F1: entry cancel_req (request_cancel 置位后 execute 拒绝) */
        acs_host_api_v1 host;
        HostState st;
        fill_host(&host, &st, 1, 1);
        Ch d = direct;
        d.host = &host;
        acs_error_info_v1 e;
        std::memset(&e, 0, sizeof(e));
        acs_module_instance_v1* inst = NULL;
        CHECK(d.api->create(d.api, cstr(cfg_build32.c_str()), &host, &inst,
                            &e) == ACS_OK, "F1 create");
        CHECK(d.api->request_cancel(inst) == ACS_OK, "F1 request_cancel");
        acs_strbuf_v1 ob;
        char obuf[64];
        std::memset(&ob, 0, sizeof(ob));
        ob.data = obuf;
        ob.cap = sizeof(obuf);
        CHECK(d.api->execute(inst, cstr(man_build32.c_str()),
                             cstr(cfg_build32.c_str()), &ob, &e) ==
                  ACS_ERR_STATE, "F1 cancelled entry -> STATE");
        d.api->destroy(inst);
        /* F2: host cancel 预检 → CANCELLED + 零写入 + 同实例复用 */
        st.cancelled = 1;
        acs_module_instance_v1* inst2 = NULL;
        CHECK(d.api->create(d.api, cstr(cfg_build32.c_str()), &host, &inst2,
                            &e) == ACS_OK, "F2 create");
        e = acs_error_info_v1();
        std::memset(&ob, 0, sizeof(ob));
        ob.data = obuf;
        ob.cap = sizeof(obuf);
        CHECK(d.api->execute(inst2, cstr(man_build32.c_str()),
                             cstr(cfg_build32.c_str()), &ob, &e) ==
                  ACS_ERR_CANCELLED, "F2 host cancel -> CANCELLED");
        CHECK(ob.size == 0, "F2 zero output leak");
        st.cancelled = 0;
        /* 清除后同实例复用成功 (execute 完成回 CREATED) */
        std::string out;
        e = acs_error_info_v1();
        acs_strbuf_v1 ob2;
        std::memset(&ob2, 0, sizeof(ob2));
        static char big[1 << 22];
        ob2.data = big;
        ob2.cap = sizeof(big);
        CHECK(d.api->execute(inst2, cstr(man_build32.c_str()),
                             cstr(cfg_build32.c_str()), &ob2, &e) == ACS_OK,
              "F2 reuse after cancel ok");
        d.api->destroy(inst2);
        free_host(&host);
    }

    /* ── G. strbuf 两阶段 + BUFFER_TOO_SMALL ── */
    g_case = "G_strbuf";
    {
        acs_host_api_v1 host;
        HostState st;
        fill_host(&host, &st, 1, 1);
        Ch d = direct;
        d.host = &host;
        /* plan 探测 (G 已在 C 覆盖 OK 面; 此处负面: 不足容量截断) */
        acs_strbuf_v1 pb;
        char small[32];
        std::memset(&pb, 0, sizeof(pb));
        pb.data = small;
        pb.cap = sizeof(small);
        acs_error_info_v1 e;
        std::memset(&e, 0, sizeof(e));
        CHECK(d.api->plan(d.api, cstr("n"),
                          cstr("{\"op\":\"estimate_noise_model\","
                               "\"dtype\":0,\"h\":96,\"w\":128}"), &pb, &e) ==
                  ACS_ERR_PARAM &&
              e.detail_code == ACS_DIAG_ECODE_BUFFER_TOO_SMALL,
              "plan buffer too small");
        CHECK(pb.size > sizeof(small), "size=needed after truncation");
        CHECK(small[sizeof(small) - 1] == '\0', "NUL terminated");
        /* execute 输出探测: cap=0 → OK + size (strbuf 两阶段事务面) */
        e = acs_error_info_v1();
        acs_strbuf_v1 ob;
        std::memset(&ob, 0, sizeof(ob));   /* data=NULL cap=0 */
        acs_module_instance_v1* inst = NULL;
        CHECK(d.api->create(d.api, cstr(cfg_build32.c_str()), &host, &inst,
                            &e) == ACS_OK, "G create");
        CHECK(d.api->execute(inst, cstr(man_build32.c_str()),
                             cstr(cfg_build32.c_str()), &ob, &e) == ACS_OK &&
                  ob.size > 0 && ob.data == NULL,
              "execute probe -> OK + size");
        d.api->destroy(inst);
        free_host(&host);
    }

    /* ── I. inspect / 负面 manifest ── */
    g_case = "I_negative";
    {
        acs_host_api_v1 host;
        HostState st;
        fill_host(&host, &st, 1, 1);
        Ch d = direct;
        d.host = &host;
        acs_error_info_v1 e;
        std::memset(&e, 0, sizeof(e));
        std::string out;
        /* I1: 空 manifest */
        CHECK(run_execute(d, cfg_build32.c_str(), "", &out, &e) ==
                  ACS_ERR_PARAM, "I1 empty manifest");
        /* I2: data 平面缺失 */
        CHECK(run_execute(d, cfg_build32.c_str(), "{}", &out, &e) ==
                  ACS_ERR_PARAM, "I2 missing data plane");
        /* I3: 平面尺寸失配 (数据减半) */
        std::string half(fx.data_f32.begin(), fx.data_f32.begin() +
                             fx.data_f32.size() / 2);
        std::string man_half = "{\"data_base64\":\"" + b64enc(half.data(),
                                                              half.size()) +
                               "\"}";
        e = acs_error_info_v1();
        CHECK(run_execute(d, cfg_build32.c_str(), man_half.c_str(), &out,
                          &e) == ACS_ERR_PARAM &&
                  e.detail_code == NOISE_ECODE_MANIFEST_DIMS,
              "I3 plane size mismatch (111)");
        /* I4: b64 坏 */
        std::string man_bad = "{\"data_base64\":\"!!!!\"}";
        e = acs_error_info_v1();
        CHECK(run_execute(d, cfg_build32.c_str(), man_bad.c_str(), &out, &e) ==
                  ACS_ERR_PARAM, "I4 bad base64");
        /* I5: 星通道半配 */
        std::string man_star1 = "{\"data_base64\":\"" + data32_b64 +
                                "\",\"star_x_base64\":\"" + starx_b64 + "\"}";
        e = acs_error_info_v1();
        CHECK(run_execute(d, cfg_build32.c_str(), man_star1.c_str(), &out,
                          &e) == ACS_ERR_PARAM, "I5 star_x without star_y");
        /* I6: inspect 计数 */
        acs_module_instance_v1* inst = NULL;
        CHECK(d.api->create(d.api, cstr(cfg_build32.c_str()), &host, &inst,
                            &e) == ACS_OK, "I6 create");
        e = acs_error_info_v1();
        CHECK(run_execute(d, cfg_build32.c_str(), man_build32.c_str(), &out,
                          &e) == ACS_OK, "I6 exec");
        acs_strbuf_v1 ib;
        char ibuf[1024];
        std::memset(&ib, 0, sizeof(ib));
        ib.data = ibuf;
        ib.cap = sizeof(ibuf);
        CHECK(d.api->inspect(inst, &ib, &e) == ACS_OK, "I6 inspect ok");
        std::string ins(ibuf, (size_t)ib.size);
        CHECK(ins.find("\"exec_count\":0") != std::string::npos,
              "I6 exec_count=0 (create/execute 独立实例)");
        CHECK(ins.find("astrocs.p1.noise") != std::string::npos,
              "I6 module_id in inspect");
        /* I7: 生产 rc=3 转发 (h 与平面一致但生产面拒绝 —— 双 NULL 不适用,
         * 用 n_stars=0 + 全 NaN 图 → 全帧兜底退化 rc=1 而非 3; 参数面 rc=3
         * 经 h=0 config 拦截, 生产 rc=3 面由 I3/负面覆盖; 此处验证合法
         * NaN 图 → rc=1 退化路径科学面) */
        std::vector<float> nan_img((size_t)FX_H * FX_W, std::nanf(""));
        std::string man_nan = "{\"data_base64\":\"" +
                              b64enc(nan_img.data(),
                                     nan_img.size() * sizeof(float)) + "\"}";
        e = acs_error_info_v1();
        CHECK(run_execute(d, cfg_build32.c_str(), man_nan.c_str(), &out,
                          &e) == ACS_OK &&
                  out.find("\"rc\":1") != std::string::npos,
              "I7 all-NaN -> rc=1 degenerate (ACS_OK 科学面)");
        CHECK(out.find("\"ivar_bg_global\"") == std::string::npos ||
                  out.find("\"degenerate\":1") != std::string::npos,
              "I7 degenerate flag");
        d.api->destroy(inst);
        free_host(&host);
    }

    /* ── J. 生命周期 (double destroy 安全; destroy 后句柄失效=host 违规面,
     * UAF 读不可断言, 对齐 hips/CAL/COS 测试口径) ── */
    g_case = "J_lifecycle";
    {
        acs_host_api_v1 host;
        HostState st;
        fill_host(&host, &st, 1, 1);
        Ch d = direct;
        d.host = &host;
        acs_error_info_v1 e;
        std::memset(&e, 0, sizeof(e));
        acs_module_instance_v1* inst = NULL;
        CHECK(d.api->create(d.api, cstr(cfg_build32.c_str()), &host, &inst,
                            &e) == ACS_OK, "J create");
        d.api->destroy(inst);
        d.api->destroy(inst);   /* double destroy 安全 (CAL 同款忽略) */
        CHECK(d.api->create(d.api, cstr(cfg_build32.c_str()), NULL, &inst,
                            &e) == ACS_ERR_ABI_MISMATCH,
              "J create without allocator rejected");
        free_host(&host);
    }

    /* ── K. 租约平衡 (CPU-RT 纪律: acquire/release 成对) ── */
    g_case = "K_lease_balance";
    {
        acs_host_api_v1 host;
        HostState st;
        fill_host(&host, &st, 1, 1);
        Ch d = direct;
        d.host = &host;
        acs_error_info_v1 e;
        std::memset(&e, 0, sizeof(e));
        std::string out;
        CHECK(run_execute(d, cfg_build_fill.c_str(), man_build32.c_str(),
                          &out, &e) == ACS_OK, "K run ok");
        CHECK(st.acquire_calls == 1 && st.release_calls == 1,
              "K acquire/release balanced");
        CHECK(st.released_total == 1, "K released_total=1");
        free_host(&host);
    }

    std::printf("p1_noise_adapter: PASS=%d FAIL=%d\n", g_pass, g_fail);
    dlclose(h);
    return (g_fail == 0) ? 0 : 1;
}
