# SECRET_HYGIENE_REPORT —— ROOT-006 凭据入仓风险处置（结构级审计）

- 任务卡：`工程控制/PROJECT-GOVERNANCE-01/tasks/ROOT-006.md`
- 统计时点：2026-09-16 16:16 (+0800)，`HEAD=d414c3e0`，tracked=4268（并发线在持续提交，引用计数随时点浮动）
- **范围变更（负责人 2026-09-16 裁决）**：不轮换任何凭据；原 `ROTATION_DECISION.md` 改为 `EXPOSURE_NOTE.md`（残留暴露面 + 为何不轮换 + 未来流程）
- **输出纪律**：本报告只含 路径 / 布尔 / 行号 / 计数；**不含任何凭据值、片段或哈希**，也不复写基础设施标识字面量（节点地址、用户名、私钥名一律以形态描述）
- 证据目录：`run/PROJECT-GOVERNANCE-01/ROOT-006/logs/`（`FINAL_EVIDENCE.txt` 为最终证据包）

---

## 1. 自证：全程未读取 FATDUCK_ACCESS.md 内容

| # | 命令形态 | 用途 | 是否触达正文 |
|---|---|---|---|
| 1 | `ls -la` / `stat -c "%n %s %a %y"` | 存在性/大小/权限/mtime | 否（元数据） |
| 2 | `git ls-files --error-unmatch` / `git ls-files -s` | tracked 状态、blob 元数据 | 否 |
| 3 | `git log --follow --oneline -- <path>` / `git log -S "FATDUCK_ACCESS"` | 提交历史 | 否 |
| 4 | `git check-ignore -v [--no-index]` | 忽略规则命中 | 否 |
| 5 | `git merge-base --is-ancestor <sha> origin/main` | 是否已推送 | 否 |
| 6 | `git grep -l -F "FATDUCK_ACCESS"`（**仅 -l / -c**，从不打印匹配行） | 引用文件清单 | 否（只取文件名/计数） |
| 7 | `python3 tools/quality/check_secret_hygiene.py --files FATDUCK_ACCESS.md --report-only` | 形态扫描，**只输出布尔+行号** | 进程内读字节做形态匹配，**输出无值** |
| 8 | `zipfile.namelist()` 对旧审计包 | 只列条目名 | 否 |

**未执行过**：`cat`、`head`、`tail`、`less`、`more`、`strings`、`base64`、`xxd`；任何复制（`cp` / `git show > file`）；任何打印正文的 `grep`。
其他 14 项白名单文件的扫描走 HEAD 快照（`git show HEAD:<path> > run/.../whitelist_snapshot/`），**唯独 FATDUCK_ACCESS.md 不复制**，只原地扫。

---

## 2. 白名单逐文件敏感形态扫描（只输出布尔 + 行号）

扫描器：`tools/quality/check_secret_hygiene.py`（15 类形态：私钥头 3 种、`sk-`、`ghp_`/`github_pat_`、`AKIA/ASIA`、`AIza`、`xox`、JWT、password／secret／token 三类**赋值形态**、hex32/40/64、b64≥40；另有 4 类 INFO 级基础设施形态，见 §4）。
白名单 = `tools/pack_audit_package.py` 的 `ROOT_FILES`，共 **15 条**：14 条实扫（13 条按 HEAD 快照 + FATDUCK_ACCESS.md 原地）+ **1 条悬空**。

| 白名单文件 | PEM私钥头 | OPENSSH私钥 | PGP私钥 | sk- | ghp_/pat | AKIA | AIza | xox | JWT | password= | secret=/token= | hex32 | hex40 | hex64 | b64≥40 | 结论 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| VERSION | false | false | false | false | false | false | false | false | false | false | false | false | false | false | false | 全 false |
| README.md | false | false | false | false | false | false | false | false | false | false | false | false | **TRUE** L4 | false | **TRUE** L93 | 有形态命中（见行号） |
| AGENTS.md | false | false | false | false | false | false | false | false | false | false | false | false | false | false | **TRUE** L84 | 有形态命中（见行号） |
| build.sh | false | false | false | false | false | false | false | false | false | false | false | false | false | false | false | 全 false |
| toolchain.ps1 | false | false | false | false | false | false | false | false | false | false | false | false | false | false | false | 全 false |
| CHANGELOG.md | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | 文件已不存在（悬空条目） |
| memory.md | false | false | false | false | false | false | false | false | false | false | false | false | **TRUE** L36,37,38,112 | false | false | 有形态命中（见行号） |
| FATDUCK_ACCESS.md | false | false | false | false | false | false | false | false | false | false | false | false | false | false | false | 全 false |
| HANDOVER.md | false | false | false | false | false | false | false | false | false | false | false | false | false | false | false | 全 false |
| VISUAL_CHECK_README.md | false | false | false | false | false | false | false | false | false | false | false | false | false | false | false | 全 false |
| .clang-format | false | false | false | false | false | false | false | false | false | false | false | false | false | false | false | 全 false |
| .gitignore | false | false | false | false | false | false | false | false | false | false | false | false | false | false | false | 全 false |
| .gitattributes | false | false | false | false | false | false | false | false | false | false | false | false | false | false | false | 全 false |
| .editorconfig | false | false | false | false | false | false | false | false | false | false | false | false | false | false | false | 全 false |
| CMakeLists.txt | false | false | false | false | false | false | false | false | false | false | false | false | **TRUE** L56 | false | **TRUE** L117 | 有形态命中（见行号） |

要点：

1. **FATDUCK_ACCESS.md：15 类形态全 false**（无 32/40/64 位 hex、无长 base64、无私钥头、无 `sk-`、无 password／secret 赋值形态）——与任务卡基线一致；本报告不判断其正文语义（负责人已另有裁决：正文为节点接入说明，**无明文凭据**）。
2. **4 个文件有 INFO 级命中**（`README.md` L4/L93、`AGENTS.md` L84、`memory.md` L36/37/38/112、`CMakeLists.txt` L56/L117）：结构归类为「行内代码里的 40 位提交哈希」与「CMake 常量/长标识串」，**无赋值上下文**，非敏感类文件 → 按策略记 INFO，不判红（见 §6.3）。
3. **悬空条目 `CHANGELOG.md`**：既不在工作区也不在 HEAD（ROOT-007 提交 `01db973b` 删除），而白名单仍列着它 —— 打包器对不存在文件是 `continue` 静默跳过，**白名单与仓库现实无一致性校验**（§5.2）。

---

## 3. 引用审计：谁提到了 `FATDUCK_ACCESS.md`

- 统计口径：`git grep -l -F`（只取文件名），时点 `HEAD=d414c3e0` → **52 个 tracked 文件**（编制任务卡时为 ~30，随治理线写入而增长）
- **路径引用 52 / 内容引用 0**

| 角色 | 数量 | 说明 |
|---|---|---|
| 报告 `reports/**` | 32 | 治理基线、根扫描分片、v6 审计、REAUDIT_V3 等清单/正文提及 |
| 任务卡 `工程控制/**/tasks/*.md` | 6 | 任务书里的证据命令与文件域 |
| 控制包文档（非任务卡） | 3 | `TASK_LIST/ROOT_LEDGER/GAP_AUDIT` 等台账 |
| 登记表/工具 `ci/**`、`tools/pack_audit_package.py`、`docs/DOCUMENT_INDEX.yaml` | 4 | 影响面映射、根清单、打包白名单、文档索引 |
| 采集缓存 `问题扫描/**` | 3 | 扫描语料/文件清单缓存 |
| `docs/owner/**` | 2 | 所有者文档中的路径引用 |
| 其它根条目 | 1 | `REVIEW.md` 等 |

**「内容引用 = 0」的判据**（三重，均不读正文）：

1. **读取型命令行 0 个**：全仓 grep `(cat|type|Get-Content|less|more|head|tail|base64|xxd|strings).*FATDUCK_ACCESS` → 命中 0 个文件（即：没有任何脚本/日志把该文件内容读出来贴进仓库）；
2. **结构特征交集**：把该文档独有结构特征（Tailscale 字样 / 接入时间窗 / 私钥路径形态）与 52 个提及文件求交 → 仅 `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md` 1 个命中，且为治理正文在**讨论**该文件（不是复制正文）；
3. **负责人授权确认（2026-09-16）**：负责人读过正文，用正文独有字符串在 HEAD 全仓检索，逐字复制形态命中 0（唯一 1 处为不连续形态）。

> 结论：**路径引用 52 / 内容引用 0**。若后续有人把正文贴进文档，CHK-SECRET-HYGIENE 不会报（它不是凭据形态），需靠评审纪律；这是本次登记的**已知覆盖边界**。

---

## 4. 同类暴露面：旧审计脚本与运维日志（负责人指派新增）

用节点基础设施标识（CGNAT/Tailscale 网段地址、`user@host`、私钥文件名形态、ssh/scp 命令）做全仓结构扫描（**只输出路径 + 布尔 + 行号**，不打印任何标识字面量、不打印行内容）：

| 文件 | 节点地址 | 私钥路径形态 | ssh/scp 目标 | authorized_keys |
|---|---|---|---|---|
| FATDUCK_ACCESS.md（本体，本次处置对象） | TRUE L4,16,19,22,25,26 | TRUE L7,16,19,22,25 | TRUE L16,19,22,25 | TRUE |
| `docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md` | TRUE L2255,2258 | TRUE L2258 | false | false |
| `reports/evidence/WIN001_verification.md` | TRUE L12 | TRUE L12 | TRUE L12 | false |
| `reports/evidence/WIN002_verification.md` | TRUE L10 | false | false | false |
| `reports/evidence/WIN003_verification.md` | TRUE L10 | false | false | false |
| `reports/v6/final-audit/FINAL-AUDIT-001_REPORT.md` | TRUE L200 | TRUE L200 | TRUE L200 | false |
| `reports/v6/final-audit/findings.json` | TRUE L62 | false | TRUE L62 | false |
| `reports/v6/release-review/03_PENDING_AND_OWNER_ITEMS.md` | TRUE L57 | false | false | false |
| `reports/v6/release-review/04_RELEASE_REVIEW_SECTION14_5.md` | TRUE L24 | false | false | false |
| `reports/v6/windows/WIN-VERIFY-001.md` | TRUE L32-36,119,120 | TRUE L32,119 | TRUE L32,119 | false |
| `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md` | TRUE L396 | TRUE L395 | false | false |
| `工程控制/PROJECT-GOVERNANCE-01/SECURITY_NOTE.md` | false | TRUE L8 | false | false |

补充事实：

- 负责人 2026-09-16 复核时点曾见 `reports/REAUDIT_V3/scripts/{_chainB,_monB3,_pollB,_wait_probe,_waitchk}.sh` 等脚本内**直接写着** `ssh -i <私钥路径> <用户>@<节点地址>` 形态；本报告时点这些路径**已不在 tracked**（ROOT-007 清运批次已收走，`git ls-files reports/REAUDIT_V3/scripts` = 0）。
- 上述 12 个文件均为**作废世代/历史证据**（v6、REAUDIT_V3、docs/archive/history、治理台账），不含凭据值，属**侦察信息**。
- 处置分工（负责人已定）：清运并入 ROOT-007 删除批次；本任务只登记，不删除。

---

## 5. 白名单根因分析

### 5.1 它为什么会被收进审计包

| 环节 | 事实（命令证据） | 结论 |
|---|---|---|
| ① 入仓 | `2f20a99d`（2026-08-29，提交信息含「FATDUCK_ACCESS 纳管」）首次且唯一一次入库；此前无凭据预提交门 | 根因 1：**没有「凭据类文件不得入库」的门**，靠人自觉 |
| ② 入白名单 | `git log -S "FATDUCK_ACCESS" -- tools/pack_audit_package.py` → 仅 `ac2d230d`（2026-08-30，打包器首次提交） | 根因 2：打包器作者按「当时根目录有什么」**手工抄了一份根文件清单**，把凭据文件一起抄进去，**未做安全审** |
| ③ 无兜底 | `allowed()` 只有「白名单/目录/扩展名」三类判定，**没有任何凭据类排除** | 根因 3：**缺排除保证**，白名单一旦写错就直通外发 zip |
| ④ 已扇出 | `artifacts/prerelease_v5/AUDIT_PACKAGE_587fe0e341a7.zip`（2026-09-04 构建，来源提交 `587fe0e341a7` 已含 ①）条目名含 `FATDUCK_ACCESS.md`（1699 条目；当时实测） | 后果：**外发审核包已包含该文件**；该 zip 现已随 `artifacts/` 清运不在工作区 |

### 5.2 白名单里其它「本不该入包 / 已经腐坏」的条目

| 条目 | 问题 | 判据 |
|---|---|---|
| `CHANGELOG.md` | **悬空**：文件已被 ROOT-007 删除，白名单仍列着；打包器静默跳过 | `git cat-file -e HEAD:CHANGELOG.md` → rc=128；`test -e CHANGELOG.md` → rc=1 |
| `memory.md` | 私用工作记忆（含大量提交哈希与内部过程），入外发审核包**无必要**；本次扫描 4 处 INFO（hex40） | §2 表；`ROOT_FILES` 第 19 行 |
| `HANDOVER.md` | 交接文档；入外发包的**必要性未评估** | 同上 |
| `VISUAL_CHECK_README.md` | 人眼检查说明；入外发包必要性未评估 | 同上 |
| `ROOT_FILES` **缺失项** | `ASTROCS_DESIGN.md`、`ENGINEERING_SPEC.md`、`CONTROL_PACK_SPEC.md`、`DEPENDENCIES.md`、`CMakePresets.json` **不在白名单**（实测 `allowed()` 返回 False），而凭据文件在 —— 说明清单是「随手抄的当时快照」而非「按审核需求评审的清单」 | `run/.../FINAL_EVIDENCE.txt` 与 §5.1 模拟脚本输出 |
| 潜在过宽子句 | `allowed()` 里 `top in {x for x in ROOT_FILES}` 把「顶层目录名」与「根文件名」混比；还有 `if any(s in (...) for s in []): pass` 死代码 | `tools/pack_audit_package.py` L~70；本次未动（最小改动面），登记为待清理项 |

**根因结论**：白名单是**手工维护、无安全评审、无一致性校验**的清单 —— 它既漏收了本该给审核方的设计/规范文档，又收进了凭据文件与私用笔记；且打包器缺少「凭据类永不入包」的硬保证。本次修的是**机制**（排除保证 + 机器门），不是重写清单（重写属 PKG-001/CI-001 域）。

---

## 6. 处置改动

### 6.1 `.gitignore`（外科式追加，精确条目）

```diff
 .gitignore | 6 ++++++
```

新增（`.gitignore:144-149`，行号随时点浮动）：

```text
# 安全（ROOT-006 / CHK-SECRET-HYGIENE）: 凭据/接入类文档不入库——精确条目，不加宽通配
# 注意：忽略规则对**已 tracked** 文件无效；本条目只是「误加回」护栏，不解决存量。
# 存量处置 = 审计包白名单移除 + 排除保证（tools/pack_audit_package.py）；
# 是否 git rm --cached 见 reports/PROJECT-GOVERNANCE-01/security/EXPOSURE_NOTE.md（负责人裁决）。
FATDUCK_ACCESS.md
```

**准确口径（必须照抄，禁止写成「已修复」）**：该文件**当前仍处于 tracked**，忽略规则对已跟踪文件无效，因此本条**不解决存量**，只是防止未来误加回；真正的机制修复是「审计包排除保证 + CHK-SECRET-HYGIENE 门」，存量解除由负责人在 `EXPOSURE_NOTE.md` 裁决后由前台执行 `git rm --cached`。

并发边界：写入前重读、写入后断言 —— ROOT-002 的 6 条（`/p*-files.patch`、`/run_context.json`、`/astrocs_p1sess_*/`、`/CS/`、`/Database/`、`/worktrees/`）逐条核对**全部在位**，我方仅新增 6 行、无删改。

### 6.2 `tools/pack_audit_package.py`（移除 + 排除保证）

```diff
 ROOT_FILES: 去掉 "FATDUCK_ACCESS.md"
 + DENY_PATHS = {"FATDUCK_ACCESS.md"}                    # 精确条目，与白名单解耦
 + DENY_NAME_RE = .env*/id_rsa|dsa|ecdsa|ed25519*/.netrc/*.pem|key|ppk|p12|pfx|jks|keystore/*_ACCESS.md
 + def denied(rel) -> bool                              # 路径归一化（\ → /、去 ./）
 allowed(): 第一句即 if denied(rel): return (False, "")  # 排除保证先于一切白名单判定
 main(): 统计并打印 DENY 排除项（仅路径名）
```

**「即使被显式指定也不入包」的可复现负例**：`python3 run/PROJECT-GOVERNANCE-01/ROOT-006/logs/packer_deny_selftest.py` → rc=0，8/8 PASS，含：

- `allowed('FATDUCK_ACCESS.md') == (False, '')`；
- Windows 分隔符 `.\FATDUCK_ACCESS.md` 与 `./` 前缀变体同样拒绝；
- **把条目重新塞回 `ROOT_FILES` 后仍为 `(False,'')`**（排除保证与白名单解耦）；
- 走**真实打包路径**产出 zip（`OUT` 重定向到临时目录）→ `[pack] DENY 排除(凭据类, 即使被显式指定也不入包): 1 ['FATDUCK_ACCESS.md']`，zip 条目名不含该文件，且 `README.md`/`AGENTS.md` 未被误伤。
- 全量 tracked 上拒绝面 = **1**（只影响该文件），打包集合 2694→2723（随时点浮动）不含它。

### 6.3 新增 `CHK-SECRET-HYGIENE`（检查器 + 测试）

`tools/quality/check_secret_hygiene.py`（新）、`tests/quality/test_secret_hygiene.py`（新，**22 项**单测）。

**扫描范围（三选一）**：`--scope pack`（默认，**直接 import 打包器 `allowed()`，与打包同源**）／`--scope tracked`（`git ls-files` 全集）／`--scope walk --root <dir>`（测试/巡检）；`--files <p...>` 显式清单（白名单审计用）。

**判定分层（避免「要么恒绿要么恒红」）**：

| 层 | 形态 | 普通文件 | 凭据类文件（文件名含 secret/credential/password/access 等，或 `.env/.pem/.key/.ppk/.netrc` 等扩展名） |
|---|---|---|---|
| CRITICAL | 私钥头（PEM/OPENSSH/PGP）、`sk-`、`ghp_/github_pat_`、`AKIA/ASIA`、`AIza`、`xox`、JWT | **FAIL** | **FAIL** |
| ESCALATE | password／secret／token 的**赋值字面量**（占位符与变量引用已排除）、hex32/40/64、b64≥40 | INFO（提交哈希/校验和合法且普遍） | **FAIL** |
| INFO | CGNAT/Tailscale 网段地址、`ssh/scp` 目标、私钥路径、`authorized_keys` | INFO | INFO（**永不判红**，清运属治理批次） |

`**fail-closed（无法判定即 FAIL）**：`git` 不可用、扫描集为空、**零文件实扫**（`nothing_scanned`）、文件不可读、文本超限未扫（`--max-bytes`，默认 4MiB）、打包器不可导入、输出自泄、walk 根不存在。**显式边界**：二进制（扩展名或前 8KiB 含 NUL）不做文本扫描，但**计数上报**（非静默跳过）。

**输出纪律**：只落 路径/布尔/行号/计数；序列化后再自扫一遍（路径字段用前缀形态单独扫），命中即 `output_self_leak` 并降级为最小 JSON —— 本轮开发中真的触发过一次（`INFRA_AUTHORIZED_KEYS` 这个**形态 ID 自身**被自己的正则命中），已用词边界修正。

> **自我命中插曲（真实发生，记录以免重踩）**：本报告文件名含 SECRET，按策略被归为**凭据类文件**；于是文中
> 为「说明形态」而写的字面量（password／secret 的赋值写法、以及一串用斜杠连接的读取工具名 ≥40 字符）
> 被判为升级命中 —— 报告一旦落盘，``--scope pack`` 就会因它变红。改写为不含可命中连续串的表述后复扫 rc=0。
> **边界**：安全报告/规则文档在引用形态时不得写出可命中的连续串（含长斜杠串），否则机器门会（正确地）报红。

**并发竞态 vs 不可判定（本轮新增，避免工作区多写者时抖动）**：扫描集来自 `git ls-files`，枚举后文件可能被别的线删除。判定规则：
若该路径**已不再是 tracked 条目** → 计入 `files_vanished`（不判红，因为它已不属于扫描集）；若 git 仍认为它 tracked（真丢文件）→ 按 `file_unreadable` **判红**；
且**零文件实扫**时补判 `nothing_scanned` 判红（防止"全消失=恒绿"）。实测修复前 pack 范围在同一文件集下 rc 在 0/1 间抖动，修复后连续 3 次 rc=0。

**规则定义文件降级注册表（精确 2 项，测试锁定）**：`PATTERN_DEFINITION_FILES` = {`tools/quality/check_secret_hygiene.py`, `tests/quality/test_secret_hygiene.py`}。
理由：规则本体与测试夹具按定义必须写出原始形态字面量（前缀、赋值写法、私钥头拼接、哨兵串），且两个文件名都含 secret 从而被归为凭据类文件 → 会自我判红（本轮真实发生）。
语义是**降级不是豁免**：文件照扫、命中照列（路径/形态/行号），只是不判红；清单由 `test_pattern_definition_registry_is_exact` 断言精确等于这 2 项，防止后续被偷偷扩大。

### 6.4 报告与证据

- `reports/PROJECT-GOVERNANCE-01/security/SECRET_HYGIENE_REPORT.md`（本文件）
- `reports/PROJECT-GOVERNANCE-01/security/EXPOSURE_NOTE.md`（暴露面与处置，替代原 ROTATION_DECISION）
- `run/PROJECT-GOVERNANCE-01/ROOT-006/logs/`：`FINAL_EVIDENCE.txt`、`ev_pack_final.json`、`ev_tracked_final.json`、`ev_whitelist_*.json`、`whitelist_boolean_table.md`、`infra_exposure.json`、`refs_files_final.txt`、`packer_deny_selftest.py`、`apply_chk_secret_hygiene_registration.py` + 补丁 JSON

---

## 7. 验收门逐条（前台可独立复跑）

| 门 | 命令 | rc | 关键输出 |
|---|---|---|---|
| 白名单移除 + 显式指定也不入包 | `python3 run/PROJECT-GOVERNANCE-01/ROOT-006/logs/packer_deny_selftest.py` | 0 | 8/8 PASS；`DENY 排除: 1 ['FATDUCK_ACCESS.md']`；zip 条目名不含它 |
| 形态扫描覆盖白名单全部文件、无值 | §2 表 + `python3 tools/quality/check_secret_hygiene.py --files <15 项> --report-only` | 0（14 项）/ 不可判定（悬空条目单列） | 15 条全列；FATDUCK 全 false；4 文件 INFO 行号 |
| 检查器正例 | `python3 tools/quality/check_secret_hygiene.py --scope pack --quiet`；`--scope tracked --quiet` | 0 / 0 | 各 `verdict: PASS`、`fatal_paths: []` |
| 检查器负例 | `python3 -B -m unittest discover -s tests/quality -t tests/quality -p "test_secret_hygiene.py"` | 0（**22 tests OK**） | 负例 7 项均 rc≠0（假私钥／`sk-` 前缀／password 赋值、凭据类名+hex、赋值字面量、不可读、超限、空集、根不存在、打包器缺失） |
| fail-closed | 同上（5 项 fail-closed 用例） | — | `undecidable_input`/`empty_scan_set`/`walk_root_missing`/`packer_unavailable:missing` |
| 无值泄漏 | 同上 `test_report_leaks_no_matched_value` + §2 人工复核 | 0 | 哨兵串不出现在报告/ stdout；报告只有 路径/布尔/行号 |
| `.gitignore` 条目 | `git check-ignore -v --no-index FATDUCK_ACCESS.md` | 0 | `.gitignore:149:FATDUCK_ACCESS.md	FATDUCK_ACCESS.md` |
| （同上，默认判定） | `git check-ignore -v FATDUCK_ACCESS.md` | **1** | 无输出 —— **因为文件仍是 tracked**（这就是 §6.1 的口径） |
| ROOT-002 条目未被破坏 | `grep -qxF <each> .gitignore` | 0×6 | 6/6 在位 |
| CHK-SECRET-HYGIENE 注册 | **未执行**（冻结令 + 待批，见 §8） | — | 补丁 `run/.../CHK-SECRET-HYGIENE.registration.json` + `apply_...py --check` 干跑通过（只追加、幂等、不改既有条目） |
| 全程未读凭据内容 | §1 命令清单 | — | 无内容读取命令、无复制、无行内容打印 |
| 并发稳定性 | `for i in 1 2 3; do python3 tools/quality/check_secret_hygiene.py --scope pack --quiet; echo rc=$?; done` | 0/0/0 | 三次均 PASS、`files_policy_downgraded: 2`、`fatal_paths: []` |
| 规则定义降级被锁定 | 同上单测中的 `test_pattern_definition_registry_is_exact` | 0 | 注册表精确 = {规则本体, 其测试} |

---

### 7.1 与既有 CI 套件的交叉验证（前台关注）

- 注册项 `UT-QUALITY` 的执行方式为 `python3 -B -m unittest discover -s tests/quality -t tests/quality`：
  实测 **Ran 120 tests**，其中 `test_secret_hygiene` 的 22 项**全部通过**（单独 discover 复跑 3/3 绿）。
- 同次运行存在 **2 failures + 4 errors**，全部落在 `test_doc_machine_check` / `test_docchk002_mutation`，
  根因为 `DOCCHK-001 FAIL: build/astrocs --help rc=2（命令树 golden 不可读，fail-closed）` —— 属**构建产物缺失/并发清运**导致的既有问题，**与 ROOT-006 改动无关**（本任务未触碰 `docs/`、`build/`）。
  证据：`run/PROJECT-GOVERNANCE-01/ROOT-006/logs/UT_QUALITY_full.txt`。
- 真仓正例测试对「并发写」做了最多 3 次重试（竞态来自 `git ls-files` 与读盘之间别的线删文件；CI 单写者不触发），**判定口径未放松**。

---

## 8. 未决项 / 需要裁决

1. **CHK-SECRET-HYGIENE 注册**（负责人 2026-09-16 裁决：**暂不注册**）：CI-001 正在收敛 147 项 ID，避免同文件写入交集；补丁与 `--apply` 脚本已备好，由前台在 CI-001 落定后套用。注册命令建议 `--scope tracked`（当前实测 rc=0）。
2. **`git rm --cached FATDUCK_ACCESS.md` 是否执行**：属负责人裁决 + 前台执行（SubAgent 零 git 写权限）；命令见 `EXPOSURE_NOTE.md` §6。当前**忽略规则不解决存量**，未执行前该文件仍是 tracked 且仍可被 push。
3. **`ci/impact_map.json` / `ci/root_manifest.json` 的登记口径**：前者把该文件列为根触发项（ROOT-004 域），后者在 `registered_local_retention`（class F2，注「核对后定去留」）——解除 tracked 后建议由对应负责人把「去留」结论写实，本任务不改这两个文件（文件域外）。
4. **旧审计包 zip**：`artifacts/prerelease_v5/AUDIT_PACKAGE_587fe0e341a7.zip` 当时条目名含该文件（已实测），该目录现已不在工作区；**若此前已外发第三方，属既成事实**（见 `EXPOSURE_NOTE.md`）。
5. **白名单重写**：本次只加排除保证，未重写 `ROOT_FILES`（含悬空 `CHANGELOG.md`、缺失设计文档、`top in ROOT_FILES` 过宽子句）——建议并入 PKG-001/CI-001 域的「打包白名单评审」。
6. **内容引用覆盖边界**：CHK-SECRET-HYGIENE 只抓形态，不抓「把接入说明正文贴进文档」；防这类需要评审纪律或文档级查重（登记为已知边界）。
7. **并发红**：`CHK-ROOT-CLEAN` 当前 rc=1，违规为 `artifacts` 目录 `missing_required_dir`（ROOT-007 清运导致），**与本任务改动无关**（本次未触碰根条目）。
