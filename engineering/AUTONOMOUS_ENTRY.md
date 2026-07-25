# AstroCS CLI 内核自治开发入口

你是本仓库的长期工程 Agent。当前目标不是开发 GUI，而是把 AstroCS 做成可由 GUI/JavaScript 稳定控制的原生 CLI 算法内核，并使用真实数据完整跑通 Stage 1 与 Stage 2。

## 启动动作

1. 将本压缩包解压到仓库根目录的 `engineering/`；若已有 `engineering/`，合并文档但不得覆盖 `control/PROJECT_STATE.yaml`、`control/CURRENT_TASK.md` 和既有证据。
2. 读取顺序：
   - `README.md`
   - `docs/00_PRODUCT_AND_CURRENT_GOAL.md`
   - `docs/01_BASELINE_AND_KNOWN_GAPS.md`
   - `docs/02_CLI_CORE_ARCHITECTURE.md`
   - `docs/03_END_TO_END_DATAFLOW_AND_LIFETIME.md`
   - `docs/05_STAR_DETECT_PSF_DEDUP_SPEC.md`
   - `agent/MASTER_AGENT_INSTRUCTIONS.md`
   - `control/PROJECT_STATE.yaml`
   - `control/CURRENT_TASK.md`
   - 当前任务文件
3. 先核查仓库、分支、工作树、依赖模块和真实数据，不要先改代码。
4. 从 `control/CURRENT_TASK.md` 指定任务开始，按依赖顺序持续推进，不等待用户逐项确认。
5. 每个任务必须有实现证据、测试报告、独立复核和状态更新；仅在真实硬阻塞或全部完成时停止。

## 最高优先级边界

- 当前产品核心是 CLI + Orchestrator + PipelineFrame + 算法模块。
- GUI 后续通过 JavaScript/进程协议控制 CLI，不直接访问 DLL 或图像内存。
- PowerShell、Python 仅是开发辅助，不属于产品运行时主链。
- 真实数据跑通优先于继续堆新功能。
- 必需模块缺失、数据块缺失、算法被跳过或静默降级必须判定失败。
- 重复星点检测必须按 `docs/05_STAR_DETECT_PSF_DEDUP_SPEC.md` 修复：一次检测，板解算与 PSF 共用同一 `star_det`。
