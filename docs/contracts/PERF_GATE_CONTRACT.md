# 性能门判据与监控字段语义合同（CONTRACT-501 / RELEASE-05）

> 上游：ASTROCS_DESIGN.md §9（CPU 后端与资源）、§8.3（调度器）；ENGINEERING_SPEC.md §10
> 依据：GAP_AUDIT G2-1/G2-2（D-10/D-12）

> ID: CONTRACT-501-PERFGATE  状态: FROZEN  机器 schema: `eng/contracts/schemas/perf_gate_criteria.schema.json`、`eng/contracts/schemas/monitor_field_semantics.schema.json`

## 1 L2 冻结判据（四条，全部为**真判红**）

| # | 判据 | 阈值 | 违规 |
|---|---|---|---|
| 1 | 平均 CPU 利用率 | ≥ 0.85 | red |
| 2 | 利用率 p50 | ≥ 0.90 | red |
| 3 | 达标样本占比（利用率 ≥ 0.85 的采样窗比例） | ≥ 0.70 | red |
| 4 | 无「连续 ≥142 s 低利用窗且无积压」 | 无 | red |

- **enforcement = fail-closed**：任一判据违规 ⇒ `verdict=red`；`record_and_justify` **不得**用于掩盖违规（GATE-501 须把 L2 门从 record_and_justify 改为 fail-closed，并用 RELEASE-04 归档违规数据回放证明改前绿、改后红）；
- 判据阈值本身**不得放宽**求绿；如确有硬件/算法上限 ⇒ 给证据化上限并登记 `OPEN_QUESTIONS`，不 waiver。

## 2 测量口径（冻结）

- **采样窗**：固定长度窗口（默认 1 s）内的 CPU 时间占比；`utilization = busy_cpu_seconds / (window_seconds × n_workers)`；
- **测量面**：进程级 `/proc/self/stat`（Linux）/ 等价的进程 CPU 时间（Windows）；不含 I/O 等待；
- **积压定义**：待处理任务队列非空且无 worker 空闲；
- **样本集合**：同一 registry SHA 下的完整运行；跨 SHA 不比较；
- **回放**：历史归档数据必须能被同一检查器重放并给出与当时一致的判定（GATE-501 的 `L2-FROZEN-GATE-REPLAY`）。

## 3 worker_balance 指标（正确算法）

```text
utilization_pct = 100 × mean_over_windows( busy_workers_in_window / n_workers )
```

- **禁止**用「0.5 × 100」式常量或「(min+max)/2」式与负载无关的算法（D-10 实测恒 50.00%）；
- 判据：合成两组不同负载必须给出**不同**输出（GATE-501 的 `WORKER-BALANCE-METRIC-REPLAY` 负例）。

## 4 监控字段语义（二选一落地，冻结为「真强制」）

| 字段 | 语义（冻结） | 执行 |
|---|---|---|
| `requires_monitor` | 声明该检查**必须**有监控证据（CPU/RSS/时长采样） | **真强制**：声明为 true 而监控证据缺失/为空/不可解析 ⇒ **判红**（fail-closed）；不得落 `else PASS` |
| `mutates_workspace` | 声明该检查**会改写工作区**（如生成产物、改配置） | **真语义**：为 true 时检查前后工作区指纹必须**可解释**（声明的 outputs 之外不得有新增/修改）；不得仅「跳过 git 对比」 |

- 8 条 `requires_monitor` 声明逐个核对执行语义，出对照表（GATE-501 交付）；
- 语义若改为纯能力声明，须改名为 `monitor_capable` 并在本文件登记——**本期选择真强制**。

## 5 fail-closed 普查（全部门禁）

每门必须对三种情况判红：**缺失证据** / **坏证据（不可解析、空文件）** / **无输出**；普查表见 `docs/ci/`（GATE-501 交付）。
