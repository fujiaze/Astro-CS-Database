# 错误模型

本标准定义状态分类、模块传播责任和进程退出码。[日志错误合同](../contracts/LOG_AND_ERROR.md)负责错误域映射、日志和降级记录；实际错误类型位于 `lib/include/acsd/core/contracts.h`。

## 1. 状态分类

| 类别 | 含义 | 处理 |
|---|---|---|
| 成功 | 所请求的对象可按声明语义消费 | 返回成功；状态为 `OK` |
| 可恢复科学状态 | 已声明的局部能力缺失或无支持，仍有有效结果或合法替代路径 | 成功返回并保留具体状态、支持域和能力 |
| 硬错误 | 当前请求无法按合同完成 | 非零/`Result` 错误，沿调用链传播 |

既有可恢复状态包括 `UNDERDETERMINED`、`NO_CANDIDATES`、`ALL_REJECTED`、`ZERO_VALID_WEIGHT`、`INPUT_TRUNCATED`。这些名称本身不授权任意继续运行：模块合同必须说明有效输出与后续可执行范围。空支持可以在局部是合法状态，在整份产品无内容时构成 `NO_DATA`。调用者同时读取返回值、status 和能力，不能只用字符串猜测结果。

`INPUT_TRUNCATED` 仅用于明确支持的截断解释；缺失、损坏或哈希不符的输入不能按它伪装成可恢复。未知误差不填零，负线性信号和有限真零不因符号而判错。

## 2. 类别、错误域与稳定状态

稳定状态使用 `^[A-Z][A-Z0-9_]{0,63}$`。业务类别包括 `CONFIG`、`INPUT_CORRUPT`、`DEPENDENCY`、`NUMERIC`、`NO_DATA`、`RESOURCE`、`TIMEOUT`、`IO`、`SCIENCE`、`INTERNAL`；类别与核心错误域分别具名。

当前 `ErrorDomain` 为 `CONFIG/DATA/SCIENCE_PRECONDITION/IO/RESOURCE/BACKEND/CANCELLED/INTERNAL`。`Error` 保存域、消息和可嵌套 cause；`Result<T>` 保存值或错误，`CancellationToken` 传递取消。模块内部异常转换成错误，异常不穿过 C ABI；模块不自行 `exit/abort`。默认值、忽略错误和数值填充只能按明确合同使用。

每个错误敏感入口说明输入非法、不可计算数值、外部依赖失败、写盘失败、取消的返回形态、对象清理及下游影响。错误包含节点/模块身份和可定位符号，排障入口为[排障说明](../../detail/infrastructure/troubleshooting.md)。这些是接口要求，不构成人为阻止研发构建的审批程序。

## 3. 进程退出码

声明源为 `lib/infrastructure/cli/exit_codes.h::acsd::ExitCode`。

| 码 | 名称 | 语义 |
|---|---|---|
| 0 | `OK` | 正常结束；局部能力状态另行记录 |
| 2 | `ARGS` | 参数或配置错误 |
| 3 | `INPUT` | 输入缺失、格式或哈希错误 |
| 4 | `SCIENCE` | 科学前置条件或数值不变量失败 |
| 5 | `BACKEND` | ABI、签名、CPU 特征或加载失败 |
| 6 | `COMPUTE` | 计算执行失败 |
| 7 | `IO` | I/O 失败 |
| 8 | `INTEGRITY` | 输出完整性或验证失败 |
| 9 | `CANCELLED` | 取消或超时 |
| 10 | `RESOURCE` | 磁盘写满或写盘失败 |
| 70 | `INTERNAL` | 未分类内部错误，保存脱敏 crash report |

CPU、内存或线程利用率不构成产品命令的资源超限门。诊断工具的裸整数返回值按该工具接口解释，不自动套用生产命令语义。核心头另有一份 `core::ExitCode` 镜像，当前仍需保持同步，不能由“唯一源”的文案断言源码已经无重复。

运行事件的模块数值码与进程码分开：20–28 对应 `STAR_DETECT_FAILED`、`PSF_FAILED`、`PHOTOMETRIC_FAILED`、`SNR_FAILED`、`STACK_FAILED`、`HISS_INVALID`、`HCSD_INVALID`、`MODULE_ABI_UNSUPPORTED`、`INPUT_INVALID`，扩展基数为 100。实际字段由[CLI 协议](../contracts/CLI_PROTOCOL.md)说明。

## 4. 阶段与失败责任

运行归属使用 `normalize/mosaic/export` 和真实节点 ID，节点定义见[模块导航](../../detail/registry/README.md)。旧日志中的 `P1.READ`、`P1.CALIBRATE`、`P1.STAR`、`P1.PSF`、`P1.PLATESOLVE`、`P1.PHOTOMETRIC`、`P1.NOISE`、`P1.DRIZZLE`、`P1.HIPS_WRITE`，`P2.COVERAGE/SAMPLER/UPM/REJECTION/INTEGRATE/HIPS_WRITE` 和 `P3.PROPERTIES/WCS/RESAMPLE/WRITE/VERIFY` 是兼容诊断标签，不规定科学依赖顺序，也不与当前节点逐一等同。

帧级失败、节点级失败和整次运行失败各自记录，不能用组级“至少一帧成功”掩盖失败帧。局部科学无支持、选择后的拒绝和基础设施故障也不能混用一个状态。错误验证使用实际触发条件与正反例，检查返回值、事件、清单、资源回收及半成品；单靠输出出现某个状态词不能证明传播正确。
