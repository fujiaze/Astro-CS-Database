# 迁移策略、自动检查点与回退规则

## 1. 原则

采用 strangler/adaptor 迁移：先建立新合同与 Runtime 骨架，再逐模块把旧实现包进标准接口；在同一小型合成输入上做新旧调用路径的临时 characterization 对照。新路径通过独立科学 Oracle 后，切换 canonical Pipeline；最后删除旧生产入口。

characterization 只用于发现重构漂移，不是科学验收，也不需要历史仓库或 32R。禁止把旧错误冻结成 golden truth。

## 2. 自动 Gate

| Gate | 内容 | 通过条件 | 失败后动作 | 是否等人 |
|---|---|---|---|---|
| G0 | 起点与现状冻结 | main 三 SHA 一致；控制包 PASS；P0 清单、构建图、调用图现场生成 | 修复证据，不改科学代码 | 否 |
| G1 | 合同真相层 | SCI/ALG/DATA/ARCH/API/MOD/TEST 索引无悬空；歧义显式登记 | 先修文档或标 SCIENCE_CONFLICT | 仅科学冲突 |
| G2 | Runtime 基座 | Pipeline IR、Artifact、Registry、RunContext、错误/取消/日志测试通过 | 回退本任务 commit，保留已通过任务 | 否 |
| G3 | I/O 与 CPU 基座 | RAII/LSan、CFITSIO reentrant、CPU provider/benchmark/profile 合同通过 | 禁止迁科学模块 | 否 |
| G4 | Phase1 迁移 | 全节点 trace、合成 Oracle、资源门禁、文档追溯通过 | 仅在 Phase1 范围修复 | 否 |
| G5 | Phase2 迁移 | 无 workers=1/global read lock；UPM 接缝、rejection、integration Oracle 通过 | 仅在 Phase2 范围修复 | 否 |
| G6 | Phase3 开发 | SCI/ALG 先行；并行重采样、WCS/单位/coverage Oracle 通过 | 不得标已实现 | 仅科学冲突 |
| G7 | 旧路径退出 | canonical run 不链接/调用旧 scheduler/ACR；CLI 薄化 | 继续适配，不提前删除可恢复代码 | 否 |
| G8 | Linux 收敛 | GCC/Clang 轻量构建、静态/合成/sanitizer、2 核资源验证 | 修复后重跑受影响项 | 否 |
| G9 | Windows 收敛 | MSVC、backend benchmark、合成、小真实、32R 一次全链 | Windows 离线则继续其他工作 | 仅最终节点缺失 |
| G10 | 发布审计 | L0 文档、包、SHA、commit 单一、所有硬门通过 | 生成 NOT_READY，不伪造 PASS | 最终视觉审核 |

Gate 状态只能由 `05_TASK_LEDGER.csv` 的任务状态计算。不得手写总数，不得通过删除 dependency 让 Gate 变绿。

## 3. 任务状态机

允许：

`NOT_STARTED → IN_PROGRESS → PASS`

或：

`IN_PROGRESS → FAIL → IN_PROGRESS → PASS`

以及远程资源暂不可用时：

`NOT_STARTED/IN_PROGRESS → WAITING_WINDOWS → IN_PROGRESS`

`BLOCKED` 只用于真实阻塞。每次状态变更记录 UTC、commit、证据路径和原因。禁止从 FAIL 直接改 PASS 而没有新证据。

## 4. 每个任务的固定执行模板

1. 复核 `HEAD == main == origin/main`，记录起点。
2. 将 Ledger 任务设为 `IN_PROGRESS`。
3. 读取该任务列出的合同、文件、符号和依赖证据。
4. 修改最小范围；不得顺带处理下一任务。
5. 运行 task-specific test；需要重计算时同时运行 resource monitor。
6. 更新该任务要求的 L1/L2 文档与机器索引。
7. 运行影响分析，执行自动计算出的受影响测试。
8. 生成 `evidence/refactor/tasks/<TASK_ID>/TASK_RESULT.json`。
9. 验证 evidence schema；状态改 PASS。
10. 一个 commit，提交信息前缀 `<TASK_ID>:`；push main。
11. 再次记录 `HEAD == origin/main`。

如任务需两类不可混合修改，必须拆为 Ledger 中已有的多个任务；禁止自行把它们塞进一个 commit。

## 5. 回退

- 不使用 `reset --hard`、force push 或重写历史。
- 尚未 commit 的失败修改：只恢复本任务明确列出的文件；若与外部改动重叠，停止并报告冲突。
- 已 push 的错误提交：用独立 `revert` commit，附失败证据；随后重新实施。
- 新 Runtime 在迁移中可通过 feature flag 选择旧 adapter，但 flag 必须在 G7 删除；发布包不得有两套不可判定的生产模式。
- 数据格式迁移必须提供只读兼容 adapter；不得原地破坏用户产物。

## 6. 科学变更隔离

架构迁移中发现公式或单位错误时：

1. 记录 `SCIENCE_FINDING`，给出当前 SCI、代码、合成反例和影响；
2. 保持架构任务只迁移现有已冻结语义；
3. 新建独立 SCI→ALG→DATA/API→implementation→TEST 变更任务；
4. 只有负责人已经冻结了唯一科学结论时才执行；
5. release notes 明确科学变化和重新验证范围。

禁止“顺手修公式后用重构误差解释”。

## 7. 性能修改隔离

- 先生成热点/资源证据，再做性能 commit；
- 同一 commit 不同时改算法和并行；
- 性能修改先过 bit/数值合同，再看速度；
- benchmark 重复只用于短基准统计，不得扩展为反复全量历史回归；
- 不追求每个轻量函数 SIMD 化，只有占据可测热点的 kernel 才进入 provider。

## 8. 最终一次真实运行

最终 Windows 候选 commit 冻结后，才运行银心三板块全部 32R：

- 输入清单和 SHA 与冻结 manifest 一致；
- 同一候选只要求一次成功全链；基础设施失败可在修复后重跑，但必须保留失败记录；
- 全程 resource monitor；
- Phase1 每帧 HiPS、Phase2 mosaic、support/weight/UPM/rejection diagnostics 完整；
- Phase3 从最终 HiPS 生成平面 FITS；
- HiPS 浏览器使用锁定坐标、FOV、stretch 展示拼接接缝、黑洞、条纹和大尺度背景；
- 不再并行跑旧历史版本；数值问题用对应合成 fixture 定位。

## 9. 任务图变更规则

`05_TASK_LEDGER.csv` 的 ID、依赖和验收语义在执行期不可修改。若发现控制包自身错误：

- 生成 `CONTROL_ERRATA.md`；
- 说明旧值、新值、原因、影响任务和负责人批准；
- 控制包版本从 V6.0 增至 V6.1；
- 校验新旧 hash；
- 禁止仅为了解除 blocker 删除依赖。
