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

## 6. 修订记录

- rev1（2026-09-09）：初版，25 任务。
- rev2（2026-09-10，执行侧）：接入负责人三项裁决与 P1 架构抽验——新增 `WCS-003`、
  `BASE-UTIL-001`、`P1-HIPS-DIGEST-001`、`ARCH-AUDIT-P1`；G-SCI/RT-001/AUD-001 门禁接线。
- rev3（2026-09-10，负责人真实数据验收指令）：见 `04_OWNER_DECISIONS_20260910.md`——
  新增 `REAL-000`（数据审计/索引 v1.2/确定性匹配计划/缺口量化）；扩写 `REAL-001`
  （全量校准解析 + M42/银心完整 Phase1/Phase2 + 本地真实 GaiaDR3/GaiaDR3SP）；扩写
  `VIS-001`（固定拉伸初审）；`WIN-001`（同候选真实数据复验子集）。任务总数 30。
- 状态真相源：`工程控制/ACTIVITY_STATE.md` 与执行态 TASK_STATE；`TASK_LEDGER.csv` 状态列
  为 rev3 同步快照（2026-09-10），后续以活动状态文件为准。
- rev3.1（2026-09-10/11）：REAL-001 增三级成功率硬门（parse/校准/solve 各≥99%）、
  九宫格（中心+四角+四边中点）solve 视觉抽检、Phase3 平面导出+拉伸预览接入；
  VIS-001 增九宫格逐格视觉判定与 Phase3 视觉核验节。
- **05_FINDINGS_REGISTER_20260911.md：下一轮控制包必须把 STD-F1/F2/F3 与 STD-REG-001
  转为任务或负责人裁决项**。
- **06_V2_PACK_DESIGN_20260911.md：第二阶段控制包编制总纲（三层 CI 拓扑/并行分组/
  CI 修复常驻线/轮报义务）。下一轮按此编制 V2 控制包，V1 剩余任务并入。**

## 7. V2 重编与启动快照制（rev5，2026-09-11）

负责人指令：**不搞复杂冻结**——前台 Agent 每次启动时把当前状态写入
`run/pack_state/STARTUP_SNAPSHOT.json`（HEAD/origin/main/三 SHA、dirty 清单、台账状态、
上一轮 CI 结果路径）即可，你和 Agent 共用同一工作区，不设额外冻结仪式。

V2 谱系（47 任务）：V1 已闭环 22 项以 PASSED 并入；新任务 25 项按
`06_V2_PACK_DESIGN_20260911.md` 六条线组织（CI 订正/治理标准/科学缺陷/架构/真实数据/收口）。
门禁链：G-STD→G-SCI→G-CODE→G-CI-L1→G-REAL-L2→G-FAT-L3→G-RELEASE。

核心要求（负责人原文口径）：
1. 所有模块符合架构设计文档（docs/architecture/）；2. SCI/算法文档/代码三者统一；
3. 合成测试全部通过且正确（known-failures 基线机器化）；4. 按三层 CI 跑全量测试：
   GitHub 双虚拟机（编译+合成）→ 本机 Linux（真实数据）→ Fatduck（Windows 真实数据）；
5. 主线开发与 CI 报错修复并行（CI-REPAIR 常驻线，每轮拉回上一轮结果）。

## 8. rev6 重编（2026-09-12，前台接续执行）

接续上一前台会话中断处（该会话末态：HEAD=`54287b48`，SCI-F2-001 已部分交付并 review_pass，
SCI-F3-001 已派发但子代理随会话中断而孤立）。rev6 的编制依据是**实测**而非推测：

- **CI 红灯事实源**：前台亲自回拉 HEAD 的 GitHub Checks 全量结果（`run/ci_repair/round4/`），
  linux-main 实测 14 项非 PASS、windows-main 9 项非 PASS，并**逐条本地复现根因**后才落任务；
- **裁决留档**：新增 `07_FRONT_DESK_RULINGS_20260912.md`（R-01…R-14 + B-01…B-04），
  **所有重要裁决一律留档**；能依宪章裁决的一律裁决，只有宪章无法裁决且属负责人专属权限的才登记 `BLOCKED_EXTERNAL`；
- **解除旧 BLOCKED_EXTERNAL**：`STD-F1-ADJ`（→R-02 方案 b，转 repo-write 正常派发）、`WIN-000`（→B-03 仅数据授权待裁）；
- **lane 拓扑变更（R-01）**：`ci-repair` lane **停止派发新任务**，全部 tracked 写归 `repo-write`
  （capacity 1），依宪章 §14.5「同一工作区 tracked 文件写入必须串行」；真实并行度来自
  `read-only`(4) + `realdata`(1) + `windows`(1)（只写 gitignore 区）。
- **写域补正（R-03）**：`SCI-F2-001` 写域由 `lib/phase2/` 修正为
  `lib/core/src/module_adapters.cpp;lib/phase2/;tests/`（真实修复面）；新增 `CORE-RACE-001`。

任务谱系 47 → **57**（新增 10 项 CI 承接任务，见 `07` §R-04 红灯→任务映射表）。
门禁链增加 `G-CI-FIX`（10 项 CI 承接任务全通过），`CI-002` 以 `requires: G-CI-FIX` 接线。

**派发优先级（repo-write 串行队列）**：
`GOV-AGENTS-001`(250) → `CI-DATA-REG-001`(245) → `CI-VER-CHK-001`(240) → `WCS-PATH-001`(235)
→ `ARCH-TB-001`(230) → `CORE-RACE-001`(228) → `CON-COMMENT-001`(225) → `CI-WIN-001`(220)
→ `CI-BACKEND-001`(215) → `CI-REPAIR-002`(210) → `STD-REG-001`… 既有队列。

**前台义务不变**：逐任务机器验收、精确暂存、原子 commit 并 push、`fetch` 后核对三 SHA；
SubAgent 不 commit/push/改总台账；预存 dirty 继续登记不收编。
