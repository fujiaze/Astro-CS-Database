# Agent总执行指令

你正在现有 AstroCS 仓库的唯一 `feature/astrocompute-runtime` 分支继续纠正和完善 ACR。若分支存在必须继续使用，不得创建新版分支、新仓库或第二套实现。

## 开始

1. 记录仓库、remote、main、feature、base、HEAD和工作区；
2. 检查 `astro_toolkit.py` 帮助、自检和可复用工具；
3. 阅读本控制包，先读 `20_PHASE_I_AUDIT_ACTION_PLAN.md`；
4. 建立算法路径guard；
5. 审计现有代码，保留oneTBB、hwloc、cpu_features、CPU ISA、Buffer/Event和有效测试；
6. 所有外部进程、编译、测试和硬件等待设置明确超时。

## 绝对范围

只开发 ACR API、TaskDescriptor、backend、Buffer、HardwareProfile Benchmark、模型拟合、CostEstimator、动态Dispatcher、资源控制、独立工具、经典实验、文档和CI。严禁修改任何现有算法、OpenMP、PipelineFrame、Orchestrator和正常CLI。

## 必须先纠正

- 删除 per-kernel `preferred_backend/routes.json`；
- HardwareProfile取代RouteProfile；
- OperationId/TaskTraits不得被忽略；
- Public API真实进入CostEstimator和backend；
- Benchmark覆盖完整能力族；
- Mixed必须真实CPU+GPU，无GPU则SKIPPED；
- 95%控制读取真实指标或明确估算；
- Sanitizer实际开启；
- Evidence统一HEAD。

## 技术原则

- 最大复用成熟开源库，但不重复引入重叠运行时；
- 公共API不泄露第三方类型；
- CPU-only永远可用；
- GPU插件可选隔离；
- 无画像CPU-only+非阻断警告；
- 画像只读、无在线学习；
- 默认FP32；
- 默认CPU/GPU利用率目标约95%；
- 不允许用户任务份额参数。

## 合并

严格执行阶段和测试矩阵。全部通过后按 `18_MAIN_MERGE_AND_DORMANT_INTEGRATION.md` 合并到main备用。未完成、真实GPU未验证、主线回归失败、算法越界或证据不一致时不得合并。

## 交付

按 `13_DELIVERY_PACKAGE_RULES.md` 交付控制包、完整源码快照、统一Evidence和Merge Report，不得只交diff。
