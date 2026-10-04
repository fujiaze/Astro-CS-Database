# 模块 acsd.phase2.upm-apply

> 上游：docs/ACSD_DESIGN.md §8.5（模块与 ABI）、§5.4（天光平面与统一相对模型 UPM）
> 科学正本：docs/science/PHASE2_UPM.md（SCI-UPM-001，FROZEN；§5/§9a/§14a/§17）、
> docs/science/algorithms/UPM_SOLVER.md（ALG-UPM-001，权威推导；F4/F6）、
> docs/science/noise_snr/NOISE_SNR.md §3.5（参数协方差）
> 实现级合同：docs/science/algorithms/PHASE2_UPM_IMPL.md（ALG-P2-UPM-IMPL-001；
> §11/§13 语义与缺陷清单、TEST-DESIGN 冻结容差、字段名与生产取值登记）
> 数据正本：docs/detail/registry/acsd.phase2.upm-apply.md（DATA-P2-UPM / DATA-P2-COR 端口表，本页输入输出端口表）
> API 正本：docs/engineering/api/PUBLIC_API.md（API-P2-UPM-001）、
> docs/engineering/api/PUBLIC_API.md「分阶段 API 面」（API-P2-001，FROZEN）

MOD ID = `MOD-acsd-phase2-upm-apply`；module_id 合同值 = `acsd.p2.upm`
（descriptor 词汇 `acsd.phase2.upm-apply` 为编排层口径，其对齐属迁移目标、未
落地）；dll_target = `acsd_p2_upm.dll`（迁移目标，未落地）。合同三件套落位
`lib/algorithms/upm/`（README / module.yaml ，CONTRACT_READY）。
生产源 = lib/algorithms/coverage/src/upm.cpp + 权威签名头
lib/algorithms/coverage/include/astro/phase2/upm.h；构建 = 根 CMakeLists 的
`acsd_phase2` 静态库成员。owner = SA-P2-U21；depends_on_int = P2-SAMP / CPU-005；
legacy_paths = 「lib/algorithms/coverage upm sources」。

fit 职能见 registry/acsd.phase2.upm-fit.md（同一 module_id 的另一职能）。

## 职责与明确非职责

职责（apply）：按 frame_id 稳定绑定逐块校准 —— **默认只扣偏差 `δ_k`、保留公共
天光面 `B_ref`**（最高设计 §5.4「公共面语义：只扣『多退少补』的偏差，不剪掉整个
背景」）。**记法消歧（强制）**：表示层全量 `C_k ≡ B_ref + δ_k`；**全量扣除
`raw − C_k`（含 `B_ref`）不是默认** —— 凡写 `raw − C_k` 处必须写明「全量」还是
「仅偏差」。

内核锚 = `p2_upm_calibrate_block`，与 `p2_upm_evaluate_c` 的 sparse / dense 走同一
科学语义（ALG-UPM-001 F4）。dense cache 物化 / 读取：`p2_upm_materialize_dense_n`
分批并行求值 → (f, tile) 单调序串行写、bit-identical；`p2_upm_dense_read_block`
stale 拒绝 rc = 2。生产 apply 消费链 = lib/algorithms/coverage/tools/stage2.cpp
（四处调用点：`p2_upm_build_geo`、`save`、`materialize_dense_n`、
`calibrate_block` ×2）。

不做：UPM 拟合 / 权重归一（fit 职能）；控制点采样 / 几何、coverage union、积分、
排异、马赛克写出；跨滤镜统一（模型 filter 分组由调用方保证）；per-frame gradient
产品的对外暴露（upm.h 冻结）；session 依赖（模型数据面显式传入）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（p2_upm_apply_descriptor，编排层口径）：

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `upm_model` | `DATA-P2-UPM` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `calibrated_frames` | `DATA-P2-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `corrected` | `DATA-P2-COR` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同 = DATA-P2-UPM（§25）模型 + DATA-P2-CAL 帧 → DATA-P2-COR
（§26）校准输出：**默认语义 = 只扣偏差**（FP64 ADU，保留公共天光面 `B_ref`）；
sparse / dense 同一科学语义。

invalid：null 参数 ⇒ rc = 1；未知 frame_id ⇒ `p2_upm_evaluate_c` 返回 NaN
（显式不可用，**禁 frame 0 参数伪装**）；`dense_read_block` source hash 不匹配
⇒ rc = 2 stale 拒绝（upm.h）。

## 公共 header、核心 symbol 与生命周期

签名头正本 = lib/algorithms/coverage/include/astro/phase2/upm.h
（`P2ModelInfo`、`calibrate_block`、`dense_read_block`）。

核心 symbol（upm.cpp 导出，extern "C"）：`p2_upm_build`、`p2_upm_build_geo`、
`p2_upm_save`、`p2_upm_open`、`p2_upm_info`、`p2_upm_calibrate_block`、
`p2_upm_evaluate_c`、`p2_upm_raw_weight`、`p2_upm_geometry_hash`、
`p2_upm_component_gauges`、`p2_upm_materialize_dense_n`、
`p2_upm_materialize_dense`、`p2_upm_dense_info`、`p2_upm_dense_read_block`、
`p2_upm_close`。`p2_upm_normalized_weights` 在 upm.cpp / upm.h 带 RETIRED 注记，
**全仓零消费者**，不列入在役导出。`p2_upm_materialize_dense` 的 wrap 恒传
`workers = 0` ⇒ **单线程串行，不是 auto**。

生命周期 = open（或 fit 侧 build 产物传递）→ info / dense_info →
calibrate_block / evaluate_c / dense_read_block → materialize → `p2_upm_close`
（delete Model；nullptr = rc 0 幂等）。模型 / 缓冲所有权 = 调用方。ABI = c++17；
api_id = API-P2-001。

entrypoint = 模型 + 帧 → 逐块校准后的 DATA-P2-COR 块；下游 rejection /
integration 消费。

## Registry descriptor 与配置 schema

module_id=`acsd.phase2.upm-apply`（占位）；execution_class=`cpu_heavy`;
parallel_ok=True（p2_upm_apply_descriptor）。descriptor 派生词汇（其对齐属迁移
目标，未落地）：SCI-P2-UPM-002 / ALG-P2-UPM-002 / TEST-P2-UPM-002；descriptor
端口为静态声明的 persist→reload 语义。apply 侧无独立求解配置（消费 fit 侧
`P2UpmBuildConfig` 的产物模型）；dense 物化 worker 数经 stage2.cpp 与积分一致传入。

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `seam.additive_mode` | `delta` | —— | 归一施加模式：`delta` = `raw − δ_k`（**设计默认**，保留公共天光面 `B_ref`）/ `c` = `raw − C_k`（全减，非默认）/ `both` = `raw − C_k − δ_k`（双重扣除，仅对照 / 回归） |
| `sky_plane.enabled` | 随 `additive_mode ∈ {delta, both}` | —— | 是否构建 / 落盘公共天光面 `B_ref` 产品；缺省 = 「要施加 `δ_k` 才构建」，显式值优先（实现 = module_adapters.cpp 的 `sp_cfg.value("enabled", delta_wanted)`） |

**设计与实现的默认值不一致，以设计为准**：设计的施加侧默认是 `delta`（仅偏差、
保留 `B_ref`）；实现锚 = 阶段二 apply 节点的 `seam.additive_mode` 读取与施加
分支（按**符号**定位），该读取点的现网默认取 `c`（全量扣除）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`。并行轴 = dense tile 求值（upm.cpp `std::thread` 池，分批求值 →
(f, tile) 单调序串行写，**无 OpenMP**）；worker 数 = **Runtime lease 唯一来源**
（stage2.cpp 传入，无 hardware_concurrency）；**0 = 单线程串行**、1 = reference、
> 1 = 调用方显式给定。

确定性 = 稠密缓存 bit-identical；`calibrate_block` 逐像素独立、无跨样本归并
（worker 数无关）。1/N 科学等价已在并行面上取证，引用不冒认。

**工程等价性**：dense cache 与 sparse `calibrate_block` 在百万点量级上逐点差在
双精度舍入量级（1e-12 等价基线，ALG-UPM-001 F6）；稀疏模型体积与峰值内存均低于
稠密物化面。

## 内存/cache/I-O/所有权

内存 = dense 物化上界 kChunk·kLeafPx·8 字节；模型 `C[frame][control]` 由调用方经
open / build 持有。

I/O = 唯一 AIO `aio_upm_write_sparse`（模型稀疏持久化，落盘标识 `acsd-upm-v2` /
DATA-UPM-MODEL-001，ENG-IO-001 原子写）+ `aio_upm_open` / `aio_upm_dense_info` /
`aio_upm_read_dense_block` 读面；dense cache = 空间求值缓存（同模型 hash / 目标
order / frame hash 校验，**stale = rc=2 拒绝**）。无其他文件 I/O。

所有权 = 调用方分配 model 与缓冲，`p2_upm_close` 是唯一释放口；无全局可变状态。

## 错误、日志、指标、取消和 checkpoint

rc 语义：0 = ok；1 = 参数 / open / parse / IO / 未知 frame；2 = dense stale cache
（source hash 不匹配）。未知 frame_id 的 `evaluate_c` = NaN。**无 `ACS_ERR_*`**
（模块级返回码独立于会话层 ACS 语义）。

- **无天光面产物而 `additive_mode ∈ {delta, both}` ⇒ `δ_k` 不存在 ⇒ 显式退化为 `c`
  （全减，公共天光面被整场扣除）⇒ 判红**。判红面 = 具名
  `degraded_reason = no_sky_plane_artifact` + `warning_codes` 含
  `P2-ADDITIVE-MODE-DEGRADED-NO-SKY-PLANE`，随 `p2_corrected.json` 与节点 manifest
  同时落盘；口径同「产品照出、rc 不变，判红由 `warning_codes` 非空承载」；
  **不得**静默变成「不校正」，**不得**回退到双重扣除；
- 该形态的产品**不得用于「无接缝」主张**：全减后背景归零，接缝判据在分母上退化
  （非退化判据口径见 registry/acsd.phase2.upm-fit.md 与 SCI-UPM-001 §9a/§17）。

取消：会话消费面整模型不写半成品（p2_session.cpp 两处）；内核无取消检查点；无段内
checkpoint（dense 物化整缓存一次写）。

错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-UPM-002` 待建；设计冻结 = ALG-P2-UPM-IMPL-001 的
「TEST-DESIGN（TEST-P2-UPM-DESIGN 冻结）」章，容差权威同该章
（dense / sparse 1e-12 等价基线）。

已取证但载体不在仓内的相邻结论（不冒认）：`calibrate_block` 的信号映射正确且不破坏星
flux；sparse / dense 等价面成立；生产并行面另有实测读数。这些读数不在本仓可复算路径上，
引用时只作背景。

Oracle 面：

- **非退化接缝判据**（见 fit 页）：在**保留 `B_ref`** 的前提下比较帧间一致性；
  判据量取有符号台阶并套适用域；负例 = 注入已知台阶必判红、两侧噪声差大但无台阶
  必判绿、旧方差比口径在同一输入上判绿（盲区复现）；
- **additive_mode 退化判红能红能绿**：无天光面产物 + `delta` ⇒
  `degraded_reason` 与 `warning_codes` 必落；正常帧 ⇒ 两者为空；
- sparse / dense 逐点等价（1e-12 基线）；`calibrate_block` 不破坏星 flux；
- 未知 frame_id 返回 NaN 而非异常、不以 frame 0 伪装；dense stale 拒绝必红；
- 1 worker vs N worker 输出一致。

## 已知限制

- 现行语义与判据正本 = docs/science/algorithms/PHASE2_UPM_IMPL.md §11/§13；
- ALG 边界 = dense cache 为**空间求值缓存**、非科学重算（stale 拒绝语义）；未知
  frame_id → NaN 而非异常；
- 缺陷登记（不改码，正本 = ALG-P2-UPM-IMPL-001 缺陷清单）：upm.h
  materialize_dense 重复声明；upm.h 注释漂移（OpenMP vs std::thread 实现）；
  p2_session.cpp 覆盖键缺口；descriptor 端口静态声明的 persist→reload 语义；
- `seam.additive_mode` 的现网默认取 `c`（全量扣除），与设计默认 `delta` 不一致，
  口径以设计为准；
- 目标交付形态 acsd_p2_upm.dll 未落地；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
