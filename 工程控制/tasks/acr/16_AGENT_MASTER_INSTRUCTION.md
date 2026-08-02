# Agent总执行指令

你正在现有AstroCS仓库的唯一`feature/astrocompute-runtime`分支继续完善ACR。若分支存在必须继续使用，不得创建新版分支、新仓库或第二套实现。

## 开始

1. 记录仓库、分支、HEAD、remote、main和工作区；
2. 检查`astro_toolkit.py`帮助和自检；
3. 阅读本控制包全部文档，优先读取`19_EXISTING_BRANCH_CORRECTION_TASKS.md`；
4. 建立算法路径guard；
5. 审计现有代码，保留有效CPU baseline和测试，不推倒重来。

## 绝对范围

只开发ACR API、任务特征、backend、Buffer、硬件画像Benchmark、成本模型、动态dispatcher、资源控制、经典实验、工具、文档和CI。严禁改任何现有算法、PipelineFrame、Orchestrator、正常CLI和OpenMP。

## 必须纠正

- 删除用户/正式API中的CPU/GPU比例；
- route profile从固定权重改为hardware profile曲线；
- public API必须真实进入dispatcher和backend；
- Benchmark必须覆盖详细能力族；
- CPU+GPU必须真实同时执行，不得用CPU模拟mixed；
- 利用率控制必须读取真实指标或明确估算，不得只喂人工数值；
- sanitizer必须实际启用；
- Evidence必须统一HEAD。

## 技术原则

- 最大复用开源项目；
- 公共API不泄露第三方类型；
- CPU-only永远可用；
- GPU可选隔离；
- 无画像CPU-only+警告；
- 固定画像、无在线学习；
- 任务特征推算+工作保持动态派发；
- 默认95%资源目标；
- 默认FP32。

## 实施和验收

严格执行`10_PHASES_TASKS_ACCEPTANCE.md`和`17_CLASSIC_EXPERIMENT_SUITE.md`。不能用几个简单测试替代全套能力验证。所有外部进程、编译、测试和硬件等待必须设置明确超时。

## 合并

全部通过后按`18_MAIN_MERGE_AND_DORMANT_INTEGRATION.md`合并到main备用。不完整、冲突不明、主线回归、算法越界或证据不一致时不得合并。

## 交付

按`13_DELIVERY_PACKAGE_RULES.md`交付控制包、合并后完整源码快照、统一Evidence和Merge Report，不得只交diff。
