/* AstrocsArtifact v1 实现 (DATA-001)
 *
 * 职责:
 *   - 解析 artifact manifest JSON → opaque 句柄 (结构校验; 无外部 JSON 依赖的最小解析器,
 *     覆盖 DATA-001 验收所需字段完整性/枚举/hex/重复 producer 拒绝)。
 *   - 查询器只暴露 URI/hex 等逻辑字段; 绝不在句柄中保存或暴露文件系统路径。
 *
 * 并发: 无全局状态, reentrant; 句柄非共享。
 * 错误码: 复用 common_abi_v1 的 acs_status 数值 (0=OK, 1=PARAM, 4=IO/解析错误)。
 */
#include "astrocs/contracts/artifact_abi_v1.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 本实现内部 C 结构: 字段值直接保存解析得到的字符串; 绝不保存路径。 */
struct acs_artifact_handle_v1_s {
    char* manifest_schema;
    char* artifact_id;
    char* type_id;
    uint32_t schema_version;
    char* storage_uri;
    char* content_digest_hex;
    uint64_t size;
    char* producer_module_id;
    char* producer_build_id;
    char* run_id;
    char* phase;
    char* node_id;
    char* config_digest_hex;
    char* created_utc;
    int status; /* acs_artifact_status 或 -1(未知) */
    size_t input_count;
    char** input_artifact_ids;
    char** input_digests;
};

/* ───────────── 最小 JSON 解析工具 (不跨 DLL; 无第三方依赖) ───────────── */

static const char* skip_ws(const char* p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    return p;
}

/* 解析 JSON string 字面量 (支持 \uXXXX 仅 hex 校验, 不做代理对重组) */
static const char* parse_json_string(const char* p, const char* end, char** out) {
    if (p >= end || *p != '"') return NULL;
    ++p;
    size_t cap = 64, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    int ok = 0;
    while (p < end) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') { ok = 1; ++p; break; }
        if (c == '\\') {
            ++p;
            if (p >= end) break;
            char e = *p;
            if (e == 'u') {
                /* \uXXXX: 校验 hex, 原文直存(ASCII 域外字符以转义保留) */
                if (p + 4 >= end) break;
                int hex_ok = 1;
                for (int i = 0; i < 4; ++i) {
                    char h = p[1 + i];
                    if (!isxdigit((unsigned char)h)) { hex_ok = 0; break; }
                }
                if (!hex_ok) break;
                if (len + 7 >= cap) { cap *= 2; char* nb = (char*)realloc(buf, cap); if (!nb) goto done; buf = nb; }
                memcpy(buf + len, p, 6); len += 6;
                p += 6;
                continue;
            }
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' || e == 't') {
                if (len + 1 >= cap) { cap *= 2; char* nb = (char*)realloc(buf, cap); if (!nb) goto done; buf = nb; }
                buf[len++] = e;
                ++p;
                continue;
            }
            break; /* 非法转义 */
        }
        if (c < 0x20) break; /* 控制字符非法 */
        if (len + 1 >= cap) { cap *= 2; char* nb = (char*)realloc(buf, cap); if (!nb) goto done; buf = nb; }
        buf[len++] = (char)c;
        ++p;
    }
    if (ok) {
        buf[len] = '\0';
        if (out) *out = buf;
        else free(buf);
        return p;
    }
done:
    free(buf);
    return NULL;
}

/* 解析 JSON number → uint64/uint32 (仅非负整数) */
static const char* parse_json_uint(const char* p, const char* end, uint64_t* out, int* is_num) {
    p = skip_ws(p, end);
    *is_num = 0;
    if (p >= end || *p == '-') return NULL; /* 负数: 视为非数值(数字域不合法) */
    if (p < end && *p == '"') { /* 引号内非数字(如 NaN/Inf 字符串): 返回 NULL 由调用方拒绝 */
        return NULL;
    }
    if (p >= end || !isdigit((unsigned char)*p)) return NULL;
    uint64_t v = 0;
    while (p < end && isdigit((unsigned char)*p)) {
        uint64_t d = (uint64_t)(*p - '0');
        if (v > (UINT64_MAX - d) / 10) return NULL; /* 溢出 */
        v = v * 10 + d;
        ++p;
    }
    if (p < end && (*p == '.' || *p == 'e' || *p == 'E')) return NULL; /* 只允许整数 */
    *out = v;
    *is_num = 1;
    return p;
}

/* 在对象中顺序查找 key 并返回其值起始位置 (不做完整 JSON 语法树) */
static const char* find_key(const char* p, const char* end, const char* key) {
    size_t klen = strlen(key);
    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end) return NULL;
        if (*p == '}') return NULL; /* 对象结束 */
        if (*p != '"') { ++p; continue; }
        const char* after = parse_json_string(p, end, NULL);
        if (!after) return NULL; /* 非法字符串字面量 */
        size_t klen2 = (size_t)(after - p - 2); /* 去掉首尾引号 */
        if (klen2 == klen && strncmp(p + 1, key, klen) == 0) {
            p = skip_ws(after, end);
            if (p < end && *p == ':') return p + 1;
            return NULL;
        }
        /* 不是目标 key: 跳过其值 */
        p = after;
        p = skip_ws(p, end);
        if (p < end && *p == ':') {
            ++p;
            p = skip_ws(p, end);
            if (p < end && (*p == '"')) {
                char* v = NULL;
                const char* e2 = parse_json_string(p, end, &v);
                if (v) free(v);
                p = e2 ? e2 : end;
            } else if (p < end && (*p == '{' || *p == '[')) {
                int depth = 0;
                while (p < end) {
                    if (*p == '{' || *p == '[') ++depth;
                    else if (*p == '}' || *p == ']') { --depth; if (depth == 0) { ++p; break; } }
                    ++p;
                }
            } else {
                /* 数字/字面量: 到逗号或对象结束 */
                while (p < end && *p != ',' && *p != '}') ++p;
            }
        } else {
            p = end;
        }
        if (p >= end) return NULL;
    }
    return NULL;
}

/* 解析一个 (key,value) 序列直到对象末尾, 供数组/子对象处理。返回停在对象 '}' 或 NULL。 */
static const char* find_key_deep(const char* p, const char* end, const char* key, char** out_str,
                                 uint64_t* out_uint, int* out_has_str, int* out_has_uint) {
    const char* v = find_key(p, end, key);
    if (!v) return NULL;
    v = skip_ws(v, end);
    *out_has_str = 0; *out_has_uint = 0;
    if (v < end && *v == '"') {
        char* s = NULL;
        const char* e2 = parse_json_string(v, end, &s);
        if (!e2 || !s) { if (s) free(s); return NULL; }
        *out_str = s;
        *out_has_str = 1;
        return e2;
    }
    uint64_t u = 0; int is_num = 0;
    const char* e2 = parse_json_uint(v, end, &u, &is_num);
    if (!e2 || !is_num) return NULL;
    *out_uint = u;
    *out_has_uint = 1;
    return e2;
}

/* 在数组内解析子对象字段(用于 producer/run/node/config_digest 的 key)。*/
static int extract_field_from_obj(const char* obj_start, const char* end, const char* key,
                                  char** out, uint64_t* out_uint, int* has_str, int* has_uint) {
    /* obj_start 指向 '{' 或其后; 查找该子对象范围内的 key */
    if (obj_start >= end) return 0;
    const char* p = obj_start;
    if (*p == '{') ++p; else return 0;
    const char* v = find_key(p, end, key);
    if (!v) return 0;
    v = skip_ws(v, end);
    if (v >= end) return 0;
    if (*v == '"') {
        char* s = NULL;
        const char* e2 = parse_json_string(v, end, &s);
        if (!e2 || !s) { if (s) free(s); return 0; }
        if (out) *out = s; else free(s);
        if (has_str) *has_str = 1;
        if (has_uint) *has_uint = 0;
        return 1;
    }
    uint64_t u = 0; int is_num = 0;
    const char* e2 = parse_json_uint(v, end, &u, &is_num);
    if (!e2 || !is_num) return 0;
    if (out_uint) *out_uint = u;
    if (has_uint) *has_uint = 1;
    if (has_str) *has_str = 0;
    return 1;
}

/* ───────────── 对象切片定位 (producer/run/node/config_digest/status 子对象) ───────────── */

/* 定位某 key 值开始位置: 返回指向值的指针(可含 '{' '"' 数字), len 为该值的字节跨度 */
static const char* locate_value(const char* p, const char* end, const char* key, const char** vstart, size_t* vlen) {
    const char* v = find_key(p, end, key);
    if (!v) return NULL;
    v = skip_ws(v, end);
    if (v >= end) return NULL;
    *vstart = v;
    if (*v == '{') {
        int depth = 0;
        const char* q = v;
        while (q < end) {
            if (*q == '{') ++depth;
            else if (*q == '}') { --depth; if (depth == 0) { ++q; break; } }
            ++q;
        }
        *vlen = (size_t)(q - v);
        return (const char*)1; /* 非 NULL */
    }
    if (*v == '[') {
        int depth = 0;
        const char* q = v;
        while (q < end) {
            if (*q == '[') ++depth;
            else if (*q == ']') { --depth; if (depth == 0) { ++q; break; } }
            ++q;
        }
        *vlen = (size_t)(q - v);
        return (const char*)1;
    }
    /* scalar: 字符串或数字 */
    const char* q = v;
    if (*q == '"') {
        char* s = NULL;
        const char* e2 = parse_json_string(q, end, &s);
        if (!e2) return NULL;
        if (s) free(s);
        *vlen = (size_t)(e2 - v);
        return (const char*)1;
    }
    while (q < end && *q != ',' && *q != '}' && *q != ']' && *q != ' ' && *q != '\n' &&
           *q != '\t' && *q != '\r') ++q;
    *vlen = (size_t)(q - v);
    return (const char*)1;
}

/* ───────────── 解析主入口 ───────────── */

static int hex64_ok(const char* s) {
    if (!s) return 0;
    if (strlen(s) != 64) return 0;
    for (int i = 0; i < 64; ++i) {
        if (!isxdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

static void handle_free(acs_artifact_handle_v1* h) {
    if (!h) return;
    free(h->manifest_schema);
    free(h->artifact_id);
    free(h->type_id);
    free(h->storage_uri);
    free(h->content_digest_hex);
    free(h->producer_module_id);
    free(h->producer_build_id);
    free(h->run_id);
    free(h->phase);
    free(h->node_id);
    free(h->config_digest_hex);
    free(h->created_utc);
    if (h->input_artifact_ids) {
        for (size_t i = 0; i < h->input_count; ++i) free(h->input_artifact_ids[i]);
        free(h->input_artifact_ids);
    }
    if (h->input_digests) {
        for (size_t i = 0; i < h->input_count; ++i) free(h->input_digests[i]);
        free(h->input_digests);
    }
    free(h);
}

int acs_artifact_manifest_parse_v1(const char* json_utf8, size_t json_bytes,
                                   acs_artifact_handle_v1** out,
                                   char* err, size_t err_cap) {
    if (!json_utf8 || !out) return 1; /* ACS_ERR_PARAM */
    if (err && err_cap) err[0] = '\0';
    const char* p = json_utf8;
    const char* end = json_utf8 + json_bytes;
    p = skip_ws(p, end);
    if (p >= end || *p != '{') {
        if (err && err_cap) snprintf(err, err_cap, "not an object");
        return 1;
    }

    acs_artifact_handle_v1* h = (acs_artifact_handle_v1*)calloc(1, sizeof(*h));
    if (!h) return 3; /* ACS_ERR_NOMEM */
    h->status = -1;

    char* s = NULL;
    uint64_t u = 0;
    int has_str = 0, has_uint = 0;

    /* manifest_schema (const 校验) */
    if (!find_key_deep(p, end, "manifest_schema", &s, NULL, &has_str, NULL) || !has_str ||
        strcmp(s, "astrocs.artifact-manifest/v1") != 0) {
        if (err && err_cap) snprintf(err, err_cap, "missing/bad manifest_schema");
        free(s); handle_free(h); return 1;
    }
    h->manifest_schema = s;

    if (!find_key_deep(p, end, "artifact_id", &s, NULL, &has_str, NULL) || !has_str || !*s) {
        if (err && err_cap) snprintf(err, err_cap, "missing artifact_id");
        free(s); handle_free(h); return 1;
    }
    h->artifact_id = s;

    if (!find_key_deep(p, end, "type_id", &s, NULL, &has_str, NULL) || !has_str || !*s) {
        if (err && err_cap) snprintf(err, err_cap, "missing type_id");
        free(s); handle_free(h); return 1;
    }
    h->type_id = s;

    if (!find_key_deep(p, end, "schema_version", NULL, &u, NULL, &has_uint) || !has_uint || u == 0 || u > UINT32_MAX) {
        if (err && err_cap) snprintf(err, err_cap, "missing/bad schema_version");
        handle_free(h); return 1;
    }
    h->schema_version = (uint32_t)u;

    if (!find_key_deep(p, end, "storage_uri", &s, NULL, &has_str, NULL) || !has_str || !*s) {
        if (err && err_cap) snprintf(err, err_cap, "missing storage_uri");
        free(s); handle_free(h); return 1;
    }
    h->storage_uri = s;

    /* content_digest: 子对象 {algorithm, hex} */
    {
        const char* vs = NULL; size_t vl = 0;
        if (!locate_value(p, end, "content_digest", &vs, &vl)) {
            if (err && err_cap) snprintf(err, err_cap, "missing content_digest");
            handle_free(h); return 1;
        }
        char* alg = NULL; char* hex = NULL;
        int hs = 0, hu = 0;
        uint64_t dummy = 0;
        if (!extract_field_from_obj(vs, vs + vl, "algorithm", &alg, &dummy, &hs, &hu) || !hs ||
            strcmp(alg, "sha256") != 0) {
            if (err && err_cap) snprintf(err, err_cap, "content_digest.algorithm must be sha256");
            free(alg); free(hex); handle_free(h); return 1;
        }
        free(alg);
        if (!extract_field_from_obj(vs, vs + vl, "hex", &hex, &dummy, &hs, &hu) || !hs ||
            !hex64_ok(hex)) {
            if (err && err_cap) snprintf(err, err_cap, "content_digest.hex must be 64 hex");
            free(hex); handle_free(h); return 1;
        }
        h->content_digest_hex = hex;
    }

    /* size: 非负整数 */
    if (!find_key_deep(p, end, "size", NULL, &u, NULL, &has_uint) || !has_uint) {
        if (err && err_cap) snprintf(err, err_cap, "missing/bad size");
        handle_free(h); return 1;
    }
    h->size = u;

    /* producer: 子对象; module_id 重复 → 拒绝 (本 manifest 只允许一个 producer) */
    {
        const char* vs = NULL; size_t vl = 0;
        if (!locate_value(p, end, "producer", &vs, &vl)) {
            if (err && err_cap) snprintf(err, err_cap, "missing producer");
            handle_free(h); return 1;
        }
        char* mid = NULL; char* bid = NULL;
        int hs = 0, hu = 0; uint64_t dummy = 0;
        if (!extract_field_from_obj(vs, vs + vl, "module_id", &mid, &dummy, &hs, &hu) || !hs || !*mid) {
            if (err && err_cap) snprintf(err, err_cap, "producer.module_id required");
            free(mid); free(bid); handle_free(h); return 1;
        }
        if (!extract_field_from_obj(vs, vs + vl, "module_build_id", &bid, &dummy, &hs, &hu) || !hs || !*bid) {
            if (err && err_cap) snprintf(err, err_cap, "producer.module_build_id required");
            free(mid); free(bid); handle_free(h); return 1;
        }
        /* 重复 producer 检查: 本结构只有一个 module_id 字段; JSON 内重复 key 会被 find_key
           顺序查找取首个; 额外整对象级重复 key 检测在 parse 层下方进行 (见 duplicated-key 扫描) */
        h->producer_module_id = mid;
        h->producer_build_id = bid;
    }

    /* run: 子对象 {run_id, phase} */
    {
        const char* vs = NULL; size_t vl = 0;
        if (!locate_value(p, end, "run", &vs, &vl)) {
            if (err && err_cap) snprintf(err, err_cap, "missing run");
            handle_free(h); return 1;
        }
        char* rid = NULL; char* ph = NULL;
        int hs = 0, hu = 0; uint64_t dummy = 0;
        if (!extract_field_from_obj(vs, vs + vl, "run_id", &rid, &dummy, &hs, &hu) || !hs || !*rid) {
            if (err && err_cap) snprintf(err, err_cap, "run.run_id required");
            free(rid); free(ph); handle_free(h); return 1;
        }
        if (!extract_field_from_obj(vs, vs + vl, "phase", &ph, &dummy, &hs, &hu) || !hs) {
            if (err && err_cap) snprintf(err, err_cap, "run.phase required");
            free(rid); free(ph); handle_free(h); return 1;
        }
        if (strcmp(ph, "phase1") != 0 && strcmp(ph, "phase2") != 0 && strcmp(ph, "phase3") != 0) {
            if (err && err_cap) snprintf(err, err_cap, "run.phase invalid: %s", ph);
            free(rid); free(ph); handle_free(h); return 1;
        }
        h->run_id = rid;
        h->phase = ph;
    }

    /* node: 子对象 {node_id} */
    {
        const char* vs = NULL; size_t vl = 0;
        if (!locate_value(p, end, "node", &vs, &vl)) {
            if (err && err_cap) snprintf(err, err_cap, "missing node");
            handle_free(h); return 1;
        }
        char* nid = NULL;
        int hs = 0, hu = 0; uint64_t dummy = 0;
        if (!extract_field_from_obj(vs, vs + vl, "node_id", &nid, &dummy, &hs, &hu) || !hs || !*nid) {
            if (err && err_cap) snprintf(err, err_cap, "node.node_id required");
            free(nid); handle_free(h); return 1;
        }
        h->node_id = nid;
    }

    /* config_digest: 子对象 {algorithm, hex} */
    {
        const char* vs = NULL; size_t vl = 0;
        if (!locate_value(p, end, "config_digest", &vs, &vl)) {
            if (err && err_cap) snprintf(err, err_cap, "missing config_digest");
            handle_free(h); return 1;
        }
        char* alg = NULL; char* hex = NULL;
        int hs = 0, hu = 0; uint64_t dummy = 0;
        if (!extract_field_from_obj(vs, vs + vl, "algorithm", &alg, &dummy, &hs, &hu) || !hs ||
            strcmp(alg, "sha256") != 0) {
            if (err && err_cap) snprintf(err, err_cap, "config_digest.algorithm must be sha256");
            free(alg); free(hex); handle_free(h); return 1;
        }
        free(alg);
        if (!extract_field_from_obj(vs, vs + vl, "hex", &hex, &dummy, &hs, &hu) || !hs ||
            !hex64_ok(hex)) {
            if (err && err_cap) snprintf(err, err_cap, "config_digest.hex must be 64 hex");
            free(hex); handle_free(h); return 1;
        }
        h->config_digest_hex = hex;
    }

    /* status: 字符串枚举 */
    {
        char* st = NULL;
        int hs = 0;
        if (!find_key_deep(p, end, "status", &st, NULL, &hs, NULL) || !hs) {
            if (err && err_cap) snprintf(err, err_cap, "missing status");
            free(st); handle_free(h); return 1;
        }
        if (strcmp(st, "COMPLETE") == 0) h->status = ACS_ART_STATUS_COMPLETE;
        else if (strcmp(st, "INCOMPLETE") == 0) h->status = ACS_ART_STATUS_INCOMPLETE;
        else if (strcmp(st, "FAILED") == 0) h->status = ACS_ART_STATUS_FAILED;
        else if (strcmp(st, "CANCELLED") == 0) h->status = ACS_ART_STATUS_CANCELLED;
        else if (strcmp(st, "PENDING") == 0) h->status = ACS_ART_STATUS_PENDING;
        else {
            if (err && err_cap) snprintf(err, err_cap, "status invalid: %s", st);
            free(st); handle_free(h); return 1;
        }
        free(st);
    }

    /* created_utc: 字符串 */
    if (!find_key_deep(p, end, "created_utc", &s, NULL, &has_str, NULL) || !has_str || !*s) {
        if (err && err_cap) snprintf(err, err_cap, "missing created_utc");
        free(s); handle_free(h); return 1;
    }
    h->created_utc = s;

    /* input_digests: 数组 (可空); 逐项 {artifact_id, digest}, artifact_id 重复拒绝 */
    {
        const char* vs = NULL; size_t vl = 0;
        if (!locate_value(p, end, "input_digests", &vs, &vl)) {
            if (err && err_cap) snprintf(err, err_cap, "missing input_digests");
            handle_free(h); return 1;
        }
        const char* q = vs;
        const char* ae = vs + vl;
        if (q >= ae || *q != '[') {
            if (err && err_cap) snprintf(err, err_cap, "input_digests must be array");
            handle_free(h); return 1;
        }
        ++q;
        /* 计数 */
        size_t cnt = 0;
        {
            const char* r = q;
            int depth = 0;
            while (r < ae) {
                if (*r == '[') ++depth;
                else if (*r == ']') { if (depth == 0) break; --depth; }
                else if (*r == '{' && depth == 1) ++cnt;
                ++r;
            }
        }
        h->input_count = cnt;
        if (cnt > 0) {
            h->input_artifact_ids = (char**)calloc(cnt, sizeof(char*));
            h->input_digests = (char**)calloc(cnt, sizeof(char*));
            if (!h->input_artifact_ids || !h->input_digests) { handle_free(h); return 3; }
            size_t idx = 0;
            const char* r = q;
            while (r < ae && idx < cnt) {
                while (r < ae && *r != '{') ++r;
                if (r >= ae) break;
                const char* obj_end = r;
                int depth = 0;
                while (obj_end < ae) {
                    if (*obj_end == '{') ++depth;
                    else if (*obj_end == '}') { --depth; if (depth == 0) { ++obj_end; break; } }
                    ++obj_end;
                }
                char* aid = NULL; char* dg = NULL;
                int hs = 0, hu = 0; uint64_t dummy = 0;
                if (!extract_field_from_obj(r, obj_end, "artifact_id", &aid, &dummy, &hs, &hu) || !hs || !*aid) {
                    free(aid); free(dg); handle_free(h); return 1;
                }
                if (!extract_field_from_obj(r, obj_end, "digest", &dg, &dummy, &hs, &hu) || !hs || !hex64_ok(dg)) {
                    free(aid); free(dg); handle_free(h); return 1;
                }
                /* 重复 artifact_id 拒绝 */
                for (size_t j = 0; j < idx; ++j) {
                    if (strcmp(h->input_artifact_ids[j], aid) == 0) {
                        if (err && err_cap) snprintf(err, err_cap, "duplicate input artifact_id: %s", aid);
                        free(aid); free(dg); handle_free(h); return 1;
                    }
                }
                h->input_artifact_ids[idx] = aid;
                h->input_digests[idx] = dg;
                ++idx;
                r = obj_end;
            }
        }
    }

    *out = h;
    return 0; /* ACS_OK */
}

void acs_artifact_handle_destroy_v1(acs_artifact_handle_v1* h) { handle_free(h); }

/* ───────────── 查询器 ───────────── */

const char* acs_artifact_query_manifest_schema_v1(const acs_artifact_handle_v1* h) {
    return h ? h->manifest_schema : NULL;
}
const char* acs_artifact_query_artifact_id_v1(const acs_artifact_handle_v1* h) {
    return h ? h->artifact_id : NULL;
}
const char* acs_artifact_query_type_id_v1(const acs_artifact_handle_v1* h) {
    return h ? h->type_id : NULL;
}
uint32_t acs_artifact_query_schema_version_v1(const acs_artifact_handle_v1* h) {
    return h ? h->schema_version : 0;
}
const char* acs_artifact_query_storage_uri_v1(const acs_artifact_handle_v1* h) {
    return h ? h->storage_uri : NULL;
}
const char* acs_artifact_query_content_digest_hex_v1(const acs_artifact_handle_v1* h) {
    return h ? h->content_digest_hex : NULL;
}
uint64_t acs_artifact_query_size_v1(const acs_artifact_handle_v1* h) { return h ? h->size : 0; }
const char* acs_artifact_query_producer_module_id_v1(const acs_artifact_handle_v1* h) {
    return h ? h->producer_module_id : NULL;
}
const char* acs_artifact_query_producer_build_id_v1(const acs_artifact_handle_v1* h) {
    return h ? h->producer_build_id : NULL;
}
const char* acs_artifact_query_run_id_v1(const acs_artifact_handle_v1* h) { return h ? h->run_id : NULL; }
const char* acs_artifact_query_phase_v1(const acs_artifact_handle_v1* h) { return h ? h->phase : NULL; }
const char* acs_artifact_query_node_id_v1(const acs_artifact_handle_v1* h) { return h ? h->node_id : NULL; }
const char* acs_artifact_query_config_digest_hex_v1(const acs_artifact_handle_v1* h) {
    return h ? h->config_digest_hex : NULL;
}
const char* acs_artifact_query_created_utc_v1(const acs_artifact_handle_v1* h) {
    return h ? h->created_utc : NULL;
}
acs_artifact_status acs_artifact_query_status_v1(const acs_artifact_handle_v1* h) {
    return h && h->status >= 0 ? (acs_artifact_status)h->status : ACS_ART_STATUS_PENDING;
}
size_t acs_artifact_query_input_count_v1(const acs_artifact_handle_v1* h) {
    return h ? h->input_count : 0;
}
const char* acs_artifact_query_input_artifact_id_v1(const acs_artifact_handle_v1* h, size_t i) {
    if (!h || i >= h->input_count) return NULL;
    return h->input_artifact_ids[i];
}
const char* acs_artifact_query_input_digest_v1(const acs_artifact_handle_v1* h, size_t i) {
    if (!h || i >= h->input_count) return NULL;
    return h->input_digests[i];
}

const char* acs_artifact_status_name_v1(acs_artifact_status s) {
    switch (s) {
        case ACS_ART_STATUS_COMPLETE: return "COMPLETE";
        case ACS_ART_STATUS_INCOMPLETE: return "INCOMPLETE";
        case ACS_ART_STATUS_FAILED: return "FAILED";
        case ACS_ART_STATUS_CANCELLED: return "CANCELLED";
        case ACS_ART_STATUS_PENDING: return "PENDING";
        default: return NULL;
    }
}
