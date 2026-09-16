# 任务：ROOT-001 仓库根清洁：全条目账本、处置分类与执行

状态：`NOT_STARTED`
层：L0　依赖：BASE-001　文件域互斥组：S1-R

## 目标

让仓库根**在物理上**只保留 ENGINEERING_SPEC §7 允许的条目：为当前全部顶层条目建立账本与处置分类，执行可确证的低风险清运，其余登记为待裁决。目标不是「git 干净」，而是**根目录干净**。

## 基线状态（编制时实测）

- 实测顶层 **133** 个条目（tracked 46 / untracked 87），远超 ENGINEERING_SPEC §7 白名单。
- 未跟踪杂项： `astrocs_run_*.json` **62** 个、`astrocs_p1sess_neg/perf/props/test`、`alloc_report.json`、`alloc_samples.csv`、`resource_samples.csv`、`resource_summary.json`、`worker_balance.csv`、`run_context.json`、`p8-files.patch` 等 5 个 patch、`build/` 6.0G、`out/`、`logs/`、`Testing/`、`.pytest_cache/`、`graph/`、`worktrees/`、`CS/`、`Database/`、`AstroCS.wiki/`、`engineering/`（空）。
- 未跟踪数据区：`BASS DR3/` 58M、`GaiaDR3/` 41G、`GaiaDR3SP/` 64G（均在 .gitignore 内，属用户数据）。
- 未登记但 tracked 的资料目录：`设计大纲/` 101M、`问题扫描/` 20M。
- 磁盘：/workspace 503G 已用 293G；其中 `run/` **102G / 634993 文件** 为临时产物（保留策略见 ROOT-003，本任务不动 `run/` 内部）。
- `.gitignore` 已含 2026-09-09 的根目录整洁兜底段，但**无机器门**证明根目录长期干净。

## 负责人裁决（2026-09-16，已授权执行）

- `设计大纲/`：**已确认无用，删除**（原为历史溯源资料，101M）。
- `run/` 与根目录一次性产物：**均可清理**（ROOT-003 不受 >1G 限制，见该任务卡的裁决记录）。
- `问题扫描/`：**不在本任务处置**——它是上一轮针对旧基线的 bug 清单，移交 ROOT-004 做基于最新权威的订正与重建。
- `VISUAL_CHECK_README.md`、`FATDUCK_ACCESS.md`、`CHANGELOG.md`：由 ROOT-004 先核对内容再定去留。
- `VERSION` 与「Alpha 前移除版本信息」：**不属本控制包**，登记进下一轮工程包（GOV-001 只记录，不动手）。

## tracked 条目的处置口径（负责人裁决 2026-09-16，消除二义）

- 默认：tracked 条目**只登记处置建议**，不动手（避免与 GOV-001/DOC-001 撞域）。
- **例外（负责人已明确授权，可直接在工作区删除/归档，由前台提交）**：
  - `设计大纲/`（tracked 344 文件 / 86.3 MB）：**已确认无用 → 删除**；删除前把 tracked 文件清单与 sha256 记入 ROOT_LEDGER 与 `reports/PROJECT-GOVERNANCE-01/root/deleted-manifest/`。
- 工作方式：SubAgent 无 git 写权限，故**删除工作区文件即可**（`git rm` 也不需要）；前台会把这个删除作为独立 commit 提交（`git add -A 设计大纲` 的删除记录）。
- 仍属「只建议不动手」的 tracked 条目：`VERSION`、`CHANGELOG.md`、`FATDUCK_ACCESS.md`、`VISUAL_CHECK_README.md`、`REVIEW.md`、`HANDOVER.md`、`ASTROCS_PROJECT_CONSTITUTION.md`、`AstroCS_ENGINEERING_CONSTRAINTS.md`、`问题扫描/`（归 ROOT-004）。

## 权威依据
- ENGINEERING_SPEC.md §7（仓库根固定条目；任何新产物必须落位到对应目录，禁止散落根目录；确需新增根条目先登记并经负责人确认）
- ASTROCS_DESIGN.md §6.3（运行产物只落配置 output_dir，不得以进程 CWD 作隐式缺省写出）
- ENGINEERING_SPEC.md §6（预存修改先登记，不自动清理）
- CONTROL_PACK_SPEC.md §7.2（验收记录与证据）

## 改动范围（文件域）
### 允许改
- 仓库根散落文件与目录的处置（删除 / 移动 / 归档 / 保留并登记）
- 新建 `工程控制/PROJECT-GOVERNANCE-01/ROOT_LEDGER.md` 与 `reports/PROJECT-GOVERNANCE-01/root/**`
- 在 `docs/archive/` 下新增归档目录（仅承载确证有保留价值的旧资料）
- 删除 `设计大纲/`（负责人已确认无用）

### 禁止改
- 已 tracked 且属产品/权威/合同/测试的文件（lib/**、docs/**、tests/**、contracts/**、ci/**、tools/**、CMakeLists.txt、AGENTS.md、ASTROCS_DESIGN.md 等）
- `run/` 内部内容（归 ROOT-003）
- 用户数据区 `BASS DR3/`、`GaiaDR3/`、`GaiaDR3SP/`、`testdata/`（只登记，不删不移）
- 任何未经确证的删除：无法证明来源与用途的条目一律只登记

## 步骤
1. 生成全条目账本 `ROOT_LEDGER.md`：对全部顶层条目逐一记录——路径、tracked/untracked、字节数、最后修改时间、内容指纹（小文件 sha256；大目录记文件数与总量）、git 最近一次相关提交、判定类别、处置建议、依据。
2. 判定分类（逐条给证据）：A 明确一次性运行产物 → 删除；B 明确构建缓存 → 删除；C 有价值的历史资料 → 归档到 docs/archive/ 或 reports/（须说明谁会用）；D 用户数据 → 原地保留并记录；E 目标结构内的合法条目 → 保留；F 无法确证 → 待负责人裁决，不得删除（`问题扫描/` 不在此列，它由 ROOT-004 处理）。
3. 执行 A/B 两类清运；C 类归档；D/E 类保留登记。凡删除，先把清单与该条目的内容指纹写入账本（保证可追溯「删了什么」）。
4. 对已 tracked 但不属 §7 白名单的条目（VERSION、CHANGELOG.md、FATDUCK_ACCESS.md、VISUAL_CHECK_README.md、设计大纲/、问题扫描/、run/ 等）**只登记处置建议**，实际删除/归档交给 GOV-001 与 DOC-001 在其文件域内执行，避免文件域冲突。
5. 清运后重跑 `git status --porcelain=v1`，确认未跟踪条目数不增加、tracked 修改不增加、且 `git ls-tree -r HEAD` 不变。
6. 把「清洁后仍存在但不在 §7 白名单」的条目列成待裁决表交给负责人，每条附一句话影响面。

## 验收门（前台独立复跑）
- [ ] `ROOT_LEDGER.md` 覆盖全部顶层条目：条目数 == 执行前 `ls -A` 计数（给出前后计数与差集）
- [ ] 每条都有判定类别（A~F）与依据；F 类集中在待裁决表，**F 类中无一条被删除**
- [ ] 删除动作可追溯：账本记录了每个被删条目的路径 + 指纹 + 判定依据
- [ ] `git ls-tree -r HEAD | wc -l` 与执行前一致（本任务未改任何 tracked 内容）
- [ ] 清洁后的顶层条目全部出现在白名单、已登记保留清单或待裁决表中（给出计数断言）

## 证据命令
```bash
mkdir -p run/PROJECT-GOVERNANCE-01/ROOT-001/logs reports/PROJECT-GOVERNANCE-01/root
ls -A . | sort > run/PROJECT-GOVERNANCE-01/ROOT-001/logs/root-before.txt
git ls-tree -r --name-only HEAD | wc -l | tee run/PROJECT-GOVERNANCE-01/ROOT-001/logs/tracked-before.txt
# 逐条判定写入 ROOT_LEDGER.md；执行 A/B/C；再取 root-after.txt 并 diff
```

## 执行规则（每个任务都适用）

- 开工前按 `AGENTS.md §1` 读完本任务「权威依据」列出的全部条款；未读不开工。
- 只改本文件「改动范围」声明的文件域；**不顺手改无关代码**；不改 `docs/science/**` 公式与 `docs/algorithms/**` 推导。
- 科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、默认容差**一律只读**（ENGINEERING_SPEC §3）。
- 工作区在本控制包编制前已有大量预存修改与未跟踪文件（见 BASE-001）：**不得** reset/stash/clean/checkout，**不得**把它们混进本任务的改动。
- SubAgent 零 git 写权限：不 commit、不 push、不建分支；改动留在工作区并交自证材料，由前台按任务原子提交。
- 所有外部命令带 `timeout`，日志落 `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/`（`run/` 已 gitignore，不入库）。
- 报告「完成」必须附：命令、退出码、关键输出片段、产物路径。禁止用「环境问题/工具问题」掩盖失败。
- 发现文档冲突、科学歧义或无法证明的状态：登记 `BLOCKED`（控制包内）或 `UNRESOLVED`（GAP_AUDIT 内），不得自行选口径。

## 交付物

1. 限「改动范围」内的改动；
2. `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/` 下的命令日志与机器输出；
3. 自证摘要（字段：任务ID / 改动清单 / 逐条验收命令与退出码 / 未决项）。
