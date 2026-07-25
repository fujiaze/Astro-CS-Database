# AstroCS CLI Core Development Pack

本包用于指导 Agent 将现有 AstroCS 算法仓库收敛为可用的原生 CLI 内核。当前阶段的完成定义是：

1. 使用真实 FITS/XISF 和真实校准帧，从 CLI 稳定生成数值与元数据正确的 HISS；
2. 使用多份真实 HISS，从 CLI 稳定生成可重新读取、可浏览、可验证的 HCSD；
3. Orchestrator 成为控制命令、PipelineFrame、算法模块和未来 GUI 之间的唯一编排入口；
4. CLI 提供稳定机器协议、严格失败语义、进度事件、日志、取消与恢复边界；
5. 所有关键数据块均有唯一生产者、明确格式、所有权、生命周期和兼容版本。

## 目录

- `docs/`：产品边界、架构、数据流、测试、发布等 Spec。
- `contracts/`：数据块、CLI 协议、错误码和接口注册表。
- `tasks/`：可直接执行的分阶段任务。
- `checklists/`：阶段验收清单。
- `control/`：当前任务、状态、依赖、风险与追踪表。
- `agent/`：自治执行、复核、阻塞和恢复规则。
- `templates/`：任务、测试、证据和 ADR 模板。
- `reference/proposed_api/`：重复检测修复所需的参考 ABI 草案。

## 执行原则

- 不进行无证据的大规模重构。
- PlateSolve 先冻结全量 TestData 和旧路径基线；共享 detections 路径仅在逐例无退化时启用，否则保留原内部检测路径并导出同次检测结果。
- 每次只推进一个主任务；并行只用于互不修改同一接口的调查、测试或复核。
- 接口与格式变更必须同时更新契约、兼容测试、迁移说明和回滚方案。
- 当前任务从 `control/CURRENT_TASK.md` 读取。
