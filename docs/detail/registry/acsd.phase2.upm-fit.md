# 模块 acsd.phase2.upm-fit

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节与「固定科学流程」一节、
> 「天光平面」一节
> 科学正本：docs/science/sky/UPM.md（SCI-UPM-001，FROZEN，相关章节）、
> docs/science/algorithms/UPM_SOLVER.md（ALG-UPM-001，权威推导；F3/F5/F6）、
> `docs/science/noise_snr/NOISE_SNR.md`「协方差传播」一节
> 实现级合同：docs/science/algorithms/PHASE2_UPM_IMPL.md（ALG-P2-UPM-IMPL-001；
> 语义与缺陷清单、TEST-DESIGN冻结容差、字段名与生产取值登记三节）
> 数据正本：docs/detail/registry/acsd.phase2.upm-fit.md（DATA-P2-UPM / DATA-P2-COR 端口表，本页输入输出端口表）；
> DATA-P2-SMP 端口表见 docs/detail/registry/acsd.phase2.sample.md
> API 正本：docs/engineering/api/PUBLIC_API.md（API-P2-UPM-001）、
> docs/engineering/api/PUBLIC_API.md「分阶段 API 面」（API-P2-001，FROZEN）

MOD ID = `MOD-acsd-phase2-upm-fit`；module_id 合同值 = `acsd.p2.upm`
（descriptor 词汇 `acsd.phase2.upm-fit` 为编排层口径，其对齐属迁移目标、未
落地）；dll_target = `acsd_p2_upm.dll`（迁移目标，未落地）。合同三件套落位
`lib/algorithms/upm/`（README / module.yaml ，CONTRACT_READY）。
生产源 = lib/algorithms/coverage/src/upm.cpp + 权威签名头
lib/algorithms/coverage/include/astro/phase2/upm.h；构建 = 根 CMakeLists 的
`acsd_phase2` 静态库成员。owner = SA-P2-U21；depends_on_int = P2-SAMP / CPU-005；
legacy_paths = 「lib/algorithms/coverage upm sources」。

apply 职能见 registry/acsd.phase2.upm-apply.md（同一 module_id 的另一职能）。

## 职责与明确非职责

职责（fit）：消费 DATA-P2-SMP 控制观测（sampler 产物），**联合求解唯一 UPM**。
模型 = 全部帧联合构建的**公共天光面 `B_ref(x)`** + 每帧只拟合自己的**平缓梯度
`δ_k(x)`**；表示层全量 `C_k ≡ B_ref + δ_k`；**实际施加量为 `δ_k`**（多退少补到
公共面，`B_ref` 保留）。本期为**纯加性**模型，不引入乘性尺度。

落地链路：**production 控制点权重 = `quality_factor × control_ivar`**
（签名头 `lib/algorithms/coverage/include/astro/phase2/upm.h` 的
`p2_upm_raw_weight` 是**单一实现**：几何可靠性**不在分子乘 geom**，而是在
per-control 归一化里施加 —— `out_norm = raw / Σ_cell raw × control_reliability`，
单元总权恒为 `control_reliability`；`control_ivar ≤ 0` 或非有限 ⇒ rc = 2
显式 INVALID，禁静默回退 support / SNR 权重臂）→ Huber IRLS（δ = 1.345 无量纲）
+ 图平滑 + 弱零锚 + 连通分量逐分量 gauge（分量内最小 frame_id，ALG-UPM-001 F3/F5）
→ 模型 C[frame][control] 8×8 control cell 双线性场 + `frame_index` /
`frame_id_by_index` 稳定绑定（绑定仅由稳定 frame_id 决定；save 前校验行数一致，
**拒绝写绑定损坏的模型文件**）。`build_geo` 变体消费全几何 `P2ControlNode`
（含单帧区），单帧区经全局平滑 / Laplacian 延拓（harmonic continuation）。

**记法消歧（强制）**：`C_k ≡ B_ref + δ_k` 是**表示层全量**；`raw − C_k`（全减，
含 `B_ref`）**不是默认路径** —— 把整张背景减掉后各帧都趋零，「接缝小」是背景没了
而不是对齐做好，该判据**退化**，必须用**非退化判据**（在保留背景的前提下比较
帧间一致性）。

不做：控制点采样 / 几何（P2-SAMP 上游，DATA-P2-SMP 域）；coverage union
（P2-COV 域）；加权积分与排异（P2-INT / P2-REJ 下游）；马赛克写出（P2-HIPS）；
乘性尺度差（SCI-UPM-001 非目标）；跨滤镜统一（filter 分组由调用方保证）；
per-frame gradient 产品的对外暴露（upm.h 冻结）；session 依赖（模型数据面显式
传入，会话消费 = lib/phase2_session/p2_session.cpp）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（p2_upm_fit_descriptor，编排层口径）：

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `samples` | `DATA-P2-SMP` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `upm_model` | `DATA-P2-UPM` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

入口面：Phase1 产品组、coverage、星点掩膜、光度控制点、天光背景采样点（每点带
值 / variance / SNR 权重）。落盘标识 = `acsd-upm-v2`
（DATA-UPM-MODEL-001）。**畸形模型 = 硬科学错误 `ERR-P2-UPM-001`**。

输出面：`P2ModelInfo`（version / precision：0=fp32, 1=fp64 / target_order /
control_count / observation_count / component_count / `model_hash[65]`）+
`C[frame][control]` FP64 + 参数协方差 + gauge 约束 + 连通性 + 加权残差 RMS +
**可辨识性判决与读数**（见下「可辨识性」）+ 收敛诊断量。**不输出** `g_k`（本期
恒等）。

invalid：null 参数 / n_obs = 0 / frame 绑定不一致 / open / parse 失败 ⇒ rc = 1；
production 模式 control_ivar ≤ 0 或非有限 ⇒ `p2_upm_raw_weight` rc = 2 →
build rc = 2（**显式 INVALID，禁静默回退 support / SNR**，upm.h）；未知 frame_id
⇒ `p2_upm_evaluate_c` 返回 NaN（显式不可用，**禁 frame 0 参数伪装**）。

### 数值落地口径

模型的冻结形式、判据式与字段名的**唯一正本 = docs/science/sky/UPM.md**
（SCI-UPM-CONV-001收敛与容差、权重禁令、依据与接缝门槛推导四节）
与 ALG-UPM-001 / PHASE2_UPM_IMPL.md；本页只记落地方式与可读数。

**权重口径 = 逆方差，禁止读作裸 SNR²**：拟合目标是采样点上的逆方差加权最小二乘（GLS 最优权重；文献与出版年双源登记见 docs/science/sky/UPM.md），与 P2 定权
式「权重 ∝ 平方信噪比 / 参考通量平方 = 逆方差」同源（SNR 以配置缺省的参考星等档
`m_ref` 归一，缺省 6.0、可被输入 JSON 覆盖；口径正本 = `eng/contracts/schemas/unified/frame_snr.schema.json` 的
`reference_baseline`）。
无参考通量归一的「权重 ∝ 裸 SNR²」与该逆方差口径**互斥**；本几何下 SNR² 权重的
伪影泄漏仅比 ivar 高约 18%，幅度**不可迁移**到其他几何。低 SNR 帧、光污染帧的
采样点权重自然变小，其异常背景无法把参考面与正常帧拉高；污染点由稳健迭代进一步
降权，硬污染交 rejection 判定。校准参数（样条系数）的不确定度经设计矩阵与权重
传播到最终 covariance（重采样为线性算子：输出协方差由输入协方差经该算子双向
作用得到）。

**稀疏天光面表示**：天光面用稀疏二维样条 / 插值基表示，节点间距与
`sky_sample_spacing` 匹配，存储的只有少量样条系数与采样点表，**内存随节点数而非
像素数增长**；**不构建稠密背景栅格** —— 需要某像素的背景值时由样条系数现场求值
（分块进行，最小单元可到一个像素）。节点间距由**输入几何**导出（上界 = 重叠带
宽度与指向间距的一半取小，下界 = 数据自身分辨率极限），保证面只表达天光与平缓
梯度、不跟踪星点与星云结构（结构区已被星点掩膜排除）；**标定常数属另一形态**，
几何量缺失 ⇒ fail-closed。

**联合参考面与逐帧梯度**：全部帧天光采样点经 WCS 投到同一球面坐标联合拟合
`B_ref`；每帧在参考面之上只拟合自己的平缓梯度（粗节点 / 低阶），把该帧背景梯度
校准到统一大平面。gauge 约束（固定参考帧或和约束）固定**加性**自由度。

**接缝判据的唯一口径 = 有符号电平台阶 + 适用域**：沿真实帧足迹边界取法向差分，
判据量为相对台阶（`bg` = 边界处局部背景电平），门 = 台阶绝对值的逐边最大值不超
门限；**门槛的推导与实测标定正本 = `docs/science/sky/UPM.md`「判据与误差」一节**（观测量与
零假设分布、虚警率、可检出下限与漏检面）。
只对两侧都在数据内部的边界计入（法向两侧都能放对照线且各 ≥ 最小样本数），被排除
的边界仍逐条落盘 `exclude` / `margin_px`。噪声比、扣对照线的净台阶、`d` 扫描与
`excess` 口径**全部只作诊断量、不判红**；**方差比对电平阶跃原理性失明**（阶跃
不改变方差），**方差比的引用面 = 诊断量本身**。该门隐含前提 = 保留背景
（SCI-UPM-001）。

**失败条件**：欠定（点数不足）、断图（天区不连通）、不可辨识或显著模型失配 ⇒
显式失败或分组件，**不假装同基准**；欠定与病态**统一走同一个返回码** rc = 3。

**收敛判据（无量纲）**：**停止判据必须无量纲**，分母用观测量的尺度（用模型矩阵
最大元作分母属另一口径）。步长判据是 `converged=1` 的唯一判据：相对容差关闭时
按绝对容差比较参数与系数步长，相对容差打开时右端改为容差乘观测量的尺度与 1 的
较大者（**生产必须打开**）；目标判据是 `converged=2` 的唯一判据：目标函数相对
改善连续 5 次低于阈值 ⇒ `stalled`。配置面 = `P2UpmBuildConfig` 的 `tolerance`
（默认 1e-6，同时作步长判据的容差）与 `tolerance_relative`（默认 0）。
**不存在 `tol_step` / `tol_obj` 两个字段。** `converged` 为状态枚举
`0 = max_iter` / `1 = converged` / `2 = stalled` / `3 = invalid`。

**拟合质量三元组独立落盘**（写进 `p2_upm_model.json`）：加权残差 RMS、权重和与
原始权重和之比、残差散度（dex）。**门 = 拟合质量三元组本身**；「帧间残差平均
绝对差」只作诊断读数。**实现现状（零承载，登记为缺口）**：该三元组在当前实现中
不产出 —— 全仓无 `rms_z` 命中，`p2_upm_model.json` 的现行键表（module_adapters.cpp
的 upm 落盘段）不含这三个键；当前数据面只落 `iterations` / `objective` /
`converged` 与可辨识性读数。判据口径为「门 = 三元组」，改动属科学口径变更。

**不收敛 / 判红不阻塞**：产品照出、构建 rc 不变，但必须在产品里落**机器可检**的
警告：顶层 `warnings[]` / `warning_codes[]` / `upm_converged_warning`（警告码
`P2-UPM-NOT-CONVERGED` / `P2-UPM-NOT-IDENTIFIABLE`）；**无警告时
`warning_codes` 为空数组**（可断言的成功态，不存在「没写就是没问题」的歧义）。
**诊断量必须一并落盘**（不靠日志）：`iterations` / `objective` / `rel_improve` /
`stall_count`，以及全部可辨识性读数。

**可辨识性判决（唯一判据）**：判在**未正则化**的列均衡数据信息矩阵上
（`H_eq = D⁻¹ H_red D⁻¹`，`D = diag(√H_ii)`），唯一相对阈值 τ = `rank_rtol`
（地板 `max(m,n)·eps`）：`identifiable ⟺ r_eff == n_free ⟺ κ(H_red) < 1/τ`。
欠定与病态是同一条不等式的两种读法，判决只有**一个位**，`r_eff` 与 κ 只是同一把
尺的两种读数。**设门位置 = 未正则化矩阵上的相对阈值 τ**（绝对条件数上限类常数属
另一口径）；正则化后矩阵上的门属另一形态 —— 正则化后条件数有上界，该门对「原问题
是否可辨识」零信息，κ 只作诊断（`kappa_solve`）。岭参数不是自由参数：它是判据
阈值派生的数值岭（τ 乘未正则化矩阵对角均值），没有配置面，也不能当自适应分支用
（唯一自适应旋钮 = 节点间距）。**自由度** = 观测数 − 有效秩（Andrae et al. 2010
式 (9)），`χ²_red` 的分母**必须**用它（用参数数作分母在秩亏时系统性高估并掩盖
未被约束的方向数）；`χ²` 本身**不参与**判决。

**产品键**（`p2_upm_model.json#identifiability`）：`rank_eff`（有效秩**计数**）、
`n_params`、`n_unidentified`、`rank_rtol` / `rank_rtol_effective`、`kappa`、
`chi2`、`dof_eff`、`chi2_red`、`chi2_red_defined`、`identifiable`、`n_blocks`、
`n_blocks_rank_deficient`、`n_blocks_single_frame`、
`n_unobserved_geometry_nodes`、`coupling_assembled`。

**发散 ≠ 缺失**：κ 在秩亏块上发散 ⇒ 产品写 `null`（JSON 不能表示非有限值），
消费方必须按「量存在但发散」读；「字段缺失」是另一个状态；自由度 ≤ 0 ⇒
`chi2_red` 写 `null` 且 `chi2_red_defined = false`（无定义，取值 = `null`；0 会
冒充完美拟合）；产品无该段 ⇒ 具名不可得（`rank_unavailable_reason`），缺键一律
具名登记。**边界如实登记**：无观测几何节点不参与判据、**不混进**
`n_unidentified`；图平滑权重 > 0 时块间耦合未装配 ⇒ `coupling_assembled = 0`；
只被单帧观测的 control 在参数意义上不可分 ⇒ 判红并单列
`n_blocks_single_frame`。判据实现与全部边角语义（零对角、负对角、非有限、尺度
不变性）见 `lib/algorithms/coverage/include/astro/phase2/identifiability.h` 的
`p2_identifiability_assess`。

**适用域**：无接缝 ⟺ 公共面可表示 —— 帧间天光差含「`B_ref` 不可表示且沿单轴
相干」的分量时残余接缝与该分量 RMS 线性相关；接缝随尺度的放大按跨实现稳健的
「峰值/长尺度比 ≈ 8」（肘点 ≈ 2h、非单调形状）刻画，端点比属实现条件依赖读数，
**不复现为固定倍数**（正本 = `docs/science/sky/UPM.md`「判据与误差」一节与
`实验/additive-sky-seamless/`）。该比值只具实验单元内相对比较意义，不得作跨几何绝对断言。**纯加性前提**：帧间乘性差必须先在 Phase1 吸收；
不可吸收的基外高频分量对**电平**接缝贡献有界，但可被分块 PSD 定位。
**不可检验域**：图平滑权重为 0 时 per-(frame,cell) 自由加性场恰好定解，
「拟合/堆叠权重同源」与「末端残差场扣除」在该域内不可检验；`final_gauge` 在
单帧全图时近似无操作且不能修复子集依赖 —— 该域内的恒真结果只作域内观察。

## 公共 header、核心 symbol 与生命周期

签名头正本 = lib/algorithms/coverage/include/astro/phase2/upm.h
（`P2ControlObservation`、`P2ModelInfo`、`P2UpmBuildConfig` 三处结构体定义，
含 build / build_geo / save / open / info / calibrate_block / evaluate_c /
raw_weight（含冻结注）/ normalized_weights / geometry_hash / component_gauges /
materialize_dense（重复声明，登记见 ALG-P2-UPM-IMPL-001）/ dense_info /
dense_read_block / close 声明）。

核心 symbol（upm.cpp 导出，extern "C"）：`p2_upm_build`、`p2_upm_build_geo`、
`p2_upm_save`、`p2_upm_open`、`p2_upm_info`、`p2_upm_calibrate_block`、
`p2_upm_evaluate_c`、`p2_upm_raw_weight`、`p2_upm_geometry_hash`、
`p2_upm_component_gauges`、`p2_upm_materialize_dense_n`、
`p2_upm_materialize_dense`、`p2_upm_dense_info`、`p2_upm_dense_read_block`、
`p2_upm_close`。`p2_upm_normalized_weights` 在 upm.cpp / upm.h 带 RETIRED 注记，
**全仓零消费者**，不列入在役导出。

`p2_upm_materialize_dense` 的 worker 语义：该 wrap 恒传 `workers = 0`，而实现取
`nw = (workers > 0) ? workers : 1` ⇒ **`workers = 0` = 单线程串行，不是 auto**；
同一约定见 `cfg.cpu_workers > 0 ? cfg.cpu_workers : 1`。

生命周期 = build / build_geo（调用方持有 `void* model`）→ info / evaluate_c /
calibrate_block / dense 物化与读取 → save / open 往返 → `p2_upm_close`（delete
Model；nullptr = rc 0 幂等）。模型 / 缓冲所有权 = 调用方。ABI = c++17；
api_id = API-P2-001。

entrypoint = 产品组 + 掩膜 + 控制点 + 天光采样点 → 加性校正场参数（公共面样条
系数 + 逐帧梯度）、协方差、归一化产品；天光面以稀疏系数对象传递与落盘；下游按块
取背景值时调用样条求值接口，**不取稠密栅格**。

## Registry descriptor 与配置 schema

module_id=`acsd.phase2.upm-fit`（编排层口径）；execution_class=`cpu_heavy`;
parallel_ok=True; abi=c++17; api_id=API-P2-001。descriptor 派生的占位 ID
（SCI-P2-UPM-001 / ALG-P2-UPM-001 / TEST-P2-UPM-001）与端口占位语义
（persist→reload）的对齐属迁移目标（未落地）；descriptor 端口为静态声明的
persist→reload 语义，与内核 probe/fill 语义的桥接未验证。

配置面分三处结构体，字段名一律以签名头为准：`P2UpmBuildConfig`
（统一头文件，字段数以签名头为准；本页登记装配面字段见下表 A）、
`P2UpmMaBuildConfig`（upm.h，乘法/加性观测求解器；判据与 gauge 字段见表 B）、
`P2SkyPlaneConfig`（sky_plane.h，天光面表示与逐帧梯度；字段见表 B）。production
默认取值单一来源 = lib/phase2_session/p2_session.cpp。

表 A —— `P2UpmBuildConfig` 的装配面字段：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `robust_loss` | 0 | —— | 0 = Huber |
| `snr_weight_mode` | 0 | —— | 0 = snr2_normalized（头字段名 `snr_weight_mode`，生产装配显式置 0）；生产权重口径由 `use_ivar_weight` 决定，本字段不选择科学权重 |
| `huber_delta` | 1.345 | —— | Huber IRLS 调谐常数（无量纲） |
| `max_iterations` | 100 | 次 | 稳健拟合迭代上限 |
| `tolerance` | 1e-6 | —— | 绝对容差（同时作步长判据的容差） |
| `tolerance_relative` | 0 | —— | 相对容差开关（1 = 右端改为容差 × max(观测尺度, 1)）；**生产必须打开**（字段名与生产取值登记于 docs/science/algorithms/PHASE2_UPM_IMPL.md + 本页输入输出端口表） |
| `target_order` | 覆盖图 order | —— | 取自 coverage（−1 = auto） |
| `sigma_floor` | 1e-3 | —— | 权重分母下限（**仅 legacy 权重臂**消费） |
| `support_power` | 1.0 | —— | legacy 权重臂的 support 幂 |
| `quality_mode` | 0 | —— | quality 因子的分支选择（0 = flags 映射）；实现在 `quality_factor(flags, mode)` 内忽略该参数 |
| `use_ivar_weight` | 1 | —— | **真开关**：非 0 时用 `control_ivar` 权重；0 时走 legacy 权重臂（仅 ablation / 诊断） |
| `control_reliability` | 1.0 | —— | per-control 相对可靠度，**在 per-control 归一化中施加**（不在 `raw_w` 分子）；实现上是配置常量，默认 1.0 且越域回退 1.0 |
| `zero_anchor_weight` | 键缺省时编译期默认 0.0 | —— | 弱零校正锚权重（头注记：生产装配显式 1e-3）；不属 `raw_w`，在 IRLS 目标函数中单独施加 |
| `smoothing_lambda` | 键缺省时编译期默认 0.0 | —— | **UPM 图平滑权重**（对天光 / δ 面的拟合正则项，默认 0 = 关闭）。`P2_SMOOTHING_LAMBDA_AUTO = 0.1` 仅在 auto 路径生效 |
| `cpu_workers` | 调用方给 lease | —— | 并行 worker 数；0 = 单线程串行（不是 auto） |
| `input_manifest_hash` | 可空 | —— | 输入稳定 manifest 哈希；非空时参与模型 hash |

表 A 未列但同属 `P2UpmBuildConfig` 的字段（阻尼 / 参考场装配 / 控制网格边长）已在
upm.h 带冻结注记登记，本页不复制其语义。

表 B —— 判据与天光面表示字段（分属另两个结构体）：

| 字段 | 所在结构体 | 默认 | 说明 |
|---|---|---|---|
| `rank_rtol` | `P2UpmMaBuildConfig` / `P2SkyPlaneConfig` | 1e-10 | **唯一**判据阈值 τ（相对量，冻结值 `FZ-AP2S-RANK-RTOL`；与天光面侧同符号、同值、同一实现） |
| `gauge_mode` | `P2UpmMaBuildConfig` / `P2SkyPlaneConfig` | 0 | UPM 侧 0 = `min_frame_id` gauge（其他值 → 参数错误）；天光面侧 0 = `reference_frame`、1 = `sum_zero` |
| `frame_gradient_order` | `P2SkyPlaneConfig` | 1 | 逐帧梯度修正的阶数（0 = 仅偏移、1 = 平面、2 = 二次；越界夹到 0..2） |
| `spline_degree` | `P2SkyPlaneConfig` | 1 | 公共天光面 `B_ref` 的样条阶数（1 = 双线性，3 = 双三次） |
| `node_spacing_deg` | `P2SkyPlaneConfig` | 0 | `B_ref` 节点间距（切平面角度，度）：>0 = 调用方显式给定；≤0 = 由输入几何经 `p2_sky_plane_derive_node_spacing` 导出；几何量缺失 ⇒ `P2_SKY_PLANE_GEOMETRY_REQUIRED` 显式失败（禁回退标定常数） |

`upm` / `smoothing_lambda` / `huber_delta` / `max_iterations` 可被 phase config
JSON 覆盖（lib/phase2_session/p2_session.cpp）。

**三个互不相同、并存不冲突的量**（勿混）：① 迭代阻尼（naive Gauss-Seidel 在
链式 / 二部覆盖图上会周期 2 振荡，故取阻尼 ≈ 0.5）；② 上表的
`smoothing_lambda`（对天光 / δ 面的拟合正则项，现行机制）；③ 堆叠平滑项
（拟合目标里的新项，本期不加，做实验验证）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`。并行轴 = 观测间 compute_raw / 聚合（worker-local 局部和、逐观测
独立权重）与 dense tile 求值（同文件 `std::thread` 池，workers 由调用方传 lease）。
模块内用 `std::thread` 实现，**无 OpenMP**（upm.h 另有一处注释仍写 OpenMP，属
注释漂移，登记见 ALG-P2-UPM-IMPL-001）。

worker 数 = **Runtime lease 唯一来源**：`cfg.cpu_workers` = ThreadBudget.max_workers
经 lib/phase2_session/p2_session.cpp 与 stage2.cpp 传入；模块无
hardware_concurrency 自行开线程。**0 = 单线程串行**、1 = 单线程 reference、
> 1 = 调用方显式给定的线程数（「0 = auto」的旧述已按实测订正）。

确定性 = 聚合用 worker-local 局部和 + **线程号升序归并**（determinism class D1 =
worker 数无关、同 worker 数位精确）；gauge / 连通分量 / 收敛 / 归并固定顺序；稠密
缓存 bit-identical。

## 内存/cache/I-O/所有权

内存：构建 O(n_ctrl + n_frame·n_ctrl)（`Model.C[frame][control]` + 权重缓存）；
dense 物化上界 = kChunk·kLeafPx·8 字节；无无界缓存。

I/O：唯一 AIO 通道 = `aio_upm_write_sparse`（模型稀疏持久化，ENG-IO-001 原子写）
+ `aio_upm_open` / `aio_upm_dense_*` 读面；dense cache = 空间求值缓存（frame ×
tile 的 C_i(p) 值，同模型 hash / 目标 order / frame hash 校验，stale 拒绝）。无
其他文件 I/O。

所有权：model（`void*`）与全部输入 / 输出缓冲由调用方分配与释放
（`p2_upm_close` 是唯一释放口）；模块无全局可变状态（reentrant = yes；并行面经
显式 worker 数受控）。

## 错误、日志、指标、取消和 checkpoint

错误面 = rc 三态 + 调用方语义承载：0 = ok；1 = 参数 / 绑定 / open / parse / IO；
2 = production 缺 control ivar 与 dense stale cache。模块面错误码词汇 = P2UPM 内核
rc，**不使用**编排层 `ACS_ERR_*` 词汇。

- 天光面几何量缺失 ⇒ fail-closed（`P2_SKY_PLANE_GEOMETRY_REQUIRED`）；
- 欠定 / 病态 / 断图 / 不可辨识 ⇒ rc = 3（两条共用这一条路径）；
- 显著模型失配（如样条面无法表达的强局部背景）⇒ 失败并报告残差结构；
- 参数协方差不输出 ⇒ 下游 covariance 不可信，标记；
- 参考面受单帧主导（权重失衡）⇒ 权重分布审计并标记；
- 错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。

取消：会话消费面整模型不写半成品（p2_session.cpp 的 upm_build 入口检查、persist
段取消点；取消即 close 模型返回 `ACS_ERR_CANCELLED`，无半成品文件）；内核无取消
检查点。无段内 checkpoint（dense 物化整缓存一次写）；stage2 消费链的逐阶段 stage
日志由会话 / 编排层承载，非模块内输出。
- **负例与归零分支 N32/N33（T05–T07 负向轮，本卡死值与静默 scale）**：N32 未收敛
  输入构造 = IRLS 达上限仍未收敛。预期行为 = `warning_codes` 非空且 rc 不变，
  无警告时为空数组（可断言），不发布伪值。落盘标记 = `warning_codes`。
  N33 零对角 / 秩亏输入构造 = 精确秩亏 / 零对角 / 不定矩阵。预期行为 = 判红，
  κ 发散时产品写 `null` 不发布伪值；加大岭参数把红买成绿 ⇒ 判红。落盘标记 =
  判红位 + `null` 产品；

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-UPM-001` 待建（不冒认）；设计冻结 = ALG-P2-UPM-IMPL-001 的
「TEST-DESIGN（TEST-P2-UPM-DESIGN 冻结）」章，容差权威同该章
（ALG-UPM-001 F6 的 dense/sparse 1e-12 等价基线）。

已取证但载体不在仓内的相邻结论（不冒认）：参数恢复 oracle 覆盖常数面恢复、逐帧偏移、
收敛确定性 model_hash 逐位与星 flux 不破坏；并行面每 worker 重复确定、1/N 科学等价、内存有界；
另有合成单元面与生产并行面的实测读数。这些读数不在本仓可复算路径上，引用时只作背景。

Oracle 面：

- 构造已知加性校正场（常数、平面梯度、平滑曲面）→ 参数恢复满足精度，加权残差
  RMS 达理论水平；乘性恒等路径须与显式乘性 = 1 的数值逐位一致；
- **逆方差加权抗污染**：注入低 SNR / 光污染帧，参考面与正常帧校准结果不被拉高；
  与等权拟合对照有显著改善；
- 稀疏性 / 内存：内存占用随样条节点数与采样点数增长，现场求值与稠密参考实现
  数值一致（容差内）；
- 平滑性：注入星点 / 星云残差不被天光面拟合（掩膜 + 节点间距联合验证）；
- 断图 / 欠定 / 不可辨识能红：判据在**未正则化**矩阵上、阈值只有 `rank_rtol`
  一个；「加大岭参数把红买成绿」必须判红（正则化后矩阵上的门是恒真门）；
- **可辨识性判据正/负例**：良性强相关系统判绿；精确秩亏、零对角、不定矩阵必须
  判红；κ 发散时产品写 `null` 而**不**发布伪值；同一矩阵在参数列缩放与全局权重
  缩放下的判决、`r_eff`、κ 逐位不变；
- **不收敛 / 判红出产品**：`warning_codes` 非空且 rc 不变；无警告时
  `warning_codes` 为空数组（可断言）；
- **非退化接缝判据**：在**保留 `B_ref`** 的前提下比较帧间一致性（全减会因背景
  归零而假通过）；判据量取**有符号**台阶并套适用域；负例 = 注入已知台阶必判红、
  两侧噪声差大但无台阶必判绿、旧方差比口径在同一输入上判绿（盲区复现）；
- 无量纲收敛判据与状态枚举（`stalled` / `invalid` 能红）；拟合质量三元组独立落盘；
- 参数不确定度传播到最终 covariance 验证；
- 与独立高精度矩阵 Oracle 对比；1 worker vs N worker 一致。

## 已知限制

- 缺陷登记（不改码，正本 = ALG-P2-UPM-IMPL-001 缺陷清单）：upm.h
  materialize_dense 重复声明；upm.h 注释漂移（OpenMP vs std::thread 实现）；
  p2_session.cpp 覆盖键缺口；descriptor 端口静态声明的 persist→reload 语义；
- ALG 边界：覆盖并集非凸区外推仅经 tile 内 cell 界锚点（外推锚只引用真实存在
  cell）；单帧区 = harmonic continuation，非数据约束解（SCI-UPM-001）；
  legacy `snr²` / `snr²/(1+snr²)` / `uncertainty²` 权重臂（含 support 幂与 σ_floor
  分母）由 `use_ivar_weight = 0` 选择，**仅 ablation / 诊断**（SNR-015）；它**不是**
  `quality_mode` —— `quality_mode` 只决定 quality 因子的分支，实现在
  `quality_factor(flags, mode)` 内恒忽略该参数，生产默认 `quality_mode = 0` +
  `use_ivar_weight = 1`；
- 拟合质量三元组当前零承载（门口径已冻结、实现三键未落）；
- 目标交付形态 acsd_p2_upm.dll 未落地；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
