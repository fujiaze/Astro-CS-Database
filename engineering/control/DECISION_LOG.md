# 架构决策日志

| ADR | 状态 | 日期 | 主题 | 决策 |
|---|---|---|---|---|
| ADR-000 | ACCEPTED | 2026-07-24 | 工程控制模式 | 先建立基线、契约和验证 Gate，再继续功能开发 |
| ADR-001 | PENDING | - | Drizzle/Stack 源码纳管 | monorepo 导入或 Git submodule |
| ADR-002 | PENDING | - | PipelineFrame 唯一所有者 | astro_image_io 或 data_pipeline |
| ADR-003 | PENDING | - | Stage 2 节点模型 | 单节点、双节点或内部子阶段 |
| ADR-004 | ACCEPTED | 2026-07-24 | 根级构建系统 | 根级 PowerShell 编排器 + 各模块保留现有 build.ps1/Makefile（方案 B）|
| ADR-005 | PENDING | - | HCSD 质量通道 | 是否保存 coverage/weight/variance |
| ADR-006 | PENDING | - | float32 星检测接口 | 扩展 star_detector 或受控归一化适配 |

新决策使用 `templates/ADR.md`，不要直接在本表写长篇论证。
