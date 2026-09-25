# Astro Celestial Sphere Database（ACSD） CLI 协议合同 v1（API-002 冻结）

> 上游：ASTROCS_DESIGN.md §7.1（命令树）、§7.2（配置、事件与退出码）

> ID: API-CLI-001  状态: FROZEN  上游: API-001/ARCH-002  下游: CLI-001/002/003, API-003..005(handler 追溯), BENCH-005
> 命令树 = `ASTROCS_DESIGN.md` §7.1 的**唯一命令树**：用户命令只有
> normalize/mosaic/export + help/--version/doctor/benchmark；`phase1|2|3` 用户命令与
> 别名（含 config */modules */selftest/test synthetic/verify*/drizzle/benchmark cpu|
> verify-profile/hardware inspect）不在命令面上，调用返回 rc=2（phase 仅为内部指代，§7.1）。

## 1 命令树(ASTROCS_DESIGN §7.1 唯一命令树;help 文本 golden 由此生成)

```text
acsd --version [--json]
acsd normalize (--json <config.json> | --template [-o <path>] | --help)
acsd mosaic (--json <config.json> | --template [-o <path>] | --help)
acsd export (--json <config.json> | --template [-o <path>] | --help)
acsd help
acsd doctor [--json]
acsd benchmark
```

命令语义（§7.1 薄入口）：
- `--json <config.json>` 运行：运行前预检三档（🟢 correct / 🟠 warn 不阻塞 / 🔴 error 阻塞）
  → 显示检查页面 → 存在 error 时阻断运行（`-y`/`-yes` 不能越过）→ 无 error 时（correct 与 warn）
  都需用户输入 `yes` 确认（`-y`/`-yes` 跳过确认）→ `-force` 跳过整个检查步骤直接运行
  → 执行并落产品 + manifest；
- `--template [-o <path>]` 生成可直接改的完整 JSON 模板（缺 `-o` → stdout）；
- `--help` 子命令帮助与字段说明；
- 三个命令**平级独立**：各自独立进程、**独立重跑**（**无断点续算**：重跑 = 新运行目录 + 新 manifest）、独立验收，**串接一律显式**（`ASTROCS_DESIGN.md` §1.2）；
- `benchmark` 生成/更新**安装目录** cpu_profile（后续运行自动读取）。

handler→内部会话 API 追溯(phase 为内部指代): normalize→API-003(会话1)；mosaic→API-004(会话2)；export→API-005(会话3)；benchmark→BENCH-001..004 harness(内部)。

## 2 退出码(全 11 条冻结,唯一源 `lib/infrastructure/cli/exit_codes.h`)

0 成功且门禁全过 / 2 CLI 参数或配置错 / 3 输入缺失格式 hash 错 / 4 科学验证或不变量失败 / 5 backend ABI 签名 CPU 特征或加载失败 / 6 计算执行失败 / 7 I/O 失败 / 8 输出完整性验证失败 / 9 用户取消或超时 / **10 磁盘写满 / 写盘失败**（`ASTROCS_DESIGN.md` §4.5：内存 / CPU / 线程不设门）/ 70 未分类内部错误(必须出脱敏 crash report)。跨平台同失败同码(golden 双平台断言)。

码值与含义**只有一份**，以 `lib/infrastructure/cli/exit_codes.h` 为唯一源。

## 3 stdout/stderr 纪律

- 人类模式: stdout=简洁结果, stderr=日志/诊断;`--json`: stdout 恰一个 JSON 文档;运行事件流: stdout 每行一个 UTF-8 JSON 事件,禁夹普通文字;JSON 路径全 UTF-8(Windows 内部 Unicode 路径正确处理)。
- **事件流 = 默认输出**（`ASTROCS_DESIGN.md` §7.2）：**不需要旗标开启**；GUI 用其它语言**直接捕获 CLI 输出**。`--events-jsonl` **保留接受**，语义**等价默认行为**（别名，`lib/infrastructure/cli/commands.cpp:2115`、`command_tree.h:43`）——**事件流的开启条件 = 默认行为本身**。
- **stdout 无日志污染**为机器测试项(CLI-002 golden)。

## 4 JSONL 运行事件流 v1（**唯一 schema**）

> **唯一性声明（GAP_AUDIT §4.3）**：运行事件流的**唯一 schema** = 实现正本
> `lib/infrastructure/cli/protocol.h`（`ValidateEventV1`，发送侧硬闸）+ `lib/infrastructure/cli/jsonl.h`（`JsonlEmitter`）。
> 本节是它的**人类可读合同**（同源；字段名 / 枚举 / 顺序键以 `protocol.h` + `jsonl.h` 为准，
> 冲突时以实现正本为准）。机器 schema = `eng/contracts/schemas/jsonl_event_v1.schema.json`（**派生件**，
> 定义只有这一份）。
> **与结构化日志分属两份合同**：`docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md`（LOG-001，
> `astrocs.log.event.v1`）是**结构化日志**合同，**显式声明它不是运行事件流**；其事件键名 `event`
> 与本流的 `kind` **各自独立**，两份流各用**不同工件名**、**各自具名**。

- 每行必含: `schema_version,event_id,run_id,timestamp_utc,sequence,kind,severity,phase,stage,message`（正本 `protocol.h::kEventFieldsV1`）;`sequence` 从 0 单调递增。
- **逐 kind 冻结扩展字段集 = 正本唯一**：`lib/infrastructure/cli/protocol.h::missing_required_extension_v1`
  的 `kExt` 表（发送侧 `ValidateEventV1` 按表逐字段硬闸，缺字段即拒发）。**本节不复制该清单**——
  复制即形成第二份定义；字段名 / 枚举 / 顺序键一律回正本读。现行 10 类 kind 的**语义**：
  - `progress`：阶段内进度（完成量/总量/单位/速率/预计剩余）；
  - `resource`：进程资源采样（CPU 核数 / RSS / 读写字节日 / 线程数）；
  - `artifact`：落盘产物登记（角色/路径/整文件字节摘要/字节数）；
  - `backend`：实际后端选择（kernel / backend_id / isa / workers / block_size / reason）；
  - `final`：运行收尾（`exit_code` 必须 ∈ §2 冻结退出码域；status / run_manifest / summary）；
  - `stage_start` / `stage_end`：阶段起止（无扩展字段）；
  - `graph`：运行图落盘（path）；
  - `resource_gate`：资源门判定记录（诊断 / 强制口径 / 工作量下界 / SO-05 签字证据）；
    逐键语义与必含集见 `docs/plugins/infrastructure/21_observability.md` §8.4「事件面登记」；
  - `v6_mode_route`：V6 路由登记（route_kind / token / surface / 来源 / 预算归属）。
  **kind 集合与逐 kind 字段集由机器门 `eng/ci/check_event_field_sets.py`（EVT-FIELD-SETS）守
  五面一致**（正本 kExt / schema `allOf[].then.required` / schema `x-astrocs-event-kind-registry` /
  读侧 CLI-004 / 读侧 FIX208）；本节只给指针与语义，**不重复该判据**。
- **`artifact` 的 DET-001 附加字段不属冻结必含集**：`integrity_sha256` / `canonical_sha256` /
  `canonical_hash_spec` / `canonical_format` 是实现侧附加字段（正本 `kExt` 的 artifact 必含集
  = role/path/sha256/size_bytes 四项）；必含集即该四项。DET-001 判据：
  sha256=整文件字节摘要(完整性)，canonical_sha256=规范产品哈希(像素数据+科学元数据，排除易变卡/键；
  口径 spec=astrocs.canonical-product-hash/v1，见 eng/tools/canonical_product_hash.py --spec)；
  可复现性判据 = canonical_sha256；sha256 只表整文件完整性。
- 重计算 stage 必发 `stage_start/stage_end`+实际 backend 事件;GUI/未来客户端只消费本协议(禁链接科学库绕过 CLI)。
- schema: `eng/contracts/schemas/jsonl_event_v1.schema.json`(CLI-002 golden 用;**派生件**，定义只有正本这一份)。

## 5 取消与崩溃

- Ctrl-C/Windows console cancel→协作取消令牌(acs_cancel, API-001 §2);内核在 ALG 5c 冻结的安全点检查。
- 取消后: 关 writer→写 incomplete manifest→删除/隔离临时产物→exit 9;**取消后的 HiPS/结果一律为 incomplete 形态**(与 ARCH-002 §5/ARCH-005 §3 原子单元一致)。
- 未捕获异常→70+run_id/阶段/最小脱敏 crash report(不泄露凭据)。

## 6 机器化一致性检查器合同(API-002 建立；现行落地 = `eng/tools/check_api_docs.py`)

1. `--help` golden 树与 §1 逐行一致;
2. JSON/JSONL 样例对 schema 有效(jsonschema 或 stdlib 等价校验);
3. 退出码常量唯一源(`lib/infrastructure/cli/exit_codes.h`, grep 无第二处数值表);
4. handler→Phase API 追溯表存在且逐行有 API id;
5. 发布 manifest 不含 `phase1|2|3` 可执行文件(与 PRODUCTION_EXECUTION_INVENTORY 的 exe_target 面同源: 生产面 `classification=production` 的行恰一 = `acsd`, CLI-001);
6. 双平台 golden command tests 字段+退出码一致(Windows 侧 Fatduck 执行)。

1–5 为 Linux 可验;6 属 WIN/FAT 域任务。

## 7 配置与 `output_dir`(权威 = `ASTROCS_DESIGN.md` §7.2 配置/退出码 + `ENGINEERING_SPEC.md` §7 目录规范)

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
>   （每秒采样 + 指纹链）**各自具名、各自独立**：列定义与采样语义各一套。
CLI 的写出位置取自显式配置的目录,进程 CWD(`"."`) 只标识进程自身位置;按 CWD 写出会在工作区根散落产物并触发
UT-CLI `mutates_workspace=false` 的 dirty 判定。

1. **必填**:`normalize|mosaic|export --json <config.json>` 的运行配置(CLI-001 唯一命令树;`phase1|2|3` 的 `run`/`plan`/`validate`/`inspect` 用户命令不在命令面上,调用返回 rc=2,见 §1 —— 独立 `validate`/`plan`/`inspect` 命令面无载体,运行前预检由 `ASTROCS_DESIGN.md` §4.5 三档页面 + `-y`/`-force` 承接,运行计划由产物 `run-plan.json`/`run-graph.json` 承接),
   无论 V1 顶层形态(`inputs`)、平铺会话形态(`input_lights`/`hips_paths`/
   `phase3`)还是 **normalize 多数据块形态**(`blocks[]`),
   都必须显式给出 **非空字符串** `output_dir`。
2. **缺失/非串/空串**:平铺会话形态与多数据块形态 → 配置错 `exit 2`(禁 silent default);
   V1 顶层形态 → `exit 3`(见 `lib/infrastructure/cli/parser.cpp` `validate_config_full`)。
3. **存在性**:V1 顶层形态要求 `output_dir` 目录已存在(`exit 3` if not found);
   平铺会话形态由 session/节点自建输出目录,不要求预先存在。
   **多数据块形态**:`output_dir` 是**块级**必填(每块一个,块间取值互不重复 —— 两块重复会写同一份
   run manifest);块 = 一次运行,CLI 逐块派发(独立 manifest / 独立 `run_context.json`)。
4. **取消路径**:SIGINT 后的 `incomplete` manifest 也写 `output_dir`(不写 CWD `"."`)。
5. 回归锚: `eng/tests/cli/test_cli001_vpi.py`、`eng/tests/cli/test_phase123_pipeline.py`
   负例矩阵的 `neg: missing output_dir` + `no CWD residue` 两条。
### 7.1 导出裁剪范围参数 `crop`（GUI 框选导出接口，EXPORT-CROP-01）

> 权威：负责人裁决 2026-09-23 逐字：「默认导出的话是要求边框不得裁剪任何有效像素，然后可以
> 导出一些黑边。到平面后我自己手动剪裁。然后支持手动输入裁剪范围。这样我以后 gui 的 HiPS
> 浏览器里面我可以直接导出框选。需要保留接口。」
> 上游：本文件 §1（命令树）/ §7（配置与 `output_dir`）+ `ASTROCS_DESIGN.md` §6/§7.2。
> 设计正本：`docs/design/PHASE3_DETAILED_DESIGN.md` §8；字段合同：
> `eng/contracts/schemas/phase_config_export.schema.json#/$defs/export_crop`；
> 字段说明（`--help` 同源）：`lib/infrastructure/cli/session_commands.h` 的
> `config_fields(SESSION_EXPORT)`。

`export --json <config.json>` 的运行配置接受一个**可选**键 `crop`（块内或平铺顶层，与
`center`/`width_px`/`scale_deg_per_px` 同面）。**缺省不裁剪** = 整幅导出（允许黑边，
有效像素全部保留）。

```jsonc
// 形式一：平面像素矩形（FITS 1-based 闭区间，相对未裁剪输出画幅）
"crop": {"crop_form": "pixels", "pixels": {"x0": 1001, "y0": 2001, "x1": 1512, "y1": 2512}}

// 形式二：天球轴对齐矩形（ICRS deg；ra_min_deg > ra_max_deg = 跨 RA=0 绕回）
"crop": {"crop_form": "sky",
         "sky": {"ra_min_deg": 83.5, "ra_max_deg": 84.0,
                 "dec_min_deg": -5.6, "dec_max_deg": -5.2}}
```

- **两形式互斥**：`crop_form` 选中其一，另一形式同时出现 ⇒ **具名拒绝**（不比较、不取一）。
- **接口稳定性**：键形固定、可机器生成；GUI 的 HiPS 浏览器框选导出**直接填该键**，
  不需要新的 CLI 命令或旗标（`export` 命令树与退出码不变）。
- **判据（fail-closed，全部具名报错，禁静默夹取）**：越界 / 宽高非正 / 两形式同时给 /
  裁剪后为空 / `sky` 边界点落在 TAN 半球外 ⇒ 拒绝。schema 面判结构（键闭包、类型、
  基本值域）；跨字段几何判据的唯一实现 = `lib/algorithms/projection/p3_wcs.h`，
  由 CLI 配置面与 scheduler 节点面共用。
- **精确性**：写出 FITS 的 WCS = 未裁剪画幅 WCS 在窗口上的**精确限制**（`CRVAL`/`CD`
  逐位不变、`CRPIX` 减**整数**窗口原点）⇒ 裁剪框内的像素与不裁剪时**逐位相同**。
- **模板口径**：`crop` 是可选键且**不进** `--template` 骨架（模板不替用户主张裁剪；
  缺省即不裁剪），只进 `--help` 字段说明。

