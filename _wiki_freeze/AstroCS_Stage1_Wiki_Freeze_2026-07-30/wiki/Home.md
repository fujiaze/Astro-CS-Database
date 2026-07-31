# AstroCS 项目 Wiki

> 本 Wiki 是 AstroCS 项目的**唯一权威知识库**。当前唯一开发主线是 **Stage1**。
> Stage2、全量回归和发布工作均保持停止，直到 Stage1 功能、HISS、浏览器检查与性能分析完成并经用户审查。

## 当前状态

- **Stage1 主链**：已有可运行原型，已冻结规范见下方标准页面。
- **HISS**：已冻结唯一正式格式规范（XISF 式 Header + attachments，无 Footer）。
- **浏览器**：作为 HISS 科学检查工具，与 Stage1 并行适配。
- **性能优化**：暂不实施；功能冻结和正确性实现完成后，先做详细性能分析并提交报告。
- **Stage2**：停止开发。
- **710 帧回归**：不属于 Agent 自治任务，只有用户在当前对话中明确批准后才能启动。

## 权威规则

1. 用户最新明确意见优先。
2. 下方"Stage1 标准页面"中标记为 **已冻结** 的内容是当前开发依据。
3. Agent 不得自行修改已冻结决策；未决事项见 [Stage1-Decision-Status](Stage1-Decision-Status)，只做实验不冻结。
4. "原型完成""代码存在""测试脚本通过"不能等同于正式流水线完成。
5. Agent 执行规则见 [Stage1-Agent-Execution](Stage1-Agent-Execution)。

## Stage1 标准页面（权威来源）

### 范围与流水线
- [[Stage1 范围与流水线|Stage1-Scope-and-Pipeline]]
- [[Stage1 校准规范|Stage1-Calibration]]
- [[Stage1 测光与 SNR|Stage1-Photometry-and-SNR]]

### Drizzle
- [[Stage1 HEALPix Drizzle 规范|Stage1-HEALPix-Drizzle]]

### HISS 容器
- [[HISS 容器与 Tile 规范|HISS-Container-and-Tiles]]
- [[HISS 元数据规范|HISS-Metadata]]

### 决策与执行
- [[Stage1 决策状态|Stage1-Decision-Status]]
- [[Stage1 Agent 执行规则|Stage1-Agent-Execution]]

## 历史页面（仅迁移参考，不是规范）

以下页面为早期版本，**已被上方标准页面取代**，仅保留作迁移参考，不得作为实现依据：

- [[Stage1-范围与架构]] — SUPERSEDED by Stage1-Scope-and-Pipeline
- [[Stage1-校准规范]] — SUPERSEDED by Stage1-Calibration
- [[HISS-格式规范]] — SUPERSEDED by HISS-Container-and-Tiles + HISS-Metadata
- [[Stage1-Drizzle规范]] — SUPERSEDED by Stage1-HEALPix-Drizzle
- [[Stage1-CLI接口]] — 历史参考，CLI 契约见 `engineering_authoritative/contracts/CLI_CONTRACT.md`
- [[Stage1-浏览器检查]] — 历史参考
- [[Stage1-验收与性能分析]] — 历史参考
- [[Stage1-开发治理]] — SUPERSEDED by Stage1-Agent-Execution
- [[Stage1-待确认事项]] — SUPERSEDED by Stage1-Decision-Status
- [[Wiki-本地推送说明]] — 工具说明

## Stage1 固定数据链

```text
单色 Light
→ 选择一种校准模式
→ PlateSolve
→ 复用同一批星点
→ PSF
→ Gaia 光谱积分 / 测光校准
→ 稀疏 SNR 控制点
→ 显式或自动 NSIDE
→ 高精度 HEALPix Drizzle
→ HISS
```
