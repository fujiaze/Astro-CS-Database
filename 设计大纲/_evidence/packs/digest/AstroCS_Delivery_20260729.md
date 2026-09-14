# 包取证摘要：AstroCS_Delivery_20260729

身份由脚本归一（去复原前缀与 .zip 后缀）。共 1 个实例。

## 【zip_recovered_from_history】history_zip/AstroCS_Delivery_20260729.zip
- 文件 10 | 21463.8 KB
- sha256: 5fd7add31d824c8af2effc00aa0bae1b9aac87061669895c7447c83155175e88
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': 'a78f5430f9fad9585b88e7ef9ac93c754d25725c'}
- zip 顶层: code, documents, 说明.md
- zip 内 marker: AUTONOMOUS_ENTRY.md
- 00_READ_FIRST 开头:

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

本包完整继承 v1.2 尚未完成内容：T1–T4 与主校

