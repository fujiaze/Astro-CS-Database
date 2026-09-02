# AstroCS V6 系统重构控制包：执行入口

控制包版本：`V6.0`  
目标软件版本：`0.10.0-alpha.1`  
冻结源码基线：`main@587fe0e341a780da726917f40ed77f610de0c73f`  
生成日期：`2026-08-30`  
工作分支：仅 `main`

## 1. 任务目标

在不改变已冻结科学语义的前提下，把当前由多套编排器、临时 CLI 路径和旧工具拼接的工程，迁移为：

`统一 CLI → Pipeline Runtime → 类型化数据管道 → 模块注册表 → 科学模块 → 纯 CPU 后端 → I/O/Artifact Store`

本轮不是增加功能，也不是再做历史版本全量对比。本轮必须建立以后可长期迭代的工程基座，使 SCI、ALG、DATA、ARCH、API、MOD、源码符号、TEST 和现场证据形成机器可检查的闭环。

## 2. 文档优先级

发生冲突时严格按下列顺序处理，不得自行折中：

1. `01_ASTROCS_ENGINEERING_CONSTRAINTS.md`：最高工程约束，禁止 Agent 修改或重新解释。
2. 本控制包的任务台账、合同和门禁。
3. 当前仓库已冻结的有效 SCI/ALG/DATA 合同。
4. 当前源码只作为“现状证据”，不因代码已经存在就自动成为正确设计。
5. 历史报告、HANDOVER、自报 PASS 只作线索，不作为验收证据。

若 SCI/ALG 文档互相冲突，标记 `SCIENCE_CONFLICT`，完成不依赖该冲突的任务；仅在冲突会导致不同科学结果时请求负责人决定。

## 3. 执行前必须完成

- 验证控制包：`python3 scripts/validate_control.py .`
- 读取 `01`、`02`、`03`、`04`、`05_TASK_LEDGER.csv` 和当前任务对应合同。
- `git fetch origin --prune`，确认 `HEAD == main == origin/main`；否则停止代码修改并报告。
- 禁止创建分支；禁止 force push、reset --hard、覆盖外部修改。
- 将任务台账复制为仓库内 `evidence/refactor/TASK_LEDGER.csv`，状态只能按脚本规则迁移。
- 冻结起点清单、源码 commit、工具链、机器与外部数据引用；不把真实数据复制进仓库。

## 4. 连续执行规则

检查点用于自动判定和生成证据，不是人工停工点。除下列真实阻塞外，Agent 必须连续推进所有可做任务：

- 权限、凭据或必需数据缺失；
- main 与 origin/main 无法安全对齐；
- 科学定义存在会改变数值结果的冲突；
- 必须执行破坏性操作；
- 不可恢复的构建/硬件故障；
- 最终发布需要负责人进行 HiPS 视觉审核。

Windows 离线不是总任务阻塞。标记相关任务 `WAITING_WINDOWS`，继续 Linux、文档、静态分析、合成数据和不依赖 Windows 的工作。禁止像旧 CP0 一样为了等待普通外部审核而整轮停工。

## 5. 修改纪律

- 一个任务一个原子 commit；通过任务级验收后立即 push `main`。
- 科学、架构、性能、文档修复不得混在同一 commit。
- 不做全仓格式化；不顺手重写无关模块。
- 迁移使用适配器和影子验证，禁止大爆炸式重写。
- 旧实现只有在新实现通过合同、合成 Oracle、运行 trace 和受影响链验证后才能删除。
- 不改变科学公式；发现科学缺陷时新建独立任务并标记，不夹在架构提交中修复。

## 6. 绝对禁止项

- 生产重计算路径固定 `workers=1`，或在可用 CPU≥2 时长期单线程运行。
- 模块自行调用 `omp_set_num_threads`、硬编码 2/8/16/32 等线程数，或绕过 Runtime 线程租约。
- 硬编码 AVX2/AVX-512 为全局唯一构建目标；在不支持的 CPU 上尝试加载高级 ISA。
- 把 ACR 链接进本 Alpha 生产 CLI、Phase1 或 Phase2。
- 让 I/O 层承担 Pipeline 编排，让 CLI 直接调用科学内部函数绕过模块注册。
- 共享同一 CFITSIO `fitsfile*` 给多个线程，或多个线程同时写同一 FITS 文件。
- 用关键词匹配、旧报告或“能跑完”宣称科学正确。
- 反复运行旧版/新版 32R 全量对比；真实 32R 只在最终 Windows 候选上跑一次成功全链。
- 将源码历史、原始 testdata、build、缓存、崩溃转储或大文件塞进审核包。
- 修改任务依赖来隐藏 blocker；任务图变更必须有独立修订记录和控制包版本递增。

## 7. 完成定义

只有 `13_RELEASE_ACCEPTANCE.md` 全部门禁通过，且审核包通过 `scripts/validate_audit.py`，才允许状态为 `READY_FOR_OWNER_REVIEW`。只有负责人完成 HiPS 视觉审核并明确批准，才允许标记 `ALPHA_RELEASE_APPROVED`。

Agent 的最终汇报必须只引用现场生成的证据，列出：目标版本、最终 commit、原子提交、科学变更状态、架构门禁、数值门禁、资源门禁、Linux/Windows 结果、未验证项、审核包路径与 SHA-256。
