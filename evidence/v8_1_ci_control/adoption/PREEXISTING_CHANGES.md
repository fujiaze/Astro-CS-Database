# V81-ADOPT-002 预存修改冻结与归类（PREEXISTING CHANGES）

- task_id: `V81-ADOPT-002` · owner: `SA-ADOPT-31` · mode: `read`
- 生成时间 (UTC): 2026-09-05T10:00:09Z
- 仓库根: `/workspace/Astro CS Database`（由 `git rev-parse --show-toplevel` 发现）
- base SHA: `a4fdee3f446de08cda50889a223740237b2da4a0`（HEAD = main = origin/main，fetch 后 0/0，SYNCED）
- 原始证据: `evidence/v8_1_ci_control/adoption/raw/`（`status_porcelain_v2.txt`、`diff_stat.txt`、`diff_stat_head.txt`、`identity_before_fetch.txt`、`identity_after_fetch.txt`、`submodule_status.txt`、`worktree_list.txt`、`remote_v_raw.txt`、`remote_v_sanitized.txt`、`fetch_origin_prune.txt`、`index_gitlink_check.txt`、`untracked_categories_peek.txt`）
- 处置原则：本任务为只读冻结，**未** reset/stash/clean/checkout/rebase，未移动或删除任何文件，未修改任何 tracked 文件内容。

## 1. 汇总

| 类别 | 数量 | 说明 |
| --- | --- | --- |
| tracked 修改（未暂存，`1 .D`） | 1 | 见 §2 |
| staged / conflicted | 0 | porcelain=v2 无 `2`/`u` 行 |
| untracked（`?` 行） | 90 | 见 §3，按 6 类归档；本快照取 fetch 后、写入本文件之前 |

## 2. tracked 修改归类

### T-1 `dist/audit/AstroCS_V6_1_AUDIT_20260902T042239Z_faad602da555.zip`（状态 `1 .D N...`，工作区删除，未暂存）

- diff：`Bin 4243274 -> 0 bytes`（`git diff --stat HEAD` 唯一变更）。
- 归属：**来源未知**。该文件是被跟踪的 V6.1 历史审核包 zip；删除发生时间与操作者均无法从工作区确定（无 reflog/状态线索指向具体任务），既非本任务（V81-ADOPT-002 只读、未删除任何文件）也非 ADOPT-001 的提交内容（其提交未触碰 dist/audit）。
- 处置：**保留现状，冻结不还原**。按任务规格不自动 reset/checkout 还原；后续由 owner/控制面决定还原（`git restore`）、恢复提交或正式移除该跟踪文件。该删除不应被本任务或后续只读任务隐式提交。

## 3. untracked 归类（共 90 项，逐类记录来源与处置，不删除、不修改）

### U-1 仓库根部历史控制包/审核包 zip 与需求文档（11 项）→ 归属：用户修改/历史遗留，处置：保留原样

10 个控制包/审核 zip：`AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905.zip`、`AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3.zip`、`AstroCS_MAIN_AUDIT_SUPPLEMENT_V2_20260826.zip`、`AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828.zip`、`AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827.zip`、`AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_ALPHA_20260828.zip`、`AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z.zip`、`AstroCS_V5_AUDIT_REVIEW_20260830.zip`、`AstroCS_V6_1_REWORK_CONTROL_20260831.zip`、`AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830.zip`；加 1 个用户/owner 治理文档 `AstroCS_MAIN_FULL_AUDIT_EVIDENCE_REQUEST_V1.md`（审核证据请求）。

来源：历年控制包/审核流程在仓库根部的交付副本（用户放置）；均不涉及源码。处置：保留原样，不纳入提交范围。

### U-2 `artifacts/prerelease_v5/`（27 项）→ 归属：生成证据，处置：保留原样

`AUDIT_PACKAGE_587fe0e341a7.zip`、`audit_src/`、`lnx_pkg/`、`capsules/`（BENCH-001~005、CLI-004~008、ISA-002~005、ISO-001、MON-001~004、P3-001~004、PAR-001 共 24 个 capsule zip）。来源：V5 pre-release 控制包各 SubAgent 任务的历史返回包/审核工件（生成产物）。处置：保留原样，不清理。

### U-3 仓库根部 `astrocs_run_*.json` 运行输出（44 项）→ 归属：生成证据，处置：保留原样

`astrocs_run_b80f3d8f6a2d.json` … `astrocs_run_f907c67b64dd.json`（b80f…/b83c…/b876…/b8a9…/b8da…/bbfd…/bbfe…/bc4c…/be64…/c1cb…/c205…/c260…/c27a…/c36f…/c4b3…/c865…/d24c…/d24d…/d2a2…/d2a3…/d3af…/f907… 共 44 个文件，成对成组）。来源：astrocs CLI 历史运行输出（生成证据）。处置：保留原样。

### U-4 治理/控制解压目录与文档（4 项）→ 归属：用户修改/历史遗留，处置：保留原样

- `工程控制/`（解压文档目录，含 `AstroCS_V6_1_REWORK_CONTROL_20260831`、`AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3`）——按 AGENTS.md 目录规范属于控制包解压文档，用户/流程放置；
- `docs/architecture/cpu/CPU_003_AVX2_PROVIDER.md`——CPU-003 冻结架构文档（FROZEN 2026-09-03），与 HEAD 上一致的文档内容存在于工作区但未被跟踪，属既有工程内容，保留原样；
- `包规范.md`——控制包/返回包/审核包通用格式规范 v1.0，用户治理文档，保留原样；
- `graph/`（l0_graph.dot/json、static_graph.dot/json/svg）——架构图生成/静态图工件，保留原样。

### U-5 `engineering/control/active/`（V8.1 控制包，已 tracked）→ 归属：当前任务链成果，处置：已在 HEAD

`engineering/control/active/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905/` 已由 V81-ADOPT-001 提交（HEAD `a4fdee3f...`，"chore(control): V81-ADOPT-001 注册控制包 V8.1 并冻结工作区身份"），不在 untracked 之列；未跟踪的根部同名 `.zip` 归入 U-1。

### U-6 `evidence/v8_1_ci_control/` 动态运行文件（4 项）→ 归属：当前任务链成果（生成证据），处置：保留原样

- `evidence/v8_1_ci_control/TASK_STATE.json`——V8.1 动态冷台账（运行时状态，由控制面维护）；
- `evidence/v8_1_ci_control/WRITE_LEASE.json`——写租约文件；
- `evidence/v8_1_ci_control/dispatch/`——控制面派发包目录（含本任务 `V81-ADOPT-002.json`）；
- `evidence/v8_1_ci_control/adoption/raw/`——本任务（V81-ADOPT-002）生成的原始命令日志目录。

来源：V8.1 CI control 运行时产生（前三项为控制面/ADOPT-001 之后预存，raw/ 为本任务生成）。处置：保留原样（TASK_STATE.json 仅控制面可写，本任务未触碰）。本任务随后新增的 `adoption/PREEXISTING_CHANGES.md`、`adoption/REMOTE_RELATION.json`、`adoption/tasks/V81-ADOPT-002/`、`ci/verify_workspace_adoption.py` 为本任务产品，**不属于**预存修改。

## 4. 结构性状态（非文件修改，一并冻结记录）

- **submodule**：`git submodule status` exit 128（fatal）：index 中存在 gitlink `AstroCS.wiki`（mode 160000，sha `901725847ded7d1185a98b995df8fce9a7e20a1e`），但仓库无 `.gitmodules` 映射。属既有仓库结构缺陷，非本任务造成；未做任何修改，交由控制面后续裁决（补 `.gitmodules` 或移除 gitlink）。
- **worktree**：`git worktree list --porcelain` 列出 41 条记录：仅 1 条有效（本仓库主 worktree，main @ a4fdee3f），其余 40 条全部 `detached` 且 `prunable gitdir 文件指向一个不存在的位置`（`/home/lighthouse/astrocs_v7_work/worktrees/SA-*/` 下 39 条 + `/tmp/base_e78_build` 1 条）。这些是 V7.1 时期 detached worktree 的残留注册记录，目录已不存在；按硬约束未执行 `git worktree prune`，仅记录。
- **远端**：origin = `https://github.com/fujiaze/Astro-CS-Database.git`（脱敏后无凭据），fetch --prune exit 0，无 pruned refs。

## 5. 结论

- 唯一 tracked 修改：历史审核包 `dist/audit/AstroCS_V6_1_AUDIT_20260902T042239Z_faad602da555.zip` 的未暂存删除，**归属来源未知**，冻结保留、待控制面裁决。
- untracked 90 项全部为历史包/生成证据/治理文档/当前任务链运行文件，无源码改动，全部保留原样。
- 未发现与远端差异（SYNCED 0/0）；工作区非干净但不含冲突条目，不影响后续不相交只读任务继续。
