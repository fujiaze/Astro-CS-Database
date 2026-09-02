# AstroCS v1.3 修复与续开发自治入口

你是本仓库的长期工程 Agent。立即执行，不要只给计划，不要要求用户手工解压、选择任务或重复确认。

## 1. 安装、迁移与恢复

1. 将当前目录视为 AstroCS 仓库根目录，先记录 Git 分支、HEAD、工作树和用户未提交修改；不得覆盖、删除或提交用户原有修改。
2. 查找最新且结构有效的 `AstroCS_Recovery_Development_Pack*.zip`，解压并将包根目录保留为 `工程控制/`。
3. 运行：

   `python 工程控制/tools/migrate_from_v12.py --repo . --new-root 工程控制`

   该步骤必须迁移 `engineering_v1.2/` 的任务状态、证据与检查点，不得把已完成任务重置为 TODO。若 P11-004 为 DEFERRED/BLOCKED/FAILED，则恢复为待执行修复任务。
4. 运行：

   `python 工程控制/tools/validate_pack.py 工程控制`

5. 阅读顺序：`README.md` → `docs/00_PHASE_GOAL_AND_BOUNDARIES.md` → `docs/23_P11_004_REVIEW_DECISION.md` → `docs/24_WCS_VALIDATION_V2_SPEC.md` → `docs/26_P11_RECOVERY_RUNBOOK.md` → Agent 规则 → 状态文件 → 当前任务。

## 2. P11-004 强制裁决

当前证据不能证明 WCS 生产端错误。旧诊断把求解器 RANSAC inlier 对换成全星表 kd-tree 最近邻对，导致误配、饱和星和质心偏差进入残差，不能与 IPV RMS 直接比较。

必须实施“双层闭环”：

1. 固定求解器的权威 inlier 对应关系；
2. 仅使用最终序列化到 Header/PipelineFrame 的标准 WCS/SIP，独立把这些 Gaia 星回投到像素；
3. 比较外部 WCS 回投与 detector 坐标，同时比较外部 WCS 预测与求解器内部预测；
4. 全星表重新匹配只作为二级诊断，不再作为 P11 硬 Gate。

分支规则：

- 权威星对闭环通过：P11-004 以 `NO_CODE_CHANGE_REQUIRED` 完成，禁止为通过 Gate 修改 CD/SIP/CRPIX；
- 权威星对闭环失败且出现一致的符号、旋转、尺度或位置误差：才允许在 WCS 生产端最小修复；
- 无论哪一分支，均进入 P11-005 的 710 全量回归。

## 3. 续开发范围

本包完整继承 v1.2 尚未完成内容：T1–T4 与主校准帧整理、测光修复、SNR/HISS、全 TestData Stage1、银心三片 32 帧梯度/叠加、浏览器异步 Tile 与 GPU Renderer、统一回归和用户结果展示。

浏览器工作可与 P11/P12 调查并行，但同一时刻只能有一个主代码修改任务；先统一接口再并行无依赖工作。

## 4. 执行与停止

每项任务：事实基线 → 失败复现 → 最小实现 → 分层测试 → 真实数据 → 证据 → 隔离复核 → 状态更新 → 下一任务。所有外部进程、网络、构建和可能阻塞的 Python 操作必须设置明确超时。

不得因任务完成而停止。仅在全部任务完成，或缺少不可替代源码/数据/凭据/硬件且已生成 `BLOCKED_REPORT.md` 时汇报。
