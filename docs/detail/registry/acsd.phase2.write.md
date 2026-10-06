# 模块 acsd.p2.hips_writer（MOD-acsd-phase2-write）

> 上游：`docs/ACSD_DESIGN.md`对应章节（模块与 ABI）、对应章节（I/O 与原子产品：原子发布条款）
> 科学正本：docs/science/algorithms/PHASE2_MOSAIC_WRITE.md（ALG-P2-HIPS-001..004；
> 对应章节 四概念分离、对应章节/对应章节 测试设计、对应章节 冻结容差）
> 共享 FROZEN SCI（零改动）：docs/science/PHASE2_UPM.md（`w_UPM` 唯一冻结式 对应章节）、
> docs/science/INTEGRATION.md（signal / sup_max）、docs/science/REJECTION.md（排异判据）
> 数据正本：docs/detail/registry/acsd.phase2.write.md（DATA-P2-HIPS 端口表，本页输入输出端口表）
> API 正本：docs/engineering/api/PUBLIC_API.md（API-P2-HIPS-001，Phase2 mosaic write 节）
> I/O：docs/detail/infrastructure/aio.md（IO-002 读合同、IO-003 原子发布）

模块级事实以 `lib/algorithms/coverage/hips_p2/` 三件套（README + module.yaml +
memory.md，CONTRACT_READY，entrypoint 未落地）为准；module.yaml 登记
MOD-acsd-phase2-hips-writer，module_id = `acsd.p2.hips_writer`，dll_target =
`acsd_p2_hips_writer.dll`。唯一生产源 = lib/algorithms/coverage/tools/stage2.cpp
（`acsd-stage2` 工具，lib/algorithms/coverage/CMakeLists.txt）+ config 层
lib/algorithms/coverage/include/astro/phase2/stage2_common.h（`P2Stage2Config`）。
共用 writer 库 lib/infrastructure/aio/src/hips/aio_hips_writer.cpp 属 P1-HIPS
冻结域（ALG-HIPS-001..005），本模块是**库消费者**；
`lib/infrastructure/aio/healpix_db` 侧的生产参与仅
`healpix_drizzle/astro_sphere_sink.cpp`（P1 写通道，引用不归属本模块）。

owner = SA-P2-I23；depends_on_int = P2-INT / IO-003；legacy_paths =
「lib/algorithms/coverage write sources; lib/infrastructure/aio/healpix_db」。

## 职责与明确非职责

**负责**（stage2.cpp HIPS_WRITE 域，ALG-P2-HIPS-001..004）：

- 输入哈希链：p2_frame_id → canonical manifest（frame_id|
  filter=;order=;frame=; 排序拼接）→ sha256 `input_manifest_hash`
  → UPM 构建（p2_stage2_make_upm_cfg）→ model_hash
  → UPM 持久层 + diagnostics.json（均 stage2.cpp）。
- 马赛克编排：DISCOVER/COVERAGE_UNION/CONTROL_SAMPLE/UPM_FIT/
  UPM_PERSIST/BLOCK_PLAN/REJECT+INTEGRATE+HIPS_WRITE/HIPS_VERIFY
  （stage2.cpp）；**target_order 禁插值伪装分辨率** —— 高于输入最高 order → rc=3
  （stage2.cpp）。
- 写出：aio_hips_product_begin（nside=1<<(target_order+9)、dtype
  、flags 仅 SIGNAL|SUPPORT、creator "ivo://acsd/phase2"）→
  逐 tile 排异+积分（p2_collect_candidate_stack/p2_reject_stack_ex/
  p2_integrate_pixel）→
  逆归一（area=sup×A_cell、flux=signal×area；均 aio_hips_writer.cpp）——
  ⚠ **该 `flux` 是 writer 视图的中间量**（`aio_hips_write_signal_support_tile` 的入参口径），
  **落盘值 = `flux_sum / covered_area` = 面亮度**（写端口 `UnitId::SURFACE_BRIGHTNESS`；**产品语义 = 面亮度**；
  通量语义只在 writer 视图内部存在，产品语义 = 面亮度）→
  FITS 序→NESTED 序转换（HIPS-IMG-001，nested_local_to_fits_index
  见 aio_hips_writer.cpp）→ aio_hips_write_signal_support_tile/
  aio_hips_finalize；HIPS_VERIFY 回读（stage2.cpp）。
- ivar 权重门：缺 ivar 产品（默认）→ rc=7 显式科学错误（stage2.cpp，rc=7 在其内）；
  禁 support 冒充 ivar。

**不负责**：单帧校准 / 星点检测 / PSF 建模 / plate solve（输入由 Phase1 帧 HiPS
提供）；叶级归一/FITS 写盘/hierarchy/MOC/properties（writer 库
aio_hips_writer.cpp，P1-HIPS 域 ALG-HIPS-001..005）；HiPS 格式解析
（IO-002/aio_hips_reader）；原子发布（IO-003 编排层；阶段二直写 out_hips 无
staging，不满足最高设计对应章节 的原子发布条款，属已登记的例外面）。
UPM/排异/积分公式（SCI-UPM-001/SCI-REJ-001/SCI-INT-001
FROZEN，w_UPM 唯一冻结式 PHASE2_UPM.md 对应章节）；P3 HiPS→FITS。

**阶段二数据面**：帧 HiPS（signal / support / SNR catalogue）；UPM sparse 模型
（落盘标识 `acsd-upm-v2`，DATA-UPM-MODEL-001）；马赛克产品 signal / support
HiPS（variance / ivar 为可选诊断）。

**阶段二配置面**：single JSON（模型 / integration / output），typed parser +
schema 单一来源。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `integrated` | `DATA-P2-INT` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL`（descriptor 词汇） |
| `mosaic` | `DATA-P2-HIPS`（descriptor 端口词汇 `DATA-P2-RES`；权威 = DATA-P2-HIPS） | 可 | `UnitId::SURFACE_BRIGHTNESS`（signal = 面亮度；枚举源 `lib/include/acsd/core/artifact.h`，phase3 同用） | NESTED 球面（HEALPix nside=2^(target_order+9)，tile 512×512） |

权威源 = DATA-P2-HIPS（本页输入输出端口表）；descriptor 端口表（`p2_write_descriptor`，
坐标记为 PIXEL）为编排词汇，球面端口以 DATA-P2-HIPS（NESTED 球面）为准。
invalid = NaN signal + support = 0（writer 库 aio_hips_writer.cpp）；ivar 缺产品 =
rc=7 science / degraded（stage2.cpp）。

**四概念分离红线（ALG-P2-HIPS）**：signal（SCI-INT 加权积分）、
variance / ivar（输入侧逐帧产品权重语义）、support（sup_max 几何覆盖 [0,1]）、
mask（rejection reasons + large_scale grow，**不输出产品、不入权重式**）严格分离；
**禁 support 冒充 ivar**。

## 公共 header、核心 symbol 与生命周期

生产入口 stage2.cpp `main`（CLI `--cpu-workers` / `--io-workers` / `--gpu-route` /
`--deterministic`，CON-002）；阶段二头文件族 = `astro/phase2/{upm, stage2_common,
coverage, sampler, rejection, block, integrate}.h`（`P2_API` /
`extern "C"`）；config 层 stage2_common.h（`p2_stage2_parse_config` /
`p2_stage2_make_upm_cfg` / `p2_acr_block_eligible`）；编排消费
`p2_collect_candidate_stack` / `p2_reject_stack_ex` / `p2_integrate_pixel` /
`p2_large_scale_apply` / `p2_block_plan`（lib/algorithms/coverage 冻结接口）；库消费
aio_hips.h 的 9 个 C ABI 符号（P1 冻结面）。源文件 =
`lib/algorithms/coverage/src/`、`lib/algorithms/coverage/include/astro/phase2/`、
`lib/algorithms/coverage/tools/`。头文件族中与冻结接口并列的 ACR 头
（`acr_kernels.h`）在 `lib/` 下不存在，集成执行路由唯一 = CPU。

公共 API 登记 = API-P2-HIPS-001（PUBLIC_API Phase2 mosaic write 节，
stage2 配置 schema + 退出码 2/3/4/5/6/7 + diagnostics.json 键集）。

**ivar 与 ACR 的强制口径**：`use_ivar_weight` 由 stage2 显式透传（默认 1）；
`weight_policy = ivar` 时 ACR 块**强制走 CPU**（ACR-IVAR-001）；ivar 产品整体缺失
= 硬科学错误（rc=7），禁 support 冒充 ivar。

## Registry descriptor 与配置 schema

module_id=`acsd.p2.hips_writer`；registry 行 ID = `MOD-acsd-phase2-write`；
descriptor（p2_write_descriptor：module_id=acsd.phase2.write、
sci_id=SCI-P2-WR-001/alg_id=ALG-P2-WR-001/test_id=TEST-P2-WR-001）为编排层
口径，其与 lib/algorithms/coverage/hips_p2/module.yaml 的对齐属迁移目标（未落地）；
冻结依据 = `docs/science/algorithms/PHASE2_MOSAIC_WRITE.md`（ALG-P2-HIPS-001..004）。
配置=single JSON
（P2Stage2Config：reject_profile（工具链默认 wbpp_2_9_1；生产入口默认
acsd_adaptive_pixel）、large_scale 默认关、
memory_limit_mb=24576 等，权威=API-P2-HIPS-001）。

## Execution class、并行轴、ThreadBudget lease、确定性

`io`；writer 单句柄串行（parallel_ok=false；tile 按 cov.n_union_cells
顺序见 stage2.cpp）；CPU reference 逐像素 OMP 并行仅内部（CON-006，
per-worker scratch + thread id 固定顺序定序归并；均 stage2.cpp），large_scale
激活强制串行；ThreadLease/取消检查点未接线（迁移整改点）。
确定性=同输入同 config 同 mosaic（tile 序固定、归并定序、单 writer；
UTC 时间戳字段除外——writer 合同 ALG-HIPS-005）。

性能特征：block planner 做内存估算并据此切块；dense cache 加速面求值。

## 内存/cache/I-O/所有权

memory_limit_mb 经 p2_block_plan（safety_factor=0.75）产出
chunk_pixels micro-chunk；large_scale 全帧 cap 缓冲
（nb×n_leaf）；磁盘空间检查（均 stage2.cpp）；I/O=每帧
signal/support(/ivar) 产品逐 tile 读 + out_hips 单 writer 写 + 可选
diagnostics.json；所有权=stage2 进程内缓冲，输入由 IO-002 读合同交付。

**缓存**：UPM dense cache（按 model_hash 校验，stale = rc=2 拒绝；详见
registry/acsd.phase2.upm-apply.md）。

## 错误、日志、指标、取消和 checkpoint

**acsd-stage2 工具返回值（工具局部，不是 `acsd::ExitCode`）**：0 = 成功；1 = 未捕获
异常兜底；2 = config 解析 / CLI 参数错误；3 = coverage 构建 / target_order 校验；
4 = frame_id / sampler 域；5 = UPM 构建 / 持久化；6 = 写路径 / 集成块（rejection
resolve、tile 写、large_scale 等）；7 = ivar 门（ivar 产品缺失且未显式降级）/
HIPS_VERIFY 回读失败。**该工具不以裸整数冒充进程退出码**：它不消费
`lib/infrastructure/cli/exit_codes.h` 的 `acsd::ExitCode`，两套码值在 3–7 区间重叠
且语义不同，按任一面反查都会取到另一面的错值（该重叠已在
docs/engineering/api/PUBLIC_API.md 的「acsd-stage2 工具返回值」处登记为未决项）。
日志落点 = 块级 `log_dir`（默认 `<output_dir>/logs`），节点事件经 observability
汇聚（最高设计对应章节，阶段二工具侧同时输出 stderr）。指标 = diagnostics.json
（`rejection_resolved_methods` / `reject_hist` / `pixels_depth_*` / `acr_*` route /
model_hash 等；stage2.cpp）。取消 = 无（长 run 无检查点，如实登记）。跨模块：
orchestrator 的 `cleanup_partial_output` 用 `fs::remove_all` 修复（orchestrator.cpp，
失败时清理 HiPS 目录树）。**进程退出码**唯一源 =
lib/infrastructure/cli/exit_codes.h；域→码映射唯一源 =
docs/engineering/contracts/LOG_AND_ERROR.md 的「错误对象与退出码映射」。

**阶段二冻结科学合同 ID 集合**（零改动）：SCI-UPM-001..010、
SCI-UPM-PERSIST-001、ALG-UPM-FRAME-BIND-001、ALG-REJ-001..008、
SCI-INT-001/002/004/008、SCI-NOISE-015、SCI-UPM-WEIGHT-001、
ALG-UPM-CONTROL-IVAR-001、DATA-UPM-CONTROL-UNC-001。`ERR-P2-UPM-001`（畸形模型）
由 UPM 侧承载（见 registry/acsd.phase2.upm-fit.md）。

## 独立 synthetic 验证命令与容差

`TEST-P2-HIPS-001` 待建；设计冻结 = ALG-P2-HIPS-001..004
（PHASE2_MOSAIC_WRITE.md 的测试设计与验收章：NumPy 参考 signal / sup_max
rtol = 1e-12、序转换恒等往返、ivar 门负例）。已取证的相邻读数：ACR
`mosaic_reject_legacy` ↔ CPU 等价（该符号在 `lib/` 下无定义，无 CUDA kernel）、
synthetic_gate UPMW 组、G5 ivar 真值、SNR-015 ablation；这些读数的载体不在本仓
可复算路径上，引用时只作背景。

## 已知限制

- 集成执行路由只有 CPU：`lib/algorithms/coverage/` 下无 ACR 源、无
  `acr_kernels.cpp`、无 `mosaic_reject_legacy` 符号（**无 CUDA kernel**），因此
  「输出仅 signal / support」不是现行限制；
  缺陷登记 = lib/algorithms/coverage/hips_p2/README.md 的
  「已知缺陷（DISP-P2HIPS，登记不改码，整改归 P2-HIPS-IMPL/INT）」段与
  ALG-P2-HIPS-001..004 缺陷清单（登记不改码，整改面未落地）：无 variance / ivar
  输出产品；hash 链未入 HiPS properties provenance；阶段二直写 `out_hips` 无
  staging（原子发布归 IO-003，不满足最高设计对应章节 的原子发布条款，属已登记的
  例外面）；O(T·N) 覆盖帧 probe。

**日志落点**：本模块的日志一律落块级 `<output_dir>/logs`。开发过程日志
不是产品日志，不在产品落盘面内；判据 R3 =
`docs/engineering/contracts/LOG_AND_ERROR.md` 的「落点合同」（落点指向块级
output_dir 之外即判红）。

目标交付形态 acsd_p2_hips_writer.dll 未落地。全局限制登记 =
artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
