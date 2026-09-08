# memory.md — astrocs.p3.projection（P3-PROJ-DOC 冻结）

- 任务: P3-PROJ-DOC（MODULE_MIGRATION_MATRIX P3-PROJ 行，owner
  SA-P3-P25，2026-09-11）——合同冻结层，不改生产源码，不 commit。
  本目录 `lib/phase3_proj/` 三件套（README r1 + module.yaml +
  memory.md）由 P3-PROJ-DOC 建立。
- 落位: `lib/phase3_proj/`（本目录）。实测生产源
  lib/phase3_session/p3_wcs.cpp + p3_wcs.h 位于 lib/phase3_session/
  ——该目录为 Phase3 会话编排域共享源（p3_session/p3_resample/
  p3_output/p3_wcs/hips_properties 五源同库 astrocs_phase3_session，
  根 CMakeLists.txt:460-465），非整目录归属本域（矩阵
  legacy_paths="lib/phase3_session projection sources" 只圈
  projection sources）；按 lib/phase2_upm→phase2_samp→phase2_rej→
  phase2_int→hips_p2→phase3_fits 迁移目录先例新建迁移目标目录，
  仅合同文件、无源码、不与 legacy 目录重叠；legacy 生产源引用
  不搬家。
- 矩阵权威（P3-PROJ 行，禁止编造）: owner=SA-P3-P25、
  module_id=astrocs.p3.projection、target_dll=
  astrocs_p3_projection.dll、legacy_paths="lib/phase3_session
  projection sources"、depends_on_int=**ABI-005;DATA-004;RT-006**、
  science_specific_acceptance="TAN/SIN/ZEA/CAR/AIT explicit;FITS WCS
  Paper II;domain/wrap/poles;CRPIX/CRVAL/CD;WCSLIB test oracle;
  round-trip thresholds"。
- module_id 决策: 矩阵行 astrocs.p3.projection 为冻结权威依据，
  直接沿用；descriptor 占位 module_id=astrocs.phase3.wcs
  （module_adapters.cpp:344-361 p3_wcs_descriptor）为编排层词汇，
  由 P3-PROJ-INT 对齐，不入合同。
- MOD ID 决策: registry 现有行 MOD-astrocs-phase3-wcs（单
  descriptor 域），module.yaml id 与之一致；registry 页手写化
  （docs/modules/registry/astrocs.phase3.wcs.md，原 GENERATED
  风格改 ACTIVE_INFORMATIVE 手写合同页，照
  astrocs.phase3.writer.md 先例），模块总页新建
  docs/modules/phase3_proj.md。
- 本任务 ID 决策（唯一方案，避免与既有占位冲突）:
  SCI=SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md 共享 FROZEN
  V5 SCI-007 2026-08-28，集合 SCI-P3-001..020；descriptor 占位
  SCI-P3-WCS-001 不入合同，映射声明于 PHASE3_PROJ_IMPL.md §5，
  照 P2-REJ/P3-FITS 先例）；
  ALG=ALG-P3-PROJ-IMPL-001（docs/algorithms/PHASE3_PROJ_IMPL.md
  新建，兼承接既有 ALG-P3-002（G1 CD 构造 + G2 反向映射）本域
  子面；ID 风格与 ALG-P2-UPM-IMPL-001/ALG-P3-FITS-IMPL-001 同族；
  既有 ALG-P3-001..004 登记于 docs/algorithms/PHASE3_RESAMPLE.md
  （该文件属 P3-RSMP 域，本任务零改动），ALG-P3-002 以独立条目入
  INDEX.yaml（条目 status DRAFT→ACTIVE 按索引规则"DERIVED→ACTIVE"
  收敛存量不一致 + downstream 增补 ALG-P3-PROJ-IMPL-001，公式
  零改动）；
  DATA=DATA-P3-WCS（descriptor/matrix 现值沿用；承载=
  DATA_SEMANTICS.md §28 新建，权威落位补齐——此前该 ID 仅存在于
  descriptor 与 registry 页占位，无权威文档）；
  API=API-P3-PROJ-001（PUBLIC_API.md 新节；descriptor 占位
  api_id=API-P3-001 为编排层 FROZEN 合同不变，镜像引用）；
  TEST=TEST-P3-WCS-001（descriptor/matrix 现值沿用；登记面=
  TEST-P3-WCS-DESIGN-001 设计冻结 VERIFIED 承载于 ALG-P3-PROJ-
  IMPL-001 §11 + registry 手写页 §9 双重陈述——照 P2-INT/P2-REJ/
  P3-FITS 先例；可执行面升级归 P3-PROJ-TEST，不冒认。现状执行
  测试三处如实登记: tests/unit/p3_wcs_test.cpp 90 行 +
  tests/backend/test_p1002_gaps.py 独立解析解回归 +
  tests/backend/p3_wcs_main.cpp 探针）。
- 生产源锚（read 实测，2026-09-11）:
  lib/phase3_session/p3_wcs.h（50 行，唯一权威签名头）:
  P3WcsDescriptor :11-20（crval_ra_deg/crval_dec_deg/crpix_x/
  crpix_y FITS 1-based pixel-center/cd[2][2] FITS 顺序 deg/px/
  width_px/height_px/projection="TAN"）、P3WcsStatus :22-27
  （P3_WCS_OK=0/P3_WCS_PARAM=1/P3_WCS_UNSUPPORTED=2/
  P3_WCS_HEMISPHERE=3）、p3_wcs_make :31-34（parity
  "east_left"|"east_right"）、p3_wcs_pix2world :38-39（0-based
  入参 FITS=+1）、p3_wcs_world2pix :42-43、p3_wcs_fits_keywords
  :46（std::string 返回）。
  lib/phase3_session/p3_wcs.cpp（165 行）: 常量 :13-22
  （kMaxAbsDec=85.0、kMaxSide=20000 可 ASTROCS_P3_MAX_SIDE 编译期
  覆盖）、normalize_ra :24-27、p3_wcs_make :30-90（parity 校验
  :39/|dec|>85 拒 :40/scale>0 :41/W,H∈[1,kMaxSide] :42-43/
  crpix=(W+1)/2 :47-48/G1 CD 构造 :51-68：east_left
  CD=diag(−s,+s)·east_right CD=diag(+s,−s)，PA≠0 推广
  CD=R(−PA)·diag(sgn_x·s,sgn_y·s) 展开式；P0 修复注释 :65-68
  bughunt_p0_wcs 旧 east_right 分支 sgn_y 误用；四角同半球守卫
  :80-88）、(void)proj :89（projection 字段硬编码 "TAN" :36）、
  pix2world :92-114（CD^{-1}→gnomonic TAN^{-1} atan2 形式→RA
  wrap 归一）、world2pix :116-140（gnomonic→iwc=CD^{-1}·…→
  denom≤0 背面 Hemisphere）、fits_keywords :145-163（CTYPE1/
  CTYPE2/CUNIT1/CUNIT2/CRPIX1/2/CRVAL1/2/CD1_1..CD2_2，每行
  80 字节内）。
  会话消费: p3_session.cpp :17 include/:160 p3_wcs_make
  （rotation_pa_deg 恒 0.0=PA 未接线实测偏差）/:163 状态映射
  （UNSUPPORTED→ACS_ERR_UNSUPPORTED，其余→ACS_ERR_PARAM）/:232
  worker 循环 pix2world 失败 continue（半球外像素 NaN）/:247-253
  std::thread 池 worker=budget.max_workers 禁
  hardware_concurrency。
  执行面: tests/backend/p3_wcs_main.cpp 探针（make/p2w/w2p/kw
  四模式）+ tests/backend/test_p1002_gaps.py（世界点→world2pix
  →pix2world 独立解析解回归 :115-138）+ tests/unit/p3_wcs_test.cpp
  90 行（WCS 完整性/溢出检查）。
- 实测偏差（如实登记，DISP/整改不修码）:
  1) PA 未接线——p3_session.cpp:160 rotation_pa_deg 恒 0.0，
     内核 PA 能力（:51-68 推广 CD）无会话消费方（整改归
     P3-PROJ-IMPL/INT）。
  2) kMaxSide=20000 可经 ASTROCS_P3_MAX_SIDE 编译期覆盖
     （p3_wcs.cpp:18-22）——合同上限 20000 为默认值语义，如实
     冻结。
  3) projection 字段硬编码 "TAN"（:36/:89 (void)proj）；
     P3_WCS_UNSUPPORTED 枚举现无产生点（备而不用，SIN/ZEA/CAR/
     AIT 扩展 TODO）。
  4) 探针/回归现状走内联编译（非独立 DLL/静态库挂载）——
     astrocs_p3_projection.dll 未建，入口由 P3-PROJ-IMPL 建立。
- 五门验收: run/local/agent_p3_proj_doc/selfcheck.py ALL PASS
  （gate1 矩阵 31 modules errors=0、gate2 pytest 9 passed、
  gate3 contracts=90、gate4 doccheck rc=0、红线域 git status 零
  输出；日志 run/local/agent_p3_proj_doc/）。
- 红线遵守: docs/science/ 根公式零改动；lib/phase3_session/
  生产源 .cpp/.h 零改动；ci/、.github/、tools/、tests/ 零改动；
  批次 P（tests/backend、tests/cli）/批次 Q（lib/core/、lib/common
  io_adapter、cli/main.cpp）在途域只读不动；本任务零 git 操作。
