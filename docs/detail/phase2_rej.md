# 模块 astrocs.p2.rejection

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同三件套落位 `lib/algorithms/rejection/`（README/module.yaml/memory.md；
> 落位规则见 docs/detail/README.md）。合同权威 = 三件套 +
> docs/science/algorithms/PHASE2_REJECTION.md（ALG-P2-REJ-001，CONTRACT_READY）。
> descriptor 词汇 module_id=astrocs.phase2.reject（p2_reject_descriptor）为编排层
> 口径，其对齐属迁移目标（未落地）；冻结依据 = `docs/science/REJECTION.md`
> （SCI-REJ-001）与 `docs/science/algorithms/PHASE2_REJECTION.md`（ALG-P2-REJ-001）。

## 身份与合同落位

- MOD ID：`MOD-astrocs-phase2-reject`（registry 行 ID 沿用，本页与
  registry astrocs.phase2.reject.md 同步合同页）；module_id 合同值=
  `astrocs.p2.rejection`（descriptor 占位 `astrocs.phase2.reject` 仅编排层词汇）；
  dll_target=
  `astrocs_p2_rejection.dll`（合同值，尚未存在；迁移目标未落地）。
- 合同三件套：`lib/algorithms/rejection/`（README/module.yaml/memory.md；
  落位规则见 docs/detail/README.md）。
- 生产源：`lib/algorithms/coverage/src/rejection.cpp`（2076 行，属根 `CMakeLists.txt` 的
  astrocs_phase2 静态库成员）+
  签名头正本 `lib/algorithms/coverage/include/astro/phase2/rejection.h`
  （329 行）。模块页=本文件。
- owner SA-xxx；depends_on_int=P2-UPM;CPU-005（P2-REJ 为 P2-INT 行本域被依赖项，勿混淆）；
  legacy_paths="lib/algorithms/coverage rejection sources"。

## 职责与明确非职责

- 职责：每像素候选栈排异决策——eligibility strided gather 单路径
  （source_indices 权威映射 PHASE2_IVAR_WIRING，签名见
  `lib/algorithms/coverage/include/astro/phase2/rejection.h`，
  compact 后 original slot 只经 source_indices 映射）→ planning 层
  AUTO 一次解析（生产默认 profile astrocs_adaptive_pixel（自研），档位表与阈值
  唯一正本 = `docs/detail/algorithms_phase2/12_rejection.md` §9 与
  `docs/science/REJECTION.md` §5）→ 10 显式方法核
  （NONE/SIGMA/WINSORIZED/AVERAGED/LINEAR_FIT/ESD/RCR/PERCENTILE/
  MEDIAN_SIGMA/MINMAX，AUTO=10 永不进 kernel；per-sample reason u8
  0..3 与 stack status int 0..7 分离；判向冻结=低于 lower threshold→
  REJECTED_LOW、高于 upper→REJECTED_HIGH，禁原始值正负号判向，
  同一 `rejection.h` 的判向冻结注）→ large_scale 结构生长后处理（trail 扩张只增
  不减，compact cosmic 不生长，默认关闭）。
- 阈值/迭代权威锚定：`lib/algorithms/coverage/src/rejection.cpp` 冻结头注释"本文件为阈值/
  迭代权威实现"（阈值/迭代取值以该冻结头为准，漂移即违约）；AUTO 路由只按 N 的几何值选算法（per-pixel n_eff 不参与重选）。
- 工作域归一 NONE/MEDIAN_CENTER/MEDIAN_SCALE（floor 1e-12，不除零）；
  mask 应用回原始 calibrated 值（经 source_indices 回映射）。
- 非职责：不合并/积分样本（下游）；不做权重策略（weights
  数组外置，构造在 Stage2；RCR 核消费同栈 weights 数组
  属官方加权语义，非策略）；不做像素外结构重建（large_scale 仅对
  已拒 mask 做 8 邻域扩张，只增不减）；无 session 依赖（无状态纯
  函数）；不做瞬变/卫星语义区分（SCI §1 非目标）；单帧无排异
  （n=1 进 UNDERDETERMINED 白名单）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（p2_reject_descriptor；
词汇按 registry 生成词保留）:

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `corrected` | `DATA-P2-COR` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `accepted_mask` | `DATA-P2-REJ` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同=DATA-P2-REJ（DATA_SEMANTICS §22）:


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
  INVALID_CONFIGURATION；空栈→MIN_SAMPLES（**非** NO_CANDIDATES；
  NO_CANDIDATES 属积分域 P2IntegrateStatus，见 ALG-P2-REJ-001 §11.3）。

## 公共 header、核心 symbol 与生命周期

- 签名头正本: lib/algorithms/coverage/include/astro/phase2/rejection.h
  （329 行；方法枚举 / reason / status 三处定义段）。
- 核心 symbol（全部在 `lib/algorithms/coverage/include/astro/phase2/rejection.h` 声明、
  `lib/algorithms/coverage/src/rejection.cpp` 定义）: p2_reject_plan_resolve（AUTO 路由唯一解析点）、
  p2_eligibility_filter、p2_collect_candidate_stack（生产 strided gather）、
  p2_reject_stack_ex（生产入口）、p2_rejection_semantic_id、p2_large_scale_apply
  （生产 stage2 唯一调用点）；compat p2_reject_stack（仅测试路径调用，
  同头文件有冻结注释"生产 Stage2 不调用"）。
- 生命周期=调用方顺序 plan_resolve→gather→stack_ex→large_scale；
  无 create/destroy，无状态纯函数（reentrant）。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase2.reject`（占位）；execution_class=
`cpu_heavy`；parallel_ok=True（p2_reject_descriptor）。
配置=`lib/algorithms/coverage/include/astro/phase2/stage2_common.h` 的
reject_method/reject_profile/reject_underdetermined_n/reject_normalization
(+floor 1e-12)/large_scale_* + typed params 唯一默认源=
cfg（`lib/algorithms/coverage/tools/stage2.cpp`）。

## Execution class、并行轴、ThreadBudget lease、确定性

- `cpu_heavy`；并行轴=像素间（调用方 OMP：`lib/algorithms/coverage/tools/stage2.cpp`
  与 `lib/algorithms/coverage/src/acr_kernels.cpp` 均用 schedule(static)）；
  rejection.cpp 无任何线程原语，逐样本独立判定、无跨样本归约 →
  **结果与 worker 数无关（1..N bitwise）**。
- per-thread 统计 thread id 定序归并（同 `stage2.cpp`）；
  large_scale 激活强制串行（同 `stage2.cpp` 的分支条件）。
- 确定性合同: 同输入同 plan 同 fid → decision bitwise；ESD
  tie-break=frame_id（1e-15 epsilon）、linear_fit 排序
  (value,orig_index) 字典序。
- worker 数=ThreadBudget.max_workers（禁 hardware_concurrency）；
  lease/取消检查点接线=迁移整改点（与 coverage 域同构，见 ALG-COV-001 §11.3）。
  determinism=fixed_reduction_order。

## 内存/cache/I-O/所有权

无文件 I/O（纯函数）；kernel 内 n≤64 固定 scratch、>64 走堆
（`lib/algorithms/coverage/include/astro/phase2/rejection.h`）；scratch 所有权=调用方分配（reasons 缓冲等由
调用方提供，P2CandidateStack/P2RejectionDecision 调用方持有）；
无内部 cache 与全局状态（reentrant=yes）。

## 错误、日志、指标、取消和 checkpoint

- 错误面=八态 status（0..7）+ rc=1（null/非法参数；plan_resolve
  null/出界/非法 profile；large_scale 参数非法，见
  `lib/algorithms/coverage/src/rejection.cpp`）；rc=0
  时语义全由 status 承载（"科学状态"而非调用错误）。调用方合同:
  status ∈ {OK, UNDERDETERMINED} 才可继续积分（`lib/algorithms/coverage/tools/stage2.cpp`
  冻结门）。
- 无日志/指标输出（纯函数；编排层日志在 `lib/algorithms/coverage/tools/stage2.cpp`）；
  无内部取消检查点/checkpoint（迁移 ThreadLease 接线待落地）。
- known_defects（登记不改码；正本 = ALG-P2-REJ-001 §7/§11.3）:
  percentile low_fraction 注释「默认 0.1」与实现/SCI 权威 0.2 漂移（整改 =
  注释对齐）；SCI §8「空栈→NO_CANDIDATES」与实现 MIN_SAMPLES 的口径差
  （NO_CANDIDATES 属积分域，语义权威 = ALG §4.1）；SCI §2/§5 行号锚漂移
  （行号权威 = ALG §3 实测）；minmax 比较器 value-only tie-break 未显式冻结
  （整改候选 = index tie-break + 等值门）；整改面未落地。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-REJ-001` 待建（不冒认）；
登记面=TEST-P2-REJ-DESIGN-001 设计冻结 VERIFIED（ALG-P2-REJ-001
§11.4 F1-F8: ESD NIST Rosner 54 值拒集 bitwise、AUTO 路由枚举精确、
small-N 状态穷尽、卫星注入 mask 精确、置换不变性 decision bitwise、
typed params 逐位、Python oracle rtol 1e-12、gather 逐元素精确；
F1-F6/F8 无 epsilon 门、F7 rtol 1e-12、large_scale mask 精确）。
现状相邻证据（引用不冒认）: lib/algorithms/coverage/tests/synthetic_gate.cpp
R1/R2/LinearFit/Rcr/G4 + G6（ESD NIST Rosner54）+ V15-V17；eng/tests/backend/
test_p2004_reject_integrate.py（P2-004 生产 Oracle）；eng/tests/unit/
p2_rejection_test.cpp（P2-005 语义 id/解析面）。

## 已知限制

- 缺陷与现行语义正本 = `docs/science/algorithms/PHASE2_REJECTION.md` §11.3
  （本页只留指针）。

## 链接

- 合同锚：`lib/algorithms/rejection/`（README/module.yaml/memory.md 三件套）
- registry 页：docs/detail/registry/astrocs.phase2.reject.md
- SCI：docs/science/REJECTION.md（SCI-REJ-001；descriptor 占位
  SCI-P2-REJ-001⇒SCI-REJ-001 映射声明=ALG §11.5）
- ALG：docs/science/algorithms/PHASE2_REJECTION.md（ALG-P2-REJ-001）；
  DATA：DATA_SEMANTICS §22（DATA-P2-REJ）；API：API-P2-REJ-001
  （PUBLIC_API.md）+ 编排层 API-P2-001（FROZEN）

## 排异档位

按 N = 该输出像素的**几何覆盖帧数**逐像素路由（`1≤N≤3` none（不排异）/ `4≤N≤5` percentile /
`N≥6` winsorized sigma clipping）；档位表与阈值的唯一正本 =
`docs/detail/algorithms_phase2/12_rejection.md` §9 与 `docs/science/REJECTION.md` §5，本页只留指针。
生产排异算法集 = none / percentile / winsorized / linear fit（`linear_fit` 仅显式指定）；**min/max 极值法不用于生产**。实际方法、参数与 N 写入 `rejection` provenance。

