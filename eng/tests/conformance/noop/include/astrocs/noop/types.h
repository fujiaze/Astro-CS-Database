/* ACSD noop conformance module — 公共类型 (BLD-003 SKELETON)
 *
 * 角色: 模块对外只经 module C ABI v1 (lib/include/acsd/abi/module_api_v1.h)
 * 暴露; 本头仅保留跨边界可复用常量/枚举, 不引入任何实现。
 * 冻结规则 (status_codes.h): 纯 POD; 无 STL/异常/RTTI; C11/C++17 双可编译。
 *
 * 状态: BLD-003 SKELETON — 无科学/宿主语义, 仅可加载性骨架 (ABI-005 填充)。
 */
#ifndef ACSD_CONFORMANCE_NOOP_TYPES_H
#define ACSD_CONFORMANCE_NOOP_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* noop 模块静态标识 (module.yaml / product manifest / DLL descriptor 三方一致) */
#define ACSD_NOOP_MODULE_ID "acsd.conformance.noop"
#define ACSD_NOOP_MODULE_VERSION "0.1.0-alpha.1"
#define ACSD_NOOP_ABI_VERSION 1u

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ACSD_CONFORMANCE_NOOP_TYPES_H */
