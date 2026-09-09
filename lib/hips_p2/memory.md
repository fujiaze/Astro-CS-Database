# lib/hips_p2 模块记忆

## 模块目标（P2-HIPS-DOC，2026-09-09）

冻结 `astrocs.p2.hips_writer`（MOD-astrocs-phase2-hips-writer）合同三件套：
Phase2 马赛克写出编排——把多帧 UPM 校准 + 排异 + 加权积分结果写为标准
IVOA HiPS 马赛克（signal + support 两个 Image HiPS），登记输入哈希链、
ivar 权重门、四概念分离（signal/variance/support/mask）、ACR/CPU 路由、
序转换合同与 HIPS_VERIFY。本任务是 -DOC 合同冻结（P2-HIPS-DOC，
wave W1，owner SA-P2-I23，lock-P2-HIPS），不是实现/迁移/测试任务。

## 落位决策（实测依据，防后续误改）

- 矩阵 legacy_paths="lib/phase2 write sources;lib/healpix_db"；实测唯一
  生产源 = lib/phase2/tools/stage2.cpp（1762 行，astrocs-stage2 工具，
  lib/phase2/CMakeLists.txt:103-110）+ 共用 writer 库
  lib/astro_image_io/src/hips/aio_hips_writer.cpp（P1-HIPS 冻结域）。
- lib/phase2/ 三件套已被 P2-COV（astrocs.p2.coverage）占用（README r1，
  2026-09-07），一目录一套合同不可覆盖；lib/hips/ 是 P1-HIPS 迁移目标。
  故按 P1-HIPS 先例新建迁移目标目录 **lib/hips_p2/**（本目录，仅合同
  三件套，无源码；astrocs_p2_hips_writer.dll 落码归 P2-HIPS-IMPL）。
- lib/healpix_db 侧生产参与仅 astro_sphere_sink.cpp（P1 写通道），
  引用不重归属。
- ID 方案：ALG-P2-HIPS-001..004（新文档 docs/algorithms/
  PHASE2_MOSAIC_WRITE.md）；DATA-P2-HIPS（DATA_SEMANTICS §20 追加）；
  API-P2-HIPS-001（PUBLIC_API 新节）；TEST-P2-HIPS-001（登记面=ALG 文档 §11.4 设计冻结 VERIFIED，COV 先例；
  可执行测试 MISSING 归 P2-HIPS-TEST）；
  SRC-PP2HIPS-001（stage2.cpp 编排 + stage2_common.h config，grep 实测）。
  追溯行沿用既有行 ID MOD-astrocs-phase2-write（module_anchor 指
  registry 页 astrocs.phase2.write.md），不新增行、不改行序。

## 关键源码事实（全部 grep/sed 实测，2026-09-09）

- 生命周期与哈希链：main :112；coverage 两阶段 :173-196；target_order
  禁插值伪装分辨率 rc=3 :203-205；p2_frame_id :218-232（0→rc=4）；
  input_manifest_hash=sha256(sorted frame_id|filter=;order=;frame=;)
  :233-245 → p2_stage2_make_upm_cfg :427-430 → model_hash :437-444 →
  UPM 持久层 + diagnostics :1746。
- ivar 门（weight_mode=2 默认）：AIO_HIPS_RD_IVAR :557；缺 ivar 且
  legacy_allow_weight_fallback=false（默认）→ rc=7 显式科学错误
  :565-578（rc=7 :574）；true 才降级 support 标红 :575-578。
- product_begin :592-597：flags 仅 SIGNAL|SUPPORT（:594）、creator
  ivo://astrocs/phase2、title "AstroCS Phase2 Mosaic"、filter=infos[0] :531。
- tile 循环 :659-1655：覆盖帧 probe :663-671；rejection 解析
  （wbpp group-level :641-659 / astrocs_adaptive tile 级 :674-696）；
  ACR 路由 :733-776（p2_acr_block_eligible 调用 :744-746，仅显式 sigma 且 !large_scale）；p2_block_plan micro-chunk :778-817（safety_factor :784）。
- 逐像素：p2_collect_candidate_stack :1084-1098/:1330-1353（source_indices
  稳定映射）；权重 mode2=ivar/mode0=support×snr²（:1136）/mode1=等权（:1139），
  保守校验 :1402-1431；p2_reject_stack_ex 调用 :1184；p2_integrate_pixel 调用 :1220；逆归一 area=sup×A_cell、
  flux=signal×area（ACR :989-1004；CPU :1227-1228/:1531-1532）。
- HIPS-IMG-001 序合同：集成缓冲 FITS 行主序 → 写前
  nested_local_to_fits_index(i,9,512)（ACR :1024-1040 注释含 16px comb
  错排教训、调用 :1032；CPU 侧 :1606-1619、调用 :1611）。
- large_scale 两遍 :1544-1605（p2_large_scale_apply :1549，mask 应用回
  原始 calibrated 值二次积分）。
- 收尾：finalize :1638-1648；HIPS_VERIFY 回读 :1659-1676（rc=7）；
  diagnostics.json 键集 :1697-1747（model_hash :1746），落盘
  out_hips/diagnostics.json :1748-1749。
- 跨模块事实：orchestrator cleanup_partial_output 已用 fs::remove_all
  修复 R5-A P1-1（lib/orchestrator/cpp/src/orchestrator.cpp:425-484，
  HiPS 目录树清理可用；P2-HIPS-DOC 只引用不改动）；p2_session 不执行
  HiPS 写（hips_paths 验证 :81-92、coverage :125/:138；写盘在 stage2）。

## 本任务交付与验收（执行后回填 rc）

- 交付：lib/hips_p2/ 三件套（本目录）；docs/algorithms/
  PHASE2_MOSAIC_WRITE.md（ALG-P2-HIPS-001..004 + DISP-P2HIPS-001..004）；
  DATA_SEMANTICS §20 DATA-P2-HIPS；PUBLIC_API API-P2-HIPS-001 节；
  registry 页 astrocs.phase2.write.md 重写；INDEX.yaml 新增 4 ID 条目
  （ALG-P2-HIPS-001..004/DATA-P2-HIPS/API-P2-HIPS-001，上游=共享 SCI，
  下游互链）；DOCUMENT_INDEX.yaml 注册；TRACEABILITY_MATRIX json/csv
  MOD-astrocs-phase2-write 行转 VERIFIED；selfcheck.py（run/local，
  不提交）。SCI 层零改动；根科学公式零改动；生产源 diff=0。
- known_defects：DISP-P2HIPS-001（无 variance/ivar 输出产品）、002
  （hash 链未入 properties provenance）、003（直写无 staging，IO-003
  承接）、004（O(T·N) 覆盖帧 probe）——登记不改码，整改归
  P2-HIPS-IMPL/INT。
- 验收基线：check_traceability_matrix rc=0 errors=0 warns=4（基线不变）；
  check_contract_graph / check_doc_index PASS；pytest tests/traceability
  PASS；lib 生产源零 diff。日志 run/local/agent_p2_hips_doc/（不提交）。

## 2026-09-10 · AIO-002 注记（原子发布原语已建立于 P1 模块事务面）

- 控制包任务 AIO-002 在 lib/hips 模块事务面（C ABI adapter execute/
  write_product）交付 staging→校验→fsync→原子 promote + staging RAII
  （lib/hips/include/astrocs/hips/publish.h v1 + lib/hips/src/aio_publish.cpp
  唯一实现；验证面 tests/unit/p1_hips/publish_atomic_test.c，
  TEST-P1-HIPS-PUBLISH-001）。生产 writer（aio_hips_writer.cpp）与 P2 写编排
  生产源（lib/phase2/tools/stage2.cpp）零改动——两者均在 AIO-002 写域之外。
- 对 DISP-P2HIPS-003（stage2 直写 out_hips :592 无 staging）的影响：整改
  语义参考面已就绪（非空目标拒绝/staging 自愈/ENOSPC discard 收敛），接线
  仍归 P2-HIPS-IMPL（astrocs_p2_hips_writer.dll 落码时消费同构原语，或
  P2-XX-INT 编排层经 IO-003 发布合同承接）。本登记不改 README/module.yaml
  合同面（DISP-P2HIPS-003 登记状态不变）。
- 写域纪律：本任务未触碰 lib/phase2/**（域外生产源）。
