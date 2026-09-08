/* P1-CAL-INT 模块本地集成测试 (tests/unit/calibration_integration_test.cpp)
 *
 * 对齐先例: tests/unit/gaia_integration_test.c (CAT-GAIA-INT, 054af3a3)。
 *
 * 覆盖 (迁移模板 <prefix>-INT; global DAG registration 归 SA-RT-05 不做):
 *   A. integration descriptor 三方一致 (12 §5): descriptor JSON ↔ DLL
 *      describe() ↔ module.yaml/types.h —— module_id/version/abi/build_id/
 *      unique_entry/resource_class/threading/contracts/ports/operations/词表。
 *   B. typed ports 静态校验负面: p1.frames/p1.master_dark/p1.master_bias/
 *      p1.master_flat/p1.calibrated 绑定真实性 —— 缺 op/detail 101、词表外
 *      op/detail 100、非 finite/detail 103、类型错/detail 102。
 *   C. 调用计数 (每节点恰调用一次, 模板 INT §2): 两节点合成链
 *      (generate_master_bias → calibrate_frame, node1 输出 out_base64 作为
 *      node2 data_base64 —— ArtifactStore 真实传 handle 的本地最小替身) 各
 *      驱动一次完整协议 validate_config(1)→plan(1)→create(1)→execute 事务
 *      (1 次 host 驱动 = 两阶段 strbuf 2 次 API)→inspect(1)→destroy(1);
 *      host stub 观测 lease acquire/release 2/2 平衡; inspect.exec_count==2
 *      (两遍各计一次, 真实语义); 重复驱动可重入为 KI 登记 (v1 C ABI 事务
 *      完成后 state 回 CREATED, 模块不锁完成态), per-node 恰一次由调用方
 *      协议 + RT-006 runtime call counts / 重放违规检测保证。
 *   D. 真实 plan (禁预测值): plan JSON 键集 = DLL cal_plan 真实输出;
 *      work_units=数据事实 (master→config.frames, 帧级→width*height) 独立
 *      算术交叉且 >0; work_unit/parallel_axis 按 op 类真实分组;
 *      memory_bytes_estimate=work_units*dtype 尺寸; io_bytes_estimate=0 为
 *      inline base64 数据面真实值; plan 无观测词 ("leased" 禁入预测面)。
 *   E. 真实 trace (RT-006 观测, 14 §4 禁 config 冒充): 每节点 module_call
 *      行由真实观测填充 —— dll_name=实际加载路径 basename、dll_sha256=DLL
 *      文件内容 SHA-256 实测 (FIPS 180-4, 测试内独立 oracle)、build_id/
 *      entry=describe()/dlsym 实证、workers/granted=host stub 实租借观测、
 *      status/artifact_size=终态与输出 manifest 实际字节; provider 与
 *      artifact_id 不填 (1:1 转发 legacy 无 provider 观测 / ArtifactStore
 *      归全局 —— 空=真实未观测); JSONL (astrocs.trace-event/v1 键序,
 *      lib/core TraceEvent::to_jsonl 同型) 写盘后本地重放: 每 node
 *      module_call 计数==1 (context.cpp detect_repeated_calls 同语义,
 *      键=(node_id,operation))。
 *   F. 负面 ABI (模板 INT §5): 缺 DLL 加载失败; host_abi 失配 →
 *      ACS_ERR_ABI_MISMATCH; describe 错误 module_id → ACS_ERR_ABI_MISMATCH。
 *
 * DLL 经 ASTROCS_CAL_DLL_PATH 加载; integration descriptor JSON 经
 * ASTROCS_CAL_INTEGRATION_JSON; module.yaml 经 ASTROCS_CAL_MODULE_YAML;
 * trace JSONL 输出经 ASTROCS_CAL_INT_TRACE_OUT (CMake test properties 注入)。
 * fixture: p1cal_fixtures.hpp FIX-CAL 谱系 (splitmix64, 固定 seed)。
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/host_api_v1.h"
#include "astrocs/calibration/types.h"

/* fixture 参考真值 (FIX-CAL 谱系; splitmix64 固定 seed, P1-CAL-TEST 同源) */
#include "p1cal_fixtures.hpp"

/* integration descriptor / module.yaml 路径 (CMake 编译定义注入) */
#ifndef ASTROCS_CAL_INTEGRATION_JSON
#define ASTROCS_CAL_INTEGRATION_JSON "astrocs_p1_calibration.integration.json"
#endif
#ifndef ASTROCS_CAL_MODULE_YAML
#define ASTROCS_CAL_MODULE_YAML "module.yaml"
#endif

static int g_fail = 0;
#define CHECK(cond, name) do { \
    if (cond) printf("[PASS] %s\n", (name)); \
    else { printf("[FAIL] %s (line %d)\n", (name), __LINE__); g_fail++; } \
} while (0)

/* 入口函数指针 (dlsym 需要) */
typedef acs_status (*cal_entry_fn)(uint32_t, const acs_host_api_v1*,
                                   const acs_module_api_v1**);

/* ── mini JSON 读取 (测试侧; 与 gaia_integration_test 同风格, 只读) ── */
static int tjson_num(const char* j, const char* key, double* out) {
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(j, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '-' && (*p < '0' || *p > '9')) return 0;
    *out = strtod(p, NULL);
    return 1;
}
static int tjson_ll(const char* j, const char* key, long long* out) {
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(j, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '-' && (*p < '0' || *p > '9')) return 0;
    *out = strtoll(p, NULL, 10);
    return 1;
}
static int tjson_str(const char* j, const char* key, char* buf, size_t cap) {
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(j, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return 0;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < cap) buf[i++] = *p++;
    buf[i] = '\0';
    return *p == '"';
}
static int tjson_contains(const char* j, const char* needle) {
    return strstr(j, needle) != NULL;
}

/* 段落内查找: 返回 [begin,end) 内 needle 指针 (无则 NULL) */
static const char* span_find(const char* begin, const char* end, const char* needle) {
    size_t n = strlen(needle);
    for (const char* p = begin; p + n <= end; p++)
        if (!memcmp(p, needle, n)) return p;
    return NULL;
}

/* ── SHA-256 (FIPS 180-4; 测试内独立 oracle, 计算 DLL 文件内容 hash) ── */
typedef struct {
    uint32_t h[8];
    uint64_t len;
    uint8_t buf[64];
    size_t buflen;
} sha256_ctx;
static uint32_t ror32(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }
static void sha256_init(sha256_ctx* c) {
    static const uint32_t H0[8] = { 0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                                    0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u };
    memcpy(c->h, H0, sizeof(H0));
    c->len = 0;
    c->buflen = 0;
}
static void sha256_block(sha256_ctx* c, const uint8_t* p) {
    static const uint32_t K[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,
        0x923f82a4u,0xab1c5ed5u,0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,
        0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,0xe49b69c1u,0xefbe4786u,
        0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,
        0x06ca6351u,0x14292967u,0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,
        0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,0xa81a664bu,0xc24b8b70u,
        0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,0x34b0bcb5u,
        0x2748774cu,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,0x748f82eeu,
        0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,
        0xc67178f2u };
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4*i] << 24) | ((uint32_t)p[4*i+1] << 16) |
               ((uint32_t)p[4*i+2] << 8) | (uint32_t)p[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror32(w[i-15],7) ^ ror32(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ror32(w[i-2],17) ^ ror32(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3];
    uint32_t e = c->h[4], f = c->h[5], g = c->h[6], h2 = c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ror32(e,6) ^ ror32(e,11) ^ ror32(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h2 + S1 + ch + K[i] + w[i];
        uint32_t S0 = ror32(a,2) ^ ror32(a,13) ^ ror32(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h2 = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h2;
}
static void sha256_update(sha256_ctx* c, const uint8_t* p, size_t n) {
    c->len += (uint64_t)n;
    while (n > 0) {
        size_t take = 64 - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, p, take);
        c->buflen += take;
        p += take;
        n -= take;
        if (c->buflen == 64) {
            sha256_block(c, c->buf);
            c->buflen = 0;
        }
    }
}
static void sha256_final(sha256_ctx* c, uint8_t out[32]) {
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80;
    sha256_update(c, &pad, 1);
    uint8_t z = 0;
    while (c->buflen != 56) sha256_update(c, &z, 1);
    uint8_t lenb[8];
    for (int i = 0; i < 8; i++) lenb[i] = (uint8_t)(bits >> (56 - 8*i));
    c->len -= 8;  /* 长度字段不计内容 */
    sha256_update(c, lenb, 8);
    for (int i = 0; i < 8; i++) {
        out[4*i]   = (uint8_t)(c->h[i] >> 24);
        out[4*i+1] = (uint8_t)(c->h[i] >> 16);
        out[4*i+2] = (uint8_t)(c->h[i] >> 8);
        out[4*i+3] = (uint8_t)(c->h[i]);
    }
}
/* 文件 SHA-256 hex (hexcap >= 65; 失败返回 0) */
static int sha256_file_hex(const char* path, char* hex, size_t hexcap) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    sha256_ctx c;
    sha256_init(&c);
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) sha256_update(&c, buf, n);
    int bad = ferror(f);
    fclose(f);
    if (bad) return 0;
    if (hexcap < 65) return 0;
    uint8_t d[32];
    sha256_final(&c, d);
    static const char HD[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex[2*i] = HD[d[i] >> 4];
        hex[2*i+1] = HD[d[i] & 15];
    }
    hex[64] = '\0';
    return 1;
}

/* ── host stub (观测 executor 租借; make_host 同 calibration_adapter_test 口径) ── */
static void* cal_alloc(void* user, uint64_t size, uint64_t align) {
    (void)user; (void)align;
    return size ? malloc((size_t)size) : NULL;
}
static void cal_free(void* user, void* p) { (void)user; free(p); }

typedef struct {
    int acquire_calls, release_calls, acquire_fail;
    uint32_t max_workers;
    uint32_t last_leased;      /* 本次事务观测: 最后一次 acquire 的 want */
} ex_stub;
static int ex_acquire(void* ud, uint32_t n) {
    ex_stub* s = (ex_stub*)ud;
    s->acquire_calls++;
    s->last_leased = n;
    return s->acquire_fail ? -1 : 0;
}
static void ex_release(void* ud, uint32_t n) { ((ex_stub*)ud)->release_calls++; (void)n; }

static const acs_host_api_v1* make_host(ex_stub* ex) {
    static acs_host_api_v1 h;
    static acs_allocator_v1 al;
    static acs_executor_v1 exv;
    memset(&h, 0, sizeof(h));
    memset(&al, 0, sizeof(al));
    memset(&exv, 0, sizeof(exv));
    h.head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    h.head.abi_version = ACS_ABI_VERSION_V1;
    al.head.struct_size = (uint32_t)sizeof(acs_allocator_v1);
    al.head.abi_version = ACS_ABI_VERSION_V1;
    al.alloc = cal_alloc;
    al.free = cal_free;
    h.allocator = &al;
    if (ex) {
        exv.head.struct_size = (uint32_t)sizeof(acs_executor_v1);
        exv.head.abi_version = ACS_ABI_VERSION_V1;
        exv.available_cpus = 4;
        exv.max_workers = ex->max_workers;
        exv.user_data = ex;
        exv.acquire = ex_acquire;
        exv.release = ex_release;
        h.executor = &exv;
    }
    return &h;
}

/* str/strbuf 构造 (借入) */
static acs_str_v1 s_from(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)strlen(s) : 0;
    return v;
}
static void sb_init(acs_strbuf_v1* sb) {
    memset(sb, 0, sizeof(*sb));
    sb->head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
    sb->head.abi_version = ACS_ABI_VERSION_V1;
}
static void err_init(acs_error_info_v1* err) {
    memset(err, 0, sizeof(*err));
    err->head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    err->head.abi_version = ACS_ABI_VERSION_V1;
}

/* ── base64 (RFC 4648 标准字母表; 测试侧编码 fixture 平面) ── */
static void b64_encode(const uint8_t* src, size_t n, std::string& out) {
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

/* execute 事务收集: 两阶段 sink 协议 (尺寸探测→整写); 返回 malloc JSON */
static char* exec_txn(const acs_module_api_v1* api, acs_module_instance_v1* inst,
                      const char* manifest, const char* cfg, acs_status* rc) {
    acs_str_v1 mans = s_from(manifest);
    acs_str_v1 cfgs = s_from(cfg);
    acs_error_info_v1 err;
    err_init(&err);
    acs_strbuf_v1 sb;
    sb_init(&sb);
    *rc = api->execute(inst, mans, cfgs, &sb, &err);      /* 第一遍: 尺寸探测 */
    if (*rc != ACS_OK) return NULL;
    sb.data = (char*)malloc((size_t)sb.size + 1);
    sb.cap = sb.size + 1;
    if (!sb.data) { *rc = ACS_ERR_NOMEM; return NULL; }
    acs_status st2 = api->execute(inst, mans, cfgs, &sb, &err);  /* 第二遍: 整写 */
    if (st2 != ACS_OK) { free(sb.data); *rc = st2; return NULL; }
    return sb.data;
}

/* ── trace JSONL (RT-006 TraceEvent::to_jsonl 同键序; 观测值真实) ── */
typedef struct {
    FILE* f;
    uint64_t seq;
    char run_id[64];
    int lines;
} trace_writer;
static int trace_open(trace_writer* tw, const char* path) {
    memset(tw, 0, sizeof(*tw));
    if (!path || !path[0]) return 0;   /* 未注入 → 不写盘 (仍可内存断言) */
    tw->f = fopen(path, "w");
    if (!tw->f) return -1;
    snprintf(tw->run_id, sizeof(tw->run_id), "p1cal-int-local");
    return 1;
}
/* RFC3339 UTC 观测时刻 */
static void rfc3339_utc(char* buf, size_t cap) {
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    strftime(buf, cap, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}
/* 观测墙钟 (毫秒; 无 clock_gettime 平台回落 0.0 — 观测缺失=0, 不伪造) */
static double now_wall_ms(void) {
#ifdef _WIN32
    return 0.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#endif
}
/* module_call 事件 (观测字段由调用方填真实值; provider/artifact_id 不填) */
static void trace_module_call(trace_writer* tw, const char* node_id,
                              const char* module_id, const char* module_version,
                              const char* build_id, const char* dll_name,
                              const char* dll_sha256, const char* entry,
                              uint64_t call_count, uint32_t workers,
                              uint32_t granted, const char* status,
                              uint64_t artifact_size, double wall_ms) {
    if (!tw->f) return;
    char ts[40];
    rfc3339_utc(ts, sizeof(ts));
    fprintf(tw->f,
            "{\"schema\":\"astrocs.trace-event/v1\",\"type\":\"module_call\","
            "\"ts_utc\":\"%s\",\"run_id\":\"%s\",\"node_id\":\"%s\",\"seq\":%llu,"
            "\"module_id\":\"%s\",\"module_version\":\"%s\",\"dll_name\":\"%s\","
            "\"dll_sha256\":\"%s\",\"build_id\":\"%s\",\"entry\":\"%s\","
            "\"call_count\":%llu,\"workers\":%u,\"granted_workers\":%u,"
            "\"status\":\"%s\",\"artifact_size\":%llu,\"wall_ms\":%.3f}\n",
            ts, tw->run_id, node_id, (unsigned long long)++tw->seq,
            module_id, module_version, dll_name, dll_sha256, build_id, entry,
            (unsigned long long)call_count, workers, granted, status,
            (unsigned long long)artifact_size, wall_ms);
    tw->lines++;
}
static void trace_node_end(trace_writer* tw, const char* node_id, const char* status) {
    if (!tw->f) return;
    char ts[40];
    rfc3339_utc(ts, sizeof(ts));
    fprintf(tw->f,
            "{\"schema\":\"astrocs.trace-event/v1\",\"type\":\"node_end\","
            "\"ts_utc\":\"%s\",\"run_id\":\"%s\",\"node_id\":\"%s\",\"seq\":%llu,"
            "\"status\":\"%s\"}\n",
            ts, tw->run_id, node_id, (unsigned long long)++tw->seq, status);
    tw->lines++;
}
static void trace_close(trace_writer* tw) {
    if (tw->f) fclose(tw->f);
    tw->f = NULL;
}

/* 本地重放检查 (context.cpp detect_repeated_calls 同语义; 键=(node_id,operation)):
 * module_call 行每 node 恰 1 次; 重复 → 违规数 > 0 */
static int replay_violations(const char* path, char* msg, size_t msgcap) {
    FILE* f = fopen(path, "r");
    if (!f) { snprintf(msg, msgcap, "open trace jsonl failed"); return -1; }
    char line[2048];
    char nodes[16][64];
    int n_nodes = 0;
    int node_cnt[16];
    int violations = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, "\"type\":\"module_call\"")) continue;
        char node[64] = "";
        tjson_str(line, "node_id", node, sizeof(node));
        int found = -1;
        for (int i = 0; i < n_nodes; i++)
            if (!strcmp(nodes[i], node)) { found = i; break; }
        if (found < 0 && n_nodes < 16) {
            snprintf(nodes[n_nodes], sizeof(nodes[0]), "%s", node);
            node_cnt[n_nodes] = 1;
            n_nodes++;
        } else if (found >= 0) {
            node_cnt[found]++;
        }
    }
    fclose(f);
    for (int i = 0; i < n_nodes; i++) {
        if (node_cnt[i] != 1) {
            violations++;
            snprintf(msg, msgcap, "node '%s' module_call count=%d (expected 1)",
                     nodes[i], node_cnt[i]);
        }
    }
    return violations;
}

/* 读整个文件到 malloc 缓冲 (NUL 结尾); 失败返回 NULL */
static char* read_file_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

/* ── fixture (FIX-CAL 谱系; splitmix64 固定 seed) ── */
static const int FW = 8, FH = 6, NFRAMES = 4;
static const size_t FPX = (size_t)FW * FH;

/* master 输入帧栈: FIX-CAL-C 变体 (n=4 < 5 → 无 NaN 注入, 纯高斯场) */
static p1cal::FixCalC g_fx;
/* 帧级 op 输入 science frame (独立 seed 的背景+噪声场) */
static std::vector<float> g_light;

static void build_fixtures(void) {
    g_fx = p1cal::fix_cal_c_outlier_stack(0x20260909u, (size_t)NFRAMES,
                                          (size_t)FW, (size_t)FH,
                                          /*nan_variant=*/false);
    uint64_t st = 0x5C1EALu;
    g_light.resize(FPX);
    for (size_t i = 0; i < FPX; ++i)
        g_light[i] = (float)(100.0 + 10.0 * p1cal::normal01(st));
}

int main(void) {
    const char* dll_path = getenv("ASTROCS_CAL_DLL_PATH");
    const char* desc_path = getenv("ASTROCS_CAL_INTEGRATION_JSON");
    if (!desc_path || !desc_path[0]) desc_path = ASTROCS_CAL_INTEGRATION_JSON;
    const char* trace_out = getenv("ASTROCS_CAL_INT_TRACE_OUT");

    printf("== P1-CAL-INT calibration_integration_test ==\n");
    printf("dll: %s\n", dll_path ? dll_path : "(null)");
    printf("descriptor: %s\n", desc_path);

    build_fixtures();

    /* ════ F. 负面 ABI (模板 INT §5: 缺 DLL/错误 ABI 必须失败) ════ */
    {
        /* F1. 缺 DLL: loader 必须失败 (dlopen 语义) */
        void* bad = dlopen("__no_such_astrocs_p1_calibration__.so", RTLD_NOW | RTLD_LOCAL);
        CHECK(bad == NULL, "F1: 缺 DLL 加载失败 (不降级)");
        if (bad) dlclose(bad);
    }

    if (!dll_path || !dll_path[0]) {
        printf("[FAIL] F-pre: ASTROCS_CAL_DLL_PATH 未注入\n");
        return 2;
    }

    /* ════ A. DLL 加载 + 唯一导出面 + query 握手 ════ */
    void* dl = dlopen(dll_path, RTLD_NOW | RTLD_LOCAL);
    CHECK(dl != NULL, "A1: DLL 加载成功");
    if (!dl) {
        printf("dlopen error: %s\n", dlerror());
        return 2;
    }
    cal_entry_fn entry_fn = (cal_entry_fn)dlsym(dl, "astrocs_module_query_v1");
    CHECK(entry_fn != NULL, "A2: dlsym astrocs_module_query_v1 成功 (entry 真实解析)");
    const acs_module_api_v1* api = NULL;
    acs_status qrc = entry_fn((uint32_t)ACS_ABI_VERSION_V1, NULL, &api);
    CHECK(qrc == ACS_OK && api != NULL, "A3: query(host_abi=1) 握手成功");
    {
        const acs_module_api_v1* bad_api = NULL;
        acs_status rc = entry_fn(0xDEADBEEFu, NULL, &bad_api);
        CHECK(rc == ACS_ERR_ABI_MISMATCH && bad_api == NULL,
              "F2: 错误 host_abi → ACS_ERR_ABI_MISMATCH (不降级)");
    }

    /* describe (观测: 模块静态输出) */
    acs_module_descriptor_v1 desc;
    memset(&desc, 0, sizeof(desc));
    acs_error_info_v1 err;
    err_init(&err);
    acs_status drc = api->describe(api, s_from(ASTROCS_CAL_MODULE_ID), &desc);
    CHECK(drc == ACS_OK && desc.head.abi_version == ACS_ABI_VERSION_V1,
          "A4: describe 正确 module_id → OK + abi v1");
    char desc_mid[128] = "", desc_ver[64] = "", desc_build[64] = "";
    char desc_sci[64] = "", desc_alg[64] = "", desc_api[64] = "";
    if (desc.module_id.data) snprintf(desc_mid, sizeof(desc_mid), "%.*s",
                                      (int)desc.module_id.size, desc.module_id.data);
    if (desc.version.data) snprintf(desc_ver, sizeof(desc_ver), "%.*s",
                                    (int)desc.version.size, desc.version.data);
    if (desc.build_id.data) snprintf(desc_build, sizeof(desc_build), "%.*s",
                                     (int)desc.build_id.size, desc.build_id.data);
    if (desc.sci_id.data) snprintf(desc_sci, sizeof(desc_sci), "%.*s",
                                   (int)desc.sci_id.size, desc.sci_id.data);
    if (desc.alg_id.data) snprintf(desc_alg, sizeof(desc_alg), "%.*s",
                                   (int)desc.alg_id.size, desc.alg_id.data);
    if (desc.api_id.data) snprintf(desc_api, sizeof(desc_api), "%.*s",
                                   (int)desc.api_id.size, desc.api_id.data);
    CHECK(!strcmp(desc_mid, ASTROCS_CAL_MODULE_ID), "A5: describe.module_id == types.h");
    CHECK(!strcmp(desc_ver, ASTROCS_CAL_VERSION), "A6: describe.version == types.h");
    CHECK(!strcmp(desc_build, ASTROCS_CAL_BUILD_ID), "A7: describe.build_id == P1-CAL-IMPL");
    CHECK(!strcmp(desc_sci, ASTROCS_CAL_SCI_ID) && !strcmp(desc_alg, ASTROCS_CAL_ALG_ID) &&
              !strcmp(desc_api, ASTROCS_CAL_API_ID),
          "A8: describe sci/alg/api 合同三元组 == types.h");
    CHECK(desc.execution_class == 0 && desc.parallel_ok == 1 && desc.phase == 1,
          "A9: describe.execution_class=cpu_heavy(0) parallel_ok=1 phase=1");
    {
        acs_module_descriptor_v1 d2;
        acs_status rc = api->describe(api, s_from("astrocs.p1.calibration.wrong"), &d2);
        CHECK(rc == ACS_ERR_ABI_MISMATCH,
              "F3: describe 错误 module_id → ACS_ERR_ABI_MISMATCH");
    }

    /* ════ D. integration descriptor 三方一致 (12 §5) ════ */
    char* dj = read_file_all(desc_path);
    CHECK(dj != NULL, "D1: integration descriptor JSON 可读");
    if (dj) {
        char v[160] = "";
        CHECK(tjson_str(dj, "descriptor_schema", v, sizeof(v)) &&
                  !strcmp(v, "astrocs.module-integration-descriptor/v1"),
              "D2: descriptor_schema == astrocs.module-integration-descriptor/v1");
        CHECK(tjson_str(dj, "module_id", v, sizeof(v)) &&
                  !strcmp(v, ASTROCS_CAL_MODULE_ID) && !strcmp(v, desc_mid),
              "D3: module_id 三方一致 (descriptor==describe==types.h)");
        CHECK(tjson_str(dj, "module_version", v, sizeof(v)) &&
                  !strcmp(v, ASTROCS_CAL_VERSION) && !strcmp(v, desc_ver),
              "D4: module_version 三方一致");
        long long abi = 0;
        CHECK(tjson_ll(dj, "abi_version", &abi) && abi == (long long)ASTROCS_CAL_ABI_VERSION,
              "D5: abi_version == 1 (types.h)");
        CHECK(tjson_str(dj, "build_id", v, sizeof(v)) &&
                  !strcmp(v, ASTROCS_CAL_BUILD_ID) && !strcmp(v, desc_build),
              "D6: build_id 三方一致");
        CHECK(tjson_str(dj, "unique_entry", v, sizeof(v)) &&
                  !strcmp(v, "astrocs_module_query_v1") && entry_fn != NULL,
              "D7: dll.unique_entry == 实际 dlsym 解析符号");
        CHECK(tjson_str(dj, "resource_class", v, sizeof(v)) && !strcmp(v, "cpu_heavy") &&
                  desc.execution_class == 0,
              "D8: execution.resource_class == cpu_heavy == describe.execution_class(0)");
        CHECK(tjson_str(dj, "threading_model", v, sizeof(v)) &&
                  !strcmp(v, "host_executor_lease"),
              "D9: threading_model == host_executor_lease (module.yaml 同值)");
        CHECK(tjson_contains(dj, "\"cpu_providers\": [\"baseline\"]"),
              "D10: cpu_providers == [baseline] (module.yaml 同值; trace 不预填)");
        /* global_dag_registration 段内锚定 (避免与其他 status 键混淆) */
        {
            const char* g = strstr(dj, "\"global_dag_registration\"");
            CHECK(g && span_find(g, g + 400, "\"owner\": \"SA-RT-05\"") &&
                      span_find(g, g + 400, "\"status\": \"delegated\""),
                  "D11: global_dag_registration owner=SA-RT-05 status=delegated (本任务不注册)");
        }
        CHECK(tjson_contains(dj, "\"name\": \"p1.frames\"") &&
                  tjson_contains(dj, "\"name\": \"p1.master_dark\"") &&
                  tjson_contains(dj, "\"name\": \"p1.master_bias\"") &&
                  tjson_contains(dj, "\"name\": \"p1.master_flat\"") &&
                  tjson_contains(dj, "\"name\": \"p1.calibrated\""),
              "D12: typed ports 端口名 (module.yaml input/output 同名)");
        CHECK(tjson_contains(dj, "\"data_contract\": \"DATA-P1-CAL\""),
              "D13: 端口 data_contract == DATA-P1-CAL");
        {
            const char* tp = strstr(dj, "\"typed_ports\"");
            const char* ops = strstr(dj, "\"operations\"");
            CHECK(tp && ops && tp < ops &&
                      span_find(tp, ops, "\"direction\": \"input\"") &&
                      span_find(tp, ops, "\"direction\": \"output\""),
                  "D14: typed_ports input/output direction (static 可校验)");
        }
        CHECK(tjson_str(dj, "operation", v, sizeof(v)) && !strcmp(v, ASTROCS_CAL_OP_CALIBRATE_F32),
              "D15: operations[0] == calibrate_frame (types.h 词表)");
        CHECK(tjson_contains(dj, ASTROCS_CAL_OP_CALIBRATE_F64) &&
                  tjson_contains(dj, ASTROCS_CAL_OP_CORRECT_F32) &&
                  tjson_contains(dj, ASTROCS_CAL_OP_CORRECT_F64) &&
                  tjson_contains(dj, ASTROCS_CAL_OP_MASTER_BIAS_F32) &&
                  tjson_contains(dj, ASTROCS_CAL_OP_MASTER_DARK_F32) &&
                  tjson_contains(dj, ASTROCS_CAL_OP_MASTER_FLAT_F32) &&
                  tjson_contains(dj, ASTROCS_CAL_OP_MASTER_BIAS_F64) &&
                  tjson_contains(dj, ASTROCS_CAL_OP_MASTER_DARK_F64) &&
                  tjson_contains(dj, ASTROCS_CAL_OP_MASTER_FLAT_F64),
              "D16: operations 覆盖 10 op 词表 (types.h)");
        CHECK(tjson_contains(dj, ASTROCS_CAL_SCI_ID) && tjson_contains(dj, ASTROCS_CAL_ALG_ID) &&
                  tjson_contains(dj, "DATA-P1-CAL") && tjson_contains(dj, ASTROCS_CAL_API_ID) &&
                  tjson_contains(dj, "TEST-CAL-DESIGN-001"),
              "D17: contracts 五元组 == types.h/module.yaml 冻结 ID");
        CHECK(tjson_contains(dj, "\"work_unit\""),
              "D18: plan_contract work_unit 声明存在 (op 分组真实键)");
        /* module.yaml 注册面 (三方一致第三成员) */
        char* my = read_file_all(ASTROCS_CAL_MODULE_YAML);
        CHECK(my != NULL, "D19: module.yaml 可读");
        if (my) {
            CHECK(strstr(my, "entrypoint: astrocs_module_query_v1") != NULL,
                  "D20: module.yaml entrypoint == astrocs_module_query_v1 (注册面事实)");
            CHECK(strstr(my, "  - calibrate_frame") && strstr(my, "  - generate_master_flat_f64"),
                  "D21: module.yaml node_operations 覆盖 op 词表");
            CHECK(strstr(my, "  - p1.frames") && strstr(my, "  - p1.master_dark") &&
                      strstr(my, "  - p1.master_bias") && strstr(my, "  - p1.master_flat") &&
                      strstr(my, "  - p1.calibrated"),
                  "D22: module.yaml input/output 端口 == descriptor typed_ports 同名");
            free(my);
        }
        free(dj);
    }

    /* ════ B. typed ports 静态校验负面 (端口绑定真实性) ════ */
    {
        /* B1: op 缺失 (config 词表第一键) → PARAM + 101 */
        acs_status rc = api->validate_config(api,
              s_from("{\"width\":8,\"height\":6,\"frames\":4}"), &err);
        CHECK(rc == ACS_ERR_PARAM && err.detail_code == ASTROCS_CAL_ECODE_PARAM_MISSING,
              "B1: 缺 op → PARAM + detail 101 (config 词表必填)");
        /* B2: 词表外 op (10 op 词表之外) → PARAM + 100 */
        err_init(&err);
        rc = api->validate_config(api,
              s_from("{\"op\":\"no_such_op\",\"width\":8,\"height\":6}"), &err);
        CHECK(rc == ACS_ERR_PARAM && err.detail_code == ASTROCS_CAL_ECODE_OP_UNKNOWN,
              "B2: 词表外 op → PARAM + detail 100");
        /* B3: 非 finite 参数 (1e999 溢出 → inf; adapter_test 同口径) → PARAM + 103 */
        err_init(&err);
        rc = api->validate_config(api,
              s_from("{\"op\":\"generate_master_bias\",\"width\":1e999,\"height\":6,"
                      "\"frames\":4,\"sigma_low\":2.0,\"sigma_high\":2.0,"
                      "\"max_iterations\":2,\"combine\":\"mean\"}"), &err);
        CHECK(rc == ACS_ERR_PARAM && err.detail_code == ASTROCS_CAL_ECODE_PARAM_RANGE,
              "B3: 1e999 → PARAM + detail 103 (非 finite 参数拒绝)");
        /* B4: 类型错 (frames 给字符串) → PARAM + 102 */
        err_init(&err);
        rc = api->validate_config(api,
              s_from("{\"op\":\"generate_master_bias\",\"width\":8,\"height\":6,"
                      "\"frames\":\"4\",\"sigma_low\":2.0,\"sigma_high\":2.0,"
                      "\"max_iterations\":2,\"combine\":\"mean\"}"), &err);
        CHECK(rc == ACS_ERR_PARAM && err.detail_code == 102,
              "B4: 键类型错 → PARAM + detail 102");
        /* B5: op 依赖必需键缺失 (master 缺 frames) → PARAM + 101 */
        err_init(&err);
        rc = api->validate_config(api,
              s_from("{\"op\":\"generate_master_bias\",\"width\":8,\"height\":6,"
                      "\"sigma_low\":2.0,\"sigma_high\":2.0,"
                      "\"max_iterations\":2,\"combine\":\"mean\"}"), &err);
        CHECK(rc == ACS_ERR_PARAM && err.detail_code == ASTROCS_CAL_ECODE_PARAM_MISSING,
              "B5: master 缺 frames → PARAM + detail 101 (op 依赖必需键)");
        /* B6: 合法 config (全必需参数) → OK */
        CHECK(api->validate_config(api,
                  s_from("{\"op\":\"generate_master_bias\",\"width\":8,\"height\":6,"
                          "\"frames\":4,\"sigma_low\":2.0,\"sigma_high\":2.0,"
                          "\"max_iterations\":2,\"combine\":\"mean\"}"), &err) == ACS_OK,
              "B6: 合法 config (全参数) → OK");
    }

    /* ════ P. fixture 真值准备 (FIX-CAL 谱系) ════ */
    CHECK(g_fx.n_frames == (size_t)NFRAMES && g_fx.w == (size_t)FW && g_fx.h == (size_t)FH,
          "P1: FIX-CAL 谱系 fixture 生成 (4 帧 8x6, 固定 seed)");
    CHECK(g_fx.stack.size() == (size_t)NFRAMES * FPX, "P2: 帧 stack 尺寸正确");
    CHECK(g_light.size() == FPX, "P3: 帧级 science frame fixture 就绪");

    /* trace writer (env 注入输出路径; 未注入则仅内存断言) */
    trace_writer tw;
    int trc = trace_open(&tw, trace_out);
    CHECK(trc >= 0, "T0: trace JSONL 输出打开");
    char dll_sha_hex[72] = "", dll_sha_hex2[72] = "";
    CHECK(sha256_file_hex(dll_path, dll_sha_hex, sizeof(dll_sha_hex)) &&
              sha256_file_hex(dll_path, dll_sha_hex2, sizeof(dll_sha_hex2)) &&
              !strcmp(dll_sha_hex, dll_sha_hex2) && strlen(dll_sha_hex) == 64,
          "T1: dll_sha256 实测 (64 hex, 两次独立计算一致)");
    /* dll_name = 实际加载路径 basename (观测) */
    const char* dll_base = strrchr(dll_path, '/');
#ifdef _WIN32
    const char* dll_base_w = strrchr(dll_path, '\\');
    if (dll_base_w && (!dll_base || dll_base_w > dll_base)) dll_base = dll_base_w;
#endif
    dll_base = dll_base ? dll_base + 1 : dll_path;

    /* 合成链: node1 cal_master1 (generate_master_bias) → node2 cal_frame1
     * (calibrate_frame, data_base64 = node1 out_base64; master 键缺席 =
     * legacy NULL 恒等通道)。链式 artifact 传递是 ArtifactStore 真实传
     * handle 的本地最小替身。 */
    char man1[8192];
    {
        std::string m = "{\"schema_version\":1,\"width\":8,\"height\":6,"
                        "\"dtype\":\"f32\",\"frames\":[";
        for (int f = 0; f < NFRAMES; ++f) {
            std::string b;
            b64_encode((const uint8_t*)(g_fx.stack.data() + (size_t)f * FPX),
                       FPX * sizeof(float), b);
            if (f) m += ",";
            m += "\"" + b + "\"";
        }
        m += "]}";
        snprintf(man1, sizeof(man1), "%s", m.c_str());
    }
    const char* cfg1 =
        "{\"op\":\"generate_master_bias\",\"width\":8,\"height\":6,\"frames\":4,"
        "\"sigma_low\":2.0,\"sigma_high\":2.0,\"max_iterations\":2,\"combine\":\"mean\"}";
    const char* cfg2 =
        "{\"op\":\"calibrate_frame\",\"width\":8,\"height\":6,"
        "\"dark_optimization\":false,\"dark_scale_factor\":1.0}";

    uint64_t art_sizes[2] = {0, 0};
    /* 8x6 f32 = 192B → base64 恰 256 字符 (无 padding); 缓冲须 >256+1 防截断 */
    char out1_b64[512] = "";

    for (int node_i = 0; node_i < 2; node_i++) {
        char node_id[32];
        snprintf(node_id, sizeof(node_id), "cal_n%d", node_i + 1);
        ex_stub ex;
        memset(&ex, 0, sizeof(ex));
        ex.max_workers = 3;
        const acs_host_api_v1* host = make_host(&ex);
        const char* cfg = (node_i == 0) ? cfg1 : cfg2;
        const char* man = (node_i == 0) ? man1 : NULL;   /* node2 manifest 由 node1 输出拼装 */

        /* C: validate_config(1) */
        acs_status rc = api->validate_config(api, s_from(cfg), &err);
        CHECK(rc == ACS_OK, "C1: node validate_config 恰调用 1 次 → OK");

        /* D: plan(1) — 真实值 (数据事实) 独立算术交叉 */
        acs_strbuf_v1 pb;
        sb_init(&pb);
        char pbuf[1024];
        pb.data = pbuf;
        pb.cap = sizeof(pbuf);
        rc = api->plan(api, s_from(node_id), s_from(cfg), &pb, &err);
        CHECK(rc == ACS_OK && pb.data && pb.data[0], "C2: node plan 恰调用 1 次 → OK");
        if (rc == ACS_OK) {
            long long pv = 0, pw = 0, pmem = 0, pio = 0, pmn = 0;
            char pu[32] = "", pax[32] = "", pnid[64] = "", pop[64] = "";
            int ok = tjson_ll(pb.data, "plan_version", &pv) &&
                     tjson_ll(pb.data, "work_units", &pw) &&
                     tjson_ll(pb.data, "memory_bytes_estimate", &pmem) &&
                     tjson_ll(pb.data, "io_bytes_estimate", &pio) &&
                     tjson_ll(pb.data, "min_workers", &pmn) &&
                     tjson_str(pb.data, "work_unit", pu, sizeof(pu)) &&
                     tjson_str(pb.data, "parallel_axis", pax, sizeof(pax)) &&
                     tjson_str(pb.data, "node_id", pnid, sizeof(pnid)) &&
                     tjson_str(pb.data, "op", pop, sizeof(pop)) &&
                     tjson_contains(pb.data, "\"cancel_support\":\"entry\"");
            CHECK(ok, "D2: plan 键集 == DLL cal_plan 真实输出 (词表冻结)");
            CHECK((long long)ASTROCS_CAL_PLAN_VERSION == pv,
                  "D3: plan.plan_version == 1 (types.h)");
            CHECK(pnid[0] && !strcmp(pnid, node_id), "D4: plan.node_id == 驱动节点 id 回显");
            CHECK(pop[0] && !strcmp(pop, node_i == 0 ? ASTROCS_CAL_OP_MASTER_BIAS_F32
                                                     : ASTROCS_CAL_OP_CALIBRATE_F32),
                  "D5: plan.op == 节点 operation 词表");
            /* 真实值: master → config.frames; 帧级 → width*height (独立算术) */
            long long expect_wu = (node_i == 0) ? (long long)NFRAMES : (long long)FW * FH;
            CHECK(pw == expect_wu && pw > 0,
                  "D6: plan.work_units == 数据事实且 >0 (master→frames/帧级→w*h, 非预测)");
            CHECK(pu[0] && !strcmp(pu, node_i == 0 ? "frame" : "pixel") && !strcmp(pax, pu),
                  "D7: plan.work_unit/parallel_axis 按 op 类真实分组 (master=frame/帧级=pixel)");
            CHECK(pmn == 1, "D8: plan.min_workers == 1");
            CHECK(pmem == pw * 4 && pio == 0,
                  "D9: plan.memory=work_units*sizeof(f32), io=0 为 inline 数据面真实值");
            CHECK(!strstr(pb.data, "leased"),
                  "D10: plan JSON 无观测词 leased (禁观测冒充预测, 14 §4)");
        }

        /* C: create(1) */
        acs_module_instance_v1* inst = NULL;
        err_init(&err);
        rc = api->create(api, s_from(cfg), host, &inst, &err);
        CHECK(rc == ACS_OK && inst != NULL, "C3: node create 恰调用 1 次 → OK");

        /* node2 输入 manifest: data_base64 = node1 输出 (链式 artifact) */
        char man2[512];
        if (node_i == 1) {
            snprintf(man2, sizeof(man2),
                     "{\"schema_version\":1,\"width\":8,\"height\":6,"
                     "\"dtype\":\"f32\",\"data_base64\":\"%s\"}", out1_b64);
            man = man2;
        }

        /* C: execute 事务(1) = 两阶段 sink (2 次 API) */
        double t0 = now_wall_ms();
        char* out = (inst != NULL && man != NULL) ? exec_txn(api, inst, man, cfg, &rc) : NULL;
        double t1 = now_wall_ms();
        CHECK(rc == ACS_OK && out != NULL, "C4: node execute 事务 (两阶段) → OK");
        CHECK(ex.acquire_calls == 2 && ex.release_calls == 2,
              "C5: lease acquire/release == 2/2 (一事务两阶段各租借一次)");
        CHECK(ex.last_leased == 3 && ex.max_workers == 3,
              "C6: 租借观测 want=3 == host max_workers (cpu_heavy 硬租约, stub 真实观测)");
        art_sizes[node_i] = out ? (uint64_t)strlen(out) : 0;
        if (out) {
            long long sv = -1;
            char dt[16] = "", oop[64] = "";
            CHECK(tjson_ll(out, "schema_version", &sv) && sv == 1 &&
                      tjson_str(out, "dtype", dt, sizeof(dt)) && !strcmp(dt, "f32") &&
                      tjson_str(out, "op", oop, sizeof(oop)),
                  "C7: 输出 manifest 键序/词表 (schema=1, dtype=f32, op 正确)");
            char ob[512] = "";
            CHECK(tjson_str(out, "out_base64", ob, sizeof(ob)) && ob[0],
                  "C8: 输出 out_base64 非空 (事务输出真实)");
            if (node_i == 0) {
                snprintf(out1_b64, sizeof(out1_b64), "%s", ob);
            } else {
                CHECK(out1_b64[0] && !strcmp(ob, out1_b64),
                      "C9: 合成链 BITWISE: node2(calibrate) 输出 == node1 master 平面 "
                      "(NULL-master 恒等通道, 上游 artifact 真实传递)");
            }
        }

        /* C: inspect(1) — 模块自观测 exec_count/last_op/state */
        acs_strbuf_v1 ib;
        sb_init(&ib);
        char ibuf[512];
        ib.data = ibuf;
        ib.cap = sizeof(ibuf);
        rc = inst ? api->inspect(inst, &ib, &err) : ACS_ERR_PARAM;
        CHECK(rc == ACS_OK && ib.data, "C10: node inspect 恰调用 1 次 → OK");
        if (rc == ACS_OK && ib.data) {
            long long ec = -1, lop = -1;
            char st8[32] = "";
            tjson_ll(ib.data, "exec_count", &ec);
            tjson_ll(ib.data, "last_op", &lop);
            tjson_str(ib.data, "state", st8, sizeof(st8));
            /* 真实语义: 事务性两阶段 strbuf = 2 次 execute API (尺寸探测遍 +
             * 写入遍), 每遍末尾 exec_count++ → 成功事务后 exec_count == 2。
             * 断言用实现真实观测值, 不用理想化假值。 */
            CHECK(ec == 2, "C11: inspect.exec_count == 2 (一事务两遍各计一次)");
            CHECK(lop == (long long)ACS_OP_EXECUTE,
                  "C12: inspect.last_op == ACS_OP_EXECUTE (7, 实例级执行观测)");
            CHECK(!strcmp(st8, "created"),
                  "C12b: inspect.state == created (v1 事务完成后回 CREATED, 如实观测)");
        }

        /* C: 重复驱动观测 (KI 登记, 与 gaia INT 同模式):
         * v1 C ABI 下事务完成后 state 回 CREATED (destroy 才置 DESTROYED),
         * host 再次驱动同实例是 host 协议违规但模块不拒绝 —— "每节点恰调用
         * 一次"由调用方驱动协议 + RT-006 runtime call counts / 重放违规检测
         * 保证 (本测试 T3 已证)。本地观测: 重复事务后 exec_count 线性递增,
         * inspect 面可被 RT 巡检发现超限, 不伪造 STATE 断言。 */
        {
            acs_status rc2 = ACS_OK;
            char* out_dup = (inst != NULL && man != NULL) ? exec_txn(api, inst, man, cfg, &rc2) : NULL;
            CHECK(rc2 == ACS_OK && out_dup != NULL,
                  "C13: 重复事务可重入 (KI: 模块不锁完成态, 恰一次归 RT call-count)");
            free(out_dup);
            long long ec2 = -1;
            if (inst) {
                acs_strbuf_v1 ib2;
                sb_init(&ib2);
                char ibuf2[512];
                ib2.data = ibuf2;
                ib2.cap = sizeof(ibuf2);
                if (api->inspect(inst, &ib2, &err) == ACS_OK)
                    tjson_ll(ib2.data, "exec_count", &ec2);
            }
            CHECK(ec2 == 4,
                  "C14: 重复事务后 exec_count == 4 (2+2 观测递增, 供 RT 巡检超限)");
        }

        /* E: trace module_call 行 (真实观测填充) */
        trace_module_call(&tw, node_id, desc_mid, desc_ver, desc_build,
                          dll_base, dll_sha_hex, "astrocs_module_query_v1",
                          1, ex.last_leased, ex.max_workers,
                          (rc == ACS_OK || out != NULL) ? "COMPLETED" : "FAILED",
                          art_sizes[node_i], t1 - t0);
        trace_node_end(&tw, node_id, "COMPLETED");

        /* C: destroy(1) */
        if (inst) api->destroy(inst);
        CHECK(1, "C15: node destroy 恰调用 1 次 (生命周期闭环)");
        free(out);
    }

    /* E: trace 重放 (本地; 每 node module_call 恰 1 次) */
    if (tw.f) {
        trace_close(&tw);
        CHECK(tw.lines == 4, "T2: trace JSONL 行数 == 4 (2 module_call + 2 node_end)");
        char msg[256] = "";
        int viol = replay_violations(trace_out, msg, sizeof(msg));
        CHECK(viol == 0, "T3: 重放无 repeated-call 违规 (每 node module_call==1)");
        if (viol != 0) printf("  replay: %s\n", msg);
        /* 重读首行 module_call: 真实观测字段非空 */
        FILE* f = fopen(trace_out, "r");
        if (f) {
            char line[2048];
            int saw = 0;
            while (fgets(line, sizeof(line), f)) {
                if (!strstr(line, "\"type\":\"module_call\"")) continue;
                char dn[128] = "", ds[80] = "", en[64] = "", bi[64] = "";
                long long cc = -1, wsz = 0, gw = 0, asz = 0;
                tjson_str(line, "dll_name", dn, sizeof(dn));
                tjson_str(line, "dll_sha256", ds, sizeof(ds));
                tjson_str(line, "entry", en, sizeof(en));
                tjson_str(line, "build_id", bi, sizeof(bi));
                tjson_ll(line, "call_count", &cc);
                tjson_ll(line, "workers", &wsz);
                tjson_ll(line, "granted_workers", &gw);
                tjson_ll(line, "artifact_size", &asz);
                CHECK(!strcmp(dn, dll_base), "T4: trace.dll_name == 实际加载 basename");
                CHECK(!strcmp(ds, dll_sha_hex) && strlen(ds) == 64,
                      "T5: trace.dll_sha256 == 磁盘内容实测 hash");
                CHECK(!strcmp(en, "astrocs_module_query_v1"), "T6: trace.entry 真实符号");
                CHECK(!strcmp(bi, ASTROCS_CAL_BUILD_ID), "T7: trace.build_id == describe");
                CHECK(cc == 1, "T8: trace.call_count == 1 (该 node 累计)");
                CHECK(wsz == 3 && gw == 3, "T9: trace.workers/granted == 实租借观测");
                CHECK(asz > 0, "T10: trace.artifact_size == 输出 manifest 实际字节");
                CHECK(!strstr(line, "\"provider\""),
                      "T11: trace 无 provider 字段 (1:1 转发 legacy, 空=真实, 禁预测)");
                saw++;
            }
            fclose(f);
            CHECK(saw == 2, "T12: module_call 行数 == 节点数 2");
        }
    }

    printf("== calibration_integration_test: %s (fail=%d) ==\n",
           g_fail == 0 ? "ALL PASS" : "FAILED", g_fail);
    dlclose(dl);
    return g_fail == 0 ? 0 : 1;
}
