# 日志与错误合同

本合同规定运行日志的工件、错误传播与显式降级。[错误模型](../standards/ERROR_MODEL.md)定义状态类别和进程退出码语义；[结构化日志](../resources/observability/STRUCTURED_LOGGING.md)定义日志行；[CLI 协议](CLI_PROTOCOL.md)定义 stdout 运行事件。三者按实际消费者连接。

## 1. 日志与事件的身份

| 对象 | 格式与消费者 | 责任 |
|---|---|---|
| 运行事件 | stdout JSONL，`kind` / `sequence`；GUI 和外部调用者 | CLI 发射器按协议生成 |
| 结构化日志 | `acsd.log.event.v1`，`event` / `seq`；操作员和诊断工具 | 汇聚层生成机器行与同源中文摘要 |
| 运行 trace | 节点、provider、worker、artifact 事件；运行图 | 实际执行点记录观测 |
| 开发诊断 | 快照与详细性能事件；开发分析 | 受 `ACSD_DEV_MODE` 编译边界控制 |

结构化日志每行一个 UTF-8 JSON 对象加 LF，包含换行最多 4096 字节。错误事件携带 `source`、`symbol`、`status`；构建 commit 来自运行二进制的真实来源。字段与脱敏细节引用结构化日志正本，不由 stdout 文本推测。

## 2. 运行日志工件

默认目录为块级 `<output_dir>/logs`，文件名为 `run_<run_id>.jsonl` 与 `run_<run_id>.log`。运行层创建目录，持有 writer，完成 flush、持久化、哈希和清单登记。模块提交事件，不拥有文件句柄。

`log_dir` 的显式覆盖必须解析到本块 `output_dir` 的可写域内；CWD 不参与默认目录推导。`log_level` 默认 `info`，取 `debug/info/warn/error`；`log_keep` 默认 `all`，或保留最近 N 次运行。它们是运行策略，取值通道及当前接线见[日志系统](../../detail/infrastructure/log_and_error_system.md)。

run manifest 的目标登记字段为 `log_artifacts[]`；每项描述一块日志集合：

| 字段 | 类型 | 约束 |
|---|---|---|
| `log_dir` | string | 相对本块 `output_dir` 的目录，默认 `logs`；与实际解析结果一致 |
| `files[].kind` | string | `jsonl` 或 `summary` |
| `files[].name` | string | 对应上述文件名，无路径分隔符 |
| `files[].sha256` | string | 实算 64 位小写十六进制 |
| `files[].bytes` / `lines` | integer | 非负，与磁盘一致 |
| `files[].level_counts` | object | `debug/info/warn/error` 齐全，总和等于行数 |
| `files[].truncated` | boolean | 截断事实；截断时记录原因与上限 |

成功、失败、取消均保留已取得的日志。解析配置前终止时可能没有合法输出目录，只能给 stderr 诊断；磁盘不可写时也不能承诺两个文件一定存在。此时报告原始失败及日志缺失，不能伪造完整性。完整日志登记的目标链为 `manifest → 文件哈希 → 日志内容`，manifest 不作为自身日志条目参与递归哈希。

该登记是工程目标；当前生产写出是否具备这些字段，集中列在[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。schema、Python 参考生成器或本字段表存在均不等于 CLI 已接线。

## 3. 错误对象与退出码映射

模块以 `Result<T>` 或其 C ABI 返回码上报。CLI 对外错误报告的目标字段为 `domain`、`exit_code`、`status`、`source`、`symbol`、脱敏 `message`、`run/node/phase` 和 `degraded`。`status` 使用稳定大写标识，词形为 `^[A-Z][A-Z0-9_]{0,63}$`。核心 `Error` 当前只直接保存域、消息和 cause；适配层补充归属，不能把目标报告字段都描述成 `Error` 的现有成员。

| `ErrorDomain` | 退出码 | 语义 |
|---|---|---|
| `CONFIG` | 2 | 参数或配置错误 |
| `DATA` | 2 | 数据合同违例；失败节点 `error_kind=input` 时改判 3 |
| `SCIENCE_PRECONDITION` | 4 | 科学前置条件或数值不变量失败 |
| `BACKEND` | 5 | 后端加载、ABI 或硬件能力错误 |
| `IO` | 7 | I/O 失败；失败节点 `error_kind=disk_full` 时改判 10 |
| `RESOURCE` | 10 | 磁盘写满或写盘失败 |
| `CANCELLED` | 9 | 取消或超时 |
| `INTERNAL` | 70 | 未分类内部错误，保存脱敏 crash report |

映射实现为 `lib/infrastructure/cli/runtime_client.cpp::exit_code_for_error_domain`；失败 manifest 的输入/磁盘分类在最终映射前处理。`INTEGRITY` 是进程退出码名称，当前 `ErrorDomain` 没有同名成员；manifest 验证命令可直接返回该码，不能虚构核心错误域。

## 4. 显式降级与能力状态

降级表示改用已定义的等价执行路径并继续运行。必须同时记录发生位置、`degraded_reason`、来源和科学影响，并在 manifest/provenance 中可追溯。若改变被估计量，必须按相应科学合同标明新的能力状态或失败，不能仍宣称原定义成立。

既有理由词为 `photscale_absent`、`upstream_artifact_absent`、`optional_keyword_unparsed`、`cache_miss_recompute`；`photscale_incomplete` 供兼容读侧解释旧产品，生产拟合失败不写成该降级。新增理由应与消费者和本合同同步。

未知误差、缺 precision 模型、局部无支持和该帧求解失败分别记录。合法 signal 的 precision unavailable 不必导致 signal 丢失；不可消费的损坏数据则明确报错。回退、默认值和跳过检查是否可用，取决于已声明合同，不能把它们全部笼统视为成功或故障。

## 5. 失败作用域与收尾

帧特有的 WCS、拟合或数据失败写入逐帧结果，其他独立帧按命令合同继续；全局配置、共享依赖或正式写盘故障在其作用域终止。组级布尔摘要不能替代逐帧结果，下游仅消费实际成功的对象。完整产品发布条件由[产品交换](../data/PHASE_PRODUCT_EXCHANGE.md)和[原子发布](ATOMIC_PUBLISH.md)规定。

正式日志写失败必须报告并使运行以非零结束；仅开发快照失败按[开发模式](../architecture/DEV_MODE.md)记录诊断缺失。日志创建/写入、清单写入、清单内容验证是不同步骤，按[清单合同](MANIFEST_VERIFY.md)保留各自错误；收尾失败不能覆盖最初科学故障的上下文。

验证包括并发顺序、两通道同源、脱敏、目录越界、只读目录、写满、取消和失败收尾，分别记录已执行范围。项目没有文档登记或测试数量构成的研发审批门。
