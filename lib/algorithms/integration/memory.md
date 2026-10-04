# memory.md — acsd.p2.integration（P2-INT-DOC 冻结）

- 任务: P2-INT-DOC（MODULE_MIGRATION_MATRIX P2-INT 行，owner SA-P2-I23，
  2026-09-09）——合同冻结层，不改生产源码，不 commit。
- 落位: `lib/algorithms/integration/` 三件套（本目录）。`lib/algorithms/coverage/` 一目录一套
  三件套已被 P2-COV（acsd.p2.coverage）占用，不可覆盖；按
  `lib/algorithms/coverage/hips_p2/`（P2-HIPS-DOC）先例新建迁移目标目录。生产源
  `lib/algorithms/coverage/src/integrate.cpp`（89 行）引用不搬家。
- 权威签名头: `lib/algorithms/coverage/include/astro/phase2/integrate.h`（83 行）。
  P2PixelStack :45-51（values/weights/support/accepted 可空语义）、
  P2IntegrateStatus :54-60（五态 0..4）、P2PixelResult :62-72、
  support canonical reducer 冻结语义 :26、权重策略注释 :11-13
  （stack.support_x_snr2.v1 / stack.equal.v1，policy/reducer 分离）。
- integrate.cpp 锚（实测）: p2_validate_candidate_weights :10-17
  （null→0；NaN/Inf/负→1）；p2_integrate_pixel :19-87（null :20 →
  rc=1；count==0/values==null :23-26 → NO_CANDIDATES；eligibility
  循环 :33-62（finite :37、support :38-42、sup_max :49-50、权重 :51-57、
  w==0 continue :56）；rc 同步 :66-69 invalid_input→INVALID_INPUT；
  状态分支 :70-81（n_positive_weight==0 →
  n_accepted==0?ALL_REJECTED:ZERO_VALID_WEIGHT）；signal=vs/wsum :83；
  support=support?sup_max:1.0 :84；OK :85）。
- 消费链: stage2.cpp 权重构造 :1106-1140（mode 2=ivar→fallback
  support；0=sup×snr²；1=等权 fill 1.0）+ validate :1141/:1402 +
  integrate :1213-1223（chunk 并行路径）/:1515-1527（CPU 串行路径，
  注释 :1525-1526 冻结 "support 唯一 canonical reducer…Stage2 只
  消费"）/:1579-1585（large_scale 二次积分）。OMP 像素间并行 = stage2.cpp 的
  `#if defined(P2_ENABLE_OPENMP) && !defined(_MSC_VER)` 段（入口
  `!large_scale_active && effective_cpu_workers(cfg.exec) > 1`，像素循环
  `#pragma omp for schedule(static)`）；per-thread 统计 thread id 定序归并 =
  同文件 `// 定序归并：thread id 固定顺序` 段（`reject_hist[kv.first] +=
  kv.second`）。⚠️ 原并列的 ACR 消费面（acr_kernels.cpp process_pixel
  :189-208、OMP :218/:228 schedule(static)）已随 `383088f2` 删除该文件一并
  退场，现存像素间并行面只有 Stage2 一处。
- known_defects: 无。原先登记的两条经本轮逐行复核**均已不成立**，理由：
  - DISP-P2INT-001（曾记 sup_max 在 `if (w==0) continue`（integrate.cpp:49）
    之后更新、零权重 accepted 样本的 support 不进 max）：**已修复**。现行
    integrate.cpp:49-50 的 `sup_max = std::max(sup_max, in->support[i])` 位于
    `w==0 continue`（:56）**之前**，作用域覆盖全部 accepted ∧ finite 样本，与
    integrate.h:26「max(**accepted** support)」及 docs/science/INTEGRATION.md:69
    「max_{accepted} support[i]」三方一致。原记「测试不可达」的前提已消失。
  - DISP-P2INT-002（曾记 INTEGRATION.md:58 写 `max_{valid,W>0} support[i]`，
    与 header 冲突）：**系误读**。INTEGRATION.md:58 讲的是 n_accepted /
    n_finite 计数定义；support 归约式在同文件 :69，口径为 `max_{accepted}`，
    与实现本就无矛盾，不存在待整改的表述面。
- 验证锚（相邻证据，引用不冒认）: P2-004 生产 Oracle
  eng/tests/backend/test_p2004_reject_integrate.py（DRIVER_SRC :18-141，
  积分段 :66-126: 状态门 :67-101、signal 10.75/support 0.5 :113-126）。
- descriptor 占位（不改码）: lib/infrastructure/scheduler/src/module_adapters.cpp
  p2_integrate_descriptor :657-675（module_id=acsd.phase2.integrate、
  ports accepted_mask/corrected/integrated、API-P2-001/
  TEST-P2-INT-001），注册 :785。编排层词汇，P2-XX-INT 对齐。
- 边界: 禁改 docs/science/ 既有文件（INTEGRATION.md FROZEN）；禁改
  生产源码/测试/其他域文档；禁 git add/commit/push；run/local/ 产物
  不提交。
- 验证（P2-INT-DOC 完成，2026-09-09，本任务实测）:
  ①check_traceability_matrix rc=0 errors=0 warns=4（基线 4 条
  REF_OUT_OF_SCOPE 不变，均不涉本模块）②pytest eng/tests/traceability
  9 passed ③check_contract_graph PASS contracts=66（+4: ALG-P2-INT-
  001/DATA-P2-INT/API-P2-INT-001/TEST-P2-INT-001，双向边一致）④
  check_doc_index PASS 219 条（新文档已登记）⑤自检
  run/local/agent_p2_int_doc/selfcheck.py ALL PASS（38 项）。
- ID 方案与矩阵行: 矩阵 13 行 MOD-acsd-phase2-integrate 原位融合
  （无双行）: SCI=SCI-INT-001（共享 FROZEN，占位 SCI-P2-INT-001 不入
  矩阵）ALG=ALG-P2-INT-001 DATA=DATA-P2-INT API=API-P2-INT-001
  SRC=SRC-P2-INT-001（integrate.h 两符号）TEST=TEST-P2-INT-001
  （test_path=registry 手写合同页 ::TEST-P2-INT-001，HIPS 行先例
  git-tracked 承载；STATUS VERIFIED=设计冻结，可执行归 P2-INT-TEST）
  EVID 仍 MISSING。csv 由 gen_traceability_csv.py 重生成（幂等核对）。
- 登记面: INDEX.yaml 4 新条目 + ALG-INT-001 path 改绑
  PHASE2_INTEGRATION.md（其 downstream=[ALG-P2-INT-001]；ALG-P2-INT-
  001.upstream=[SCI-INT-001, ALG-INT-001]）+ SCI-INT-001.downstream
  追加；DATA_SEMANTICS §21 新节 + :998-999 词汇注记指向 §21；
  PUBLIC_API 尾部 API-P2-INT-001 消费面节；DOCUMENT_INDEX.yaml
  PHASE2_INTEGRATION.md/phase2_int.md/registry integrate 三处登记；
  docs/detail/phase2_int.md 新模块页。
- 需前台 git add 的新文件（未跟踪）: docs/science/algorithms/
  PHASE2_INTEGRATION.md、docs/detail/phase2_int.md、lib/algorithms/integration/
  （三件套）。
