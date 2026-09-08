---
id: MOD-astrocs-phase2-sample
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-P2-SMP-001, ALG-P2-SMP-001, API-P2-001]
downstream: [TEST-P2-SMP-001]
---

# 模块 astrocs.p2.sampling（P2-SAMP-DOC 新建，2026-09-09）

> P2-SAMP-DOC（SA-P2-S20）新建模块页。合同三件套落位
> `lib/phase2_samp/`（README/module.yaml/memory.md，按
> `lib/phase2_int/`→`lib/hips_p2/` 先例新建；`lib/phase2/` 一目录
> 一套已被 P2-COV 占用，不可覆盖）；合同权威=三件套 +
> docs/algorithms/PHASE2_SAMPLER.md（ALG-P2-SMP-001，
> CONTRACT_READY）。descriptor 词汇 module_id=astrocs.phase2.sample
> （module_adapters.cpp:580-592）为编排层占位，由 P2-XX-INT 对齐
> astrocs.p2.sampling（MODULE_MIGRATION_MATRIX P2-SAMP 行），不得
> 反向作为冻结依据。

## 身份与合同落位

- MOD ID：`MOD-astrocs-phase2-sample`（registry 行 ID 沿用，本页与
  registry astrocs.phase2.sample.md 同步合同页）；module_id 合同值=
  `astrocs.p2.sampling`（矩阵 P2-SAMP 行；descriptor 占位
  `astrocs.phase2.sample` 仅编排层词汇）；dll_target=
  `astrocs_p2_sampling.dll`（合同值，尚未存在，迁移归 P2-SAMP-IMPL）。
- 合同三件套：`lib/phase2_samp/`（README/module.yaml/memory.md），按
  `lib/phase2_int/`→`lib/hips_p2/` 先例新建；`lib/phase2/` 三件套已
  被 P2-COV（astrocs.p2.coverage）占用，不可覆盖。
- 生产源：`lib/phase2/src/sampler.cpp`（1156 行，根 CMakeLists.txt
  :337-346 astrocs_phase2 静态库成员，sampler.cpp 列于 :342）+
  唯一权威签名头 `lib/phase2/include/astro/phase2/sampler.h`
  （136 行）。模块页=本文件。
- owner SA-P2-S20；depends_on_int=P2-COV-INT;CPU-005
  （MODULE_MIGRATION_MATRIX.csv P2-SAMP 行权威）；
  legacy_paths="lib/phase2 sampling sources"。

## 职责与明确非职责

- 职责：三阶段 background-clean 控制点采样——Stage A 候选 patch
  （cell 中心 ±background_patch_radius 默认 17×17）→ Stage B 亮端
  迭代 sigma-clipping（median/MAD，保留负值）→ Stage C DBE-like
  局部 tolerance gate（同 tile 邻域基线）→ Stage D
  contamination/retained 双门 → Stage E SNR catalogue veto
  （sampler.cpp:584-591 冻结注释）。控制点几何由 union 几何与目标
  角间距决定、**不由 SNR 决定**（sampler.h:6 语义冻结）；每 union
  tile 8×8 cell 网格，control_id=cells 索引，out_n_controls=
  n_union×G² 含空覆盖占位（h:118-119）。control estimator 方差
  权威 = k_corr×(π/2)×σ_bg²/N_retained（:840-842；
  ALG-UPM-CONTROL-IVAR-001；k_corr 逐帧 Drizzle provenance 查表
  :547-555，回退冻结保守 1.4 :82）。frame_id 内容稳定身份
  （truncated-64 canonical SHA-256，h:85-93/DATA-FRAME-ID-001）。
- 工作域纪律：value 为 patch median（可负，ADU）；ivar 弃用仅诊断
  （单 leaf Phase1 ivar ≠ Var(control estimator)），科学权重一律
  control_ivar；snr_available=0 时 snr 回退整帧精确中位，禁以 1.0
  伪装 unknown（upm.h:51-55）。
- 非职责：不做 UPM 拟合/权重归一（P2-UPM 下游）；不做星点检测
  （SNR 纯查询，h:11-12）；不做 coverage union 计算（上游 P2-COV
  域 DATA-P2-COV）；无 session 依赖（coverage 数据面显式传入）；
  不产 per-pixel 科学场产品；≥2 clean 帧才入 UPM（:1013-1027，
  单帧区由 UPM Laplacian 延续）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（module_adapters.cpp:580-592 实测；占位
词汇，按 registry 生成词保留）:

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `coverage` | `DATA-P2-COV` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |
| `samples` | `DATA-P2-SMP` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同=DATA-P2-SMP（DATA_SEMANTICS §23，P2-SAMP-DOC
同批冻结）:

- 输入 P2CoverageResult（n_union 上限 1e6、cells 上限 2e8）+
  hips_paths/frame_ids（cached 版可空=内部重算；0=非法哨兵
  :512-523）+ P2SamplerConfig 15 字段（默认单一来源 :294-312；
  `<=0→默认` 修补吞显式 0=DISP-P2SMP-001）。
- 输出 P2ControlObservation 13 字段（frame_id/control_id/leaf_ipix
  u64、ra_deg/dec_deg/value/uncertainty/snr/ivar/control_variance/
  control_ivar/support f64、snr_available int、quality_flags u32）
  + P2SampleStats 10 字段 u64 诊断计数（insufficient_retained
  现状双计数=DISP-P2SMP-002）+ P2ControlNode 7 字段（out_n_controls
  全几何含空覆盖占位，与 accepted/overlap_controls 区分）。
- invalid 显式化: bad args/frame_id 0/open failed/n_union>1e6/
  cells>2e8/首 tile 越界/exception → rc=1（err 8KB 文本）；容量
  不足**不报错**（probe/fill 截断拷贝 + out_n_* 真实需求，
  h:101-102 冻结）；ivar 产品缺失 → o.ivar=0.0 如实降级（UPM 侧
  回退 1/uncertainty²）；catalogue 缺失 → snr_available=0。

## 公共 header、核心 symbol 与生命周期

- 唯一权威签名头: lib/phase2/include/astro/phase2/sampler.h
  （136 行；cfg :32-57、stats :63-74、node :77-83、frame_id 冻结注
  :85-92、stats :95-99、probe/fill 冻结注 :101-102、两入口
  :103-114/:120-132）。
- 核心 symbol: p2_sampler_default_config（:60，配置默认单一来源）、
  p2_frame_id（:93）、p2_stats_median（:97）/p2_stats_mad（:98-99，
  与 UPM 域共享实现）、p2_sample_controls（:103，基础入口）、
  p2_sample_controls_cached（:120，生产入口）。
- 生命周期=调用方（可选 frame_id 预计算 stage2.cpp:219-231）→
  probe 查容量（out_obs=nullptr :279-281）→ 分配 → fill（:306-311）；
  无 create/destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase2.sample`（占位）；execution_class=
`cpu_heavy`；parallel_ok=True。配置=P2SamplerConfig 15 字段
（sampler.h:32-57；control_k_corr 默认 1.4 h:47-53 冻结）+ sccfg
14 字段显式透传（stage2.cpp:256-274；control_k_corr 未透传，零
初始化经 impl :497-498 修补回退默认）。

## Execution class、并行轴、ThreadBudget lease、确定性

- `cpu_heavy`；并行轴=union cell 间（模块内 std::thread 池
  :886-913，next_c.fetch_add 动态领取、固定槽位写回 cells[idx]
  :870-872、per-worker 独立 AIO 句柄 :894；=1 串行 reference
  :916-933）；OpenMP 已从实现移除（:882-883 注释），lib/phase2/
  CMakeLists.txt:28 P2_ENABLE_OPENMP option 保留仅旧 target 编译面。
- **输出 obs 序列 bitwise 与 worker 数无关**（1/N 等价）+ 第三遍
  单线程顺序扫描；验证门=F8
  sampler_parallel_consistency_test.cpp:29
  TEST(Phase2SamplerParallel, OneTvsTwoTDeterminism)。
- worker 数=Runtime lease（cfg.cpu_workers=ThreadBudget.max_workers
  经 stage2.cpp:273-274 透传，模块无 hardware_concurrency 自行开
  线程 :880-883）；lease/取消检查点接线=迁移整改点（P2-SAMP-IMPL，
  与 DISP-COV-005 同构）。determinism=fixed_reduction_order。

## 内存/cache/I-O/所有权

- I/O=astro_image_io（AIO）HiPS tile 读（signal/support/snr/ivar
  四产品 :529-546；read_tile_pair :163-180）；观测/节点输出缓冲
  调用方分配（probe/fill 协议 h:101-102）；cells 中间态模块内
  持有（:649-654，上限 2e8 :644-648）。
- 全局态仅 g_aio_mu（:161，read_tile_pair :166 加锁，仅串行路径
  共享句柄）与 frame_id AIO 缓存面；reentrant=yes /
  threadsafe=no（h 无线程注记，经 impl 实测）。

## 错误、日志、指标、取消和 checkpoint

- 错误面=rc 二值 + err 8KB 文本（细分语义=DATA §23.5/ALG §11.1）；
  无状态机（accept/reason u8 0..5 逐观测承载，DATA §23.3）；
  容量不足不报错（probe/fill）。
- 诊断进度日志 17 处直写 stderr（:641-672 等，DISP-P2SMP-003
  登记；结构化通道整改归 P2-SAMP-IMPL）；无内部取消检查点/
  checkpoint（迁移 ThreadLease 接线归 P2-SAMP-IMPL）。
- known_defects（登记不改码，与 ALG-P2-SMP-001 §11.2 同口径）:
  DISP-P2SMP-001（cfg `<=0→默认` 吞显式 0 :485-502，bughunt
  R3-A P3-③）；DISP-P2SMP-002（insufficient_retained 双计数
  :1006+:1022）；DISP-P2SMP-003（stderr 直写）；DISP-P2SMP-004
  （veto 阈值 10×frame_snr_med 与半径 0.012° 硬编码 :849-850）；
  DISP-P2SMP-005（m0≈0 收敛阈值退化全迭代 :818）。整改归
  P2-SAMP-IMPL/TEST。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-SMP-001` MISSING（P2-SAMP-TEST 建立，不冒认）；
登记面=TEST-P2-SMP-DESIGN-001 设计冻结 VERIFIED（ALG-P2-SMP-001
§11.3 F1-F9: F1 统计量逐值 bitwise、F2 kcorr 角点 exact/插值
rtol 1e-12、F3 cvar Python oracle rtol 1e-12 + UPMW-004 MC 3σ、
F4 坐标 atol 1e-9 deg、F5 constant/gradient/impulse cvar rtol
1e-12、F6 边界/seam exact、F7 missing/invalid exact、F8 串并行
bitwise、F9 计数守恒现状口径）。现状相邻证据（引用不冒认）:
lib/phase2/tests/synthetic_gate.cpp Phase2Sampler 组
（RealHipsControlSampling :3423、G6LocalSnrAvailabilityThreeZones
:3470、G1StatisticsCorrectness :3594、UPMW-004 MC :4001、cvar
:4089）；lib/phase2/tests/sampler_parallel_consistency_test.cpp
:29；ivar_wiring_test.cpp:223（ivar 面）。

## 已知限制

- DISP-P2SMP-001..005（上节，登记不改码）。
- 本页若引用旧派生词汇（"错误码=ACS_ERR_*"、"取消=host cancel
  回调"等 session 层词汇），以本合同页与 DATA_SEMANTICS §23 为准
  修订。

## 链接

- 合同三件套：`lib/phase2_samp/`（README/module.yaml/memory.md）
- registry 页：docs/modules/registry/astrocs.phase2.sample.md
- SCI：docs/science/PHASE2_UPM.md（SCI-UPM-001，FROZEN T106，零
  改动；descriptor 占位 SCI-P2-SMP-001⇒SCI-UPM-001 映射声明=ALG
  §11.4）
- ALG：docs/algorithms/PHASE2_SAMPLER.md（ALG-P2-SMP-001）；
  DATA：DATA_SEMANTICS §23（DATA-P2-SMP）；API：API-P2-SMP-001
  （PUBLIC_API.md）+ 编排层 API-P2-001（FROZEN）
