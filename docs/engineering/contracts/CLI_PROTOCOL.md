# 命令行协议合同

本文依据最高设计[1]的命令行、机器输出和错误传播要求，规定命令树、退出码、标准输出与标准错误纪律、运行事件流、
取消与崩溃语义、运行配置与输出目录规则。退出码的逐码语义见 `../standards/ERROR_MODEL.md`[3]、
错误域到退出码的映射见 `LOG_AND_ERROR.md`[6]；
运行配置的完整字段合同见 `CONFIG.md`[4]；运行清单的校验合同见 `MANIFEST_VERIFY.md`。
机器输出的 stdout 纪律与退出码分型、取消语义的上位条款见最高设计的命令行合同一章[1]。

## 命令树

唯一命令树以最高设计的命令行合同一章[1]为准，帮助文本由此生成。`phase1`、`phase2`、`phase3`
只是内部指代，不是用户命令。

**用户命令只有** normalize/mosaic/export + help/--version/doctor/benchmark。`phase1|2|3`
用户命令与别名（`config *` / `modules *` / `selftest` / `test synthetic` / `verify*` /
`drizzle` / `benchmark cpu` / `verify-profile` / `hardware inspect`）**不在命令面上，
调用返回 rc=2**（phase 仅为内部指代）。

命令语义（薄入口[1]）：
- `--json <config.json>` 指定运行配置，执行输入检查、阶段计算并写产品与 manifest。目标合同不因设备差异、质量分数或人工审批阻止合法输入；配置损坏、文件缺失和不可计算几何仍报具体错误。现有预检/确认及 `-y`、`--yes`、`-force` 属命令行实现的兼容行为，需要与最高设计的运行诊断目标分别说明；
- `--template [-o <path>]` 生成可直接改的完整 JSON 模板（缺 `-o` → stdout）；
- `--help` 子命令帮助与字段说明；
- 三个命令**平级独立**：各自独立进程、**独立重跑**（**无断点续算**：重跑 = 新运行目录 + 新 manifest）、独立验收，**串接一律显式**（最高设计[1]）；
- `benchmark` 生成/更新**安装目录** cpu_profile（后续运行自动读取）。
- **安装目录口径的适用域**：安装目录是发行布局概念，输出路径相对发行布局解析；源码树内运行构建期二进制不构成产品契约面，此时落点转用户可写目录并在标准错误明示原因与实际落点。正本是最高设计的 CPU 后端与资源一章，唯一实现是 `lib/infrastructure/cli/commands.cpp` 的 `cli_resolve_cpu_profile_path`。

实际阶段调用由 `commands.cpp` 的 `run_phase1_block`、`run_phase2_block`、`run_phase3_block` 进入 Runtime 与阶段节点。局部会话 facade 的 API 不能作为完整生产链已执行的证据；Normalize 的实际交接由[生产链架构](../architecture/NORMALIZE_PIPELINE.md)说明。

## 退出码

退出码的码值与含义只有一份，唯一源是 `lib/infrastructure/cli/exit_codes.h` 的 `acsd::ExitCode`
枚举，逐码语义表见 `../standards/ERROR_MODEL.md`[3]「进程退出码」一节。上位分型条款见最高设计的机器输出与退出码[1]。共 11 个码：0 成功、2 命令行参数或配置错、
3 输入缺失或格式或散列错、4 科学验证或不变量失败、5 后端 ABI、签名、CPU 特征或加载失败、
6 计算执行失败、7 I/O 失败、8 输出完整性验证失败、9 用户取消或超时、10 磁盘写满或写盘失败、
70 未分类内部错误（必须出脱敏崩溃报告）。

码 10 的适用面是磁盘；内存、CPU 与线程不设资源超限门，内存面由调度器按预算内化处理。
跨平台同类失败按相同语义映射；是否已经由双平台用例覆盖需附实际命令与输出。

## 标准输出与标准错误纪律

- 运行三命令时，`--json <config.json>` 是配置路径参数，stdout 为逐行 UTF-8 JSON 事件，人可读摘要和诊断写 stderr。`--version --json`、`doctor --json` 等非运行机器响应在其命令合同下输出单个 JSON 文档；模板按模板命令输出 JSON。不能仅凭 `--json` 的字面出现就判断输出为单文档。帮助和非机器版本响应使用各自文本形态；路径按 UTF-8/Windows Unicode 规则处理。
- **事件流 = 默认输出**（最高设计的机器输出与退出码[1]）：**不需要旗标开启**；GUI 用其它语言**直接捕获 CLI 输出**。`--events-jsonl` **保留接受**，语义**等价默认行为**（别名，`lib/infrastructure/cli/commands.cpp`、`command_tree.h`）——**事件流的开启条件 = 默认行为本身**。
- **stdout 无日志污染**为机器测试项(CLI-002 golden)。

## JSONL 运行事件流

本节规定事件语义；`lib/infrastructure/cli/protocol.h` 的 ValidateEventV1 与 `jsonl.h` 的 JsonlEmitter 是当前发送实现，机器形状由 eng/contracts/schemas/jsonl_event_v1.schema.json 表达。三者应一致；冲突时按文档权威链订正，不将代码提升为设计正本。结构化日志[5]与运行事件是不同通道，其 event 与 kind 等字段各自有定义。

- 每行必含: `schema_version,event_id,run_id,timestamp_utc,sequence,kind,severity,phase,stage,message`（正本 `protocol.h::kEventFieldsV1`）;`sequence` 从 0 单调递增。
- 逐 kind 的扩展字段由机器 schema 表达，发送侧 `lib/infrastructure/cli/protocol.h::missing_required_extension_v1` 的 `kExt` 表执行相应检查。两者须保持一致；当前 10 类 kind 的语义为：
 - `progress`：阶段内进度（完成量/总量/单位/速率/预计剩余）；
 - `resource`：进程资源采样（CPU 核数 / RSS / 读写字节日 / 线程数）；
 - `artifact`：落盘产物登记（角色/路径/整文件字节摘要/字节数）；
 - `backend`：实际后端选择（kernel / backend_id / isa / workers / block_size / reason）；
 - `final`：运行收尾（`exit_code` 必须 ∈ 「退出码」一节 冻结退出码域；status / run_manifest / summary）；
 - `stage_start` / `stage_end`：阶段起止（无扩展字段）；
 - `graph`：运行图落盘（path）；
 - `resource_gate`：资源检查的诊断记录，不能据此新增产品命令的人工审批或资源拒绝条件；
 逐键语义与必含集由当前发送接口、机器 schema 与[可观测性细节](../../detail/infrastructure/observability.md)交叉核对；
 - `v6_mode_route`：模式路由登记（route_kind / token / surface / 来源 / 预算归属）。
 kind 集合、schema 必填字段、发送校验与客户端读取需要共同核对。检查编号或字段登记不能代替实际执行证据。
- **`artifact` 的 DET-001 附加字段不属冻结必含集**：`integrity_sha256` / `canonical_sha256` /
 `canonical_hash_spec` / `canonical_format` 是实现侧附加字段（正本 `kExt` 的 artifact 必含集
 = role/path/sha256/size_bytes 四项）；必含集即该四项。DET-001 判据：
 sha256=整文件字节摘要(完整性)，canonical_sha256=规范产品哈希(像素数据+科学元数据，排除易变卡/键；
 口径 spec=acsd.canonical-product-hash/v1，见 eng/tools/canonical_product_hash.py --spec)；
 可复现性判据 = canonical_sha256；sha256 只表整文件完整性。
- 重计算 stage 必发 `stage_start/stage_end`+实际 backend 事件;GUI/未来客户端只消费本协议(禁链接科学库绕过 CLI)。
- schema：`eng/contracts/schemas/jsonl_event_v1.schema.json`；接口、样例与发送器应实现本文的事件语义。

## 取消与崩溃

- Ctrl-C/Windows console cancel 触发协作取消令牌；内核在已声明的安全点检查，停止在途任务并释放资源后完成收尾。
- 取消后停止派发、等待在飞消费者并收尾writer，尝试写incomplete运行清单，清理或隔离未发布暂存后返回9。已完成独立帧与已可见但持久化未确认对象按[原子发布](ATOMIC_PUBLISH.md)处理，不能把所有已发布结果一律改成未完成或删除。
- 未捕获异常→70+run_id/阶段/最小脱敏 crash report(不泄露凭据)。

## 机器化一致性检查项

1. `--help` golden 树与 「命令树」一节 逐行一致;
2. JSON/JSONL 样例对 schema 有效(jsonschema 或 stdlib 等价校验);
3. 退出码声明源为`lib/infrastructure/cli/exit_codes.h`，与核心头现存镜像逐名逐值核对；不能以“唯一源”的规范文字代替源码去重事实；
4. handler 到实际 Runtime、节点与 writer 的生产调用路径可追溯，局部 facade 不能代替完整阶段;
5. 发布 manifest 不含 `phase1|2|3` 可执行文件(与 `../build/BUILD_GRAPH.md` 的 exe_target 面同源: 生产可执行目标恰一 = `acsd`, CLI-001);
6. 双平台命令测试的字段与退出码一致。

这些是验证要求，只有附实际入口、命令和输出才可登记为已通过。

## 配置与 `output_dir`

运行产物(每相 run manifest `acsd_run_*.json`、资源三件套
`resource_timeseries.csv` / `resource_summary.json` / `worker_balance.csv`、
`alloc_samples.csv` / `alloc_report.json`、节点科学产物)**只落 `output_dir`**;

> **资源时序工件的唯一列合同**：
> - **唯一列合同 = 生产实现** `lib/infrastructure/cli/resource_recorder.h`：**20 列**
> `elapsed_seconds,stage,cpu_pct,system_cpu_pct,active_workers,runnable_workers,rss_bytes,pss_bytes,commit_bytes,page_faults,read_bytes,write_bytes,queue_depth,lock_wait_ns,progress,threads,active_compute_threads,per_thread_cpu_max_pct,per_thread_cpu_sum_pct,io_wait_pct`
> （**run 收尾一次性落盘**，被 manifest / 目录树哈希覆盖）；`lib/infrastructure/cli/resource_events.h` 明文
> 「资源时序曲线的**唯一载体** = 磁盘工件 `resource_timeseries.csv`」。
> - `../resources/observability/RESOURCE_MONITORING.md`的「每秒采样 + seed 行 + 指纹链」
> CSV 是**监控伴随器的原始数据**，**不是同一工件** ⇒ 其工件名固定为 **`monitor_timeseries.csv`**。
> - **两工件不同名、不互替**：`resource_timeseries.csv`（20 列，收尾一次性）与 `monitor_timeseries.csv`
> （每秒采样 + 指纹链）**各自具名、各自独立**：列定义与采样语义各一套。
CLI 的写出位置取自显式配置的目录,进程 CWD(`"."`) 只标识进程自身位置;按 CWD 写出会在工作区根散落产物并触发
UT-CLI `mutates_workspace=false` 的 dirty 判定。

1. **必填**:`normalize|mosaic|export --json <config.json>` 的运行配置指定输出目录；`phase1|2|3 run/plan/validate/inspect` 不属于用户命令树。运行检查和诊断由命令本身处理，计划可由 run-plan.json/run-graph.json 表达；`output_dir` 字段由配置合同[4]规定。
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
5. 回归验证须包含缺失 `output_dir`、块间重复输出位置和 CWD 残留等负例。测试目录或测试名存在不等于这些行为已被执行；具体状态须核对可复现入口与结果。
#### 导出裁剪范围参数 `crop`

> 权威口径：默认导出要求边框不得裁剪任何有效像素（允许含黑边，到平面域后手动剪裁）；
> 同时支持手动输入裁剪范围；GUI HiPS 浏览器的框选直接导出依赖此接口，接口须保留。
> 权威：最高设计的投影导出与命令行合同两章[1]；字段合同见 `CONFIG.md`[4]；几何唯一实现是
> `lib/algorithms/projection/p3_wcs.h`（命令行配置面与调度节点面共用）；生产消费点是调度器的
> `export` 节点链 WCS、写出与验证三节点。
> 裁剪实施说明：[Export 管线](../../detail/export/pipeline.md)；字段合同：
> `eng/contracts/schemas/phase_config_export.schema.json` 的 `$defs/export_crop`；
> 字段说明（`--help` 同源）：`lib/infrastructure/cli/session_commands.h` 的
> `config_fields(SESSION_EXPORT)`。

`export --json <config.json>` 的运行配置接受一个**可选**键 `crop`（块内或平铺顶层，与
`center`/`width_px`/`scale_deg_per_px` 同面）。缺省不裁剪表示保留已定义的输出画幅，允许黑边；画幅本身是否覆盖全部输入有效域需要另行检查。

```jsonc
// 形式一：平面像素矩形（FITS 1-based 闭区间，相对未裁剪输出画幅；索引基址依 FITS 标准 4.0[2]）
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
- **映射一致性**：写出 FITS 的 WCS 是未裁剪画幅 WCS 在窗口上的限制，`CRVAL`/`CD` 保持、`CRPIX` 减零基整数窗口偏移。它保持同一天球映射，不由数学恒等式推断浮点求值或重采样结果逐位相同。
  写出关键字的 FITS 语法（`CRPIX`/`CRVAL`/`CD` 卡片、`1-based` 像素基址）依 FITS 标准 4.0[2]。
  天空范围还需保证连续边界被包围；当前固定边界采样不构成这一保证的证明，实施限制由 Export 管线说明。
- **模板口径**：`crop` 是可选键且**不进** `--template` 骨架（模板不替用户主张裁剪；
 缺省即不裁剪），只进 `--help` 字段说明。

## 参考文献

[1] [ACSD 最高设计](../../ACSD_DESIGN.md)，命令行、运行诊断与投影导出。

[2] FITS 工作组. FITS 标准 4.0. IAU, 2018. https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf

[3] 内部文档 `../standards/ERROR_MODEL.md`，错误模型标准（「进程退出码」一节含逐码语义表，码值语义的唯一正本）。

[4] 内部文档 `CONFIG.md`，配置合同（`output_dir` 与 `crop` 字段合同）。

[5] 内部文档 `../resources/observability/STRUCTURED_LOGGING.md`，结构化日志合同。

[6] 内部文档 `LOG_AND_ERROR.md`，日志与错误合同（「错误对象与退出码映射」一节含错误域到退出码的映射表；码值含义不在本文）。
