# 日志与错误系统

本页把[日志错误合同](../../engineering/contracts/LOG_AND_ERROR.md)落实为产生、汇聚、写出与 CLI 收敛。[结构化日志](../../engineering/resources/observability/STRUCTURED_LOGGING.md)定义行格式，本页说明调用和生命周期。

## 组件与调用

| 层 | 输入与职责 | 入口或消费者 |
|---|---|---|
| 模块/节点 | 产生带归属的事件；返回错误，不决定进程退出 | `Result<T>`、C ABI 状态、节点 manifest |
| 日志汇聚 | 分配序号、过滤、脱敏、同源渲染 JSONL 与摘要 | `lib/infrastructure/observability/logging/log_event.py` 是参考生成器 |
| 文件写出 | 根据输出域创建目录、追加写、持久化、计算哈希 | 正式 I/O 经 AIO；运行层持有句柄 |
| CLI 收敛 | 解释失败 manifest、映射域、输出终止事件与 stderr 摘要 | `runtime_client.cpp::exit_code_for_error_domain` 及 `run_pipeline` |

`lib/include/acsd/core/logging.h` 的 Core logger、Python 日志行参考实现和 CLI JSONL emitter 是不同接口。适配必须显式映射 `component→module`、`message→diagnostic` 等语义；头文件存在不表示三者已形成一条生产双写链。

## 运行日志生命周期

1. 解析块级 `output_dir` 与运行日志策略，默认 `<output_dir>/logs`。目录覆盖须在本块可写域内。
2. 在节点执行前创建日志 sink，写开始事件；所有线程提交到同一序号分配边界。
3. 持续追加机器日志与同源摘要。运行事件走 CLI stdout，普通文本不混入该事件流。
4. 成功、失败或取消时停止提交并有界收尾，flush、持久化、关闭句柄。
5. 对实际文件计算字节数、行数、级别计数和哈希，按合同登记清单。仅对真实写成的文件声明完整。

开始前配置失败可能没有合法日志目录；磁盘故障可能阻止收尾。此时保留 stderr 和已取得的错误上下文，报告日志缺失。清单不能证明一个实际未写成的文件存在。

目录、文件名和登记字段引用日志错误合同。`log_level`、`log_dir`、`log_keep` 是运行策略；旧文档描述这些旋钮不代表现有 CLI 已提供对应旗标或环境变量，新增通道应与 CLI 文档和实际消费者同步。

## 错误传播

节点失败通过 `Result` 上行；`run_pipeline` 先读失败节点 manifest 的 `error_kind`，对输入和磁盘写满分类，再执行域映射。核心 `Error` 没有完整的 `error_report` 字段集，CLI 必须从实际节点上下文补充，缺失符号不能伪造。

正式产品、正式日志和清单写入失败属于运行故障；开发快照失败只影响诊断完整性。清单写入错误和已写出产品的完整性错误分别处理。最初故障与收尾次生故障都保留，不能用“日志写失败”抹去先前科学失败。

多帧节点按逐帧结果传递可消费对象；共享配置或依赖故障按全局作用域处理。精度不可用、测光拟合失败、WCS 无解和合法负信号具有不同语义，以相应科学合同为准。

## 并发、所有权与验证

sink 由运行层拥有，事件中的临时字符串和数组在异步处理前复制或转移所有权；结束运行后不得保留上次运行的句柄、run_id 或计数。事件序号与接受顺序一致，墙钟仅用于展示。

核对实际生成、取消、只读目录、写满、双通道一致和句柄释放。源码静态核对已确认域映射与 manifest 分类入口；完整 `log_artifacts[]` 生产写出仍需补齐，见[实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。本页未声明真实运行已经覆盖全部路径。
