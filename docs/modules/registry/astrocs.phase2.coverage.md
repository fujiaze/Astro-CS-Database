---
id: MOD-astrocs-phase2-coverage
version: 1.0.0
status: ACTIVE
owner: SA-P2-S20
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-P2-COV-001, ALG-COV-001, API-COV-001, API-P2-001]
downstream: [TEST-P2-COV-001, DATA-COV-001]
---

# 模块 astrocs.p2.coverage

> P2-COV-DOC 手写合同页（2026-09-07，SA-P2-S20）：本页重写旧 registry
> 占位页（旧页以 module_adapters.cpp descriptor 为唯一源——编排层
> 词汇不得作为冻结依据，本页手写登记事实修订；registry 页保留先例
> astrocs.phase1.session.md / astrocs.phase1.star-detection.md）。模块级
> 事实以 lib/phase2/README.md（r1，CONTRACT_READY）+ module.yaml
> （MOD-astrocs-phase2-coverage，module_id=astrocs.p2.coverage，
> dll_target=astrocs_p2_coverage.dll，entrypoint=MISSING）为准。权威
> 签名头 lib/phase2/include/astro/phase2/coverage.h:1-59（grep 实测，
> 禁止手抄他版）。

- 模块词汇：`astrocs.p2.coverage`（MODULE_MIGRATION_MATRIX P2-COV 行
  module_id；owner=SA-P2-S20，legacy_paths="lib/phase2 coverage
  sources"，迁移目标 astrocs_p2_coverage.dll——合同值，尚未存在；
  depends_on_int=IO-003;DATA-004;RT-006）。
- 层级：Phase2 生产模块（DAG 首节点 coverage）；现状构建根
  CMakeLists.txt:338-346 astrocs_phase2 STATIC（src/coverage.cpp :341，
  无独立 DLL target）；lib/phase2/CMakeLists.txt:42 phase2 STATIC 为
  模块自测 compatibility target（非产品事实源）；生产调用=
  lib/phase2_session/p2_session.cpp:119-148 coverage 阶段（两阶段
  调用 :125/:138，manifest 登记 :145-147）。
- 合同：SCI-P2-COV-001（指向既有 FROZEN 共享 SCI：docs/science/
  PHASE2_UPM.md §1 覆盖并集 + docs/science/INTEGRATION.md §5
  support/validity 分离 + docs/science/SCIENCE_SCOPE.md §处理链第 5
  步；共享 SCI 不改动，状态声明=docs/algorithms/PHASE2_COVERAGE.md
  §11.5，P1-WCS-DOC SCI-WCS-001=共享 ASTROMETRY.md 先例）/
  ALG-COV-001（PHASE2_COVERAGE.md §2 逐公式行号锚）/ DATA-COV-001
  （DATA_SEMANTICS §19）/ API-COV-001（PUBLIC_API Coverage union C API
  节）/ 编排上游 API-P2-001（PHASE2_API_V1 FROZEN §1 所有权图
  Coverage 行 + §2 并发五字段行 1）。
- 生产符号（SRC-COV-001，2 导出 + 2 内部链接，coverage.cpp 实测）：
  p2_coverage_build（:144，唯一生产入口）/ p2_coverage_free（:233，
  POD memset 清零，不释放堆）/ parse_props（:20）/ inspect_frame
  （:59）；P2MocCell/P2HipsInputInfo/P2CoverageResult
  （coverage.h:26-48）。
- 端口：入 calibrated（HiPS 树路径数组，`const char* const*
  [n_inputs]`）；出 coverage（union MOC，P2MocCell [K] 无量纲整数，
  **HEALPix NESTED equatorial/ICRS**——旧 descriptor 登记端口坐标
  PIXEL 与实际不符（module_adapters.cpp:623-638），以 DATA-COV-001
  §19.3 为准，P2-COV-INT 修订；DATA-P2-COV 端口名保留为编排词汇，
  权威定义=DATA-COV-001）。
- 科学红线（matrix P2-COV 专项，负向条款）：coverage/support/validity
  三概念分离（support=SCI-INT-001 §2 样本级 [0,1]、validity=§5 有效性
  标志，均不在本模块域）；**no use as implicit scientific weight**——
  union cell/n_tiles/覆盖帧数为几何登记量，禁入任何权重式（w_UPM
  唯一冻结式 PHASE2_UPM.md §5，support 仅 eligibility/coverage 语义）。
- 并发：reentrant=yes / threadsafe=no（独立对象）/ internal_parallel=
  none（单线程整数集合运算，bitwise 确定，determinism=
  fixed_reduction_order）；ThreadLease/取消检查点未接线（阶段级取消
  由 session 阶段边界 p2_session.cpp:119-121（检查 :120）提供；整改归 P2-COV-IMPL）。
- 错误：rc 0=成功（含 K=0）/1=失败 + error[512] 载因；status 与 rc
  同步（"no inputs" 分支例外=DISP-COV-001）；编排映射 ACS_ERR_PARAM/
  ACS_ERR_STATE（API-P2-001 §4）。
- 已知限制：DISP-COV-001..005（ALG-COV-001 §11.3，登记不改码，整改归
  P2-COV-IMPL/INT）：001 "no inputs" status 不一致 / 002 frame_id 基名
  截断 / 003 空 filter 静默放行 / 004 intersection/depth/missing-tiles
  产品缺失（matrix 四语义仅 union 落地；覆盖度几何≠UPM
  geometric_reliability 权重因子，该乘数恒 1.0 修正归 P2-UPM 域）/
  005 extern "C" include+两阶段全量重扫+ThreadLease。
- 测试设计：TEST-COV-DESIGN-001（PHASE2_COVERAGE.md §11.4，冻结容差
  =整数/bitwise 断言零数值容差），由 P2-COV-TEST 执行落
  TEST-P2-COV-001 + EVIDENCE；legacy gate Phase2Coverage.RealHipsUnion
  （synthetic_gate.cpp:3374）/ FilterMismatchRejected（:3410）依赖
  Fatduck 本地路径 GTEST_SKIP（:3376/:3413），合成 fixture 归
  P2-COV-TEST。
