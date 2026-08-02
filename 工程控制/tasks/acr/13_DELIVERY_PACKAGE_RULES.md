# 严格交付包规则

## 1. 必须交付四个逻辑部分

### A. Control Package

- 本控制包的最终执行副本；
- ADR；
- 依赖锁定和许可证清单；
- API、schema、Benchmark 和测试规范；
- 风险、决策和已知限制。

### B. Complete Source Snapshot

必须是结果 commit 的完整 AstroCS 当前源码快照，按原目录结构组织，足以脱离 Agent 本地仓库进行静态审查。

不得只交付 diff。不得包含：

- `.git`；
- Git 历史；
- 旧版本源码；
- 废弃副本；
- 构建产物；
- GPU SDK；
- 依赖缓存；
- 大型 Benchmark 临时数据。

### C. Evidence Package

至少包含：

- 构建日志；
- 测试日志；
- 经典实验结果；
- Qualification 样例；
- 设备报告；
- sanitizer/故障注入结果；
- path guard 报告；
- main 合并前后回归；
- 未通过或 SKIPPED 项目及原因。

### D. Merge Report

- base branch/commit；
- feature result commit；
- merge 前 main commit；
- merge commit；
- remote/branch；
- 冲突文件和处理；
- 合并前后测试；
- 是否保持 dormant；
- 是否修改算法目录（必须为否）；
- 后续算法集成建议分支名。

## 2. 文件清单和哈希

每个 ZIP 和总目录必须提供：

- `package_manifest.json`；
- `SHA256SUMS.txt`；
- 文件路径、大小、SHA-256；
- 生成时间；
- 基准/结果/merge commit；
- 工具版本。

生成后必须重新解压或校验 ZIP 完整性。

## 3. 依赖交付

- 提供 `dependency-lock.json`；
- 提供 SPDX 许可证和 NOTICE；
- 记录本地补丁；
- 不把 CUDA/ROCm/oneAPI SDK 放进交付包；
- 不提交包管理器缓存；
- 可包含小型 CMake FetchContent lock/patch，但必须可审计。

## 4. 源码快照规则

源码快照必须反映 merge 后 `main` 的完整最新状态，而不是 feature 分支旧状态。若 merge 因失败未执行，则明确交付 feature snapshot 并标记“未合并”，不得伪造 main snapshot。

## 5. 经典实验数据

- 生成器 seed 必须记录；
- 小型 golden data 可包含；
- 大数据应由脚本确定性生成；
- 不上传无必要的 GB 级结果；
- 原始 benchmark JSON、摘要和失败样本必须保留；
- 无硬件的后端标记 SKIPPED。

## 6. 禁止事项

- 不得仅交付修改文件；
- 不得仅交付补丁；
- 不得声称未运行的测试通过；
- 不得删除失败日志；
- 不得在交付包里包含凭据、绝对隐私路径或用户数据；
- 不得用“全部通过”替代逐项结果；
- 不得合并后继续在同一分支改算法。


## 7. 单一HEAD一致性

Evidence、源码快照、summary、JSON、测试日志、manifest和Merge Report必须来自同一个明确commit。生成流程开始后若发生任何代码或文档提交，必须全部重新生成。禁止把修复前的test_summary和修复后的文字报告混装。

## 8. 控制包命名

本项目未发布，不使用V1/V2/V3控制包名。最终权威文件名使用：

```text
AstroCS_ACR_Branch_Control_Package_2026-08-02.zip
```

后续同一任务修订直接更新该控制包内容；不要并列交付多个版本让Agent自行猜测。
