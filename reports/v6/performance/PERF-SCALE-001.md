# PERF-SCALE-001 — V6 并行性能与确定性验收报告

- 控制包：AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915（Wave 10）
- 基线 HEAD：`5eb702bd72649e498022ca34e4810886b1a6b800`（现场复核，与任务给定一致）
- CLI 二进制：`build/astrocs`，`astrocs 0.11.0-alpha.2+g5eb702bd…`（原二进制陈旧于 `g44e1cb65`，已按任务要求在 HEAD 重建，rc=0）
- 执行面：Linux amd64 控制节点（宪章 §15.1），16 逻辑 CPU（affinity 0-15），无 cgroup CPU 配额
- 建议状态：**REVIEW_REQUIRED**（确定性硬门 PASS；§10.5 资源门按 C-007/SO-05 **只记录不裁决**，且 16 worker 实测均值低于冻结 85% 门限，需负责人裁决）

---

## 0. 结论摘要

| 验收项 | 实测结论 | 依据 |
|---|---|---|
| 1/4/16 worker 墙钟与加速比 | 实测：123.01 s / 50.59 s / 22.02 s；加速比 1.00× / 2.43× / 5.59×；并行效率 100% / 60.8% / 34.9% | `p1_real_ldn43_4k_perf_summary.json` |
| 进程/每线程 CPU、活跃计算线程 | 实测（见 §3） | 同上 + `resource_summary.json` |
| RSS/PSS 峰值与增长 | 实测（见 §3、§5） | `*_runs.json`（外部采样）+ `alloc_report.json`（in-band） |
| 读写字节 / I/O wait / 队列深度 / worker 均衡 | 实测（见 §3；**queue_depth 生产链恒为 0，见 §8 局限**） | in-band `resource_samples.csv` |
| 确定性硬门（逐字节） | **PASS（对 worker 数）**：science payload 原始字节一致 14/18（Phase1）/ 4/7（Phase3），其余仅 run-scoped 元数据；归一后 18/18、7/7 一致；同预算对照产生**相同**差异集合，证明差异与 worker 数无关 | `determinism_cli_*.json`、`determinism_v6_write_paths.json` |
| V6 三模式 + Phase1/2/3 写盘路径 1/4/16 逐字节 | **PASS**：v6_determinism_driver 三条写盘路径 digest 在 1/4/16 完全一致 | `determinism_v6_write_paths.json` |
| §10.5 计算区间平均 CPU 利用率 | 实测 99.6% / 90.7% / **65.1%**（占已分配容量）；16 worker 低于 85% → **PENDING_OWNER_SIGNOFF（只记录）** | `p1_real_ldn43_4k_resource_gate_record.json` |
| 连续 ≥10 s <60% | 正常负载未触发（≤3.6 s）；**注入 CPU 争用后 34.68 s → 被记录** | `neg_contention_16_resource_gate_record.json` |
| 是否只有单线程参与重计算 | 多核预算下**否**（4/16 worker 活跃计算线程均值 4.15 / 14.44）；1 worker 为基线串行 | `*_runs.json.active` |
| 是否存在无界内存增长 | 16 worker 被 in-band 记录为 `alloc_growth_unbounded`（34.01 MB/s ≥ 32 MB/s）；但斜率随墙钟缩短而升高，疑为启动工作集爬坡而非真无界增长 → **登记待裁决** | `alloc_report.json`、run.log resource_gate 事件 |
| 负向/边界 | 确定性门可红（payload 翻转、FITS 数据翻转检出）；资源门在争用注入下记录 3 条 finding | `negative_checks.json` |

**未测/不可达项**：Phase2 CLI 真实数据三模式端到端（fail-closed，见 §6）；Phase3 CLI 为极小负载，资源门不在判定域（<10 s）；Windows/Fatduck 复验属 WIN-VERIFY-001，不在本任务范围。

---

## 1. 方法

### 1.1 两条独立测量面

1. **进程外独立采样（外部权威）**：`artifacts/v6/performance/perf_harness.py` 以 `taskset` 固定 CPU affinity 启动 CLI，同时用 `runtime/v6_budget.py::sample_proc` 每 0.25 s 读 `/proc/<pid>`（进程 CPU、每线程 CPU、RSS/PSS、读写字节、iowait）。`runtime/v6_budget.py` 是 RUNTIME-CI-001 交付的**独立 Python 记录面**，与 CLI 内 C++ `ResourceRecorder` 不同实现。
2. **进程内 in-band 记录器（CLI 自带）**：`resource_samples.csv / resource_summary.json / alloc_report.json / worker_balance.csv`，分 init/active/flush 阶段。§10.5 判定域语义（“计算区间”）以 **active 阶段**为准。

affinity 即 worker 预算：CLI 的 `cmd_phaseN_run` 用 `cli_affinity_cpu_count()` 作为 Runtime budget（`cli/commands.cpp`），故 `taskset -c 0` / `0-3` / `0-15` 对应 1/4/16 worker。

### 1.2 负载

- **重负载（W1，真实数据）**：`astrocs phase1 run` 单帧 LDN43 T2 4096×4096（`LDN43_LRGBH_flying_dutchman-20250504@034648-1200S-Green.fts`）+ T2 `masterBias/Dark/Flat`（.xisf），显式 WCS（取自真实 FITS 头）、`drizzle.nside=512, pixfrac=0.8, fp32`。每预算 3 次重复。
- **V6 三模式写盘（W2）**：`tools/v6/v6_determinism_driver.py` → `tests/integration/v6_p1|v6_p2|v6_p3` 三条写盘路径（`v6_p1_phase1_write` / `v6_p2_three_modes_write` / `v6_p3_export_write`），预算 1/4/16。
- **Phase3 CLI 三导出模式（W3）**：`astrocs phase3 run --export-mode {surface_brightness,point_source_flux,visualization}`，源 = W1 的 16 worker 相位1 产品，`max_tiles=16`（超出默认内存护栏会被 fail-closed 拒绝）。

### 1.3 确定性判定

- 原始 SHA-256 逐文件对照（`check_determinism_cli.py`）。
- 因运行沙盒路径/时间戳/run_id 属 run-scoped 元数据（项目官方 `v6_determinism_driver` 同样对沙盒路径做归一），另给**归一后**对照：屏蔽 run_id、输出路径、`elapsed_sec`、ISO 创建时间、以及依赖 FITS RUNID/CHECKSUM 的 recorded digest。
- FITS 科学载荷按 **HDU 数据数组逐字节** + 归一化头（剔除 RUNID/CHECKSUM/DATE）比较。
- **同预算对照**：同一预算、不同输出目录重复运行，其原始差异集合若与跨预算差异集合完全相同，则差异由 run-scoping 而非 worker 数造成。
- **门灵敏度自检**：翻转 JSON 数值载荷字节、翻转 FITS 数据数组字节必须使归一摘要改变；仅改 RUNID 卡不得改变归一摘要。

---

## 2. 环境与基线校验

| 项 | 值 | 来源 |
|---|---|---|
| git HEAD | `5eb702bd72649e498022ca34e4810886b1a6b800` | `git rev-parse HEAD` |
| CLI 版本串 | `0.11.0-alpha.2+g5eb702bd72649e498022ca34e4810886b1a6b800` | `./build/astrocs --version` |
| CLI sha256 | `3d265a55bf4e9f99bb2d29fabb7aeba4e9c7a0a0ea893e12bd3fbf765894d3bf` | `sha256sum build/astrocs` |
| 重建 | `cmake --build build --target astrocs -j8` rc=0 | `run/v6/performance/build_cmake.log` |
| 内核 / 架构 | Linux 6.12.107+deb13-amd64 / x86_64 | `uname -r` |
| CPU | Genuine Intel(R) CPU 0000 @ 1.70GHz ×16（affinity 0-15） | `/proc/cpuinfo`、`taskset -pc` |
| 内存 | 16,767,508,480 B（≈15.6 GiB） | `free -b` |
| cgroup cpu.max | max（无配额） | `/sys/fs/cgroup/cpu.max` |

完整结构见 `artifacts/v6/performance/environment.json`。

**基线一致性注记**：工作树有 12 个先于本包存在的 tracked 差异（C-006/C-007 记录），本任务**未触碰**任何 tracked 文件；只读 `git rev-parse` 用于基线核对。

**测量后 HEAD 前进的说明**：本任务全部测量在基线 `5eb702bd` 的二进制上完成。任务执行期间控制器集成了 REAL-SCIENCE-001（`815f161f`）。复核 `git diff --name-only 5eb702bd..815f161f -- lib cli runtime CMakeLists.txt tests ci contracts docs` **为空**，即该提交只新增 `artifacts/`+`reports/`+`工程控制/`，未触碰任何生产面，故不影响本次性能/确定性结论。

---

## 3. 1/4/16 worker 性能与资源实测

负载 W1，每预算 n=3，数值为 3 次中位数（外部独立采样）。`CPU%` 单位 = percent_of_one_core；`%alloc` = 占已分配容量百分比。

| worker | 墙钟 (s) | 进程 CPU (s) | 等效核 | 加速比 | 并行效率 | in-band active CPU mean (%alloc) | p50 (%alloc) | p95 (%alloc) | RSS 峰值 (MiB) | PSS 峰值 (MiB) | 写字节 (MB) | iowait% | 活跃计算线程 peak/mean | 单线程区间 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1  | 123.01 | 122.67 | 0.996 | 1.00× | 100% | 99.6 | 100.0 | 102.0 | 690.6 | 687.9 | 380.0 | 1.77 | 3 / 1.27 | 1239/1432 |
| 4  | 50.59  | 179.07 | 3.552 | 2.43× | 60.8% | 90.7 | 99.5  | 100.5 | 754.5 | 751.9 | 381.9 | 1.70 | 5 / 4.15 | 52/597 |
| 16 | 22.02  | 215.26 | 9.746 | 5.59× | 34.9% | **65.1** | **87.6** | 92.9 | 916.8 | 914.1 | 363.0 | 0.88 | 22 / 14.44 | 49/244 |

**解释与不确定性**

- 总 CPU 秒随并行度升高（123 → 179 → 215 s）：说明并行带来额外计算/同步开销（OpenMP 自旋等待与调度），因此**加速比低于等效核比**，墙钟加速（5.59×）低于峰值等效核（9.75）所能给出的上界。这是可复现的真实测量，不是估算。
- 16 worker 时等效核仅 9.75/16；`ASTROCS_LEASE_TRACE=1` 显示 Phase1 各节点**逐个**取得整份 16 租约（`cap=16, budget_available=0`），即节点间串行、节点内并行，叠加 drizzle 单核标度上界，形成利用率缺口。
- **队列深度 queue_depth 在 Phase1 生产链恒为 0**：`resource_samples.csv` 的 `queue_depth` 由外部注入，当前 CLI 未注入（`set_queue` 未被调用）。因此“队列深度”实测值**不可得**，以 `runnable_workers` 与 `ASTROCS_LEASE_TRACE` 作为替代证据（见 §8）。
- `worker_balance.csv` 的 `active_workers/runnable_workers` 亦为配置注入值（恒等于预算），非观测；真正的并行证据是 `active_compute_threads`（按每线程 CPU 正增量计）与 `per_thread_cpu_sum`。报告以观测面为准。

**原始数据**：`artifacts/v6/performance/p1_real_ldn43_4k_perf_table.csv`（逐次运行）、`p1_real_ldn43_4k_perf_summary.json`（聚合）、`p1_real_ldn43_4k_runs.json`（外部采样全量）；逐次 in-band 产物在 `run/v6/performance/p1_real_ldn43_4k_w{1,4,16}_r{0,1,2}/`。

---

## 4. 确定性硬门（逐字节）

### 4.1 官方写盘路径（V6 三模式 + Phase1/2/3）@ 1/4/16

`tools/v6/v6_determinism_driver.py --budgets 1,4,16`，rc=0：

| 写盘路径 | 1 worker digest | 4 worker digest | 16 worker digest | 文件数 | 结论 |
|---|---|---|---|---|---|
| `v6_p1_phase1_write` | `46e62599…258f` | `46e62599…258f` | `46e62599…258f` | 4 | 一致 |
| `v6_p2_three_modes_write` | `9ef8cbfd…f33d` | `9ef8cbfd…f33d` | `9ef8cbfd…f33d` | 17 | 一致 |
| `v6_p3_export_write` | `23ca5bf1…f912` | `23ca5bf1…f912` | `23ca5bf1…f912` | 9 | 一致 |

（digest = 相对路径 + 文件内容 SHA-256 的排序聚合；驱动内置“载荷翻转必须改变摘要”的灵敏度自检。）

> 范围注记：这三条集成写盘路径的库级算子在当前构建下未随 taskset 预算改变线程数（Phase2 构建日志明示 `OpenMP disabled … serial sampler`）。因此它们是**确定性**证据，而**worker 扩展性**证据来自 §3 的真实 CLI 路径。

### 4.2 CLI 真实数据 Phase1（1/4/16）+ Phase3 三模式（1/4/16）

**Phase1（真实 4096² 帧）** — science products 18 个：

| 文件 | 原始一致 | 归一后一致 |
|---|---|---|
| `calibrated_*.fts` | ✅ | ✅ |
| `signal/Norder0/Dir0/Npix7.fits`、`support/…/*.fits`、`Moc.fits`、`metadata.fits` | ✅ | ✅ |
| `p1_snr.json`、`p1_psf.json`、`p1_phot.json`、`p1_flux.json`、`p1_sources.json`、`p1_wcs.json`、`manifest.json` | ✅ | ✅ |
| `p1_final.json` | ❌（仅 `hips_root`/`properties` 输出路径） | ✅ |
| `p1_stack.json` | ❌（仅 `elapsed_sec`） | ✅ |
| `signal/properties`、`support/properties` | ❌（仅 `hips_creation_date`） | ✅ |
| `astrocs_run_*.json`、`run_context.json`、`graph/**`、`resource_*`、`alloc_report.json`、`worker_balance.csv` | ❌（run-scoped：run_id/时间/资源序列） | — (非科学载荷) |

**Phase3 三导出模式** — science products 各 7 个，三种模式结论相同：

| 文件 | 原始一致 | 归一后一致 |
|---|---|---|
| `output_phase3.fits` | ❌（仅头 RUNID/CHECKSUM；**HDU 数据数组逐字节一致**） | ✅ |
| `p3_resampled.bin`、`p3_resampled.json`、`p3_props.json`、`p3_wcs.json` | ✅ | ✅ |
| `p3_verify.json`、`p3_writer.json` | ❌（仅 `output_fits` 路径 / run_id / 记录的原始 product sha256） | ✅ |

### 4.3 同预算对照（排除 worker 依赖）

| 对照（同预算，不同输出目录） | 原始差异集合 |
|---|---|
| 16 worker × 3 次 | 与跨预算完全相同的 4 个 Phase1 文件 |
| 4 worker × 3 次 | 同上 |
| Phase3 surface_brightness @16 × 2 次 | 与跨预算完全相同的 3 个 Phase3 文件 |

→ 原始差异由**输出目录/时间戳/run_id** 造成，与 worker 数无关；worker 数确定性成立。

### 4.4 门灵敏度（负向）

- `p1_snr.json` 载荷字节翻转 → 归一摘要改变 ✅
- FITS 数据数组 1 个 float 翻转 → FITS 数据摘要改变 ✅
- 仅改 FITS RUNID 卡（原始字节原地替换，raw digest 改变）→ 归一摘要**不变** ✅（证明归一化只针对 run-scoped provenance，不会掩盖科学载荷）

哈希对照表：`artifacts/v6/performance/determinism_cli_p1_hash_table.csv` 及 `determinism_cli_p3_*.csv`。

---

## 5. §10.5 / §17.6 资源门（只记录，PENDING_OWNER_SIGNOFF）

阈值引用（**冻结**，未改动）：宪章 §10.5 / §18.2 —— 平均 ≥ 已分配容量 85%、任意连续 10 s < 60% 失败、只有一个活跃计算线程失败；内存增长 32 MB/s 失败线；SO-05 未签字前只记录。

| worker | active 窗口 (s) | active CPU mean (%alloc) | p50 (%alloc) | 连续 <60% 最长 (s) | 判定阈值越线（只记录） |
|---|---|---|---|---|---|
| 1 | 121.0 | 99.6 | 100.0 | 0.25 | 无（且 allocated<2，资源门判定域不成立） |
| 4 | 48.5 | 90.7 | 99.5 | 2.01 | 无 |
| 16 | 20.0 | **65.1** | **87.6** | 2.55 | `cpu_mean_low`、`cpu_p50_low` |

**处置**：`status = record_only_pending_owner_signoff`、`hard_fail = false`、`auto_adjudication_allowed = false`（与 `cli/v6_runtime_contract.h` 生产策略一致）。**本报告不据此宣布通过或失败**。原始：`p1_real_ldn43_4k_resource_gate_record.json`。

### 两项发布门禁事实（§17.6）

1. **只有单线程参与重计算？** 多核预算下**否**。4/16 worker 的活跃计算线程均值 4.15 / 14.44（峰值 5 / 22）；16 worker 中 49/244 个区间瞬时 ≤1 线程，但无连续 ≥10 s 的单线程窗口。1 worker 是基线串行（affinity=1），非生产推荐配置。
2. **无界内存增长？** 16 worker 有 ≥1 次运行被 in-band 记录 `alloc_growth_unbounded`（三次稳健斜率 34.01 / 25.03(中位) MB/s，失败线 32 MB/s，active 窗口 20.0 s ≥10 s）；三个预算均记录 `alloc_reclaim_missing`（结束残留不可解释）。
   **不确定性**：斜率随墙钟缩短而升高（1w 5.98、4w 15.37、16w 34.01 MB/s），与“固定启动工作集爬坡”一致，而非随运行时长单调增长；`peak_commit` 1w 0.85 GB → 4w 1.39 GB → 16w 3.31 GB 亦为并发缓冲放大。因此**不能断言真无界增长**，但该记录是真实的 in-band 门禁事实，登记为待负责人裁决项。

---

## 6. 未测 / UNAVAILABLE

| 项 | 状态 | 证据 / 原因 |
|---|---|---|
| Phase2 CLI 三生产模式真实数据端到端 | **UNAVAILABLE（fail-closed）** | `node upm_fit failed: p2_upm_build_geo failed rc=1 (production control-ivar weight: missing/invalid control ivar…)`；根因为采样阶段 `obs=0 overlap_controls=0`（单帧覆盖远小于 control 网格）。日志：`run/v6/performance/trial2_p2.log`、`trial2_p2b.log`。改用官方写盘路径 `v6_p2_three_modes_write`（§4.1）承担三模式确定性。 |
| Phase3 CLI 资源利用率 | **不适用** | 墙钟 ≈0.75 s（64×64 导出，非 heavy），active 窗口 <10 s，§10.5 判定域不成立；仅作确定性检查。 |
| Windows/Fatduck 复验 | **不在范围** | 属 WIN-VERIFY-001；本报告不据此宣称发布。 |
| 队列深度 queue_depth | **不可得** | Phase1 生产链未注入 `set_queue`；以 `runnable_workers`/lease trace 替代。 |
| 外部采样 read_bytes | 恒 0 | `/proc/<pid>/io read_bytes` 只计真实磁盘读，输入在 page cache；in-band `read_bytes` 同为 0，`write_bytes` 有实测值。 |

---

## 7. 负向 / 边界检查

| ID | 注入 | 结果 | 证据 |
|---|---|---|---|
| A1 | `p1_snr.json` 载荷字节翻转 | ✅ 归一摘要改变（门可红） | `negative_checks.json` |
| A2 | FITS 数据数组 1 float 翻转 | ✅ FITS 数据摘要改变 | 同上 |
| A3 | 仅原位改 FITS RUNID 卡 | ✅ 归一摘要不变（归一化只针对 provenance） | 同上 |
| B | `OMP_NUM_THREADS=1` @allocated 16 | ⚠️ **未限制成功**：墙钟 17.98 s、等效核 10.11 —— Runtime 节点级 `ScopedOmpWorkerInjection` 覆盖 `OMP_NUM_THREADS`，故该注入不是有效并发限制（如实登记为“非检出”） | `neg_omp1_16_resource_gate_record.json` |
| B2 | 12 个忙循环进程钉在 CPU 0-11 制造 CPU 争用 @allocated 16 | ✅ **被记录**：外部 mean 39.4% / in-band 41.2% of alloc；最长连续 <60% = **34.68 s ≥10 s** → 3 条 finding（含 `consecutive_low_utilization`）；run.log 捕获 `resource_gate … record_only` | `neg_contention_16_resource_gate_record.json`、`neg_contention_harness.log` |
| C | `ASTROCS_LEASE_TRACE=1` @16 | 记录 22 条 lease/budget：各节点 `host_workers=16 acquired=1 cap=16 budget_available=0`（节点间串行、整份租约、无嵌套） | `negative_checks.json`、`lease_trace_16_w16_r0/run.log` |
| CLI 模式路由 | `tools/v6/v6_cli_mode_matrix.py` | ✅ 23/23（生产三模式放行；`psf_snr_power`/legacy 拒绝 rc=2） | `cli_mode_matrix.json` |

---

## 8. 未决风险与需负责人裁决

1. **SO-05（最高优先）**：16 worker 计算区间平均 CPU 利用率 65.1% < 冻结 85%，p50 87.6% < 90%；是否判失败、或按 §10.5 例外的“内存带宽饱和/不可并行依赖/明确 I/O 等待”调整单 kernel 门禁，需负责人签字。本任务按 C-007 **只记录**。
2. **内存门事实**：16 worker `alloc_growth_unbounded` + 全预算 `alloc_reclaim_missing` 需裁决；建议区分“启动工作集爬坡”与“真无界增长”（本报告已给斜率随墙钟的趋势证据），必要时以更长墙钟/多帧负载重测。
3. **Phase2 CLI 真实数据不可达**：`obs=0` 导致 control ivar 缺失 fail-closed。属数据流覆盖问题（REAL-SCIENCE-001 域）；建议由真实多帧覆盖流程复验 Phase2 三模式端到端与资源面。
4. **queue_depth / worker_balance 注入面未接**：`set_queue` 未被生产链调用，`worker_balance.csv` 为预算回填值；如需“队列深度/worker 均衡”作为正式门禁证据，建议在 Runtime 侧接入真实 runnable/queue 观测（属 RUNTIME-CI-001 后续面，不在本任务写域）。
5. **注入有效性**：`OMP_NUM_THREADS` 不能限制 Runtime 管理的节点并行度（这是 §10.4 单一预算源的正面证据），后续资源违规注入应走 CPU 争用/cgroup 配额而非 OMP 环境变量。
6. **构建面遗留**：`Phase2: OpenMP disabled (hotfix default, serial sampler)` —— 若 Phase2 被期望重并行，需负责人确认是否属预期。

---

## 9. 可复现命令清单

所有命令在仓库根执行，均带 `timeout` 并落日志；退出码见各自日志。

```bash
# 0) 基线与二进制（重建至 HEAD）
git rev-parse HEAD                      # 5eb702bd…
cmake --build build --target astrocs -j8   # rc=0; run/v6/performance/build_cmake.log
./build/astrocs --version                  # +g5eb702bd…

# 1) W2 官方写盘路径确定性（V6 三模式 + Phase1/2/3）@ 1/4/16
python3 tools/v6/v6_determinism_driver.py --work-root run/v6/performance/det_integration \
  --budgets 1,4,16 --json-out artifacts/v6/performance/determinism_v6_write_paths.json \
  2>&1 | tee run/v6/performance/determinism_driver.log          # rc=0, V6_DETERMINISM_PASS

# 2) W1 真实数据 heavy 负载 1/4/16（外部独立采样）
python3 artifacts/v6/performance/perf_harness.py --name p1_real_ldn43_4k \
  --cmd "./build/astrocs phase1 run --config {cfg} --events-jsonl" \
  --config-template artifacts/v6/performance/configs/p1_real_ldn43.json.tmpl \
  --budgets 1,4,16 --reps 3 \
  --out artifacts/v6/performance 2>&1 | tee run/v6/performance/perf_p1_real.log

# 3) 聚合 + §10.5 只记录裁决
python3 artifacts/v6/performance/analyze_perf.py \
  --runs-json artifacts/v6/performance/p1_real_ldn43_4k_runs.json \
  --out-prefix artifacts/v6/performance/p1_real_ldn43_4k

# 4) 确定性对照（跨预算 + hash 表）
python3 artifacts/v6/performance/check_determinism_cli.py \
  --runs "1:run/v6/performance/p1_real_ldn43_4k_w1_r0,4:run/v6/performance/p1_real_ldn43_4k_w4_r0,16:run/v6/performance/p1_real_ldn43_4k_w16_r0" \
  --out-json artifacts/v6/performance/determinism_cli_p1.json \
  --out-csv  artifacts/v6/performance/determinism_cli_p1_hash_table.csv

# 4b) 同预算对照（应产生相同原始差异集合）
python3 artifacts/v6/performance/check_determinism_cli.py \
  --runs "16a:run/v6/performance/p1_real_ldn43_4k_w16_r0,16b:run/v6/performance/p1_real_ldn43_4k_w16_r1,16c:run/v6/performance/p1_real_ldn43_4k_w16_r2" \
  --out-json run/v6/performance/ctrl_p1_16_ctrl.json --out-csv run/v6/performance/ctrl_p1_16_ctrl.csv

# 5) W3 Phase3 三导出模式 @ 1/4/16
for M in surface_brightness point_source_flux visualization; do
  python3 artifacts/v6/performance/perf_harness.py --name p3_$M \
    --cmd "./build/astrocs phase3 run --config {cfg} --export-mode $M --events-jsonl" \
    --config-template artifacts/v6/performance/configs/p3_export_16tiles.json.tmpl \
    --budgets 1,4,16 --reps 1 --out artifacts/v6/performance
done

# 6) 负向/边界（载荷翻转 + OMP 注入 + CPU 争用注入 + lease trace）
python3 artifacts/v6/performance/negative_checks.py

# 7) CLI 模式路由矩阵（独立交叉校验）
python3 tools/v6/v6_cli_mode_matrix.py --cli-bin build/astrocs \
  --work run/v6/performance/cli_mode_matrix \
  --json-out artifacts/v6/performance/cli_mode_matrix.json
```

---

## 10. 声明

- 本任务**未** commit/push，**未**执行任何 git 写操作；仅只读 `git rev-parse/log`。
- 写入仅限 `artifacts/v6/performance/`、`reports/v6/performance/`、`run/v6/performance/`；未修改任何生产源码、测试、`docs/`、`contracts/`、`ci/`、根构建面。`build/` 仅按任务指令重建 CLI（gitignore）。
- **未**派生子代理。
- 所有指标均来自真实运行的实测输出，附命令、原始日志路径；未测/UNAVAILABLE 项如实标注，未用推测值充数。
- 未宣布发布；未改动任何冻结公式/容差/门；未让 median(SNR_F)/support/coverage/FWHM 进入权重面；`psf_snr_power` 保持 DEFERRED。
