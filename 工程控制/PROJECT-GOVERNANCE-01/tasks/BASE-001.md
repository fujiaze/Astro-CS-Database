# 任务：BASE-001 冻结基线与预存工作区边界

状态：`NOT_STARTED`
层：L0　依赖：无　文件域互斥组：S0

## 目标

在动任何文件之前，把「从哪个提交开始治理」「工作区里哪些改动先于本控制包存在」冻结成可复核记录，使后续所有任务的 diff 都能被判定「只包含本任务的东西」。

## 基线状态（编制时实测）

- `git rev-parse HEAD main origin/main` → 三者均为 `a861d8f63a1f6c17dea2f201349006f6a6bd1ad2`。
- `git status --porcelain=v1` 存在**大量预存修改与未跟踪文件**（`artifacts/**`、`reports/v19r2/**`、`问题扫描/**`、`p8..p15a-files.patch`、`run_context.json`、`lib/snr_estimator/**`、`tests/unit/p1_noise/**` 等），与本次治理无关。
- 本控制包目录 `工程控制/PROJECT-GOVERNANCE-01/` 自身在编制时也是未跟踪状态。

## 权威依据
- ENGINEERING_SPEC.md §6（Git 与提交：预存修改先登记，不自动 reset/stash/clean/rebase/覆盖）
- CONTROL_PACK_SPEC.md §3.3（制作过程不改代码）、§6.2（SubAgent 零 git 写权限）

## 改动范围（文件域）
### 允许改
- `reports/PROJECT-GOVERNANCE-01/baseline/**`（新建）
- `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md`（**只补证据与 UNRESOLVED，不删条目**）

### 禁止改
- 任何产品源码、测试、合同、配置、文档
- 工作区中已存在的预存修改与未跟踪文件（不得清理、不得提交、不得覆盖）
- `git reset / stash / clean / checkout -- . / rebase`

## 步骤
1. 记录环境快照：`git rev-parse HEAD main origin/main`、`git status --porcelain=v1`、`git stash list`、`git log -1 --format=%H%n%ci`、`cmake --version`、`ninja --version`、`g++ --version`、`python3 --version`；原始输出存 `reports/PROJECT-GOVERNANCE-01/baseline/`。
2. 生成预存工作区清单：对 `git status --porcelain=v1` 的每一条标注类别（预存修改 / 预存未跟踪 / 本控制包自身新增）与是否属于本次治理范围；清单做 `sha256sum`。
3. 逐条复核 GAP_AUDIT 的 §0 与 GAP-001..GAP-020：补「当前证据」的精确文件、行号或命令+输出片段；无法判定的改写为 `UNRESOLVED` 并写明缺什么。
4. 实测并记录 GAP_AUDIT §0 三项红灯命令的退出码：`tools/check_agents_gov.py`、`tools/doccheck/check_engineering_constraints.py`、`tools/doccheck/check_doc_index.py --strict`。
5. **不执行任何修复。**

## 验收门（前台独立复跑）
- [ ] `git rev-parse HEAD` == `git rev-parse main` == `git rev-parse origin/main`
- [ ] `reports/PROJECT-GOVERNANCE-01/baseline/` 内含完整 `git status --porcelain=v1` 快照及其 sha256
- [ ] GAP_AUDIT 每条 GAP 都有：条目号、类型、权威条款号（章节真实存在）、至少一条仓内可复跑证据或明确 `UNRESOLVED`
- [ ] 相对步骤 1 快照，`git status --porcelain=v1` **只增加** `reports/PROJECT-GOVERNANCE-01/baseline/**` 与 GAP_AUDIT 的改动，无删除、无覆盖

## 证据命令
```bash
mkdir -p run/PROJECT-GOVERNANCE-01/BASE-001/logs
git rev-parse HEAD main origin/main | tee run/PROJECT-GOVERNANCE-01/BASE-001/logs/sha.txt
git status --porcelain=v1 | tee run/PROJECT-GOVERNANCE-01/BASE-001/logs/status.txt
python3 tools/check_agents_gov.py; echo "rc=$?"
python3 tools/doccheck/check_engineering_constraints.py; echo "rc=$?"
python3 tools/doccheck/check_doc_index.py --strict; echo "rc=$?"
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

1. 限「改动范围」内的代码/合同/配置改动；
2. `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/` 下的命令日志与机器输出；
3. 自证摘要（字段：任务ID / 改动文件清单 / 逐条验收命令与退出码 / 未决项）。
