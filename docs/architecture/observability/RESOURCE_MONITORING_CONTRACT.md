# AstroCS 资源监控伴随器合同（LOG-002）

> 文档 ID：`ARCH-LOG-MONITOR-002`（归属 `docs/architecture/observability/`，owner SA-LOG-08）
> 状态：ACTIVE_NORMATIVE（LOG-002 冻结）
> 机器可读事实源：`runtime/monitoring/monitor.py`（合同列/指纹/校验）、
> `tools/monitoring/verify_monitor_csv.py`（检查器）、
> `runtime/monitoring/linux_procfs.py`（Linux 采集）、
> `runtime/monitoring/windows_pdh_etw.py`（Windows 显式未实现 stub）。
> 本文档是视图；字段定义与验收以 schema/检查器为权威。

## 1. 目的与边界

AstroCS 需要一个**资源监控伴随器**：重任务（`cpu_heavy` / 长运行）run 自动创建
monitor，同一 run ID **每秒**采集 process/system CPU、active/granted workers、
RSS/private/commit、read/write bytes、queue/lock/io wait、provider/module，
产出**不可手工合成**的原始 CSV，供运行图（LOG-003）、审计、容量分析消费。

**验收（tasks/03_RUNTIME_DATA_IO_TASKS.md LOG-002）**：
- 无 monitor 的 `cpu_heavy` run **失败**（负测强制点）；
- I/O 区间与初始化区间**分开**记录；
- 原始 CSV **不可手工合成**（header 指纹链 + 写后只读 + 时间戳单调断言）；
- Linux procfs 与 Windows PDH/ETW 适配**分开**（Windows 仅隔离 stub/显式
  未实现，不误报已支持）。

**边界**：本任务不改科学公式（`scientific_change=false`）；不触碰
LOG-001/RT-006 已冻结语义；不实现 Windows PDH/ETW 真实采集。

## 2. 强制自动建档

`runtime/monitoring/runner.py::HeavyRunGuard` 是强制点：

- `resource_class ∈ {cpu_heavy, io}` 的 run **必须**先
  `create_monitor(csv_path)` 再 `assert_ready()`；否则抛
  `MonitorRequired`（调用方转 FAIL/非零退出）——这就是"无 monitor 的
  `cpu_heavy` run 失败"的执行点。
- `run_heavy_with_monitor(...)` 提供自包含伴随执行助手：自动建档、区间分段
  （init → io → active → flush）、合成 cpu 负载（可选）、写后只读收尾。

## 3. CSV 合同（原始时序数据）

文件布局（UTF-8，LF；`row_fingerprint` 恒为最后一列）：

```
t_iso_utc,seq,run_id,run_phase,interval_s,cpu_pct,sys_cpu_pct,
active_workers,granted_workers,provider,module,rss_bytes,private_bytes,
commit_bytes,read_bytes,write_bytes,queue_wait,lock_wait_ns_est,
io_wait_rate_est,faults_rate,row_fingerprint
```

- 第 1 行 = 合同 header；第 2 行 = **seed 行（seq=0）**：仅 `seq=0`、`run_id`
  与 `row_fingerprint` 非空；其余列为空。
- 其后每行 = 一个样本（`seq=1..N` 严格递增无空洞）。

| 字段 | 单位/取值 | 语义 |
|---|---|---|
| `t_iso_utc` | RFC3339 UTC 秒 | 采样墙钟；展示/跨进程比对（单调断言见 §5） |
| `seq` | 1..N | 样本序号（seed=0）；严格递增 |
| `run_id` | 安全字符串 | 与 trace run_id 同源 |
| `run_phase` | init/active/io/flush | **I/O 区间与初始化区间分开** |
| `interval_s` | 秒 | 距上一采样单调秒（≈1s） |
| `cpu_pct` | % | 进程 CPU = CPU 秒差值/墙钟差值 ×100（100%=1 核满载） |
| `sys_cpu_pct` | % | 系统级 CPU = 非空闲 jiffies/总 jiffies（整机） |
| `active_workers` | int | **真实观测**（RT-006 trace 注入；无来源留 0） |
| `granted_workers` | int | **真实观测**授予租约上限（无来源留 0） |
| `provider` | string | **真实观测** provider（禁止 config 冒充） |
| `module` | string | **真实观测** module/node 归属 |
| `rss_bytes` | B | RSS（VmRSS） |
| `private_bytes` | B | private 匿名内存（RssAnon） |
| `commit_bytes` | B | VmSize 近似 commit |
| `read_bytes` / `write_bytes` | B | 累计读/写字节（/proc/self/io） |
| `queue_wait` | 运行队列 | 系统 loadavg 1m（就绪+不可中断） |
| `lock_wait_ns_est` | ns | **估计**：非自愿 ctx 切换差值 ×1000（`_est` 标注代理） |
| `io_wait_rate_est` | /s | **估计**：缺页差值/墙钟（代理） |
| `faults_rate` | /s | 缺页速率（minflt+majflt 差值/墙钟） |
| `row_fingerprint` | hex64 | 行指纹（§4） |

空值（缺失/不可得）写空串；**绝不静默填 0 冒充观测**。

## 4. 不可手工合成（指纹链 + 写后只读）

### 4.1 行指纹链

```
seed_fp      = sha256(salt | "seed" | sorted(HEADER) | run_id)
fp(seq=n)    = sha256(salt | fp(seq=n-1) | json(行字符串形态) | n)
```

- 每行指纹绑定**前一行指纹**、**行字符串形态**（与文件逐字节一致）与**行号**；
- 修改任意字节 / 手工追加 / 删除行 → 该行及后续全部失配（链式断裂）；
- salt 固定为 `b"astrocs-log002-v1"`，跨进程/跨主机可复验。

### 4.2 写后只读

`ResourceMonitor.seal()`：flush + fsync + close + chmod 只读。监测结束后原始
CSV 不可再写；复验/审计只读。

### 4.3 单 run 单链

`begin()` 拒绝在已有非空 CSV 上续写（每个 run 独立链，防伪造延续）。

## 5. 机器校验（verify_csv）

`tools/monitoring/verify_monitor_csv.py --csv <file> [--run-id R] [--stats]`：
exit 0 = PASS；违例 => 非 0 + machine JSON verdict=FAIL。检查：

1. header 精确等于合同 HEADER；
2. seed 行指纹 == `_seed_fingerprint(seed.run_id)`；
3. 每样本行指纹 == `_fingerprint(prev_fp, 行, seq)`（篡改/追加被抓）；
4. `seq` 严格 1..N 无空洞/重复；
5. `run_id` 全行一致（`--run-id` 指定时）；
6. `t_iso_utc` 单调（秒精度允许相等；`--no-ts-monotonic` 可关）；
7. `run_phase ∈ PHASES`。

## 6. 真实观测接线（RT-006 trace，禁止 config 冒充）

`runtime/monitoring/trace_feed.py::TraceSnapshotObserver` 从 **RT-006 trace
事件**（JSONL 或 TraceStore 快照 dict 列表）推导当前观测：

- `provider`：最近一条携带非空 provider 的 trace 事件（MODULE_CALL /
  PROVIDER_ENTER / WORKER_TASK / NODE_START / NODE_END）的 provider；
- `module`：最近 MODULE_CALL/NODE_START 的 module_id / node_id；
- `active_workers`：NODE_START 未 NODE_END 的活动节点数；
- `granted_workers`：活动节点 NODE_START/MODULE_CALL 的 granted_workers 观测
  最大值；无则最近 WORKER_TASK/MODULE_CALL 的 workers 观测。

**硬约束**：observer 只读 trace 事件，绝不读计划/配置 JSON——配置值冒充观测
即违例。`resource_monitor` 无 observer 且无真实置位时相关列为空/0，不造假。

## 7. Linux/Windows 路径分离

| 后端 | 位置 | 状态 |
|---|---|---|
| Linux procfs | `runtime/monitoring/linux_procfs.py` | **真实实现**（控制节点） |
| Windows PDH/ETW | `runtime/monitoring/windows_pdh_etw.py` | **显式未实现 stub**（隔离） |

- `linux_procfs.collect()`：/proc/self/status、smaps_rollup、stat、io、
  /proc/stat、/proc/loadavg 真实观测；
- `windows_pdh_etw.is_available()` 恒 False；`collect()` 抛
  `NotImplementedError`（**绝不返回伪造观测**）；
- 已知限制：Windows PDH/ETW 真实采集未实现（known_limits；接口签名与
  Linux 对齐，未来 FATDUCK/Windows 控制节点填充）。

## 8. 与 LOG-001 的关系

LOG-002 不修改 LOG-001 冻结 schema/参考实现；可选的 metric 事件输出
（`trace_feed.emit_metric_event`）复用 `astrocs.log.event.v1` 语义（phase=
`monitoring`、event=`metric`）。若后续生产 Runtime 把 monitor 摘要写入统一
JSONL，按 LOG-001 合同做适配（本任务交付 CSV + 指纹 + 校验闭环）。

## 9. 验收（LOG-002）

| # | 验收点 | 证据 |
|---|---|---|
| B1 | 同一 run ID 每秒采集：字段齐、时间戳单调、间隔≈1s | `test_monitor_contract.py` real-run |
| B2 | 无 monitor 的 `cpu_heavy` run FAIL | `HeavyRunGuard.assert_ready` 负测 |
| B3 | CSV 篡改（改 1 字节/追加行）校验失败 | tamper/append 负测 |
| B4 | I/O 区间与初始化区间分开 | `phases_seen` 含独立 init/io 行 |
| B5 | provider/module/workers 来自 trace 真实观测 | `TraceSnapshotObserver` 测试 |
| B6 | Linux procfs 与 Windows PDH/ETW 分开 | 后端分离 + Windows stub 负测 |
| B7 | LOG-001 / RT-006 回归不破坏 | LOG-001 checker + rt006 py surface 回归 |

## 10. 参考

- 任务规格：`tasks/03_RUNTIME_DATA_IO_TASKS.md` LOG-002
- LOG-001：`docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md`
- RT-006：`include/astrocs/core/contracts.h` TraceEvent、
  `runtime/pipeline/trace_replay.py`
- 控制包标准：`14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md` §4/§5
