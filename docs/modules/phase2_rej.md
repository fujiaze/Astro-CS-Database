---
id: MOD-astrocs-phase2-reject
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-REJ-001, ALG-P2-REJ-001, API-P2-001]
downstream: [TEST-P2-REJ-001]
---

# 模块 astrocs.p2.rejection（P2-REJ-DOC 新建，2026-09-09）

> P2-REJ-DOC（SA-xxx）新建模块页。合同三件套落位 `lib/phase2_rej/`
> （README/module.yaml/memory.md，按 `lib/phase2_int/`→`lib/hips_p2/`
> 先例新建；`lib/phase2/` 一目录一套已被 P2-COV 占用，不可覆盖）；
> 合同权威=三件套 + docs/algorithms/PHASE2_REJECTION.md
> （ALG-P2-REJ-001，CONTRACT_READY）。descriptor 词汇
> module_id=astrocs.phase2.reject（module_adapters.cpp:700-717，
> 注册 :785）为编排层占位，由 P2-XX-INT 对齐 astrocs.p2.rejection
> （MODULE_MIGRATION_MATRIX P2-REJ 行），不得反向作为冻结依据。

## 身份与合同落位

- MOD ID：`MOD-astrocs-phase2-reject`（registry 行 ID 沿用，本页与
  registry astrocs.phase2.reject.md 同步合同页）；module_id 合同值=
  `astrocs.p2.rejection`（矩阵 P2-REJ 行；descriptor 占位
  `astrocs.phase2.reject` 仅编排层词汇）；dll_target=
  `astrocs_p2_rejection.dll`（合同值，尚未存在，迁移归 P2-REJ-IMPL）。
- 合同三件套：`lib/phase2_rej/`（README/module.yaml/memory.md），按
  `lib/phase2_int/`→`lib/hips_p2/` 先例新建；`lib/phase2/` 三件套已
  被 P2-COV（astrocs.p2.coverage）占用，不可覆盖。
- 生产源：`lib/phase2/src/rejection.cpp`（2076 行，根 CMakeLists.txt
  :336-346 astrocs_phase2 静态库成员，rejection.cpp 列于 :340）+
  唯一权威签名头 `lib/phase2/include/astro/phase2/rejection.h`
  （329 行）。模块页=本文件。
- owner SA-xxx；depends_on_int=P2-UPM-INT;CPU-005（MODULE_MIGRATION_MATRIX.csv:15 权威；P2-REJ-INT 为 P2-INT 行本域被依赖项，勿混淆）；
  legacy_paths="lib/phase2 rejection sources"。

## 职责与明确非职责

- 职责：每像素候选栈排异决策——eligibility strided gather 单路径
  （source_indices 权威映射 PHASE2_IVAR_WIRING，rejection.h:252-255，
  compact 后禁止用 compact index 猜 original slot）→ planning 层
  AUTO 一次解析（nominal n<6→PERCENTILE、6..15→WINSORIZED、>15→
  LINEAR_FIT；profile wbpp_2_9_1 / astrocs_adaptive）→ 10 显式方法核
  （NONE/SIGMA/WINSORIZED/AVERAGED/LINEAR_FIT/ESD/RCR/PERCENTILE/
  MEDIAN_SIGMA/MINMAX，AUTO=10 永不进 kernel；per-sample reason u8
  0..3 与 stack status int 0..7 分离；判向冻结=低于 lower threshold→
  REJECTED_LOW、高于 upper→REJECTED_HIGH，禁原始值正负号判向，
  rejection.h:20-21）→ large_scale 结构生长后处理（trail 扩张只增
  不减，compact cosmic 不生长，默认关闭）。
- 阈值/迭代权威锚定：rejection.cpp:1-11 冻结头注释"本文件为阈值/
  迭代权威实现，禁止阈值漂移"；AUTO 路由禁止 per-pixel n_eff 重选。
- 工作域归一 NONE/MEDIAN_CENTER/MEDIAN_SCALE（floor 1e-12，不除零）；
  mask 应用回原始 calibrated 值（经 source_indices 回映射）。
- 非职责：不合并/积分样本（P2-INT 下游）；不做权重策略（weights
  数组外置，构造在 Stage2 weight_mode；RCR 核消费同栈 weights 数组
  属官方加权语义，非策略）；不做像素外结构重建（large_scale 仅对
  已拒 mask 做 8 邻域扩张，只增不减）；无 session 依赖（无状态纯
  函数）；不做瞬变/卫星语义区分（SCI §1 非目标）；单帧无排异
  （n=1 进 UNDERDETERMINED 白名单）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（module_adapters.cpp:707-710 实测；占位
词汇，按 registry 生成词保留）:

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `corrected` | `DATA-P2-COR` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `accepted_mask` | `DATA-P2-REJ` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同=DATA-P2-REJ（DATA_SEMANTICS §22，P2-REJ-DOC
同批冻结）:

- 输入 P2CandidateStack（values f64 ADU / weights f64 1/ADU² 可空=
  等权 / frame_ids u64 / count u32）+ P2RejectionPlan（typed params
  六组；AUTO 仅在 plan_resolve 解析，进 kernel 即 INVALID_METHOD；
  normalization floor 1e-12；underdetermined_n=2）。
- 输出 P2RejectionDecision（reasons u8 0..3 / accepted_count /
  rejected_low / rejected_high / iterations u32 / status int 0..7）
  + gather 四诊断计数（invalid_finite/invalid_valid/invalid_support/
  invalid_quality）+ large_scale 原地 u8 mask（low/high 独立半径）。
- invalid 显式化: 非 finite 输入→INVALID_INPUT；AUTO 入 kernel→
  INVALID_METHOD；PERCENTILE×norm≠MEDIAN_CENTER、RCR×norm≠NONE→
  INVALID_CONFIGURATION；空栈→MIN_SAMPLES（**非** NO_CANDIDATES，
  DISP-P2REJ-002；NO_CANDIDATES 属积分域 P2IntegrateStatus）。

## 公共 header、核心 symbol 与生命周期

- 唯一权威签名头: lib/phase2/include/astro/phase2/rejection.h
  （329 行；方法枚举 :45-57、reason :71-77、status :79-90）。
- 核心 symbol: p2_reject_plan_resolve（:191，AUTO 路由唯一解析点）、
  p2_eligibility_filter（:222）、p2_collect_candidate_stack（:263，
  生产 strided gather）、p2_reject_stack_ex（:287，生产入口）、
  p2_rejection_semantic_id（:196）、p2_large_scale_apply（:295，
  生产 stage2 唯一调用点）；compat p2_reject_stack（:325，仅测试/
  旧调用，:299 冻结注释"生产 Stage2 不再调用"）。
- 生命周期=调用方顺序 plan_resolve→gather→stack_ex→large_scale；
  无 create/destroy，无状态纯函数（reentrant）。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase2.reject`（占位）；execution_class=
`cpu_heavy`；parallel_ok=True（module_adapters.cpp:702-706 实测）。
配置=stage2_common.h reject_method/reject_profile/
reject_underdetermined_n/reject_normalization(+floor 1e-12)/
large_scale_*（stage2_common.h:52-63）+ typed params 唯一默认源=
cfg（stage2.cpp:698-728）。

## Execution class、并行轴、ThreadBudget lease、确定性

- `cpu_heavy`；并行轴=像素间（调用方 OMP: stage2.cpp:1288/:1298
  schedule(static)、acr_kernels.cpp:218/:228 schedule(static)）；
  rejection.cpp 无任何线程原语，逐样本独立判定、无跨样本归约 →
  **结果与 worker 数无关（1..N bitwise）**。
- per-thread 统计 thread id 定序归并（stage2.cpp:1305-1313）；
  large_scale 激活强制串行（stage2.cpp:1280 条件）。
- 确定性合同: 同输入同 plan 同 fid → decision bitwise；ESD
  tie-break=frame_id（1e-15 epsilon）、linear_fit 排序
  (value,orig_index) 字典序。
- worker 数=ThreadBudget.max_workers（禁 hardware_concurrency）；
  lease/取消检查点接线=迁移整改点（P2-REJ-IMPL，与 DISP-COV-005
  同构）。determinism=fixed_reduction_order。

## 内存/cache/I-O/所有权

无文件 I/O（纯函数）；kernel 内 n≤64 固定 scratch、>64 走堆
（rejection.h:286）；scratch 所有权=调用方分配（reasons 缓冲等由
调用方提供，P2CandidateStack/P2RejectionDecision 调用方持有）；
无内部 cache 与全局状态（reentrant=yes）。

## 错误、日志、指标、取消和 checkpoint

- 错误面=八态 status（0..7）+ rc=1（null/非法参数；plan_resolve
  null/出界/非法 profile；large_scale 参数非法 :2054-2060）；rc=0
  时语义全由 status 承载（"科学状态"而非调用错误）。调用方合同:
  status ∈ {OK, UNDERDETERMINED} 才可继续积分（stage2.cpp:1189-1195
  冻结门）。
- 无日志/指标输出（纯函数；编排层日志在 stage2.cpp:730-738）；
  无内部取消检查点/checkpoint（迁移 ThreadLease 接线归
  P2-REJ-IMPL）。
- known_defects（登记不改码，与 ALG-P2-REJ-001 §7/§11.3 同口径）:
  DISP-P2REJ-001（rejection.h:118 percentile low_fraction 注释
  "默认 0.1" vs 实现/SCI 权威 0.2，注释漂移，整改=注释对齐）；DISP-
  P2REJ-002（SCI §8 "空栈→NO_CANDIDATES" vs 实现 MIN_SAMPLES，
  NO_CANDIDATES 属积分域，语义权威=ALG §4.1）；DISP-P2REJ-003（SCI
  §2/§5 行号锚漂移，行号权威=ALG §3 实测 2076/329 行）；DISP-
  P2REJ-004（minmax 比较器 value-only tie-break 未显式冻结，整改
  候选=index tie-break + 等值门）。整改归 P2-REJ-IMPL/TEST。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-REJ-001` MISSING（P2-REJ-TEST 建立，不冒认）；
登记面=TEST-P2-REJ-DESIGN-001 设计冻结 VERIFIED（ALG-P2-REJ-001
§11.4 F1-F8: ESD NIST Rosner 54 值拒集 bitwise、AUTO 路由枚举精确、
small-N 状态穷尽、卫星注入 mask 精确、置换不变性 decision bitwise、
typed params 逐位、Python oracle rtol 1e-12、gather 逐元素精确；
F1-F6/F8 无 epsilon 门、F7 rtol 1e-12、large_scale mask 精确）。
现状相邻证据（引用不冒认）: lib/phase2/tests/synthetic_gate.cpp
R1/R2/LinearFit/Rcr/G4（:2639-2725）+ G6（ESD NIST Rosner54
:2779-2863）+ V15-V17（:4138-4864）；tests/backend/
test_p2004_reject_integrate.py（P2-004 生产 Oracle）；tests/unit/
p2_rejection_test.cpp（P2-005 语义 id/解析面）。

## 已知限制

- DISP-P2REJ-001/002/003/004（上节，登记不改码）。
- 本页若引用旧派生词汇（"错误码=ACS_ERR_*"、"取消=host cancel
  回调"等 session 层词汇），以本合同页与 DATA_SEMANTICS §22 为准
  修订。

## 链接

- 合同三件套：`lib/phase2_rej/`（README/module.yaml/memory.md）
- registry 页：docs/modules/registry/astrocs.phase2.reject.md
- SCI：docs/science/REJECTION.md（SCI-REJ-001，FROZEN T107，零改动；
  descriptor 占位 SCI-P2-REJ-001⇒SCI-REJ-001 映射声明=ALG §11.5）
- ALG：docs/algorithms/PHASE2_REJECTION.md（ALG-P2-REJ-001）；
  DATA：DATA_SEMANTICS §22（DATA-P2-REJ）；API：API-P2-REJ-001
  （PUBLIC_API.md）+ 编排层 API-P2-001（FROZEN）
