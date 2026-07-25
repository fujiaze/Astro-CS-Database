# P00 基线冻结与仓库完整性恢复

## 目标

把“作者本机能看到的项目”变成“仓库和清单能完整描述的项目”。

## 任务

### P00-001 基线预检

只读生成 commit、模块、缺失目录、构建、测试、tag、CI、文档冲突报告。

### P00-002/P00-003 Drizzle/Stack 源码纳管

分别确认：

- 正确远端仓库；
- 实际使用 commit；
- 与 orchestrator API 的匹配版本；
- 许可证；
- 纳入 monorepo 或 submodule；
- 构建和最小测试。

不得直接抓“最新 main”代替当前实际版本。

### P00-004 依赖图

生成源码模块、DLL、头文件、运行库、数据依赖和调用方向。

### P00-005 环境基线

采集 PowerShell/Python/GCC/Qt/GSL/zstd/lz4/OpenMP 等实际版本。

### P00-006 旧审计复核

163 项逐项标记：

- OPEN：当前源码仍存在；
- CLOSED：已有代码和测试证据；
- STALE：路径/架构已变化；
- UNVERIFIED：源码/数据缺失；
- REJECTED：原硬约束无有效来源或被 ADR 否决。

### P00-007 文档冲突

至少复核：monorepo、Stage 编号、SNR 块、Stack 节点、模块状态、已修 GAP。

### P00-008 baseline tag

G0 通过后创建第一个 baseline tag，并冻结证据。

## G0 Checklist

- [ ] 13 个实际运行模块/子模块源码均受控；
- [ ] 每个依赖固定版本；
- [ ] 当前工程可否构建有明确证据；
- [ ] 旧审计已复核；
- [ ] 文档冲突已登记；
- [ ] 风险和阻塞清晰；
- [ ] baseline tag 与 SHA-256 完成。
