/* AstroCS 模块 C ABI v1 — 生命周期/错误/能力协商语义合同（ABI-002）
 *
 * 文件: include/astrocs/abi/lifecycle_v1.h
 * 依赖: module_api_v1.h（族内单向: lifecycle → module → host → artifact → status）。
 *
 * 角色与权威（ABI-002; 规格 = 控制包 tasks/02_ABI_BUILD_CLI_TASKS.md ABI-002 节 +
 * 12_DLL_ABI_AND_LOADER_STANDARD.md §2/§4 + 11_MODULE_SOURCE_TEST_STANDARD.md §6）:
 *   ABI-001 冻结了类型/入口形状; ABI-002 在其上冻结"语义":
 *     - module 实例与 module 静态表的调用时序（状态机）与非法时序拒绝;
 *     - 同实例/跨实例并发与重复调用规则;
 *     - 稳定错误码/错误域/detail_code 映射与诊断缓冲规则（截断/前缀/NUL）;
 *     - 版本协商语义: ABI major 不符拒绝; struct_size 旧/新尾部扩展兼容规则。
 *   本头是合同权威形态之一; 机器可校验形态见
 *   contracts/config/module_lifecycle_contract.schema.json（同一冻结语义的双形态,
 *   一致性测试见 tests/abi/test_abi002_lifecycle.py）。头文件与 schema 必须同步修改。
 *
 * 顶层调用时序（module 静态表; 12 §2）:
 *   query → describe / validate_config / plan / (self_test) → create
 *         → execute* → inspect → request_cancel(可选) → destroy
 * - query 之前没有任何表函数可调用（out_api 尚不存在）; query 成功后才可获得表。
 * - describe/validate_config/plan/self_test 是 module 级操作: query 成功后任意顺序、
 *   任意次数、可跨线程并发（reentrant=yes, threadsafe=yes, 无内部并行）; 不触发科学
 *   execute 或大 I/O（CLI-003 语义）; 与实例状态机无关。
 * - create 是实例入口: 只需 query 成功即可（host 责任: 最佳顺序为
 *   validate_config → plan → create; 合同不强制 create 前必须 validate/plan,
 *   因为 validate/plan 都是无副作用只读操作, 实例化自身承担配置校验）。
 * - destroy 全部实例后才可卸载模块; module 卸载后表指针失效。
 *
 * 实例状态机（create 返回的 opaque 句柄; 本头枚举 + 判定函数冻结）:
 *
 *   CREATED --execute(合法输入)--> EXECUTING --(完成, 任意状态码)--> CREATED
 *      ^                            |  --request_cancel--> CANCELLING --(execute返回)--> CREATED
 *      |                            v
 *      |                          (execute 期间: 同实例并发 execute / destroy 均拒绝)
 *      +---inspect(任意态, const)---+
 *      +---request_cancel(任意非销毁态, 幂等)---+
 *      +---destroy(仅 CREATED)--> DESTROYED (句柄失效; 此后一切实例调用→ACS_ERR_STATE)
 *
 *   execute 完成（ACS_OK / ACS_ERR_CANCELLED / 其他错误）后实例回 CREATED,
 *   可重复调用 execute（每次 execute 自包含: 输入 manifest/config 借入, 输出为调用方
 *   buffer; 取消后允许重跑; 失败后可重跑或 destroy）。因此 v1 不引入
 *   DONE/FAILED 死态——实例只有 CREATED/EXECUTING/CANCELLING/DESTROYED 四态。
 *
 * 并发规则（与 module_api_v1.h 逐函数注释一致, 此处固化）:
 *   - 同一实例: execute 之间互斥（threadsafe=no, reentrant=no）; inspect 可并发
 *     （const 只读诊断）; request_cancel 可并发（原子置位）; destroy 与其他任何
 *     实例调用互斥且仅允许 CREATED 态。
 *   - 不同实例: create/inspect/execute/destroy 全部可并行（threadsafe=yes,
 *     reentrant=yes）; 模块级操作与实例操作可并行。
 *   - module 不得私建线程池; execute 的内部并行只经 host executor 租借
 *     （acs_executor_v1; 约束 D.3/D.4; FORBID-003）。
 *
 * 版本协商（12 §4: module version / ABI version / data schema version / product
 * version 分开; ABI-001: host_abi 失配 → ACS_ERR_ABI_MISMATCH, 不降级猜测）:
 *   - v1 传输值: astrocs_module_query_v1 / astrocs_provider_query_v1 的 host_abi
 *     参数必须 == ACS_ABI_VERSION_V1（=1）。任何其他值（含未来 major=2 的 host、
 *     minor 漂移、0、垃圾值）一律 → ACS_ERR_ABI_MISMATCH, 不猜布局。
 *   - ACS_ABI_MAJOR_V1/ACS_ABI_MINOR_V1 为 v1 的 major/minor 分量; 从 v2 起
 *     若采用 (major<<16)|minor 复合编码, 必须同时 bump ACS_ABI_VERSION_V1;
 *     本层 v1 保持单一数值 1（ABI-001 冻结互操作值, 不得改）。
 *   - struct_size 尾部扩展兼容（accepted）: 跨边界结构前两字段恒 acs_head;
 *     "对端 struct_size >= 自身编译期 sizeof" 表示对端更新（尾部扩展）: 本方按自身
 *     视图读取并忽略扩展尾字段 → 兼容。判定: acs_struct_ext_ok_v1(peer, self)。
 *     "对端 struct_size < 自身 sizeof" 表示对端太旧缺字段 → 不兼容,
 *     ACS_ERR_ABI_MISMATCH（acs_struct_ext_ok_v1 返回 0）。
 *   - data schema（config_schema_ver / manifest schema）与 ABI 分离; data schema
 *     协商由 host 在 config/plan 期处理（DATA-001/002 schema_version 校验）,
 *     不属本 ABI 层。
 *
 * 错误码/诊断（本头冻结映射表; acs_status 数值见 status_codes.h）:
 *   - 调用时序/状态违例（含 execute 期间 destroy、destroy 后访问、double destroy
 *     检测）→ ACS_ERR_STATE + domain=ACS_ERR_DOMAIN_CONFIG + detail_code
 *     ACS_DIAG_ECODE_ILLEGAL_STATE / ACS_DIAG_ECODE_DOUBLE_DESTROY。
 *   - 空回调/空必填参数（module vtable 缺必填回调、host.allocator==NULL、
 *     create 的 config 空等）→ ACS_ERR_PARAM + detail NULL_CALLBACK/NULL_CONFIG。
 *   - 输出 JSON 缓冲不足 → ACS_ERR_PARAM + detail BUFFER_TOO_SMALL; 且
 *     strbuf.size=所需总字节(不含 NUL), cap>0 时尽力写前缀并写 NUL（acs_strbuf_v1
 *     截断语义, 见下）。
 *   - ABI/handshake 失配 → ACS_ERR_ABI_MISMATCH（域=CONFIG）。
 *   - 取消: execute 在安全点响应 host cancel（本实例 request_cancel 置位与
 *     host 注入的 acs_cancel_v1 任一命中）→ ACS_ERR_CANCELLED + domain CANCELLED。
 *   - self_test 失败 → ACS_ERR_SELFTEST（不得 create/execute）。
 *   - 其余 execute 期错误按实际域（IO/SCIENCE_PRECONDITION/DATA/RESOURCE…）。
 *
 * 诊断缓冲（错误信息稳定规则）:
 *   - 每个可失败函数带可空 acs_error_info_v1* err; err!=NULL 时必须写 status 与
 *     domain; message_utf8 借入（module 静态存储, 错误码级稳定; 所有权=module,
 *     有效至调用返回或所属句柄销毁）; detail_code=模块自定义细分（本头冻结
 *     ACS_DIAG_ECODE_* 通用值, 0=无细分）; message_bytes=字节数(不含 NUL)。
 *   - err.message_utf8 长度上限 ACS_DIAG_MAX_UTF8（4096 字节）; 超限必须前缀截断
 *     （message_bytes=4096, 仍不 NUL 结尾——span 语义）。
 *   - 文本公共格式 UTF-8; Windows 内部路径 UTF-16 由 io.dll 层处理, 模块不得
 *     泄漏原始路径进 err.message（只报 storage_uri/类别）。
 *
 * 静态断言: 本头与族内结构布局一致性 + 版本常量关系。
 */
#ifndef ASTROCS_ABI_LIFECYCLE_V1_H
#define ASTROCS_ABI_LIFECYCLE_V1_H

#include "astrocs/abi/module_api_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════ 1. ABI 版本分量与 host_abi 传输值 ═══════════════ */

/* v1 的 major/minor 分量（只读事实; 从 v2 起复合编码时一并 bump ACS_ABI_VERSION_V1）。
 * ACS_ABI_VERSION_V1（=1u）在 status_codes.h 冻结, 是 host_abi 唯一合法 v1 传输值。 */
#define ACS_ABI_MAJOR_V1 1u
#define ACS_ABI_MINOR_V1 0u

ACS_STATIC_ASSERT(ACS_ABI_MAJOR_V1 == 1u, "ABI v1 major is 1");
ACS_STATIC_ASSERT(ACS_ABI_MINOR_V1 == 0u, "ABI v1 minor is 0");
ACS_STATIC_ASSERT(ACS_ABI_VERSION_V1 == 1u, "ABI v1 wire value is 1 (ABI-001 frozen)");

/* ═══════════════ 2. 实例状态机 ═══════════════ */

/* module 实例生命周期状态（v1 冻结; 值域只增不改语义, 见文件头时序图） */
enum acs_lc_state_v1 {
    ACS_LC_STATE_CREATED = 1,    /* create 成功返回后; 唯一可 destroy 的状态 */
    ACS_LC_STATE_EXECUTING = 2,  /* execute 运行中（同实例串行窗口） */
    ACS_LC_STATE_CANCELLING = 3, /* execute 运行中且 cancel 已请求（等待安全点响应） */
    ACS_LC_STATE_DESTROYED = 4   /* destroy 已返回; 句柄失效; 实例操作全部拒绝 */
};

/* module 级 + 实例级操作 ID（v1 冻结; 供状态机判定/诊断引用） */
enum acs_module_op_v1 {
    ACS_OP_QUERY = 1,             /* module 级: 唯一入口握手（无实例） */
    ACS_OP_DESCRIBE = 2,          /* module 级 */
    ACS_OP_VALIDATE_CONFIG = 3,   /* module 级 */
    ACS_OP_PLAN = 4,              /* module 级 */
    ACS_OP_SELF_TEST = 5,         /* module 级（host 在首次 create 前驱动） */
    ACS_OP_CREATE = 6,            /* module 级 → 产生实例 */
    ACS_OP_EXECUTE = 7,           /* 实例级 */
    ACS_OP_INSPECT = 8,           /* 实例级（const 只读, 任意非销毁态） */
    ACS_OP_REQUEST_CANCEL = 9,    /* 实例级（原子置位, 幂等） */
    ACS_OP_DESTROY = 10           /* 实例级（句柄失效） */
};

/* 实例状态机转移判定（纯函数, 无状态; host 可预检, module 自检同源使用）。
 * 只受理实例级 op（ACS_OP_EXECUTE/INSPECT/REQUEST_CANCEL/DESTROY）: module 级
 * op（describe/validate_config/plan/self_test/create/query）不经实例状态机
 * （无实例态检查, 随时可用; 传入本函数一律按拒绝返回 0, host 不得用本判定
 * 拦截 module 级 op）。
 * 返回 1=允许 / 0=拒绝。拒绝时实例函数应返回 ACS_ERR_STATE
 * （+detail ACS_DIAG_ECODE_ILLEGAL_STATE / ACS_DIAG_ECODE_DOUBLE_DESTROY）。
 * state 非法值（0 或越界）一律返回 0; op 非法值一律返回 0。
 * 转移表（与 contracts/config/module_lifecycle_contract.schema.json
 * transitions[] 完全一致, 一致性由 tests/abi/test_abi002_lifecycle.py 校验）:
 *   CREATED     : execute→EXECUTING; inspect/request_cancel→CREATED(自环); destroy→DESTROYED
 *   EXECUTING   : inspect→EXECUTING; request_cancel→CANCELLING
 *   CANCELLING  : inspect→CANCELLING; request_cancel→CANCELLING(幂等)
 *   DESTROYED   : （无任何允许转移; destroy(non-NULL) 再调 → 拒绝=double destroy 检测）
 * execute 完成（任意状态码）后由 host 把实例状态从 EXECUTING/CANCELLING 更新回
 * CREATED（本判定为纯函数, 不含该完成转移; 参考实现见
 * tests/abi/abi002_lifecycle_probe.c 的状态模拟器）。 */
int acs_lc_transition_allowed_v1(int state, int op);

/* 便捷判定: 状态 (state) 下执行 (op) 的期望返回码 —— 允许→ACS_OK,
 * 拒绝→ACS_ERR_STATE（真实 module 在拒绝路径上应返回此码; request_cancel 在
 * DESTROYED 上同样 ACS_ERR_STATE）。state/op 非法 → ACS_ERR_STATE。 */
acs_status acs_lc_op_error_v1(int state, int op);

/* ═══════════════ 3. 版本协商判定 ═══════════════ */

/* host_abi 兼容判定: v1 下唯一合法值为 ACS_ABI_VERSION_V1（=1）。
 * 任何其他值（未来 major≠1 的 host、minor 漂移、0、垃圾）→ 0（ACS_ERR_ABI_MISMATCH）。 */
int acs_abi_compat_v1(uint32_t host_abi);

/* struct_size 尾部扩展兼容判定: 本方按自身编译期 sizeof(self_size) 与对端声称
 * struct_size(peer_size) 比较:
 *   peer_size >= self_size → 对端更新（含扩展尾字段）→ 1: 本方按自身视图读取,
 *     忽略扩展尾字段（旧/新双向兼容, 扩展只允许追加尾部字段, 12 §1）。
 *   peer_size <  self_size → 对端太旧缺字段 → 0: 拒绝（ACS_ERR_ABI_MISMATCH,
 *     不猜布局）。 */
int acs_struct_ext_ok_v1(uint64_t peer_size, uint64_t self_size);

/* 版本协商快照（module query 期/表校验期填充; POD, 前两字段 acs_head） */
typedef struct acs_version_negotiation_v1 {
    acs_head head;
    uint32_t abi_major;          /* ACS_ABI_MAJOR_V1（对方 major; 不符即拒） */
    uint32_t abi_minor;          /* ACS_ABI_MINOR_V1 */
    uint32_t host_abi;           /* query 收到的 host_abi 原值 */
    uint32_t config_schema_ver;  /* data schema 版本（与 ABI 分离, 12 §4）; 0=未知 */
    uint64_t flags;              /* 保留; v1=0 */
} acs_version_negotiation_v1;

/* 协商组合判定: 全部满足 → ACS_OK; 否则:
 *   host_abi != ACS_ABI_VERSION_V1 / abi_major != 1 → ACS_ERR_ABI_MISMATCH
 *   host_struct_size 太旧（peer < sizeof(acs_host_api_v1)）→ ACS_ERR_ABI_MISMATCH
 *   api_struct_size 太旧（peer < sizeof(acs_module_api_v1)）→ ACS_ERR_ABI_MISMATCH
 * v1 冻结: 不做 minor 级降级猜测。 */
acs_status acs_negotiate_v1(uint32_t host_abi,
                            uint64_t host_struct_size,
                            uint64_t api_struct_size);

/* ═══════════════ 4. 诊断缓冲常量与通用细分码 ═══════════════ */

#define ACS_DIAG_MAX_UTF8 4096u   /* err.message_utf8 最长字节数(不含 NUL); 超限前缀截断 */

/* 通用 detail_code（acs_error_info_v1.detail_code; 0=无细分; 模块自定义从 100 起） */
enum acs_diag_ecode_v1 {
    ACS_DIAG_ECODE_NONE = 0,
    ACS_DIAG_ECODE_NULL_CALLBACK = 1,  /* 必填回调/表项为空（module vtable / host 服务） */
    ACS_DIAG_ECODE_NULL_CONFIG = 2,    /* create/execute 的 config 空串/NULL span */
    ACS_DIAG_ECODE_CONFIG_SCHEMA = 3,  /* config JSON 违反 module 声明 schema */
    ACS_DIAG_ECODE_BUFFER_TOO_SMALL = 4, /* 输出 JSON 缓冲不足(截断); strbuf.size=所需 */
    ACS_DIAG_ECODE_ILLEGAL_STATE = 5,  /* 状态机时序违例(非法调用序列被拒绝) */
    ACS_DIAG_ECODE_DOUBLE_DESTROY = 6, /* destroy 后再 destroy(non-NULL) 检测 */
    ACS_DIAG_ECODE_UNSUPPORTED_OP = 7  /* 本 build 不支持该操作(退 baseline/降级路径) */
};

/* 错误码映射冻结（status × domain × detail; 文本权威见文件头, 机器形态见 schema）:
 *   ACS_ERR_ABI_MISMATCH | CONFIG | (无细分)
 *   ACS_ERR_PARAM        | CONFIG | NULL_CALLBACK / NULL_CONFIG / CONFIG_SCHEMA /
 *                                  BUFFER_TOO_SMALL / UNSUPPORTED_OP
 *   ACS_ERR_STATE        | CONFIG | ILLEGAL_STATE / DOUBLE_DESTROY
 *   ACS_ERR_SELFTEST     | INTERNAL | (无细分)
 *   ACS_ERR_CANCELLED    | CANCELLED | (无细分)
 *   execute 期其余错误    | 实际域(IO/DATA/SCIENCE_PRECONDITION/RESOURCE) | 模块自定义 */

/* ═══════════════ 5. module 级 self_test 语义 ═══════════════ */

/* host/loader 侧契约自检（ABI-002 冻结语义; 参考实现见 tests/abi/abi002_lifecycle_probe.c;
 * ABI-003 loader 接入生产实现）。调用时序: query 成功后、首次 create 前, host 执行
 * 一次; 可重复调用（幂等）。检查项:
 *   1. api != NULL 且 head.abi_version == ACS_ABI_VERSION_V1;
 *   2. 必填回调非空: describe/validate_config/plan/create/execute/inspect/destroy
 *      （request_cancel 可空 = 不支持取消; host 不得调用之）;
 *   3. descriptor 自洽（describe 返回的 module_id 与 query 侧登记一致——ABI-004 细化）。
 * 失败 → ACS_ERR_SELFTEST; 模块不得进入 create/execute（12 §7 provider 同规）。
 * reentrant=yes; threadsafe=yes; internal_parallel=none。 */
acs_status acs_module_selftest_v1(const acs_module_api_v1* api);

/* 实例状态文本（诊断/日志用; 非法值返回 NULL, 与 acs_status_name_v1 同规） */
const char* acs_lc_state_name_v1(int state);
const char* acs_module_op_name_v1(int op);

/* ═══════════════ 6. 布局静态断言（本头新增合同; 与 ABI-001 族断言同规） ═══════════════ */

/* acs_version_negotiation_v1: head 前置 + 全平台稳定尺寸
 * （8 head + 4×4 分量 + uint64 flags; 对齐后 32/64 位均 32 字节, 见文件头推算） */
ACS_STATIC_ASSERT(offsetof(acs_version_negotiation_v1, head) == 0u,
                  "negotiation head first");
ACS_STATIC_ASSERT(offsetof(acs_version_negotiation_v1, abi_major) == 8u,
                  "abi_major at 8");
ACS_STATIC_ASSERT(offsetof(acs_version_negotiation_v1, abi_minor) == 12u,
                  "abi_minor at 12");
ACS_STATIC_ASSERT(offsetof(acs_version_negotiation_v1, host_abi) == 16u,
                  "host_abi at 16");
ACS_STATIC_ASSERT(offsetof(acs_version_negotiation_v1, config_schema_ver) == 20u,
                  "config_schema_ver at 20");
ACS_STATIC_ASSERT(offsetof(acs_version_negotiation_v1, flags) == 24u,
                  "flags at 24 (uint64 align)");
ACS_STATIC_ASSERT(sizeof(acs_version_negotiation_v1) == 32u,
                  "negotiation struct is 32 bytes on 32/64-bit");

/* module 生命周期 vtable 顺序冻结: 回调顺序即 ABI 合同（尾部扩展才可插入,
 * 头部/中部插入会破坏 offsetof）。destroy 恒为末位回调, host 以
 * head.struct_size 判定可用回调集（acs_struct_ext_ok_v1）。
 * 回调为函数指针: 64 位每格 8 字节, 32 位每格 4 字节 → 精确 offsetof 按
 * ACS_ABI_PTR_BITS 分支（ABI-001 32/64 双预期纪律）。 */
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, head) == 0u, "module api head first");
#if ACS_ABI_PTR_BITS == 64
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, describe) == 8u, "describe first callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, validate_config) == 16u,
                  "validate_config second callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, plan) == 24u, "plan third callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, create) == 32u, "create fourth callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, execute) == 40u, "execute fifth callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, inspect) == 48u, "inspect sixth callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, request_cancel) == 56u,
                  "request_cancel seventh callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, destroy) == 64u,
                  "destroy last callback (末位, 可尾扩)");
ACS_STATIC_ASSERT(sizeof(acs_module_api_v1) == 72u,
                  "module api vtable is 72 bytes on 64-bit");
#elif ACS_ABI_PTR_BITS == 32
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, describe) == 8u, "describe first callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, validate_config) == 12u,
                  "validate_config second callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, plan) == 16u, "plan third callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, create) == 20u, "create fourth callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, execute) == 24u, "execute fifth callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, inspect) == 28u, "inspect sixth callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, request_cancel) == 32u,
                  "request_cancel seventh callback");
ACS_STATIC_ASSERT(offsetof(acs_module_api_v1, destroy) == 36u,
                  "destroy last callback (末位, 可尾扩)");
ACS_STATIC_ASSERT(sizeof(acs_module_api_v1) == 40u,
                  "module api vtable is 40 bytes on 32-bit");
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_ABI_LIFECYCLE_V1_H */
