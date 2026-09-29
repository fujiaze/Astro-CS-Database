# 模块 astrocs.phase2.reject

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同：LIB 面 = lib/algorithms/rejection/ 三件套（CONTRACT_READY）；
> 科学/算法正本 = docs/science/algorithms/PHASE2_REJECTION.md（ALG-P2-REJ-001）；
> 数据合同 = DATA-P2-REJ（DATA_SEMANTICS §22）；C API = API-P2-REJ-001（PUBLIC_API）
> + 编排级 API-P2-001（docs/engineering/PHASE2_API_V1.md，FROZEN）。descriptor 词汇
> （module_id=astrocs.phase2.reject、端口表、坐标 PIXEL）为编排层口径，
> 冻结依据 = ALG-P2-REJ-001。

## 身份与合同落位

- MOD ID：MOD-astrocs-phase2-reject；module_id 合同值 = astrocs.p2.rejection；
  dll_target = astrocs_p2_rejection.dll（迁移目标，未落地）。
- 合同三件套：lib/algorithms/rejection/（CONTRACT_READY；落位规则见
  docs/detail/README.md）。
- 生产源：lib/algorithms/coverage/src/rejection.cpp + 签名头正本
  lib/algorithms/coverage/include/astro/phase2/rejection.h；构建 = 根 CMakeLists.txt
  的 astrocs_phase2 静态库成员。
- 模块页：docs/detail/phase2_rej.md。

## 职责与明确非职责

- 职责：eligibility strided gather（source_indices 权威映射
  PHASE2_IVAR_WIRING，rejection.h:252-255）→ planning AUTO 一次解析
  （生产默认 profile astrocs_adaptive_pixel（自研），档位表与阈值唯一正本 =
  `docs/detail/algorithms_phase2/12_rejection.md` §9 / `docs/science/REJECTION.md` §5）
  → 10 显式方法核（per-sample reason
  u8 0..3 与 stack status int 0..7 分离；判向=低于 lower→
  REJECTED_LOW/高于 upper→REJECTED_HIGH，禁原始值正负号判向，
  rejection.h:20-21）→ large_scale 结构生长后处理（trail 扩张只增
  不减，cosmic 不生长，默认关闭）。阈值/迭代权威锚定
  rejection.cpp:1-12 冻结头注释。
- 非职责：不合并/积分（下游）；权重策略（weights 外置
  Stage2；RCR 消费同栈 weights 属官方加权语义）；像素外
  结构重建；session 依赖（无状态纯函数）；瞬变/卫星语义区分
  （SCI §1）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（p2_reject_descriptor，编排层口径）:

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `corrected` | `DATA-P2-COR` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `accepted_mask` | `DATA-P2-REJ` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同=DATA-P2-REJ（DATA_SEMANTICS §22）: 输入
P2CandidateStack（values f64 ADU/weights f64 1/ADU² 可空=等权/
frame_ids u64/count u32）+ P2RejectionPlan（typed params 六组/AUTO
仅 plan_resolve/norm floor 1e-12/underdetermined_n=2）；输出
P2RejectionDecision（reasons u8 0..3/accepted_count/rejected_low/
rejected_high/iterations u32/status int 0..7）+ gather 四诊断计数 +
large_scale 原地 u8 mask。invalid: 非 finite→INVALID_INPUT、AUTO 入
kernel→INVALID_METHOD、PERCENTILE×norm≠MEDIAN_CENTER/RCR×norm≠NONE→
INVALID_CONFIGURATION、空栈→MIN_SAMPLES（非 NO_CANDIDATES，缺陷登记 =
PHASE2_REJECTION.md §11.3）。

## 公共 header、核心 symbol 与生命周期

- 签名头正本: lib/algorithms/coverage/include/astro/phase2/rejection.h
  （329 行；reason :71-77/status :79-90）。
- 核心 symbol: p2_reject_plan_resolve（:191）、p2_eligibility_filter
  （:222）、p2_collect_candidate_stack（:263）、p2_reject_stack_ex
  （:287，生产入口）、p2_rejection_semantic_id（:196）、
  p2_large_scale_apply（:295）；compat 入口 p2_reject_stack（:325，仅测试调用；
  :299 冻结注释声明生产 Stage2 不再调用）。
- 生命周期=调用方顺序 plan_resolve→gather→stack_ex→large_scale；
  无 create/destroy，无状态纯函数。C API 面: API-P2-REJ-001
  （PUBLIC_API.md）+ 编排级 API-P2-001（FROZEN）。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase2.reject`（占位）；execution_class=
`cpu_heavy`；parallel_ok=True（descriptor 登记）。
配置=stage2_common.h reject_method/reject_profile/
reject_underdetermined_n/reject_normalization(+floor 1e-12)/
large_scale_*（:52-63）；typed params 唯一默认源=cfg
（stage2.cpp:698-727）。

## Execution class、并行轴、ThreadBudget lease、确定性

- `cpu_heavy`；并行轴=像素间（调用方 OMP: stage2.cpp:1288/:1298
  schedule(static)、acr_kernels.cpp:218/:228）；逐样本独立判定无
  跨样本归约 → **结果与 worker 数无关（1..N bitwise）**；per-thread
  统计 thread id 定序归并（stage2.cpp:1305-1313）；large_scale 激活
  强制串行（:1280）。ESD tie-break frame_id 1e-15、linear_fit 排序
  (value,orig_index)。
- worker 数=ThreadBudget.max_workers（禁 hardware_concurrency）；
  lease/取消检查点接线属迁移整改面（未落地）。determinism=fixed_reduction_order。

## 内存、cache、I-O、所有权

无文件 I/O（纯函数）；kernel 内 n≤64 固定 scratch、>64 堆
（rejection.h:404）；scratch 所有权=调用方分配（reasons 缓冲调用方
提供）；无内部 cache 与全局状态（reentrant=yes）。

## 错误、日志、指标、取消和 checkpoint

- 错误面=八态 status int 0..7 + rc=1（null/非法参数；large_scale
  参数非法 rc=1）；rc=0 时语义全由 status 承载；调用方门=status ∈
  {OK,UNDERDETERMINED} 才可继续积分（stage2.cpp:1189-1195 冻结门）。
  无日志/指标输出（纯函数；编排层日志 stage2.cpp:730-738）；无内部
  取消检查点（迁移 ThreadLease 接线属迁移目标，未落地）。
- 已知缺陷（登记不改码，正本 = PHASE2_REJECTION.md §11.3）：rejection.h:118 的
  percentile 注释漂移、空栈 NO_CANDIDATES 归属积分域、行号锚漂移（权威 = ALG §3）、
  minmax value-only tie-break 未显式冻结；整改面未落地。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-REJ-001` 待建；设计冻结 = TEST-P2-REJ-DESIGN-001（ALG-P2-REJ-001
§11.4 + 本节；F1-F8: ESD NIST Rosner 54 值拒集 bitwise/AUTO 路由
枚举精确/small-N 状态穷尽/卫星注入 mask 精确/置换不变性 decision
bitwise/typed params 逐位/Python oracle rtol 1e-12/gather 逐元素
精确；F1-F6/F8 无 epsilon 门、large_scale mask 精确）。现状相邻
证据（引用不冒认）: lib/algorithms/coverage/tests/synthetic_gate.cpp R1/R2/
LinearFit/Rcr/G4（:2639-2725）+ G6（:2779-2863）+ 并行/ACR 等价组
（:4138-4864）；eng/tests/backend/test_p2004_reject_integrate.py（生产 Oracle）；
eng/tests/unit/p2_rejection_test.cpp。

## 已知限制

现行语义与判据正本 = `docs/science/algorithms/PHASE2_REJECTION.md` §11.3
（本页只留指针）。

## 排异档位

按 N = 该输出像素的**几何覆盖帧数**逐像素路由。档位表与阈值的唯一正本 =
`docs/detail/algorithms_phase2/12_rejection.md` §9（科学条款 = `docs/science/REJECTION.md` §5），
本页只留指针。生产排异算法集 = none / percentile / winsorized / linear fit
（`linear_fit` 仅显式指定）；**min/max 极值法不用于生产**。
