/* AstroCS 动态模块 Registry — 真实实现 (ABI-004)
 *
 * 文件: runtime/registry/module_registry.c
 * 实现: 12_DLL_ABI_AND_LOADER_STANDARD.md §5/§6 + 合同头 module_registry.h +
 *       ABI-003 secure_loader 编排。
 *
 * 分层(host 侧工具库; 内部临时内存用 libc; 句柄/输出内存经 options.allocator):
 *   1. product manifest 最小 JSON 提取(仅 units[] 形状; 无第三方依赖);
 *   2. module.yaml 最小顶层 key 提取(module_id/module_version/abi_version);
 *   3. ABI-003 loader 编排(query+describe; 加载失败记 finding 不中止);
 *   4. 检测: 重复 unit_id/module_id、版本冲突、未登记 DLL、descriptor/hash 不一致。
 *
 * 安全: 全部 abs = manifest 目录 + rel_path; canonical + (可选)allowed_root 校验;
 * sha256 现场计算(FIPS 180-4 内部, 与 loader 一致); 日志纪律同 loader
 * (错误/发现消息静态字面量; 不写文件)。
 *
 * 存储: entry 字段全部定长 char 数组内联(避免 allocator 字符串生命周期),
 * entries/findings 为 allocator 动态数组; close 一次释放。定长上限由结构保证。
 */
#define _XOPEN_SOURCE 700

#include "module_registry.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <dirent.h>
#include <sys/stat.h>
#endif

/* ═══════════════════════════ 定长字段上限 ═══════════════════════════ */
#define REG_SZ_UNIT_ID 64
#define REG_SZ_KIND 24
#define REG_SZ_REL 1024
#define REG_SZ_ABS 4096
#define REG_SZ_MID 192
#define REG_SZ_VER 96
#define REG_SZ_BID 96
#define REG_SZ_SHA 65
#define REG_SZ_STATUS 24
#define REG_SZ_JSON_LINE 2048

/* ═══════════════════════════ 内部 FIPS 180-4 SHA-256 ═══════════════════════════ */

typedef struct rsha_ctx_s {
    uint32_t h[8];
    uint64_t total;
    uint8_t block[64];
    uint32_t blen;
} rsha_ctx;

static const uint32_t rK[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,
    0x923f82a4u,0xab1c5ed5u,0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,
    0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,0xe49b69c1u,0xefbe4786u,
    0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,
    0x06ca6351u,0x14292967u,0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,
    0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,0xa2bfe8a1u,0xa81a664bu,
    0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,
    0x5b9cca4fu,0x682e6ff3u,0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,
    0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static uint32_t rrot(uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }

static void rsha_block(rsha_ctx* c, const uint8_t* p) {
    uint32_t w[64]; unsigned i;
    for (i = 0; i < 16; ++i)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (i = 16; i < 64; ++i) {
        uint32_t s0 = rrot(w[i-15],7) ^ rrot(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rrot(w[i-2],17) ^ rrot(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
    for (i = 0; i < 64; ++i) {
        uint32_t s1 = rrot(e,6) ^ rrot(e,11) ^ rrot(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + ch + rK[i] + w[i];
        uint32_t s0 = rrot(a,2) ^ rrot(a,13) ^ rrot(a,22);
        uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = s0 + mj;
        h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;
}

static void rsha_init(rsha_ctx* c) {
    c->h[0]=0x6a09e667u;c->h[1]=0xbb67ae85u;c->h[2]=0x3c6ef372u;c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu;c->h[5]=0x9b05688cu;c->h[6]=0x1f83d9abu;c->h[7]=0x5be0cd19u;
    c->total=0; c->blen=0;
}

static void rsha_update(rsha_ctx* c, const void* data, uint64_t len) {
    const uint8_t* p = (const uint8_t*)data;
    c->total += len;
    if (c->blen > 0) {
        uint32_t need = 64u - c->blen;
        if ((uint64_t)need > len) need = (uint32_t)len;
        memcpy(c->block + c->blen, p, need);
        c->blen += need; p += need; len -= need;
        if (c->blen == 64u) { rsha_block(c, c->block); c->blen = 0; }
    }
    while (len >= 64u) { rsha_block(c, p); p += 64; len -= 64; }
    if (len > 0) { memcpy(c->block, p, (size_t)len); c->blen = (uint32_t)len; }
}

static void rsha_final(rsha_ctx* c, uint8_t out[32]) {
    uint64_t bits = c->total * 8u;
    uint8_t pad = 0x80u;
    rsha_update(c, &pad, 1);
    uint8_t z = 0u;
    while (c->blen != 56u) rsha_update(c, &z, 1);
    uint8_t lb[8]; unsigned i;
    for (i = 0; i < 8; ++i) lb[i] = (uint8_t)(bits >> (56u - i*8u));
    rsha_update(c, lb, 8);
    for (i = 0; i < 8; ++i) {
        out[i*4]=(uint8_t)(c->h[i]>>24); out[i*4+1]=(uint8_t)(c->h[i]>>16);
        out[i*4+2]=(uint8_t)(c->h[i]>>8); out[i*4+3]=(uint8_t)(c->h[i]);
    }
}

static void rsha_hex(const uint8_t d[32], char out[65]) {
    static const char hx[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 32; ++i) { out[i*2]=hx[d[i]>>4]; out[i*2+1]=hx[d[i]&0xf]; }
    out[64] = '\0';
}

/* 内存 buffer sha256 → hex(供自检与文件比对) */
static void sha256_buf_hex(const void* data, uint64_t len, char out[65]) {
    rsha_ctx c;
    uint8_t d[32];
    rsha_init(&c);
    rsha_update(&c, data, len);
    rsha_final(&c, d);
    rsha_hex(d, out);
}

/* 文件 sha256 → hex; 0=成功 */
static int file_sha256_hex(const char* path, char out[65]) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    rsha_ctx c;
    rsha_init(&c);
    uint8_t tmp[8192];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) rsha_update(&c, tmp, n);
    int bad = ferror(f);
    fclose(f);
    if (bad) return -1;
    uint8_t d[32];
    rsha_final(&c, d);
    rsha_hex(d, out);
    return 0;
}

/* ═══════════════════════════ 最小 JSON(仅 units[] 形状) ═══════════════════════════ */

typedef struct json_p_s { const char* p; const char* end; int error; } json_p;

static void jp_ws(json_p* j) {
    while (j->p < j->end && (*j->p==' '||*j->p=='\t'||*j->p=='\n'||*j->p=='\r')) j->p++;
}
static int jp_peek(json_p* j, char c) { jp_ws(j); return j->p < j->end && *j->p == c; }
static void jp_expect(json_p* j, char c) {
    jp_ws(j);
    if (j->p >= j->end || *j->p != c) { j->error = 1; return; }
    j->p++;
}
/* 解析字符串到 dst(容量 cap); 返回长度; 转义只支持 \" \\ \/ \n \t \r */
static size_t jp_str(json_p* j, char* dst, size_t cap) {
    jp_ws(j);
    if (j->p >= j->end || *j->p != '"') { j->error = 1; return 0; }
    j->p++;
    size_t n = 0;
    int done = 0;
    while (j->p < j->end) {
        unsigned char c = (unsigned char)*j->p;
        if (c == '"') { j->p++; done = 1; break; }
        if (c == '\\') {
            j->p++;
            if (j->p >= j->end) { j->error = 1; break; }
            char e = *j->p++;
            char out = e;
            if (e == 'n') out = '\n';
            else if (e == 't') out = '\t';
            else if (e == 'r') out = '\r';
            else if (e == 'u') { j->error = 1; break; }
            if (n + 1 < cap) dst[n++] = out;
            continue;
        }
        if (c < 0x20) { j->error = 1; break; }
        j->p++;   /* 普通字符: 前进 */
        if (n + 1 < cap) dst[n++] = (char)c;
    }
    if (cap > 0) dst[n < cap ? n : cap - 1] = '\0';   /* 恒 NUL 结尾 */
    if (!done && !j->error) j->error = 1;
    return n;
}
static void jp_skip_value(json_p* j) {
    jp_ws(j);
    if (j->p >= j->end) { j->error = 1; return; }
    if (*j->p == '"') { char b[64]; (void)jp_str(j, b, sizeof(b)); return; }
    if (*j->p == '{' || *j->p == '[') {
        char open = *j->p, close = (open=='{') ? '}' : ']';
        int depth = 0;
        while (j->p < j->end) {
            char d = *j->p++;
            if (d == '"') { char b[64]; (void)jp_str(j, b, sizeof(b)); continue; }
            if (d == open) depth++;
            else if (d == close) { depth--; if (depth == 0) return; }
        }
        j->error = 1;
        return;
    }
    while (j->p < j->end && *j->p != ',' && *j->p != '}' && *j->p != ']') j->p++;
}


/* 截断拷贝: dst 保证 NUL 结尾 */
static void tc(char* dst, size_t dst_sz, const char* src) {
    if (!dst || dst_sz == 0) return;
    if (!src) { dst[0] = 0; return; }
    size_t n = strlen(src);
    if (n >= dst_sz) n = dst_sz - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}
/* ═══════════════════════════ manifest unit 中间结构 ═══════════════════════════ */

typedef struct mu_s {
    char unit_id[REG_SZ_UNIT_ID];
    char kind[REG_SZ_KIND];
    char rel_path[REG_SZ_REL];
    char module_id[REG_SZ_MID];
    char sha[REG_SZ_SHA];
    char status[REG_SZ_STATUS];
    uint32_t abi;
} mu;

/* 解析 units[] → mu 数组(动态; 调用方 free)。返回 unit 数; -1 = JSON/schema 错
 * (err_detail 填细分码)。kind 白名单: exe/runtime/io/module/provider。 */
static int parse_units(const char* js, size_t jsz, mu** out, int* err_detail) {
    *out = NULL;
    *err_detail = ACS_REG_EC_MANIFEST_JSON;
    json_p j;
    j.p = js; j.end = js + jsz; j.error = 0;
    int found_units = 0;
    size_t cap = 0, n = 0;
    mu* arr = NULL;
    jp_ws(&j);
    jp_expect(&j, '{');
    if (j.error) return -1;
    while (!j.error && j.p < j.end) {
        jp_ws(&j);
        if (jp_peek(&j, '}')) { j.p++; break; }
        char key[64];
        (void)jp_str(&j, key, sizeof(key));
        if (j.error) return -1;
        jp_expect(&j, ':');
        if (j.error) return -1;
        if (strcmp(key, "units") == 0) {
            found_units = 1;
            jp_ws(&j);
            if (j.p >= j.end || *j.p != '[') { *err_detail = ACS_REG_EC_MANIFEST_JSON; goto fail; }
            j.p++;
            while (!j.error && j.p < j.end) {
                jp_ws(&j);
                if (jp_peek(&j, ']')) { j.p++; break; }
                if (!jp_peek(&j, '{')) { j.error = 1; break; }
                j.p++;
                mu u;
                memset(&u, 0, sizeof(u));
                while (!j.error && j.p < j.end) {
                    jp_ws(&j);
                    if (j.p < j.end && *j.p == ',') j.p++;   /* 字段分隔 */
                    jp_ws(&j);
                    if (jp_peek(&j, '}')) { j.p++; break; }
                    char fk[32];
                    (void)jp_str(&j, fk, sizeof(fk));
                    jp_expect(&j, ':');
                    if (j.error) break;
                    jp_ws(&j);
                    if (j.p < j.end && *j.p == '"') {
                        char bv[REG_SZ_REL + 64];
                        size_t bn = jp_str(&j, bv, sizeof(bv));
                        (void)bn;
                        if (strcmp(fk,"unit_id")==0) tc(u.unit_id, sizeof(u.unit_id), bv);
                        else if (strcmp(fk,"kind")==0) tc(u.kind, sizeof(u.kind), bv);
                        else if (strcmp(fk,"rel_path")==0) tc(u.rel_path, sizeof(u.rel_path), bv);
                        else if (strcmp(fk,"module_id")==0) tc(u.module_id, sizeof(u.module_id), bv);
                        else if (strcmp(fk,"sha256")==0) tc(u.sha, sizeof(u.sha), bv);
                        else if (strcmp(fk,"status")==0) tc(u.status, sizeof(u.status), bv);
                        else if (strcmp(fk,"abi_version")==0) { char ab[16]; tc(ab, sizeof(ab), bv); u.abi = (uint32_t)strtoul(ab, NULL, 10); }
                    } else {
                        /* null/数字/布尔 */
                        if (j.p < j.end && j.p[0]=='n' && j.p[1]=='u' && j.p[2]=='l' && j.p[3]=='l') {
                            j.p += 4;   /* null: 字段保持空 */
                        } else if (j.p < j.end && *j.p == '{') {
                            jp_skip_value(&j);
                        } else {
                            char tok[64]; size_t ti = 0;
                            while (j.p < j.end && *j.p != ',' && *j.p != '}' && *j.p != ']' && ti+1 < sizeof(tok)) tok[ti++] = *j.p++;
                            tok[ti] = 0;
                            if (strcmp(fk,"abi_version")==0) u.abi = (uint32_t)strtoul(tok, NULL, 10);
                        }
                    }
                }
                /* schema 语义检测 */
                int kind_ok = strcmp(u.kind,"exe")==0 || strcmp(u.kind,"runtime")==0 ||
                              strcmp(u.kind,"io")==0 || strcmp(u.kind,"module")==0 ||
                              strcmp(u.kind,"provider")==0;
                if (!kind_ok) { *err_detail = ACS_REG_EC_MANIFEST_SCHEMA; goto fail; }
                int is_mod = strcmp(u.kind,"module")==0;
                if (is_mod && u.module_id[0] == 0) { *err_detail = ACS_REG_EC_MANIFEST_SCHEMA; goto fail; }
                if (u.sha[0]) {
                    size_t sl = strlen(u.sha);
                    int hex_ok = sl == 64;
                    if (hex_ok) { size_t k; for (k = 0; k < sl; ++k) if (!isxdigit((unsigned char)u.sha[k])) { hex_ok = 0; break; } }
                    if (!hex_ok) { *err_detail = ACS_REG_EC_MANIFEST_SCHEMA; goto fail; }
                }
                if (n == cap) {
                    cap = cap ? cap * 2 : 8;
                    mu* na = (mu*)realloc(arr, cap * sizeof(mu));
                    if (!na) { free(arr); *err_detail = ACS_REG_EC_INTERNAL; return -1; }
                    arr = na;
                }
                arr[n++] = u;
                if (!j.error && j.p < j.end && *j.p == ',') j.p++;
            }
        } else {
            jp_skip_value(&j);
        }
        if (!j.error && j.p < j.end && *j.p == ',') j.p++;
    }
    if (!found_units || j.error) { *err_detail = ACS_REG_EC_MANIFEST_JSON; goto fail; }
    *err_detail = ACS_REG_EC_NONE;   /* 成功路径清除初值(selftest 断言 ed==0) */
    *out = arr;
    return (int)n;
fail:
    free(arr);
    return -1;
}

/* ═══════════════════════════ 静态字面量消息 ═══════════════════════════ */

static const char* reg_msg(int32_t detail) {
    switch (detail) {
        case ACS_REG_EC_INVALID_INPUT:       return "registry: invalid input";
        case ACS_REG_EC_MANIFEST_READ:       return "registry: manifest unreadable";
        case ACS_REG_EC_MANIFEST_JSON:       return "registry: manifest JSON malformed";
        case ACS_REG_EC_MANIFEST_SCHEMA:     return "registry: manifest schema violation";
        case ACS_REG_EC_PATH_NOT_ABS:        return "registry: resolved path not absolute";
        case ACS_REG_EC_PATH_NOT_CANONICAL:  return "registry: resolved path not canonical";
        case ACS_REG_EC_DUP_UNIT_ID:         return "registry: duplicate unit id";
        case ACS_REG_EC_DUP_MODULE_ID:       return "registry: duplicate module id";
        case ACS_REG_EC_VERSION_CONFLICT:    return "registry: version conflict";
        case ACS_REG_EC_UNREGISTERED_DLL:    return "registry: unregistered module file";
        case ACS_REG_EC_MODULE_ID_MISMATCH:  return "registry: module id mismatch";
        case ACS_REG_EC_HASH_MISMATCH:       return "registry: hash mismatch";
        case ACS_REG_EC_LOADER:              return "registry: loader rejected entry";
        case ACS_REG_EC_YAML_INCONSISTENT:   return "registry: module.yaml inconsistent";
        default:                             return "registry: internal error";
    }
}

static void fill_err(acs_error_info_v1* err, acs_status st, int32_t detail) {
    if (!err) return;
    memset(err, 0, sizeof(*err));
    err->head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    err->head.abi_version = ACS_ABI_VERSION_V1;
    err->status = st;
    err->domain = ACS_ERR_DOMAIN_CONFIG;
    err->detail_code = (uint32_t)detail;
    const char* m = reg_msg(detail);
    err->message_utf8 = m;
    err->message_bytes = (uint32_t)strlen(m);
}

/* ═══════════════════════════ 句柄存储(定长 entry 内联) ═══════════════════════════ */

typedef struct entry_s {
    char unit_id[REG_SZ_UNIT_ID];
    char kind[REG_SZ_KIND];
    char rel_path[REG_SZ_REL];
    char abs_path[REG_SZ_ABS];
    char module_id[REG_SZ_MID];      /* manifest */
    char module_id_dll[REG_SZ_MID];
    char module_id_yaml[REG_SZ_MID];
    char version_dll[REG_SZ_VER];
    char version_yaml[REG_SZ_VER];
    char build_id_dll[REG_SZ_BID];
    char sha_registered[REG_SZ_SHA];
    char sha_actual[REG_SZ_SHA];
    char status[REG_SZ_STATUS];
    uint32_t abi_version;
    uint32_t loaded;
    int32_t finding_mask;
    uint32_t detail_code;
} entry_s;

typedef struct finding_s {
    uint32_t kind;
    int32_t entry_index;
    int32_t aux_index;
} finding_s;

struct acs_registry_s {
    acs_allocator_v1 allocator;
    entry_s* entries;
    uint32_t entry_count;
    finding_s* findings;
    uint32_t finding_count;
};

static acs_str_v1 sp(const char* p) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = p;
    v.size = p ? (uint64_t)strlen(p) : 0;
    return v;
}

/* ═══════════════════════════ module.yaml 最小提取 ═══════════════════════════ */

typedef struct yaml_s {
    char module_id[REG_SZ_MID];
    char module_version[REG_SZ_VER];
    char abi[16];
    int has_mid, has_ver, has_abi;
} yaml_s;

static void yaml_load(const char* path, yaml_s* y) {
    memset(y, 0, sizeof(*y));
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1]=='\n'||line[n-1]=='\r')) line[--n] = 0;
        char* s = line;
        while (*s==' '||*s=='\t') s++;
        if (!*s || *s=='#') continue;
        char* colon = strchr(s, ':');
        if (!colon) continue;
        *colon = 0;
        char* key = s;
        char* ke = colon - 1;
        while (ke >= key && (*ke==' '||*ke=='\t')) *ke-- = 0;
        char* val = colon + 1;
        while (*val==' '||*val=='\t') val++;
        size_t vl = strlen(val);
        while (vl > 0 && val[vl-1]=='\r') val[--vl] = 0;
        if (*val=='"'||*val=='\'') { val++; vl = strlen(val); if (vl>0 && (val[vl-1]=='"'||val[vl-1]=='\'')) val[vl-1]=0; }
        if (strcmp(key,"module_id")==0) { snprintf(y->module_id, sizeof(y->module_id), "%s", val); y->has_mid = 1; }
        else if (strcmp(key,"module_version")==0) { snprintf(y->module_version, sizeof(y->module_version), "%s", val); y->has_ver = 1; }
        else if (strcmp(key,"abi_version")==0) { snprintf(y->abi, sizeof(y->abi), "%s", val); y->has_abi = 1; }
    }
    fclose(f);
}

/* ═══════════════════════════ loader 编排 ═══════════════════════════ */

static acs_status load_dll(const acs_allocator_v1* alloc, const char* kind,
                           const char* abs, const char* exp_mid, const char* exp_sha,
                           const char* exp_bid, uint32_t abi,
                           char* out_mid, size_t out_mid_sz,
                           char* out_ver, size_t out_ver_sz,
                           char* out_bid, size_t out_bid_sz,
                           char* out_sha, acs_error_info_v1* err) {
    out_mid[0]=0; out_ver[0]=0; out_bid[0]=0; out_sha[0]=0;
    acs_load_manifest_unit_v1 unit;
    memset(&unit, 0, sizeof(unit));
    unit.head.struct_size = (uint32_t)sizeof(acs_load_manifest_unit_v1);
    unit.head.abi_version = ACS_ABI_VERSION_V1;
    unit.unit_id = sp("REG-ENTRY");
    unit.kind = sp(kind);
    unit.abs_path_utf8 = sp(abs);
    unit.module_id = sp(exp_mid && *exp_mid ? exp_mid : "");
    unit.expected_sha256 = sp(exp_sha && *exp_sha ? exp_sha : "");
    unit.expected_build_id = sp(exp_bid && *exp_bid ? exp_bid : "");
    unit.abi_version = abi;
    acs_loader_options_v1 opt;
    memset(&opt, 0, sizeof(opt));
    opt.head.struct_size = (uint32_t)sizeof(acs_loader_options_v1);
    opt.head.abi_version = ACS_ABI_VERSION_V1;
    opt.unit = unit;
    opt.allocator = alloc;
    opt.allowed_root_utf8 = sp("");

    acs_loader_handle* h = NULL;
    acs_status st = acs_secure_loader_load_v1(&opt, err, &h);
    if (st != ACS_OK) return st;
    acs_loaded_module_v1 info;
    memset(&info, 0, sizeof(info));
    info.head.struct_size = (uint32_t)sizeof(acs_loaded_module_v1);
    info.head.abi_version = ACS_ABI_VERSION_V1;
    acs_status ds = acs_secure_loader_describe_v1(h, &info);
    if (ds == ACS_OK) {
        if (info.module_id.data && info.module_id.size > 0) {
            size_t n = info.module_id.size < out_mid_sz-1 ? (size_t)info.module_id.size : out_mid_sz-1;
            memcpy(out_mid, info.module_id.data, n); out_mid[n]=0;
        }
        if (info.module_version.data && info.module_version.size > 0) {
            size_t n = info.module_version.size < out_ver_sz-1 ? (size_t)info.module_version.size : out_ver_sz-1;
            memcpy(out_ver, info.module_version.data, n); out_ver[n]=0;
        }
        if (info.build_id.data && info.build_id.size > 0) {
            size_t n = info.build_id.size < out_bid_sz-1 ? (size_t)info.build_id.size : out_bid_sz-1;
            memcpy(out_bid, info.build_id.data, n); out_bid[n]=0;
        }
        if (info.loaded_sha256.data && info.loaded_sha256.size == 64) {
            memcpy(out_sha, info.loaded_sha256.data, 64); out_sha[64]=0;
        }
    }
    acs_secure_loader_release_v1(h);
    return ACS_OK;
}

/* ═══════════════════════════ 公开 API ═══════════════════════════ */

static acs_status grow_entries(acs_registry* r) {
    entry_s* ne = (entry_s*)r->allocator.alloc(
        r->allocator.user_data, (uint64_t)(r->entry_count+1) * sizeof(entry_s), 8u);
    if (!ne) return ACS_ERR_NOMEM;
    if (r->entries) {
        memcpy(ne, r->entries, (size_t)r->entry_count * sizeof(entry_s));
        r->allocator.free(r->allocator.user_data, r->entries);
    }
    r->entries = ne;
    return ACS_OK;
}

static acs_status grow_findings(acs_registry* r, uint32_t kind, int32_t idx, int32_t aux) {
    finding_s* nf = (finding_s*)r->allocator.alloc(
        r->allocator.user_data, (uint64_t)(r->finding_count+1) * sizeof(finding_s), 8u);
    if (!nf) return ACS_ERR_NOMEM;
    if (r->findings) {
        memcpy(nf, r->findings, (size_t)r->finding_count * sizeof(finding_s));
        r->allocator.free(r->allocator.user_data, r->findings);
    }
    r->findings = nf;
    finding_s* f = &nf[r->finding_count];
    f->kind = kind; f->entry_index = idx; f->aux_index = aux;
    r->finding_count++;
    return ACS_OK;
}

static void set_entry_str(char* dst, size_t dst_sz, const char* src) {
    tc(dst, dst_sz, src);
}

static acs_status registry_add_finding_bit(acs_registry* r, entry_s* e,
                                           uint32_t finding_kind, int32_t mask_bit) {
    e->finding_mask |= mask_bit;
    return grow_findings(r, finding_kind, (int32_t)(e - r->entries), -1);
}

ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_open_v1(const acs_registry_options_v1* opt,
                     acs_error_info_v1* err,
                     acs_registry** out) {
    if (!opt || !out || !opt->allocator) {
        fill_err(err, ACS_ERR_PARAM, ACS_REG_EC_INVALID_INPUT);
        return ACS_ERR_PARAM;
    }
    *out = NULL;
    if (opt->head.abi_version != ACS_ABI_VERSION_V1 ||
        opt->head.struct_size != sizeof(acs_registry_options_v1)) {
        fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_REG_EC_INVALID_INPUT);
        return ACS_ERR_ABI_MISMATCH;
    }
    const acs_str_v1* mp = &opt->manifest_abs_path;
    if (mp->size == 0 || mp->data == NULL || mp->data[0] != '/') {
        fill_err(err, ACS_ERR_PARAM, ACS_REG_EC_PATH_NOT_ABS);
        return ACS_ERR_PARAM;
    }
    /* manifest 绝对 canonical */
    char* mpath = (char*)malloc((size_t)mp->size + 1u);
    if (!mpath) { fill_err(err, ACS_ERR_NOMEM, ACS_REG_EC_INTERNAL); return ACS_ERR_NOMEM; }
    memcpy(mpath, mp->data, (size_t)mp->size);
    mpath[mp->size] = 0;
    char* mcanon = realpath(mpath, NULL);
    free(mpath);
    if (!mcanon) { fill_err(err, ACS_ERR_IO, ACS_REG_EC_MANIFEST_READ); return ACS_ERR_IO; }

    /* 读全文 */
    FILE* f = fopen(mcanon, "rb");
    if (!f) { free(mcanon); fill_err(err, ACS_ERR_IO, ACS_REG_EC_MANIFEST_READ); return ACS_ERR_IO; }
    long sz;
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f); free(mcanon); fill_err(err, ACS_ERR_IO, ACS_REG_EC_MANIFEST_READ); return ACS_ERR_IO;
    }
    char* js = (char*)malloc((size_t)sz + 1u);
    if (!js) { fclose(f); free(mcanon); fill_err(err, ACS_ERR_NOMEM, ACS_REG_EC_INTERNAL); return ACS_ERR_NOMEM; }
    size_t rd = fread(js, 1, (size_t)sz, f);
    if (rd != (size_t)sz) { fclose(f); free(js); free(mcanon); fill_err(err, ACS_ERR_IO, ACS_REG_EC_MANIFEST_READ); return ACS_ERR_IO; }
    js[sz] = 0;
    fclose(f);

    /* 解析 units */
    mu* mus = NULL;
    int ed = 0;
    int mu_n = parse_units(js, (size_t)sz, &mus, &ed);
    free(js);
    if (mu_n < 0) {
        free(mcanon);
        fill_err(err, (ed == ACS_REG_EC_MANIFEST_SCHEMA) ? ACS_ERR_PARAM : ACS_ERR_IO, ed);
        return (ed == ACS_REG_EC_MANIFEST_SCHEMA) ? ACS_ERR_PARAM : ACS_ERR_IO;
    }

    /* manifest 目录 */
    char* mdir = strdup(mcanon);
    if (!mdir) { free(mus); free(mcanon); fill_err(err, ACS_ERR_NOMEM, ACS_REG_EC_INTERNAL); return ACS_ERR_NOMEM; }
    char* slash = strrchr(mdir, '/');
    if (slash) *slash = 0;
    else mdir[0] = 0;

    /* 句柄 */
    acs_registry* r = (acs_registry*)opt->allocator->alloc(
        opt->allocator->user_data, sizeof(acs_registry), 8u);
    if (!r) { free(mus); free(mdir); free(mcanon); fill_err(err, ACS_ERR_NOMEM, ACS_REG_EC_INTERNAL); return ACS_ERR_NOMEM; }
    memset(r, 0, sizeof(*r));
    r->allocator = *opt->allocator;

    /* allowed_root(canonical 一次, 全局比较) */
    char* root_canon = NULL;
    if (opt->allowed_root.size > 0 && opt->allowed_root.data) {
        char* rc = (char*)malloc((size_t)opt->allowed_root.size + 1u);
        if (!rc) { free(mus); free(mdir); free(mcanon); r->allocator.free(r->allocator.user_data, r);
                   fill_err(err, ACS_ERR_NOMEM, ACS_REG_EC_INTERNAL); return ACS_ERR_NOMEM; }
        memcpy(rc, opt->allowed_root.data, (size_t)opt->allowed_root.size);
        rc[opt->allowed_root.size] = 0;
        root_canon = realpath(rc, NULL);
        free(rc);
        if (!root_canon) {
            free(mus); free(mdir); free(mcanon); r->allocator.free(r->allocator.user_data, r);
            fill_err(err, ACS_ERR_PARAM, ACS_REG_EC_PATH_NOT_CANONICAL);
            return ACS_ERR_PARAM;
        }
    }

    /* 第一遍: 重复 unit_id / module_id 检测(registry 级 finding) */
    int i, j2;
    for (i = 0; i < mu_n; ++i) {
        if (!mus[i].unit_id[0]) continue;
        for (j2 = i + 1; j2 < mu_n; ++j2) {
            if (strcmp(mus[i].unit_id, mus[j2].unit_id) == 0) {
                grow_findings(r, ACS_REG_EC_DUP_UNIT_ID, -1, -1);
            }
        }
    }
    for (i = 0; i < mu_n; ++i) {
        if (mus[i].module_id[0] == 0) continue;
        for (j2 = i + 1; j2 < mu_n; ++j2) {
            if (mus[j2].module_id[0] && strcmp(mus[i].module_id, mus[j2].module_id) == 0) {
                grow_findings(r, ACS_REG_EC_DUP_MODULE_ID, -1, -1);
            }
        }
    }

    /* 第二遍: 逐 module/provider unit 构建 entry + 检测 */
    for (i = 0; i < mu_n; ++i) {
        int is_mod = strcmp(mus[i].kind, "module") == 0;
        int is_prov = strcmp(mus[i].kind, "provider") == 0;
        if (!is_mod && !is_prov) continue;   /* exe/runtime/io 不进 registry entries */

        acs_status st2 = grow_entries(r);
        if (st2 != ACS_OK) {
            free(mus); free(mdir); free(mcanon); if (root_canon) free(root_canon);
            acs_registry_close_v1(r);
            fill_err(err, ACS_ERR_NOMEM, ACS_REG_EC_INTERNAL);
            return ACS_ERR_NOMEM;
        }
        entry_s* e = &r->entries[r->entry_count];
        memset(e, 0, sizeof(*e));
        set_entry_str(e->unit_id, sizeof(e->unit_id), mus[i].unit_id);
        set_entry_str(e->kind, sizeof(e->kind), mus[i].kind);
        set_entry_str(e->rel_path, sizeof(e->rel_path), mus[i].rel_path);
        set_entry_str(e->module_id, sizeof(e->module_id), mus[i].module_id);
        set_entry_str(e->sha_registered, sizeof(e->sha_registered), mus[i].sha);
        set_entry_str(e->status, sizeof(e->status), mus[i].status);
        e->abi_version = mus[i].abi;

        /* abs = mdir + '/' + rel_path → canonical */
        char absbuf[REG_SZ_ABS + 64];
        if (mus[i].rel_path[0] == '/') {
            snprintf(absbuf, sizeof(absbuf), "%s", mus[i].rel_path);
        } else if (mus[i].rel_path[0]) {
            snprintf(absbuf, sizeof(absbuf), "%s/%s", mdir, mus[i].rel_path);
        } else {
            snprintf(absbuf, sizeof(absbuf), "%s", mdir);
        }
        char* canon = realpath(absbuf, NULL);
        if (!canon) {
            /* manifest 声明文件不存在 → 缺 DLL: finding LOADER(宿主必须报错) */
            grow_findings(r, ACS_REG_EC_LOADER, (int32_t)r->entry_count, -1);
            e->finding_mask |= ACS_REG_F_LOAD_FAILED;
            e->detail_code = ACS_REG_EC_LOADER;
            r->entry_count++;
            continue;
        }
        /* root 校验: canonical 结果必须在 allowed_root 内; 越界 = 结构/安全
         * 错误, 硬失败(open 返回非 0) —— 绝不放行 escape, 也不静默记 finding。 */
        if (root_canon) {
            size_t rl = strlen(root_canon);
            int within = strncmp(canon, root_canon, rl) == 0 &&
                         (canon[rl] == '/' || canon[rl] == 0);
            if (!within) {
                free(canon);
                free(mus); free(mdir); free(mcanon);
                acs_registry_close_v1(r);
                fill_err(err, ACS_ERR_PARAM, ACS_REG_EC_PATH_NOT_CANONICAL);
                return ACS_ERR_PARAM;
            }
        }
        set_entry_str(e->abs_path, sizeof(e->abs_path), canon);

        /* 实际 sha256 */
        if (file_sha256_hex(canon, e->sha_actual) != 0) {
            e->sha_actual[0] = 0;
        }
        /* hash 比对 */
        if (e->sha_registered[0] && e->sha_actual[0] &&
            strcmp(e->sha_registered, e->sha_actual) != 0) {
            registry_add_finding_bit(r, e, ACS_REG_EC_HASH_MISMATCH, ACS_REG_F_HASH_MISMATCH);
        }

        /* module.yaml(module kind; yaml_root 可选) */
        if (is_mod && opt->module_yaml_root.size > 0 && opt->module_yaml_root.data) {
            char* yr = (char*)malloc((size_t)opt->module_yaml_root.size + 1u);
            if (yr) {
                memcpy(yr, opt->module_yaml_root.data, (size_t)opt->module_yaml_root.size);
                yr[opt->module_yaml_root.size] = 0;
                /* rel 目录 = rel_path 的 dirname */
                char rel_dir[REG_SZ_REL + 64];
                snprintf(rel_dir, sizeof(rel_dir), "%s", mus[i].rel_path);
                char* rsl = strrchr(rel_dir, '/');
                if (rsl) {
                    *rsl = 0;
                    if (!rel_dir[0]) snprintf(rel_dir, sizeof(rel_dir), ".");
                } else {
                    snprintf(rel_dir, sizeof(rel_dir), ".");
                }
                char yp[4096];
                if (strcmp(rel_dir, ".") == 0) snprintf(yp, sizeof(yp), "%s/module.yaml", yr);
                else snprintf(yp, sizeof(yp), "%s/%s/module.yaml", yr, rel_dir);
                yaml_s y;
                yaml_load(yp, &y);
                if (y.has_mid) {
                    set_entry_str(e->module_id_yaml, sizeof(e->module_id_yaml), y.module_id);
                    /* yaml module_id ↔ manifest module_id */
                    if (e->module_id[0] && strcmp(e->module_id, y.module_id) != 0) {
                        grow_findings(r, ACS_REG_EC_YAML_INCONSISTENT, (int32_t)r->entry_count, -1);
                        e->finding_mask |= ACS_REG_F_YAML_INCONSISTENT;
                    }
                }
                if (y.has_ver) set_entry_str(e->version_yaml, sizeof(e->version_yaml), y.module_version);
                free(yr);
            }
        }

        /* loader 加载(query+describe): module/provider 均走入口握手 */
        {
            acs_error_info_v1 ler;
            memset(&ler, 0, sizeof(ler));
            char lmid[REG_SZ_MID], lver[REG_SZ_VER], lbid[REG_SZ_BID], lsha[REG_SZ_SHA];
            acs_status lst = load_dll(&r->allocator, mus[i].kind, canon,
                                      (is_mod && e->module_id[0]) ? e->module_id : "",
                                      e->sha_registered, "", e->abi_version,
                                      lmid, sizeof(lmid), lver, sizeof(lver),
                                      lbid, sizeof(lbid), lsha, &ler);
            if (lst != ACS_OK) {
                /* loader 拒绝分类(registry 输出权威 kind, 非笼统 LOADER):
                 *   - module_id 失配(loader 在 describe 握手后拒绝) = 三方
                 *     descriptor 不一致检测项 → MODULE_ID_MISMATCH;
                 *   - hash 失配 = registry 前序文件 sha256 比对已报(登记 sha
                 *     非空且不等时), 此处补 mask/detail 但不重复加 finding;
                 *   - 其它(缺 symbol/ELF/ABI/文件缺失…) → LOADER(entry 未加载)。
                 * 任何拒绝均 loaded=0: 未加载 entry 绝不被当作可用路由。 */
                uint32_t ldet = ler.detail_code;
                int classified = 0;
                if (is_mod && e->module_id[0] &&
                    ldet == ACS_LOADER_EC_MODULE_ID_MISMATCH) {
                    registry_add_finding_bit(r, e, ACS_REG_EC_MODULE_ID_MISMATCH,
                                             ACS_REG_F_MODULE_ID_MISMATCH);
                    e->detail_code = ACS_REG_EC_MODULE_ID_MISMATCH;
                    classified = 1;
                } else if (ldet == ACS_LOADER_EC_HASH_MISMATCH) {
                    if (!(e->finding_mask & ACS_REG_F_HASH_MISMATCH))
                        registry_add_finding_bit(r, e, ACS_REG_EC_HASH_MISMATCH,
                                                 ACS_REG_F_HASH_MISMATCH);
                    e->detail_code = ACS_REG_EC_HASH_MISMATCH;
                    classified = 1;
                }
                if (!classified) {
                    e->finding_mask |= ACS_REG_F_LOAD_FAILED;
                    e->detail_code = ACS_REG_EC_LOADER;
                    grow_findings(r, ACS_REG_EC_LOADER,
                                  (int32_t)r->entry_count, -1);
                }
            } else {
                e->loaded = 1;
                if (lmid[0]) {
                    set_entry_str(e->module_id_dll, sizeof(e->module_id_dll), lmid);
                    if (is_mod && e->module_id[0] && strcmp(e->module_id, lmid) != 0) {
                        registry_add_finding_bit(r, e, ACS_REG_EC_MODULE_ID_MISMATCH,
                                                 ACS_REG_F_MODULE_ID_MISMATCH);
                    }
                }
                if (lver[0]) set_entry_str(e->version_dll, sizeof(e->version_dll), lver);
                if (lbid[0]) set_entry_str(e->build_id_dll, sizeof(e->build_id_dll), lbid);
                if (lsha[0]) set_entry_str(e->sha_actual, sizeof(e->sha_actual), lsha);
                /* 版本冲突: yaml ↔ dll */
                if (e->version_yaml[0] && e->version_dll[0] &&
                    strcmp(e->version_yaml, e->version_dll) != 0) {
                    grow_findings(r, ACS_REG_EC_VERSION_CONFLICT, (int32_t)r->entry_count, -1);
                    e->finding_mask |= ACS_REG_F_VERSION_CONFLICT;
                }
            }
        }
        r->entry_count++;
        free(canon);
    }
    free(mus);

    /* 可选: 扫描未登记 DLL(modules/ 与 providers/ 子目录, 一层) */
    if (opt->scan_unregistered) {
        const char* scan_dirs[2] = { "modules", "providers" };
        int s;
        for (s = 0; s < 2; ++s) {
            char sd[4096];
            snprintf(sd, sizeof(sd), "%s/%s", mdir, scan_dirs[s]);
            DIR* d = opendir(sd);
            if (!d) continue;
            struct dirent* de;
            while ((de = readdir(d)) != NULL) {
                if (de->d_name[0] == '.') continue;
                size_t dl = strlen(de->d_name);
                int is_so = dl > 3 && strcmp(de->d_name + dl - 3, ".so") == 0;
                int is_dll = dl > 4 && strcmp(de->d_name + dl - 4, ".dll") == 0;
                if (!is_so && !is_dll) continue;
                char fp[4096];
                tc(fp, sizeof(fp), sd);
                size_t fp_len = strlen(fp);
                if (fp_len + 1 + strlen(de->d_name) < sizeof(fp)) {
                    fp[fp_len] = '/';
                    tc(fp + fp_len + 1, sizeof(fp) - fp_len - 1, de->d_name);
                }
                struct stat st;
                if (stat(fp, &st) != 0 || !S_ISREG(st.st_mode)) continue;
                /* 与登记 rel_path 比对(相对 manifest 目录) */
                char reg[4096];
                snprintf(reg, sizeof(reg), "%s/%.240s", scan_dirs[s], de->d_name);
                int registered = 0;
                for (uint32_t k = 0; k < r->entry_count; ++k) {
                    if (strcmp(r->entries[k].rel_path, reg) == 0) { registered = 1; break; }
                }
                if (!registered) {
                    grow_findings(r, ACS_REG_EC_UNREGISTERED_DLL, -1, -1);
                }
            }
            closedir(d);
        }
    }

    free(mdir);
    free(mcanon);
    if (root_canon) free(root_canon);

    /* 句柄无外部分配路径串(mdir 等已 free; abs_path 等内联) */
    *out = r;
    return ACS_OK;
}

ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_check_v1(acs_registry* r, acs_error_info_v1* err,
                      uint32_t* out_issue_count) {
    if (!r) { if (err) fill_err(err, ACS_ERR_PARAM, ACS_REG_EC_INVALID_INPUT); return ACS_ERR_PARAM; }
    if (out_issue_count) *out_issue_count = r->finding_count;
    return ACS_OK;
}

ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_get_finding_v1(acs_registry* r, uint32_t index,
                        acs_registry_finding_v1* out) {
    if (!r || !out) return ACS_ERR_PARAM;
    if (index >= r->finding_count) return ACS_ERR_PARAM;
    memset(out, 0, sizeof(*out));
    out->head.struct_size = (uint32_t)sizeof(acs_registry_finding_v1);
    out->head.abi_version = ACS_ABI_VERSION_V1;
    out->kind = r->findings[index].kind;
    out->entry_index = r->findings[index].entry_index;
    out->aux_index = r->findings[index].aux_index;
    const char* m = reg_msg((int32_t)out->kind);
    out->detail.data = m;
    out->detail.size = (uint64_t)strlen(m);
    out->detail.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    out->detail.head.abi_version = ACS_ABI_VERSION_V1;
    return ACS_OK;
}

ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_entry_count_v1(acs_registry* r, uint32_t* out_count) {
    if (!r || !out_count) return ACS_ERR_PARAM;
    *out_count = r->entry_count;
    return ACS_OK;
}

ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_get_entry_v1(acs_registry* r, uint32_t index,
                      acs_registry_entry_v1* out) {
    if (!r || !out) return ACS_ERR_PARAM;
    if (index >= r->entry_count) return ACS_ERR_PARAM;
    entry_s* e = &r->entries[index];
    memset(out, 0, sizeof(*out));
    out->head.struct_size = (uint32_t)sizeof(acs_registry_entry_v1);
    out->head.abi_version = ACS_ABI_VERSION_V1;
    out->unit_id = sp(e->unit_id);
    out->kind = sp(e->kind);
    out->rel_path = sp(e->rel_path);
    out->abs_path = sp(e->abs_path);
    out->module_id = sp(e->module_id);
    out->module_id_dll = sp(e->module_id_dll);
    out->module_id_yaml = sp(e->module_id_yaml);
    out->version_dll = sp(e->version_dll);
    out->version_yaml = sp(e->version_yaml);
    out->build_id_dll = sp(e->build_id_dll);
    out->sha256_registered = sp(e->sha_registered);
    out->sha256_actual = sp(e->sha_actual);
    out->status = sp(e->status);
    out->abi_version = e->abi_version;
    out->loaded = e->loaded;
    out->finding_mask = e->finding_mask;
    out->detail_code = e->detail_code;
    return ACS_OK;
}

ASTROCS_EXPORT void ASTROCS_CALL
acs_registry_close_v1(acs_registry* r) {
    if (!r) return;
    if (r->entries) r->allocator.free(r->allocator.user_data, r->entries);
    if (r->findings) r->allocator.free(r->allocator.user_data, r->findings);
    r->allocator.free(r->allocator.user_data, r);
}

ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_self_test_v1(void) {
    /* sha256 向量: 空串 / "abc" */
    char hx[65];
    sha256_buf_hex("", 0, hx);
    if (strcmp(hx, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") != 0)
        return ACS_ERR_SELFTEST;
    sha256_buf_hex("abc", 3, hx);
    if (strcmp(hx, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0)
        return ACS_ERR_SELFTEST;
    /* 长输入跨块向量 */
    static const char m2[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha256_buf_hex(m2, strlen(m2), hx);
    if (strcmp(hx, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") != 0)
        return ACS_ERR_SELFTEST;
    /* JSON 提取器样本: units 2 条, module 一条有 module_id */
    {
        const char* sample = "{\"product_version\":\"0.0.0\",\"units\":["
                             "{\"unit_id\":\"A\",\"kind\":\"module\",\"rel_path\":\"m.so\","
                             "\"module_id\":\"m.x\",\"sha256\":null,\"abi_version\":1},"
                             "{\"unit_id\":\"B\",\"kind\":\"exe\",\"rel_path\":\"bin\"}]}";
        mu* arr = NULL;
        int ed = 0;
        int n = parse_units(sample, strlen(sample), &arr, &ed);
        if (n != 2 || ed != 0) { free(arr); return ACS_ERR_SELFTEST; }
        if (strcmp(arr[0].module_id, "m.x") != 0 || arr[0].abi != 1 ||
            strcmp(arr[1].kind, "exe") != 0) { free(arr); return ACS_ERR_SELFTEST; }
        free(arr);
    }
    return ACS_OK;
}
