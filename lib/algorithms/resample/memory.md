# memory.md — astrocs.p3.resample（P3-RSMP-DOC 冻结）

- 任务: P3-RSMP-DOC（MODULE_MIGRATION_MATRIX P3-RSMP 行，owner
  SA-P3-S26，2026-09-12）——合同冻结层，不改生产源码，不 commit。
  本目录 `lib/phase3_rsmp/` 三件套（README r1 + module.yaml +
  memory.md）由 P3-RSMP-DOC 建立。
- 落位: `lib/phase3_rsmp/`（本目录）。实测生产源
  lib/phase3_session/p3_resample.cpp + p3_resample.h 位于
  lib/phase3_session/ ——该目录为 Phase3 会话编排域共享源
  （p3_session/p3_output/p3_wcs/p3_resample/hips_properties 五源同库
  astrocs_phase3_session，根 CMakeLists.txt:460-465），非整目录归属
  本域（矩阵 legacy_paths="lib/phase3_session sampler sources" 只圈
  sampler sources）；按 lib/phase3_proj→phase3_fits 迁移目录先例
  新建迁移目标目录，仅合同文件、无源码、不与 legacy 目录重叠；
  legacy 生产源引用不搬家。
- 矩阵权威（P3-RSMP 行，禁止编造）: owner=SA-P3-S26、
  module_id=astrocs.p3.resample、target_dll=astrocs_p3_resample.dll、
  legacy_paths="lib/phase3_session sampler sources"、
  depends_on_int=**P3-PROJ-INT;IO-003;CPU-005**、
  science_specific_acceptance="source_order actually used;nearest
  exact cell;HEALPix iso-latitude interp4;constant/vector field;
  strict missing;support/mask;shared bounded cache"。
- module_id 决策: 矩阵行 astrocs.p3.resample 为冻结权威依据，
  直接沿用；descriptor 占位 module_id=astrocs.phase3.resample2
  （module_adapters.cpp:363-377 p3_resample2_descriptor）为编排层
  词汇，由 P3-RSMP-INT 对齐，不入合同。
- MOD ID 决策: registry 现有行 **MOD-astrocs-phase3-resample2**
  （本域生产 descriptor——alg_id=ALG-P3-003、data_id=DATA-P3-RES、
  ports wcs_plan(DATA-P3-WCS 必)+hips(DATA-HIPS-001 必)+
  resampled(DATA-P3-RES 可)，与采样域功能对应），module.yaml id 与
  之一致；相邻占位行 MOD-astrocs-phase3-resample
  （module_adapters.cpp:300-318 phase3_descriptor，P2 模板复制残留，
  ports hips/tile、alg_id=ALG-P3-RES-001 占位）**不在本任务触碰**，
  留给 P3-RSMP-INT 对齐处理；registry 页手写化
  （docs/modules/registry/astrocs.phase3.resample2.md，原 GENERATED
  风格改 ACTIVE_INFORMATIVE 手写合同页，照 astrocs.phase3.writer.md/
  astrocs.phase3.wcs.md 先例），模块总页新建 docs/modules/
  phase3_rsmp.md。
- 本任务 ID 决策（唯一方案，避免与既有占位冲突）:
  SCI=SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md 共享 FROZEN
  V5 SCI-007 2026-08-28，集合 SCI-P3-001..020；矩阵行 science_id
  占位 SCI-P3-RES-001 MISSING 不入合同，映射声明于
  PHASE3_RSMP_IMPL.md §5，照 P3-PROJ 先例）；
  ALG=ALG-P3-RSMP-IMPL-001（docs/algorithms/PHASE3_RSMP_IMPL.md
  新建，兼承接既有 ALG-P3-003（G3 order 选择 + G4 leaf 采样）本域
  子面；ID 风格与 ALG-P2-UPM-IMPL-001/ALG-P3-PROJ-IMPL-001/
  ALG-P3-FITS-IMPL-001 同族；既有 ALG-P3-001..004 登记于
  docs/algorithms/PHASE3_RESAMPLE.md（本文件为公共施工规格，本任务
  仅表述级修订：LRU→FIFO 更正 + 实现锚补记，G1-G5 公式零改动），
  ALG-P3-003 以独立条目入 INDEX.yaml（ACTIVE，downstream 增补
  ALG-P3-RSMP-IMPL-001）；ALG-P3-RES-001 为 descriptor 占位 ID
  不入 INDEX（照 P3-PROJ 先例 SCI-P3-WCS-001 不入）；
  DATA=DATA-P3-RES（descriptor/matrix 现值沿用；承载=
  DATA_SEMANTICS.md §29 新建，权威落位补齐——此前该 ID 仅存在于
  §27 交叉引用与 descriptor 占位，无权威文档节）；
  API=API-P3-RSMP-001（PUBLIC_API.md 新节；descriptor 占位
  api_id=API-P3-001 为编排层 FROZEN 合同不变，镜像引用）；
  TEST=TEST-P3-RES-001（descriptor/matrix 现值沿用，矩阵状态
  DORMANT；登记面=TEST-P3-RSMP-DESIGN-001 设计冻结 VERIFIED 承载于
  ALG-P3-RSMP-IMPL-001 §12 + registry 手写页 §9 双重陈述——照
  P3-PROJ/P3-FITS 先例；可执行面升级归 P3-RSMP-TEST，不冒认。现状
  执行测试四处如实登记: tests/backend/p3_resample_probe_main.cpp
  探针 + tests/backend/test_p3_resample.py 156 行 +
  test_p3003_parallel_resampler.py 104 行 + tests/unit/
  p3_interp_test.cpp 109 行 / p3_coverage_test.cpp 106 行独立参考
  实现）。
- 生产源锚（read/grep 实测，2026-09-12）:
  lib/phase3_session/p3_resample.h（58 行，唯一权威签名头）:
  P3ResampleStatus :12-17（P3_RS_OK=0/P3_RS_PARAM=1/
  P3_RS_UNSUPPORTED=2/P3_RS_IO=3）、p3_order_select :21、
  p3_resample_check_mode :25、P3SamplerImpl 前置声明 :28、
  P3Sampler :29-31（impl+last_error[256]）、p3_sampler_open :32-33、
  p3_sampler_open_ex :36-38、p3_sampler_set_max_tiles :42、
  p3_sample_nearest :46-47、p3_sample_bilinear :51-52、
  p3_sampler_close :54；头注 :1-3（ALG-P3-003/P3-003 合同面）。
  lib/phase3_session/p3_resample.cpp（239 行）: kTileWidth=512 :18
  （SCI §9a-1 冻结一致）、kReaderBuf=512² :19、TileCache :22-36
  （FIFO 最旧逐出 keys.erase(begin())，cap 默认 8）、read_leaf
  :49-80（leaf_order=tile_order+9；leaf_to_tile_nest/tile_to_leaf_
  nest/nested_local_to_fits_index(local,9,512) 权威函数；fits_index
  =(511-x)*512+y=DATA_SEMANTICS §3 CDS oracle 冻结；缓存命中 :62-66/
  未命中 :68-80）、p3_order_select :82-93（线性扫描最小 k 使
  pixel_resolution_arcsec(512<<k)/3600≤scale_deg_per_px；无更细层取
  max_order；与 G3 ceil 式数学等价——pixel_resolution_arcsec=
  sqrt(4π/(12nside²))·180·3600/π=sqrt(π/3)/nside rad，
  healpix_core.cpp 权威）、p3_resample_check_mode :95-107
  （surface_brightness 唯一合法）、P3SamplerImpl :109 起
  （open :109+/open_ex :130-159 BUNIT 缺省 ADU 绝不 Jy/beam/
  set_max_tiles :161-168 ≤0 恢复默认 8/close :170-180 幂等）、
  p3_sample_bilinear :196-230（leaf 3×3 邻域四象限最近中心 d² 比较/
  den=dx·dy2-dx2·dy≤0 跳过背面/|dx|>1e-300 防 0 除/u,v clamp [0,1]/
  四权重 FP64 Σ=1/any_nan→nanf("")/coverage 恒 1/(void)y1 压
  unused）、p3_sample_nearest :232-239（ang2pix 精确 cell）。
  会话消费: p3_session.cpp :16 include/:167-178 主 sampler open_ex :171
  （实际 order/BUNIT）/:179-194 max_tiles 内存守卫（默认
  min(1024, ceil(W·H/512²)+16)，请求超默认→ACS_ERR_BUDGET 可降
  不可升）/:196-199 p3_order_select（max_order=输入实际 order，
  clamp≤20 禁仅写 metadata）/:208-244 worker 池（:211-214 worker 数
  决定/:217-244 worker 闭包每 worker 独立
  P3Sampler+TileCache（n_workers=budget.max_workers 禁
  hardware_concurrency，>hpx clamp，<2 串行）/:236-237 nearest/
  bilinear 分派/:258-263 cancel 收尾（:260-261 "cancelled at row"→
  ACS_ERR_CANCELLED；:263 worker open 失败→ACS_ERR_IO）/:265-277
  provenance（order_sel_used/
  sampler_used 填真，manifest_hash/missing_tiles 恒 nullptr）。
  请求守卫: parse_request :95-129（projection 仅 TAN/frame 仅 icrs/
  |dec|≤85°/scale>0/W,H∈[1,20000]/sampler∈{nearest,bilinear} 缺省
  bilinear/parity/bitpix∈{-32,-64}/coverage_output 仅 mask）。
  执行面: tests/backend/p3_resample_probe_main.cpp 探针
  （order/mode/open/nearest/bilinear/pix2ang 六模式）+
  tests/backend/test_p3_resample.py 156 行（test_05_nan_semantics
  tile 内 NaN→C=1+值 NaN/test_06_no_silent_default_open/seam 域界
  1e8-1..12e8+1 连续性 1e-5°）+ test_p3003_parallel_resampler.py
  104 行 + tests/unit/p3_interp_test.cpp 109 行/p3_coverage_test.cpp
  106 行（独立参考实现，非生产自证）。
- 实测偏差（如实登记，DISP/整改不修码）:
  1) DISP-P3RSMP-001: bilinear=切平面四象限最近中心双线性
     （cpp:196-230）；ALG-P3-003 G4 施工规格写"面积重叠分数（投影
     线性化）"——同族一阶插值、Σw=1 不变量一致，离散化方案不同
     （整改归 P3-RSMP-IMPL 决策）。
  2) DISP-P3RSMP-002: cache 逐出 FIFO（cpp:22-36）vs ALG §3 伪代码
     "LRU" 表述（整改归 P3-RSMP-IMPL）。
  3) DISP-P3RSMP-003: p3_resample_check_mode 会话编排层无调用点
     （grep 全仓仅 p3_resample_probe_main.cpp:31 消费）——flux/
     variance 拒绝守卫能力在内核、会话未接线（整改归 P3-RSMP-INT）。
  4) DISP-P3RSMP-004: provenance.missing_tiles 恒 nullptr
     （p3_session.cpp:265-277）——缺 tile 聚合上报未接线（SCI §9a-9
     要求记 missing；整改归 P3-RSMP-IMPL/INT）。
  5) DISP-P3RSMP-005: astrocs_p3_resample.dll 未建（entrypoint
     MISSING；探针/回归现状内联编译，入口由 P3-RSMP-IMPL 建立）。
- 五门验收: run/local/agent_p3_rsmp_doc/selfcheck.py（结果见
  run/local/agent_p3_rsmp_doc/ 日志；gate1 矩阵 30 modules
  errors=0 warns=4 基线、gate2 pytest 9 passed、gate3 contracts
  增长、gate4 doccheck rc=0、红线域 git status 零输出）。
- 红线遵守: docs/science/ 根公式零改动；lib/phase3_session/ 生产源
  .cpp/.h 零改动；ci/、.github/、tools/、tests/ 零改动；批次 P
  （tests/backend、tests/cli）在途域只读不动；本任务零 git 操作。
