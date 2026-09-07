# astrocs.p2.hips_writer — Phase2 HiPS 马赛克写出模块（P2-HIPS）

> P2-HIPS-DOC（2026-09-09，SA-P2-I23）新建模块页。合同三件套落位
> `lib/hips_p2/`（README r1 + module.yaml + memory.md，CONTRACT_READY，
> entrypoint=MISSING）——迁移目标目录按 `lib/hips/`（P1-HIPS）先例新建；
> `lib/phase2/` 三件套已被 P2-COV（astrocs.p2.coverage）占用，不可覆盖。
> 唯一生产源 `lib/phase2/tools/stage2.cpp`（1762 行，astrocs-stage2 工具，
> lib/phase2/CMakeLists.txt:103-110）+ config 层
> `lib/phase2/include/astro/phase2/stage2_common.h`；共用 writer 库
> `lib/astro_image_io/src/hips/aio_hips_writer.cpp` 为 P1-HIPS 冻结域
> （ALG-HIPS-001..005），本模块为库消费者；`lib/healpix_db` 侧生产参与
> 仅 `healpix_drizzle/astro_sphere_sink.cpp`（P1 写通道，引用不归属）。

## 身份与合同

- MOD ID：`MOD-astrocs-phase2-hips-writer`（registry 行 ID 沿用
  `MOD-astrocs-phase2-write`）；module_id：`astrocs.p2.hips_writer`
  （MODULE_MIGRATION_MATRIX P2-HIPS 行）；dll_target：
  `astrocs_p2_hips_writer.dll`（合同值，尚未存在，迁移归 P2-HIPS-IMPL）。
- owner SA-P2-I23；depends_on_int=P2-INT-INT;IO-003；legacy_paths=
  "lib/phase2 write sources;lib/healpix_db"。
- 合同链：SCI-UPM-001（w_UPM）+ SCI-INT-001（signal/sup_max）+
  SCI-REJ-001（排异判据）——共享 FROZEN SCI 零改动 → ALG-P2-HIPS-001..004
  （docs/algorithms/PHASE2_MOSAIC_WRITE.md）→ DATA-P2-HIPS
  （DATA_SEMANTICS §20）/ API-P2-HIPS-001（PUBLIC_API Phase2 mosaic
  write 节）→ TEST-P2-HIPS-001（登记面=ALG 文档 §11.4 设计冻结 VERIFIED，
  COV 先例；可执行测试 MISSING 归 P2-HIPS-TEST）。

## 职责（摘要，权威=lib/hips_p2/README.md §2）

- 输入哈希链：p2_frame_id → canonical manifest（frame_id|filter=;order=;
  frame=; 排序）→ sha256 input_manifest_hash → UPM model_hash → UPM 持久
  层 + diagnostics.json。
- 马赛克编排生命周期 DISCOVER→COVERAGE_UNION→CONTROL_SAMPLE→UPM_FIT→
  UPM_PERSIST→BLOCK_PLAN→REJECT+INTEGRATE+HIPS_WRITE→HIPS_VERIFY；
  target_order 禁插值伪装分辨率（高于输入最高 order → rc=3）。
- 逐 tile 排异+加权积分（weight_mode=2 ivar 权重门：缺 ivar 产品且
  legacy_allow_weight_fallback=false → rc=7 显式科学错误）→ 逆归一
  （area=sup×A_cell、flux=signal×area）→ FITS 序→NESTED 序转换
  （HIPS-IMG-001）→ writer 库写 signal+support 两产品 → HIPS_VERIFY 回读。
- 非职责：叶级归一/FITS 写盘/hierarchy/MOC/properties（writer 库 P1 域）；
  原子发布（IO-003 编排层）；UPM/排异/积分公式（SCI FROZEN）；P3。

## 关键合同事实

- 四概念分离（matrix 专项）：signal（SCI-INT §5 加权积分）、variance/ivar
  （输入侧逐帧产品权重语义）、support（sup_max 几何覆盖 [0,1]）、mask
  （rejection reasons + large_scale grow，不输出不入权重式）；禁 support
  冒充 ivar。
- 并发：writer 单句柄串行（parallel_ok=false）；CPU reference 逐像素 OMP
  并行仅内部（CON-006 定序归并）；large_scale 激活强制串行。
- 确定性：同输入同 config → 同 mosaic（tile 序固定、归并定序、单 writer；
  UTC 时间戳字段除外）。
- known_defects：DISP-P2HIPS-001..004（无 variance/ivar 输出产品；hash 链
  未入 properties provenance；直写无 staging 归 IO-003 承接；O(T·N) probe）
  ——登记不改码，整改归 P2-HIPS-IMPL/INT。

## 验证

可执行 `TEST-P2-HIPS-001` MISSING（P2-HIPS-TEST 建立）；登记面=设计冻结 VERIFIED；
设计内容与容差来源=
ALG-P2-HIPS-001..004（PHASE2_MOSAIC_WRITE.md §8/§9）。现状相邻证据：
phase2_synthetic_gate W9 ACR mosaic_reject legacy↔CPU 等价
（synthetic_gate.cpp:3021-3160）、tests/api/test_reject_integration_oracle.py
（引用不冒认）。
