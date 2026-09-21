# AstroCS CLI 协议合同 v1（API-002 冻结）

> 上游：ASTROCS_DESIGN.md §7.1（命令树）、§7.2（配置、事件与退出码）

> ID: API-CLI-001  状态: FROZEN  上游: API-001/ARCH-002  下游: CLI-001/002/003, API-003..005(handler 追溯), BENCH-005
> 命令树 = `ASTROCS_DESIGN.md` §6.1/§6.2 的**唯一命令树**：用户命令只有
> normalize/mosaic/export + help/--version/doctor/benchmark；`phase1|2|3` 用户命令与
> 别名（含 config */modules */selftest/test synthetic/verify*/drizzle/benchmark cpu|
> verify-profile/hardware inspect）不在命令面上，调用返回 rc=2（phase 仅为内部指代，§6.2）。

## 1 命令树(ASTROCS_DESIGN §6.2 唯一命令树;help 文本 golden 由此生成)

```text
astrocs --version [--json]
astrocs normalize (--json <config.json> | --template [-o <path>] | --help)
astrocs mosaic (--json <config.json> | --template [-o <path>] | --help)
astrocs export (--json <config.json> | --template [-o <path>] | --help)
astrocs help
astrocs doctor [--json]
astrocs benchmark
```

命令语义（§6.1 薄入口）：
- `--json <config.json>` 运行：运行前预检三档（🟢 correct / 🟠 warn 不阻塞 / 🔴 error 阻塞）
  → 显示检查页面 → 存在 error 时阻断运行（`-y`/`-yes` 不能越过）→ 无 error 时（correct 与 warn）
  都需用户输入 `yes` 确认（`-y`/`-yes` 跳过确认）→ `-force` 跳过整个检查步骤直接运行
  → 执行并落产品 + manifest；
- `--template [-o <path>]` 生成可直接改的完整 JSON 模板（缺 `-o` → stdout）；
- `--help` 子命令帮助与字段说明；
- 三个命令**平级独立**：各自独立进程、**独立重跑**（**无断点续算**：重跑 = 新运行目录 + 新 manifest）、独立验收，**禁止**隐式串接（`ASTROCS_DESIGN.md` §1.2:78）；
- `benchmark` 生成/更新**安装目录** cpu_profile（后续运行自动读取）。

handler→内部会话 API 追溯(phase 为内部指代): normalize→API-003(会话1)；mosaic→API-004(会话2)；export→API-005(会话3)；benchmark→BENCH-001..004 harness(内部)。

## 2 退出码(全 11 条冻结,唯一源 `lib/infrastructure/cli/exit_codes.h`)

0 成功且门禁全过 / 2 CLI 参数或配置错 / 3 输入缺失格式 hash 错 / 4 科学验证或不变量失败 / 5 backend ABI 签名 CPU 特征或加载失败 / 6 计算执行失败 / 7 I/O 失败 / 8 输出完整性验证失败 / 9 用户取消或超时 / **10 磁盘写满 / 写盘失败**（`ASTROCS_DESIGN.md` §6.3：内存 / CPU / 线程不设门）/ 70 未分类内部错误(必须出脱敏 crash report)。跨平台同失败同码(golden 双平台断言)。

码值与含义**只有一份**，以 `lib/infrastructure/cli/exit_codes.h` 为唯一源。

## 3 stdout/stderr 纪律

- 人类模式: stdout=简洁结果, stderr=日志/诊断;`--json`: stdout 恰一个 JSON 文档;运行事件流: stdout 每行一个 UTF-8 JSON 事件,禁夹普通文字;JSON 路径全 UTF-8(Windows 内部 Unicode 路径正确处理)。
- **事件流 = 默认输出**（`ASTROCS_DESIGN.md` §6.3）：**不需要旗标开启**；GUI 用其它语言**直接捕获 CLI 输出**。`--events-jsonl` **保留接受**，语义**等价默认行为**（别名，`lib/infrastructure/cli/commands.cpp:2115`、`command_tree.h:43`）——**不得**把它当作开启事件流的必要条件。
- **stdout 无日志污染**为机器测试项(CLI-002 golden)。

## 4 JSONL 运行事件流 v1（**唯一 schema**）

> **唯一性声明（GAP_AUDIT §4.3）**：运行事件流的**唯一 schema** = 实现正本
> `lib/infrastructure/cli/protocol.h`（`ValidateEventV1`，发送侧硬闸）+ `lib/infrastructure/cli/jsonl.h`（`JsonlEmitter`）。
> 本节是它的**人类可读合同**（同源；字段名 / 枚举 / 顺序键以 `protocol.h` + `jsonl.h` 为准，
> 冲突时以实现正本为准）。机器 schema = `eng/contracts/schemas/jsonl_event_v1.schema.json`（**派生件**，
> 不得自成第二份定义）。
> **不得与结构化日志混用**：`docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md`（LOG-001，
> `astrocs.log.event.v1`）是**结构化日志**合同，**显式声明它不是运行事件流**；其事件键名 `event`
> 与本流的 `kind` **不得混用**，两份流各用**不同工件名**、不得互相冒充。

- 每行必含: `schema_version,event_id,run_id,timestamp_utc,sequence,kind,severity,phase,stage,message`;`sequence` 从 0 单调递增。
- kind 扩展字段: progress{completed,total,unit,rate,eta_seconds} / resource{cpu_cores_used,rss_bytes,io_read_bytes,io_write_bytes,threads} / artifact{role,path,sha256,size_bytes,integrity_sha256,canonical_sha256,canonical_hash_spec,canonical_format}（**DET-001**：sha256=整文件字节摘要(完整性)，canonical_sha256=规范产品哈希(像素数据+科学元数据，排除易变卡/键；口径 spec=astrocs.canonical-product-hash/v1，见 eng/tools/canonical_product_hash.py --spec)；可复现性判据用 canonical_sha256，不得用 sha256） / backend{kernel,backend_id,isa,workers,block_size,reason} / final{exit_code,status,run_manifest,summary}。
- 重计算 stage 必发 `stage_start/stage_end`+实际 backend 事件;GUI/未来客户端只消费本协议(禁链接科学库绕过 CLI)。
- schema: `eng/contracts/schemas/jsonl_event_v1.schema.json`(CLI-002 golden 用;**派生件**，不得自成第二份定义)。

## 5 取消与崩溃

- Ctrl-C/Windows console cancel→协作取消令牌(acs_cancel, API-001 §2);内核在 ALG 5c 冻结的安全点检查。
- 取消后: 关 writer→写 incomplete manifest→删除/隔离临时产物→exit 9;**不得留下看似完整的 HiPS/结果**(与 ARCH-002 §5/ARCH-005 §3 原子单元一致)。
- 未捕获异常→70+run_id/阶段/最小脱敏 crash report(不泄露凭据)。

## 6 机器化一致性检查器合同(API-002 建立 `eng/tools/check_cli_protocol.py`)

1. `--help` golden 树与 §1 逐行一致;
2. JSON/JSONL 样例对 schema 有效(jsonschema 或 stdlib 等价校验);
3. 退出码常量唯一源(`lib/infrastructure/cli/exit_codes.h`, grep 无第二处数值表);
4. handler→Phase API 追溯表存在且逐行有 API id;
5. 发布 manifest 不含 `phase1|2|3` 可执行文件(与 PRODUCTION_EXECUTION_INVENTORY production exe=0 联动, CLI-001 后=恰一 astrocs);
6. 双平台 golden command tests 字段+退出码一致(Windows 侧 Fatduck 执行)。

1–5 为 Linux 可验;6 属 WIN/FAT 域任务。

## 7 配置与 `output_dir`(权威 = `ASTROCS_DESIGN.md` §6.3 配置/退出码 + `ENGINEERING_SPEC.md` §7 目录规范)

运行产物(每相 run manifest `astrocs_run_*.json`、资源三件套
`resource_timeseries.csv` / `resource_summary.json` / `worker_balance.csv`、
`alloc_samples.csv` / `alloc_report.json`、节点科学产物)**只落 `output_dir`**;

> **资源时序工件的唯一列合同（GAP_AUDIT §4.2）**：
> - **唯一列合同 = 生产实现** `lib/infrastructure/cli/resource_recorder.h:260-266`：**20 列**
>   `elapsed_seconds,stage,cpu_pct,system_cpu_pct,active_workers,runnable_workers,rss_bytes,pss_bytes,commit_bytes,page_faults,read_bytes,write_bytes,queue_depth,lock_wait_ns,progress,threads,active_compute_threads,per_thread_cpu_max_pct,per_thread_cpu_sum_pct,io_wait_pct`
>   （**run 收尾一次性落盘**，被 manifest / 目录树哈希覆盖）；`lib/infrastructure/cli/resource_events.h:6` 明文
>   「资源时序曲线的**唯一载体** = 磁盘工件 `resource_timeseries.csv`」。
> - `docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md`（LOG-002）的「每秒采样 + seed 行 + 指纹链」
>   CSV 是**监控伴随器的原始数据**，**不是同一工件** ⇒ 其工件名固定为 **`monitor_timeseries.csv`**。
> - **两工件不同名、不互替**：`resource_timeseries.csv`（20 列，收尾一次性）与 `monitor_timeseries.csv`
>   （每秒采样 + 指纹链）**不得**互相冒充、**不得**共用一套列定义或采样语义。
CLI 不得以进程 CWD(`"."`)作为隐式缺省写出,否则在工作区根散落产物并触发
UT-CLI `mutates_workspace=false` 的 dirty 判定。

1. **必填**:`normalize|mosaic|export --json <config.json>` 的运行配置(CLI-001 唯一命令树;`phase1|2|3` 的 `run`/`plan`/`validate`/`inspect` 用户命令不在命令面上,调用返回 rc=2,见 §1 —— 独立 `validate`/`plan`/`inspect` 命令面无载体,运行前预检由 `ASTROCS_DESIGN.md` §4.5 三档页面 + `-y`/`-force` 承接,运行计划由产物 `run-plan.json`/`run-graph.json` 承接),
   无论 V1 顶层形态(`inputs`)、平铺会话形态(`input_lights`/`hips_paths`/
   `phase3`)还是 **normalize 多数据块形态**(`blocks[]`),
   都必须显式给出 **非空字符串** `output_dir`。
2. **缺失/非串/空串**:平铺会话形态与多数据块形态 → 配置错 `exit 2`(禁 silent default);
   V1 顶层形态 → `exit 3`(见 `lib/infrastructure/cli/parser.cpp` `validate_config_full`)。
3. **存在性**:V1 顶层形态要求 `output_dir` 目录已存在(`exit 3` if not found);
   平铺会话形态由 session/节点自建输出目录,不要求预先存在。
   **多数据块形态**:`output_dir` 是**块级**必填(每块一个,块间不得重复 —— 否则两块会写同一份
   run manifest);块 = 一次运行,CLI 逐块派发(独立 manifest / 独立 `run_context.json`)。
4. **取消路径**:SIGINT 后的 `incomplete` manifest 也写 `output_dir`(不写 CWD `"."`)。
5. 回归锚: `eng/tests/cli/test_cli001_vpi.py`、`eng/tests/cli/test_phase123_pipeline.py`
   负例矩阵的 `neg: missing output_dir` + `no CWD residue` 两条。
