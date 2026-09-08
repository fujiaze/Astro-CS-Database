# memory.md — astrocs.p2.rejection（P2-REJ-DOC 冻结）

- 任务: P2-REJ-DOC（MODULE_MIGRATION_MATRIX P2-REJ 行，owner SA-P2-R22，
  2026-09-09）——合同冻结层，不改生产源码，不 commit。本目录
  `lib/phase2_rej/` 三件套（README r1 + module.yaml + memory.md）由
  P2-REJ-DOC 建立。
- 落位: `lib/phase2_rej/`（本目录）。`lib/phase2/` 一目录一套三件套已被
  P2-COV（astrocs.p2.coverage）占用（lib/phase2/README.md r1，
  2026-09-07），不可覆盖；按 `lib/phase2_int/`（P2-INT-DOC，其按
  `lib/hips_p2/` P2-HIPS-DOC 先例）新建迁移目标目录，仅合同文件、
  无源码、不与 legacy 目录重叠。生产源引用不搬家。
- 矩阵权威（P2-REJ 行，禁止编造）: owner=SA-P2-R22、
  module_id=astrocs.p2.rejection、target_dll=astrocs_p2_rejection.dll、
  legacy_paths="lib/phase2 rejection sources"、depends_on_int=
  **P2-UPM-INT;CPU-005**（注: P2-REJ-INT 是 P2-INT 行的 depends_on_int
  本域被依赖项，勿混淆）、science_specific_acceptance="sigma/winsor/
  linear-fit/ESD definitions;frame identity;small N;NaN;cosmic ray
  injection;mask/provenance;deterministic decisions"。
- 生产源锚（grep/read 实测）: lib/phase2/src/rejection.cpp（2076 行）
  + lib/phase2/include/astro/phase2/rejection.h（329 行，唯一权威
  签名头）。判向冻结 h:20-21；normalization 三态 h:94-96；percentile
  params h:117-119（DISP-P2REJ-001 锚）；large_scale 默认 h:142-148
  （enabled=0/min_structure 8/半径 2/2）；方法 0..10 h:46-56
  （AUTO=10 永不进 kernel）；reason u8 0..3 h:74-77；status int
  0..7 h:82-89；compat 门冻结注释 h:299。
- rejection.cpp 锚（实测）: 冻结头注释 :1-11（阈值冻结表）；AUTO
  规划 :1072-1078（n<6→PERCENTILE、6..15→WINSORIZED、>15→
  LINEAR_FIT）；semantic_id :1011；plan_resolve :1028-1084（默认
  underdetermined_n=2 :1048、linear_fit 5.0/3.5 :1057、esd :1059、
  rcr technique=0 SS_MEDIAN_DL :1065）；eligibility_filter :1128；
  collect_candidate_stack :1150（source_indices 权威映射）；
  kernel p2_reject_stack_ex :1684-1858（INVALID_METHOD 注 :1689、
  非 finite→INVALID_INPUT :1711-1719、PERCENTILE×norm≠MEDIAN_CENTER
  :1722-1730、RCR×norm≠NONE :1731-1739、UNDERDETERMINED 门
  :1738-1745、n==0→MIN_SAMPLES :1706、tally :1820-1858、n=2 白名单
  SCIENCE_FREEZE 注 :1838）；linear_fit (value, orig_index)
  字典序 :1418-1419；ESD tie-break frame_id 1e-15 :1515-1518；minmax
  value-only sort :1667-1669（DISP-P2REJ-004）；compat p2_reject_stack
  :1863-1905（:1869 n==0、:1891-1898 min_samples、:1905
  underdetermined_n=2）；large_scale :2051-2074。
- 消费链: stage2.cpp（1762 行）权重构造 mode2=ivar :1106-1140、
  gather :1101/:1349、p2_reject_stack_ex :1184（串行）/:1449
  （并行块内）、OMP 像素间并行 :1288、per-thread 统计 thread id
  定序归并 :1305-1313、large_scale 两遍 :1544-1605（调用 :1549）。
  acr_kernels.cpp（361 行）OMP :218/:228 schedule(static)。
- descriptor 占位（不改码）: lib/core/src/module_adapters.cpp
  p2_reject_descriptor :638-655（module_id=astrocs.phase2.reject、
  sci_id=SCI-P2-REJ-001/alg_id=ALG-P2-REJ-001/data_id=DATA-P2-REJ/
  api_id=API-P2-001/test_id=TEST-P2-REJ-001、ports corrected（in，
  DATA-P2-COR/ADU/PIXEL）与 accepted_mask（out，DATA-P2-REJ/
  DIMENSIONLESS/PIXEL），注册 :785）。编排层词汇，P2-XX-INT 对齐。
  注: manifest input_ports 照 descriptor 实际 corrected/accepted_mask
  （support/ivar/quality_flags 为消费链词汇：stage2 逐帧打开与
  gather 栈内消费，非编排 port）；占位 sci_id=SCI-P2-REJ-001 ⇒
  SCI-REJ-001 映射声明 = docs/algorithms/PHASE2_REJECTION.md §11.5，
  占位 ID 不入合同。
- known_defects（登记不改码，整改归 P2-REJ-IMPL/TEST）:
  - DISP-P2REJ-001: rejection.h:118 percentile low_fraction 注释
    "默认 0.1" vs 实现/SCI 0.2（冻结表以 ALG §5/rejection.cpp:1-11
    为准）。
  - DISP-P2REJ-002: SCI REJECTION.md:89 退化态 "NO_CANDIDATES" vs
    实现空栈=MIN_SAMPLES（:1706/:1869）；NO_CANDIDATES 属积分域
    P2IntegrateStatus（integrate.h:46）。
  - DISP-P2REJ-003: SCI/REJECTION_ALGORITHMS 行号锚漂移；行号权威
    =ALG-P2-REJ-001 §3。
  - DISP-P2REJ-004: minmax value-only tie-break 未显式冻结
    （sort :1667-1669，比较器仅 value），等值样本 permutation
    不变性未承诺。
- 验证锚（相邻证据，引用不冒认）: lib/phase2/tests/synthetic_gate.cpp
  Phase2Rejection 组 :2639-2725（R1/G4 等）+ G6 组 :2779-2863
  （ESD NIST Rosner 54/掩蔽/winsor≠sigma/permutation）+ V15-V17 组
  :4138-4864（V16GatherStridedFp32Fp64 :4592、V17InvalidMethodStatus
  :4763、V17LargeScale* :4798-4864）；tests/backend/
  test_p2004_reject_integrate.py（P2-004 生产 Oracle）与
  tests/unit/p2_rejection_test.cpp（P2-005）。可执行 TEST-P2-REJ-001
  MISSING（P2-REJ-TEST 建立）；登记面=TEST-P2-REJ-DESIGN-001 设计
  冻结 VERIFIED（registry 页 astrocs.phase2.reject.md §独立
  synthetic 验证节 :126-129 + ALG §11.4 F1-F8 容差 :554-609）。
- 后续任务锚: P2-REJ-IMPL=迁移到独立 astrocs_p2_rejection.dll
  （C ABI adapter/plan-execute-cancel-inspect/ThreadLease 接线）；
  P2-REJ-TEST=建立可执行 TEST-P2-REJ-001 与负面/串并行/资源测试；
  P2-REJ-INT=descriptor 对齐注册与 Phase DAG 接入；P2-XX-INT=编排层
  descriptor 占位词汇与 manifest/registry 对齐（不改科学合同）。
- 边界: 禁改 docs/science/ 既有文件（REJECTION.md FROZEN T107）；禁改
  生产源码/测试/lib/phase2、lib/phase2_int、lib/hips_p2 既有文件；
  禁 git add/commit/push；run/local/ 产物不提交。
