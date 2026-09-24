# 插件文档：observability（可观测性）

> 上游：ASTROCS_DESIGN.md §7.3（错误传播与运行日志）、§8.1（顶层结构）

## 1. 职责与边界

- **职责**：结构化日志与**运行日志落盘**、事件流（JSONL）、运行图、资源监控与诊断，贯穿 CLI 到科学模块。
- **不是**：不改变科学结果；不做 I/O 提交（`aio` 是唯一 I/O 边界，本模块经它写日志）；不判定退出码（退出码收敛在 CLI）；不因观测开销影响性能预算。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §7.2（配置、事件与退出码：JSONL 事件流）、§7.3（错误传播与运行日志）、§9（CPU 后端与资源：资源记录与重计算资源门）、§10（I/O 与原子产品：run 产物）
- `docs/design/LOG_AND_ERROR_SYSTEM.md`（日志与错误系统详细设计）、`docs/contracts/LOG_AND_ERROR_CONTRACT.md`（日志行/落点/降级/退出码映射合同）
- `eng/contracts/schemas/events.schema.json`
- `eng/contracts/resource_gate_v1.json`（G-RES-01 数值唯一源，见 §8）

## 3. 输入/输出数据合同

- **输出**：JSONL 事件流（schema_version/event_id/run_id/kind：progress/resource/artifact/backend/final）、run-graph.json、resource_timeseries.csv、resource_summary.json、worker_balance.csv；
- **运行日志工件**：`<output_dir>/logs/run_<run_id>.jsonl`（机器，行格式 = `astrocs.log.event.v1`）与 `<output_dir>/logs/run_<run_id>.log`（人可读摘要，与 JSONL 同源）；两工件在 run manifest 的 `log_artifacts[]` 登记（字段表见 `docs/contracts/LOG_AND_ERROR_CONTRACT.md` §4）。
- 参考：`eng/contracts/schemas/events.schema.json`、`run_*.schema.json`、`lib/infrastructure/observability/logging/log_event_v1.schema.json`。

## 4. 算法与公式要点

- 事件类型统一 schema，跨模块一致；
- stdout 无日志污染（CLI 合同）：日志走文件/JSONL；
- 运行图（run-graph）是实际观测（trace），`plan` 是预期，**禁止把计划值伪装成实际值**；
- 资源监控字段见 `19_runtime.md`（scheduler + pipeline）；与退出码联动（exit 10 资源门禁）。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `log_level` | `info` | —— | debug/info/warn/error |
| `log_dir` | `<output_dir>/logs` | —— | 运行日志目录；由块级 `output_dir` 派生，禁落 CWD/源码树/`run/` |
| `log_keep` | `all` | —— | 运行日志保留策略（`all` = 全保留，或最近 N 次） |
| `sampling` | —— | —— | 资源采样间隔 |

## 6. 接口/ABI

- 公共日志/事件 API（C ABI 版本化），所有模块调用。

## 7. 错误与边界

- 日志/事件写入失败 → 记 stderr 脱敏摘要 + 本次运行以非 0 退出码结束（IO=7；磁盘满=10）；**禁止**"换一路日志继续跑"式的静默吞错；
- 日志目录解析失败 → exit 2（ARGS）；目录创建失败 → exit 7（IO）；收尾 fsync 失败 → exit 7；哈希或 manifest 登记失败 → exit 8（INTEGRITY）；
- 降级必须显式：写 `degraded_reason` 并入 manifest；静默回退到低优先输入/静默保持缺省值/静默跳过校验都按故障上行（`docs/contracts/LOG_AND_ERROR_CONTRACT.md` §6）；
- 凭据/密钥与绝对用户路径不得出现在日志/事件/诊断（脱敏规则唯一源 = LOG-001 §5）。

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

- **程序内恒 record_only**（`ASTROCS_DESIGN.md` §3.5「资源门只管磁盘…内存、CPU、线程不设门」；§9.74 裁决 10）：CLI 运行期只记录与报告，**不因资源判据改变退出码**；`--strict-resource-gate` / `--on-resource-gate strict` **保留接受但不改变裁决**（旗标请求事实由事件字段如实登记，见下「事件面登记」）。实现唯一收口 = `lib/infrastructure/cli/resource_gate.h::gate_enforcement`（恒返回 `RecordOnly`；`Enforced` 枚举值仅为既有 ABI/测试引用保留）。
- **唯一判定点 = CI 重计算检查（`CHK-RESOURCE` 的 `RESOURCE-GATE-REAL` 步骤）+ 发布验收**，以 `run_monitored.py --gate-required --gate-workers <registry 声明>` 形式执行。**`--gate-workers` 必须由 registry 显式声明**；未声明时利用率类判据不成立（记 `allocated_capacity_undeclared`），只有 ① 生效。
- **exit 10（RESOURCE）** 的充分条件：判定域内 ①②③ 任一违约且处于 enforce 面——该路径**只存在于 CI 判定点**，程序内（CLI）无此路径。`NOT_APPLICABLE` 与 record-only 记录项**都不产生 exit 10**。

**事件面登记（`resource` / `resource_gate`）**

`resource_gate`（severity=warning，仅判定为违规时发出）的**冻结必含扩展字段**（五面一致；机器门 = `eng/ci/check_event_field_sets.py`，正本 = `lib/infrastructure/cli/protocol.h::missing_required_extension_v1`）：

| 字段 | 承载事实 |
|---|---|
| `diag` / `enforcement` / `enforced` | 诊断分类 / 处置口径（恒 `record_only`）/ 是否已强制（恒 `false`） |
| `strict` | 用户是否请求了 strict 旗标（`--strict-resource-gate` / `--on-resource-gate strict`）；**不改变裁决** |
| `work_core_seconds` / `workload_floor_core_seconds` / `workload_floor_reached` | 工作量下限事实（判据 ⑦，只记录，不参与判定） |
| `so05_signoff_id` / `so05_signoff_status` | SO-05 签字项身份与状态（`DATA_SEMANTICS.md` §31.10：恒 `SO-05` / `PENDING_OWNER_SIGNOFF`） |
| `auto_adjudication_allowed` | 自动裁决是否放行（恒 `false`） |

**不得**在 `resource_gate` 事件写「若已签字是否会失败」类字段：§8.3 的 ④⑤⑥ 是 `record_and_justify`（违约后果 = 记录 + 超标登记，**不改退出码**），事件级常量无法表达逐判据的 enforce/record 分类，写了就是错的。

`resource`（severity=info，每阶段末发出）的必含扩展字段 = CLI-004 冻结五项（`cpu_cores_used` / `rss_bytes` / `io_read_bytes` / `io_write_bytes` / `threads`）；其余（含 SO-05 记录字段 `measurement_policy` / `so05_signoff_id` / `so05_signoff_status` / `auto_adjudication_allowed` / `auto_adjudication_policy` / `one_budget_source_rule` / `determinism_contract`，以及 `strict_flag_requested`）是**实现侧附加字段**，不属冻结必含集（口径同 `CLI_PROTOCOL_V1.md` §4 对 `artifact` 的 DET-001 附加字段）。

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

### 8.7 节点级归因探针与两个 I/O 口径（PERF-501）

G-RES-01 的采样面是**进程级**的：它能判「利用率低」，不能回答「哪个节点、几条线程」。
补上归因面的三个 env-gated 观测开关（默认零输出零开销，只观测、不改调度与数值路径）：

| 开关 | 输出行 | 落点 |
|---|---|---|
| `ASTROCS_NODE_TRACE=1` | `[nodetrace] BEGIN <node> <steady_s>` / `END <node> <elapsed_s>` | `module_adapters.cpp` P10-UTIL2-006 |
| `ASTROCS_LEASE_TRACE=1` | `[lease] <node> host_workers=… acquired=… cap=… budget_available=…` | `module_adapters.cpp` P7-UTIL-001 |
| `ASTROCS_P1CAP_TRACE=1` | `[p1cap] frame_workers/parallel_for node=… lease=… memory_cap=… frame_workers=… n_units=… inner_omp=…` | `module_adapters.cpp` PERF-501 |

消费者 = `eng/tools/monitoring/node_waterfall.py`：把上述行与 `resource_timeseries.csv`
（或外挂 `resource_monitor.py` 的 `samples.csv`）按时间轴对齐，输出**节点瀑布 + 逐节点并行宽度**
（`--self-test` 自证解析与归因口径）。

**两个 I/O 口径不可混用**（本任务实测澄清）：

- `resource_timeseries.csv` 的 `io_wait_pct` / `resource_summary.json` 的 `io_wait_pct_mean`
  是 **`/proc/stat` 首行第 5 个数值（系统级 iowait，jiffies）÷ 采样区间 × 100**
  （`lib/infrastructure/cli/monitor.h:189-210` 取值、`resource_recorder.h:158-159` 归一），
  单位与 `cpu_pct` 同刻度 = **等效核 × 100**，且**含其它进程**的 I/O 等待。
  ⇒ 它回答「盘上有没有负载」，**不**回答「是不是 Astro Celestial Sphere Database（ACSD） 在等盘」。
- `resource_monitor.py` 新增的 `io_wait_all_ms` = **Σ 全线程 `delayacct_blkio_ticks` 增量**
  （`/proc/<pid>/task/<tid>/stat` 第 42 字段），是**本进程树**真实块 I/O 等待；旧字段
  `io_wait_ms` 只取线程组组长（`/proc/<pid>/stat`），在帧级并行下会系统性低估。

读字节同理有两档，报告时必须写明用哪档：`/proc/<pid>/io` 的 `read_bytes` = 物理盘读；
`rchar` = 含 page cache 命中的逻辑读（`run_monitored.py --io` 默认取 `rchar`）。

## 9. 测试与 Oracle

- 事件 schema 校验；
- 事件与 run 产物一致性（trace 反映实际）；
- 取消/失败路径事件完整，且**失败与取消路径同样产出 `<output_dir>/logs/` 两工件**并在 manifest 登记（sha256/行数/级别分布与磁盘一致）；
- 日志落点测试：`log_dir` 缺省落 `<output_dir>/logs`；显式 `log_dir` 越出 `output_dir` 判 exit 2；**注入"落 `run/`/CWD/源码树"必红**（判据 `CHK-LOG-SYS` R3）；
- 日志写失败不静默：注入只读日志目录 ⇒ 运行非 0 退出 + stderr 有脱敏摘要；
- 脱敏测试（无凭据泄漏）；
- G-RES-01：判据边界（10 s 严格界 / ≥10 s 窗）、分母三分量与哨兵、record_and_justify 不改退出码、fail-closed 注入（抹掉样本必翻转）—— 见 `eng/tests/monitoring/test_frozen_gate.py`。
