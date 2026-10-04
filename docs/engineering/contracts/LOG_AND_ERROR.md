# 日志与错误合同

上游：最高设计的机器输出与退出码、错误传播与日志两章。

本文冻结四件事：运行日志行格式的机器可校验性、运行日志工件在运行清单中的登记字段、错误对象与
退出码映射、显式降级的登记要件与落点。科学判定、产品 schema、运行事件流、运行图、资源监控
时序工件与探针格式各有一份正本，本文不复制它们的字段。

---

## 1 合同范围

本合同冻结四件事：

1. 运行日志**行格式**的机器可校验性（指向既有正本，不复制字段表）；
2. 运行日志**工件**在 run manifest 中的登记字段；
3. **错误对象**与退出码映射；
4. **显式降级**的登记要件与词表与**落点**。

**不冻结**：科学判定、产品 schema、运行事件流（`protocol.h` / `jsonl.h`）、运行图、
资源监控 CSV、探针格式。上述各项各有一份正本，本合同不复制其字段。

错误的三层语义、硬错误类别表、码值语义与 error-sensitive 模块的必备要件属于
`../standards/ERROR_MODEL.md`；本合同只做域到退出码的映射，不复写码值语义。

---

## 2 日志行格式：机器校验格式

**运行日志的机器通道是机器校验格式**，正本为结构化日志合同与其 schema：

| 项 | 正本 |
|---|---|
| 字段表、语义 | `../resources/observability/STRUCTURED_LOGGING.md` 的事件模型（JSONL 行结构、必需字段、error 载荷三节） |
| JSON Schema（draft-07） | `lib/infrastructure/observability/logging/log_event_v1.schema.json` |
| 参考实现与校验器 | `lib/infrastructure/observability/logging/log_event.py`；校验面 = 日志行 schema / 字段 / 枚举 / 单行大小（正本同上 schema，校验项） |
| 诊断工具 | `eng/tools/acsd_diagnose.py <run_dir>`，由一次运行的日志工件汇总输出小 bundle |

- 每行 = 一个 JSON 对象 + `\n`；单行（含换行）≤ **4096 字节**；
- 事件键名 `event`、顺序键 `seq`；与运行事件流的 `kind` / `sequence` **各自独立**；
- `level=error` 必须携带 `error{source,symbol,status}`；
- `commit` 为真实运行现场的 40 位小写 SHA（取值只来自运行现场，config 值不参与）；
- 落点文件名固定 `run_<run_id>.jsonl`（见本合同的落点合同一节）。

**为什么定成机器校验格式**：运行日志要能被机器判定（判据 R1–R3 的输入）、被审计回放、
被哈希登记进 manifest；自由文本无法承担这三件事。

---

## 3 人可读摘要：自由文本

摘要通道（`run_<run_id>.log`）是**自由文本**，理由：

- 它的消费者是人（操作员 / 验收者），不参与任何机器判定；
- 它的内容必须与 JSONL **同源生成**（同一事件的两路渲染，摘要模板见结构化日志合同的中文摘要一节），
  因此自由文本不引入第二套语义；
- 机器判定一律走 JSONL；摘要行的用途限于人读。

摘要行仍受两条约束：脱敏（见本合同的脱敏与大小上限一节）与单行 ≤ 4096 字节。

---

## 4 `run_log` 与 manifest 登记字段

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

**载体缺口登记**：本节要求的 `log_artifacts[]` 顶层键目前**没有任何生产写出点**，
全仓对 `log_artifacts` 的字面引用为零（生产写出点是 `commands.cpp` 的 `write_run_manifest`，
其顶层键集见运行清单与校验合同）。因此本节的字段表是**已冻结但未接线**的合同，
不得据此认为线上运行会产出该键。

---

## 5 错误对象与退出码映射

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

**退出码映射（唯一；码值语义以 `exit_codes.h` 为准，全文码值表见
`../standards/ERROR_MODEL.md` 的进程退出码一节）**：

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
- 现行实现与本表的域映射偏差逐条登记在错误模型的稳定错误码族登记一节（登记不改码；
  未登记的偏差按判红处理）。

---

## 6 显式降级登记要件

「降级」= 上游产物 / 能力缺失时改走替代路径并**继续运行**。降级合法当且仅当三要件齐备：

| # | 要件 | 判据 |
|---|---|---|
| D1 | **显式**：代码路径写出机器可读的 `degraded_reason` 字符串 | 函数体内出现 `degraded_reason` 赋值或写入 |
| D2 | **可溯**：manifest / provenance 记录降级事实与理由键 | manifest 含 `degraded_reason`（或 `degraded` 布尔 + 理由） |
| D3 | **不改科学语义**：降级不改变科学结果的定义（只改变执行路径或元数据完整度） | 科学语义改变 ⇒ 不是降级，必须 fail-closed 上行 |

`degraded_reason` 词表（小写下划线，稳定不变）：`photscale_incomplete`、`photscale_absent`、
`upstream_artifact_absent`、`optional_keyword_unparsed`、`cache_miss_recompute`。
新增词先在本合同登记再使用。`photscale_incomplete` 为**读侧保留词**（读侧仍可解释既有产物），
生产写侧不产生它：测光拟合失败属**显式失败**，按错误模型的三层语义第 3 层口径逐帧记
`status=fail` + `error_domain` / `error_status` / `error`，不写 `degraded_reason`
（失败 ≠ 降级，判据见本节 D1–D3 与 `../../detail/LOG_AND_ERROR_SYSTEM.md` ）。

**三种情形一律具名上行**：回退到低优先输入、保持缺省值、跳过校验都属故障，必须上行到 CLI。

---

## 7 落点合同

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

## 8 脱敏与大小上限

沿用结构化日志合同的敏感路径脱敏与大小上限两节，不另立规则：绝对用户路径 / 家目录 / URL 凭据 / 密钥键值一律 `<redacted>`；
结构化字段只接受安全字符；单行 ≤ 4096 字节，超限按 UTF-8 边界截断。

---

## 9 判据与负例

| 判据 | 审核面 | 正例（绿） | 负例（红） |
|---|---|---|---|
| R1 错误不吞 | 生产收敛面 | 无未登记吞错点 | 注入 `catch (...) {}` ⇒ FAIL |
| R2 降级显式 | 同上 | 条件回退点已登记且写 `degraded_reason` | 注入静默回退函数 ⇒ FAIL |
| R3 日志落点 | 同上 | 落点均派生自 `output_dir` 或已登记 | 落点字面量指向 `output_dir` 之外 ⇒ FAIL |
| R4 登记完整 | 同上 | 锚存活、已登记偏差条目只减不增、审核面非空 | 删条目 / 新增未登记偏差 / 清空审核面 ⇒ FAIL |
| R5 合同锚 | 同上 | 落点默认值与本合同、最高设计一致 | 改落点默认值 ⇒ FAIL |

判据的审核面 = `lib/infrastructure/observability/` 的生产收敛面，与错误模型标准的
阶段 ID、error-sensitive 模块的必备要件两张表；逐条由人工对抗审核执行，
证据 = 复核命令 + 实测输出或结构化读数，不产出流水线判决。
每条判据必须能红：注入对应缺陷后复核不给出红，即判该判据失效。

R3 的落点扫描按字面量执行：源码中出现写死的落点路径字面量（不限于本节列举的落点之外位置）
即判红，注入样例取仓库过程产物目录下的日志路径。该判据只允许一处声明，即本节。

## 参考文献

[1] 内部文档 `../../ACSD_DESIGN.md`，最高设计的机器输出与错误传播两章。

[2] 内部文档 `CLI_PROTOCOL.md`，命令行协议合同。

[3] 内部文档 `MANIFEST_VERIFY.md`，运行清单与校验合同。

[4] 内部文档 `../resources/observability/STRUCTURED_LOGGING.md`，结构化日志合同。

[5] 内部文档 `../standards/ERROR_MODEL.md`，错误模型标准。
