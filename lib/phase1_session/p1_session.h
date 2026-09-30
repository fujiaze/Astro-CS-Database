// lib/phase1_session/p1_session.h — Phase1 进程内会话 (API-P1-001 冻结合同) — CLI-004
// 四段式: create→validate→run→inspect(+destroy); opaque handle, owner=创建者。
// 不 shell-out: CLI handler 直调本会话; cancel/线程预算/监控经 host services 注入。
#ifndef ACSD_P1_SESSION_H
#define ACSD_P1_SESSION_H

#include <cstdint>

#include "acsd/common_abi_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* reentrant:yes; threadsafe:no(handle 级); 内部并行=omp 预算驱动(ac_set_num_threads 注入) */
acsd_status p1_session_create(const acsd_host_services_v1* host, acsd_handle* out);

/* 纯读; 无 IO; 幂等。拒绝未知键/缺必需键/类型错(无 silent default)。 */
acsd_status p1_session_validate(acsd_handle h, const acsd_span_u8 config_json);

/* async_io_depth∈{0,1,2}: ≥1 且预算允许时用 1 个预算租借 worker 做下一帧预读;
 * 取消点=帧粒度; 每阶段经 logger 发 stage 事件(监控)。 */
acsd_status p1_session_run(acsd_handle h, const acsd_span_u8 config_json, int async_io_depth);

/* out=host alloc, 调用方经 host free 释放; JSON 文本(manifest 摘要)。 */
acsd_status p1_session_inspect(acsd_handle h, acsd_span_u8* out_manifest_json);

acsd_status p1_session_destroy(acsd_handle h);

#ifdef __cplusplus
}  // extern "C"

#include <string>
namespace acsd::phase1 {
/* 最近一次会话错误的脱敏摘要(handle 为空→空串); 非科学接口, 供 CLI 诊断 */
std::string last_error(acsd_handle h);
}  // namespace acsd::phase1
#endif

#endif  // ACSD_P1_SESSION_H
