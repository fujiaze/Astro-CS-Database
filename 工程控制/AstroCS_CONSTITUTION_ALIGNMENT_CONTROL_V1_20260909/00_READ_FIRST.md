# 00｜唯一执行入口

控制包 ID：`ASTROCS-CONSTITUTION-ALIGNMENT-V1`  
生成时间：`2026-09-09T15:34:42Z`  
项目：AstroCS  
基线：`789c5b6cec5e7b25f6c13e41c55a0a2e8e905cd9`（HEAD/main/origin/main 一致）  
正式分支：`main`

## 1. 本轮目标

以项目负责人提交的 `ASTROCS_PROJECT_CONSTITUTION.md` 为目标上位规范，先消除治理状态冲突，再按原子任务修复当前仓库在科学正确性、真实模块化、AIO、并行资源、CLI、CI、真实数据和发布方面的差距。最终只产出 `READY_FOR_OWNER_REVIEW` 或诚实的 `NOT_READY/BLOCKED_EXTERNAL/PACKAGE_INVALID`，任何 Agent 不得宣布发布。

## 2. 执行前硬门（G-GOV）

用户称宪章“已经冻结”，但收到的仓库文件 SHA-256 为 `37fefdcc64ebb83c35a8fc3115e96d427a6c0986ed8220d53335f1468e2749a3`，正文第 4、651–660 行仍为 `DRAFT_FOR_OWNER_REVIEW` 且四项待确认；现行 `AstroCS_ENGINEERING_CONSTRAINTS.md` SHA-256 为 `b3c66eb542a1f03dd82e26279299fe8210c68c488cc2c77b4c4cf9510d4e7089`，仍声明 `ACTIVE_NORMATIVE`。

因此执行任何修复任务前，负责人必须以仓库内可机器验证的提交完成：宪章状态 `FROZEN`；裁决 Phase3 投影、CPU 门限、Alpha 范围、插件范围；明确 supersession；解决 Linux 职责、资源阈值、worktree 双值；登记根白名单与活动索引。Agent 不得代裁。`GOV-001` 未通过时其余写任务全部等待。

## 3. 基线保护

- dirty 状态指纹：`e271debba378057f09f2bd516aa89c942cbc08140700fd17390a6573ba383713`。
- tracked index 指纹：`eb01dce9382c4f7c76001b927d3318a400f7c6d2d3b930c6feefaaa15a11aa3f`。
- 当前有预存修改和未跟踪产物；不得 reset、stash、clean、覆盖或顺手收编。
- 只在现有工作区、现有 main；不得 branch/worktree/clone。
- SubAgent 不 commit/push/改总台账；前台逐任务机器验收、精确暂存、原子提交并立即 push。

## 4. 启动顺序

1. 运行两个 validators；2. 执行只读 `BASE-001`；3. 等待负责人完成并验收 `GOV-001`；4. 按 control-pack 图派发；5. G-CODE→G-CI→G-REAL→G-RELEASE；6. 独审、审核包、Owner 裁定。

## 5. 完成定义

宪章与活动规范单一；P0/P1=0；三 Phase 可用性与产物一致；每 DAG 节点唯一真实入口且 call_count=1；AIO 唯一且原子；唯一 executor 与实测资源门；同 SHA Linux/Windows CI；Linux 全 testdata/Gaia、M42/银心与图像初审；Fatduck 同候选复验；安装树/ABI/hash/version/provenance 一致；审核包白名单和 SHA256 通过。
