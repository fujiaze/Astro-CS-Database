// lib/phase2_session/p2_session.h — Phase2 进程内会话 (API-P2-001 冻结合同) — CLI-005
// 生产路由: coverage → sampler → UPM build → persist(可选); 全部直调 lib/phase2 生产函数。
// 数据所有权(合同 §1): session 持有 Coverage/Observations/Model, 统一经 free/close。
// 预算绑定(合同 §3): sampler=1(串行 reference); upm=blocks(budget); Σ≤全局 budget。
#ifndef ASTROCS_P2_SESSION_H
#define ASTROCS_P2_SESSION_H

#include <cstdint>

#include "astrocs/common_abi_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* reentrant:yes; threadsafe:no(handle 级); 内部并行仅 UPM blocks(预算驱动) */
acs_status p2_session_create(const astrocs_host_services_v1* host, acs_handle* out);

/* 纯读; 无 IO; 拒未知键/缺必需键(无 silent default) */
acs_status p2_session_validate(acs_handle h, const acs_span_u8 config_json);

/* 取消点=阶段边界(coverage/sample/upm/persist; upm 整模型不写半成品) */
acs_status p2_session_run(acs_handle h, const acs_span_u8 config_json);

acs_status p2_session_inspect(acs_handle h, acs_span_u8* out_manifest_json);

acs_status p2_session_destroy(acs_handle h);

#ifdef __cplusplus
}  // extern "C"

#include <string>
namespace astrocs::phase2 {
/* 最近一次会话错误脱敏摘要(诊断; 非科学接口) */
std::string last_error(acs_handle h);
}  // namespace astrocs::phase2
#endif

#endif  // ASTROCS_P2_SESSION_H
