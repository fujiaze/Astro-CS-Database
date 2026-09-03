/* AstroCS echo conformance module — 公共类型 (ABI-005)
 *
 * 角色: 模块对外只经 module C ABI v1 (include/astrocs/abi/module_api_v1.h)
 * 暴露; 本头仅保留跨边界可复用常量/枚举, 不引入任何实现。
 * 冻结规则 (status_codes.h): 纯 POD; 无 STL/异常/RTTI; C11/C++17 双可编译;
 * C++17 下 -fno-exceptions 亦可编译。
 *
 * echo module 语义 (ABI-005): 无科学含义的 conformance 探针 —— 每个 host
 * callback (artifact read/manifest query、allocator、logger、cancel、
 * executor lease、config/registry query) 都有正/负行为可被 host 侧测试驱动;
 * artifact "write" 在 ABI v1 冻结面不存在写回调 → 模块请求写通道时如实返回
 * UNSUPPORTED(不绕过 host 自开文件, FORBID-002)。本模块是跨平台 ABI 探针,
 * 不做任何科学/数据加工。
 */
#ifndef ASTROCS_CONFORMANCE_ECHO_TYPES_H
#define ASTROCS_CONFORMANCE_ECHO_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* echo 模块静态标识 (module.yaml / product manifest / DLL descriptor 三方一致) */
#define ASTROCS_ECHO_MODULE_ID "astrocs.conformance.echo"
#define ASTROCS_ECHO_MODULE_VERSION "0.11.0-alpha.1"
#define ASTROCS_ECHO_ABI_VERSION 1u
#define ASTROCS_ECHO_BUILD_ID "ABI-005-echo"

/* echo config JSON 指令 (execute/plan/validate 共享语义; 测试 host 侧解析) */
#define ASTROCS_ECHO_CFG_ACTION_ECHO "echo"          /* 默认: 回显 input manifest */
#define ASTROCS_ECHO_CFG_ACTION_ARTIFACT_READ "artifact_read"  /* 经 host artifacts 读 storage_uri */
#define ASTROCS_ECHO_CFG_ACTION_WRITE_BLOCKED "write_blocked"  /* 请求写通道 → UNSUPPORTED(无写服务) */
#define ASTROCS_ECHO_CFG_ACTION_CANCEL_POLL "cancel_poll"      /* execute 轮询 host cancel */
#define ASTROCS_ECHO_CFG_ACTION_LEASE "executor_lease"         /* acquire/release host executor 租借 */
#define ASTROCS_ECHO_CFG_ACTION_ERROR "request_error"          /* 按 config 触发稳定错误码 */
#define ASTROCS_ECHO_CFG_KEY_ACTION "action"
#define ASTROCS_ECHO_CFG_KEY_STORAGE_URI "storage_uri"
#define ASTROCS_ECHO_CFG_KEY_WORKERS "workers"
#define ASTROCS_ECHO_CFG_KEY_ERROR_STATUS "error_status"

/* 实例私有诊断字段名 (inspect JSON 输出; 测试断言用) */
#define ASTROCS_ECHO_INSPECT_ACTION "action"
#define ASTROCS_ECHO_INSPECT_LOADED "host_loaded"

/* echo 模块自定义 err.detail_code (生命周期通用值 0-7 冻结于 lifecycle_v1.h;
 * 模块自定义从 100 起, lifecycle_v1.h §4) */
enum {
    ACS_ECHO_ECODE_NONE = 0,
    ACS_ECHO_ECODE_ACTION_UNKNOWN = 100,  /* config action 未知 */
    ACS_ECHO_ECODE_ARTIFACT_MISSING = 101,/* host.artifacts == NULL(需要 I/O 却无服务) */
    ACS_ECHO_ECODE_HOST_LOGGER_MISSING = 102, /* config 要求 logger 而 host.logger == NULL */
    ACS_ECHO_ECODE_EXECUTOR_MISSING = 103,    /* config 要求 lease 而 host.executor == NULL */
    ACS_ECHO_ECODE_CANCEL_MISSING = 104,      /* config 要求 cancel 而 host.cancel == NULL */
    ACS_ECHO_ECODE_ARTIFACT_WRITE_UNSUPPORTED = 105 /* ABI v1 无写回调: 拒绝不自开文件 */
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_CONFORMANCE_ECHO_TYPES_H */
