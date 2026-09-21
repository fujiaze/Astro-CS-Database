# 插件文档：observability（可观测性）

> 上游：ASTROCS_DESIGN.md §8.1（顶层结构）

## 1. 职责与边界

- **职责**：结构化日志、事件流（JSONL）、运行图、资源监控与诊断，贯穿 CLI 到科学模块。
- **不是**：不改变科学结果；不做 I/O 提交（aio）；不因观测开销影响性能预算。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §7.2（配置、事件与退出码：JSONL 事件流）、§9（CPU 后端与资源：资源记录与重计算资源门）、§10（I/O 与原子产品：run 产物）
- `eng/contracts/schemas/events.schema.json`
- `eng/contracts/resource_gate_v1.json`（G-RES-01 数值唯一源，见 §8）

## 3. 输入/输出数据合同

- **输出**：JSONL 事件流（schema_version/event_id/run_id/kind：progress/resource/artifact/backend/final）、run-graph.json、resource_timeseries.csv、resource_summary.json、worker_balance.csv、日志。
- 参考：`eng/contracts/schemas/events.schema.json`、`run_*.schema.json`。

## 4. 算法与公式要点

- 事件类型统一 schema，跨模块一致；
- stdout 无日志污染（CLI 合同）：日志走文件/JSONL；
- 运行图（run-graph）是实际观测（trace），`plan` 是预期，**禁止把计划值伪装成实际值**；
- 资源监控字段见 `19_runtime.md`（scheduler + pipeline）；与退出码联动（exit 10 资源门禁）。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `log_level` | `info` | —— | trace/debug/info/warn/error |
| `event_dir` | `run/<run_id>/` | —— | 事件输出目录 |
| `sampling` | —— | —— | 资源采样间隔 |

## 6. 接口/ABI

- 公共日志/事件 API（C ABI 版本化），所有模块调用。

## 7. 错误与边界

- 事件写失败 → 降级为文件日志并标记，不静默吞；
- 凭据/密钥不得出现在日志/事件（脱敏）。

## 8. 重计算负载资源门（G-RES-01）

> **本节是 G-RES-01 判据的唯一语义权威**（`ASTROCS_DESIGN.md` §0：具体硬约束与细节写入下级文档）。
> **数值唯一源 = `eng/contracts/resource_gate_v1.json`**；实现侧（C++ / Python 冻结门 / 外挂 judge）不得再出现字面量阈值。
> 维护规则：**改数值只改契约，改语义只改本节**；两侧必须同一次提交内保持一致。

### 8.1 判定域（applicability）

| 条件 | 处置 |
|---|---|
| 有效 CPU 数 < 2 | `NOT_APPLICABLE`（显式分类，**不是豁免**） |
| 计算区间**未严格超过 10 s**（`<= 10.0 s`） | `NOT_APPLICABLE`（显式分类） |
| 有效 CPU 数不可得（affinity ∩ cgroup 均不可得） | **fail-closed → FAIL**（不得以机器总核或配置值冒充） |

判定域的界是**严格大于 10 s**；窗口判据自身的长度界是 **≥ 10 s**（滑窗长度，与判定域区分）。

### 8.2 已分配容量分母（denominator）

利用率是占**已分配容量**的百分比，不是占机器核数的百分比。

| 优先级 | 取值 | 说明 |
|---|---|---|
| ① 主 | `granted_workers`（并发租约授予宽度峰值） | 真实观测/显式声明 |
| ② 回落 | `min(selected_workers, available_cpus)` | 仅当 ① 取哨兵 0（未观测）时启用 |
| ③ 两者皆哨兵 | 分母 = 0 → 利用率类判据不成立 | 记入 `recorded`（`allocated_capacity_undeclared`） |

- 哨兵 0 = 未观测，按未观测处理；
- 判定证据同时回显三分量：`selected_workers` / `available_cpus` / `granted_workers`（外加解析结果 `allocated` 与来源标签 `allocated_source`）。

### 8.3 判据表

| # | 判据 | 阈值 | 执行面 | 违约后果 |
|---|---|---|---|---|
| ① | 单活跃计算线程 | 活跃计算线程统计量 < **2** | **enforce** | FAIL → exit 10 |
| ② | 连续低利用窗 | 任何连续 **≥10 s** 窗利用率 < **60%**，且队列有工作 | **enforce** | FAIL → exit 10 |
| ③ | 无界内存增长 | 分配/RSS 稳健斜率 **≥ 32 MiB·s⁻¹** 且 run 结束回落不可解释 | **enforce**（C++ 分配报告面） | FAIL → exit 10 |
| ④ | 平均利用率 | 计算区间均值 < **85%** | **record_and_justify** | 记录 + 超标登记（不改退出码） |
| ⑤ | 利用率 p50 | 样本中位数 < **90%** | **record_and_justify** | 记录 + 超标登记 |
| ⑥ | 逐样本利用率 | 单样本 ≥ **85%** 的样本占比 < **0.70** | **record_and_justify** | 记录 + 超标登记 |
| ⑦ | 工作量下限 | 线程秒（等效核·秒）< **10** | 事实标记 | 只记录，不参与判定 |

硬失败（enforce）由分母无关的 ①②③ 承担；④⑤⑥ 为分母敏感的统计项，记录并要求超标登记。

**统计量口径（全实现统一）**

- ①的被测量：**并发活跃计算线程数的 p50**；有效样本 < 2 时回落峰值，并在证据里回显所用统计量。C++ 侧采样量为租约活跃宽度（`workers_p50`），外挂监控侧为进程线程数中位数（`threads_p50`）：采样源允许不同，统计量与方向保持一致。
- ②的队列有工作：在低利用窗内，**就绪线程数（`/proc` R 态）中位数 > 已分配容量核数**（就绪线程多于可用槽位即有线程在排队），且同时 ≥ 2。并行宽度不足（如 2 线程在 16 核配额上）归记录项 ④⑤⑥。证据面不可得时沿用无前置判据。
- ③的单位统一为 **MiB·s⁻¹（1048576 B/s）**，方向统一 **≥**。

### 8.4 record / enforce 划分与判定点

- **程序内默认 record_only**：CLI 运行期只记录与报告，不因资源判据改变退出码（`--strict-resource-gate` / `--on-resource-gate strict` 可复现 enforce 语义）。
- **唯一判定点 = CI 重计算检查（`CHK-RESOURCE` 的 `RESOURCE-GATE-REAL` 步骤）+ 发布验收**，以 `run_monitored.py --gate-required --gate-workers <registry 声明>` 形式执行。**`--gate-workers` 必须由 registry 显式声明**；未声明时利用率类判据不成立（记 `allocated_capacity_undeclared`），只有 ① 生效。
- **exit 10（RESOURCE）** 的充分条件：判定域内 ①②③ 任一违约且处于 enforce 面。`NOT_APPLICABLE` 与 record-only 记录项**都不产生 exit 10**。

### 8.5 豁免与不可豁免面

- `NOT_APPLICABLE`（有效 CPU<2 或计算区间 ≤10 s）是**显式分类**，必须在证据里给出 `reason`，不得写成通过。
- 监控/采样证据缺失（无 CPU 样本、`threads_max` 非法、区间中部断流、`effective_cpus` 不可得）**不是**低利用率的豁免，一律 fail-closed 判 FAIL。
- 本门不因任务性质自动豁免：非重计算面**不得请求判定**（`--gate-required` / `--gate-workers` 不出现即不判定）。

### 8.6 实现落点

| 实现 | 落点 | 与本节的关系 |
|---|---|---|
| C++ CLI | `lib/infrastructure/cli/resource_gate.h`、`memory_report.h`（阈值经 CMake 从契约生成的 `resource_gate_thresholds_generated.h` 引入） | 程序内 record_only；① 用 `workers_p50` |
| Python 冻结门 | `eng/tools/monitoring/run_monitored.py::evaluate_frozen_gate` / `resolve_allocated_capacity` | CI 判定点实现；① 用 `threads_p50` |
| 外挂 judge（建议面） | `eng/tools/quality/resource_monitor.py` | 只给建议，不做发布判定；阈值同契约 |

## 9. 测试与 Oracle

- 事件 schema 校验；
- 事件与 run 产物一致性（trace 反映实际）；
- 取消/失败路径事件完整；
- 脱敏测试（无凭据泄漏）；
- G-RES-01：判据边界（10 s 严格界 / ≥10 s 窗）、分母三分量与哨兵、record_and_justify 不改退出码、fail-closed 注入（抹掉样本必翻转）—— 见 `eng/tests/monitoring/test_frozen_gate.py`。
