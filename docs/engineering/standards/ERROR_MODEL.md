# 错误模型标准

上游：最高设计的错误传播与日志一章、命令行合同一章的机器输出与退出码[1]。

本文规定错误的三层语义、违规判据、硬错误类别表、进程退出码与 error-sensitive 模块的必备要件。
域到退出码的映射面不在本文，映射的唯一正本是 `../contracts/LOG_AND_ERROR.md`[2]；本文定义码值语义本身。

## 1 三层语义

1. **success**：`rc=0`，状态为 `OK`；
2. **recoverable science status**：`rc=0`，`status` 字段取 `UNDERDETERMINED`、
   `NO_CANDIDATES`、`ALL_REJECTED`、`ZERO_VALID_WEIGHT`、`INPUT_TRUNCATED` 之一，
   产品仍有效或走显式 fallback；
3. **hard error**：`rc≠0`，`status` 字段取第 3 节硬错误类别之一。

三层之外没有第四种结果：任何未分类的失败按 `INTERNAL`（码 70）上行，并生成脱敏 crash report。

**两层的互斥判据**：第 2 层与第 3 层共用 `status` 字段与同一个机器可校验词形
`^[A-Z][A-Z0-9_]{0,63}$`，二者由**返回码**区分而非由词形区分。消费面的读法固定为
先读 `rc`，`rc=0` 时 `status` 必属第 2 层词表、`rc≠0` 时 `status` 必属第 3 层词表；
只读 `status` 不带 `rc` 的消费面不得对两层做判定，视为调用方缺陷。

因此第 2 层的 `INPUT_TRUNCATED`（输入在可辨识域内被截断，走显式 fallback、产品仍有效）
与第 3 层的 `INPUT_CORRUPT`（输入缺失、格式错误或 hash 不符，产品不可消费）是两个不同语义的
稳定码，不得互相顶替：把不可消费的输入按可恢复状态上行，等于让损坏输入继续参与计算。

## 2 违规判据

下列形态一律判为未传播，判红：

| # | 违规形态 | 判红理由 |
|---|---|---|
| V1 | `rc=0` + invalid status 双语义 | 一个返回值承载两套语义，调用方无法判定成功与否 |
| V2 | 以 warning 或 log 替代 error status | 失败不进入收敛面，退出码与实际结果不一致 |
| V3 | 默认 swallow 错误（`catch(...)` 空吞） | 错误在模块边界消失，溯源链断裂 |
| V4 | 静默截断或置零（损坏输入） | 损坏输入被当作有效数据继续参与计算 |

## 3 硬错误类别

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

## 4 错误域

机器面错误域 = `lib/include/acsd/core/contracts.h` 的 `ErrorDomain` 枚举：
`CONFIG` / `DATA` / `SCIENCE_PRECONDITION` / `IO` / `RESOURCE` / `BACKEND` /
`CANCELLED` / `INTERNAL`。

错误域是分类维度，退出码是对外收敛维度，二者的映射表唯一声明处 =
`../contracts/LOG_AND_ERROR.md` 的错误对象与退出码映射一节。

模块以 `Result` 报错，`exit` / `abort` 归进程宿主；异常在模块内部转为 `Result`，
不穿过 C ABI。`Result<T>` 携带 nested cause、serialization、cancel 与错误码映射。

## 5 阶段 ID

每个阶段有一个阶段 ID 前缀，逐节点一枚；节点名与 `docs/detail/registry/` 的注册表节点一致。
日志与诊断面统一使用本表的阶段 ID：

| 阶段 | 阶段 ID |
| --- | --- |
| normalize | `P1.READ` / `P1.CALIBRATE` / `P1.STAR` / `P1.PSF` / `P1.PLATESOLVE` / `P1.PHOTOMETRIC` / `P1.NOISE` / `P1.DRIZZLE` / `P1.HIPS_WRITE` |
| mosaic | `P2.COVERAGE` / `P2.SAMPLER` / `P2.UPM` / `P2.REJECTION` / `P2.INTEGRATE` / `P2.HIPS_WRITE` |
| export | `P3.PROPERTIES` / `P3.WCS` / `P3.RESAMPLE` / `P3.WRITE` / `P3.VERIFY` |

## 6 稳定错误语义

- C API：`0` = success；非 `0` = hard error，类别由调用上下文与 troubleshooting 条目定位；
- 可恢复科学状态经 `status` 字段表达，取值集合与第 1 节第 2 层一致；
- 每个 high-risk error 对应一条 troubleshooting 条目
  （`../../detail/merged_TROUBLESHOOTING.md`）。

`status` 字段的机器可校验形态为 `^[A-Z][A-Z0-9_]{0,63}$`；错误对象的完整字段表见
`../contracts/LOG_AND_ERROR.md` 的错误对象与退出码映射一节。

## 7 进程退出码

**退出码唯一源 = `lib/infrastructure/cli/exit_codes.h` 的 `acsd::ExitCode` 枚举**。
凡需要退出码数值处一律引用该头文件；本节的表与最高设计的机器输出与退出码一节同源[1]，
是全仓唯一一份码值语义表。码值在命令行协议上的对外可观测形态见
`../contracts/CLI_PROTOCOL.md`[3]；清单与校验面触发 hard error 的判定见
`../contracts/MANIFEST_VERIFY.md`[4]；错误事件与结构化日志字段的落盘形态见
`../resources/observability/STRUCTURED_LOGGING.md`[5]。

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

### 7.1 模块特定码

模块特定的非进程退出码走 JSONL 事件流的 `error.numeric_code` 面，与进程退出码分立，
保留区间 20–29，已定义 20–28：

```text
STAR_DETECT_FAILED=20    PSF_FAILED=21              PHOTOMETRIC_FAILED=22
SNR_FAILED=23            STACK_FAILED=24             HISS_INVALID=25
HCSD_INVALID=26          MODULE_ABI_UNSUPPORTED=27   INPUT_INVALID=28
MODULE_SPECIFIC_BASE=100
```

### 7.2 机器判据

本节 11 码与 `lib/infrastructure/cli/exit_codes.h` 的 `acsd::ExitCode` 枚举逐名逐值一致。
该一致性目前**无自动执行器**：仓内的文档一致性检查脚本集不含本项，核对由人读两侧表逐码对照完成。
第 7.1 节的模块特定码属 JSONL `error.numeric_code` 面，不参与该核对。

## 8 error-sensitive 模块的必备要件

每个 error-sensitive 模块必须同时具备：

| # | 要件 | 判据 |
|---|---|---|
| M1 | diagnostics stage ID | 每一失败节点都能给出第 5 节表中的阶段 ID |
| M2 | stable error category / code | `status` 为稳定码，`error.category` 取第 3 节类别之一 |
| M3 | troubleshooting 条目 | 条目存在且可由错误码直达（`../../detail/merged_TROUBLESHOOTING.md`） |

三项缺任一 ⇒ 该模块不进入生产构建。

## 9 稳定错误码族登记

稳定错误码族 `ERR-*` 的登记面 = `../governance/TRACEABILITY.md` 的 `error_codes` 列。

以 `ERR-P2-UPM-001`（UPM 模型文件畸形）为例：`frames` 非数组、`frames` 内重复、
帧数与 C 行数不等，都在模型打开时判错，并沿 `p2_upm_open` 上行
（实现 `lib/algorithms/coverage/src/upm.cpp` 的 frames 校验）。

## 参考文献

[1] 内部文档 `../../ACSD_DESIGN.md`，最高设计的机器输出与错误传播两章。

[2] 内部文档 `../contracts/LOG_AND_ERROR.md`，错误域到退出码的映射正本与落点合同。

[3] 内部文档 `../contracts/CLI_PROTOCOL.md`，命令行协议合同。

[4] 内部文档 `../contracts/MANIFEST_VERIFY.md`，运行清单与校验合同。

[5] 内部文档 `../resources/observability/STRUCTURED_LOGGING.md`，结构化日志合同。
