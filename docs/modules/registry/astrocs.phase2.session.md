---
id: MOD-astrocs-phase2-session
version: 1.0.0
status: ACTIVE
owner: SA-P2-X24
source_commit: dc1ed210fecdd2981d3aaced916b73072e1ca067
upstream: [SCI-UPM-001, SCI-INT-001, SCI-REJ-001, ARCH-001, API-P2-001]
downstream: [DATA-P2-SESSION, API-P2-SESSION-001, TEST-P2-SESSION-001]
---

# 模块 astrocs.phase2.session

> P2-SESSION-DOC 手写合同页（2026-09-10，SA-P2-X24）：registry
> descriptor 无 astrocs.p2.session 词汇（module_adapters.cpp:643 起
> descriptor 族为 p2_sample_descriptor 等编排占位，无 session 条目；
> 编排委托=lib/core/src/module_adapters.cpp:23-26 五 C ABI 声明，
> RT-005 IModule 工厂）——本页为 Phase2 装配会话（p2_session 函数族）
> 手写登记，重生成时须保留。权威签名头 lib/phase2_session/p2_session.h。

- 模块词汇：`astrocs.p2.session`（现行 registry descriptor 无此
  module_id，五函数经 module_adapters.cpp:23-26 声明供 IModule 工厂
  委托；占位词汇对齐归 P2-XX-INT，不作冻结依据）；迁移目标
  astrocs_p2_session.dll **未建——MISSING 如实登记**，补齐归
  P2-SESSION-IMPL。
- 层级：assembly（编排，纯 facade 直调 lib/phase2 生产符号，不实现
  科学公式）；构建=静态库 astrocs_phase2_session（根
  CMakeLists.txt:454-458，link astrocs_contracts astrocs_phase2；
  CLI target 汇总 :504/:522/:538）。
- 合同：ALG-P2-SESSION-001（docs/algorithms/PHASE2_SESSION.md）/
  DATA-P2-SESSION（DATA_SEMANTICS §24）/ API-P2-SESSION-001
  （PUBLIC_API「Phase2 装配会话 C API」节）/ 编排上游 API-P2-001
  （docs/api/PHASE2_API_V1.md，FROZEN，引用不改动）。
- 节点：canonical 4 段 coverage→sample→upm_build→persist
  （p2_session.cpp:119-148/:150-178/:180-219/:221-240 实测；科学实现
  委托 p2_coverage_build/:125/:138、p2_sample_controls/:158/:167、
  p2_upm_build/:204、p2_upm_save/:230）。
- 导出符号：SRC-P2-SESSION-001 五 C API（p2_session.h:17/:20/:23/
  :25/:27：p2_session_create/validate/run/inspect/destroy）+ C++
  诊断面 last_error（h:35，非 C ABI）；展开冻结=API-P2-SESSION-001。
- 外部输入：config JSON 键集（DATA §24.1：hips_paths/output_dir
  必填，upm{max_iterations,huber_delta,smoothing_lambda}/persist_upm/
  upm_save_path 可选；output_dir 现状 validate 必填、run 不消费——
  如实登记）/ host services 四通道（common_abi_v1.h:110-117：
  allocator/logger/cancel/budget）/ 输出落盘仅 upm_save_path 注入
  （persist 段单文件直写，§24.4(4)）。
- 并发与取消：threadsafe:no（handle 级）+ reentrant:yes
  （p2_session.h:16 注释锚）；取消点=阶段边界 4 检查点
  （p2_session.h:22 注释锚；p2_session.cpp:120/:152/:181/:223-227；
  upm 整模型不写半成品）；内部并行仅 UPM blocks
  （cpu_workers=budget.max_workers :155/:195，预算驱动禁硬编码）。
- 错误：ACS_ERR_*（PARAM/ABI_MISMATCH/NOMEM/STATE/IO/INTERNAL/
  CANCELLED；map_rc p2_session.cpp:43-50，语义表=DATA §24.3）。
- manifest 状态机：created→complete/failed（p2_session.cpp:253-256/
  :245；段内 running/ok/fail/cancelled；字段表=DATA §24.2）。
- 已知差距：API-P2-001（PHASE2_API_V1）冻结段集 vs 现状四段
  （coverage/sample/upm_build/persist）——如实登记，补齐归
  P2-SESSION-IMPL；另有 h:19"拒未知键"与 validate 实现漂移、
  run 子键类型错未捕获路径（登记不改码）。
- 已知缺陷：DISP-P2SES-*（编号见 ALG-P2-SESSION-001 DISP 清单，
  本页不另行编号）。
- 测试：TEST-P2-SESSION-001=**MISSING**（DORMANT 登记；设计冻结面=
  ALG-P2-SESSION-001 TEST-DESIGN，引用 id 不引节号）；共址现状=
  tests/unit/p2_ir_facade_test.cpp:27-67 静态结构断言（canonical
  节点声明/facade 委托），无可执行会话行为测试；lib/phase2_session/
  下无 tests 目录（现状如实）。
