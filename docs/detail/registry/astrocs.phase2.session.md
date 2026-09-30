# 模块 astrocs.phase2.session

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> Phase2 装配会话（p2_session 函数族）登记页；权威签名头
> lib/phase2_session/p2_session.h，实现 lib/phase2_session/p2_session.cpp。
> 装配会话不设独立 registry descriptor：编排占位 descriptor 的 IModule 工厂
> 经 module_adapters.cpp 的五 C ABI 声明委托本模块（RT-005）。

- 模块词汇：`astrocs.p2.session`（装配会话词汇；五函数经 module_adapters.cpp
  声明供 IModule 工厂委托，无独立 registry descriptor）；迁移目标
  astrocs_p2_session.dll（未落地）。
- 层级：assembly（编排，纯 facade 直调 lib/algorithms/coverage 生产符号，不实现
  科学公式）；构建=静态库 astrocs_phase2_session（根
  CMakeLists.txt，link astrocs_contracts astrocs_phase2；
  CLI target 汇总三处）。
- 合同：ALG-P2-SESSION-001（docs/science/algorithms/PHASE2_SESSION.md）/
  DATA-P2-SESSION（DATA_SEMANTICS §24）/ API-P2-SESSION-001
  （PUBLIC_API「Phase2 装配会话 C API」节）/ 编排上游 API-P2-001
  （docs/engineering/PHASE2_API_V1.md，FROZEN，引用不改动）。
- 节点：canonical 4 段 coverage→sample→upm_build→persist
  （p2_session.cpp 四段实测；科学实现
  委托 p2_coverage_build 两处、p2_sample_controls 两处、
  p2_upm_build、p2_upm_save）。
- 导出符号：SRC-P2-SESSION-001 五 C API（p2_session.h：
  p2_session_create/validate/run/inspect/destroy）+ C++
  诊断面 last_error（p2_session.h，非 C ABI）；展开冻结=API-P2-SESSION-001。
- 外部输入：config JSON 键集（DATA §24.1：hips_paths/output_dir
  必填，upm{max_iterations,huber_delta,smoothing_lambda}/persist_upm/
  upm_save_path 可选；output_dir 现状 validate 必填、run 不消费，登记不改码）/
  host services 四通道（common_abi_v1.h：
  allocator/logger/cancel/budget）/ 输出落盘仅 upm_save_path 注入
  （persist 段单文件直写，§24.4(4)）。
- 并发与取消：threadsafe:no（handle 级）+ reentrant:yes
  （p2_session.h 注释锚）；取消点=阶段边界 4 检查点
  （p2_session.h 注释锚；p2_session.cpp 四处；
  upm 整模型不写半成品）；内部并行仅 UPM blocks
  （cpu_workers=budget.max_workers 两处，预算驱动禁硬编码）。
- 错误：ACS_ERR_*（PARAM/ABI_MISMATCH/NOMEM/STATE/IO/INTERNAL/
  CANCELLED；map_rc p2_session.cpp，语义表=DATA §24.3）。
- manifest 状态机：created→complete/failed（p2_session.cpp；
  段内 running/ok/fail/cancelled；字段表=DATA §24.2）。
- 已知差距（登记不改码）：API-P2-001（PHASE2_API_V1）冻结段集 vs 现状四段
  （coverage/sample/upm_build/persist），补齐属迁移目标（未落地）；
  p2_session.h「拒未知键」与 validate 实现漂移；run 子键类型错未捕获路径。
- 已知缺陷：编号与语义见 ALG-P2-SESSION-001 的缺陷清单（本页不另行编号）。
- 测试：TEST-P2-SESSION-001 待建（设计冻结面 = ALG-P2-SESSION-001 TEST-DESIGN）；
  共址现状 = eng/tests/unit/p2_ir_facade_test.cpp 静态结构断言（canonical
  节点声明/facade 委托），无会话行为测试；lib/phase2_session/ 下无 tests 目录。
