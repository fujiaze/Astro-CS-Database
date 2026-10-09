# 模块 acsd.phase2.upm-apply

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节与「信号面」一节
> 科学正本：docs/science/sky/UPM.md（SCI-UPM-001，FROZEN，相关章节）、
> docs/science/algorithms/UPM_SOLVER.md（ALG-UPM-001，权威推导；F4/F6）、
> `docs/science/noise_snr/NOISE_SNR.md`「协方差传播」一节
> 实现级合同：docs/science/algorithms/PHASE2_UPM_IMPL.md（ALG-P2-UPM-IMPL-001；
> 语义与缺陷清单、TEST-DESIGN冻结容差、字段名与生产取值登记三节）
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

职责（apply）：按 frame_id 稳定绑定逐块校准 —— **只扣偏差 `δ_k = δ_poly,k + F_k`、
保留公共信号面**（最高设计「信号面」一节「公共面语义：只扣『多退少补』的偏差，不剪掉
整个背景」）。**记法消歧（强制）**：UPM 内核表示层全量 `C_k ≡ B_ref + δ_k`；**全量
扣除 `raw − C_k`（含 `B_ref`）不是生产默认** —— 凡写 `raw − C_k` 处必须写明「全量」
还是「仅偏差」。

生产内核锚 = `p2_sky_plane_eval_delta_block`（信号面：公共面 + 联合解逐帧残差场，
见 §5.11 与 `p2_sky_plane_build_joint`）；本页同 module_id 的 UPM 内核
`p2_upm_calibrate_block` / `p2_upm_evaluate_c` 走同一"仅偏差"科学语义
（ALG-UPM-001 F4），但**不在生产扣除链上**（其逐帧分块场跨瓦片不连续、无观测格取 0，
见「已知限制」）。dense cache 物化 / 读取：`p2_upm_materialize_dense_n`
分批并行求值 → (f, tile) 单调序串行写、bit-identical；`p2_upm_dense_read_block`
stale 拒绝 rc = 2。生产 apply 消费链 = lib/infrastructure/scheduler/src/module_adapters.cpp
的 `p2_op_upm_apply`（`p2_sky_plane_open` + 逐像素 `p2_sky_plane_eval_delta_block`）。

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

内核级真实I/O合同 = DATA-P2-UPM模型 + DATA-P2-CAL帧 → DATA-P2-COR
校准输出：**默认语义 = 只扣偏差**（FP64 ADU，保留公共信号面 `B_ref`）；
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
| 施加语义（唯一） | `corrected = raw − δ_k` | —— | 一次扣除单语义：`δ_k = δ_poly,k + F_k`，`F_k` 为拟合残差上的逐帧场（构建时已去跨帧公共模，每个格点 `Σ_k w_k F_k ≡ 0`），由块坐标联合解给出（`p2_sky_plane_build_joint`）。产物加权平均电平不变，真实信号按构造保留。`seam.additive_mode` 三档（c/delta/both）退役，出现即判错 |
| `sky_plane.enabled` | 开（`true`） | —— | 是否构建 / 落盘信号面产品（公共面 + 联合解逐帧场）。面是生产必需产物，缺省必建；显式 `false` 时 apply 侧缺面即 fail-closed（实现 = module_adapters.cpp 的 `sp_cfg.value("enabled", true)`） |
| `sky_plane.own_surface_step_deg` | 0.02 | deg | 逐帧场 F 的格点间距（表示基频）。0.02–0.04° 为实测甜点；过粗中段变差、过细吃噪声。实现 = `spc.own_surface_step_deg` |
| `sky_plane.joint_outer_iterations` | 1 | 层 | 块坐标联合解的迭代层数（0 = 退回"公共面 + 低阶项"）。实现 = `spc.joint_outer_iterations` |

**施加口径的文字正本**：生产施加取 `docs/science/sky/UPM.md`「公式与推导」一节的
`calibrated = raw − δ_k`（δ_k = δ_poly,k + F_k）。F 由拟合残差构建并去跨帧公共模，
故 `Σ_k w_k F_k ≡ 0`；实现锚 = 阶段二 apply 节点对 `p2_sky_plane_eval_delta_block`
的单次求值与扣除（原 `seam.additive_mode` 读取、
逐帧分块校正场扣除与加回分支均已删除）。逐帧分块校正场（另一条曾被接入的路）
因跨瓦片不连续、无观测格子取 0 而退出生产，见「已知限制」。

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

- **公共信号面缺席即 fail-closed**：生产扣除需要 `p2_sky_plane.bin`。面缺失或
  打不开时本节点直接失败（不静默回退、不改扣别的场）。正常路径下
  `degraded_reason` 恒 `null`、`warning_codes` 恒空数组，
  `sky_plane_applied` / `sky_plane_loaded` 恒 `true`。fit 侧显式
  `sky_plane.enabled=false`、无采样点或构建/落盘失败时记
  `sky_plane_status`（`disabled_by_config` / `skipped_no_samples` /
  `build_failed` / `save_failed`）并置 `sky_plane_degraded = true`。
- 口径字段：`additive_mode_requested` / `additive_mode_effective` 恒 `single`，
  `additive_combination` 恒 `raw_minus_delta_to_shared_surface`，
  `c_subtracted = false`、`delta_subtracted = true`、
  `sky_plane_mode = delta_to_shared_surface`。

取消：会话消费面整模型不写半成品（p2_session.cpp 两处）；内核无取消检查点；无段内
checkpoint（dense 物化整缓存一次写）。
- **负例与归零分支 N34（T05–T07 负向轮，本卡死值与静默 scale）**：负例输入构造甲 =
  未知 `frame_id` 的求值请求；负例输入构造乙 = dense 缓存 stale（source hash
  不匹配）。预期行为甲 = 返回 NaN，不以 frame 0 伪装，不抛异常。落盘标记甲 =
  NaN + 未知帧登记。预期行为乙 = `rc = 2` 拒绝。落盘标记乙 = stale 拒绝码；
  伪装或静默重算 ⇒ 判红。

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
- **施加语义判别 oracle**：在控制点重叠面上复算死约束
  `D_i − D_j = raw_i − raw_j`，逐臂比较扣除量。生产臂 = 逐帧低阶偏差 δ_k
  （落回公共面）；负例 = 用逐帧分块校正场（跨瓦片不连续、无观测格子取 0），
  成品上必现瓦片菱形格与帧足迹矩形台阶，实测瓦片边界跳变 5e-06 ~ 1.5e-04、
  转 0 处可达 7e-04（复算脚本 `run/SEAM-C-ONLY/upm_eval.py`）；
- **缺面必红**：删/损坏 `p2_sky_plane.bin` 后 apply 必 fail-closed；
  正常路径 `degraded_reason = null`、`warning_codes = []`；
- sparse / dense 逐点等价（1e-12 基线）；`calibrate_block` 不破坏星 flux；
- 未知 frame_id 返回 NaN 而非异常、不以 frame 0 伪装；dense stale 拒绝必红；
- 1 worker vs N worker 输出一致。

## 已知限制

- 现行语义与判据正本 = `docs/science/algorithms/mosaic/PHASE2_UPM_IMPL.md`［A-1］；
- ALG 边界 = dense cache 为**空间求值缓存**、非科学重算（stale 拒绝语义）；未知
  frame_id → NaN 而非异常；
- 缺陷登记（不改码，正本 = ALG-P2-UPM-IMPL-001 缺陷清单）：upm.h
  materialize_dense 重复声明；upm.h 注释漂移（OpenMP vs std::thread 实现）；
  p2_session.cpp 覆盖键缺口；descriptor 端口静态声明的 persist→reload 语义；
- 施加口径唯一（`corrected = raw − δ_k`，δ_k 为逐帧低阶偏差，落回公共连续信号面）：
  `seam.additive_mode` 退役；**逐帧分块校正场不得再接入生产扣除**——它跨瓦片
  不连续（瓦片各自外推）且无观测格子取 0，修复前会引入瓦片菱形格与帧足迹台阶
  （实测跳变 5e-06 ~ 1.5e-04、转 0 处 7e-04，复算 `run/SEAM-C-ONLY/upm_eval.py`）；
  该项为待修缺陷，修复内容 = 跨瓦片连续求值 + 无观测格子平滑补值；
- **信号面的多尺度低频 overlay 必须关闭**：该 overlay 逐帧幅值实测达 2.8e-02
  （单侧形状：min −2.8e-02 / max +3.2e-05），随 δ_k 扣进成品会在星云核心形成
  撕裂台阶（把真实信号的低频结构当帧间差吃掉）。生产要求模型里
  `delta_ms_enabled = 0`；复现/审计时才用 `ACSD_DELTA_MS_ENABLE=1` 打开。
  判据与实测见采样算法分册的公共信号面施加路径一节；
- **`sky_plane` 段不在 CLI 配置白名单**：三命令的未知键门会拒绝该顶层键，故
  `enabled` / `joint_outer_iterations` / `own_surface_step_deg` /
  `own_surface_enabled` 目前**只能由编译缺省决定**（缺省即生产口径：面必建、
  联合解 1 层、格点 0.02°）。要把它们做成可调科学参数，须先补配置合同
  （白名单 + 文法 + 登记），在此之前不得按"可配"引用；
- 目标交付形态 acsd_p2_upm.dll 未落地；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
