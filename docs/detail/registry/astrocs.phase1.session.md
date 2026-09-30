# 模块 astrocs.phase1.session

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> Phase1 装配底座（p1_session 函数族）登记页；权威签名头
> lib/phase1_session/p1_session.h，实现 lib/phase1_session/p1_session.cpp。
> 装配底座不设独立 registry descriptor：八个 Phase1 descriptor 工厂经 P1Api
> 委托本模块的五函数。

- 模块词汇：`astrocs.phase1.session`（现行 registry
  descriptor 无此 module_id，五函数经 P1Api 被 8 descriptor 工厂委托——
  lib/infrastructure/scheduler/src/module_adapters.cpp 的工厂注册面）。
- 层级：assembly（编排），不设独立 DLL；构建 = 静态库 astrocs_phase1_session
  （根 CMakeLists.txt）。
- 合同：DATA-P1-SESSION（DATA_SEMANTICS §16）/ API-P1-SESSION
  （PUBLIC_API「Phase1 装配会话 C API」节）/ 编排上游 API-P1-001
  （docs/engineering/PHASE1_API_V1.md FROZEN）。
- 节点：canonical 4 段 io_read→calibrate→cosmetic→io_write
  （p1_session.cpp 四段；eng/tests/unit/p1_ir_facade_test.cpp
  断言）；科学实现委托 ac_calibrate_frame / ac_correct_frame（均在 p1_session.cpp）。
- 外部输入：config JSON 键集（§16.1）/host services 四通道
  （common_abi_v1.h）/FITS·XISF 读帧/FITS 写帧。
- 并发：threadsafe:no（handle 级）+ reentrant:yes；budget.max_workers
  注入 ac_set_num_threads（p1_session.cpp，禁硬编码）。
- 错误：ACS_ERR_*（PARAM/ABI_MISMATCH/NOMEM/IO/CANCELLED/INTERNAL…）；
  manifest 状态机 created→complete/failed。
- 已知差距：API-P1-001 冻结 7-stage vs 现状 4 段（CAL+COS）——如实
  登记（README §3）；补齐属迁移目标（未落地）。
- 测试：TEST-P1-SESSION-001=eng/tests/unit/p1_ir_facade_test.cpp；生命周期
  登记=eng/tests/api/test_p1_api.py（API-003）。
