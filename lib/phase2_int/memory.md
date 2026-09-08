# memory.md — astrocs.p2.integration（P2-INT-DOC 冻结）

- 任务: P2-INT-DOC（MODULE_MIGRATION_MATRIX P2-INT 行，owner SA-P2-I23，
  2026-09-09）——合同冻结层，不改生产源码，不 commit。
- 落位: `lib/phase2_int/` 三件套（本目录）。`lib/phase2/` 一目录一套
  三件套已被 P2-COV（astrocs.p2.coverage）占用，不可覆盖；按
  `lib/hips_p2/`（P2-HIPS-DOC）先例新建迁移目标目录。生产源
  `lib/phase2/src/integrate.cpp`（76 行）引用不搬家。
- 权威签名头: `lib/phase2/include/astro/phase2/integrate.h`（74 行）。
  P2PixelStack :36-42（values/weights/support/accepted 可空语义）、
  P2IntegrateStatus :45-51（五态 0..4）、P2PixelResult :53-63、
  support canonical reducer 冻结语义 :17、权重策略注释 :8-11
  （stack.support_x_snr2.v1 / stack.equal.v1，policy/reducer 分离）。
- integrate.cpp 锚（实测）: p2_validate_candidate_weights :10-17
  （null→0；NaN/Inf/负→1）；p2_integrate_pixel :19-74（null :20 →
  rc=1；count==0/values==null :23-26 → NO_CANDIDATES；eligibility
  循环 :30-57（finite :37、support :38-42、权重 :44-49、w==0
  continue :49）；rc 同步 :58-63 invalid_input→INVALID_INPUT；
  状态分支 :65-69（n_positive_weight==0 →
  n_accepted==0?ALL_REJECTED:ZERO_VALID_WEIGHT）；signal=vs/wsum :70；
  support=support?sup_max:1.0 :71；OK :72）。
- 消费链: stage2.cpp 权重构造 :1106-1140（mode 2=ivar→fallback
  support；0=sup×snr²；1=等权 fill 1.0）+ validate :1141/:1402 +
  integrate :1213-1223（chunk 并行路径）/:1515-1527（CPU 串行路径，
  注释 :1525-1526 冻结 "support 唯一 canonical reducer…Stage2 只
  消费"）/:1579-1585（large_scale 二次积分）；OMP 像素间并行 :1288/
  :1298，per-thread 统计 thread id 定序归并 :1305-1313。ACR:
  acr_kernels.cpp process_pixel :189-208（integrate :196、失败清零
  :199-208）、OMP :218/:228 schedule(static)。
- known_defects（登记不改码，整改归 P2-INT-IMPL/TEST）:
  - DISP-P2INT-001（bughunt ledger R3-A，ledger.md:249）: sup_max
    在 `if (w==0) continue`（integrate.cpp:49）之后更新（:54-55），
    零权重 accepted 样本的 support 不进 max → 输出 support 偏低
    （保守方向），偏离 integrate.h:17 "max(accepted support)" 冻结
    语义；Stage2/ACR 直接消费。测试现状: test_p2004 :124 与
    synthetic_gate :4737-4738 样本 support 全正，缺陷不可达。
  - DISP-P2INT-002（文档级表述矛盾，本任务登记）: INTEGRATION.md:58
    写 `max_{valid,W>0} support[i]`（≡ 实现现状），与 integrate.h:17
    "max(accepted support)" 冻结注释表述冲突（accepted ⊋ {W>0}，
    零权重样本差集）。SCI FROZEN 禁改；冻结口径以 header :17 为准
    （合同文本），实现现状按 DISP-P2INT-001 整改后归一。
- 验证锚（相邻证据，引用不冒认）: lib/phase2/tests/synthetic_gate.cpp
  Phase2Integrate 组——零权重合同 :4702-4715（validate 零权重=0、
  signal=10.0、n_positive_weight=1）、NaN/Inf support→INVALID
  :4718-4738、五态+计数器 :4741-4760、加权均值+ALL_REJECTED
  :2628-2643、weight policy 门 :2941-2990（equal/snr/snr2/
  support_x_snr2/inverse_variance 等 bias/rmse 门）；W9 ACR 等价门
  :3021-3160（LegacyLauncherEquivalent）；P2-004 生产 Oracle
  tests/backend/test_p2004_reject_integrate.py（DRIVER_SRC :18-141，
  积分段 :66-126: 状态门 :67-101、signal 10.75/support 0.5 :113-126）。
- descriptor 占位（不改码）: lib/core/src/module_adapters.cpp
  p2_integrate_descriptor :657-675（module_id=astrocs.phase2.integrate、
  ports accepted_mask/corrected/integrated、API-P2-001/
  TEST-P2-INT-001），注册 :785。编排层词汇，P2-XX-INT 对齐。
- 边界: 禁改 docs/science/ 既有文件（INTEGRATION.md FROZEN）；禁改
  生产源码/测试/其他域文档；禁 git add/commit/push；run/local/ 产物
  不提交。
- 验证（P2-INT-DOC 完成，2026-09-09，本任务实测）:
  ①check_traceability_matrix rc=0 errors=0 warns=4（基线 4 条
  REF_OUT_OF_SCOPE 不变，均不涉本模块）②pytest tests/traceability
  9 passed ③check_contract_graph PASS contracts=66（+4: ALG-P2-INT-
  001/DATA-P2-INT/API-P2-INT-001/TEST-P2-INT-001，双向边一致）④
  check_doc_index PASS 219 条（新文档已登记）⑤自检
  run/local/agent_p2_int_doc/selfcheck.py ALL PASS（38 项）。
- ID 方案与矩阵行: 矩阵 13 行 MOD-astrocs-phase2-integrate 原位融合
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
  docs/modules/phase2_int.md 新模块页。
- 需前台 git add 的新文件（未跟踪）: docs/algorithms/
  PHASE2_INTEGRATION.md、docs/modules/phase2_int.md、lib/phase2_int/
  （三件套）。
