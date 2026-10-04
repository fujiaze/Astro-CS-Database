# 日志与错误合同

上游：最高设计的机器输出与退出码、错误传播与日志两章。

本文冻结四件事：运行日志行格式的机器可校验性、运行日志工件在运行清单中的登记字段、错误对象与
退出码映射、显式降级的登记要件与落点。科学判定、产品 schema、运行事件流、运行图、资源监控
时序工件与探针格式各有一份正本，本文不复制它们的字段。

---

## 合同范围

本合同冻结四件事：

1. 运行日志**行格式**的机器可校验性（「违规判据」一节，指向既有正本，不复制字段表）；
2. 运行日志**工件**在 run manifest 中的登记字段（「错误域」一节）；
3. **错误对象**与退出码映射（「阶段 ID」一节）；
4. **显式降级**的登记要件与词表（「稳定错误语义」一节）与**落点**（「进程退出码」一节）。

**不冻结**：科学判定、产品 schema、运行事件流（`protocol.h` / `jsonl.h`）、运行图、
资源监控 CSV、探针格式。上述各项各有一份正本，本合同不复制其字段。

---

## 日志行格式：机器校验格式

**运行日志的机器通道是机器校验格式**，正本为日志合同与 schema：

| 项 | 正本 |
|---|---|
| 字段表、语义、枚举、error 载荷 | `../resources/observability/STRUCTURED_LOGGING.md` 「违规判据」一节 |
| JSON Schema（draft-07） | `lib/infrastructure/observability/logging/log_event_v1.schema.json` |
| 参考实现与校验器 | `lib/infrastructure/observability/logging/log_event.py`（参考实现）；校验面 = 日志行 schema / 字段 / 枚举 / 单行大小（正本 `lib/infrastructure/observability/logging/log_event_v1.schema.json`，校验项） |
| 诊断工具 | `eng/tools/acsd_diagnose.py <run_dir>`，由一次运行的日志工件汇总输出小 bundle |

- 每行 = 一个 JSON 对象 + `\n`；单行（含换行）≤ **4096 字节**；
- 事件键名 `event`、顺序键 `seq`；与运行事件流的 `kind` / `sequence` **各自独立**；
- `level=error` 必须携带 `error{source,symbol,status}`；
- `commit` 为真实运行现场的 40 位小写 SHA（取值只来自运行现场，config 值不参与）；
- 落点文件名固定 `run_<run_id>.jsonl`（「进程退出码」一节）。

**为什么定成机器校验格式**：运行日志要能被机器判定（判据 R1–R3 的输入）、被审计回放、
被哈希登记进 manifest；自由文本无法承担这三件事。

---

## 人可读摘要：自由文本

摘要通道（`run_<run_id>.log`）是**自由文本**，理由：

- 它的消费者是人（操作员 / 验收者），不参与任何机器判定；
- 它的内容必须与 JSONL **同源生成**（同一事件的两路渲染，摘要模板见日志合同 「违规判据」一节），
  因此自由文本不引入第二套语义；
- 机器判定一律走 JSONL；摘要行的用途限于人读。

摘要行仍受两条约束：脱敏（「error-sensitive 模块的必备要件」一节）与单行 ≤ 4096 字节。

---

## `run_log` 与 manifest 登记字段

run manifest 增列 `log_artifacts[]`（**每次运行必填，可为空数组仅当运行在解析配置前终止**）：

| 字段 | 类型 | 必填 | 语义 | 约束 |
|---|---|---|---|---|
| `log_dir` | string | 是 | 相对 `output_dir` 的日志目录 | 固定 `logs`；非该值即合同违例 |
| `files[].kind` | string | 是 | 工件种类 | `jsonl` \| `summary` |
| `files[].name` | string | 是 | 相对 `log_dir` 的文件名 | `run_<run_id>.jsonl` / `run_<run_id>.log`；不含路径分隔符 |
| `files[].sha256` | string | 是 | 文件内容 SHA-256 | 64 位小写 hex；与磁盘实算一致 |
| `files[].bytes` | int | 是 | 文件字节数 | ≥ 0；与磁盘实际一致 |
| `files[].lines` | int | 是 | 行数 | ≥ 0；与磁盘实际一致 |
| `files[].level_counts` | object | 是 | 级别分布 | 四键 `debug/info/warn/error` 齐全，和 == `lines` |
| `files[].truncated` | bool | 是 | 是否因上限截断 | true 时日志内首行记截断原因与上限 |

- 两个工件**必须都存在**（成功、失败、取消三路皆然）；缺任一 ⇒ exit 8（`INTEGRITY`）；
- manifest 自身**不写进日志**（避免自引用）；
- 溯源链：`run manifest → log_artifacts[] → sha256 → 行内容`，任一环断裂 ⇒ exit 8。

---

## 错误对象与退出码映射

`error_report` 是 CLI 收敛面的唯一错误对象：

| 字段 | 类型 | 语义 |
|---|---|---|
| `domain` | string | `ErrorDomain` 名（`lib/include/acsd/core/contracts.h`） |
| `exit_code` | int | 下表映射结果（`lib/infrastructure/cli/exit_codes.h` 的 `acsd::ExitCode`） |
| `status` | string | 稳定错误码（`^[A-Z][A-Z0-9_]{0,63}$`） |
| `source` | string | 出错位置（模块 id 或仓库内相对路径） |
| `symbol` | string | 出错符号 |
| `message` | string | 脱敏后的中文诊断 |
| `run` / `node` / `phase` | string | 归属（与日志行同源字段） |
| `degraded` | bool | 本次运行是否发生过显式降级 |

**退出码映射（唯一；码值语义以 `exit_codes.h` 为准，全文表见
`LOG_AND_ERROR.md` 「进程退出码」一节）**：

| `ErrorDomain` | 退出码 | 依据 |
|---|---|---|
| `CONFIG` | 2（ARGS） | 参数 / 配置错误 |
| `DATA` | 2（ARGS） | 数据合同违例；失败节点 manifest 的 `error_kind==input` 时改判 3（INPUT） |
| `SCIENCE_PRECONDITION` | 4（SCIENCE） | 科学验证 / 不变量失败 |
| `BACKEND` | 5（BACKEND） | ABI / 签名 / CPU 特征 / 加载失败 |
| `IO` | 7（IO） | I/O 失败；失败节点 manifest 的 `error_kind==disk_full` 时改判 10 |
| `RESOURCE` | 10（RESOURCE） | 磁盘写满 / 写盘失败 |
| `CANCELLED` | 9（CANCELLED） | 用户取消或超时 |
| `INTERNAL` | 70（INTERNAL） | 未分类内部错误；必须生成脱敏 crash report |

- **数值表只有一份**：本合同只做「域 → 码」映射，不复写码值含义；
- 未列出的域一律 70，并在 `status` 里给出可定位的稳定错误码；
- 现行实现与本表的域映射偏差逐条登记在本合同（登记不改码；未登记的偏差按判红处理）。

---

## 显式降级登记要件

「降级」= 上游产物 / 能力缺失时改走替代路径并**继续运行**。降级合法当且仅当三要件齐备：

| # | 要件 | 判据 |
|---|---|---|
| D1 | **显式**：代码路径写出机器可读的 `degraded_reason` 字符串 | 函数体内出现 `degraded_reason` 赋值或写入 |
| D2 | **可溯**：manifest / provenance 记录降级事实与理由键 | manifest 含 `degraded_reason`（或 `degraded` 布尔 + 理由） |
| D3 | **不改科学语义**：降级不改变科学结果的定义（只改变执行路径或元数据完整度） | 科学语义改变 ⇒ 不是降级，必须 fail-closed 上行 |

`degraded_reason` 词表（小写下划线，稳定不变）：`photscale_incomplete`、`photscale_absent`、
`upstream_artifact_absent`、`optional_keyword_unparsed`、`cache_miss_recompute`。
新增词先在本合同登记再使用。`photscale_incomplete` 为**读侧保留词**（读侧仍可解释既有产物），
生产写侧不产生它：测光拟合失败属**显式失败**，按 「阶段 ID」一节 口径逐帧记 `status=fail` +
`error_domain` / `error_status` / `error`，不写 `degraded_reason`
（失败 ≠ 降级，判据见本节 D1–D3 与 `../../detail/LOG_AND_ERROR_SYSTEM.md` ）。

**三种情形一律具名上行**：回退到低优先输入、保持缺省值、跳过校验都属故障，必须上行到 CLI。

---

## 落点合同

| 项 | 值 |
|---|---|
| 日志目录 | `log_dir`，默认 `<output_dir>/logs` |
| 机器日志 | `<log_dir>/run_<run_id>.jsonl` |
| 人可读摘要 | `<log_dir>/run_<run_id>.log` |
| 目录创建 | 幂等；由运行层在 `run_start` 前创建 |

运行日志的落点全部由 `output_dir` 派生。写出位置取自显式配置；
进程 CWD 只标识进程自身位置，不作为落点依据。

判据 R3 强制本约束（落点合同本身即判据），
落点之外的位置一律判红，包括：进程 CWD 相对路径、源码树目录（如 `lib/**/logs/`）、
仓库过程产物目录（`run/`，承载开发与实验过程日志）、安装目录、用户家目录。

---

## 脱敏与大小上限

沿用日志合同 「阶段 ID」一节 / 「稳定错误语义」一节，不另立规则：绝对用户路径 / 家目录 / URL 凭据 / 密钥键值一律 `<redacted>`；
结构化字段只接受安全字符；单行 ≤ 4096 字节，超限按 UTF-8 边界截断。

---

## 判据与负例

| 判据 | 审核面 | 正例（绿） | 负例（红） |
|---|---|---|---|
| R1 错误不吞 | 生产收敛面 | 无未登记吞错点 | 注入 `catch (...) {}` ⇒ FAIL |
| R2 降级显式 | 同上 | 条件回退点已登记且写 `degraded_reason` | 注入静默回退函数 ⇒ FAIL |
| R3 日志落点 | 同上 | 落点均派生自 `output_dir` 或已登记 | 落点字面量指向 `output_dir` 之外 ⇒ FAIL |
| R4 登记完整 | 同上 | 锚存活、已登记偏差条目只减不增、审核面非空 | 删条目 / 新增未登记偏差 / 清空审核面 ⇒ FAIL |
| R5 合同锚 | 同上 | 落点默认值与本合同、最高设计一致 | 改落点默认值 ⇒ FAIL |

判据的审核面 = `lib/infrastructure/observability/` 的生产收敛面与本合同 「阶段 ID」一节–「error-sensitive 模块的必备要件」一节 的各表；
逐条由人工对抗审核执行，证据 = 复核命令 + 实测输出或结构化读数，不产出流水线判决。
每条判据必须能红：注入对应缺陷后复核不给出红，即判该判据失效。

R3 的落点扫描按字面量执行：源码中出现写死的落点路径字面量（不限于 「进程退出码」一节 列举的落点之外位置）
即判红，注入样例取仓库过程产物目录下的日志路径。该判据只允许一处声明，即本节。

本标准规定错误的三层语义、违规判据、错误分类表、进程退出码与 error-sensitive 模块的必备要件。
域 → 退出码的映射面在 `LOG_AND_ERROR.md` 「阶段 ID」一节；本文档定义码值语义本身。

## 三层语义

1. **success**：`rc=0`，状态为 `OK`；
2. **recoverable science status**：`rc=0`，`status` 字段取 `UNDERDETERMINED`、
   `NO_CANDIDATES`、`ALL_REJECTED`、`ZERO_VALID_WEIGHT`、`INVALID_INPUT` 之一，
   产品仍有效或走显式 fallback；
3. **hard error**：`rc≠0`，`status` 字段取第 3 节硬错误类别之一。

三层之外没有第四种结果：任何未分类的失败按 `INTERNAL`（码 70）上行，并生成脱敏 crash report。

## 违规判据

下列形态一律判为未传播，判红：

| # | 违规形态 | 判红理由 |
|---|---|---|
| V1 | `rc=0` + invalid status 双语义 | 一个返回值承载两套语义，调用方无法判定成功与否 |
| V2 | 以 warning 或 log 替代 error status | 失败不进入收敛面，退出码与实际结果不一致 |
| V3 | 默认 swallow 错误（`catch(...)` 空吞） | 错误在模块边界消失，溯源链断裂 |
| V4 | 静默截断或置零（损坏输入） | 损坏输入被当作有效数据继续参与计算 |

## 硬错误类别

十类硬错误覆盖模块可能上报的全部不可恢复情形：

| 类别 | 语义 |
|---|---|
| `CONFIG` | 参数或配置错误 |
| `INPUT_CORRUPT` | 输入缺失、格式错误或 hash 不符 |
| `DEPENDENCY` | 依赖模块或产物不可用 |
| `NUMERIC` | 数值不变量失败 |
| `NO_DATA` | 无可用数据，产品无内容可发 |
| `RESOURCE` | 磁盘写满或写盘失败 |
| `TIMEOUT` | 超时 |
| `IO` | I/O 失败 |
| `SCIENCE` | 科学验证失败或不变量被破坏 |
| `INTERNAL` | 未分类内部错误 |

## 错误域

机器面错误域 = `lib/include/acsd/core/contracts.h` 的 `ErrorDomain` 枚举：
`CONFIG` / `DATA` / `SCIENCE_PRECONDITION` / `IO` / `RESOURCE` / `BACKEND` /
`CANCELLED` / `INTERNAL`。

错误域是分类维度，退出码是对外收敛维度，二者的映射表唯一声明处 =
`LOG_AND_ERROR.md` 「阶段 ID」一节。

模块以 `Result` 报错，`exit` / `abort` 归进程宿主；异常在模块内部转为 `Result`，
不穿过 C ABI。`Result<T>` 携带 nested cause、serialization、cancel 与错误码映射。

## 阶段 ID

每个阶段有一个阶段 ID 前缀，逐节点一枚；节点名与 `docs/detail/registry/` 的注册表节点一致。
日志与诊断面统一使用本表的阶段 ID：

| 阶段 | 阶段 ID |
| --- | --- |
| normalize | `P1.READ` / `P1.CALIBRATE` / `P1.STAR` / `P1.PSF` / `P1.PLATESOLVE` / `P1.PHOTOMETRIC` / `P1.NOISE` / `P1.DRIZZLE` / `P1.HIPS_WRITE` |
| mosaic | `P2.COVERAGE` / `P2.SAMPLER` / `P2.UPM` / `P2.REJECTION` / `P2.INTEGRATE` / `P2.HIPS_WRITE` |
| export | `P3.PROPERTIES` / `P3.WCS` / `P3.RESAMPLE` / `P3.WRITE` / `P3.VERIFY` |

## 稳定错误语义

- C API：`0` = success；非 `0` = hard error，类别由调用上下文与 troubleshooting 条目定位；
- 可恢复科学状态经 `status` 字段表达，取值集合与第 1 节第 2 层一致；
- 每个 high-risk error 对应一条 troubleshooting 条目
  （`../../detail/merged_TROUBLESHOOTING.md`）。

`status` 字段的机器可校验形态为 `^[A-Z][A-Z0-9_]{0,63}$`；错误对象的完整字段表见
`LOG_AND_ERROR.md` 「阶段 ID」一节。

## 进程退出码

**退出码唯一源 = `lib/infrastructure/cli/exit_codes.h` 的 `acsd::ExitCode` 枚举**。
凡需要退出码数值处一律引用该头文件；本文档第 7 节的表与最高设计 「机器判据」一节 同源，
是全仓唯一一份码值语义表。

| 码 | 枚举名 | 语义 |
|---|---|---|
| 0 | `OK` | 运行正常结束：无硬错误、无取消；可恢复科学状态由 `status` 字段表达 |
| 2 | `ARGS` | CLI 参数或配置错误 |
| 3 | `INPUT` | 输入缺失、格式或 hash 错误 |
| 4 | `SCIENCE` | 科学验证 / 数值不变量失败 |
| 5 | `BACKEND` | backend ABI / 签名 / CPU 特征 / 加载失败 |
| 6 | `COMPUTE` | 计算执行失败 |
| 7 | `IO` | I/O 失败 |
| 8 | `INTEGRITY` | 输出完整性 / 验证失败 |
| 9 | `CANCELLED` | 用户取消或超时 |
| 10 | `RESOURCE` | 磁盘写满 / 写盘失败 |
| 70 | `INTERNAL` | 未分类内部错误（必须生成脱敏 crash report） |

- 码 10 的适用面是磁盘；内存、CPU 与线程不设资源超限门，内存面由调度器按预算内化处理；
- CLI 面的同一张表不再重复声明，接口面引用本节。
- **本表的适用边界**：本表只解释 `acsd::ExitCode` 枚举成员在三个生产命令
  （`cmd_session{1,2,3}_run`）上的语义。未 include 本头文件、以裸整数 `return` 收敛的
  诊断/工具二进制，其返回值不属于本表；这类二进制的返回值语义在各自的接口正本中就地登记，
  且**不得**按本表反查。两者共用同一码值空间时，以 `acsd::ExitCode` 成员的语义为准，
  非枚举来源的裸整数不获得本表的语义。

### 模块特定码

模块特定的非进程退出码走 JSONL 事件流的 `error.numeric_code` 面，与进程退出码分立，
保留区间 20–29，已定义 20–28：

```text
STAR_DETECT_FAILED=20    PSF_FAILED=21              PHOTOMETRIC_FAILED=22
SNR_FAILED=23            STACK_FAILED=24             HISS_INVALID=25
HCSD_INVALID=26          MODULE_ABI_UNSUPPORTED=27   INPUT_INVALID=28
MODULE_SPECIFIC_BASE=100
```

### 机器判据

本节 11 码与 `lib/infrastructure/cli/exit_codes.h` 的 `acsd::ExitCode` 枚举逐名逐值一致，
由 `eng/tools/docs_machine_consistency.py`（`error_taxonomy_exit_codes`）执行校验。
第 7.1 节的模块特定码属 JSONL `error.numeric_code` 面，不参与该校验。

## error-sensitive 模块的必备要件

每个 error-sensitive 模块必须同时具备：

| # | 要件 | 判据 |
|---|---|---|
| M1 | diagnostics stage ID | 每一失败节点都能给出第 5 节表中的阶段 ID |
| M2 | stable error category / code | `status` 为稳定码，`error.category` 取第 3 节类别之一 |
| M3 | troubleshooting 条目 | 条目存在且可由错误码直达（`../../detail/merged_TROUBLESHOOTING.md`） |

三项缺任一 ⇒ 该模块不进入生产构建。

## `ERR-*` 登记与追溯

稳定错误码族 `ERR-*` 的登记面 = `../governance/TRACEABILITY.md ` 的 `error_codes` 列。

以 `ERR-P2-UPM-001`（UPM 模型文件畸形）为例：`frames` 非数组、`frames` 内重复、
帧数与 C 行数不等，都在模型打开时判错，并沿 `p2_upm_open` 上行
（实现 `lib/algorithms/coverage/src/upm.cpp` 的 frames 校验）。
## 参考文献

[1] 内部文档 `../../ACSD_DESIGN.md`，最高设计的机器输出与错误传播两章。

[2] 内部文档 `CLI_PROTOCOL.md`，命令行协议合同。

[3] 内部文档 `MANIFEST_VERIFY.md`，运行清单与校验合同。

[4] 内部文档 `../resources/observability/STRUCTURED_LOGGING.md`，结构化日志合同。
