// lib/phase3_session/p3_session.h — Phase3 进程内会话 (API-P3-001 冻结合同) — CLI-006
// 五段式: create→validate→run→inspect(+destroy); opaque handle, owner=创建者。
// 不 shell-out: CLI handler 直调本会话; cancel/线程预算经 host services 注入。
// 组装: HiPSReader→WCS→Resampler→FitsWriter。
#ifndef ASTROCS_P3_SESSION_H
#define ASTROCS_P3_SESSION_H

#include <cstdint>

#include "astrocs/common_abi_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

acs_status p3_session_create(const astrocs_host_services_v1* host, acs_handle* out);

/* 纯读; 无 IO; 幂等。显式拒清单全查(projection≠TAN/frame≠ICRS/W·H 越界/abs(dec)<=85°/sampler 非法/
 * longitude_parity 非法/bitpix 非法/coverage_output 非法/scale≤0/缺 source.hips_dir&properties)。 */
acs_status p3_session_validate(acs_handle h, const acs_span_u8 request_json);

/* 取消点=行带; 输出 FITS 原子写; 全程经 logger 发 stage 事件。 */
acs_status p3_session_run(acs_handle h, const acs_span_u8 request_json);

/* out=host alloc, 调用方经 host free 释放; JSON 文本(result 摘要)。 */
acs_status p3_session_inspect(acs_handle h, acs_span_u8* out_result_json);

acs_status p3_session_destroy(acs_handle h);

#ifdef __cplusplus
}  // extern "C"

#include <string>
namespace astrocs::phase3 {
/* 最近一次会话错误的脱敏摘要(handle 为空→空串); 非科学接口, 供 CLI 诊断 */
std::string last_error(acs_handle h);
}  // namespace astrocs::phase3
#endif

#endif  // ASTROCS_P3_SESSION_H
