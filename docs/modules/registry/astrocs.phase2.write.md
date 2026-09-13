---
id: MOD-astrocs-phase2-write
version: 1.0.0
status: ACTIVE
owner: SA-P2-I23
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-UPM-001, SCI-INT-001, SCI-REJ-001, ALG-P2-HIPS-001, ALG-P2-HIPS-002, ALG-P2-HIPS-003, ALG-P2-HIPS-004, API-P2-HIPS-001, API-P2-001]
downstream: [TEST-P2-HIPS-001, DATA-P2-HIPS]
---

# 模块 astrocs.p2.hips_writer（registry 行 MOD-astrocs-phase2-write）

> P2-HIPS-DOC 手写合同页（2026-09-09，SA-P2-I23）：本页重写旧 registry
> 占位页（旧页以 module_adapters.cpp descriptor 为唯一源——编排层
> 词汇不得作为冻结依据，本页手写登记事实修订；registry 页保留先例
> astrocs.phase1.session.md / astrocs.phase1.star-detection.md /
> astrocs.phase2.coverage.md）。模块级事实以 lib/hips_p2/README.md
> （r1，CONTRACT_READY）+ lib/hips_p2/module.yaml（MOD-astrocs-phase2-
> hips-writer，module_id=astrocs.p2.hips_writer，dll_target=
> astrocs_p2_hips_writer.dll，entrypoint=MISSING）为准。唯一生产源
> lib/phase2/tools/stage2.cpp（1762 行，astrocs-stage2 工具，
> lib/phase2/CMakeLists.txt:103-110）+ config 层
> lib/phase2/include/astro/phase2/stage2_common.h（P2Stage2Config :16-100），
> 全部行锚 grep/sed 实测（P2-HIPS-DOC，2026-09-09），禁止手抄他版。

## 职责与明确非职责

**负责**（stage2.cpp HIPS_WRITE 域，ALG-P2-HIPS-001..004）：

- 输入哈希链：p2_frame_id（:218-232）→ canonical manifest（frame_id|
  filter=;order=;frame=; 排序拼接 :233-243）→ sha256 `input_manifest_hash`
  :243-245 → UPM 构建（p2_stage2_make_upm_cfg :427-431）→ model_hash
  （:437-444）→ UPM 持久层 + diagnostics.json（:1746）。
- 马赛克编排：DISCOVER/COVERAGE_UNION/CONTROL_SAMPLE/UPM_FIT/
  UPM_PERSIST/BLOCK_PLAN/REJECT+INTEGRATE+HIPS_WRITE/HIPS_VERIFY
  （:172-1762）；target_order 高于输入最高 order 拒绝（:199-204）。
- 写出：aio_hips_product_begin（nside=1<<(target_order+9) :525、dtype
  :528、flags 仅 SIGNAL|SUPPORT :594、creator "ivo://astrocs/phase2"）→
  逐 tile 排异+积分（p2_collect_candidate_stack/p2_reject_stack_ex/
  p2_integrate_pixel，权重 mode2=ivar/mode0=support×snr²/mode1=等权）→
  逆归一（area=sup×A_cell :527、flux=signal×area :1000-1017/:1219-1233）→
  FITS 序→NESTED 序转换（HIPS-IMG-001，nested_local_to_fits_index
  :1032/:1611）→ aio_hips_write_signal_support_tile/
  aio_hips_finalize；HIPS_VERIFY 回读（:1659-1676）。
- ivar 权重门（matrix 专项）：缺 ivar 产品且
  legacy_allow_weight_fallback=false（默认）→ rc=7 显式科学错误
  （:565-578，rc=7 :574）；禁 support 冒充 ivar。

**不负责**：叶级归一/FITS 写盘/hierarchy/MOC/properties（writer 库
aio_hips_writer.cpp，P1-HIPS 域 ALG-HIPS-001..005）；HiPS 格式解析
（IO-002/aio_hips_reader）；原子发布（IO-003 编排层，DISP-P2HIPS-003
如实登记）；UPM/排异/积分公式（SCI-UPM-001/SCI-REJ-001/SCI-INT-001
FROZEN，w_UPM 唯一冻结式 PHASE2_UPM.md §5）；P3 HiPS→FITS。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `integrated` | `DATA-P2-INT` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL`（descriptor 词汇） |
| `mosaic` | `DATA-P2-RES`→`DATA-P2-HIPS` | 可 | `UnitId::ADU`（signal surface brightness） | NESTED 球面（HEALPix nside=2^(target_order+9)，tile 512×512） |

唯一权威 = DATA-P2-HIPS（DATA_SEMANTICS §20）：descriptor 端口表
（module_adapters.cpp:739-756 p2_write_descriptor，坐标 PIXEL 与 NESTED
球面实际不符）为编排词汇，以 DATA-P2-HIPS 为准修订，P2-XX-INT 对齐。
invalid = NaN signal + support=0（writer 库 aio_hips_writer.cpp :481-485）；ivar 缺产品=rc=7
science/degraded（:565-574）。四概念分离红线：signal/variance/support/
mask 严格分离，mask 不输出产品不入权重式（ALG-P2-HIPS §7）。

## 公共 header、核心 symbol 与生命周期

生产入口 stage2.cpp `main`（:112，CLI --cpu-workers/--io-workers/
--gpu-route/--deterministic CON-002 :142-159）；config 层
stage2_common.h（p2_stage2_parse_config :102/p2_stage2_make_upm_cfg
:106/p2_acr_block_eligible :113）；编排消费 p2_collect_candidate_stack/
p2_reject_stack_ex/p2_integrate_pixel/p2_large_scale_apply/p2_block_plan
（lib/phase2 冻结接口）；库消费 aio_hips.h 9 C ABI 符号（P1 冻结面）。
公共 API 登记 = API-P2-HIPS-001（PUBLIC_API Phase2 mosaic write 节，
stage2 配置 schema + 退出码 2/3/4/5/6/7 + diagnostics.json 键集）。

## Registry descriptor 与配置 schema

module_id=`astrocs.p2.hips_writer`（matrix P2-HIPS 行）；registry 行 ID
沿用 `MOD-astrocs-phase2-write`；descriptor（module_adapters.cpp:739-752，
module_id=astrocs.phase2.write、sci_id=SCI-P2-WR-001/alg_id=ALG-P2-WR-001/
test_id=TEST-P2-WR-001）为编排占位词汇，不得反向作为冻结依据，由
P2-XX-INT 对齐本页与 lib/hips_p2/module.yaml。配置=single JSON
（P2Stage2Config :16-99：weight_mode=2、legacy_allow_weight_fallback=
false、reject_profile=wbpp_2_9_1、large_scale 默认关、acr_route=auto、
memory_limit_mb=24576 等，权威=API-P2-HIPS-001）。

## Execution class、并行轴、ThreadBudget lease、确定性

`io`；writer 单句柄串行（parallel_ok=false；tile 按 cov.n_union_cells
顺序 :661）；CPU reference 逐像素 OMP 并行仅内部（CON-006 :1288-1317，
per-worker scratch + thread id 固定顺序定序归并 :1315-1317），large_scale
激活强制串行；ThreadLease/取消检查点未接线（迁移整改点，P2-HIPS-IMPL）。
确定性=同输入同 config 同 mosaic（tile 序固定、归并定序、单 writer；
UTC 时间戳字段除外——writer 合同 ALG-HIPS-005）。

## 内存/cache/I-O/所有权

memory_limit_mb 经 p2_block_plan（safety_factor=0.75 :784）产出
chunk_pixels micro-chunk（:785-820）；large_scale 全帧 cap 缓冲
（nb×n_leaf :847-856）；磁盘空间检查（:464-467/:581-583）；I/O=每帧
signal/support(/ivar) 产品逐 tile 读 + out_hips 单 writer 写 + 可选
diagnostics.json；所有权=stage2 进程内缓冲，输入由 IO-002 读合同交付。

## 错误、日志、指标、取消和 checkpoint

退出码 2=config/CLI、3=coverage/target_order、4=frame_id/sampler、
5=UPM、6=tile 读写/块不可行/finalize、7=ivar 门/HIPS_VERIFY；日志
run/logs/phase2/<YYYYMMDD>/stage2.log（:161-165）+ stderr；指标=
diagnostics.json（rejection_resolved_methods/reject_hist/pixels_depth_*/
acr_* route/model_hash 等 :1697-1747）；取消=无（长 run 无检查点，
如实登记）；跨模块：orchestrator cleanup_partial_output 已
fs::remove_all 修复（orchestrator.cpp:425-484，失败清理 HiPS 目录树）。

## 独立 synthetic 验证命令与容差

`TEST-P2-HIPS-001` 登记面=ALG 文档 §11.4 设计冻结 VERIFIED（COV TEST-COV-DESIGN-001
先例；可执行测试 MISSING 归 P2-HIPS-TEST，不冒认）；
TEST-DESIGN 与容差来源=ALG-P2-HIPS-001..004（PHASE2_MOSAIC_WRITE.md
§8/§9：NumPy 参考 signal/sup_max rtol=1e-12、序转换恒等往返、ivar 门
负测）。现状相邻证据（引用不冒认）：phase2_synthetic_gate W9 ACR
mosaic_reject legacy↔CPU 等价（synthetic_gate.cpp:3021-3160）、
tests/api/test_reject_integration_oracle.py。

## 已知限制

DISP-P2HIPS-001..004（lib/hips_p2/README.md §7）：无 variance/ivar 输出
产品；hash 链未入 HiPS properties provenance；直写 out_hips 无 staging
（原子发布归 IO-003）；O(T·N) 覆盖帧 probe。迁移目标
astrocs_p2_hips_writer.dll 归 P2-HIPS-IMPL；见 docs/KNOWN_LIMITATIONS.md。
