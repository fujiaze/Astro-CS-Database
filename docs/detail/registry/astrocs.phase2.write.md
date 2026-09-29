# 模块 astrocs.p2.hips_writer（MOD-astrocs-phase2-write）

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同：科学/算法正本 = docs/science/algorithms/PHASE2_MOSAIC_WRITE.md
> （ALG-P2-HIPS-001..004）；数据合同 = DATA-P2-HIPS（DATA_SEMANTICS §20）；
> C API = API-P2-HIPS-001（PUBLIC_API Phase2 mosaic write 节）。模块级事实以
> lib/algorithms/coverage/hips_p2/README.md（CONTRACT_READY）+ module.yaml
> （MOD-astrocs-phase2-hips-writer，module_id=astrocs.p2.hips_writer，
> dll_target=astrocs_p2_hips_writer.dll）为准；合同三件套落位规则见
> docs/detail/README.md。唯一生产源 lib/algorithms/coverage/tools/stage2.cpp
> （astrocs-stage2 工具，lib/algorithms/coverage/CMakeLists.txt:103-110）+ config 层
> lib/algorithms/coverage/include/astro/phase2/stage2_common.h（P2Stage2Config :16-100）。

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
  p2_integrate_pixel）→
  逆归一（area=sup×A_cell :527、flux=signal×area :1000-1017/:1219-1233）——
  ⚠ **该 `flux` 是 writer 视图的中间量**（`aio_hips_write_signal_support_tile` 的入参口径），
  **落盘值 = `flux_sum / covered_area` = 面亮度**（写端口 `UnitId::SURFACE_BRIGHTNESS`；**产品语义 = 面亮度**；
  通量语义只在 writer 视图内部存在，产品语义 = 面亮度）→
  FITS 序→NESTED 序转换（HIPS-IMG-001，nested_local_to_fits_index
  :1032/:1611）→ aio_hips_write_signal_support_tile/
  aio_hips_finalize；HIPS_VERIFY 回读（:1659-1676）。
- ivar 权重门：缺 ivar 产品（默认）→ rc=7 显式科学错误（:565-578，rc=7 :574）；
  禁 support 冒充 ivar。

**不负责**：叶级归一/FITS 写盘/hierarchy/MOC/properties（writer 库
aio_hips_writer.cpp，P1-HIPS 域 ALG-HIPS-001..005）；HiPS 格式解析
（IO-002/aio_hips_reader）；原子发布（IO-003 编排层；阶段二直写 out_hips 无
staging，不满足最高设计 §9 的原子发布条款，属已登记的例外面）。
UPM/排异/积分公式（SCI-UPM-001/SCI-REJ-001/SCI-INT-001
FROZEN，w_UPM 唯一冻结式 PHASE2_UPM.md §5）；P3 HiPS→FITS。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `integrated` | `DATA-P2-INT` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL`（descriptor 词汇） |
| `mosaic` | `DATA-P2-HIPS`（descriptor 端口词汇 `DATA-P2-RES`；权威 = DATA-P2-HIPS） | 可 | `UnitId::SURFACE_BRIGHTNESS`（signal = 面亮度；枚举源 `lib/include/astrocs/core/artifact.h:36`，phase3 同用） | NESTED 球面（HEALPix nside=2^(target_order+9)，tile 512×512） |

权威源 = DATA-P2-HIPS（DATA_SEMANTICS §20）；descriptor 端口表（`p2_write_descriptor`，
坐标记为 PIXEL）为编排词汇，球面端口以 DATA-P2-HIPS（NESTED 球面）为准。
invalid = NaN signal + support=0（writer 库 aio_hips_writer.cpp :481-485）；ivar 缺产品=rc=7
science/degraded（:565-574）。四概念分离红线：signal/variance/support/
mask 严格分离，mask 不输出产品不入权重式（ALG-P2-HIPS §7）。

## 公共 header、核心 symbol 与生命周期

生产入口 stage2.cpp `main`（:112，CLI --cpu-workers/--io-workers/
--gpu-route/--deterministic CON-002 :142-159）；config 层
stage2_common.h（p2_stage2_parse_config :102/p2_stage2_make_upm_cfg
:106/p2_acr_block_eligible :113）；编排消费 p2_collect_candidate_stack/
p2_reject_stack_ex/p2_integrate_pixel/p2_large_scale_apply/p2_block_plan
（lib/algorithms/coverage 冻结接口）；库消费 aio_hips.h 9 C ABI 符号（P1 冻结面）。
公共 API 登记 = API-P2-HIPS-001（PUBLIC_API Phase2 mosaic write 节，
stage2 配置 schema + 退出码 2/3/4/5/6/7 + diagnostics.json 键集）。

## Registry descriptor 与配置 schema

module_id=`astrocs.p2.hips_writer`；registry 行 ID = `MOD-astrocs-phase2-write`；
descriptor（p2_write_descriptor：module_id=astrocs.phase2.write、
sci_id=SCI-P2-WR-001/alg_id=ALG-P2-WR-001/test_id=TEST-P2-WR-001）为编排层
口径，其与 lib/algorithms/coverage/hips_p2/module.yaml 的对齐属迁移目标（未落地）；
冻结依据 = `docs/science/algorithms/PHASE2_MOSAIC_WRITE.md`（ALG-P2-HIPS-001..004）。
配置=single JSON
（P2Stage2Config :16-99：reject_profile（工具链默认 wbpp_2_9_1；生产入口默认
astrocs_adaptive_pixel）、large_scale 默认关、acr_route=auto、
memory_limit_mb=24576 等，权威=API-P2-HIPS-001）。

## Execution class、并行轴、ThreadBudget lease、确定性

`io`；writer 单句柄串行（parallel_ok=false；tile 按 cov.n_union_cells
顺序 :661）；CPU reference 逐像素 OMP 并行仅内部（CON-006 :1288-1317，
per-worker scratch + thread id 固定顺序定序归并 :1315-1317），large_scale
激活强制串行；ThreadLease/取消检查点未接线（迁移整改点）。
确定性=同输入同 config 同 mosaic（tile 序固定、归并定序、单 writer；
UTC 时间戳字段除外——writer 合同 ALG-HIPS-005）。

## 内存/cache/I-O/所有权

memory_limit_mb 经 p2_block_plan（safety_factor=0.75 :784）产出
chunk_pixels micro-chunk（:785-820）；large_scale 全帧 cap 缓冲
（nb×n_leaf :847-856）；磁盘空间检查（:464-467/:581-583）；I/O=每帧
signal/support(/ivar) 产品逐 tile 读 + out_hips 单 writer 写 + 可选
diagnostics.json；所有权=stage2 进程内缓冲，输入由 IO-002 读合同交付。

## 错误、日志、指标、取消和 checkpoint

退出码 2=eng/packaging/config/CLI、3=coverage/target_order、4=frame_id/sampler、
5=UPM、6=tile 读写/块不可行/finalize、7=ivar 门/HIPS_VERIFY；日志
run/logs/phase2/<YYYYMMDD>/stage2.log（:161-165）+ stderr；指标=
diagnostics.json（rejection_resolved_methods/reject_hist/pixels_depth_*/
acr_* route/model_hash 等 :1697-1747）；取消=无（长 run 无检查点，
如实登记）；跨模块：orchestrator cleanup_partial_output 已
fs::remove_all 修复（orchestrator.cpp:425-484，失败清理 HiPS 目录树）。

## 独立 synthetic 验证命令与容差

`TEST-P2-HIPS-001` 待建；设计冻结 = ALG-P2-HIPS-001..004
（PHASE2_MOSAIC_WRITE.md §8/§9：NumPy 参考 signal/sup_max rtol=1e-12、序转换
恒等往返、ivar 门负例）。现状相邻证据：phase2_synthetic_gate ACR
mosaic_reject_legacy↔CPU 等价（synthetic_gate.cpp:3021-3160）、
eng/tests/api/test_reject_integration_oracle.py。

## 已知限制

缺陷登记 = lib/algorithms/coverage/hips_p2/README.md §7：无 variance/ivar 输出产品；
hash 链未入 HiPS properties provenance；阶段二直写 out_hips 无 staging
（原子发布归 IO-003）；O(T·N) 覆盖帧 probe。迁移目标
astrocs_p2_hips_writer.dll（未落地）；见 docs/KNOWN_LIMITATIONS.md。
