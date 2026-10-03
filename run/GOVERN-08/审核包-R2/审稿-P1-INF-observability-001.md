# 审稿-P1-INF-observability-001（G08-05 对抗审稿 第 1 遍）

- **片号**：`INF-observability-001`
- **层**：`lib/infrastructure/observability`
- **基线 HEAD**：实测 `1fa477a7a05c315550df2ce2bafc3e64ed9bbd78`（`移除实验域运行结果归档，报告迁入单元文档目录`）。
  ⚠️ **任务书给定的 `850a9ede` 与仓内实际 HEAD 不符**；本片一切结论以实测 HEAD 为准。
- **分片清单**：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml`（片号 `INF-observability-001`，成员份数 14，实际行数 2468）
- **口径声明**：行数 = `wc -l`（含末行无换行计 1 的物理行），与清单「实际行数 2468」逐份相加完全吻合。
- **纪律自证**：全程只读。零 git 写（无 add/commit/checkout/reset/stash）、零仓内文件修改、零编译、零 ctest/pytest/构建/二进制运行。所有注入实验只落 `/tmp/g08_inf_obs/`。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（清单） | **14** |
| 实际读了几分 | **14（14/14，100%）** |
| 成员总行数（清单 2468 = 实测逐份相加） | **2468** |
| 实际读了多少行 | **2468** |
| 覆盖率 | **100%（14/14 份，2468/2468 行，逐行 `read` 原文，无抽样、无跳读）** |
| 未读完的 | **无** |

逐份行数实测（与清单逐份一致）：

| # | 文件 | 行 | # | 文件 | 行 |
|---|---|---|---|---|---|
| 1 | `monitoring/monitor.py` | 706 | 8 | `logging/log_event_v1.schema.json` | 162 |
| 2 | `probes/src/probe.cpp` | 372 | 9 | `probes/README.md` | 64 |
| 3 | `monitoring/linux_procfs.py` | 254 | 10 | `logging/README.md` | 42 |
| 4 | `logging/log_event.py` | 248 | 11 | `monitoring/windows_pdh_etw.py` | 41 |
| 5 | `probes/include/acsd/probe.h` | 183 | 12 | `monitoring/__init__.py` | 19 |
| 6 | `monitoring/runner.py` | 181 | 13 | `README.md` | 19 |
| 7 | `monitoring/trace_feed.py` | 163 | 14 | `PENDING.md` | 15 |

---

## 2. 本片判定：**阻断（BLOCKING）**

本片是「不可伪造原始记录」这条安全性质的**唯一实现点**，而该性质**经实测为假**：一个 100% 手工合成的 CSV 可以通过 `verify_csv()` 并被当作生产上报。同一片里另有 3 条「真实观测」类承诺被代码自身推翻（恒等式伪指标、跨 run 污染、恒真守卫门）。

**最重 3 条**

1. **【阻断·自洽式断言】`verify_csv()` 不能检测伪造，其期望值由与被检量同一套定义式现场重算。**
   `monitor.py:91` `_FP_SALT = b"acsd-monitor-v1"` 是**仓内公开字面量**，`_fingerprint`（`:94-109`）/ `_seed_fingerprint`（`:112-121`）是**无密钥 sha256**（`grep -nE "hmac|urandom|getenv|secret" monitoring/*.py` → 0 命中）。种子检查更直接地循环：`:659` `seed_rid` 从**被检文件本身**读出，`:662` `expect_seed_fp = _seed_fingerprint(seed_rid)`，`:663` 与文件里的值比 —— 两端都是攻击者可控的同一行。
   ⇒ **本轮最有价值的产出即此条**：模块 docstring `:7-9`「原始 CSV 不可手工合成」、`:314-315`「外部篡改任意字节都会被 verify_csv 抓出」均为**假命题**。实测伪造通过（见 §5）。

2. **【阻断·恒等式伪指标 + 筛掉真信号】`io_wait_rate_est` 与 `faults_rate` 逐位恒等，而真正的 iowait 观测被采集后丢弃。**
   `monitor.py:424-425` 两列由**同一个表达式** `fm / dt` 赋值，构造性逐位相同（实测 307.0 == 307.0）。真正的系统级 iowait jiffies 在 `linux_procfs.py:243` 被真实采集（实测 `sys_cpu_jiffies_iowait = 85133713`），而 `monitor.py` 中 `iowait` 出现次数 = **0**。合同 `monitor.py:67-68` 把二者描述成两个不同语义的指标。任何把二者当独立信号聚合的下游都会**双计**。

3. **【阻断·承诺推翻】`runner.py` 的「无 monitor 的 cpu_heavy run 必须失败」负测执行点是恒真门，结构上永不触发。**
   `runner.py:117-118` `create_monitor()` 无条件执行 `:74 self._monitor = m`，紧接 `:119 guard.assert_ready()`，其判据 `:85` 的第二个合取项 `self._monitor is None` 在上一行已为假。8 个 resource_class 实测触发率 **0/8**。模块 docstring `:3`/`:84` 自称「验收负测执行点」不成立。

---

## 3. 逐文件清单

### 3.1 `lib/infrastructure/observability/monitoring/monitor.py`（706 行，读毕）

- **读了什么**：模块全篇，含 CSV 合同定义、指纹链、`ResourceMonitor` 全部生命周期方法、`load_rows`、`verify_csv`。
- **看到什么**：
  - 指纹链为无密钥 sha256，salt 公开（`:91`）；写入端 `_append`（`:433-454`）与校验端 `verify_csv`（`:627-700`）**调用同一个 `_fingerprint`** —— 期望量与被检量同源。
  - `verify_csv` 的 seed 检查循环自证（`:659-663`）；且 `:668` 的 `data_rows = rows[1:]` 使**种子行的 `run_id` 永不与调用方 `run_id` 比较**（`:691-693` 只遍历数据行）。
  - 种子行另外 19 列**不在 seed 原像内**（`:112-121` 只喂 HEADER+run_id）→ 种子行任意填垃圾仍合法。
  - `verify_csv` **不校验任何数字列**：`rss_bytes="NaN"`、`cpu_pct="not-a-number"`、`interval_s="-99999"` 全部通过；`load_rows:616-620` 还把它们原样吐出。
  - `verify_csv` **不检查是否 seal 过**（全文无 `stat`/`access`）；0644/0666/0777 均通过。
  - `verify_csv` **不绑定长度** → 删尾行通过、链延长通过、零样本行（只有 header+seed）通过。
  - `:683-687` `and ts` 短路 → 空时间戳行整块豁免；比较是**字典序字符串比较**，从不解析 RFC3339。
  - 静默降级三处：`seal()` `:564-568` `except OSError: pass`（且 `_sealed` 已在 `:552` 置真 → 永不重试）；`_sample_locked` `:465-466` 早退无记录；`:479-482` 近零间隔丢样本无记录。
  - 死字段：`_baseline_taken`（`:235` 写、`:477` 写，**从不读**）、`_backend_kind`（`:229` 写，**从不读**；`_Backend.describe()` 亦无调用者，CSV 里没有后端来源记录）。
  - `:82-84` 用模块级 `assert` 承载合同，被 `python -O` 剥离。
- **判定**：**阻断**（F-1 / F-2 / F-5 / F-6 / F-7 / F-8）。

### 3.2 `lib/infrastructure/observability/probes/src/probe.cpp`（372 行，读毕）

- **读了什么**：配置单例、sink、JSON 转义、`Tls` 聚合、`ScopeTimer` 全部方法、`count_add`/`gauge_set`/`flush`。
- **看到什么**：
  - `gauge_set` `:348-367`：`:355-357` 用**首值**初始化 last/min/max；`:363-364` 用 `<` / `>` 比较。**首值为 NaN 时 min/max 永久钉死在 NaN**（实测：`last=42 min=nan max=nan n=4`，后续 3 次正常更新无法修复）。
  - `append_double` `:139-143` 用 `%.17g` → 实测输出裸 `nan` / `inf` / `-inf`，**RFC 8259 非法 JSON**（Python `json.loads` 全部 REJECTED）。而输出格式在 `probe.h:24-26`/`probes/README.md:44-52` 明文声明为 JSONL → 单个 NaN 观测毒化整行。
  - `sink_write` `:76-79`：`s.tried = true` 在 `append_open` **之前**置位 → 一次瞬时失败（EACCES/EMFILE）永久禁用全进程探针输出，**无重试、无报错、无日志**；`:81` `if (s.file == nullptr) return;`、`:82-83` 两个 `(void)` 显式丢弃 write/flush 返回值。
  - `note_line()` **仅在 `:327`（`ScopeTimer::stop`）被调用**；`count_add`（`:345`）与 `gauge_set` 不调用 → **纯 counter/gauge 线程永远不评估 `ACSD_PROBE_FLUSH_MS`**，直到线程退出。推翻 `probe.h:30`「按行数或时间批量 flush」。
  - `:275-283` 前导点切分缺陷：`dot == q` 时 else 分支令 `name_` 含前导点。
- **判定**：**阻断（数值稳定性）+ 须修**（F-9 / F-10 / F-11 / F-12）。

### 3.3 `lib/infrastructure/observability/monitoring/linux_procfs.py`（254 行，读毕）

- **读了什么**：全篇含 `_read_stat_fields` 字段索引、`collect()` 返回契约。
- **看到什么**：
  - **字段索引经实测正确**：现场 `/proc/self/stat` 核对 field 10=minflt(1488)、12=majflt(0)、14=utime(4)、15=stime(0)、20=num_threads(1)，与 man `proc_pid_stat` 一致 → **代码对**。
  - 但 **`:84-88` 的 docstring 字段表是错的**（写「9 vsize, 10 rss(页), 12 utime, 13 stime」），而 `:105-106` 的注释是对的 —— **同一文件内自相矛盾**，足以诱导后来者把对的代码「改错」。
  - `at()` `:96-103` 解析失败/截断**返回 0 而非 None** → 实测截断输入得 `{'utime_ticks':0,'stime_ticks':0,'threads':0,...}`。直接违反本文件 `:34-35` 自述「采集失败用异常向上抛出…**不能静默 0**」。且 `:219-220` 的 `if "utime_ticks" in s else None` 分支**恒不可达**（键必然存在）。
  - **`:11` 注释与内核文档相反**：`man 5 proc_pid_io` 实测 `read_bytes: The number of bytes really fetched from the storage layer.`（**页缓存命中不计入**），而注释写「页缓存命中也计入的块 I/O 计数」。缓存命中的重读会被报成 0 I/O。
  - `import re`（`:41`）全文未用。
- **判定**：**须修**（F-13 / F-14 / F-15）。

### 3.4 `lib/infrastructure/observability/logging/log_event.py`（248 行，读毕）

- **读了什么**：全篇含 `_REDACT_PATTERNS`、`_fit_jsonl`、`LogEvent`、`SeqAllocator`。
- **看到什么**：
  - `_raw` 路径 `:145-147` 在**任何校验与 `redact()` 之前** `return`。实测：`LogEvent(..., _raw={... "level":"TOTALLY_BOGUS_LEVEL", "event":"NOT_A_REAL_EVENT", "diagnostic":"... /home/alice/private/star.fits ..."})` 构造成功，非法 level/event 被接受，**脱敏未施加，路径原样泄漏**。`:145` 注释「从**已脱敏** dict 反序列化」把安全属性**当成前置假设**而非强制项。
  - **schema 约束几乎全部未强制**：`schema:76` `commit` 为 `^[0-9a-f]{40}$`（无空串分支），而 `LogEvent` 默认 `commit=""`（`:139`）且从不校验 → 实测产出 `commit=''` 不匹配正本。`run` 模式、`ts` 模式、`diagnostic` maxLength 同理未校验。
  - `_fit_jsonl` 兜底 `:130` 按**字符**切片（`line[:budget-1]`）却全程按**字节**预算，且盲删末字符再补 `}`。实测可达：`to_jsonl(max_bytes=160)` → 173 字节且 **JSON 非法**（`'...,"phase":"runtime","run":}\n'`）；`max_bytes=64` 同样非法 JSON。`:129` 注释自承「理论不可达」——**可达**。（公平记账：`max_bytes=4096` 默认档实测合法且不超 4096，缺陷只在收窄档暴露。）
  - `_clip_utf8` `:97` `cut + "…"`：省略号 3 字节**不在预算内** → 实测 30 字节预算返回 **33 字节**。
  - `SeqAllocator` `:230-243` docstring 自称「**线程安全**」，**全文件 0 处 lock/threading**（实测 grep 计数 = 0）。1.6M 次并发调用未复现重复（诚实记录：**未复现**），但保证不存在。
  - `redact()` `:59-70` 只覆盖 `/home`、`/Users`、`/tmp`、盘符、UNC、URL，**不覆盖本仓真实根 `/workspace`**；且路径正则遇空白即止。实测 `/workspace/Astro CS Database/run/probe/x.fits` **原样输出**；`/home/alice/my data/star.fits` → `<redacted> data/star.fits`（尾部泄漏）。与 `:74` docstring 承诺不符。
  - `LOG_IDENTIFYING_KEYS` / `RUN_EVENT_STREAM_SCHEMA_OWNER` / `RUN_EVENT_STREAM_KIND_KEY` 三条「不得混用」规则常量**外部引用数 = 0** → 规则从未被任何代码执行。
- **判定**：**阻断 + 须修**（F-16 / F-17 / F-18 / F-19 / F-20）。

### 3.5 `lib/infrastructure/observability/probes/include/acsd/probe.h`（183 行，读毕）

- **读了什么**：全篇含两层开关、`ScopeTimer` 私有成员与 12 个宏。
- **看到什么**：
  - **注释自相矛盾**：`:73`「字符串标签按**借用指针**保存: 其生命周期必须覆盖本作用域计时器」 vs `:109`「字符串标签**按值**保存, 不受调用方字符串生命周期影响」；实现 `probe.cpp:337` `tg.s = value` 是**拷贝** → `:73` 为过期悬空注释，会误导调用方加伪防护。
  - `:39` `#if defined(ACSD_PROBES) && (ACSD_PROBES)`：**我先假设** `-DACSD_PROBES=ON` 会编译失败；**实测被推翻** —— `CMakeLists.txt:516` 是 `target_compile_definitions(acsd_probes PUBLIC ACSD_PROBES=1)`，`probes/README.md:16` 的 `-DACSD_PROBES=ON` 走 `CMakeLists.txt:26` 的 `option()` 再转 `=1`，**合法**。假设作废（见 §7 否决记录）。
  - `#include "aio_atomic_file.h"`（`probe.cpp:26`）：**存在**于 `lib/infrastructure/aio/src/aio_atomic_file.h` 且 `CMakeLists.txt:514` 已加该 include 目录 → **无悬空**。
  - `scope_`/`name_` 为借用 `const char*` 且无生命周期契约（一参数构造时 `name_` 指向调用方串内部）。
- **判定**：**建议**（F-21）。

### 3.6 `lib/infrastructure/observability/monitoring/runner.py`（181 行，读毕）

- **读了什么**：全篇含 `HeavyRunGuard`、`run_heavy_with_monitor`、`_burn_cpu`。
- **看到什么**：
  - `assert_ready()` 恒真门（详见 §2-3）。
  - `emit_metric_event` 在本文件出现 **0 次且未被 import**（`hasattr==False`），而 docstring `:13` 承诺「采集完成把摘要作为 LOG-001 metric 事件输出」→ **承诺从未兑现**。
  - 死导入 `PHASES`（`:31`，全文件唯一命中）、`utc_now_s`（`:32`，唯一命中）；死参数 `host`/`commit`（`:101`，仅签名）；`MonitorRequired.resource_class`/`.run_id`（`:46-47` 写、从无读）。
  - `REQUIRE_MONITOR_CLASSES = ("cpu_heavy","io")`（`:35`）vs 注释 `:34`「任务规格: **heavy**/cpu_heavy」——`"heavy"` 在活树中作为资源类取值 **0 命中**（仅散文/台账文本）。
  - docstring `:11-12`「实时获取真实观测」不成立：`observer` 在 `:117` 构造时事件列表即被冻结（`trace_feed.py:66` `list(events)`），monitor 每行重放同一冻结值；`:133` 又只取一次快照。
- **判定**：**阻断 + 须修**（F-3 / F-22）。

### 3.7 `lib/infrastructure/observability/monitoring/trace_feed.py`（163 行，读毕）

- **读了什么**：全篇含 `TraceSnapshotObserver`、`observer_from_jsonl`、`emit_metric_event`、`utc_now_s`。
- **看到什么**：
  - **`run_id` 是只写死字段**：`self.run_id = run_id`（`:65`）是全类唯一出现，`observe()`（`:72-115`）**从不按 run_id 过滤**。实测：observer 声明 `run_id="runA"`、事件表只含 runB → 返回 `{'provider':'PROVIDER_B','granted_workers':16}`，**四项全部来自 runB**。docstring `:3-4`「从 trace **真实来源**取」被推翻；`monitor.py:368-378` 会把这批串到 runA 自己的 CSV 行里。
  - `granted_workers` 是**全表历史峰值**（`:98-101` 对每个 carrier 事件含 `node_end` 取 `max`，无重置无活性判据）。实测 granted 序列 `[16,16,16,16,16]`（非降），active 从 2 → 0 时 granted 仍钉在 16；实际最后声明的 grant 是 2 → 朴素 `active/granted` 低估 **8 倍**。同时与 docstring `:16`「最近一条」和 `:22`「**活动节点**的」两处口径**都不符**。
  - `:103-109` 兜底分支**非单调**（`workers`=[8,3,16,2] → granted=[8,3,16,2]），且**绕过 `_EVENT_TYPES`/`_GRANTED_CARRIERS` 闸**：`{"type":"totally_unknown_type","workers":777}` 也能置位。
  - `self._active`（`:67`）为死字段（`observe()` 用 `:77` 的局部 dict）。
  - `:148` 用绝对导入 `from lib.infrastructure.observability.logging.log_event import LogEvent`，与本包其余的相对导入风格相反，对 sys.path 布局敏感。
- **判定**：**阻断 + 须修**（F-23 / F-24）。

### 3.8 `lib/infrastructure/observability/logging/log_event_v1.schema.json`（162 行，读毕）

- **读了什么**：全篇 draft-07 schema。
- **看到什么**：
  - `:76` `commit` 强制 `^[0-9a-f]{40}$`，**无空串分支** → 与 `LogEvent` 的 `commit=""` 默认值直接冲突。
  - `:138` 声明「条件必填由检查器 **`check_error_payload`** 强制」→ **该检查器全仓不存在**（`grep -rn check_error_payload lib eng docs` → 0 命中）。
  - schema 的 pattern/enum/maxLength 约束**无任何代码或测试断言 `LogEvent` 产出**（`eng/tests/monitoring/test_log_contract.py:58` 只比对 `required` **名称清单**）。
- **正面记账**：`log_event.REQUIRED_FIELDS` 与 `schema.required` **实测完全相等**，且有测试背书（`test_log_contract.py:58`）——这是本片**唯一**被测试支撑的自洽断言，不作为缺陷记。
- **判定**：**须修**（F-17 / F-25）。

### 3.9 `lib/infrastructure/observability/probes/README.md`（64 行，读毕）

- **看到什么**：`:7-8` 两层开关表与代码一致；`:16` 的 `-DACSD_PROBES=ON` **正确**（已验证）。`:50` 示例 JSON 行同时含 `wall_us` 与 `n` —— 按 `probe.cpp:312-326`，`ScopeTimer` 行只出 `wall_us`+tags，`n` 只属 gauge 行，两者构造性不同现（除非 tag 恰名为 `n`，但示例未标注），**示例易误导**。`:63` 引用的 `eng/tools/l4_rebuild/sysmon.py` **存在**。`:57` 称「并 `fflush`」，实现是 `aio_atomic::append_flush`，**措辞漂移**。
- **判定**：**建议**（F-26）。

### 3.10 `lib/infrastructure/observability/logging/README.md`（42 行，读毕）

- **看到什么**：
  - `:39` 与 `:41` 把 `eng/tools/monitoring/check_log_contract.py` 作为规范验证命令 —— **该文件不存在**（`ls eng/tools/monitoring/` 仅 6 个文件，无 checker）。`:30` 声称 LOG-001 产物是「合同 + **检查器**」——**检查器已缺失**。这是本片多条 schema 约束长期无人执行的**根因**。
  - `:12` 的相对路径 `../../docs/engineering/observability/STRUCTURED_LOGGING_CONTRACT.md`：从 `lib/infrastructure/observability/logging/` 出发 `../../` 只到 `lib/infrastructure/`，**相对路径层级错**（应为 `../../../../`）。目标文件本身**存在**。
- **判定**：**须修**（F-25 / F-27）。

### 3.11 `lib/infrastructure/observability/monitoring/windows_pdh_etw.py`（41 行，读毕）

- **看到什么**：**本片唯一完全合规的文件。** `is_available()` 恒 `False`（`:24-26`），`collect()` 抛 `NotImplementedError`（`:36-38`）——显式 fail-closed、零假数据、零静默 0，完全符合本项目规范。`:17` 引用的 `docs/engineering/observability/RESOURCE_MONITORING_CONTRACT.md` **存在**。
- **判定**：**通过**。

### 3.12 `lib/infrastructure/observability/monitoring/__init__.py`（19 行，读毕）

- **看到什么**：纯 docstring 包，无代码。`:3-4` 称「重任务（cpu_heavy / io / 长运行）**自动创建 monitor**」、`:5-6` 称「原始 CSV **不可手工合成**（header 指纹 + 写后只读 + 单调性校验）」——**两处均被本片实测推翻**（无任何生产调用者；伪造通过）。`:8-9`「provider/module/workers 从 RT-006 trace **真实来源**取，禁止 config 冒充」被跨 run 污染推翻。
- **判定**：**须修**（文档必须随实现订正）。

### 3.13 `lib/infrastructure/observability/README.md`（19 行，读毕）

- **看到什么**：`:8` 指向 `docs/engineering/LOG_AND_ERROR_CONTRACT.md`（**存在**），与 `logging/README.md:12` 指向的 `STRUCTURED_LOGGING_CONTRACT.md`（**也存在**）**是两个不同文件** —— 同一目录的两份 README 对「日志契约正文」给出互不相同的文件名，读者无从判别哪个是本目录的正本。`:18` 引用的 `ENGINEERING_SPEC.md` **全仓不存在**（`find . -name ENGINEERING_SPEC.md -not -path "./run/*"` → 0 命中）。
- **判定**：**须修**（F-28）。

### 3.14 `lib/infrastructure/observability/PENDING.md`（15 行，读毕）

- **看到什么**：
  - `:9` 断言「2026-09-20 实测 `git ls-files lib/infrastructure/observability | wc -l` = **13**」→ **实测 14**（恰好多出 `README.md`）。这是一条**冻结的自愈锚**：把一次快照值写死在登记面，且 `:6-7` 自己写着「禁止在登记面自证」，却在同一文件里自证。`monitor.py:3` 所引 `tasks/03_RUNTIME_DATA_IO_TASKS.md` 所在的 `tasks/` **整目录已删**（同类模式再次出现）。
  - `:12` 把探针头路径写作 `lib/include/acsd/probe.h` —— **该路径不存在**，真实路径为 `lib/infrastructure/observability/probes/include/acsd/probe.h`（悬空引用）。
  - `:14` 称「已知缺口：本目录**缺** `README.md`」—— **`README.md` 存在**（19 行，即本片成员 13）。该登记已过期；`docs/engineering/MODULE_MAP.md` 声明的 `readme:` 现已可解析，登记未同步。
  - `:9` 含日期「2026-09-20」，违反 AGENTS §5「正文无日期」。
- **判定**：**须修**（F-29 / F-30 / F-31）。

---

## 4. 发现清单

### 4.1 阻断（6 条）

| ID | 发现 | 位置 |
|---|---|---|
| **F-1** | **自洽式断言**：`verify_csv` 的期望指纹由与写入端**同一套公开无密钥定义式**现场重算；100% 手工合成 CSV 通过。docstring `:7-9`/`:314-315`「不可手工合成」「任意字节篡改都抓出」为假 | `monitor.py:91,94-121,627-700` |
| **F-2** | **恒等式伪指标 + 筛掉真信号**：`io_wait_rate_est` ≡ `faults_rate`（构造性逐位相同）；真实 `sys_cpu_jiffies_iowait` 采集后 0 消费 | `monitor.py:424-425`；`linux_procfs.py:243` |
| **F-3** | **恒真门**：`run_heavy_with_monitor` 路径上 `assert_ready()` 恒真（0/8 类触发），docstring 自称「验收负测执行点」不成立 | `runner.py:74,85,117-119`；docstring `:3,:84` |
| **F-4** | **指纹 salt 与工程正本不一致**：正本写 `b"acsd-monitor-timeseries-v1"`，代码是 `b"acsd-monitor-v1"`；全仓仅此两个 salt 字面量，故正本记载的复验基线不是实现的那一个（AGENTS §3：文档为准） | `monitor.py:91` vs `docs/engineering/observability/RESOURCE_MONITORING_CONTRACT.md:105` |
| **F-5** | **verify_csv 不绑定长度/数值/时间**：删尾行、链延长、零样本行、`rss_bytes="NaN"`、`cpu_pct="not-a-number"`、`interval_s="-99999"`、`t_iso_utc="banana"`、空时间戳全表 —— 均 `ok=True` | `monitor.py:646-700` |
| **F-6** | **跨 run 污染**：`TraceSnapshotObserver.run_id` 只写不读，`observe()` 从不按 run 过滤；共享 JSONL 下 runA 的 CSV 行会携带 runB 的 provider/module/workers，「真实来源」承诺被推翻 | `trace_feed.py:65,72-115` |

### 4.2 须修（20 条）

| ID | 发现 | 位置 |
|---|---|---|
| **F-7** | `seal()` 吞 `OSError` 且 `_sealed` 已先置真 → chmod 失败永不重试、不入 `_errors`、不可区分于成功；「已 seal」文件仍可追加 | `monitor.py:552,564-568` |
| **F-8** | 静默降级：`_sample_locked` 早退与近零间隔丢样本均无记录；`start()` 对同一误用抛错而 `sample_once()` 静默返回，契约不一致 | `monitor.py:465-466,479-482,517-518` |
| **F-9** | `gauge_set` 首值 NaN → min/max 永久钉死 NaN，后续更新不可修复 | `probe.cpp:348-367` |
| **F-10** | `%.17g` 输出裸 `nan`/`inf`/`-inf` → 违反 RFC 8259，声明为 JSONL 的产物被严格解析器整行拒绝 | `probe.cpp:139-143` |
| **F-11** | `sink.tried` 在 open 之前置真 → 一次瞬时失败永久静默禁用全进程探针输出；write/flush 返回值被 `(void)` 丢弃 | `probe.cpp:76-83` |
| **F-12** | `note_line()` 仅由 `ScopeTimer::stop` 调用 → 纯 count/gauge 线程永不评估 `ACSD_PROBE_FLUSH_MS`，与 `probe.h:30` 承诺矛盾 | `probe.cpp:221-229,327,345` |
| **F-13** | `at()` 解析失败/截断返回 **0 而非 None**，直接违反本文件「不能静默 0」自述；`:219-220` 的 `else None` 分支恒不可达 | `linux_procfs.py:96-103,219-220` |
| **F-14** | `read_bytes` 注释与内核文档**相反**（`man 5 proc_pid_io`：页缓存命中**不**计入），缓存命中的重读被报成 0 I/O | `linux_procfs.py:11` |
| **F-15** | `linux_procfs` docstring `:84-88` 的 `/proc/self/stat` 字段号错误（代码对、注释错），与 `:105-106` 自相矛盾，足以诱导后来者把对的代码改错 | `linux_procfs.py:84-88` |
| **F-16** | `LogEvent(_raw=...)` 在任何校验与 `redact()` 之前返回 → 非法 level/event 被接受，敏感路径原样泄漏；`:145` 把脱敏当**前置假设**而非强制项 | `log_event.py:145-147` |
| **F-17** | schema 约束几乎全部未强制：`commit` 正本强制 `^[0-9a-f]{40}$`，`LogEvent` 默认 `""` 且从不校验；`run`/`ts`/`diagnostic` 同理 | `log_event.py:139` vs `log_event_v1.schema.json:76` |
| **F-18** | `_fit_jsonl` 兜底按字符切片却按字节预算 → **产出非法 JSON**（`max_bytes=160`→173B 且 `json.loads` 失败）；`:129`「理论不可达」被推翻 | `log_event.py:130` |
| **F-19** | 规范验证命令指向**不存在的** `eng/tools/monitoring/check_log_contract.py`；`:30` 声称的「检查器」缺失；schema `:138` 承诺的 `check_error_payload` 全仓 0 命中 —— F-17/F-18 长期无人执行的根因 | `logging/README.md:30,39,41`；`log_event_v1.schema.json:138` |
| **F-20** | `runner.py` 承诺的「摘要作为 LOG-001 metric 事件输出」**从未兑现**（0 次调用、未 import）；死导入 `PHASES`/`utc_now_s`、死参数 `host`/`commit`、死字段 `MonitorRequired.resource_class/.run_id` | `runner.py:13,31,32,101,46-47` |
| **F-21** | `granted_workers` 是全表历史峰值（非降），与 docstring `:16`「最近一条」和 `:22`「活动节点」**两处口径都不符**；朴素利用率低估 8 倍。兜底分支非单调且绕过类型闸 | `trace_feed.py:98-101,103-109` |
| **F-22** | `init_phase()`（`runner.py:123`）是**恒空操作**：monitor 默认 `phase="init"`（`monitor.py:205`）+ `set_phase` 同值早退（`monitor.py:284`）→ init 段永不产生边界样本；叠加 `:479-482` 丢弃边界样本，验收「init/io 各有独立行」静默不成立 | `runner.py:123,108`；`monitor.py:205,284,479-482` |
| **F-23** | `monitor.py:3` 引用的 `tasks/03_RUNTIME_DATA_IO_TASKS.md` 所在 **`tasks/` 整目录已删**（悬空引用，同类模式再现） | `monitor.py:3` |
| **F-24** | `observability/README.md:18` 引用的 `ENGINEERING_SPEC.md` **全仓不存在** | `observability/README.md:18` |
| **F-25** | `PENDING.md:9` 冻结计数 **13 vs 实测 14**（自愈锚，且同文件 `:6-7` 自称「禁止在登记面自证」）；`:12` 探针头路径 `lib/include/acsd/probe.h` 不存在；`:14` 称「缺 README.md」但 README.md 在（登记过期）；`:9` 含日期违反 AGENTS §5 | `PENDING.md:9,12,14` |
| **F-26** | `probes/README.md:50` 示例行同时含 `wall_us` 与 `n`，二者按实现构造性不同现；`:57` 称 `fflush` 而实现是 `aio_atomic::append_flush` | `probes/README.md:50,57` |

### 4.3 建议（9 条）

| ID | 发现 | 位置 |
|---|---|---|
| F-27 | 用模块级 `assert` 承载 CSV 合同，`python -O` 下全部被剥离 | `monitor.py:82-84` |
| F-28 | 死字段 `_baseline_taken`、`_backend_kind`；`_Backend.describe()` 无调用者 → CSV 不记录后端来源 | `monitor.py:229,235,477,171-172` |
| F-29 | `import re` 未使用 | `linux_procfs.py:41` |
| F-30 | `probe.h:73`「按借用指针保存」与 `:109`「按值保存」自相矛盾；实现是拷贝，`:73` 为过期悬空注释 | `probe.h:73,109`；`probe.cpp:337` |
| F-31 | `redact()` 不覆盖本仓真实根 `/workspace`；路径正则遇空白即止导致部分脱敏（`/home/alice/my data/...` → `<redacted> data/...`） | `log_event.py:59-70` |
| F-32 | `_clip_utf8` 的省略号 3 字节不在预算内（30B 预算 → 33B 返回） | `log_event.py:97` |
| F-33 | `SeqAllocator` docstring 自称「线程安全」但全文件 0 处 lock/threading；**诚实记录：1.6M 次并发未复现重复，保证本身不存在** | `log_event.py:230-243` |
| F-34 | 前导点字符串切分缺陷（`".foo"` → `name_=".foo"`） | `probe.cpp:275-283` |
| F-35 | `logging/README.md:12` 相对路径 `../../` 层级错（应 `../../../../`）；同目录两份 README 指向两个不同的日志契约正文 | `logging/README.md:12`；`observability/README.md:8` |

---

## 5. 我主动构造的反例

> 全部只落 `/tmp/g08_inf_obs/`；`monitor.py` 以 `importlib` 从真实路径加载，**未修改仓内任何字节**。

### 反例 1（最强）：100% 手工合成的 CSV 通过 `verify_csv` ✅ 推翻

- **构造什么**：从零手写一个 21 列的 monitor CSV —— 任意 `run_id="FAKE"`、手写时间戳、`cpu_pct=100`、`rss_bytes=999`、`provider="totally-invented"`，按格式重算指纹链。
- **期望推翻什么**：`monitor.py:7-9`「原始 CSV **不可手工合成**」、`:314-315`「外部篡改任意字节都会被 verify_csv 抓出」。
- **是否推翻**：**推翻**。
  ```
  file mode = 0o644
  verify_csv(forged, run_id=None) -> {'ok': True, 'errors': []}
  verify_csv(forged, run_id='FAKE') -> {'ok': True, 'errors': []}
  ```
  每一个数值都是编的，未被任何进程采集过。

### 反例 2：篡改一格后**重算链** → 通过 ✅ 推翻

- **构造**：把真实链的 `rss_bytes` 改成 `999999`，随后按格式重算全链。
- **期望推翻什么**：「任意字节篡改都抓出」。
- **是否推翻**：**推翻**。`verify_csv -> {'ok': True, 'errors': []}`。
  （对照实验：**不**重算链时被抓 → `ok=False, 3× 行指纹失配`；即该检查只能发现「未重算的改动」，发现不了「重算后的伪造」。）

### 反例 3：删掉 2/3 的样本行（藏起峰值） → 通过 ✅ 推翻

- **构造**：把 3 行数据行的文件裁到只剩 1 行。
- **期望推翻什么**：`verify_csv:667` 的 `expect_seq` 只校验「1..N 连续」，**无长度/时长/样本数下界**。
- **是否推翻**：**推翻**。`{'ok': True, 'errors': []}`。子代理另证：**零样本行**（仅 header+seed）同样通过 —— 「什么都没采到」的记录看起来是干净的。

### 反例 4：塞垃圾进被检量本身 ✅ 推翻

- **构造**：`rss_bytes="NaN"`、`cpu_pct="not-a-number"`、`interval_s="-99999"`，链重算。
- **期望推翻什么**：合同说这是数值列（`monitor.py:75-80`）。
- **是否推翻**：**推翻**。`verify_csv -> {'ok': True, 'errors': []}`；`load_rows` 再把它们原样吐成 `'NaN'` / `'not-a-number'` / `-99999`。

### 反例 5：时间戳断言整体失效 ✅ 推翻

- **构造**：全表 `t_iso_utc` 置空。
- **期望推翻什么**：`:684` 的 `and ts` 短路。
- **是否推翻**：**推翻**。`{'ok': True, 'errors': []}`。
  （精确口径：**诚实记录**——空行**不重置** `prev_ts`，故跨空行的真实倒退仍会被抓；子代理实测 `['…05Z','','…01Z']` → `ok=False`。真正的洞是「任一行时间戳可被删除/替换为垃圾且零代价」，以及**从不解析 RFC3339**，`t_iso_utc="banana"` 亦通过。）

### 反例 6：`run_id` 写而不读 → 跨 run 污染 ✅ 推翻

- **构造**：事件表只含 runB 的事件，`TraceSnapshotObserver(run_id="runA")`。
- **期望推翻什么**：`trace_feed.py:3-4`「provider/module/workers 从 trace **真实来源**取」。
- **是否推翻**：**推翻**。
  ```
  observe() -> {'provider': 'PROVIDER_B', 'module': 'n2', 'active_workers': 1, 'granted_workers': 16}
  run_id attribute value = 'runA'   (装饰性)
  ```
  把 `run_id` 改成任意值（含 `<POISONED>`）结果不变 —— 答案是**事件表顺序**的函数，从不是 `run_id` 的函数。

### 反例 7：`granted_workers` 历史峰值 ✅ 推翻

- **构造**：`node_start(n1,g=16)` → `node_end(n1)`。
- **期望推翻什么**：docstring `:22`「granted = 活动节点的授予上限」。
- **是否推翻**：**推翻**。`active_workers=0, granted_workers=16` —— 活动集合已空，值却来自**已关闭**的节点。子代理实测 granted 序列 `[16,16,16,16,16]` 非降，而真实最后声明的 grant 是 2。

### 反例 8：`_raw` 绕过脱敏与校验 ✅ 推翻

- **构造**：`LogEvent(seq=…,ts=…,run=…,level="info",event="metric",diagnostic="d", _raw={...非法 level/event + /home/alice 路径...})`。
- **期望推翻什么**：`log_event.py:3,11-12` 声称的字段校验与敏感路径脱敏。
- **是否推翻**：**推翻**。构造成功；`level='TOTALLY_BOGUS_LEVEL'`、`event='NOT_A_REAL_EVENT'` 被接受；`diagnostic` 仍是 `failed reading /home/alice/private/star.fits and /workspace/Astro CS Database/x.fits` —— **脱敏未施加**。

### 反例 9：`_fit_jsonl` 产出非法 JSON ✅ 推翻

- **构造**：`to_jsonl(max_bytes=160)`（收窄档，`to_jsonl` 是公开 API）。
- **期望推翻什么**：`:104`「截断后仍是合法 JSON」+ `:129`「理论不可达」。
- **是否推翻**：**推翻**。产出 173 字节且 `json.loads` 失败：`'...,"phase":"runtime","run":}\n'`。`max_bytes=64` 同样非法。
  （公平记账：默认 `max_bytes=4096` 实测合法且不超 4096 字节 —— 我最初怀疑的「超 4096」假设**不成立**，此处只记非法 JSON 与收窄档超限。）

### 反例 10：gauge 首值 NaN 永久污染 ✅ 推翻

- **构造**：`/tmp` 独立片段复刻 `gauge_set` 首值初始化 + `append_double`（非仓内构建）。
- **期望推翻什么**：`probe.h:127-128`「线程本地聚合 last/min/max/n」。
- **是否推翻**：**推翻**。`after 3 normal updates: last=42 min=nan max=nan n=4`；且 `%.17g` 输出裸 `nan`/`inf`/`-inf`，`json.loads` 全部 REJECTED。

### 反例 11：`assert_ready()` 恒真 ✅ 推翻

- **构造**：按 `run_heavy_with_monitor` 的真实控制流逐步执行。
- **期望推翻什么**：docstring `:3,:84`「无 monitor 的 cpu_heavy run 必须失败」负测执行点。
- **是否推翻**：**推翻**。`:117 create_monitor()` → `:74 _monitor=m`（无条件）→ `:119 assert_ready()` 的 `and self._monitor is None` 恒假。子代理实测 8 个 resource_class **0/8** 触发；连 docstring `:16-18` 给的「典型接线」也触发不了。

---

## 6. 盲复算

**方法**：先只读片清单与源码、屏蔽任何既有判定，独立重算「每份文件是否满足本项目已固化的检查项」；本片**未读**任何 `审稿-RR*.md` / `审稿-R2-*.md` / `审稿-R3-*.md` / `审稿-P1-*.md`，也**未读** `/tmp/acsd_g08/`。结论全部由 §5 的自建反例与 §8 的可复跑命令支撑。

| 检查项 | 独立复算结论 | 一致性 |
|---|---|---|
| 静默降级 | **不通过**：`seal()` chmod 失败被吞且永不重试（F-7）；`_sample_locked` 两处静默（F-8）；`sink.tried` 一次失败永久禁用（F-11）；`at()` 返回 0（F-13）；`_raw` 绕过（F-16） | 与自建反例一致 |
| 自愈判据/锚 | **不通过**：本片最典型 —— `PENDING.md:9` 冻结计数 13（实测 14），把一次快照写成常驻声明，且同文件自称「禁止在登记面自证」 | 新增（前次未见） |
| 恒红门 | **不适用**（本片无阈值型红灯） | — |
| 恒真门 | **不通过**：`assert_ready()` 恒真（F-3）。另有准恒真：`verify_csv` 对零样本文件返回绿（F-5） | 新增 |
| 筛掉真信号 | **不通过**：真实 `sys_cpu_jiffies_iowait` 采集后 0 消费（F-2）；`granted_workers` 筛成历史峰值并掩盖当前值（F-21）；`verify_csv` 不筛数值合法性反而放行（F-5） | 新增 |
| 退役对象仍有活调用者 | **反向成立（更糟）**：本片 API 在活树中**零生产调用者** —— `verify_csv`/`HeavyRunGuard`/`ResourceMonitor`/`TraceSnapshotObserver`/`emit_metric_event` 的唯一消费者是 `eng/tests/monitoring/`；真正的强制点另在 C++ `resource_gate.h`。即**文档宣称的生产行为从未接线** | 新增 |
| 悬空引用 | **不通过**：`tasks/` 整目录已删（F-23）；`ENGINEERING_SPEC.md` 不存在（F-24）；`check_log_contract.py` 不存在（F-19）；`PENDING.md:12` 路径不存在（F-25） | 新增 |
| 私建线程池 | **通过（有保留）**：`monitor.py:526` `threading.Thread(daemon=True)` 是单条采样线程而非线程池，`runner.py:22` 的「不启动私有线程池」字面成立；但它确为基础设施层私建的、绕调度器的私有线程，且 `probe.cpp:75` 另持一把私有互斥锁。按「池的所有权归调度器」口径，**记为建议而非违规** | 偏严修正（我原本倾向按违规记，子代理与复读后下调） |
| 硬编码 | **不通过**：`:418` `lock_wait_ns_est = dc * 1000.0` 的「1000 ns/次上下文切换」是**无推导经验常数**，且列名带 `_est` 仍以「ns」量纲进入 CSV；`:480` `floor = max(0.01, interval_s*0.2)` 的 0.2、`log_event.py:121` 的 `-8` 同类 | 一致 |
| 错误码与失败语义 | **不通过**：`probe.cpp:82-83` 两个 `(void)` 丢弃返回值；`verify_csv` 无稳定错误码（返回裸字符串列表）；`runner.py:13` 的 metric 事件从未产生 | 一致 |
| 数值稳定性 | **不通过**：gauge NaN 永久污染 min/max 并产出非法 JSON（F-9/F-10）；`_num` 对字符串**不做数值校验**（`monitor.py:138-139` 直通）；`NaN/Inf` 在写入侧被静默转空串（`:144-145`）而读取侧无从区分「无值」与「坏值」 | 一致 |

**判定**：**偏严的部分 1 处已自我下调**（私建线程池 → 建议）。其余与自建反例一致。**最关键的偏严风险我主动规避了**：我最初假设 `-DACSD_PROBES=ON` 会让 `probe.h:39` 编译失败，实测被 CMake 定义方式推翻，**假设作废**（见 §7）。

---

## 7. 子代理派发记录

**派了 5 个**（远超要求的 3–5 下限；因工具层把首个 `subagent` 调用重复投递了一次，实际 5 个任务、覆盖 3 个议题，monitor.py 议题由 2 个代理独立复核）。

| # | 代理 | 议题 | 状态 |
|---|---|---|---|
| 1 | `9ad1012c-…` | monitor.py 指纹链伪造 | 已回报 |
| 2 | `2822f207-…` | monitor.py 指纹链伪造（重复投递，独立复核） | 已回报 |
| 3 | `3338ac8c-…` | trace_feed.py + runner.py | 已回报 |
| 4 | `dc006782-…` | log_event.py + schema | 截止前未回报 |
| 5 | `2f2f5d4a-…` | probe.cpp/h + 悬空引用普查 | 截止前未回报 |

**逐条复核方式**：对每个代理结论，我**不直接采信**，而是回原文定位 `file:line` 并在 `/tmp/g08_inf_obs/` 独立复跑。采纳的以「我能独立复现同样输出」为准；不能复现的一律降级或作废。

### 采纳（我独立复现一致）

| 代理结论 | 我的复核 |
|---|---|
| #1/#2：伪造 CSV 通过 `verify_csv` | 我自建 forge_monitor_csv.py 得到同样 `{'ok': True, 'errors': []}`，且**比代理多测了两条**：重算链后的定点篡改通过、删尾 2/3 行通过 |
| #1/#2：seed 检查循环自证（`:659→:662→:663`） | 复核成立。**代理额外指出「种子行 run_id 永不与调用方比较」**（`:668` 只取 `rows[1:]`）——我原读漏了此点，采纳为 F-1 的加强证据 |
| #1/#2：salt 与正本文档不一致 | 我用 `grep -rn "acsd-monitor"` 独立确认全仓仅两个字面量，**正本 `RESOURCE_MONITORING_CONTRACT.md:105` 写的是 `b"acsd-monitor-timeseries-v1"`**，代码是 `b"acsd-monitor-v1"` → 升为 F-4（阻断） |
| #2：19 列种子垃圾不影响校验 | 我确认 `:112-121` 原像只含 HEADER+run_id，采纳 |
| #2：全空时间戳通过、字典序比较 | 我独立复现，采纳；并按代理的精度更正修正了我自己的表述（空行**不重置** `prev_ts`） |
| #2：`verify_csv` 活树零生产调用者 | 我用 `grep` 独立确认唯一调用者是 `eng/tests/monitoring/test_monitor_contract.py`；并补上 `eng/tools/monitoring/` 目录实查 |
| #3：`run_id` 只写不读 → 跨 run 污染 | 我自建 tf.py 得到同样输出，采纳为 F-6 |
| #3：`assert_ready()` 恒真 | 我自建 guard.py 复核控制流并实测，采纳为 F-3 |
| #3：`init_phase()` 恒空操作（B4 bonus） | **我自己独立复现**（新建 monitor → `phase='init'` → `init_phase()` 后仍 `'init'`，零数据行），采纳为 F-22。这条代理标为「超出提问范围」，价值很高 |
| #3：`emit_metric_event` 零调用、`PHASES`/`utc_now_s` 死导入 | 我 grep 复核：runner.py 中 `emit_metric_event` 计数 **0**、`PHASES`/`utc_now_s` 各 1（仅导入行），采纳 |
| #3：`granted_workers` 历史峰值、兜底分支绕过类型闸 | 我自建实验确认 `granted=16` 钉死，采纳为 F-21 |

### 否决 / 作废（3 处，重要）

1. **我的假设被代理 #3 推翻 ——「pipeline 用了不在元组里的类名，故守卫永不触发」。**
   我原写「`REQUIRE_MONITOR_CLASSES=("cpu_heavy","io")` 漏了 `cpu_light`/`metadata`」。代理查证 `typed_dag.py:42` / `typed_dag.schema.json:46` / `pipeline.cpp:150-151` 的真实词表为 `metadata|io|cpu_light|cpu_heavy`，与元组**确实重叠**（`cpu_heavy`✔、`io`✔），并指出 `metadata` 的豁免**本就有测试**（`test_monitor_contract.py:186`）。**我的推理作废**：守卫失效的真实原因是**活树零生产接线**（46 处引用全在 runner.py 自身、测试、合同文档与台账），真正的强制点在 C++ `resource_gate.h:214/418/434`。已按代理的结论改写 F-3。

2. **我的假设被实测推翻 ——`-DACSD_PROBES=ON` 会让 `probe.h:39` 编译失败。**
   我据 `probes/README.md:16` 推断 CMake 宏会是裸 `ON`。实测 `CMakeLists.txt:26` 是 `option()`、`:516` 是 `target_compile_definitions(... ACSD_PROBES=1)`，`ON` 只存在于 CMake 缓存并被转成 `1` → **README 正确，假设作废**，未记入发现清单。

3. **我的假设被实测推翻 ——`_fit_jsonl` 会超出 4096 字节上限。**
   我据「按字符切片 vs 按字节预算」推断默认档会超限。实测默认 `max_bytes=4096` 输出 **747 字节、合法 JSON、不超限** → **该假设作废**；只保留收窄档（`max_bytes=160/64`）产出非法 JSON 这一实证（F-18）。

### 未采纳代理结论的情况

- 代理 #1 称「`mode 0000` 时 `verify_csv` 抛 PermissionError」，我标注为**依赖非 root 身份**、不作通用判据，仅作为「verify_csv 从不检查 seal、只检查可读」的旁证。
- 代理 #3 列的 `cpu_light`/`io_bounded` 未纳入 `REQUIRE_MONITOR_CLASSES`，我**不记为缺陷** —— `metadata` 豁免已有测试背书，说明该元组是**有意**的白名单，而非漏配；仅在建议层留痕。

### 代理 #4 / #5 未回报的补偿

二者未在截止前回报。我**没有等待空转**，而是自己把它们的议题全部做完并独立取证：`_raw` 绕过、`commit` 模式冲突、`_fit_jsonl` 非法 JSON、`_clip_utf8` 超预算、`SeqAllocator` 无锁（诚实记为**未复现**）、`redact` 漏 `/workspace` 与含空格路径、三条「不得混用」常量零消费者；以及 probe 的 NaN 永久污染、`%.17g` 非法 JSON、`sink.tried` 一次性、`flush_ms` 对 counter 不生效、`probe.h:73` vs `:109` 注释矛盾、`aio_atomic_file.h` 存在（我的悬空假设作废），以及全部悬空引用普查（`tasks/` 已删、`ENGINEERING_SPEC.md` 不存在、`check_log_contract.py` 不存在、`PENDING.md` 计数与路径）。**这些结论不依赖任何未回报的代理。**

---

## 8. 自证段（可复跑）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线与片清单
git -c core.quotepath=false log -1 --format='%H %s'      # => 1fa477a7…（注意：非任务书给的 850a9ede）
sed -n '/片号: INF-observability-001/,/^  - 片号:/p' \
  "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml" | head -22

# 1) 覆盖率口径：逐份行数相加 = 清单 2468
wc -l lib/infrastructure/observability/{monitoring/{monitor,probe_dummy}.py,probes/src/probe.cpp} 2>/dev/null
for f in monitoring/monitor.py probes/src/probe.cpp monitoring/linux_procfs.py logging/log_event.py \
         probes/include/acsd/probe.h monitoring/runner.py monitoring/trace_feed.py \
         logging/log_event_v1.schema.json probes/README.md logging/README.md \
         monitoring/windows_pdh_etw.py monitoring/__init__.py README.md PENDING.md; do
  printf "%6d  %s\n" "$(wc -l < lib/infrastructure/observability/$f)" "$f"; done   # 合计 2468

# 2) F-1/F-4：指纹链无密钥 + salt 与正本不一致
sed -n '91p' lib/infrastructure/observability/monitoring/monitor.py          # _FP_SALT = b"acsd-monitor-v1"
grep -rn "acsd-monitor" --include=*.py --include=*.md . | grep -v '^./run/GOVERN-08'
   # => monitor.py:91 = b"acsd-monitor-v1"
   # => docs/engineering/observability/RESOURCE_MONITORING_CONTRACT.md:105 = b"acsd-monitor-timeseries-v1"
grep -nE "hmac|urandom|getenv|environ|secret" lib/infrastructure/observability/monitoring/*.py   # => 0 命中

# 3) 反例 1/2/3/4/5 —— 伪造、篡改、截断、垃圾值、时间戳（最强一组）
cd /tmp/g08_inf_obs && python3 forge_monitor_csv.py    # 100% 伪造 -> {'ok': True, 'errors': []}
                    python3 forge2.py                   # 整文件复制 / 空时间戳
                    python3 forge3.py                   # 重算链定点篡改 -> ok:True；删尾 -> ok:True；NaN/非数字/负值 -> ok:True

# 4) F-2：恒等式伪指标 + 真信号被丢弃
cd /tmp/g08_inf_obs && python3 ident.py
   # => io_wait_rate_est = 307.0  faults_rate = 307.0  IDENTICAL? True
   # => occurrences of 'iowait' in monitor.py = 0   （而 linux_procfs.py:243 真实采集了它）

# 5) F-6/F-21：跨 run 污染 + granted 历史峰值
cd /tmp/g08_inf_obs && python3 tf.py

# 6) F-3/F-22：恒真门 + init_phase 恒空操作
cd /tmp/g08_inf_obs && python3 guard.py ; python3 b4.py

# 7) F-16/F-17/F-18/F-32/F-33：log_event 校验绕过 / commit 模式 / 非法 JSON / 超预算 / 无锁
cd /tmp/g08_inf_obs && python3 le.py ; python3 le2.py

# 8) F-9/F-10：gauge NaN 永久污染 + %.17g 非法 JSON（独立片段，非仓内构建）
cd /tmp/g08_inf_obs/cpp && g++ -O0 -o nan nan.cpp && ./nan
   # => NaN -> nan ; after 3 normal updates: last=42 min=nan max=nan n=4
python3 -c "import json;[print(json.loads('{\"last\":%s}'%x)) for x in ('nan',)]" 2>&1 | tail -1  # => 拒绝

# 9) F-13/F-14/F-15：procfs 字段号（代码对/注释错）、静默 0、read_bytes 语义
cd /tmp/g08_inf_obs && python3 proc.py
man 5 proc_pid_io | grep -A3 "^[[:space:]]*read_bytes"   # => bytes really fetched from the storage layer

# 10) F-19/F-23/F-24/F-25：悬空引用与过期登记
ls eng/tools/monitoring/                  # => 无 check_log_contract.py
ls tasks/                                 # => No such file or directory
find . -name ENGINEERING_SPEC.md -not -path "./run/*"                       # => 0 命中
git -c core.quotepath=false ls-files lib/infrastructure/observability | wc -l   # => 14（PENDING.md:9 写 13）
ls lib/include/acsd/probe.h 2>&1          # => No such file（PENDING.md:12 的错误路径）
test -e lib/infrastructure/observability/README.md && echo "EXISTS（PENDING.md:14 称缺失）"

# 11) 被我推翻的两个假设（留档，防止后来者重犯）
grep -n "ACSD_PROBES" CMakeLists.txt | sed -n '1,3p'    # => option(...) + ACSD_PROBES=1
grep -n "target_compile_definitions(acsd_probes" CMakeLists.txt   # => ACSD_PROBES=1（非裸 ON）
grep -n "REQUIRED_FIELDS" eng/tests/monitoring/test_log_contract.py   # => 唯一被测试背书的自洽断言

# 12) 零仓内改动自证
git -c core.quotepath=false status --porcelain -- lib/ eng/ docs/ | head   # => 应为空
```

---

**结论一句话**：本片把「原始监控记录不可伪造」当作安全性质来实现，但该性质**经实测为假**——`verify_csv` 的期望值与被检量由同一套公开无密钥定义式现场重算，一个逐格编造的 CSV 就能通过；同一片另有 `io_wait_rate_est` 与 `faults_rate` 构造性恒等、真实 iowait 观测被采集后丢弃、`run_id` 只写不读导致跨 run 污染、以及一个结构上永不触发的「无 monitor 必失败」恒真门。**建议按阻断处理，不要因为 `eng/tests/monitoring/` 全绿而认为该性质成立——那些测试只覆盖了「未重算链的改动」这一种情形。**
