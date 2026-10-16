"""Preserve reviewed source documents, then install the curated rewrite."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
UNIT = ROOT / '实验/documentation-refactor'


def preserve(names):
    destination = UNIT / 'docs/OBSERVABILITY_SOURCE.txt'
    if destination.exists():
        raise SystemExit('Source archive already exists; refusing to overwrite')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with destination.open('wb') as out:
        out.write(('Reviewed sources before observability rewrite\ncommit=' + revision + '\n').encode())
        for name in names:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], input=raw, cwd=ROOT).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')


DOCUMENTS = {
    'docs/engineering/contracts/LOG_AND_ERROR.md': r'''# 日志与错误合同

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
''',
    'docs/engineering/standards/ERROR_MODEL.md': r'''# 错误模型

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
''',
    'docs/detail/infrastructure/log_and_error_system.md': r'''# 日志与错误系统

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
''',
    'docs/detail/infrastructure/observability.md': r'''# 可观测性

可观测性提供运行事件、日志、trace、资源采样和开发诊断，使调用者知道实际执行了什么。依据[最高设计](../../ACSD_DESIGN.md)、[日志错误合同](../../engineering/contracts/LOG_AND_ERROR.md)和[开发模式](../../engineering/architecture/DEV_MODE.md)。

## 通道与消费者

| 通道 | 入口/生产者 | 消费者和边界 |
|---|---|---|
| stdout 运行事件 | `lib/infrastructure/cli/jsonl.h` 与 `protocol.h` | GUI、外部运行器；`kind/sequence` |
| 结构化日志 | `observability/logging/log_event.py` 参考格式；Core logger 需适配 | 操作员和诊断；`event/seq` |
| 节点 trace | `TraceEvent`、Runtime 和调度器真实调用点 | replay、[运行图](../../engineering/resources/observability/RUN_GRAPH.md) |
| 生产资源记录 | `cli/resource_recorder.h` | `resource_timeseries.csv` 与运行摘要 |
| Python 资源伴随器 | `observability/monitoring/monitor.py` | `monitor_timeseries.csv`，独立列合同与指纹链 |
| 开发性能与快照 | `acsd/probe.h`、`acsd/dev_diagnostics.h` | 开发工作区；[探针规范](../../engineering/standards/DIAGNOSTIC_PROBES.md) |

通道之间通过 run/node/frame 身份关联。计划中的 worker、provider、哈希和入口不能填入真实观测字段。运行图有 `PLAN_ONLY` 节点不代表节点已执行。不同 CSV 不因都含 CPU/RSS 而互换。

## 调用与数据寿命

模块报告事件或只读观察 view，汇聚与运行层负责文件句柄、目录、队列和收尾。异步写出先复制或持有显式租约，保证科学对象回收后不再借用。模块不自行建日志线程池或选择任意落点。

正式日志默认 `<output_dir>/logs`，错误处理和运行策略集中在[日志系统](log_and_error_system.md)。快照位于独立开发工作区，不能成为下游科学输入。正式 cache/spill 按科学消费者生命周期保留，与开发快照分开。

## 资源测量

资源统计定义与评价归[性能模型](../../engineering/resources/PERFORMANCE_MODEL.md)，字段采集归[资源监控合同](../../engineering/resources/observability/RESOURCE_MONITORING.md)。CPU、内存和线程指标用于记录与分析，生产命令不因利用率阈值改变退出码。外部监控脚本的 `--gate-required` 是该脚本的分析模式，不成为项目研发审批或产品发布授权。

系统 iowait、进程块 I/O 等待、缺页速率、逻辑读取和物理读取分别报告。活动节点数不等于活动计算线程数；VmSize 不等于 Windows commit；无观测不能填真实零。遗留字段名保持可解释，但统计结论以实际来源和口径为准。

`ACSD_NODE_TRACE`、`ACSD_LEASE_TRACE`、`ACSD_P1CAP_TRACE`、`ACSD_UPM_PROFILE` 是已有调用路径的观察选择器，应受唯一开发编译边界控制；其各路径是否全部隔离仍需核对。`ACSD_SKY_MAX_ITER` 与 `ACSD_UPM_CG_MAX` 会改变算法迭代，属于实验数值旋钮，不能作为被动探针推荐给用户。

## 失败与验证

正式日志/事件写失败上行到 CLI；开发诊断写失败记录缺失原因。取消时有界收尾，错误保留原始归属。验证分别检查事件 schema、数据来源、实际调用计数、目录与哈希、缺失样本、并发寿命和 ON/OFF 科学结果，不能以 trace 行数代替科学验证。
''',
    'docs/engineering/resources/PERFORMANCE_MODEL.md': r'''# 性能模型与资源测量

本页定义并行预算模型、参数标定和测量解释。并行轴由[数据流](../architecture/DATA_FLOW.md)定义，采样字段由[资源监控](observability/RESOURCE_MONITORING.md)定义。优化首先保持[科学语义](../../science/unified/UNIFIED_SCIENCE_MODEL.md)与[数值约定](../standards/NUMERIC.md)，实际读数保存在实验记录。

## 1. 计算与资源原则

像元热点使用连续缓冲，避免逐像元分配、日志、文件访问和时钟。CPU 租约由调度器统一分配，模块不另建全局线程池。帧级并发和帧内并行共享同一预算；大数组、cache、队列与 scratch 容量纳入生命周期和内存预算。

快路径说明其等价范围和独立核验方法：Drizzle 候选裁剪不能漏真实交叠；UPM 缓存与原查询在同域同模型下比较；噪声权重用独立矩阵和 Monte Carlo 检查；Gaia 球面剪枝与穷举候选对照。配置值或代码注释不能代替数值比较。

稠密图像按最高设计默认 FP32，几何、标定和模型元数据 FP64。高精度 reference 与生产类型分别报告；误差容差由被估计量、量级与归约方式给出，不能用统一 FP32/FP64 标签证明科学等价。

## 2. 参数及标定

| 参数 | 配置来源 | 使用位置/含义 |
|---|---|---|
| 帧驻留字节系数 B | `eng/packaging/config/runtime_resources.json` 的 `frame_memory_gate.bytes_per_pixel` | `module_adapters.cpp::p1_memory_cap` 的准入估计 |
| 安全系数 s | 同文件 `frame_memory_gate.safety_frac` | 给基础驻留和波动留余量 |
| 内存预算比例 | 同文件 `memory_budget_percent` | 从实际可用内存派生预算 |
| 帧内宽度 | 当前线程租约与实际在飞帧数 | `p1_parallel_for` 分配剩余预算 |
| 每帧 scratch 上限 K | Drizzle 配置及实际 `num_threads` | 限制同时可用临时槽数 |

JSON 是参数配置的维护源；实现若仍保留字面量镜像，必须核对两侧，不能因文档称单一来源就认为已消费生成头。B 和 s 是对具体输入、类型和算法驻留的经验估计，换机器、几何和精度模式时重新标定；它们不是物理常数。

固定像素数 P 的峰值模型为 `RSS = base + P × b × m`，m 为实际并发帧数，b 为测得边际字节/像元。准入系数 B 与拟合斜率 b 的角色分别说明；拟合的样本量、残差、机器可用内存和范围保存在[工程实验](../../../实验/engineering-evidence/README.md)。旧正文读数原文保留于[重构来源档案](../../../实验/documentation-refactor/docs/OBSERVABILITY_SOURCE.txt)，未重新运行。

## 3. 并行宽度模型

设 A 为运行时可用内存，P 为单帧像素数，L 为正线程租约，n 为非空输入帧数：

```text
F = min(L, floor(s A / (P B)))
m = min(n, F)
I = max(1, floor(L / m))
W_model = m × min(I, K, num_threads)
```

该式要求至少一帧可准入；F=0 时由调度器处理不足，不能继续除以 m。它是资源上限模型，不是实测忙碌线程数或吞吐预测。未受 scratch/内核限制时，`m floor(L/m) ≤ L`，只有 m 整除 L 才等于 L；因此提高帧并发不保证单调提高忙碌宽度。

在 `p1_parallel_for` 设置线程 ICV、Drizzle 没有显式线程覆盖且实际 `num_threads=I` 的范围内，`K≥I` 才能保留模型的全部帧内宽度；K=I 是最小槽数，K 更大不增加这个上限。其他调用路径不能套用该等式。更多槽数增加驻留，科学归约与性能都需实际对照。

FITS 读取、writer、查询与求解存在串行段的程度取决于当前实现；它们是可优化的工程结构，不能描述为物理上不可并行。考察 writer/Drizzle 重叠和 stripe 效率时，分别测量 CPU、I/O 与驻留，不从预算代数推出最佳配置。

## 4. 资源统计与既有评价配置

利用率定义为 `busy_cpu_seconds / (window_seconds × allocated_workers)`。必须记录工作区间、采样间隔、实际租约来源和缺失样本；100% 进程 CPU 表示一核满载时，先换算到同一分母。有效 CPU 由亲和性与平台配额约束，不能使用机器逻辑核总数替代本运行容量。

既有分析参数保存在 `eng/contracts/resource_gate_v1.json`：适用域要求有效 CPU≥2 且活跃区间严格大于10秒；滑动低利用窗长度为≥10秒。均值85%、中位数90%、达到85%样本的比例70%为记录与解释项；连续低于60%并有积压的窗口在外部脚本中另有失败分类。产品进程内始终记录，不因这些 CPU、内存、线程指标改变退出码。

分母优先使用实际 `granted_workers`，没有租约观测时，既有外部执行器有显式 `selected_workers/available_cpus` 回落；两者皆无则记录 `allocated_capacity_undeclared`。声明容量、实际授予和实测忙碌不能混写。C++/Python 各执行器可能使用不同来源，尚不能称同一统计量已统一。

旧文档对低利用是否要求积压、未观测容量能否回落、活动节点能否代表线程、外部门退出10的适用条件有相互冲突的描述。保持现有代码与 JSON 数值，分歧列入[待推导问题](../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)，本轮不以文案选择算法。外部分析程序的返回码只表示该程序的结论，不构成研发审批、科学验收或发布授权。

## 5. 实验与验证

固定机器、输入、科学配置、二进制、ISA 与工作量，至少重复3次，报告 median/p95、样本数和散度。分别测 OFF、ON 无快照、性能探针、选中快照的墙钟与开销。比较线程宽度或 scratch 时，核对像元、WCS、科学元数据、参考 controls 与归约误差；遥测路径、时间戳和 run_id 单独处理。

`worker_balance` 依实际窗口内忙碌 worker 比例计算，不以 `(min+max)/2` 或常量替代。声明 `requires_monitor` 的实验缺采样时说明证据不足；声明改写工作区时保留输入输出身份与改动范围。校验器或回放执行器未提供时明确登记，不能把文档里的测试名当成可复跑命令。

性能结论给出输入、命令、失败尝试、计时边界、内存/I/O 来源、科学等价结果及适用域。优化前后差异需能解释，不能因目标利用率未达到就降低科学精度或凭空改阈值。
''',
}


if __name__ == '__main__':
    preserve(DOCUMENTS)
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
