# 性能门判据与监控字段语义合同

> 上游：docs/ASTROCS_DESIGN.md §9（CPU 后端与资源）、§8.3（调度器）；docs/ASTROCS_DESIGN.md §9（CPU 后端与资源）

机器 schema：`eng/contracts/schemas/perf_gate_criteria.schema.json`、`eng/contracts/schemas/monitor_field_semantics.schema.json`；判据阈值正本 = `docs/engineering/CI_SPEC.md` §9.2，阈值唯一数值源 = `eng/contracts/resource_gate_v1.json`。

## 1 L2 冻结判据（四条，全部为**真判红**）

| # | 判据 | 阈值 | 违规 |
|---|---|---|---|
| 1 | 平均 CPU 利用率 | ≥ 0.85 | red |
| 2 | 利用率 p50 | ≥ 0.90 | red |
| 3 | 达标样本占比（利用率 ≥ 0.85 的采样窗比例） | ≥ 0.70 | red |
| 4 | 无「连续 ≥10 s 且利用率 <60% 的低利用窗」（无就绪积压同样计违规） | 无 | red |

- **enforcement = fail-closed**：任一判据违规 ⇒ `verdict=red`；`record_and_justify` 只是无违规样本的**记录语义**，不参与裁决；
- 判据阈值本身**保持事前冻结值**（唯一数值源 = `eng/contracts/resource_gate_v1.json::compute`）；硬件/算法上限只作证据化上限随判据登记，不 waiver。
- **定义与适用域（自洽说明）**：`utilization = busy_cpu_seconds / (window_seconds × n_workers)`；适用域 = 生产重计算面、`effective_cpus ≥ 2` 且采样区间 > 10 s；**验证方式** = 已归档运行证据回放（`实验/engineering-evidence/l2_performance/gates/`）与红绿双向自测（`eng/ci/check_frozen_gate.py --self-test`）。

## 2 测量口径（冻结）

- **采样窗**：固定长度窗口（默认 1 s）内的 CPU 时间占比；`utilization = busy_cpu_seconds / (window_seconds × n_workers)`；
- **测量面**：进程级 `/proc/self/stat`（Linux）/ 等价的进程 CPU 时间（Windows）；不含 I/O 等待；
- **积压定义**：待处理任务队列非空且无 worker 空闲；
- **样本集合**：同一 registry SHA 下的完整运行；跨 SHA 不比较；
- **回放**：已归档运行数据必须能被同一检查器重放并给出与当时一致的判定（`L2-FROZEN-GATE-REPLAY`）。

## 3 worker_balance 指标（正确算法）

```text
utilization_pct = 100 × mean_over_windows( busy_workers_in_window / n_workers )
```

- 判据算法必须与负载相关：「0.5 × 100」式常量与「(min+max)/2」式恒值算法一律判红；
- 判据：合成两组不同负载必须给出**不同**输出（`WORKER-BALANCE-METRIC-REPLAY` 负例）。

## 4 监控字段语义（二选一落地，冻结为「真强制」）

| 字段 | 语义（冻结） | 执行 |
|---|---|---|
| `requires_monitor` | 声明该检查**必须**有监控证据（CPU/RSS/时长采样） | **真强制**：声明为 true 而监控证据缺失/为空/不可解析 ⇒ **判红**（fail-closed）；判定只走具名分支（`else PASS` 属未登记形态） |
| `mutates_workspace` | 声明该检查**会改写工作区**（如生成产物、改配置） | **真语义**：为 true 时检查前后工作区指纹必须**可解释**（改动面 = 声明的 outputs）；指纹对比是唯一判据（「跳过 git 对比」属未登记形态） |

- `requires_monitor` 声明逐个核对执行语义，对照表见 `docs/engineering/03_GATES.md` §6.2；
- 语义是**真强制**，不改名为 `monitor_capable`；改名属合同变更，走变更流程。

## 5 fail-closed 普查（全部门禁）

每门必须对三种情况判红：**缺失证据** / **坏证据（不可解析、空文件）** / **无输出**；普查表见 `实验/engineering-evidence/`。
