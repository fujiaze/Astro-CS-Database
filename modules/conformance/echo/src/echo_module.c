/* AstroCS echo conformance module — modules/conformance/echo/src/echo_module.c
 *
 * 状态: ABI-005 conformance (跨平台契约探针, 无科学含义)。
 * 角色: 每个 host callback (artifact read/manifest query、host allocator、
 * logger、cancel、executor lease、config query) 都有正/负行为可被 host 侧
 * 测试驱动; artifact "write" 在 ABI v1 冻结面无写回调 → 模块请求写通道如实
 * 返回 ACS_ERR_UNSUPPORTED (不绕过 host 自开文件, FORBID-002)。metrics 在
 * host_api_v1 无回调字段 (仅 ACS_METRIC_* 枚举冻结) → 模块以 inspect 输出
 * 内部计数 (exec_count/bytes_count) 作上报形态。
 *
 * 生命周期语义对齐 ABI-002 (lifecycle_v1.h): 同实例 execute 并发 → STATE;
 * request_cancel 幂等置位 + host cancel 任一命中 → execute 返回 CANCELLED;
 * 输出 JSON 缓冲不足 → PARAM + BUFFER_TOO_SMALL (strbuf.size=所需)。
 *
 * 编译合同: 纯 C11; 无 STL/异常/RTTI; -fno-exceptions 亦可编译。
 * 导出可见性: ASTROCS_ABI_SHARED 由 target 编译期定义; 构建 DLL 本体时
 * 需 ASTROCS_ABI_EXPORTS (由 CMake target 编译定义) → dllexport/visibility。
 * 唯一导出 astrocs_module_query_v1 (12 §1 / ARC-001 §1.1; ABI-006 全查 exports)。
 */
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/echo/types.h"

#include <stdio.h>
#include <string.h>

#if defined(ASTROCS_ABI_SHARED) && !defined(ASTROCS_ABI_EXPORTS)
#define ASTROCS_ABI_EXPORTS 1
#endif

/* ───────── 静态字符串 (descriptor 输出; 所有权=module 静态, 调用方不得 free) ───────── */

static const char kModuleId[] = ASTROCS_ECHO_MODULE_ID;
static const char kVersion[]  = ASTROCS_ECHO_MODULE_VERSION;
static const char kBuildId[]  = ASTROCS_ECHO_BUILD_ID;
static const char kSciId[]    = "SCI-NONE";   /* conformance: 无科学合同 */
static const char kAlgId[]    = "ALG-NONE";   /* conformance: 无算法合同 */
static const char kApiId[]    = "API-ABI-001";/* module C ABI v1 (ABI-001) */

static const char kLogComponent[] = "astrocs.conformance.echo";

/* ───────── 基础 helper ───────── */

static acs_str_v1 acs_str_from(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)strlen(s) : 0;
    return v;
}

static int str_eq_n(const char* a, const char* b, uint64_t n) {
    if (n != strlen(b)) return 0;
    return memcmp(a, b, (size_t)n) == 0;
}

/* 错误填充 (message 恒为编译期静态字面量; 无路径/sha/内容) */
static void efill(acs_error_info_v1* err, acs_status st, int32_t domain,
                  uint32_t detail, const char* msg) {
    if (!err) return;
    memset(err, 0, sizeof(*err));
    err->head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    err->head.abi_version = ACS_ABI_VERSION_V1;
    err->status = st;
    err->domain = domain;
    err->message_utf8 = msg;
    err->message_bytes = msg ? (uint32_t)strlen(msg) : 0;
    err->detail_code = detail;
}

/* strbuf 写 N 字节 (lifecycle_v1.h 截断语义: size=所需; cap>0 写前缀+NUL;
 * 不足 → PARAM + BUFFER_TOO_SMALL) */
static acs_status strbuf_write(acs_strbuf_v1* out, const char* data, uint64_t n,
                               acs_error_info_v1* err) {
    if (!out) { efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                      ACS_DIAG_ECODE_NULL_CALLBACK, "echo: null output buffer");
                return ACS_ERR_PARAM; }
    out->size = n;
    if (!out->data || out->cap == 0) return ACS_OK;   /* 只问尺寸 */
    uint64_t room = out->cap - 1;                     /* 留 NUL 位 */
    uint64_t wr = n < room ? n : room;
    if (wr > 0) memcpy(out->data, data, (size_t)wr);
    out->data[wr] = '\0';
    if (n >= out->cap) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_BUFFER_TOO_SMALL, "echo: output buffer too small");
        return ACS_ERR_PARAM;
    }
    return ACS_OK;
}

static acs_status strbuf_write_cstr(acs_strbuf_v1* out, const char* s,
                                    acs_error_info_v1* err) {
    return strbuf_write(out, s, (uint64_t)strlen(s), err);
}

/* 借入串 (data,size) copy 到栈 NUL 结尾 (config 解析用; 截断>cap-1) */
static uint64_t copy_to_cstr(const char* data, uint64_t size, char* dst, uint64_t cap) {
    if (!data || size == 0) { dst[0] = 0; return 0; }
    uint64_t n = size < cap - 1 ? size : cap - 1;
    memcpy(dst, data, (size_t)n);
    dst[n] = 0;
    return n;
}

/* 实例 (create 经 host allocator 分配; destroy 同一 allocator 释放) */
typedef struct echo_inst_s {
    const acs_allocator_v1* alloc;   /* create 时快照; destroy 用它释放自身 */
    const acs_host_api_v1*  host;    /* host 服务表快照 (只读; 模块不改写) */
    int32_t  phase;                  /* 0=idle 1=executing 2=cancelling */
    int32_t  cancel_flag;            /* request_cancel 置位 (幂等) */
    char     last_action[32];        /* 最近 action (inspect 诊断) */
    uint64_t exec_count;             /* metrics: 完成 execute 次数 */
    uint64_t bytes_count;            /* metrics: 处理字节 */
} echo_inst_s;

/* ───────── config JSON 最小解析 (无第三方; 仅测试自产简单 JSON) ───────── */

/* 在 [js,js+len) 内找 "key": 精确 (带引号) 匹配; 返回 ':' 后值起点 */
static const char* json_key_value(const char* js, uint64_t len, const char* key,
                                  uint64_t* out_vlen) {
    uint64_t kl = (uint64_t)strlen(key);
    uint64_t i;
    for (i = 0; i + 2 + kl <= len; ++i) {
        if (js[i] == '"' && memcmp(js + i + 1, key, (size_t)kl) == 0 &&
            i + 2 + kl <= len && js[i + 1 + kl] == '"') {
            uint64_t p = i + 2 + kl;
            while (p < len && (js[p] == ' ' || js[p] == '\t' || js[p] == '\n' ||
                               js[p] == '\r')) p++;
            if (p < len && js[p] == ':') {
                p++;
                while (p < len && (js[p] == ' ' || js[p] == '\t' || js[p] == '\n' ||
                                   js[p] == '\r')) p++;
                *out_vlen = len - p;
                return js + p;
            }
        }
    }
    *out_vlen = 0;
    return NULL;
}

/* 提取字符串值 → dst (NUL 结尾); 1=命中
 * 内容 = 两端引号之间字节 (end-1; end 停在闭引号或 span 尾); 截断>cap-1 前缀保留。 */
static int json_get_str(const char* js, uint64_t len, const char* key,
                        char* dst, uint64_t cap) {
    uint64_t vlen = 0;
    const char* v = json_key_value(js, len, key, &vlen);
    dst[0] = 0;
    if (!v || vlen == 0 || v[0] != '"') return 0;
    uint64_t end = 1;
    while (end < vlen && v[end] != '"') end++;
    uint64_t content = end - 1;             /* 引号间字节数 */
    uint64_t n = content < cap - 1 ? content : cap - 1;
    if (n > 0) memcpy(dst, v + 1, (size_t)n);
    dst[n] = 0;
    return 1;
}

/* 提取整数/nulll 值 → *out; 返回 1=命中 */
static int json_get_u64(const char* js, uint64_t len, const char* key,
                        uint64_t* out) {
    uint64_t vlen = 0;
    const char* v = json_key_value(js, len, key, &vlen);
    *out = 0;
    if (!v || vlen == 0) return 0;
    if (v[0] == 'n' && vlen >= 4 && memcmp(v, "null", 4) == 0) return 0;
    uint64_t val = 0, i = 0;
    if (i < vlen && v[i] == '"') {   /* 数字在测试侧也可能带引号: 宽容 */
        i++;
        while (i < vlen && v[i] != '"') {
            if (v[i] < '0' || v[i] > '9') return 0;
            val = val * 10 + (uint64_t)(v[i] - '0');
            i++;
        }
        *out = val;
        return 1;
    }
    while (i < vlen && v[i] >= '0' && v[i] <= '9') {
        val = val * 10 + (uint64_t)(v[i] - '0');
        i++;
    }
    if (i == 0) return 0;
    *out = val;
    return 1;
}

/* 从 config 提取 action; 空/缺失 → 默认 echo */
static void cfg_action(const char* js, uint64_t len, char* out, uint64_t cap) {
    if (!json_get_str(js, len, ASTROCS_ECHO_CFG_KEY_ACTION, out, cap)) {
        copy_to_cstr(ASTROCS_ECHO_CFG_ACTION_ECHO,
                     (uint64_t)strlen(ASTROCS_ECHO_CFG_ACTION_ECHO), out, cap);
    }
}

/* action 白名单 */
static int action_valid(const char* a) {
    return strcmp(a, ASTROCS_ECHO_CFG_ACTION_ECHO) == 0 ||
           strcmp(a, ASTROCS_ECHO_CFG_ACTION_ARTIFACT_READ) == 0 ||
           strcmp(a, ASTROCS_ECHO_CFG_ACTION_WRITE_BLOCKED) == 0 ||
           strcmp(a, ASTROCS_ECHO_CFG_ACTION_CANCEL_POLL) == 0 ||
           strcmp(a, ASTROCS_ECHO_CFG_ACTION_LEASE) == 0 ||
           strcmp(a, ASTROCS_ECHO_CFG_ACTION_ERROR) == 0 ||
           strcmp(a, "config_query") == 0;
}

/* 取消检查点: 实例 request_cancel 置位 或 host cancel 命中 → CANCELLED */
static acs_status cancel_point(echo_inst_s* inst, acs_error_info_v1* err) {
    const acs_cancel_v1* c = inst->host ? inst->host->cancel : NULL;
    int hit = inst->cancel_flag != 0;
    if (!hit && c && c->is_cancelled && c->is_cancelled(c->user_data)) hit = 1;
    if (hit) {
        inst->phase = 0;
        efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED, 0,
              "echo: cancelled at safety point");
        return ACS_ERR_CANCELLED;
    }
    return ACS_OK;
}

/* ───────── 生命周期 vtable 实现 ───────── */

static acs_status echo_describe(const acs_module_api_v1* self,
                                acs_str_v1 module_id,
                                acs_module_descriptor_v1* out_desc) {
    (void)self;
    if (!out_desc) return ACS_ERR_PARAM;
    memset(out_desc, 0, sizeof(*out_desc));
    out_desc->head.struct_size = (uint32_t)sizeof(acs_module_descriptor_v1);
    out_desc->head.abi_version = ACS_ABI_VERSION_V1;
    out_desc->module_id  = acs_str_from(kModuleId);
    out_desc->version    = acs_str_from(kVersion);
    out_desc->build_id   = acs_str_from(kBuildId);
    out_desc->sci_id     = acs_str_from(kSciId);
    out_desc->alg_id     = acs_str_from(kAlgId);
    out_desc->api_id     = acs_str_from(kApiId);
    out_desc->phase             = 0;   /* 平台/conformance (module.yaml registry 定义) */
    out_desc->config_schema_ver = 1;
    out_desc->execution_class   = 2;   /* metadata (与 module_api_v1.h enum 注释一致) */
    out_desc->parallel_ok       = 1;   /* echo 无内部共享状态竞争 (host 可并发实例) */
    out_desc->flags             = 0;
    if (module_id.data != NULL && module_id.size > 0) {
        if (!str_eq_n(module_id.data, kModuleId, module_id.size)) return ACS_ERR_PARAM;
    }
    return ACS_OK;
}

static acs_status echo_validate_config(const acs_module_api_v1* self,
                                       acs_str_v1 config_json,
                                       acs_error_info_v1* err) {
    (void)self;
    if (config_json.data == NULL || config_json.size == 0) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CONFIG, "echo: empty config");
        return ACS_ERR_PARAM;
    }
    char cfg[2048];
    copy_to_cstr(config_json.data, config_json.size, cfg, sizeof(cfg));
    char act[32];
    cfg_action(cfg, (uint64_t)strlen(cfg), act, sizeof(act));
    if (!action_valid(act)) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_CONFIG_SCHEMA, "echo: unknown config action");
        return ACS_ERR_PARAM;
    }
    /* request_error 需 error_status 数值 (>=1 稳定码) */
    if (strcmp(act, ASTROCS_ECHO_CFG_ACTION_ERROR) == 0) {
        uint64_t es = 0;
        if (!json_get_u64(cfg, (uint64_t)strlen(cfg),
                          ASTROCS_ECHO_CFG_KEY_ERROR_STATUS, &es) || es == 0 ||
            es > 70) {
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  ACS_DIAG_ECODE_CONFIG_SCHEMA, "echo: invalid error_status");
            return ACS_ERR_PARAM;
        }
    }
    return ACS_OK;
}

static acs_status echo_plan(const acs_module_api_v1* self,
                            acs_str_v1 node_id,
                            acs_str_v1 config_json,
                            acs_strbuf_v1* out_plan_json,
                            acs_error_info_v1* err) {
    (void)self;
    char cfg[2048];
    copy_to_cstr(config_json.data ? config_json.data : "",
                 config_json.size, cfg, sizeof(cfg));
    char act[32];
    cfg_action(cfg, (uint64_t)strlen(cfg), act, sizeof(act));
    if (!action_valid(act)) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_CONFIG_SCHEMA, "echo: unknown config action");
        return ACS_ERR_PARAM;
    }
    char nodebuf[128];
    copy_to_cstr(node_id.data ? node_id.data : "", node_id.size,
                 nodebuf, sizeof(nodebuf));
    char out[512];
    snprintf(out, sizeof(out),
             "{\"module_id\":\"%s\",\"action\":\"%s\",\"node_id\":\"%s\"}",
             kModuleId, act, nodebuf);
    return strbuf_write_cstr(out_plan_json, out, err);
}

static acs_status echo_create(const acs_module_api_v1* self,
                              acs_str_v1 config_json,
                              const acs_host_api_v1* host,
                              acs_module_instance_v1** out,
                              acs_error_info_v1* err) {
    (void)self; (void)config_json;
    if (out) *out = NULL;
    if (!host || !host->allocator) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "echo: host allocator missing");
        return ACS_ERR_PARAM;
    }
    echo_inst_s* inst = (echo_inst_s*)host->allocator->alloc(
        host->allocator->user_data, sizeof(echo_inst_s), 8u);
    if (!inst) {
        efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE, 0,
              "echo: instance alloc failed");
        return ACS_ERR_NOMEM;
    }
    memset(inst, 0, sizeof(*inst));
    inst->alloc = host->allocator;
    inst->host = host;
    inst->phase = 0;
    if (out) *out = (acs_module_instance_v1*)inst;
    return ACS_OK;
}

/* artifact read 动作: host.artifacts 判空负测 + open/read_all/query/parse 正测 */
static acs_status act_artifact_read(echo_inst_s* inst, const char* cfg,
                                    acs_strbuf_v1* out, acs_error_info_v1* err) {
    const acs_artifact_service_v1* ar = inst->host ? inst->host->artifacts : NULL;
    if (!ar) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              ACS_ECHO_ECODE_ARTIFACT_MISSING, "echo: host artifact service missing");
        return ACS_ERR_PARAM;
    }
    char uri[1024];
    if (!json_get_str(cfg, (uint64_t)strlen(cfg), ASTROCS_ECHO_CFG_KEY_STORAGE_URI,
                      uri, sizeof(uri)) || !uri[0]) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_CONFIG_SCHEMA, "echo: storage_uri missing");
        return ACS_ERR_PARAM;
    }
    acs_error_info_v1 aerr;
    memset(&aerr, 0, sizeof(aerr));
    acs_artifact_handle_v1* h = NULL;
    acs_status st = ar->artifact_open(ar, acs_str_from(uri), acs_str_from(""), &h, &aerr);
    if (st != ACS_OK) {
        efill(err, st, ACS_ERR_DOMAIN_IO, 0, "echo: artifact open failed");
        return st;
    }
    if (!h) {
        efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO, 0, "echo: artifact open null");
        return ACS_ERR_IO;
    }
    /* 查询声明字段 (type_id 经 manifest 内容校验; digest/size 直接上报) */
    acs_str_v1 digest = ar->artifact_query_content_digest_hex(h);
    uint64_t asize = ar->artifact_query_size(h);
    /* 读全部 (host allocator 分配; 模块同一 allocator free) */
    acs_span_u8 data;
    memset(&data, 0, sizeof(data));
    acs_error_info_v1 rerr;
    memset(&rerr, 0, sizeof(rerr));
    st = ar->artifact_read_all(h, inst->alloc, &data, &rerr);
    if (st != ACS_OK) {
        ar->artifact_close(h);
        efill(err, st, ACS_ERR_DOMAIN_IO, 0, "echo: artifact read failed");
        return st;
    }
    inst->bytes_count += data.count;
    /* manifest 解析 + 顶层字段查询 (内容须为 JSON; echo fixture 用 manifest) */
    char field_val[256] = "";
    uint64_t field_num = 0;
    int has_field = 0;
    if (data.data && data.count > 0) {
        acs_manifest_handle_v1* mh = NULL;
        char* jbuf = (char*)inst->alloc->alloc(inst->alloc->user_data,
                                               data.count + 1u, 1u);
        if (jbuf) {
            memcpy(jbuf, data.data, (size_t)data.count);
            jbuf[data.count] = 0;
            acs_error_info_v1 perr;
            memset(&perr, 0, sizeof(perr));
            if (ar->manifest_parse(ar, acs_str_from(jbuf), &mh, &perr) == ACS_OK &&
                mh) {
                acs_str_v1 v;
                if (ar->manifest_get_str(mh, acs_str_from("type_id"), &v) &&
                    v.data && v.size < sizeof(field_val)) {
                    memcpy(field_val, v.data, (size_t)v.size);
                    field_val[v.size] = 0;
                    has_field = 1;
                }
                if (ar->manifest_get_u64(mh, acs_str_from("schema_version"),
                                         &field_num)) {
                }
                ar->manifest_destroy(mh);
            }
            inst->alloc->free(inst->alloc->user_data, jbuf);
        }
    }
    char outb[512];
    snprintf(outb, sizeof(outb),
             "{\"echo\":\"artifact_read\",\"type_id\":\"%s\",\"bytes\":%llu,"
             "\"digest_prefix\":\"%.*s\",\"has_field\":%d,\"schema_version\":%llu}",
             has_field ? field_val : "",
             (unsigned long long)asize,
             (int)(digest.size > 12 ? 12 : digest.size),
             digest.data ? digest.data : "",
             has_field, (unsigned long long)field_num);
    if (data.data) inst->alloc->free(inst->alloc->user_data, data.data);
    ar->artifact_close(h);
    return strbuf_write_cstr(out, outb, err);
}

/* execute 主实现 */
static acs_status echo_execute(acs_module_instance_v1* self,
                               acs_str_v1 input_manifest_json,
                               acs_str_v1 config_json,
                               acs_strbuf_v1* out_manifest_json,
                               acs_error_info_v1* err) {
    echo_inst_s* inst = (echo_inst_s*)self;
    if (!inst || !inst->host) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "echo: null instance");
        return ACS_ERR_PARAM;
    }
    /* 同实例并发 execute 拒绝 (ABI-002) */
    if (inst->phase != 0) {
        efill(err, ACS_ERR_STATE, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_ILLEGAL_STATE, "echo: instance busy");
        return ACS_ERR_STATE;
    }
    inst->phase = 1;
    acs_status rc = ACS_OK;

    char cfg[2048];
    copy_to_cstr(config_json.data ? config_json.data : "",
                 config_json.size, cfg, sizeof(cfg));
    char act[32];
    cfg_action(cfg, (uint64_t)strlen(cfg), act, sizeof(act));
    copy_to_cstr(act, (uint64_t)strlen(act), inst->last_action,
                 sizeof(inst->last_action));

    const acs_host_api_v1* host = inst->host;

    if (strcmp(act, ASTROCS_ECHO_CFG_ACTION_ECHO) == 0) {
        /* 回显 input manifest (正测: 内容往返一致; cancel 前置检查点) */
        rc = cancel_point(inst, err);
        if (rc == ACS_OK) {
            rc = strbuf_write(out_manifest_json,
                              input_manifest_json.data ? input_manifest_json.data : "",
                              input_manifest_json.size, err);
            inst->bytes_count += input_manifest_json.size;
        }
    } else if (strcmp(act, ASTROCS_ECHO_CFG_ACTION_ARTIFACT_READ) == 0) {
        rc = act_artifact_read(inst, cfg, out_manifest_json, err);
    } else if (strcmp(act, ASTROCS_ECHO_CFG_ACTION_WRITE_BLOCKED) == 0) {
        /* ABI v1 host 无 artifact 写回调: 如实拒绝, 绝不自行开文件 (FORBID-002) */
        if (host->logger && host->logger->log) {
            host->logger->log(host->logger->user_data, ACS_LOG_WARN,
                              acs_str_from(kLogComponent),
                              acs_str_from("echo: artifact write not supported by host ABI v1"));
        }
        efill(err, ACS_ERR_UNSUPPORTED, ACS_ERR_DOMAIN_DATA,
              ACS_ECHO_ECODE_ARTIFACT_WRITE_UNSUPPORTED,
              "echo: artifact write unsupported");
        rc = ACS_ERR_UNSUPPORTED;
    } else if (strcmp(act, ASTROCS_ECHO_CFG_ACTION_CANCEL_POLL) == 0) {
        const acs_cancel_v1* c = host->cancel;
        if (!c || !c->is_cancelled) {
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  ACS_ECHO_ECODE_CANCEL_MISSING, "echo: host cancel missing");
            rc = ACS_ERR_PARAM;
        } else {
            /* 有界轮询: host 置位后首个安全点返回 CANCELLED */
            int i, cancelled = 0;
            for (i = 0; i < 10000; ++i) {
                if (inst->cancel_flag || c->is_cancelled(c->user_data)) {
                    cancelled = 1;
                    break;
                }
            }
            if (cancelled) {
                efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED, 0,
                      "echo: cancelled");
                rc = ACS_ERR_CANCELLED;
            } else {
                rc = strbuf_write_cstr(out_manifest_json,
                                       "{\"echo\":\"cancel_poll\",\"cancelled\":0}",
                                       err);
            }
        }
    } else if (strcmp(act, ASTROCS_ECHO_CFG_ACTION_LEASE) == 0) {
        const acs_executor_v1* ex = host->executor;
        if (!ex || !ex->acquire || !ex->release) {
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  ACS_ECHO_ECODE_EXECUTOR_MISSING, "echo: host executor missing");
            rc = ACS_ERR_PARAM;
        } else {
            uint64_t nw = 1;
            json_get_u64(cfg, (uint64_t)strlen(cfg), ASTROCS_ECHO_CFG_KEY_WORKERS,
                         &nw);
            if (nw == 0 || nw > ex->max_workers) {
                efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                      ACS_DIAG_ECODE_CONFIG_SCHEMA, "echo: invalid workers");
                rc = ACS_ERR_PARAM;
            } else if (ex->acquire(ex->user_data, (uint32_t)nw) != 0) {
                efill(err, ACS_ERR_BUDGET, ACS_ERR_DOMAIN_RESOURCE, 0,
                      "echo: executor budget denied");
                rc = ACS_ERR_BUDGET;
            } else {
                ex->release(ex->user_data, (uint32_t)nw);   /* 归还 (正测) */
                rc = strbuf_write_cstr(out_manifest_json,
                                       "{\"echo\":\"lease\",\"workers\":"
                                       "\"released\"}", err);
            }
        }
    } else if (strcmp(act, "config_query") == 0) {
        const acs_host_api_v1* h = host;
        if (!h->config_query) {
            efill(err, ACS_ERR_UNSUPPORTED, ACS_ERR_DOMAIN_CONFIG,
                  ACS_DIAG_ECODE_UNSUPPORTED_OP, "echo: host config_query unsupported");
            rc = ACS_ERR_UNSUPPORTED;
        } else {
            char key[128];
            copy_to_cstr("module_id", 9, key, sizeof(key));
            char outb[1024];
            acs_strbuf_v1 sb;
            memset(&sb, 0, sizeof(sb));
            sb.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
            sb.head.abi_version = ACS_ABI_VERSION_V1;
            sb.data = outb;
            sb.cap = sizeof(outb);
            acs_error_info_v1 qerr;
            memset(&qerr, 0, sizeof(qerr));
            rc = h->config_query(h, acs_str_from(kModuleId), acs_str_from(key),
                                 &sb, &qerr);
            if (rc == ACS_OK) {
                rc = strbuf_write_cstr(out_manifest_json, outb, err);
            } else {
                efill(err, rc, ACS_ERR_DOMAIN_CONFIG, 0,
                      "echo: host config_query failed");
            }
        }
    } else if (strcmp(act, ASTROCS_ECHO_CFG_ACTION_ERROR) == 0) {
        uint64_t es = 0;
        json_get_u64(cfg, (uint64_t)strlen(cfg), ASTROCS_ECHO_CFG_KEY_ERROR_STATUS,
                     &es);
        int32_t dom = ACS_ERR_DOMAIN_CONFIG;
        switch ((int)es) {
            case ACS_ERR_IO: dom = ACS_ERR_DOMAIN_IO; break;
            case ACS_ERR_CANCELLED: dom = ACS_ERR_DOMAIN_CANCELLED; break;
            case ACS_ERR_BUDGET: dom = ACS_ERR_DOMAIN_RESOURCE; break;
            default: dom = ACS_ERR_DOMAIN_CONFIG; break;
        }
        efill(err, (acs_status)es, dom, 0, "echo: requested error");
        rc = (acs_status)es;
    } else {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_CONFIG_SCHEMA, "echo: unknown action");
        rc = ACS_ERR_PARAM;
    }

    if (rc == ACS_OK) inst->exec_count++;
    inst->phase = 0;
    return rc;
}

static acs_status echo_inspect(const acs_module_instance_v1* self,
                               acs_strbuf_v1* out_json,
                               acs_error_info_v1* err) {
    const echo_inst_s* inst = (const echo_inst_s*)self;
    if (!inst) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "echo: null instance");
        return ACS_ERR_PARAM;
    }
    char outb[512];
    snprintf(outb, sizeof(outb),
             "{\"module_id\":\"%s\",\"action\":\"%s\",\"host_loaded\":%d,"
             "\"exec_count\":%llu,\"bytes_count\":%llu,\"cancel\":%d}",
             kModuleId, inst->last_action[0] ? inst->last_action : "none",
             inst->host != NULL ? 1 : 0,
             (unsigned long long)inst->exec_count,
             (unsigned long long)inst->bytes_count, inst->cancel_flag);
    return strbuf_write_cstr(out_json, outb, err);
}

static acs_status echo_request_cancel(acs_module_instance_v1* self) {
    echo_inst_s* inst = (echo_inst_s*)self;
    if (!inst) return ACS_ERR_STATE;
    inst->cancel_flag = 1;   /* 幂等置位; execute 在安全点响应 */
    return ACS_OK;
}

static void echo_destroy(acs_module_instance_v1* self) {
    echo_inst_s* inst = (echo_inst_s*)self;
    if (!inst) return;
    const acs_allocator_v1* a = inst->alloc;
    if (a && a->free) a->free(a->user_data, inst);   /* 同一 allocator 释放 (12 §4) */
}

/* 模块静态 vtable: 所有权=module 静态存储 (12 §1); host 只读 */
static const acs_module_api_v1 g_echo_api = {
    { (uint32_t)sizeof(acs_module_api_v1), ACS_ABI_VERSION_V1 },
    echo_describe,
    echo_validate_config,
    echo_plan,
    echo_create,
    echo_execute,
    echo_inspect,
    echo_request_cancel,
    echo_destroy
};

/* ───────── 唯一导出入口 (12 §1) ─────────
 * host_abi 失配 → ACS_ERR_ABI_MISMATCH, 不降级猜测 (ABI-002)。
 * host 必填 allocator (host_api_v1.h: allocator 必填, NULL → query 拒绝)。
 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_module_query_v1(uint32_t host_abi,
                        const acs_host_api_v1* host,
                        const acs_module_api_v1** out_api) {
    if (host_abi != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
    if (!host || !host->allocator) return ACS_ERR_ABI_MISMATCH;
    if (!out_api) return ACS_ERR_PARAM;
    *out_api = &g_echo_api;
    return ACS_OK;
}
