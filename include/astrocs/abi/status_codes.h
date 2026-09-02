/* AstroCS 模块 C ABI v1 — 基础层：宏 / 版本 / 状态码 / 基础 POD（ABI-001）
 *
 * 文件: include/astrocs/abi/status_codes.h   （modular ABI 层中最底层）
 *
 * 角色与关系（依赖单向, 每头独立可编译）:
 *   status_codes.h  ←  artifact_api_v1.h  ←  host_api_v1.h  ←  module_api_v1.h
 * 本头只依赖 C11/C++17 标准库头 <stdint.h>/<stddef.h>（纯 POD, 无 STL）。
 *
 * 冻结规则（权威 = 控制包 12_DLL_ABI_AND_LOADER_STANDARD.md §1/§4 + ARC-001
 * FORBID-004; 机器检查见 tests/abi/run_abi_checks.sh）:
 *   1) 跨 DLL 边界类型白名单: 固定宽度整数、POD struct（前两字段恒
 *      struct_size+abi_version, 扩展只允许追加尾部字段）、长度显式 UTF-8 span、
 *      opaque handle、callback。禁止: STL/模板/引用/RTTI/C++ 异常/
 *      裸所有权指针/平台宽度歧义类型（size_t/long/原生 bool）。
 *   2) 所有跨边界结构前两字段恒 acs_head; 失配即 ACS_ERR_ABI_MISMATCH,
 *      不做布局猜测。
 *   3) 内存所有权: 每函数/每字段注释"谁分配谁释放"; 跨边界内存只经
 *      host allocator 或"分配方释放"。文本公共格式 UTF-8（Windows 内部
 *      UTF-16 见 artifact_api_v1.h utf16 service）。
 *   4) 异常边界: 任何 DLL 导出 C 函数内部必须 try/catch 全包裹, 全部
 *      C++ 异常转稳定 acs_status（ACS_ERR_EXCEPTION）; 调用约定统一
 *      ASTROCS_CALL; 导出可见性经 ASTROCS_EXPORT。
 *   5) C11 与 C++17 双可编译（extern "C"）; -fno-exceptions 亦可编译。
 *
 * 兼容说明（与 legacy astrocs/common_abi_v1.h）: 本头族与 legacy 单头共享
 * 基础类型名（acs_head / acs_status / acs_span 系列 / acs_allocator 及数值）,
 * 语义与数值与 legacy 冻结一致（v1 互操作: acs_status 数值位图相同,
 * ACS_ERR_EXCEPTION=10 为本层新增, legacy 不产生该值）。二者不得在同一 TU
 * 混合 include（legacy 随模块迁移退役, ABI-002/006 处理收编）。
 */
#ifndef ASTROCS_ABI_STATUS_CODES_H
#define ASTROCS_ABI_STATUS_CODES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════ 1. 宏: 导出 / 调用约定 / 断言 ═══════════════════════════ */

/* 调用约定: Windows x64 统一为 Microsoft x64 约定（__cdecl 被忽略但显式无害）;
 * Windows x86 显式 __cdecl; 其余平台为空。跨 DLL 函数一律 ASTROCS_CALL。 */
#if defined(_WIN32)
#define ASTROCS_CALL __cdecl
#else
#define ASTROCS_CALL
#endif

/* 导出可见性: ASTROCS_ABI_SHARED=1 且当前 TU 在构建 DLL 本体时定义
 * ASTROCS_ABI_EXPORTS → dllexport; 否则 dllimport/空。
 * Linux 非共享构建默认空（-fvisibility=hidden 场景由目标自行加
 * ASTROCS_ABI_SHARED 并在编译期定义导出）。 */
#if defined(_WIN32) && defined(ASTROCS_ABI_SHARED)
#if defined(ASTROCS_ABI_EXPORTS)
#define ASTROCS_EXPORT __declspec(dllexport)
#else
#define ASTROCS_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) && defined(ASTROCS_ABI_SHARED)
#define ASTROCS_EXPORT __attribute__((visibility("default")))
#else
#define ASTROCS_EXPORT
#endif

/* C11 与 C++17 统一的编译期静态断言 */
#ifdef __cplusplus
#define ACS_STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#else
#define ACS_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#endif

/* 指针宽度探测（布局静态断言的平台分支依据; <stdint.h> 提供 UINTPTR_MAX） */
#if UINTPTR_MAX == UINT64_MAX
#define ACS_ABI_PTR_BITS 64
#elif UINTPTR_MAX == UINT32_MAX
#define ACS_ABI_PTR_BITS 32
#else
#error "AstroCS ABI: unsupported pointer width (only 32/64-bit)"
#endif

/* ═══════════════════════════ 2. 版本常量 ═══════════════════════════ */

/* v1 ABI 版本: 结构布局 + 语义版本。host_abi/abi_version 失配 → ACS_ERR_ABI_MISMATCH。
 * 注意: 与 legacy astrocs/common_abi_v1.h 数值相同（互操作必须一致）。 */
#define ACS_ABI_VERSION_V1 1u

/* ═══════════════════════════ 3. 基础 POD ═══════════════════════════ */

/* 所有跨边界结构的首两字段（handshake; 扩展只允许尾部追加）。
 * 逐字段单位/所有权注释是合同的一部分（ABI layout 测试核对）。 */
typedef struct acs_head {
    uint32_t struct_size;  /* sizeof(具体结构); 失配即拒绝 */
    uint32_t abi_version;  /* ACS_ABI_VERSION_V1 */
} acs_head;

/* 固定宽度 UTF-8 span: 只读文本（JSON/config/错误消息/标识）。
 * data 指向 size 字节 UTF-8; 不要求 NUL 结尾; size 不含 NUL。
 * 所有权: 借入方向由 API 注释; 返回方向所有权=所属句柄（有效至 destroy）。 */
typedef struct acs_str_v1 {
    acs_head head;
    const char* data;   /* UTF-8; 借入/句柄所有 */
    uint64_t    size;   /* 字节数（不含 NUL） */
} acs_str_v1;

/* 可写文本 buffer（调用方提供 data+cap; 被调方写入, size=实际写入字节,
 * 失败/截断返回非 0 状态并写 NUL 于可用位置） */
typedef struct acs_strbuf_v1 {
    acs_head head;
    char*   data;   /* 调用方 buffer; 非 NULL 且 cap>0 时被调方才写 */
    uint64_t cap;   /* 容量（字节, 含 NUL 余量） */
    uint64_t size;  /* 输出: 实际字节数（不含 NUL） */
} acs_strbuf_v1;

/* 固定宽度二进制/数值 span（count=元素数, 非字节）; data 非 const 以允许
 * 输出方向; 输入方向按逐 API 合同只读。所有权=外部分配方, 边界内不释放。 */
typedef struct acs_span_u8 {
    acs_head  head;
    uint8_t*  data;
    uint64_t  count;
} acs_span_u8;

typedef struct acs_span_f32 {
    acs_head head;
    float*   data;
    uint64_t count;
} acs_span_f32;

typedef struct acs_span_f64 {
    acs_head head;
    double*  data;
    uint64_t count;
} acs_span_f64;

/* span 构造宏（自动填充 head; C11/C++17 聚合初始化均可用） */
#define ACS_SPAN_U8(ptr, n) \
    { { sizeof(acs_span_u8), ACS_ABI_VERSION_V1 }, (ptr), (n) }
#define ACS_SPAN_F32(ptr, n) \
    { { sizeof(acs_span_f32), ACS_ABI_VERSION_V1 }, (ptr), (n) }
#define ACS_SPAN_F64(ptr, n) \
    { { sizeof(acs_span_f64), ACS_ABI_VERSION_V1 }, (ptr), (n) }

/* 基础布局静态断言（C11/C++17 任意 include 本头的 TU 均验证;
 * 32 位 i386 SysV 与 64 位 LP64/LLP64 双预期） */
ACS_STATIC_ASSERT(sizeof(acs_head) == 8u, "acs_head must be 8 bytes");
ACS_STATIC_ASSERT(offsetof(acs_head, struct_size) == 0u, "struct_size first");
ACS_STATIC_ASSERT(offsetof(acs_head, abi_version) == 4u, "abi_version second");

/* ═══════════════════════════ 4. 稳定状态码 ═══════════════════════════
 * 0=success; 非 0=hard error（C_ABI_STANDARD: 禁止 rc 双语义）。
 * 数值与 legacy common_abi_v1.h 冻结值一致（v1 互操作）; v1 冻结不可改值。 */
typedef enum acs_status {
    ACS_OK = 0,
    ACS_ERR_PARAM = 1,        /* 参数/配置错误（含 checked 整数越界、空回调） */
    ACS_ERR_ABI_MISMATCH = 2, /* struct_size/abi_version/host_abi 失配 */
    ACS_ERR_NOMEM = 3,        /* host allocator 分配失败 / 资源配额 */
    ACS_ERR_IO = 4,           /* I/O 失败（仅经 host io/artifact service） */
    ACS_ERR_UNSUPPORTED = 5,  /* 本平台/本 build 不支持（host 应退 baseline） */
    ACS_ERR_CANCELLED = 6,    /* cancel 已请求并已停止 */
    ACS_ERR_STATE = 7,        /* 生命周期/状态机违例（double destroy 等） */
    ACS_ERR_BUDGET = 8,       /* executor/ThreadBudget 租借不足 */
    ACS_ERR_SELFTEST = 9,     /* 加载后 self-test 失败, 不得运行 */
    ACS_ERR_EXCEPTION = 10,   /* DLL 边界捕获内部 C++ 异常并转换（禁异常外泄） */
    ACS_ERR_INTERNAL = 70     /* 未分类内部错误; 等价 CLI 退出码 70 语义 */
} acs_status;

/* 错误域（docs/architecture/ERROR_MODEL.md; v1 冻结） */
enum {
    ACS_ERR_DOMAIN_CONFIG = 0,
    ACS_ERR_DOMAIN_DATA = 1,
    ACS_ERR_DOMAIN_SCIENCE_PRECONDITION = 2,
    ACS_ERR_DOMAIN_IO = 3,
    ACS_ERR_DOMAIN_RESOURCE = 4,
    ACS_ERR_DOMAIN_BACKEND = 5,
    ACS_ERR_DOMAIN_CANCELLED = 6,
    ACS_ERR_DOMAIN_INTERNAL = 7
};

/* 日志 severity（ACS_LOG_*; 与 legacy 数值一致） */
enum {
    ACS_LOG_DEBUG = 0,
    ACS_LOG_INFO = 1,
    ACS_LOG_WARN = 2,
    ACS_LOG_ERROR = 3
};

/* 结构化错误详情（可空; message_utf8 借入, 有效至调用返回或所属句柄销毁） */
typedef struct acs_error_info_v1 {
    acs_head    head;
    acs_status  status;          /* 主状态码 */
    int32_t     domain;          /* ACS_ERR_DOMAIN_* */
    const char* message_utf8;    /* 借入 UTF-8, 可 NULL */
    uint32_t    message_bytes;   /* message 字节数（无 NUL） */
    uint32_t    detail_code;     /* 模块自定义细分码; 0=无 */
} acs_error_info_v1;

/* ═══════════════════════ host allocator（跨边界内存唯一归口）═══════════════════════
 * 所有跨边界内存经此或"分配方释放"; 分配与释放必须同一 allocator 实例,
 * 绝不跨 CRT/堆释放（12 §4 / FORBID-004）。alloc 失败返 NULL;
 * align 为 2 的幂（0 或 1=无要求）; free(NULL)=空操作。 */
typedef struct acs_allocator_v1 {
    acs_head head;                     /* struct_size/abi_version */
    void* (*alloc)(void* user_data, uint64_t size, uint64_t align);
    void  (*free)(void* user_data, void* p);
    void* user_data;
} acs_allocator_v1;

/* 稳定枚举 → 冻结字符串名（与 manifest/schema enum 对齐; 非法值返回 NULL） */
const char* acs_status_name_v1(acs_status s);
const char* acs_status_domain_name_v1(int32_t domain);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_ABI_STATUS_CODES_H */
