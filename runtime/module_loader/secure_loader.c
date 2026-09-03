/* AstroCS Secure Loader — Linux 真实实现 (ABI-003)
 *
 * 文件: runtime/module_loader/secure_loader.c
 * 实现: 12_DLL_ABI_AND_LOADER_STANDARD.md §6 + 合同头 secure_loader.h。
 *
 * 本文件是 host 侧(runtime, 非跨 DLL 模块)工具库: 内部临时内存用 libc;
 * 跨出 loader 的字符串(loaded 信息)经 options.allocator 分配并由
 * acs_secure_loader_release_v1 释放(同一 allocator 实例, 绝不跨 CRT/堆)。
 *
 * Linux 加载语义:
 *   - 路径: 必须绝对(拒绝相对 → 当前目录发现无从谈起); realpath 求 canonical;
 *     与 allowed_root 前缀比对拒绝 symlink escape; canonical 结果与入参不一致
 *     拒绝(入参含 symlink/.. 分量)。Windows 同序语义 + LoadLibraryExW 安全 flags
 *     由 WIN-* 在 _WIN32 分支落地(本文件 _WIN32 下返回 ACS_ERR_UNSUPPORTED)。
 *   - 只从 manifest 给的绝对 canonical 路径 dlopen(RTLD_NOW|RTLD_LOCAL);
 *     不读 LD_LIBRARY_PATH, 不 fallback 其它目录/静态算法。
 *   - 加载前: ELF64-x86-64 头校验 + sha256(FIPS 180-4 内部实现)与 manifest 比对;
 *   - 加载后: 必需导出符号(module/provider 查询入口)存在性 + host_abi 握手 +
 *     describe → 校验 head/abi_version/module_id/build_id/version。
 *
 * 日志纪律: loader 不自行写任何日志(无 logger 依赖路径); 详细诊断经
 * acs_error_info_v1 返回调用方。错误消息为编译期静态字面量, 绝不含路径/
 * sha256/文件内容(12 §6; 验收: 日志不泄凭据)。
 */
/* realpath(NULL 输出) 需 XSI 700 特性宏; 必须在任何 include 之前 */
#define _XOPEN_SOURCE 700

#include "secure_loader.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
/* Windows 落地属 WIN-* 任务(在 Fatduck 验证): 本文件提供同源语义契约与
 * 错误码, 不包含 win32 LoadLibrary 代码路径(见 secure_loader.h 平台注释)。 */
#define ACS_LOADER_IMPL_PLATFORM 0
#else
#define ACS_LOADER_IMPL_PLATFORM 1
#endif

/* ═══════════════════════════ 内部 FIPS 180-4 SHA-256 ═══════════════════════════
 * 纯 C、自包含、无第三方依赖(仓库无现成纯 C sha256; lib/common/crypto 为 C++,
 * loader 不跨语言边界)。仅用于二进制身份比对(非密钥用途)。 */

typedef struct acs_sha256_ctx_s {
    uint32_t h[8];
    uint64_t total_bytes;
    uint8_t  block[64];
    uint32_t block_len;
} acs_sha256_ctx;

static const uint32_t kSha256K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t rot_r(uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }

static void sha256_block(acs_sha256_ctx* c, const uint8_t* p) {
    uint32_t w[64];
    unsigned i;
    for (i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    }
    for (i = 16; i < 64; ++i) {
        uint32_t s0 = rot_r(w[i - 15], 7) ^ rot_r(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rot_r(w[i - 2], 17) ^ rot_r(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3];
    uint32_t e = c->h[4], f = c->h[5], g = c->h[6], h = c->h[7];
    for (i = 0; i < 64; ++i) {
        uint32_t s1 = rot_r(e, 6) ^ rot_r(e, 11) ^ rot_r(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + ch + kSha256K[i] + w[i];
        uint32_t s0 = rot_r(a, 2) ^ rot_r(a, 13) ^ rot_r(a, 22);
        uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = s0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

static void sha256_init(acs_sha256_ctx* c) {
    c->h[0] = 0x6a09e667u; c->h[1] = 0xbb67ae85u;
    c->h[2] = 0x3c6ef372u; c->h[3] = 0xa54ff53au;
    c->h[4] = 0x510e527fu; c->h[5] = 0x9b05688cu;
    c->h[6] = 0x1f83d9abu; c->h[7] = 0x5be0cd19u;
    c->total_bytes = 0;
    c->block_len = 0;
}

static void sha256_update(acs_sha256_ctx* c, const void* data, uint64_t len) {
    const uint8_t* p = (const uint8_t*)data;
    c->total_bytes += len;
    if (c->block_len > 0) {
        uint32_t need = 64u - c->block_len;
        if ((uint64_t)need > len) need = (uint32_t)len;
        memcpy(c->block + c->block_len, p, need);
        c->block_len += need;
        p += need;
        len -= need;
        if (c->block_len == 64u) {
            sha256_block(c, c->block);
            c->block_len = 0;
        }
    }
    while (len >= 64u) {
        sha256_block(c, p);
        p += 64;
        len -= 64;
    }
    if (len > 0) {
        memcpy(c->block, p, (size_t)len);
        c->block_len = (uint32_t)len;
    }
}

static void sha256_final(acs_sha256_ctx* c, uint8_t out[32]) {
    uint64_t bits = c->total_bytes * 8u;
    uint8_t pad = 0x80u;
    sha256_update(c, &pad, 1);
    uint8_t zero = 0u;
    while (c->block_len != 56u) sha256_update(c, &zero, 1);
    uint8_t lenb[8];
    unsigned i;
    for (i = 0; i < 8; ++i) lenb[i] = (uint8_t)(bits >> (56u - i * 8u));
    sha256_update(c, lenb, 8);
    for (i = 0; i < 8; ++i) {
        out[i * 4]     = (uint8_t)(c->h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(c->h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(c->h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(c->h[i]);
    }
}

static void sha256_hex(const uint8_t d[32], char out[65]) {
    static const char hexc[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 32; ++i) {
        out[i * 2]     = hexc[d[i] >> 4];
        out[i * 2 + 1] = hexc[d[i] & 0x0fu];
    }
    out[64] = '\0';
}

/* ═══════════════════════════ span/str 工具 ═══════════════════════════ */

static acs_str_v1 str_view(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = (uint64_t)strlen(s);
    return v;
}

static int str_eq_span(const acs_str_v1* a, const acs_str_v1* b) {
    if (a->size != b->size) return 0;
    if (a->size == 0) return 1;
    if (a->data == NULL || b->data == NULL) return 0;
    return memcmp(a->data, b->data, (size_t)a->size) == 0;
}

static int span_is(const acs_str_v1* s, const char* lit) {
    size_t n = strlen(lit);
    return s->size == (uint64_t)n && memcmp(s->data, lit, n) == 0;
}

/* 由 loader allocator 拥有的 NUL 结尾串构造 span(仅用于 describe 输出读取) */
static acs_str_v1 make_str(const char* p, uint64_t len) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = p;
    v.size = len;
    return v;
}

/* ═══════════════════════════ 错误消息(静态字面量, 无路径/内容) ═══════════════════════════ */

static const char* detail_message(int32_t detail) {
    switch (detail) {
        case ACS_LOADER_EC_INVALID_INPUT:       return "loader: invalid input (empty options/unit/allocator/out)";
        case ACS_LOADER_EC_PATH_NOT_ABS:        return "loader: path must be absolute (relative discovery rejected)";
        case ACS_LOADER_EC_PATH_NOT_CANONICAL:  return "loader: path not canonical (symlink/.. resolves differently)";
        case ACS_LOADER_EC_PATH_ESCAPE:         return "loader: path escapes allowed root (symlink escape rejected)";
        case ACS_LOADER_EC_HASH_MISMATCH:       return "loader: file sha256 does not match manifest";
        case ACS_LOADER_EC_FILE_MISSING:        return "loader: file does not exist";
        case ACS_LOADER_EC_FILE_READ:           return "loader: file unreadable";
        case ACS_LOADER_EC_ELF_MAGIC:           return "loader: not an ELF file";
        case ACS_LOADER_EC_ELF_CLASS:           return "loader: ELF class is not 64-bit little-endian";
        case ACS_LOADER_EC_ELF_MACHINE:         return "loader: ELF machine is not AMD64 (x86-64)";
        case ACS_LOADER_EC_SYMBOL_MISSING:      return "loader: required entry symbol missing";
        case ACS_LOADER_EC_HANDSHAKE_ABI:       return "loader: module ABI handshake rejected";
        case ACS_LOADER_EC_HANDSHAKE_PARAM:     return "loader: module query returned error / no api";
        case ACS_LOADER_EC_DESCRIPTOR_MISMATCH: return "loader: module descriptor ABI/version mismatch";
        case ACS_LOADER_EC_MODULE_ID_MISMATCH:  return "loader: module id does not match manifest";
        case ACS_LOADER_EC_BUILD_ID_MISMATCH:   return "loader: build id does not match manifest";
        case ACS_LOADER_EC_KIND_UNSUPPORTED:    return "loader: unit kind not supported (module/provider)";
        case ACS_LOADER_EC_OS_LOAD:             return "loader: OS dynamic load failed";
        default:                                return "loader: internal error";
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
    const char* m = detail_message(detail);
    err->message_utf8 = m;
    err->message_bytes = (uint32_t)strlen(m);
}

/* ═══════════════════════════ 句柄/分配 ═══════════════════════════ */

struct acs_loader_handle_s {
    acs_allocator_v1 allocator;   /* 快照(同一实例分配/释放) */
    void* dl;
    const acs_module_api_v1*   module_api;
    const acs_provider_api_v1* provider_api;
    char* resolved_path;          /* allocator 分配, NUL 结尾 */
    char* loaded_sha;             /* allocator 分配, 64 hex + NUL */
    uint32_t abi_version;
    /* 以下借 module 静态(describe); 有效至 release(dlclose) */
    acs_str_v1 module_id;
    acs_str_v1 module_version;
    acs_str_v1 build_id;
    acs_str_v1 api_id;
};

static char* alloc_str_copy(const acs_allocator_v1* a, const void* data,
                            uint64_t len, uint64_t extra) {
    void* p = a->alloc(a->user_data, len + extra, 8u);
    if (!p) return NULL;
    if (len > 0) memcpy(p, data, (size_t)len);
    return (char*)p;
}

/* ═══════════════════════════ 平台相关(路径/ELF/dlopen) ═══════════════════════════ */

#if ACS_LOADER_IMPL_PLATFORM

/* canonical 化绝对路径; 返回 libc 分配(调用方 free); 失败 NULL + errno 保留 */
static char* path_canonical(const char* abs) {
    return realpath(abs, NULL);
}

/* ELF64-x86-64 头校验(加载前, 读文件头前 20 字节); *detail_out 填细分码 */
static int elf64_ok(const uint8_t* h, int32_t* detail_out) {
    *detail_out = ACS_LOADER_EC_INTERNAL;
    if (h[0] != 0x7f || h[1] != 'E' || h[2] != 'L' || h[3] != 'F') {
        *detail_out = ACS_LOADER_EC_ELF_MAGIC;
        return 0;
    }
    if (h[4] != 2) {                 /* ELFCLASS64 */
        *detail_out = ACS_LOADER_EC_ELF_CLASS;
        return 0;
    }
    if (h[5] != 1) {                 /* ELFDATA2LSB(仅小端接受) */
        *detail_out = ACS_LOADER_EC_ELF_CLASS;
        return 0;
    }
    uint16_t machine = (uint16_t)((uint16_t)h[18] | ((uint16_t)h[19] << 8));
    if (machine != 62u) {            /* EM_X86_64 */
        *detail_out = ACS_LOADER_EC_ELF_MACHINE;
        return 0;
    }
    *detail_out = ACS_LOADER_EC_NONE;
    return 1;
}

/* 读整文件; 返回 libc 分配 buffer(*size 输出); 失败 NULL */
static uint8_t* read_file_all(const char* path, uint64_t* size_out) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    uint8_t* buf = (uint8_t*)malloc((size_t)sz > 0 ? (size_t)sz : 1u);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = 0;
    if (sz > 0) {
        rd = fread(buf, 1, (size_t)sz, f);
        if (rd != (size_t)sz) { free(buf); fclose(f); return NULL; }
    }
    fclose(f);
    *size_out = (uint64_t)rd;
    return buf;
}

/* module/provider 查询入口符号类型 */
typedef acs_status (ASTROCS_CALL *module_query_fn)(uint32_t,
                                                   const acs_host_api_v1*,
                                                   const acs_module_api_v1**);
typedef acs_status (ASTROCS_CALL *provider_query_fn)(uint32_t,
                                                     const acs_host_api_v1*,
                                                     const acs_provider_api_v1**);

static void* resolve_symbol(void* dl, const char* name) {
    dlerror();  /* 清残留 */
    return dlsym(dl, name);
}

#else /* _WIN32: 契约占位(实现由 WIN-* 落地) */

static int elf64_ok(const uint8_t* h, int32_t* detail_out) {
    (void)h;
    if (detail_out) *detail_out = ACS_LOADER_EC_KIND_UNSUPPORTED;
    return 0;
}

#endif /* ACS_LOADER_IMPL_PLATFORM */

/* ═══════════════════════════ 加载主流程 ═══════════════════════════ */

static acs_status check_span(const acs_str_v1* s, acs_error_info_v1* err) {
    if (s->head.abi_version != ACS_ABI_VERSION_V1 ||
        s->head.struct_size != sizeof(acs_str_v1)) {
        fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_INVALID_INPUT);
        return ACS_ERR_ABI_MISMATCH;
    }
    if (s->size > 0 && s->data == NULL) {
        fill_err(err, ACS_ERR_PARAM, ACS_LOADER_EC_INVALID_INPUT);
        return ACS_ERR_PARAM;
    }
    return ACS_OK;
}

/* 构造最小 host 表(query 握手需要; allocator 必填, 其余可空) */
static acs_host_api_v1 make_host(const acs_allocator_v1* allocator) {
    acs_host_api_v1 host;
    memset(&host, 0, sizeof(host));
    host.head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    host.head.abi_version = ACS_ABI_VERSION_V1;
    host.allocator = allocator;
    return host;
}

ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_secure_loader_load_v1(const acs_loader_options_v1* opt,
                          acs_error_info_v1* err,
                          acs_loader_handle** out) {
    acs_loader_handle* h = NULL;
    char* canon = NULL;
    char computed_hex[65];

    if (!opt || !out || !opt->allocator) {
        fill_err(err, ACS_ERR_PARAM, ACS_LOADER_EC_INVALID_INPUT);
        return ACS_ERR_PARAM;
    }
    *out = NULL;
    if (opt->head.abi_version != ACS_ABI_VERSION_V1 ||
        opt->head.struct_size != sizeof(acs_loader_options_v1)) {
        fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_INVALID_INPUT);
        return ACS_ERR_ABI_MISMATCH;
    }
    const acs_load_manifest_unit_v1* u = &opt->unit;
    if (u->head.abi_version != ACS_ABI_VERSION_V1 ||
        u->head.struct_size != sizeof(acs_load_manifest_unit_v1)) {
        fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_INVALID_INPUT);
        return ACS_ERR_ABI_MISMATCH;
    }
    acs_status st;
    if ((st = check_span(&u->abs_path_utf8, err)) != ACS_OK) return st;
    if ((st = check_span(&u->kind, err)) != ACS_OK) return st;
    if ((st = check_span(&u->module_id, err)) != ACS_OK) return st;
    if ((st = check_span(&u->expected_sha256, err)) != ACS_OK) return st;
    if ((st = check_span(&u->expected_build_id, err)) != ACS_OK) return st;
    if ((st = check_span(&opt->allowed_root_utf8, err)) != ACS_OK) return st;
    if (u->abs_path_utf8.size == 0 || u->kind.size == 0) {
        fill_err(err, ACS_ERR_PARAM, ACS_LOADER_EC_INVALID_INPUT);
        return ACS_ERR_PARAM;
    }

    int is_module = span_is(&u->kind, "module");
    int is_provider = span_is(&u->kind, "provider");
    if (!is_module && !is_provider) {
        fill_err(err, ACS_ERR_UNSUPPORTED, ACS_LOADER_EC_KIND_UNSUPPORTED);
        return ACS_ERR_UNSUPPORTED;
    }

#if !ACS_LOADER_IMPL_PLATFORM
    (void)is_module; (void)is_provider;
    (void)u; (void)opt;
    fill_err(err, ACS_ERR_UNSUPPORTED, ACS_LOADER_EC_KIND_UNSUPPORTED);
    return ACS_ERR_UNSUPPORTED;   /* Windows: WIN-* 落地; loader 契约先冻结 */
#else

    /* ── 1. 路径: 绝对 + canonical + allowed_root 内 ── */
    if (u->abs_path_utf8.data[0] != '/') {
        fill_err(err, ACS_ERR_PARAM, ACS_LOADER_EC_PATH_NOT_ABS);
        return ACS_ERR_PARAM;
    }
    char* abs_copy = (char*)malloc((size_t)u->abs_path_utf8.size + 1u);
    if (!abs_copy) {
        fill_err(err, ACS_ERR_NOMEM, ACS_LOADER_EC_INTERNAL);
        return ACS_ERR_NOMEM;
    }
    memcpy(abs_copy, u->abs_path_utf8.data, (size_t)u->abs_path_utf8.size);
    abs_copy[u->abs_path_utf8.size] = '\0';

    canon = path_canonical(abs_copy);
    if (!canon) {
        free(abs_copy);
        fill_err(err, ACS_ERR_IO, ACS_LOADER_EC_FILE_MISSING);
        return ACS_ERR_IO;
    }
    /* 入参本身必须已是 canonical 文本(不含 symlink/.. 分量): realpath 结果与
     * 入参文本不一致 → 目录内链接或 .. 分量 → 拒绝(防 symlink 伪装/escape)。 */
    if (strcmp(canon, abs_copy) != 0) {
        free(abs_copy);
        free(canon);
        canon = NULL;
        fill_err(err, ACS_ERR_PARAM, ACS_LOADER_EC_PATH_NOT_CANONICAL);
        return ACS_ERR_PARAM;
    }
    free(abs_copy);

    /* allowed_root 前缀(防 symlink escape; root 自身 canonical 后比对) */
    if (opt->allowed_root_utf8.size > 0) {
        char* root_copy = (char*)malloc((size_t)opt->allowed_root_utf8.size + 1u);
        if (!root_copy) {
            free(canon); canon = NULL;
            fill_err(err, ACS_ERR_NOMEM, ACS_LOADER_EC_INTERNAL);
            return ACS_ERR_NOMEM;
        }
        memcpy(root_copy, opt->allowed_root_utf8.data, (size_t)opt->allowed_root_utf8.size);
        root_copy[opt->allowed_root_utf8.size] = '\0';
        char* root_canon = path_canonical(root_copy);
        free(root_copy);
        if (!root_canon) {
            free(canon); canon = NULL;
            fill_err(err, ACS_ERR_PARAM, ACS_LOADER_EC_PATH_NOT_CANONICAL);
            return ACS_ERR_PARAM;
        }
        size_t rl = strlen(root_canon);
        int within = strcmp(canon, root_canon) == 0 ||
                     (strncmp(canon, root_canon, rl) == 0 && canon[rl] == '/');
        free(root_canon);
        if (!within) {
            free(canon); canon = NULL;
            fill_err(err, ACS_ERR_PARAM, ACS_LOADER_EC_PATH_ESCAPE);
            return ACS_ERR_PARAM;
        }
    }

    /* ── 2. 读文件 + ELF64 头校验 + sha256 比对 ── */
    uint64_t fsize = 0;
    uint8_t* fbuf = read_file_all(canon, &fsize);
    if (!fbuf) {
        free(canon); canon = NULL;
        fill_err(err, ACS_ERR_IO, ACS_LOADER_EC_FILE_READ);
        return ACS_ERR_IO;
    }
    int32_t elf_detail = ACS_LOADER_EC_INTERNAL;
    if (fsize < 20u || !elf64_ok(fbuf, &elf_detail)) {
        free(fbuf); free(canon); canon = NULL;
        fill_err(err, ACS_ERR_ABI_MISMATCH, elf_detail);
        return ACS_ERR_ABI_MISMATCH;
    }
    {
        acs_sha256_ctx c;
        uint8_t digest[32];
        sha256_init(&c);
        sha256_update(&c, fbuf, fsize);
        sha256_final(&c, digest);
        sha256_hex(digest, computed_hex);
    }
    free(fbuf);
    if (u->expected_sha256.size > 0) {
        if (u->expected_sha256.size != 64) {
            free(canon); canon = NULL;
            fill_err(err, ACS_ERR_PARAM, ACS_LOADER_EC_HASH_MISMATCH);
            return ACS_ERR_PARAM;
        }
        char want[65];
        memcpy(want, u->expected_sha256.data, 64);
        want[64] = '\0';
        if (strcmp(computed_hex, want) != 0) {
            free(canon); canon = NULL;
            fill_err(err, ACS_ERR_PARAM, ACS_LOADER_EC_HASH_MISMATCH);
            return ACS_ERR_PARAM;
        }
    }

    /* ── 3. dlopen(绝对 canonical path; RTLD_NOW|RTLD_LOCAL; 不涉搜索路径) ── */
    void* dl = dlopen(canon, RTLD_NOW | RTLD_LOCAL);
    if (!dl) {
        free(canon); canon = NULL;
        fill_err(err, ACS_ERR_IO, ACS_LOADER_EC_OS_LOAD);
        return ACS_ERR_IO;
    }

    /* ── 4. 句柄分配(此后所有权=句柄; canon 仍须在成功/失败路径释放) ── */
    h = (acs_loader_handle*)opt->allocator->alloc(
        opt->allocator->user_data, sizeof(acs_loader_handle), 8u);
    if (!h) {
        dlclose(dl);
        free(canon); canon = NULL;
        fill_err(err, ACS_ERR_NOMEM, ACS_LOADER_EC_INTERNAL);
        return ACS_ERR_NOMEM;
    }
    memset(h, 0, sizeof(*h));
    h->allocator = *opt->allocator;
    h->dl = dl;
    h->abi_version = ACS_ABI_VERSION_V1;

    /* 记录实际加载 canonical 路径 + 实际 sha256(在一切校验前定格, 失败也要释放) */
    h->resolved_path = alloc_str_copy(&h->allocator, canon, strlen(canon), 1u);
    if (!h->resolved_path) {
        free(canon); canon = NULL;
        fill_err(err, ACS_ERR_NOMEM, ACS_LOADER_EC_INTERNAL);
        goto fail_release_h;
    }
    h->resolved_path[strlen(canon)] = '\0';
    h->loaded_sha = alloc_str_copy(&h->allocator, computed_hex, 64, 1u);
    if (!h->loaded_sha) {
        free(canon); canon = NULL;
        fill_err(err, ACS_ERR_NOMEM, ACS_LOADER_EC_INTERNAL);
        goto fail_release_h;
    }
    h->loaded_sha[64] = '\0';
    free(canon); canon = NULL;

    acs_host_api_v1 host = make_host(opt->allocator);

    /* ── 5. 加载后: 必需符号 + 握手 + describe 校验 ── */
    if (is_module) {
        module_query_fn qfn = (module_query_fn)(void*)resolve_symbol(dl, "astrocs_module_query_v1");
        if (!qfn) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_SYMBOL_MISSING);
            goto fail_release_h;
        }
        const acs_module_api_v1* api = NULL;
        acs_status qr = qfn(ACS_ABI_VERSION_V1, &host, &api);
        if (qr != ACS_OK || !api) {
            fill_err(err, ACS_ERR_ABI_MISMATCH,
                     (qr == ACS_ERR_ABI_MISMATCH) ? ACS_LOADER_EC_HANDSHAKE_ABI
                                                  : ACS_LOADER_EC_HANDSHAKE_PARAM);
            goto fail_release_h;
        }
        if (api->head.abi_version != ACS_ABI_VERSION_V1 ||
            api->head.struct_size < sizeof(acs_module_api_v1)) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_HANDSHAKE_ABI);
            goto fail_release_h;
        }
        if (u->abi_version != 0 && api->head.abi_version != u->abi_version) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_DESCRIPTOR_MISMATCH);
            goto fail_release_h;
        }
        if (!api->describe) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_SYMBOL_MISSING);
            goto fail_release_h;
        }
        acs_module_descriptor_v1 desc;
        memset(&desc, 0, sizeof(desc));
        acs_str_v1 empty = str_view("");
        acs_status dr = api->describe(api, empty, &desc);
        if (dr != ACS_OK) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_DESCRIPTOR_MISMATCH);
            goto fail_release_h;
        }
        if (desc.head.abi_version != ACS_ABI_VERSION_V1 ||
            desc.head.struct_size < sizeof(acs_module_descriptor_v1)) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_DESCRIPTOR_MISMATCH);
            goto fail_release_h;
        }
        if (u->module_id.size > 0 && !str_eq_span(&desc.module_id, &u->module_id)) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_MODULE_ID_MISMATCH);
            goto fail_release_h;
        }
        if (u->expected_build_id.size > 0 &&
            !str_eq_span(&desc.build_id, &u->expected_build_id)) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_BUILD_ID_MISMATCH);
            goto fail_release_h;
        }
        h->module_api = api;
        h->module_id = desc.module_id;
        h->module_version = desc.version;
        h->build_id = desc.build_id;
        h->api_id = desc.api_id;
    } else { /* provider */
        provider_query_fn qfn = (provider_query_fn)(void*)resolve_symbol(dl, "astrocs_provider_query_v1");
        if (!qfn) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_SYMBOL_MISSING);
            goto fail_release_h;
        }
        const acs_provider_api_v1* api = NULL;
        acs_status qr = qfn(ACS_ABI_VERSION_V1, &host, &api);
        if (qr != ACS_OK || !api) {
            fill_err(err, ACS_ERR_ABI_MISMATCH,
                     (qr == ACS_ERR_ABI_MISMATCH) ? ACS_LOADER_EC_HANDSHAKE_ABI
                                                  : ACS_LOADER_EC_HANDSHAKE_PARAM);
            goto fail_release_h;
        }
        if (api->head.abi_version != ACS_ABI_VERSION_V1 ||
            api->head.struct_size < sizeof(acs_provider_api_v1)) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_HANDSHAKE_ABI);
            goto fail_release_h;
        }
        if (u->abi_version != 0 && api->head.abi_version != u->abi_version) {
            fill_err(err, ACS_ERR_ABI_MISMATCH, ACS_LOADER_EC_DESCRIPTOR_MISMATCH);
            goto fail_release_h;
        }
        h->provider_api = api;
    }

    *out = h;
    return ACS_OK;

fail_release_h:
    if (h) {
        if (h->dl) dlclose(h->dl);
        if (h->resolved_path) h->allocator.free(h->allocator.user_data, h->resolved_path);
        if (h->loaded_sha) h->allocator.free(h->allocator.user_data, h->loaded_sha);
        h->allocator.free(h->allocator.user_data, h);
    }
    if (canon) { free(canon); canon = NULL; }
    /* 失败路径均先 fill_err(非零 status); err 可空(NULL)时 fallback ABI_MISMATCH */
    if (err && err->status != ACS_OK) return err->status;
    return ACS_ERR_ABI_MISMATCH;
#endif /* ACS_LOADER_IMPL_PLATFORM */
}

ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_secure_loader_describe_v1(acs_loader_handle* h, acs_loaded_module_v1* out) {
    if (!h || !out) return ACS_ERR_PARAM;
    if (out->head.abi_version != ACS_ABI_VERSION_V1 ||
        out->head.struct_size != sizeof(acs_loaded_module_v1)) {
        return ACS_ERR_ABI_MISMATCH;
    }
    memset(out, 0, sizeof(*out));
    out->head.struct_size = (uint32_t)sizeof(acs_loaded_module_v1);
    out->head.abi_version = ACS_ABI_VERSION_V1;
    out->module_api = h->module_api;
    out->provider_api = h->provider_api;
    out->abi_version = h->abi_version;
    out->detail_code = ACS_LOADER_EC_NONE;
    if (h->resolved_path) out->resolved_path_utf8 = make_str(h->resolved_path, strlen(h->resolved_path));
    if (h->loaded_sha) out->loaded_sha256 = make_str(h->loaded_sha, strlen(h->loaded_sha));
    out->module_id = h->module_id;
    out->module_version = h->module_version;
    out->build_id = h->build_id;
    out->api_id = h->api_id;
    return ACS_OK;
}

ASTROCS_EXPORT void ASTROCS_CALL
acs_secure_loader_release_v1(acs_loader_handle* h) {
    if (!h) return;
#if ACS_LOADER_IMPL_PLATFORM
    if (h->dl) dlclose(h->dl);
#endif
    if (h->resolved_path) h->allocator.free(h->allocator.user_data, h->resolved_path);
    if (h->loaded_sha) h->allocator.free(h->allocator.user_data, h->loaded_sha);
    h->allocator.free(h->allocator.user_data, h);
}

ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_secure_loader_self_test_v1(void) {
    /* FIPS 180-4 已知向量: 空串 e3b0c442...; "abc" ba7816bf... */
    static const uint8_t kEmpty[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    static const uint8_t kAbc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    acs_sha256_ctx c;
    uint8_t d[32];
    sha256_init(&c); sha256_final(&c, d);
    if (memcmp(d, kEmpty, 32) != 0) return ACS_ERR_SELFTEST;
    sha256_init(&c); sha256_update(&c, "abc", 3); sha256_final(&c, d);
    if (memcmp(d, kAbc, 32) != 0) return ACS_ERR_SELFTEST;
    /* 长输入跨块路径向量: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
     * (FIPS 180-4 附录 B 第二向量) 248d... */
    static const char kMsg2[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    static const uint8_t kMsg2Digest[32] = {
        0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8,
        0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
        0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67,
        0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1
    };
    sha256_init(&c); sha256_update(&c, kMsg2, strlen(kMsg2)); sha256_final(&c, d);
    if (memcmp(d, kMsg2Digest, 32) != 0) return ACS_ERR_SELFTEST;
    if (sizeof(acs_load_manifest_unit_v1) <= sizeof(acs_head)) return ACS_ERR_SELFTEST;
    if (sizeof(acs_loader_options_v1) <= sizeof(acs_head)) return ACS_ERR_SELFTEST;
    if (sizeof(acs_loaded_module_v1) <= sizeof(acs_head)) return ACS_ERR_SELFTEST;
    return ACS_OK;
}
