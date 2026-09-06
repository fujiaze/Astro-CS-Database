# V8-CI-011 GitHub 仓库设置矩阵（SA-CI-32）

- 仓库：`fujiaze/Astro-CS-Database`（公开仓库，default branch `main`）
- base SHA：`2c1ffef10b0feb36e779b68e08e7e1b147371c6d`
- 执行时间：2026-09-06（API 现场读回确认；trace：`logs/api_trace.jsonl`，脱敏无 token）

| # | 设置项 | before（GET 现值） | action | after（GET 复读） | status |
|---|--------|--------------------|--------|--------------------|--------|
| 1 | Actions enabled | `enabled=true, allowed_actions=all` | 只读确认（未改动 enabled；workflows 在跑即证） | `enabled=true, allowed_actions=selected` | READ_ONLY_CONFIRMED |
| 2 | 默认 workflow token 只读 | `default_workflow_permissions=read, can_approve_pull_request_reviews=false` | PUT `/actions/permissions/workflow` 同值幂等（HTTP 204） | `read / false` | CONFIGURED |
| 3 | allowed actions 与 `ci/actions.lock.json` 一致 | `allowed_actions=all`（selected-actions GET=409） | PUT `actions/permissions {enabled=true, allowed_actions=selected}` → PUT selected-actions（**首次按派发语义 `enabled_actions` 返回 204 但复读 `patterns_allowed=[]`，语义不符，立即修正**）→ PUT `{github_owned_allowed:true, verified_allowed:false, patterns_allowed:[actions/checkout@*, actions/upload-artifact@*, actions/download-artifact@*]}` | `github_owned_allowed=true, verified_allowed=false, patterns_allowed=[actions/checkout@*, actions/upload-artifact@*, actions/download-artifact@*]` | CONFIGURED（含一次语义偏差修正，见 §双层机制） |
| 4 | artifact retention | `days=90` | PUT `/actions/permissions/artifact-and-log-retention {days:14}`（HTTP 204） | `days=14`（maximum_allowed_days=90） | CONFIGURED |
| 5 | Fatduck runner | `total_count=0, runners=[]` | 只读确认 + 期望终态登记（API 无法代建 runner） | 无 runner、无 `fatduck-realdata` label；期望终态：V8-FAT-001 装机后 repository-level、labels 仅 `[self-hosted, fatduck-realdata]`、无 push/个人目录权限 | READ_ONLY_CONFIRMED（FATDUCK_PENDING） |
| 6 | 公开 fork/PR workflow 不匹配 Fatduck | 见 §6 防线 | 只读确认源码与设置语义 | 两道防线在场（§6） | READ_ONLY_CONFIRMED |

## Issues（单一 Owner Review）

只读读回：`has_issues=true, open_issues_count=0, has_discussions=false`；凭据 `permissions.admin=true`。Issues 可用于单一 Owner Review；本任务未新建/修改/关闭任何 issue。

## 双层 action 保障机制（第 3 项说明）

1. **第一层（repo 级，本次 API 配置）**：`allowed_actions=selected` + 白名单 `patterns_allowed` 仅含 lock 表使用中的 3 个官方 action 名（`actions/checkout`、`actions/upload-artifact`、`actions/download-artifact`，通配 `@*`）。GitHub API 语义仅支持 action 名@版本模式，**不支持 commit SHA 级白名单**（`sha_pinning_required` 为全局强制开关且未启用）。
2. **第二层（源码级，既有保障）**：全部 workflow 对这 3 个 action 固定 commit SHA（`ci/actions.lock.json`，4 entries 含 1 条 v4 模板参考项），由 `ci/verify_actions_lock.py` 机器复验（本次 `--offline` PASS）。
   两层叠加：repo 白名单限制"哪个 action 可用"，lock 表+复验器强制"必须用哪个 SHA"——第三方/未登记 action 即使 workflow 误引用也会被 repo 级白名单拒绝运行。

**偏差登记**：派发指令给定 PUT 体 `{"enabled_actions":[...]}` 与 API 实际语义不符——该端点字段为 `patterns_allowed`；首次 PUT 返回 204 但复读白名单为空（空名单会禁用全部可复用 action，在跑 workflow 将失败），已立即以正确语义修正并复读确认，全程落 `logs/api_trace.jsonl`。最终读回与 lock 表 action 名单严格一致。

## §6 公开 fork/PR 不匹配 Fatduck —— 两道防线

- **防线一（触发与候选双重校验）**：`.github/workflows/fatduck.yml` 的 `workflow_run` 触发限定 `workflows:["AstroCS Windows CI"] + branches:[main]`；`ci/select_candidate.py` 在 run 内显式复校 `head_branch==main` 且 `conclusion==success`（exit 3 = skip）。fork PR 的 head_branch 必然≠main，两层校验均拦截。
- **防线二（runner label 隔离）**：`fatduck-validate` runs-on 独有 label `fatduck-realdata`；仓库 runner `total_count=0`，fork 触发的任何 run 拿不到该 label 的成功 Windows run → 无 candidate artifact → select exit 3 skip。
- 仓库为公开（`private=false`），fork PR 数据不可能经此链路进入 Fatduck。

## 遗留风险

1. `patterns_allowed` 为 `@*` 通配：若未来某 action 在官方账号下发布含恶意代码的新 tag 且被 workflow 引用，repo 级白名单不拦截版本升级——依赖第二层 SHA 锁+verify 复验（已覆盖）。
2. `has_wiki=true, has_projects=true`：超出本任务范围未改动（禁止项）；如需收敛另派设置任务。
3. retention `maximum_allowed_days=90` 为上游约束；若后续策略要求 >90 天需走 enterprise/组织层设置。
4. Fatduck runner 未装机（FATDUCK_PENDING）：V8-FAT-001 建机时须按期望终态校验 labels 与权限，防提前接入。
