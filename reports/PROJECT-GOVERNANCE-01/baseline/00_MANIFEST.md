# BASE-001 基线冻结清单（PROJECT-GOVERNANCE-01）

生成时间：2026-09-16（本地时区 +0800）
任务：BASE-001「冻结基线与预存工作区边界」　状态：IN_PROGRESS（执行 Agent 自证，PASS 只由前台复跑后写入）

## 1. 基线三 SHA 实测（**不一致，已登记**）

| 引用 | SHA |
|---|---|
| `git rev-parse HEAD` | `ecf6ad6fe55102c08a156564cc0dd30cea3a7cdc` |
| `git rev-parse main` | `ecf6ad6fe55102c08a156564cc0dd30cea3a7cdc` |
| `git rev-parse origin/main` | `f96dff61e6ae9cc052a925713f9864f5d6bac3bb` |
| 本地 main 领先 origin/main | 1 个提交：`ecf6ad6f docs(governance): 新增根目录清洁任务 ROOT-001/002/003（29 任务）` |
| 本文件/派发提示词声明 | 提示词写 `f96dff61`；`GAP_AUDIT.md` 写 `a861d8f6…`；**均 ≠ 当前 HEAD** |

- 结论：**HEAD = main ≠ origin/main**，且控制包文档声明的基线是第三个值。登记为 `UNRESOLVED U-08`（见 `GAP_AUDIT.md §5`），缺「负责人确认本次治理以哪个 SHA 为基线」。
- 本任务未执行任何 git 写操作（无 commit/push/branch/stash/reset/clean/checkout）。

## 2. 工作区快照

| 指标 | 值 | 证据文件 |
|---|---|---|
| `git status --porcelain=v1` 条目 | 142（tracked 修改 12 + untracked 130） | `02_git_status_porcelain.txt` |
| `git stash list` 条目 | **1** | `04_git_stash_list.txt` |
| 最近提交 | `ecf6ad6f 2026-09-16 13:56:26 +0800` | `05_git_log_last.txt` |
| tracked 文件总数 | 5366 | `06_git_ls_tree_count.txt` |
| 顶层条目 | 133 | `07_root_entries_before.txt` |
| /workspace | 503G 总 / 293G 已用 / 186G 可用（62%） | `11_df_workspace.txt` |
| `du -sh run build` | run 102G、build 6.0G | `run/PROJECT-GOVERNANCE-01/BASE-001/logs/` |

> 说明：BASE-001 自身产物 `reports/PROJECT-GOVERNANCE-01/baseline/**` 会计入 untracked；分类清单 `14_preexisting_classification.txt` 把「本控制包自身新增」定义为 `工程控制/PROJECT-GOVERNANCE-01/**` + `reports/PROJECT-GOVERNANCE-01/**`，其余全部为控制包编制前既存，**不得混入本包任何任务提交**。

## 3. 环境

见 `13_env_versions.txt`（cmake / ninja / g++ / python3 / uname / nproc / date）。

## 4. 基线红绿灯（BASE-001 独立复跑）

| 命令 | rc | 说明 | 证据 |
|---|---|---|---|
| `python3 tools/check_agents_gov.py` | **1** | 旧 AGENTS.md 字符串期望失配（编制时同为 rc=1，一致） | `17_gap_sec0_redlights.txt` |
| `python3 tools/doccheck/check_engineering_constraints.py` | **1** | 仍以旧工程约束为权威对象 | 同上 |
| `python3 tools/doccheck/check_doc_index.py --strict` | **1** | `docs/DOCUMENT_INDEX.yaml` 仍索引旧文档体系 | 同上 |
| `python3 ci/check_version.py --expected 0.11.0-alpha.2` | 0 | **不是干净**：旧版本号自洽掩盖（GAP-017） | `18_gap_sec2_gates.txt` |
| `python3 tools/check_cli_command_layer.py` | 0 | **不是干净**：以旧 phase 命令层为合格判据（GAP-005） | 同上 |
| `python3 tools/check_l0_docs.py` | 0 | 绑定旧活动文档集合 | 同上 |
| `python3 tools/check_module_readmes.py` | 0 | 只查少量模块 README 链接（GAP-008） | 同上 |
| `ls ci/run_checks.py` | 2 | 文件不存在 → GAP-016「入口名不符」成立 | 同上 |
| `python3 ci/run_checks.py --help` | 2 | 同上 | 同上 |

## 5. 文件索引

| 文件 | 内容 |
|---|---|
| `01_git_rev_parse.txt` | 三 SHA 原始输出 |
| `02_git_status_porcelain.txt` | 完整 `git status --porcelain=v1` 快照（**验收门要求**） |
| `03_git_status_ignored.txt` | 含 ignored 的完整快照 |
| `04_git_stash_list.txt` | stash 列表（1 条） |
| `05_git_log_last.txt` | 最近提交 |
| `06_git_ls_tree_count.txt` | tracked 文件数（5366） |
| `07_root_entries_before.txt` / `08_root_entries_stat.txt` | 顶层 133 条目清单 / 带类型与字节 |
| `09_git_tracked_root.txt` / `10_version_file.txt` | 顶层 tracked 名单 / VERSION 内容 |
| `11_df_workspace.txt` / `12_gitignore.txt` | 磁盘 / .gitignore 原文 |
| `13_env_versions.txt` | 工具链版本 |
| `14_preexisting_classification.txt` | 预存改动分类清单（预存修改 / 预存未跟踪 / 本控制包自身） |
| `15_git_diff_stat.txt` / `16_git_diff_cached_stat.txt` | tracked 修改 diff --stat 快照 |
| `17_gap_sec0_redlights.txt` | GAP_AUDIT §0 三项红灯实测 |
| `18_gap_sec2_gates.txt` | GAP_AUDIT §2 基线门复核实测 |
| `19_gap_evidence.txt` | GAP-001..GAP-022 逐条证据（第一轮，全量） |
| `20_gap_evidence_scan2.txt` | 定向复核（数字冲突项） |
| `21_gap_evidence_scan3.txt` | 定向复核（CLI 命中原文 / 顶层口径 / checks.json 格式） |
| `22_gap_evidence_scan4.txt` | 定向复核（module.yaml / registry / p2_session / gitignore 覆盖） |
| `23_gap_evidence_appendix.md` | GAP_AUDIT §5 证据补全正文（已同步进 GAP_AUDIT.md） |
| `SHA256SUMS` | 本目录全部文件 sha256 |

## 6. 复跑方式

```bash
cd "/workspace/Astro CS Database"
bash run/PROJECT-GOVERNANCE-01/BASE-001/evidence-scan.sh  > /tmp/s1.txt 2>&1
bash run/PROJECT-GOVERNANCE-01/BASE-001/evidence-scan2.sh > /tmp/s2.txt 2>&1
bash run/PROJECT-GOVERNANCE-01/BASE-001/evidence-scan3.sh > /tmp/s3.txt 2>&1
bash run/PROJECT-GOVERNANCE-01/BASE-001/evidence-scan4.sh > /tmp/s4.txt 2>&1
git rev-parse HEAD main origin/main; git stash list; git status --porcelain=v1 | wc -l
```
## 7. 修订说明（2026-09-16 收口；**不覆盖 §1 原始快照**）

| 项 | BASE-001 冻结时（13:58） | 收口时 | 说明 |
|---|---|---|---|
| `HEAD` / `main` | `ecf6ad6fe55102c08a156564cc0dd30cea3a7cdc` | 见 `24_revision_snapshot.txt`（收口时 `939d3f6c…`，其后仍随前台提交前移） | 前台调度线在 BASE-001 执行期内连续提交（`5f891080`、`2c328348`、`c5392daa`、`939d3f6c`），含对 `tasks/ROOT-001.md`/`ROOT-003.md` 的改写与 ROOT-004/005 新增 → 直接造成 U-08 与 GAP-030 |
| `origin/main` | `f96dff61e6ae9cc052a925713f9864f5d6bac3bb` | 收口时 `2c328348…`（后台仍在 push） | U-08 的「三 SHA 不一致」**已由前台 push 消除**（见 `GAP_AUDIT.md §5.4` U-08 行）；但 HEAD 与 origin/main 在执行期内持续前移，属 GAP-030 |
| tracked 文件数 | 5,366 | 5,369 | 增量来自前台提交（ROOT-004/005 等），**非本任务所致**；ROOT-001 的「tracked 行数不变」门改用同一次脚本执行内的 before/after 相对口径（5,367/5,367） |
| 顶层条目 | 133 | 60（15:14 起为 61：并发线重建了 gitignored 的 `build/`，机器门按 `ignored_patterns` 容忍并上报） | ROOT-001/ROOT-003 清运结果（见 `ROOT_LEDGER.md` §10） |

- §1 的 `06_git_ls_tree_count.txt`（5,366）与 `01_git_rev_parse.txt`（ecf6ad6f）**保持原样不覆盖**，作为「开工基线」的证据；本修订说明是其时间线补充。
- 新增登记：`GAP-023`（`.gitignore` 覆盖缺口）、`GAP-028`（注册表/`tests/quality` 预存红灯）、`GAP-029`（`run/` 体积与被引用证据重合）、`GAP-030`（执行期提交导致基线失效），以及 `U-08` 的消除记录。
- 本任务全程零 git 写操作：未 commit / push / branch / stash / reset / clean / checkout；`git status` 中 344 个 `D`（`设计大纲/`）由前台按 ROOT-001 任务卡裁决提交。
