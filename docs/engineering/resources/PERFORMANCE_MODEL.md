# 性能模型与资源测量

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
