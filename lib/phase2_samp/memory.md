# memory.md — astrocs.p2.sampling（P2-SAMP-DOC 冻结）

- 任务: P2-SAMP-DOC（MODULE_MIGRATION_MATRIX P2-SAMP 行，owner
  SA-P2-S20，2026-09-09）——合同冻结层，不改生产源码，不 commit。
  本目录 `lib/phase2_samp/` 三件套（README r1 + module.yaml +
  memory.md）由 P2-SAMP-DOC 建立。
- 落位: `lib/phase2_samp/`（本目录）。`lib/phase2/` 一目录一套三件套
  已被 P2-COV（astrocs.p2.coverage）占用（lib/phase2/README.md r1，
  2026-09-07），不可覆盖；按 `lib/phase2_int/`（P2-INT-DOC，其按
  `lib/hips_p2/` P2-HIPS-DOC 先例）→ `lib/phase2_rej/`（P2-REJ-DOC）
  先例新建迁移目标目录，仅合同文件、无源码、不与 legacy 目录重叠。
  生产源引用不搬家。
- 矩阵权威（P2-SAMP 行，禁止编造）: owner=SA-P2-S20、
  module_id=astrocs.p2.sampling、target_dll=astrocs_p2_sampling.dll、
  legacy_paths="lib/phase2 sampling sources"、depends_on_int=
  **P2-COV-INT;CPU-005**、science_specific_acceptance="coordinate/tile
  mapping;signal/variance/weight alignment;constant/gradient/impulse;
  boundary/seam;missing and invalid handling"。
- 生产源锚（grep/read 实测，2026-09-09）: lib/phase2/src/sampler.cpp
  （1156 行）+ lib/phase2/include/astro/phase2/sampler.h（136 行，
  唯一权威签名头）。几何解耦 h:6；SNR 纯查询 h:11-12；配置 15 字段
  h:32-57（control_k_corr 默认 1.4 :53、k_corr MC 注 :49-50）；
  default_config 声明 h:60；stats 10 字段 h:63-74；node h:77-83；
  frame_id 声明 h:93（FNV 禁述注 :92）；统计量声明 h:96-99；入口
  h:103-114（probe/fill 注 :101-102）与 cached h:120-132
  （n_geometry_controls 注 :118-119）。
- sampler.cpp 锚（实测）: kTileWidth/Shift/SnrCatalogMax :75-77；
  kControlCorrDefault :82 / kPiHalf :83；kcorr_lookup :88-109（表
  :91-94 九值、clamp :95-96、双线性 :97-108）；frame_drizzle_
  provenance :112-137；g_aio_mu :161；read_tile_pair :163-180
  （锁 :166）；median_of :191-204（P0-01 修复 :196-203）；SnrIndex
  :208-287（query cos_guard :244-248、any_above :263-286）；default
  :294-312；p2_frame_id :314-438（keys :349-352、signal tile :371-378、
  support "S" :381-393、SNR :394-419、截断 :422-430）；stats_median
  :440-447 / stats_mad :449-461；impl :463-1119（bad args :476-479、
  修补 :485-502【DISP-001】、frame_id 0 :512-523、帧打开 :529-546、
  per-frame kcorr :547-555、SNR :562-578、ivar 可缺 :579-581、Stage
  A-E 注 :584-591、CellStat :599-612、snr 中位 :615-627、n_union 门
  :634-638、cells 门 :644-648、首 tile :656-669、pass1_cell lambda
  :699-878（越界 tile :702-714、坐标 :737-742、patch :775-790、
  min_samples :793-800、clipping :802-829、cvar :838-846、veto
  :847-851【DISP-004 :849-850】、SNR 值 :853-867）；并行 :886-913 /
  串行 :914-934（SEH :936-944）；第二遍 :952-1009（回退 :977-985、
  B/S :987-992、三 gate :993-1006）；第三遍 :1011-1063（≥2 clean
  :1013-1027、obs 组装 :1029-1059）；容量 :1071-1077；stats 补偿
  :1078-1088【DISP-002 :1022 vs :1006】；输出拷贝 :1098-1117）；
  入口 :1121-1136 / :1138-1154。
- 消费链: stage2.cpp（1762 行）frame_id 预计算 :219-231、sccfg 组装
  :256-274（14 字段显式透传，control_k_corr 零初始化经 impl :497-498
  修补回退默认 1.4）、probe :279-281 / 上限 :296-300 / 分配 :301 /
  fill :306-311。descriptor 占位（不改码）: lib/core/src/
  module_adapters.cpp p2_sample_descriptor :580-592（module_id=
  astrocs.phase2.sample、ports coverage（in，DATA-P2-COV/ADU/PIXEL）
  → samples（out，DATA-P2-SMP/DIMENSIONLESS/PIXEL），占位 sci_id=
  SCI-P2-SMP-001/alg_id=ALG-P2-SMP-001/data_id=DATA-P2-SMP/
  api_id=API-P2-001/test_id=TEST-P2-SMP-001）。编排层词汇，
  P2-XX-INT 对齐；占位 SCI-P2-SMP-001 ⇒ SCI-UPM-001 映射声明 =
  docs/algorithms/PHASE2_SAMPLER.md §11.4，占位 ID 不入合同。
- known_defects（登记不改码，整改归 P2-SAMP-IMPL/TEST）:
  - DISP-P2SMP-001: 配置修补 `<=0→默认` 吞显式 0（:485-502）；
    bughunt ledger R3-A P3-③（run/local/bughunt/ledger.md:247-250）。
  - DISP-P2SMP-002: 第三遍 :1022 对 reason==2 拒绝帧重复
    ++rejected_insufficient_retained（与第二遍 ：1006 双计数）；
    统计面偏差，obs 输出不受影响。
  - DISP-P2SMP-003: 诊断进度日志直写 stderr（17 处 fprintf）。
  - DISP-P2SMP-004: catalog veto 阈值 10×frame_snr_med 与半径
    0.012° 硬编码（:849-850）。
  - DISP-P2SMP-005: clipping 收敛阈值 1e-12×max(|m0|,1e-12) 在
    m0≈0 时过严（:818），退化固定轮数全迭代（确定性无影响）。
- 验证锚（相邻证据，引用不冒认）: lib/phase2/tests/synthetic_gate.cpp
  Phase2Sampler 组（RealHipsControlSampling :3423、
  G6LocalSnrAvailabilityThreeZones :3470、G1StatisticsCorrectness
  :3594、UPMW-004 MC :4001、cvar 公式 :4089）+
  sampler_parallel_consistency_test.cpp:29（OneTvsTwoTDeterminism）+
  ivar_wiring_test.cpp:223（WireProductionStage2PerFrameIvar）。可执行
  TEST-P2-SMP-001 MISSING（P2-SAMP-TEST 建立）；登记面=
  TEST-P2-SMP-DESIGN-001 设计冻结 VERIFIED（registry 页
  astrocs.phase2.sample.md §独立 synthetic 验证节 + ALG §11.3
  F1-F9 容差）。
- 后续任务锚: P2-SAMP-IMPL=迁移到独立 astrocs_p2_sampling.dll
  （C ABI adapter/plan-execute-cancel-inspect/ThreadLease 接线）；
  P2-SAMP-TEST=建立可执行 TEST-P2-SMP-001 与 F1-F9/负面/串并行/
  资源测试；P2-SAMP-INT=descriptor 对齐注册与 Phase DAG 接入；
  P2-XX-INT=编排层 descriptor 占位词汇与 manifest/registry 对齐
  （不改科学合同）。
- 边界: 禁改 docs/science/ 既有文件（PHASE2_UPM.md FROZEN T106）；禁改
  生产源码/测试/lib/phase2、lib/phase2_int、lib/phase2_rej、lib/hips_p2
  既有文件；禁 git add/commit/push；run/local/ 产物不提交。
