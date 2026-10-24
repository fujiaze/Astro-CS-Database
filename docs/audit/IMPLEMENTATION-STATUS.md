# ACSD v2 实施与验收状态正本

> 冻结核查时间：2026-10-24；范围：`rebuild/acsd-v2` 的代码/文档提交 [`6f08c4ead6ad454231141f2d4ec4883a140c2705`](https://github.com/fujiaze/Astro-CS-Database/commit/6f08c4ead6ad454231141f2d4ec4883a140c2705)。本文件是**当前进度的唯一导航**，不是新的科学/API/调度合同，也不是本次重新运行测试的声明。后续提交须按实际新证据更新本文件及验证产物；历史审核快照不回写。

## 状态与证据规则

- `DESIGN_REVIEWED`：独立审阅过规范可实施性，不代表源码、数值、产品或性能通过。
- `IMPLEMENTED_NOT_VERIFIED`：可找到代码/接口，但未取得针对当前代码的完整可复现证据。
- `PASS_SCOPED`：具名冻结 case 的全部**实际要求**通过；只认证 case 的有限范围。
- `FAIL`：冻结 case 中至少一项失败；退出码为 0、成功运行多数次、历史曾通过均不能覆盖它。
- `PENDING`：未运行、没有生产实现、缺 Oracle 或当前验收链不完整；不能计入 PASS。
- `HISTORICAL`：有绑定旧 SHA 的有效历史结论；不得自动继承至新实现。
- 所有报告写明源码 SHA、验证代码/夹具 SHA、运行命令、输入/期望 hash、环境、断言、observed 与 comparison。文档/算法修改应当重新判定影响范围；未复跑时保持旧快照身份。
- [REVIEW](REVIEW.md)、[final-review](final-review.md)、[m0-implementation-matrix](m0-implementation-matrix.md)、[review-m0m4](review-m0m4.md)均为**锚定旧提交的审计证据**，不可单独当作本 HEAD 最新裁决。

## 里程碑当前快照

| 范围 | 代码与目标关系 | 验收裁决/边界 | 可核查证据 |
|---|---|---|---|
| 科学与架构设计 | 5 份 foundation、22 份 process、57 个科学 operation、固定基建/typed 目标已写成规范 | **DESIGN_REVIEWED**；不是 57 个 operation 全部完成/全部科学认证 | [设计审查](final-review.md)、[process 清单](../engineering/rebuild/processes/README.md) |
| M0 声明/注册 | `model.hpp`、`runtime/*.hpp`、`modules/*.hpp` 和聚合 `contracts.hpp` 已存在 | **PASS_SCOPED**：`reg01` 等当前记录通过；覆盖检查与复杂负例继续分别核验 | [runtime manifest](../../validation/runtime/manifest.json)、[reg01](../../validation/runtime/reg01/comparison.json) |
| M1 账本/所有权 | 实际 `memory.cpp` 与 Arena/Lease/Scope 代码存在 | **PASS_SCOPED**：`life01`/`work01`/`a01` 通过；旧审核的 metadata、CPU 归因等 P2 不因此自动关闭 | [manifest](../../validation/runtime/manifest.json)、[定向复核](review-m0m4.md) |
| M2 Task/pool/pipe | 单池、Task、Pipe、Host 等已有代码 | **FAIL**：`multi01` 当前 39/41 checks、5 次运行中的一轮不足 100 个完整可行轮；其它已记录的 M2 case 可逐项 PASS_SCOPED | [multi01](../../validation/runtime/multi01/comparison.json)、[manifest](../../validation/runtime/manifest.json) |
| M3 Storage/诊断 | `src/runtime/storage.cpp` 已有 begin/append/seal/abort、staging/metadata claim 和 NaN/-0 处理 | **PASS_SCOPED**：`io01`/`b01`/`storage01`/`d01_off`/`d01_on` 通过；Dyadic 读路径、真正的 staging 峰值、跨平台产品恢复等须另核 | [manifest](../../validation/runtime/manifest.json)、[storage01](../../validation/runtime/storage01/comparison.json) |
| M4 observation.calibrate | 校准唯一内核与 typed Host 路径已接入 | **PASS_SCOPED**：`cal01` 通过；部分故障注入只覆盖 writer 层，完整 adapter 链仍需具名复验 | [cal01](../../validation/runtime/cal01/comparison.json)、[校准证据](../../validation/runtime/calibration/evidence.md) |
| M5 其余科学模块 | 部分 typed-process 已接入，另有只有记录或待接入的模块 | **IMPLEMENTED_NOT_VERIFIED / PASS_SCOPED 混合**；正例不能替代每个 process 全部 operation、探针和资源验收 | [M5 记录](../../validation/process/README.md)、[模块矩阵](m0-implementation-matrix.md) |
| M6 工作流/发布 | WORKFLOWS 已明确 Normalize→Mosaic→Export；历史科学 driver 有部分运行 | **PENDING 完整发布链**；科学旧基线 11 case 中 7 pass、4 pending（W01/P01/R01/X01），它们是带历史身份的结果，非 v2 新链的完整验收 | [历史汇总](../../validation/run_summary.json)、[流程标准](../engineering/rebuild/standards/WORKFLOWS.md) |
| GPU | KernelExecutor/ArrayBacking/Completion 扩展合同 | **PENDING**；首期 CPU，不将模拟 Completion 视为真实 GPU 验收 | [EXECUTION](../engineering/rebuild/EXECUTION.md) |

**Runtime 统计口径**：`validation/runtime/manifest.json` 共 16 个 case，15 pass / 1 fail；其中 `multi01` 失败。manifest 的 `git_commit=04af17cb7...` 是被测 Runtime 代码锚点，manifest 文件本身位于较新的 `6f08c4e` 测试提交中。二者不同并不自动证明篡改，但必须区分**被测代码版本**与**证据提交版本**，且任何其后算法变动都需重跑相关 case。

## 当前优先闭合项（只列有依据的缺口）

| ID | 优先级 | 实质问题与判定方法 | 责任正本 |
|---|---|---|---|
| E-01 | P1 | `multi01` 的完整可行轮数不足，保持 FAIL。分开记录“每轮权重正确性”和“足够样本覆盖”；重复 5 次，所有完整可行轮多重集均为 {1,2,2}，且每次可行轮至少 100 才能关门。不以累计比例/选取最好一次替代 | [SCHEDULER](../engineering/rebuild/standards/SCHEDULER.md)、[测试](../../validation/runtime/multi01/) |
| E-02 | P1（验收缺口） | M5 已接入同步算法的 `std::vector`、矩阵分解/排序/临时大整数是否完整纳入 R/F/实际分配账本；必须用分配跟踪、低 H、真实超限和有界拒绝证明 | [EXECUTION](../engineering/rebuild/EXECUTION.md)、[DEVELOPMENT](../engineering/rebuild/standards/DEVELOPMENT.md) |
| E-03 | P1（端到端缺口） | 无原始 FITS 的 full 产品重新打开、共享随机根、未知噪声、唯一 HiPS signal、WCS Export 与真实文件读写尚未构成当前完整 PASS 证据 | [VERIFICATION](../engineering/rebuild/VERIFICATION.md)、[WORKFLOWS](../engineering/rebuild/standards/WORKFLOWS.md) |
| E-04 | P2 | M5 七个已记录接入模块的具名 probe 仅注册而未全面采样；补生产采样及 DEV_MODE OFF/ON 科学结果等价性 | [PROBES](../engineering/rebuild/standards/PROBES.md)、[M5](../../validation/process/README.md) |
| E-05 | P2（复验） | 旧审核中 spill staging 未计费的 P1 和 -0 散列的 P2 已有针对性源码修订：`storage.cpp` 当前有 staging/metadata claim、NaN-only 规范化。需更新原复现输入、用当前二进制做并发/泄漏/边界复验，**不继续作为未经验证的当前事实指控** | [旧定向报告](review-m0m4.md)、[Storage](../../src/runtime/storage.cpp) |
| E-06 | P2（复验） | `FOUNDATION_API.md` 已明确 `detail::pipe_expect` 的 frontier 规则；需要正式 map/pipe 执行器跨 cohort fixture，不是“接口文档完全缺失” | [FOUNDATION_API](../engineering/rebuild/standards/FOUNDATION_API.md) |
| E-07 | P2（复验） | 旧 M5 的 5-source 欠定 QR `length_error` 复现报告对应旧代码；当前 `linalg.cpp` 已对 `m<n` 具名拒绝。用原 5-source case 重新运行并验证上层给出科学可解释的非成功码 | [M5](../../validation/process/README.md)、[Numerics](../../src/numerics/linalg.cpp) |
| E-08 | P2（量纲 oracle） | 响应积分约 `5.54e20` 的数量级，与平坦 `1 W m^-2 nm^-1` 谱、450–650 nm、单位光子响应按 `λ×10^-9/(hc)` 积分的解析真值吻合；需确认实际 response 标志/输出维度并新增解析比较，不以“数字很大”判为科学错误 | [GAIA_RESPONSE](../science/foundation/GAIA_RESPONSE.md)、[响应模块](../engineering/rebuild/processes/response.integrate.md) |

## 更新纪律与下一步

1. 每完成一个实际修复：保留失败 attempt，提交最小反例及新 PASS 的 `case.json/observed.json/comparison.json`，写清适用范围和相同被测 SHA；不因只修改文档提升验收状态。
2. M5 每个 process 必须提供 typed 正负例、单位/index/数值误差、独立真值、资源上界、取消/低内存/存储重开，以及单独/组合/driver 同路径。特别记录真实 allocator peak 与计划 R/F/metadata，不以 `sizeof(T)` 或经验常数冒充任意 DTO 的真实占用。
3. M6 必须由同一当前科学代码和真实服务完成 Normalize→publish→new-process open→Mosaic→Export，比较独立 Oracle；随后开展真实 HST/M42、Windows/Linux、cold/warm I/O、性能基线，不把历史探针或仅成功计数的 case 作为科学认证。
4. 接口字段与签名的**可编译唯一正本**是 `include/acsd/model.hpp`、`include/acsd/runtime/*.hpp`、`include/acsd/modules/*.hpp`；`contracts.hpp` 只聚合，不再定义全部字段。科学、对象、调度的语义正本仍在本设计树；当文档与源码冲突时，记录偏差并修正规范或实现，禁止静默选择“更方便”的一侧。
