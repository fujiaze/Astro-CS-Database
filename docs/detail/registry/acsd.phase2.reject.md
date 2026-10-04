# 模块 acsd.phase2.reject

> 上游：docs/ACSD_DESIGN.md §8.5（模块与 ABI）、§5.2（固定科学流程）、
> §5.5（逐像素排异：按几何覆盖帧数 N 自动选择算法）
> 科学正本：docs/science/REJECTION.md（SCI-REJ-001；§5 档位条款、§8 空栈语义、
> §14a 上游一手出处）、docs/detail/PHASE2_DETAILED_DESIGN.md §5
> 算法正本：docs/science/algorithms/PHASE2_REJECTION.md（ALG-P2-REJ-001；§3 行号
> 权威、§4.1 语义权威、§7/§11.3 缺陷登记、§11.4 测试设计、§11.5 映射声明）
> 数据正本：docs/science/DATA_SEMANTICS.md §22（DATA-P2-REJ）、
> eng/contracts/schemas/unified/rejection.schema.json（rejection 对象 canonical schema）
> API 正本：docs/engineering/PUBLIC_API.md（API-P2-REJ-001）、
> docs/engineering/PHASE2_API_V1.md（API-P2-001，FROZEN）

LIB 面 = `lib/algorithms/rejection/` 三件套（README / module.yaml ，
CONTRACT_READY）。MOD ID = MOD-acsd-phase2-reject；module_id 合同值 =
`acsd.p2.rejection`（descriptor 词汇 `acsd.phase2.reject` 为编排层口径，其
对齐属迁移目标、未落地）；dll_target = `acsd_p2_rejection.dll`（迁移目标，
未落地）。生产源 = lib/algorithms/coverage/src/rejection.cpp + 签名头正本
lib/algorithms/coverage/include/astro/phase2/rejection.h；构建 = 根 CMakeLists 的
`acsd_phase2` 静态库成员。owner = SA-xxx；depends_on_int = P2-UPM / CPU-005
（P2-REJ 为 P2-INT 行本域被依赖项）；legacy_paths = 「lib/algorithms/coverage
rejection sources」。

## 职责与明确非职责

职责：估计潜在污染状态（cosmic ray、卫星线、坏列、移动源、云/梯度、失焦/拖线），
输出 mask / count / reason / probability / 方法版本；每像素候选栈排异决策。

落地链路：eligibility strided gather 单路径（`source_indices` 权威映射
PHASE2_IVAR_WIRING；compact 后 original slot 只经 `source_indices` 映射）→
planning 层 AUTO 一次解析（生产默认 profile `acsd_adaptive_pixel`，自研）→
**10 显式方法核**（NONE / SIGMA / WINSORIZED / AVERAGED / LINEAR_FIT / ESD / RCR /
PERCENTILE / MEDIAN_SIGMA / MINMAX；**AUTO = 10，永不进 kernel**）→ large_scale
结构生长后处理（trail 扩张只增不减，compact cosmic 不生长，默认关闭）。

**判向冻结**：per-sample reason u8 0..3 与 stack status int 0..7 分离；低于 lower
threshold → `REJECTED_LOW`、高于 upper → `REJECTED_HIGH`，**禁原始值正负号判向**。
**阈值 / 迭代权威** = rejection.cpp 的冻结头注释（本文件为阈值/迭代权威实现，
漂移即违约）；AUTO 路由**只按 N 的几何值**选算法，per-pixel `n_eff` 不参与重选。
**工作域归一** NONE / MEDIAN_CENTER / MEDIAN_SCALE（floor 1e-12，不除零）；mask
应用时回映射到原始 calibrated 值（经 `source_indices`）。

**不是**：不是把异常值变成零；不是 coverage；不合并 / 积分样本（下游）；不做权重
策略（weights 数组外置、构造在 Stage2；RCR 核消费同栈 weights 数组属官方加权
语义，非策略）；不做像素外结构重建（large_scale 仅对已拒 mask 做 8 邻域扩张，只
增不减）；无 session 依赖（无状态纯函数）；不做瞬变 / 卫星语义区分（SCI §1 非
目标）；单帧无排异（n = 1 进 `UNDERDETERMINED` 白名单）。移动源等科学信号可选择
保留到独立层，**不默认当缺陷删除**。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（p2_reject_descriptor，编排层口径）：

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `corrected` | `DATA-P2-COR` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `accepted_mask` | `DATA-P2-REJ` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同 = DATA-P2-REJ（DATA_SEMANTICS §22）：

- 输入 = `P2CandidateStack`（values f64 ADU / weights f64 1/ADU²，可空 = 等权 /
  frame_ids u64 / count u32）+ `P2RejectionPlan`（typed params 六组；AUTO 仅在
  `plan_resolve` 解析；normalization floor 1e-12；`underdetermined_n = 2`）；
- 输出 = `P2RejectionDecision`（reasons u8 0..3 / accepted_count / rejected_low /
  rejected_high / iterations u32 / status int 0..7）+ gather 四诊断计数
  （invalid_finite / invalid_valid / invalid_support / invalid_quality）+
  large_scale 原地 u8 mask（low / high 独立半径）。

invalid 显式化：非 finite 输入 → `INVALID_INPUT`；AUTO 入 kernel → `INVALID_METHOD`；
`PERCENTILE × norm ≠ MEDIAN_CENTER`、`RCR × norm ≠ NONE` → `INVALID_CONFIGURATION`；
空栈 → `MIN_SAMPLES`（**非** `NO_CANDIDATES` —— 后者属积分域
`P2IntegrateStatus`，语义权威 = ALG-P2-REJ-001 §4.1）。

模块注册 = `lib/infrastructure/pipeline/module_ports.registry.json` 的
`acsd.phase2.reject`；生产接线 =
lib/infrastructure/scheduler/src/module_adapters.cpp 的 `p2_op_reject`。输出被
integration 消费（作为门 / 概率）。

### 数值落地口径

阈值使用**预测残差方差**（包含 Phase1 噪声与 UPM 参数不确定度）；**固定全局阈值
属另一口径**。小样本规则、迭代上限与方法版本化的取值以 ALG-P2-REJ-001 与
rejection.cpp 冻结头为准；本页只记落地方式与路由。

**排异算法自动路由：按几何覆盖帧数 N 的档位表**

排异算法**逐像素**、按该像素的**几何可贡献帧数 N** 自动选择（**不是**全局单
算法）。`N` 由 coverage 覆盖图一次解析得出；**整组帧数、掩膜后存活数
（per-pixel `n_eff`）均属另一口径**。

| N（该像素几何可贡献帧数） | 方法 |
|---|---|
| **1 ≤ N ≤ 3** | **none（不排异，直接逆方差加权积分）** |
| **4 ≤ N ≤ 5** | percentile clipping |
| **N ≥ 6** | winsorized sigma clipping |

生产档 `acsd_adaptive_pixel` 的档位表 = 上述三档。**依据（生产 kernel 受控
评估，正本 = docs/science/REJECTION.md §5）**：linear fit 在 `N ≥ 16` 档的等效
上阈显著低于名义 3.5 倍拟合噪声（秩轴拟合的噪声估计被序统计量间距压小），表现为
**干净像素过拒**与**显著点漏检**。对照档 `wbpp_2_9_1` / `acsd_adaptive` 仍为
`N > 15 → linear fit`，作为 WBPP 档界对照基线。`linear_fit` 仍是合法显式方法
（`request = linear_fit`），AUTO 在生产档不产出该档。

生产排异算法集 = none / percentile / winsorized / linear fit；**min/max 极值法不
用于生产**（WBPP 2.5.9 一手源码明文拒绝：`WeightedBatchPreprocessing-engine.js`
的 `rejectionIsGood()`；其算法清单 `StackEngine.rejectionMethods` 亦不含 min/max。
包 sha1 与可核验出处见 docs/science/REJECTION.md §14a）。**`none` 是显式档位，
不是「静默跳过」：必须写 provenance。**

**显式指定的合法性窗口**（同 WBPP 2.5.9 `rejectionIsGood()`，**只告警、不硬
阻断**）：

| 算法 | 合法性窗口 |
|---|---|
| percentile | 仅 ≤ 8 帧 |
| winsorized / linear fit / ESD | 需 ≥ 8 帧（linear fit 建议 ≥ 20；ESD 建议 ≥ 20–25） |
| RCR | 需 ≥ 15 帧 |
| averaged sigma | 仅 8–10 帧 |
| sigma clip | 8–15 帧 |

- 排异字段留空或 `auto` 时按上表逐像素路由；显式指定单一算法时按指定执行；
  指定算法与该 N 的适用域冲突时**报 warn**；方法名不存在或表达式非法**报 error**。
  不合适**只告警、不硬阻断**；算法变更与降级一律显式具名登记；
- **provenance**：实际使用的方法、参数与 N **必须**写入 `rejection` provenance，
  可追溯；
- **NaN 采用样本级掩膜**：污染样本掩除后**重归一**、覆盖级缺数置 NaN 并**强制
  计数**（规则见 docs/science/REJECTION.md）；
- 本表档界与 WBPP 档界的差异及其实验依据（低/中电平下强制 percentile 的有损性）
  见 docs/science/REJECTION.md；算法本身的公式与参数语义同以该文件为唯一正本。

## 公共 header、核心 symbol 与生命周期

签名头正本 = lib/algorithms/coverage/include/astro/phase2/rejection.h（方法枚举 /
reason / status 三处定义段）。

核心 symbol（rejection.h 声明、rejection.cpp 定义）：`p2_reject_plan_resolve`
（AUTO 路由**唯一**解析点）、`p2_eligibility_filter`、
`p2_collect_candidate_stack`（生产 strided gather）、`p2_reject_stack_ex`
（生产入口）、`p2_rejection_semantic_id`、`p2_large_scale_apply`（生产 stage2
唯一调用点）；compat 入口 `p2_reject_stack`（仅测试路径调用，同头文件有冻结注释
「生产 Stage2 不调用」）。

生命周期 = 调用方顺序 `plan_resolve → gather → stack_ex → large_scale`；无
create / destroy，无状态纯函数（reentrant）。

## Registry descriptor 与配置 schema

module_id=`acsd.phase2.reject`（占位）；execution_class=`cpu_heavy`;
parallel_ok=True。配置面 = lib/algorithms/coverage/include/astro/phase2/stage2_common.h
的 `reject_method` / `reject_profile` / `reject_underdetermined_n` /
`reject_normalization`（+ floor 1e-12）/ `large_scale_*`；typed params 的唯一默认
源 = cfg（stage2.cpp）。

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `rejection_classes` | 全类 | —— | 启用的排异类 |
| `sigma_gate` | —— | σ | 预测残差阈值（用预测残差方差，不用固定全局阈值） |
| `max_iter` | —— | 次 | 迭代上限 |
| `keep_moving_sources` | true | —— | 移动源独立层保留（不默认当缺陷删除） |

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`。并行轴 = 像素间（**调用方** OMP：现存调用方 stage2.cpp 用
schedule(static)；rejection.cpp 无任何线程原语）。原并列调用方 acr_kernels.cpp
已随 ACR 子树退场删除（`383088f2`）。逐样本独立判定、无跨样本归约 ⇒ **结果与
worker 数无关（1..N bitwise）**。

per-thread 统计按 thread id 定序归并（stage2.cpp）；large_scale 激活时强制串行
（stage2.cpp 的分支条件）。确定性合同：同输入同 plan 同 fid → decision bitwise；
ESD tie-break = frame_id（1e-15 epsilon）、linear_fit 排序按
`(value, orig_index)` 字典序。

worker 数 = ThreadBudget.max_workers（禁 hardware_concurrency）；lease / 取消
检查点接线属迁移整改点（与 coverage 域同构，见 ALG-COV-001 §11.3；未落地）。
determinism = `fixed_reduction_order`。

## 内存/cache/I-O/所有权

无文件 I/O（纯函数）。kernel 内 n ≤ 64 走固定 scratch、> 64 走堆（rejection.h）。
无内部 cache 与全局状态（reentrant = yes）。

所有权 = 调用方分配 scratch 与缓冲（reasons 缓冲等由调用方提供；
`P2CandidateStack` / `P2RejectionDecision` 由调用方持有）。

## 错误、日志、指标、取消和 checkpoint

错误面 = 八态 status（0..7）+ rc = 1（null / 非法参数；`plan_resolve` null / 出界
/ 非法 profile；large_scale 参数非法）。**rc = 0 时语义全由 status 承载**（科学
状态而非调用错误）。调用方合同：status ∈ {OK, UNDERDETERMINED} 才可继续积分
（stage2.cpp 冻结门）。

- 预测残差方差缺失 → fail-closed（不能用猜测阈值）；
- 小样本 → 明确规则（不静默删）；
- 方法版本必须记录（改变排异策略即改变版本）；
- 无日志 / 指标输出（纯函数；编排层日志在 stage2.cpp）；无内部取消检查点 /
  checkpoint；
- 错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-REJ-001` 待建（不冒认）；设计冻结 = `TEST-P2-REJ-DESIGN-001`
（ALG-P2-REJ-001 §11.4 F1–F8：F1 ESD NIST Rosner 54 值拒集 bitwise、F2 AUTO 路由
枚举精确、F3 small-N 状态穷尽、F4 卫星注入 mask 精确、F5 置换不变性 decision
bitwise、F6 typed params 逐位、F7 Python oracle rtol 1e-12、F8 gather 逐元素精确；
F1–F6/F8 无 epsilon 门、F7 rtol 1e-12、large_scale mask 精确）。

已取证但载体不在仓内的相邻结论（不冒认）：R1/R2/LinearFit/Rcr/G4 + G6（ESD NIST Rosner 54）
+ V15–V17 的合成门读数；生产 Oracle 的排异-集成读数；语义 id 与解析面的单测读数。
这些读数不在本仓可复算路径上，引用时只作背景。

Oracle 面：

- 注入各类污染（cosmic ray / 卫星线 / 坏列 / 移动源 / 云）→ 检测与分类符合；
- 预测残差阈值正确性（含 UPM 不确定度）；
- 移动源保留到独立层验证；
- 小样本规则与迭代上限；
- **判据能红能绿**：注入卫星线 / 宇宙线必被剔除；无污染样本按真实信号保留；
  1 worker 与 N worker 结果一致。

## 已知限制

- 缺陷登记（不改码，正本 = ALG-P2-REJ-001 §7/§11.3）：rejection.h 的 percentile
  `low_fraction` 注释「默认 0.1」与实现 / SCI 权威 0.2 漂移；SCI §8「空栈 →
  NO_CANDIDATES」与实现 MIN_SAMPLES 的口径差（NO_CANDIDATES 属积分域，语义
  权威 = ALG §4.1）；SCI §2/§5 行号锚漂移（行号权威 = ALG §3 实测）；minmax
  比较器 value-only tie-break 未显式冻结；整改面未落地；
- 目标交付形态 acsd_p2_rejection.dll 未落地；descriptor 占位 module_id 与合同
  值 `acsd.p2.rejection` 的对齐属迁移目标（未落地）；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
