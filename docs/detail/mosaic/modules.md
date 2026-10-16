# Mosaic 模块与接口

本页按生产依赖组织节点接口，依据[阶段管线](pipeline.md)和所引科学、工程合同。每节说明输入输出、公开头、所有权、错误及实现边界；库能力、局部会话与 CLI 生产节点分别标识。

| 模块 | 调用职责 |
|---|---|
| [coverage](#coverage) | Mosaic 输入发现与覆盖并集 |
| [sample](#sample) | Mosaic 背景控制采样 |
| [upm-fit](#upm-fit) | Mosaic 公共面与加性差模拟合 |
| [upm-apply](#upm-apply) | Mosaic 加性差模应用 |
| [reject](#reject) | Mosaic 候选栈排异 |
| [integrate](#integrate) | Mosaic 数值集成与精度消费 |
| [write](#write) | Mosaic 单信号 HiPS 写出 |
| [session](#session) | Mosaic 局部模型会话 |
| [resample](#resample) | Mosaic 聚合会话适配描述符 |

## coverage

**Mosaic 输入发现与覆盖并集。**

本节点枚举逐帧 HiPS 的产品几何，计算目标阶和覆盖并集，给后续采样与空间调度提供帧/区域映射。用户可自由组合设备、滤镜和带宽；单位/几何损坏仍需报具体错误。科学边界见[最高设计](../../ACSD_DESIGN.md)和 [Mosaic 管线](pipeline.md)。

### 入口与生产者、消费者

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_coverage` 对应 `acsd.phase2.coverage`，调用 [coverage.h](../../../lib/algorithms/coverage/include/astro/phase2/coverage.h) 的
`p2_coverage_build(hips_paths,n_inputs,out)`。当前分两次查询容量/回填，并写 `p2_coverage.json`，供 sample、apply、write 等消费。局部 `p2_session` 也可调用该库。

[coverage.cpp](../../../lib/algorithms/coverage/src/coverage.cpp)检查实际产品，保留各帧 filter 字符串而不作跨帧全等拒绝。坐标/ordering、tile_width、signal/support 语义及属性可解析性是数据检查，不能写成统一物理通带的证明。

### 字段、坐标与内存

| 类型/字段 | 合同 |
|---|---|
| `P2HipsInputInfo` | hips_path、frame_id、max_leaf_order、n_tiles、filter_passband、frame_type、hips_ordering |
| `P2MocCell` | order/ipix 整数 NESTED 位置；须随实际层级解释，tile 阶与叶阶相差 9 |
| `P2CoverageResult` | 调用方分配 inputs/union_cells，n_inputs/n_union_cells/target_order/status/error |
| target_order | 当前取各输入最大叶阶的最小值；不是独立配置的科学目标分辨率已实现 |
| union | 几何并集，允许断连；不提供点源信息量或面亮度统计权重 |

`p2_coverage_free` 清零 POD，不释放调用方数组；调用者拥有所有缓冲。整数集合处理无独立线程预算，文件读取经 AIO；Runtime 节点管理生命周期和取消。覆盖数/并集不是背景有效观测边，也不自动给出 UPM 连通分量的绝对零点。

### 公共辅助 API 与错误

同头的 `p2_coverage_support_classify` 区分 uncovered、covered_unsupported、supported、unavailable；coverage/support 的 known 位不能用零替代。另有 `p2_tile_boundary_owner`、`p2_deterministic_reduction_order` 及 `p2_weight_source_token_reject`。它们分别处理边界归属、整数定序及具名来源检查，不生成科学权重，也不能据头内辅助 API 推断 CLI 全部调用它们。

主 build 返回 0/非零并带 `error[512]`；probe 容量与普通失败需要结合需求量判断。缺属性、坏数据、非 NESTED 等按真实 reader 错误上行；空 filter 或不同 filter 字符串不自动构成物理坏帧。

### 验证入口与边界

从 `p2_coverage_build` probe/fill、[coverage 实现](../../../lib/algorithms/coverage/src/coverage.cpp)及 [Mosaic 管线](pipeline.md)的调用者进入核对：已知并集、断连、降阶父聚合、空集、缺产品、同天区几何与混合通带。target_order、帧身份及输出响应的当前限制集中见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)，本页未进行运行验证。

## sample

**Mosaic 背景控制采样。**

采样从已发现的逐帧 HiPS 读取局部背景观测，为公共面/逐帧差模拟合提供稀疏值及其自身方差。背景 control 精度、参考点源精度与最终像元精度是不同统计量；定义依 [UPM 正本](../../science/sky/UPM.md)和[噪声正本](../../science/noise_snr/NOISE_SNR.md)。

### 实际入口与交接

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_sample` 对应 `acsd.phase2.sample`：读取 `p2_coverage.json`，生成/复用 `p2_frame_id`，以 probe/fill 调用 `p2_sample_controls_cached_with_halos`，写 `p2_samples.json`。fit 消费 sky_samples、control 几何、观测/帧映射；apply 还使用控制统计量解释校正方差。

公共头为 [sampler.h](../../../lib/algorithms/coverage/include/astro/phase2/sampler.h)，实现为 [sampler.cpp](../../../lib/algorithms/coverage/src/sampler.cpp)。基础入口 `p2_sample_controls`、缓存入口 `p2_sample_controls_cached` 和 with_halos 共享采样；后者接可空 `P2GaiaHaloCaps`，签名存在不表示运行确实进行了 Gaia 查询。当前 adapter 的停用帽路径应按实际分支理解，不能将参数声明当已生效掩膜。

### 数据与采样语义

| 对象 | 字段/单位 |
|---|---|
| `P2ControlNode` | control_id、tile_ipix、gx/gy、ICRS ra_deg/dec_deg、leaf_ipix |
| `P2ControlObservation` | frame/control/leaf ID，value、uncertainty、control_variance、control_ivar、support、snr/available、quality_flags |
| value/uncertainty | 原信号面亮度单位及同单位标准误；不是未经球面化的 ADU 积分量 |
| control_variance/control_ivar | patch 背景估计量的方差及倒数；与单 leaf ivar 分开 |
| counts | geometry controls、accepted controls、overlap controls 和 accepted observations 分别登记 |

网格由 union 几何和配置决定，不由 SNR 定义。背景 patch 用有限/support 样本，经亮端裁剪、局部容差、污染/保留比例与 Catalogue veto。保留负值；Catalogue 查询不进行新的星点检测。sky_samples 可包含单帧 clean 区域，重叠约束所需观测集合与之分开。

当前 control 方差含 `control_k_corr` 与中位数近似；这只是特定估计器/几何的模型，不能把默认 1.4 称全域物理常数。空间相关性与有效样本数的适用域由噪声正本解释。

### API、配置与所有权

`p2_sampler_default_config` 给默认；`p2_frame_id` 返回内容派生 uint64 ID，0 为失败；`p2_stats_median/mad` 为共享统计入口。buffer 由调用者 probe 后分配，out_n 给实际需求；容量截断规则依头文件，不能把调用成功当已完整收齐。

`P2SamplerConfig` 承载网格、patch、clip、保留比例、Catalogue/星帽、k_corr 和 cpu_workers。CLI 的 `model` 实际消费由 `p2_sample_cfg_from_doc` 解析，和库默认/工具入口区分；不在本页另列无消费者键。halo/回退参数存在时也不代表相应分支可用。

按 cell/帧工作分片，worker 使用独立 AIO 读句柄，输出按固定索引组织；预算由 Runtime 传入，不自选硬件线程数。大中间 cells 随 coverage 增长，不能声称内存只随最终 accepted 点数增长。取消由上层协作处理。

### 错误、验证与能力边界

参数、帧身份、open/read、容量/规模和异常由返回码及 err 文本说明；单点丢弃计数不是整个节点执行失败。缺 Catalogue、缺 ivar 和零背景尺度分别记录，不能补出虚假的 reference precision。

验证从本头 probe/fill、`p2_sky_patch_estimate` 及 [Mosaic 管线](pipeline.md)进入：纯噪声/已知梯度、亮源污染、空洞、缺产品、计数守恒、控制方差 Monte Carlo、线程等价。现有规整 sparse SNR 或 Catalogue 的可读性不证明 canonical 控制已贯通；相邻差异只见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。本页未重跑验证。

## upm-fit

**Mosaic 公共面与加性差模拟合。**

生产 fit 联合拟合公共信号面与逐帧加性差，供 apply 仅扣差模、保留公共面。可辨识性、连通分量规范、通带/PSF/乘性差的边界由 [UPM 正本](../../science/sky/UPM.md)规定，不在登记卡另立科学模型。

### 生产入口与对象

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_upm_fit` 对应 `acsd.phase2.upm-fit`：读取 `p2_samples.json`，装配几何/配置，调用 `p2_sky_plane_build_joint`，保存 `p2_sky_plane.bin` 和拟合诊断。该 .bin 当前是模型序列化载体，不能按扩展名猜数组布局。apply 打开同一模型；writer 读其真实 hash/审计读数。

公共头为 [sky_plane.h](../../../lib/algorithms/coverage/include/astro/phase2/sky_plane.h)，实现为 [sky_plane.cpp](../../../lib/algorithms/coverage/src/sky_plane.cpp)。`p2_sky_plane_default_config`、`_default_adaptive_config`、`_geometry_from_controls`、`_derive_node_spacing`、`_build`、`_build_adaptive`、`_build_joint`、`_info`、`_save/open/close` 提供模型生命周期。

### 字段、单位和配置

| 对象 | 合同 |
|---|---|
| `P2SkySample` | 同帧身份、ICRS sky、背景值与其方差/权重、支持/质量 |
| `P2SkyPlaneGeometry` | control/footprint 派生的几何和可用范围；不能由目录名猜目标天区 |
| `P2SkyPlaneConfig` | 公共面节点、样条阶、逐帧梯度阶、规范模式、rank_rtol、稳健迭代、逐帧残差场及 worker |
| `P2SkyPlaneInfo` | 参数数/秩、条件数、自由度/残差、可辨识读数、model_hash 与模型配置 |
| 模型求值 | 背景值/差模沿实际信号面亮度单位；sky 为 degree，节点间距为 degree |

联合构建先求公共面和低阶项，再从残差建立逐帧场并去公共模，按配置重解。保留某个加权公共量是代数性质，不能据此认定真实低频天空已无偏恢复。单帧或断连区的延拓/规范不增加观测信息。

配置从 `p2_sky_plane_default_config` 和实际 adapter 解析得到；结构体字段与 CLI 白名单是两个消费面，不能只凭库字段宣称用户可配。乘性恒等、颜色差和 PSF 差也不能被加性拟合的成功状态吸收。

### 库 UPM 接口与生产边界

[upm.h](../../../lib/algorithms/coverage/include/astro/phase2/upm.h) / [upm.cpp](../../../lib/algorithms/coverage/src/upm.cpp)另有 `p2_upm_build/build_geo`、`_raw_weight`、`_save/open/info`、`_component_gauges`、`_convergence` 与 dense cache 接口，局部 session/工具可消费。它们不是当前 CLI fit→apply 数值链的模型载体。

库 `P2ControlObservation.control_ivar` 是背景估计方差的倒数；`p2_upm_raw_weight` 处理 quality/control_ivar。不能用点源 SNR²/F_ref²代替该统计量，也不能把几何可靠性或 support 命名为普适精度。

### 所有权、错误与验证入口

build 返回 opaque model，由调用方用对应 close 释放；输入样本借用至同步调用结束。模型参数/系数与必要缓冲驻留，求值按需执行；dense cache 是另外的计算缓存。worker 由宿主预算传入，固定块/帧归约仍需以实际容差验证，不凭线程局部和宣称跨 worker 逐位一致。

配置/几何缺失、文件损坏、不可计算与不可辨识诊断分开；模型返回码与 CLI 退出码不能直接等同。收敛或秩读数是解释模型的证据，不是新增研发审批关卡。

验证从本页 build_joint、info、save/open 和[拟合/应用链](pipeline.md)进入：已知加性场、每连通分量规范、弱重叠、断连、真低频星云、PSF/色差阴性世界、参数恢复与留出残差。尚无实测不能写已完成无接缝；生产差异与相邻接口限制集中见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## upm-apply

**Mosaic 加性差模应用。**

本节点将帧信号扣到所选公共面，消费 fit 的同一模型。施加语义与保真边界由 [UPM 正本](../../science/sky/UPM.md)规定：只扣逐帧偏差，公共天空保留；颜色、PSF 和未知乘性失配不自动成为可扣背景。

### 真实入口和数据交接

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_upm_apply` 对应 `acsd.phase2.upm-apply`。它读取实际 `p2_sky_plane.bin`、coverage、samples 及帧产品，调用 `p2_sky_plane_open` 与 `p2_sky_plane_eval_delta_block`，输出 `p2_corrected.json`、逐帧 corrected 数据及可用方差工作面。reject 和 integrate 消费这些对象。

descriptor 的 upm_model/calibrated_frames→corrected 是排序/载荷声明；真实模型是 sky-plane，不能要求不存在于生产链的 `p2_upm_model` 文件仅充依赖条件。

### 接口、数值与单位

[sky_plane.h](../../../lib/algorithms/coverage/include/astro/phase2/sky_plane.h)声明 `p2_sky_plane_eval_delta`、`_eval_delta_block`、`_eval/_eval_block`、`_info`、`_frame_delta` 及 `_open/close`；[实现](../../../lib/algorithms/coverage/src/sky_plane.cpp)由 fit 复用。

| 字段/载荷 | 语义 |
|---|---|
| frame_id | 与采样/模型同一身份，不能按 compact 或输入位置猜帧 |
| ra/dec | ICRS degree；每叶位置由真实目标网格生成 |
| eval_delta 输出 | 逐帧低阶差与残差场；不是包括公共面的全量背景 |
| corrected | raw−delta；继承真实球面面亮度单位，负值与零合法 |
| support/validity | 覆盖/资格，不能作为像元统计权重 |
| corrected variance | 可用时为内部统计对象，条件、拟合耦合及缺项随 provenance 表达 |

当前 apply 有基于控制观测的残差制造者方差计算路径；存在此路径不证明任意图像位置、相关性、模型失配与交叉项已完整描述。精度权重/实际条件由[噪声正本](../../science/noise_snr/NOISE_SNR.md)解释，具体消费见[集成卡](modules.md#integrate)。

### 所有权、并行和兼容 API

模型由 open 获取，作用域结束用 `p2_sky_plane_close`；逐帧独立读句柄、tile 缓冲和工作文件由 adapter/AIO 管理，不需复制全帧常驻。Runtime 提供并行预算，错误/取消沿节点接口传播；不能把无段内取消参数写成已具备任意时刻取消。

[upm.h](../../../lib/algorithms/coverage/include/astro/phase2/upm.h)另有 `p2_upm_calibrate_block`、`_evaluate_c`、`_materialize_dense_n`、`_dense_read_block`，用于库模型/缓存；生产 apply 不改走该场。dense stale=2 是该库缓存语义，不是所有节点统一错误码。`p2_upm_close` 与 sky-plane close 不能混用。

### 配置、错误与验证入口

apply 不独立拟合新参数，消费 fit 保存的配置/模型；不能重复施加 Normalize 光度 scale。缺实际模型、坏模型或错误帧绑定须报具体错误，不静默改扣其它场；无覆盖保持 NaN。合法数据组合不能因为仪器/滤镜字符串不同而被拒绝。

验证从 `p2_sky_plane_eval_delta_block` 与[Mosaic 管线](pipeline.md)进入：已知差模、公共面保留、跨 tile 连续、单帧/空洞支持、缺模型与未知 frame、方差 Monte Carlo、同输入不同块/worker。实测状态和相邻差异仅按[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)报告，本页不新增数值成功结论。

## reject

**Mosaic 候选栈排异。**

排异决定同一天球位置哪些帧样本进入集成，不合并数值、不生成权重。方法与小样本定义来自[排异正本](../../science/integration/REJECTION.md)；预测误差、实际保留集合与条件方差需按[噪声正本](../../science/noise_snr/NOISE_SNR.md)解释。

### 生产入口与步骤

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_reject` 对应 `acsd.phase2.reject`，读取 corrected 元数据和样本工作面，调用 planning、资格收集、显式 kernel、大尺度 mask 后处理。输出 `p2_rejection.json` 及 accepted、nrej、candidates、逐样本 mask 工作面，由 integrate 消费。

公开头 [rejection.h](../../../lib/algorithms/coverage/include/astro/phase2/rejection.h)，实现 [rejection.cpp](../../../lib/algorithms/coverage/src/rejection.cpp)。

| 接口 | 合同 |
|---|---|
| `p2_reject_plan_resolve` / `_resolve_n` | AUTO 在 planning 解析；输入 nominal 为几何可贡献帧数，不是资格过滤后 n_eff |
| `p2_collect_candidate_stack` | frame-major strided gather，产 compact 栈与 source_indices |
| `p2_reject_stack_ex` | 只接显式 method；decision reasons 和栈 status 分开 |
| `p2_large_scale_apply` | 低/高 mask 原地做连通结构生长，只增拒绝范围 |
| `p2_rejection_semantic_id`、`_applicability`、`_percentile_band_min_n` | 返回实际方法身份、适用提示和小 N 策略读数 |
| `p2_reject_stack` | 连续输入兼容 API，不作为 CLI 实际 gather 路线描述 |

### 数据、索引和方法

`P2CandidateStack` 包含 double values/weights、frame_ids、count 和可选 prior_sigma/prior_sky。values 沿 corrected 球面面亮度单位，weights 必须与所估量/单位配对。kernel 不重新解释 sky 或投影，compact 样本关联真实原帧必须经 source_indices。

逐样本 reason 为 ACCEPTED、REJECTED_LOW、REJECTED_HIGH、UNDERDETERMINED；低/高由相对阈值方向决定，与原值是否为负无关。栈级 status 另含 OK、MIN_SAMPLES、ALL_REJECTED、INVALID_INPUT、INVALID_CONFIGURATION、INVALID_METHOD、INTERNAL_ERROR。

生产 `acsd_adaptive_pixel` 当前小 N≤3 保守不排异，4–5 percentile，N≥6 winsorized；其它 profile 和显式方法有各自解析。方法枚举还包括 sigma、averaged、linear-fit、ESD、RCR、median-sigma、minmax 及显式 prior-sigma 入口，不能把枚举全集称 AUTO 的生产输出。percentile 的电平比例判据不等于预测噪声 sigma；其适用域见科学分册。

### 配置、所有权与边界

配置来自 `reject` 的方法/profile/normalization 和 large_scale；参数按方法分结构，不能用同名 low/high 跨方法解释。当前 CLI large_scale 默认开与库/工具默认关分开记录，不将源码默认差异改写为科学结论。

内核无文件 I/O/线程预算，调用者提供 reasons/scratch/输入缓冲；像素/tile 并行由 Runtime 组织。large_scale 需要帧内邻域 mask，不能在任意独立微块上直接声称相同结果。调用 rc 与 status 都要检查，空栈 status 属排异域，不冒用集成 NO_CANDIDATES。

同头的 `p2_reject_classify` 和 probability 校准接口是另一组库能力；没有真实生产输入/消费者证据时，不将其宣称为 CLI 已完成污染机制分类或移动源独立层。最终产品仍为一个 signal HiPS。

### 验证入口

[路由单元入口](../../../eng/tests/unit/test_rejection_routing.py)检查 planning，[Mosaic 管线](pipeline.md)追至真实生产。独立注入检验覆盖无污染、异常点/轨迹、负信号、空栈、小样本、源索引映射、结构生长边界和选择偏差。当前科学/实现边界集中见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)，本页未执行测试。

## integrate

**Mosaic 数值集成与精度消费。**

集成把同位置已校正/已接受样本作数值归约。reducer 与权重策略分工由[集成正本](../../science/integration/INTEGRATION.md)定义；参考点源与面亮度精度的区别由[噪声正本](../../science/noise_snr/NOISE_SNR.md)定义。最终对外只发布 signal HiPS，内部 support、wsum 和方差不因此删除。

### 实际入口与权重来源

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_integrate` 对应 `acsd.phase2.integrate`，读取 `p2_corrected.json` 与 `p2_rejection.json`，构造外部数值 weights，调用 `p2_validate_candidate_weights` 和 `p2_integrate_pixel`。输出 `p2_integrated.json` 及 signal/support/wsum/nused/nrej 工作数组供 writer。

当前优先用已声明可用的 corrected variance，其次已有逐样本 ivar，缺失才使用兼容 SNR 链。[weight_chain.cpp](../../../lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp)处理来源/配对，`snr_path_requested/effective/reason` 记录请求与实际路径。dense 请求当前明确不可用。

`p2_sparse_layer_probe` 读取属性 `ACSD_SPARSE_SNR_LAYER` 引用的规整网格；不是 canonical `frame_science.json` reader。不存在层时的具名帧级路径、声明层损坏的错误和真正 canonical 可估计模型必须分开。兼容 SNR²/F_ref²得到 I_F，不能直接宣称面亮度最优权重；转换还需同条件 T。具体限制见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)，本页不切换算法。

### reducer 公开合同

签名头 [integrate.h](../../../lib/algorithms/coverage/include/astro/phase2/integrate.h)，实现 [integrate.cpp](../../../lib/algorithms/coverage/src/integrate.cpp)。

| 对象/入口 | 合同 |
|---|---|
| `P2PixelStack` | values、weights、support、accepted、count；调用方拥有数组 |
| values/weights | 同一个面亮度标度；权重是调用方提供的数值，若解释逆方差则单位为信号单位平方的倒数 |
| 空 weights/support/accepted | 库 API 分别表示等权、全覆盖、全接受；CLI 实际提供权重数组 |
| `p2_integrate_pixel(in,out)` | 固定输入索引顺序归约正权样本；同时返回 signal、support、n_used 和诊断计数 |
| support reducer | max(accepted support)，包含合法零权 accepted；不是精度权重或精确覆盖并集 |
| `p2_validate_candidate_weights` | 有限非负，零合法；负/NaN/Inf 非法 |

accepted 样本中的非有限值、非法 support 或 weight 不能被 reducer 静默当正常数据。有限零和负信号合法。成功 signal 是加权均值；无有效贡献为 NaN。

### 状态、资源与错误

`P2IntegrateStatus` 现有六态：OK、NO_CANDIDATES、ALL_REJECTED、ZERO_VALID_WEIGHT、INVALID_INPUT、NON_FINITE_OUTPUT。最后一态区分合法输入归约溢出，不能继续写成仅五态，也不能把溢出结果当有效零。

纯 reducer 不持有 cache、不做 I/O、不分配长期大面，不独立建线程池。调用者在像素/tile 间按 Runtime 预算并行，每像素内序固定；资源取消与工作文件生命周期由 adapter 管理。rc 与 status 需分别检查，局部枚举不直接当 CLI 退出码。

普通像元 ivar 平均的最优性依相同被估量、独立随机误差和固定接受集合；PSF/通带失配、共同母版和数据驱动排异不能靠 1/Σw 宣称完整误差传播。最终无第二个 support/variance/ivar 科学图像层。

### 验证入口

从本页 reducer/weight-chain 与[Mosaic 管线](pipeline.md)进入，检查已知加权均值、零权惰性、accepted support 最大值、六态、极端溢出、帧索引、请求/实际路径和 T 不同的独立反例。[稀疏重建研究入口](../../../eng/tests/synthetic/test_p4_dense_snr.py)不自动验证 canonical 生产转换；本页未运行数值测试。

## write

**Mosaic 单信号 HiPS 写出。**

writer 将集成信号和内部归一对象写为一个 signal 科学图像通道。产品范围来自[最高设计](../../ACSD_DESIGN.md)与[Mosaic 管线](pipeline.md)，不能以共用写库支持多 flags 反推本阶段必须发布误差层。

### 生产入口和输入

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_write` 对应 `acsd.phase2.write`。它消费 `p2_integrated.json` 和相应 signal/support/wsum 工作数组，读取 coverage、samples、sky-plane 和 rejection 的实际来源，写马赛克及 `p2_final.json`。

生产入口是七节点 `P2NodeModule`，不是 `lib/algorithms/coverage/tools/stage2.cpp::main`。工具的配置、默认、退出整数与产物能力不能直接当正式 `mosaic` 命令事实。

### 写库 API 与数值

共用 [aio_hips.h](../../../lib/infrastructure/aio/include/aio_hips.h) / [aio_hips_writer.cpp](../../../lib/infrastructure/aio/src/hips/aio_hips_writer.cpp)提供 begin、write_signal_support_tile、set_provenance、finalize/abort 和 verify_product_set。当前 `p2_op_write` 明确设置 `flags=AIO_HIPS_PRODUCT_SIGNAL`。

| 输入/输出 | 合同 |
|---|---|
| integrated signal | 已接受样本的合成球面面亮度；真实单位/光度状态来自输入链 |
| support、wsum、内部方差 | 仍用于有效性、归一及内部统计；不发布为第二科学图像层 |
| `AstroSphereTileView` | 以 flux_sum/covered_area 适配 signal；这两个是写入视图中间量，不改产品为积分通量 |
| nside/order | 叶 nside、NESTED、tile=512；实际叶阶与 tile 阶分清 |
| 输出 | signal 层级、properties、MOC、manifest 与运行摘要；无覆盖 NaN，真实零/负值保留 |

tile FITS 与 NESTED local 的转换复用共享实现，不能各端复制手性公式。输入/模型 hash、排异 profile 与实际可用性由真实工作产物读取，不编造通过状态。最终单信号形态不能被 `uncertainty_available` 的内部模型事实误写成 variance/ivar 已发布。

### 响应、所有权与并行

writer 登记 `response_distribution` 并可写混合响应；不同 filter 字符串不阻止用户合并。单个 obs_filter 不自动等于精确总体响应，尤其有效帧集/权重随位置变化时。当前响应分布边界集中见[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

tile 变换可在 Runtime 预算下并行写独立槽，实际 writer 句柄按序串行调用；输入数组/视图由 adapter 持有至写调用结束。finalize/abort 释放句柄；AIO 完成、目录发布和运行摘要必须分别检查，不由单次 write 返回推断整个目录原子性已成立。

### 配置、错误与验证入口

读取本次输出位置、dtype/精度、目标几何和真实 provenance；不独立选择科学权重。I/O、模型读回、产品一致性或哈希失败通过节点 Result 上行，和合法精度能力受限分开。最终发布纪律依[原子发布合同](../../engineering/contracts/ATOMIC_PUBLISH.md)。

验证从 `p2_op_write` 与 `aio_hips_verify_product_set` 进入，独立枚举仅一个科学通道、真实零/NaN、所有已声明层级、tile 手性、来源 hash、混合响应和失败残留。内部 support 仍参与计算是源码事实；本页未执行写出或重开实验，不将目录自洽等同科学保真。

## session

**Mosaic 局部模型会话。**

本页登记 `p2_session_*` 进程内 facade：coverage、sample、UPM build、可选 persist。完整用户 `mosaic` 命令由[七节点管线](pipeline.md)执行，另含 apply、reject、integrate 和最终 signal HiPS 写出；二者不能混称。

### 接口与调用关系

公开头 [p2_session.h](../../../lib/phase2_session/p2_session.h)、实现 [p2_session.cpp](../../../lib/phase2_session/p2_session.cpp)。接口为 `p2_session_create(host,out)`、`p2_session_validate(handle,config_json)`、`p2_session_run(handle,config_json)`、`p2_session_inspect(handle,out_manifest_json)`、`p2_session_destroy(handle)`，返回 `acsd_status`。C++ `acsd::phase2::last_error` 仅用于诊断。

此会话直接调用 `p2_coverage_build`、采样和 `p2_upm_build/save`。CLI 经 [module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `P2NodeModule` 分节点执行，生产 fit 使用 `p2_sky_plane_build_joint`；不能因保留 P2Api 会话适配就把库 UPM 模型认作生产扣除模型。

### 数据、配置与所有权

| 对象 | 语义 |
|---|---|
| host | allocator/logger/cancel/budget，寿命覆盖 handle |
| config | hips_paths/output_dir，局部 upm 参数与 persist_upm/upm_save_path；以本 facade validate/run 的实际消费为准 |
| coverage、observations | 从产品枚举与采样产生；sky 为 ICRS degree，样本值继承信号单位 |
| model | 本会话构建的 UPM 模型，调用方不能视为完整马赛克 |
| inspect | created/complete/failed 及阶段/模型信息；不是最终 signal 发布凭据 |

创建者拥有 handle，单 handle 串行，不同 handle 可独立运行。输出 JSON 经 host allocator 分配/释放；模型与缓冲由会话 RAII 持有并用相应 close/free 清理。阶段边界检查取消，UPM worker 数来自 host budget；局部会话不建立跨命令内存共享。

### 错误与验证入口

返回 `ACS_ERR_*`，细节由错误摘要与 manifest 说明；纯配置检查与文件/模型构建错误分开。输出路径和配置字段是否被 validate 接受不证明 run 真正消费该字段。

验证从本头五函数检查生命周期、分配配对、非法类型、取消、persist/open；完整七节点检查从 [Mosaic 实现导航](pipeline.md)追至 CLI。科学模型由 [UPM 正本](../../science/sky/UPM.md)拥有；本页不以局部会话成功代替完整合成或无接缝实测。

## resample

**Mosaic 聚合会话适配描述符。**

`acsd.phase2.resample` 是 [module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 中 `phase2_descriptor` 保留的聚合 session 适配名，不是正式 Mosaic 七节点中额外的重采样算法。实际阶段职责与交接见 [Mosaic 管线](pipeline.md)。

### 入口与数据边界

注册工厂通过 `SessionModule/P2Api` 委托 [p2_session.h](../../../lib/phase2_session/p2_session.h) 五函数；实际局部会话为 coverage→sample→UPM build→可选 persist，不能据描述符名宣称它执行球面重新采样、排异、集成和 HiPS 发布。CLI 完整路径使用 `P2NodeModule` 的七个 operation。

| 描述符声明 | 实际解释 |
|---|---|
| calibrated / DATA-P1-CAL / ADU / PIXEL | 聚合兼容声明；不是 Mosaic 正式输入的产品身份 |
| resampled / DATA-P2-RES / ADU / PIXEL | 聚合输出名；不能代替 NESTED 马赛克 signal 合同 |
| cpu_heavy、parallel_ok | 调度元数据；不证明本适配器有独立算法或线程池 |

用户输入实际为已发布逐帧 HiPS；阶段间只通过磁盘产品交换。这个聚合描述符的单位/坐标声明不得反向覆盖[最高设计](../../ACSD_DESIGN.md)的球面面亮度与最终单 signal 产品要求。

### 生命周期、错误与验证

handle、host allocator、budget、cancel、JSON 和 manifest 的实际所有权依[局部 session 卡](modules.md#session)。本描述符不定义新的 source 数组、WCS、插值核、缓存格式或物理单位，不新增重采样配置。返回码经 session→Result 映射，不能把占位输出存在当完整马赛克成功。

验证应分别检查聚合工厂确实委托会话、CLI 七节点没有误选该兼容描述符，以及产品消费端使用真实 HiPS 合同。没有独立 kernel 就没有本页专属数值通过证据；本页未运行测试。生产差异统一见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 归约与背景内核的当前接口核对

以下按当前公开头和生产调用补充，对上文保留模块材料中的旧五态、行数、固定ADU或旧工具生产声明作消歧。科学定义分别由[集成](../../science/integration/INTEGRATION.md)和[天空模型](../../science/sky/UPM.md)拥有，旧算法全文保存在[来源档案](../../../实验/documentation-refactor/docs/MOSAIC_REDUCERS_SOURCE.txt)。

### 集成入口、状态与计数

公开头为lib/algorithms/coverage/include/astro/phase2/integrate.h，实现为coverage/src/integrate.cpp。p2_integrate_pixel(const P2PixelStack*,P2PixelResult*)同步借用数组，由调用者持有输入/输出；无内部线程或可变共享状态，独立缓冲可并发，同一输出不可无同步共写。内部为C++可调用C链接面，头使用cstdint/std::uint32_t，不能由extern C推断纯C编译能力。

P2PixelStack包含values、weights、support、accepted和count。values缺失或count=0归NO_CANDIDATES；weights缺省等权、support缺省1、accepted缺省全接受。公开函数p2_validate_candidate_weights接受非负有限权重，null合规。算法值域保持实际面亮度尺度，reducer不决定物理响应、精度策略或最终图像发布集合。

| status | 数值 | 实际含义 |
|---|---|---|
| OK | 0 | 输入合法、有正权贡献且结果有限 |
| NO_CANDIDATES | 1 | count=0或values=null |
| ALL_REJECTED | 2 | 有候选但无接受样本 |
| ZERO_VALID_WEIGHT | 3 | 接受样本无正权贡献 |
| INVALID_INPUT | 4 | 接受样本值/支撑/权重非法 |
| NON_FINITE_OUTPUT | 5 | 输入有限但乘积、累加或商不可表示 |

rc=1仅用于stack/result空指针，rc=0时仍需检查status。输出先memset为零，空栈/全拒/非法/全零权等分支不都主动填NaN；生产调用者按非OK转换信号。因此不能凭默认signal=0当成功或声称库全部失败分支已编码NaN。NON_FINITE_OUTPUT自身写NaN并保留支撑，ZERO_VALID_WEIGHT保留接受样本支撑。

n_candidates=count；n_accepted计掩膜接受数；当前n_finite在检查value/support之后、检查weight之前增加，与头注释“value/support/weight finite”不完全相符。n_positive_weight/n_used计真实正权贡献；INVALID_INPUT仍可能保留先前合法样本计数，不能据n_used>0当可发布。support在权重检查前取合法value/support的最大值，包含接受零权样本，且实现未检查support≤1。

实际CLI调用在module_adapters.cpp::p2_op_integrate，构造外部weights并调用reducer与权重校验；非OK信号转换NaN，ZERO_VALID_WEIGHT的内部wsum为0，其余失败设NaN。上层完整掩膜/溢出计数还须验证。旧stage2/ACR文档不作为当前CLI入口证明。真实权重构造与点源到面亮度传递仍见实施差异，reducer成功不认证其科学含义。

### 背景库与生产链

coverage/src/upm.cpp保留p2_upm_build、raw_weight、convergence及持久化库能力，当前mosaic adapter生产拟合/施加使用p2_sky_plane_build_joint、p2_sky_plane_open与p2_sky_plane_eval_delta_block。不能把保留M+C内核、同名upm端口和旧模型文件当作当前扣除路径。

p2_upm_control_variance输入k_corr、sigma_bg、n_retained及域/标定身份，输出控制方差和逆方差；当前拒k_corr<1、=1以及缺少规定身份的情况，具体码值从upm.h/实现核对，不直接映射CLI。其科学适用域、弱锚和格内归一问题记录在Q31，本轮未改。库调用者持有输入数组，opaque model按公开释放接口回收，在途求值结束后才销毁；构建成功与converged标志分开报告。
