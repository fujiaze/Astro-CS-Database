# 日志与错误合同（LOG-004）

> 上游：ASTROCS_DESIGN.md §7.3（错误传播与运行日志：顶层约束）

> 详细设计：`docs/design/LOG_AND_ERROR_SYSTEM.md`
> 机器事实源：`lib/infrastructure/observability/logging/log_event_v1.schema.json`（日志行格式正本，LOG-001）、
> `eng/ci/ledgers/log_system_ledger.json`（登记台账）、`eng/tools/quality/check_log_system.py`（判据）。

---

## 1. 合同范围与"不复制"声明

本合同冻结四件事：

1. 运行日志**行格式**的机器可校验性（§2，指向既有正本，不复制字段表）；
2. 运行日志**工件**在 run manifest 中的登记字段（§4）；
3. **错误对象**与退出码映射（§5）；
4. **显式降级**的登记要件与词表（§6）与**落点**（§7）。

**不冻结**：科学判定、产品 schema、运行事件流（`protocol.h`/`jsonl.h`）、运行图（LOG-003）、
资源监控 CSV（LOG-002）、探针格式（RELEASE-02）。上述各项各有一份正本，本合同不复制其字段。

---

## 2. 日志行格式：机器校验格式

**运行日志的机器通道是机器校验格式**，正本 = LOG-001 合同与 schema：

| 项 | 正本 |
|---|---|
| 字段表、语义、枚举、error 载荷 | `docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md` §2 |
| JSON Schema（draft-07） | `lib/infrastructure/observability/logging/log_event_v1.schema.json` |
| 参考实现与校验器 | `lib/infrastructure/observability/logging/log_event.py`、`eng/tools/monitoring/check_log_contract.py` |

- 每行 = 一个 JSON 对象 + `\n`；单行（含换行）≤ **4096 字节**；
- 事件键名 `event`、顺序键 `seq`；**不得**与运行事件流的 `kind`/`sequence` 混用；
- `level=error` 必须携带 `error{source,symbol,status}`；
- `commit` 为真实运行现场的 40 位小写 SHA，禁止 config 值冒充；
- 落点文件名固定 `run_<run_id>.jsonl`（§7）。

**为什么定成机器校验格式**：运行日志要能被机器判定（判据 R1–R3 的输入）、被审计回放、
被哈希登记进 manifest；自由文本无法承担这三件事。

---

## 3. 人可读摘要：自由文本

摘要通道（`run_<run_id>.log`）是**自由文本**，理由：

- 它的消费者是人（操作员/验收者），不参与任何机器判定；
- 它的内容必须与 JSONL **同源生成**（同一事件的两路渲染，LOG-001 §2 摘要模板），
  因此自由文本不引入第二套语义；
- 机器判定一律走 JSONL；摘要行**不得**作为任何判据的输入。

摘要行仍受两条约束：脱敏（§8）与单行 ≤ 4096 字节。

---

## 4. `run_log` 与 manifest 登记字段

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

- 两个工件**必须都存在**（成功、失败、取消三路皆然）；缺任一 ⇒ exit 8（INTEGRITY）；
- manifest 自身**不写进日志**（避免自引用）；
- 溯源链：`run manifest → log_artifacts[] → sha256 → 行内容`，任一环断裂 ⇒ exit 8。

---

## 5. 错误对象与退出码映射

`error_report` 是 CLI 收敛面的唯一错误对象：

| 字段 | 类型 | 语义 |
|---|---|---|
| `domain` | string | `ErrorDomain` 名（`lib/include/astrocs/core/contracts.h`） |
| `exit_code` | int | 下表映射结果（`lib/infrastructure/cli/exit_codes.h` 的 `astrocs::ExitCode`） |
| `status` | string | 稳定错误码（`^[A-Z][A-Z0-9_]{0,63}$`） |
| `source` | string | 出错位置（模块 id 或仓库内相对路径） |
| `symbol` | string | 出错符号 |
| `message` | string | 脱敏后的中文诊断 |
| `run`/`node`/`phase` | string | 归属（与日志行同源字段） |
| `degraded` | bool | 本次运行是否发生过显式降级 |

**退出码映射（唯一，码值语义以 `exit_codes.h` 为准）**：

| `ErrorDomain` | 退出码 | 依据 |
|---|---|---|
| `CONFIG` | 2（ARGS） | 参数/配置错误 |
| `DATA` | 2（ARGS） | 数据合同违例；失败节点 manifest 的 `error_kind==input` 时改判 3（INPUT） |
| `SCIENCE_PRECONDITION` | 4（SCIENCE） | 科学验证/不变量失败 |
| `BACKEND` | 5（BACKEND） | ABI/签名/CPU 特征/加载失败 |
| `IO` | 7（IO） | I/O 失败；失败节点 manifest 的 `error_kind==disk_full` 时改判 10 |
| `RESOURCE` | 10（RESOURCE） | 磁盘写满/写盘失败 |
| `CANCELLED` | 9（CANCELLED） | 用户取消或超时 |
| `INTERNAL` | 70（INTERNAL） | 未分类内部错误；必须生成脱敏 crash report |

- **禁止第二套数值表**：本文档只做"域 → 码"映射，不复写码值含义；
- 未列出的域一律 70，并在 `status` 里给出可定位的稳定错误码；
- 现行实现的域映射偏差登记在 `eng/ci/ledgers/log_system_ledger.json#findings`（登记不改码）。

---

## 6. 显式降级登记要件

"降级"= 上游产物/能力缺失时改走替代路径并**继续运行**。降级合法当且仅当三要件齐备：

| # | 要件 | 判据 |
|---|---|---|
| D1 | **显式**：代码路径写出机器可读的 `degraded_reason` 字符串 | 函数体内出现 `degraded_reason` 赋值或写入 |
| D2 | **可溯**：manifest/provenance 记录降级事实与理由键 | manifest 含 `degraded_reason`（或 `degraded` 布尔 + 理由） |
| D3 | **不改科学语义**：降级不改变科学结果的定义（只改变执行路径或元数据完整度） | 科学语义改变 ⇒ 不是降级，必须 fail-closed 上行 |

`degraded_reason` 词表（小写下划线，稳定不变）：`photscale_incomplete`、`photscale_absent`、
`upstream_artifact_absent`、`optional_keyword_unparsed`、`cache_miss_recompute`。
新增词先在本文档登记再使用。`photscale_incomplete` 为**读侧保留词**（旧产物仍可解释），生产写侧不产生它：
测光拟合失败属**显式失败**，按 §5 口径逐帧记 `status=fail` + `error_domain`/`error_status`/`error`，
不写 `degraded_reason`（失败 ≠ 降级，判据见 §6 D1–D3 与 `docs/design/LOG_AND_ERROR_SYSTEM.md` §10）。

**禁止**：静默回退到低优先输入、静默保持缺省值、静默跳过校验。三者都是故障，必须上行到 CLI。

---

## 7. 落点合同

| 项 | 值 |
|---|---|
| 日志目录 | `log_dir`，默认 `<output_dir>/logs` |
| 机器日志 | `<log_dir>/run_<run_id>.jsonl` |
| 人可读摘要 | `<log_dir>/run_<run_id>.log` |
| 目录创建 | 幂等；由 L2 在 run_start 前创建 |

**禁止落点**：进程 CWD 相对路径、源码树目录（如 `lib/**/logs/`）、`run/`（那是开发/CI 过程日志）、
安装目录、用户家目录。判据 R3 与台账 `log_system_ledger.json#log_landing` 强制本约束。

---

## 8. 脱敏与大小上限

沿用 LOG-001 §5/§6，不另立规则：绝对用户路径/家目录/URL 凭据/密钥键值一律 `<redacted>`；
结构化字段只接受安全字符；单行 ≤ 4096 字节，超限按 UTF-8 边界截断。

---

## 9. 判据与负例

| 判据 | 机器入口 | 正例（绿） | 负例（红） |
|---|---|---|---|
| R1 错误不吞 | `check_log_system.py` | 生产收敛面无未登记吞错点 | 注入 `catch (...) {}` ⇒ FAIL |
| R2 降级显式 | 同上 | 条件回退点已登记且写 `degraded_reason` | 注入静默回退函数 ⇒ FAIL |
| R3 日志落点 | 同上 | 落点均派生自 `output_dir` 或已登记 | 注入 `run/logs/...` 字面量 ⇒ FAIL |
| R4 台账完整 | 同上 | 锚存活、条目只减不增、扫描面非空 | 抹掉锚 / 清空扫描面 ⇒ FAIL |
| R5 合同锚 | 同上 | 台账落点默认值与本文档、设计文档一致 | 改台账默认值 ⇒ FAIL |

命令：

```bash
python3 eng/tools/quality/check_log_system.py --json-out run/ci/log-system/log_system.json
python3 eng/tools/quality/check_log_system.py --self-test
```

注册项 = `CHK-LOG-SYS`（`eng/ci/checks.json`、`docs/ci/01_CHECKS.md` §2）。
