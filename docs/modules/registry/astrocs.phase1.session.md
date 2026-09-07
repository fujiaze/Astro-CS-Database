---
id: MOD-astrocs-phase1-session
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 0d32c07d65c6d7489fa408cbafaa98ddf9ecf4da
upstream: [DATA-SEMANTICS-001, ARCH-001, API-P1-001, API-CAL-001, API-COS-001]
downstream: [DATA-P1-SESSION, API-P1-SESSION, TEST-P1-SESSION-001]
---

# 模块 astrocs.phase1.session

> P1-SESSION-DOC 手写合同页（2026-09-07，SA-P1-R19）：registry 无
> astrocs.phase1.session descriptor——gen_module_readmes.py 以
> module_adapters.cpp 为源仅生成 8 个 Phase1 descriptor 页
>（calibration/cosmetic/star-psf/wcs-platesolve/photometry/noise-snr/
> drizzle/writer），本页为其装配底座（p1_session 函数族）手写登记，
> 重生成时须保留。权威签名头 lib/phase1_session/p1_session.h。

- 模块词汇：`astrocs.phase1.session`（归档映射
  docs/archive/refactor/P1_SYMBOL_MAP.md:14 既有词汇；现行 registry
  descriptor 无此 module_id，五函数经 P1Api 被 8 descriptor 工厂委托——
  lib/core/src/module_adapters.cpp:693-700/:728-735/:755-770）。
- 层级：assembly（编排），不设独立 DLL（MODULE_MIGRATION_MATRIX 无
  P1-SESSION 行）；构建=静态库 astrocs_phase1_session（根
  CMakeLists.txt:448-452）。
- 合同：DATA-P1-SESSION（DATA_SEMANTICS §16）/ API-P1-SESSION
  （PUBLIC_API「Phase1 装配会话 C API」节）/ 编排上游 API-P1-001
  （docs/api/PHASE1_API_V1.md FROZEN）。
- 节点：canonical 4 段 io_read→calibrate→cosmetic→io_write
  （p1_session.cpp:168/:195/:283/:319；tests/unit/p1_ir_facade_test.cpp:33-40
  断言）；科学实现委托 ac_calibrate_frame（:243）/ac_correct_frame（:294）。
- 外部输入：config JSON 键集（§16.1）/host services 四通道
  （common_abi_v1.h:110-117）/FITS·XISF 读帧/FITS 写帧。
- 并发：threadsafe:no（handle 级）+ reentrant:yes；budget.max_workers
  注入 ac_set_num_threads（:162-165，禁硬编码）。
- 错误：ACS_ERR_*（PARAM/ABI_MISMATCH/NOMEM/IO/CANCELLED/INTERNAL…）；
  manifest 状态机 created→complete/failed。
- 已知差距：API-P1-001 冻结 7-stage vs 现状 4 段（CAL+COS）——如实
  登记（README §3），补齐归 P1-SESSION-IMPL。
- 测试：TEST-P1-SESSION-001=tests/unit/p1_ir_facade_test.cpp；生命周期
  登记=tests/api/test_p1_api.py（API-003）。
