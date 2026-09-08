---
id: MOD-astrocs-phase2-integrate
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-P2-INT-001, ALG-P2-INT-001, API-P2-001]
downstream: [TEST-P2-INT-001]
---

# 模块 astrocs.phase2.integrate（P2-INT-DOC 事实修订，2026-09-09）

> P2-INT-DOC（SA-P2-I23）将本页自 gen_module_readmes 派生页事实修订为
> 手写合同页（手写 registry 先例 astrocs.phase2.write.md/
> astrocs.phase2.coverage.md）。frontmatter 的 source_commit/upstream/
> downstream 为 registry 生成词，保持不动；合同权威=lib/phase2_int/
> 三件套 + docs/algorithms/PHASE2_INTEGRATION.md（ALG-P2-INT-001）。
> descriptor 词汇（本节标题 module_id=astrocs.phase2.integrate、端口
> 表、坐标 PIXEL）为编排层占位（module_adapters.cpp:657-675
> p2_integrate_descriptor，注册 :785），由 P2-XX-INT 对齐
> astrocs.p2.integration（MODULE_MIGRATION_MATRIX P2-INT 行），
> 不得反向作为冻结依据。

## 身份与合同落位

- MOD ID：MOD-astrocs-phase2-integrate；module_id 合同值=
  astrocs.p2.integration（矩阵 P2-INT 行）；dll_target=
  astrocs_p2_integration.dll（合同值，尚未存在，P2-INT-IMPL）。
- 合同三件套：lib/phase2_int/（README r1/module.yaml CONTRACT_READY
  entrypoint=MISSING/memory.md；lib/phase2/ 一目录一套已被 P2-COV
  占用，按 lib/hips_p2/ 先例新建）。
- 生产源：lib/phase2/src/integrate.cpp（76 行，根 CMakeLists.txt
  :336-346/:344 astrocs_phase2 静态库成员）+ 唯一权威签名头
  lib/phase2/include/astro/phase2/integrate.h（74 行）。
- 模块页：docs/modules/phase2_int.md。

## 职责与明确非职责

- 职责：逐像素加权积分 reducer——signal=Σwᵢxᵢ/Σwᵢ（仅 eligible ∧
  正权重样本，候选索引固定序）+ support canonical reducer
  （integrate.h:17 "max(accepted support)" 冻结语义）+ 五态显式
  status + p2_validate_candidate_weights 输入预检（:10-17）。
- 非职责：权重策略（外部 numeric weights，构造在 Stage2
  :1106-1140，policy/reducer 分离冻结）、排异/eligibility gather
  （P2-REJ）、马赛克编排/逆归一化（Stage2 域）、内部并行（像素级
  纯函数）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（module_adapters.cpp:659-671，占位词汇，
按 frontmatter registry 生成词保留）:

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `accepted_mask` | `DATA-P2-REJ` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |
| `corrected` | `DATA-P2-COR` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `integrated` | `DATA-P2-INT` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同=DATA-P2-INT（DATA_SEMANTICS §21，P2-INT-DOC
冻结）: 输入 P2PixelStack（values f64 ADU/weights f64 1/ADU² 可空=
等权/support f64 [0,1] 可空=1.0/accepted u8 可空=全接受/count u32）；
输出 P2PixelResult（signal f64 ADU/support f64 [0,1]/五计数器/status
0..4）。invalid 显式化: 非法输入→INVALID_INPUT、无候选→
NO_CANDIDATES、全拒→ALL_REJECTED、全零权重→ZERO_VALID_WEIGHT——
禁止静默 0/±Inf（§4 同源条款；wsum==0 不做除法，integrate.cpp:65-69）。

## 公共 header、核心 symbol 与生命周期

- 唯一权威签名头: lib/phase2/include/astro/phase2/integrate.h
  （P2PixelStack :36-42 / P2IntegrateStatus :45-51 / P2PixelResult
  :53-63 / 函数声明 :58-66）。
- 核心 symbol: p2_integrate_pixel（integrate.cpp:19-74，唯一生产
  入口 C ABI）、p2_validate_candidate_weights（:10-17，调用方
  stage2.cpp:1141/:1402 预检）。
- 编排生命周期 create→validate→run→inspect→destroy 由 session
  承接（module_adapters.cpp:690-695 工厂），本内核为无状态纯函数。
- C API 面: API-P2-INT-001（PUBLIC_API.md，P2-INT-DOC 登记）+
  编排级 API-P2-001（FROZEN）。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase2.integrate`（占位）；execution_class=
`cpu_heavy`；parallel_ok=True（像素间）；配置=phase config JSON
（权重策略在 Stage2 weight_mode，本内核无策略配置——weights 数组
外置）。

## Execution class、并行轴、ThreadBudget lease、确定性

- `cpu_heavy`；并行轴=像素间（调用方 OMP: stage2.cpp:1288/:1298
  schedule(static)、acr_kernels.cpp:218/:228 schedule(static)）；
  像素内候选归约固定序、无跨 worker 浮点重结合 → **结果与 worker
  数无关（1..N bitwise）**；per-thread 统计 thread id 定序归并
  （stage2.cpp:1305-1313）；large_scale 激活强制串行（:1280 条件）。
- worker 数=ThreadBudget.max_workers（禁 hardware_concurrency），
  lease/取消检查点接线=迁移整改点（P2-INT-IMPL，与 DISP-COV-005
  同构）。
- determinism=fixed_reduction_order（module.yaml 合同值）。

## 内存/cache/I-O/所有权

无 I/O（纯函数）；scratch buffer 所有权=调用方（P2PixelStack/
P2PixelResult 调用方分配，integrate.h:36-42/:53-63）；无内部 cache
与全局状态（reentrant=yes）。

## 错误、日志、指标、取消和 checkpoint

- 错误面=五态 status + rc=1（null 栈，:20-21）；无日志/指标输出
  （纯函数）；无内部取消检查点/checkpoint（迁移 ThreadLease 接线
  归 P2-INT-IMPL）。
- known_defects（登记不改码）: DISP-P2INT-001（sup_max 漏计零权重
  accepted 样本，integrate.cpp:54-55 vs integrate.h:17，保守方向，
  bughunt R3-A）、DISP-P2INT-002（INTEGRATION.md:58 vs integrate.h:17
  表述矛盾）——整改归 P2-INT-IMPL/TEST。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-INT-001` MISSING（P2-INT-TEST 建立，不冒认）；
登记面=TEST-P2-INT-DESIGN-001 设计冻结 VERIFIED（ALG-P2-INT-001
§11.4: 常量场 bitwise/零权重惰性/五态穷尽/支撑 max 门/NumPy 参考
rtol 1e-12/并行 1..N 线程 bitwise+ACR↔CPU 等价）。现状相邻证据
（引用不冒认）: lib/phase2/tests/synthetic_gate.cpp Phase2Integrate
组 :4690-4760 + weight policy 门 :2941-2990 + W9 ACR 等价
:3021-3160；tests/backend/test_p2004_reject_integrate.py（P2-004
DRIVER_SRC :18-141）。

## 已知限制

DISP-P2INT-001/002（上节）；support 输出现状为保守方向偏差（不改
覆盖并集保守下界语义）；本页旧派生内容（"错误码=ACS_ERR_*"、
"取消=host cancel 回调" 等 session 层词汇）以本合同页与
DATA_SEMANTICS §21 为准修订。
