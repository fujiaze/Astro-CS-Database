# 任务：ROOT-006 凭据入仓风险处置（FATDUCK_ACCESS.md 与审计包白名单）

状态：NOT_STARTED
层：L0　**优先级：SECURITY-URGENT**（唯一一条可即刻执行、且不依赖其它任务的安全项）
依赖：—　文件域互斥组：S4-T

## 目标

处置「凭据被纳入版本控制并被扇出到审计包」的风险：

1. 把凭据文件移出 tracked（保留本地文件）；
2. 修掉审计包工具把它打进包的白名单；
3. 逐项审计**其它曾被审核包收集过的文件**是否含敏感形态；
4. 给负责人一份**轮换裁决单**（是否需要轮换、轮换哪些、轮换后要改哪些引用）。

## 基线状态（编制时实测，2026-09-16，**全程未读取文件内容**）

| 事实 | 证据 |
|---|---|
| `FATDUCK_ACCESS.md` **处于 tracked 状态** | `git ls-files --error-unmatch FATDUCK_ACCESS.md` → 命中 |
| 首次且唯一一次入库：`2f20a99d`（2026-08-29），此后**未修改** | `git log --follow --oneline -- FATDUCK_ACCESS.md` → 1 条 |
| **未被 .gitignore 覆盖** | `git check-ignore -v FATDUCK_ACCESS.md` → 无匹配 |
| 被**审计包工具白名单**收录 | `tools/pack_audit_package.py:19` 含 `"memory.md", "FATDUCK_ACCESS.md", "HANDOVER.md", "VISUAL_CHECK_README.md"` |
| 结构层面：43 行；**无** 32/40/64 位 hex、无 base64 长串、无 `sk-` 前缀形态；含「凭据类词」布尔为真 | 结构扫描脚本（只输出布尔，未打印值） |
| 全仓被引用的规模 | 约 30 个 tracked 文件提及该路径（多为路径引用：REVIEW.md、docs/DOCUMENT_INDEX.yaml、docs/owner/*、evidence/**、reports/**、tools/pack_audit_package.py 等） |
| 内容是否含**真实可用凭据** | **未知**——本任务与执行 Agent 均**不读取该文件**；由负责人判定 |

## 权威依据

- AGENTS.md §5：「**不读取/打印密钥与凭据**（如 Fatduck 密钥只允许 `-i` 路径引用）」
- ENGINEERING_SPEC.md §7（禁止散落根目录）、§6（提交纪律）
- ASTROCS_DESIGN.md §0（权威链）、§12（不得用声明冒充）
- docs/ci/01_CHECKS.md（新增检查项的注册要求）

## 改动范围（文件域）

### 允许改
- `.gitignore`：新增 `FATDUCK_ACCESS.md`（精确条目，不加宽通配）
- `tools/pack_audit_package.py`：从白名单移除该条目，并给该文件加**排除保证**（即使被显式指定也不入包）
- 新增 `ci/checks.json` 检查项 `CHK-SECRET-HYGIENE`（**注册前必须避开 ci/checks.json 的并发写窗口**：与 CI-001 串行，见其任务卡冻结令）+ 对应检查器 `tools/quality/check_secret_hygiene.py` + 测试
- 新建 `reports/PROJECT-GOVERNANCE-01/security/`：引用清单、白名单根因分析、敏感形态扫描报告（**只允许写路径与布尔，禁止写任何值**）

### 禁止改 / 禁止做
- **禁止读取、打印、复制 `FATDUCK_ACCESS.md` 的内容**（含 base64、片段、哈希以外的任何形式）；本任务全程只做**结构级**判定；
- **执行 Agent 无权执行 `git rm --cached`**：该动作与提交一并由**前台**在负责人裁决后执行（SubAgent 零 git 写权限）；
- 不删除本地文件（只解除 tracked，文件保留在本机）；
- 不改 `.github/workflows/fatduck*.yml`（属下一轮工程包；本任务只登记依赖）；
- 不轮换任何凭据（轮换只由负责人执行）。

## 步骤

1. 结构级审计（不读内容）：对 `tools/pack_audit_package.py` 白名单中的**每一个**文件，跑敏感形态扫描（32/40/64 hex、长 base64、`sk-`、私钥头、`AWS/GCP/GitHub token` 形态、"password=" 形态），只输出**布尔 + 命中行号**；
2. 引用审计：列出所有提及 `FATDUCK_ACCESS.md` 的 tracked 文件，区分「路径引用」与「内容引用」，产出清单；
3. 白名单根因分析：为什么它会被收进审计包？是否还有其它「本不该入包」的条目（同时审计 `memory.md`、`HANDOVER.md`、`VISUAL_CHECK_README.md`）？
4. `.gitignore` 加精确条目 + 白名单移除 + 排除保证；
5. 新增 `CHK-SECRET-HYGIENE`：能红能绿——正例（干净树）绿；负例（临时把一份带形态的假文件放进被扫目录）红；要求 fail-closed（无法判定时判 FAIL）；
6. 产出**轮换裁决单**：哪些凭据可能已泄露（按「文件形态 + 是否曾 push」推断，不读值）、轮换后需要同步修改哪些引用（清单）、以及`git rm --cached` 的提交计划（前台执行）。

## 验收门（前台独立复跑）

- [ ] `git check-ignore -v FATDUCK_ACCESS.md` 命中 .gitignore 新条目；
- [ ] `tools/pack_audit_package.py` 白名单不再含该文件，且「显式指定也不入包」的负例可复现（给出命令与 rc）；
- [ ] 敏感形态扫描报告覆盖白名单**全部**文件，每条只含路径/布尔/行号，**无任何值**（人工复核 + 关键字扫描）；
- [ ] `CHK-SECRET-HYGIENE` 在 `ci/checks.json` 中注册成功（与 CI-001 的收敛不冲突，见其任务卡）；正例 rc=0、负例 rc≠0 均有证据；
- [ ] 轮换裁决单含「文件清单 + 推断依据 + 引用修改清单 + 提交计划」四项；
- [ ] 全程无对 `FATDUCK_ACCESS.md` 内容的读取（可用 shell 历史/命令日志自证：所有相关命令只做 stat/ls/grep -l）。

## 交付物

1. `.gitignore` 与 `tools/pack_audit_package.py` 改动；
2. `CHK-SECRET-HYGIENE` 检查器 + 测试 + 注册项；
3. `reports/PROJECT-GOVERNANCE-01/security/SECRET_HYGIENE_REPORT.md`（引用清单 + 白名单根因 + 形态扫描布尔表）；
4. `reports/PROJECT-GOVERNANCE-01/security/ROTATION_DECISION.md`（轮换裁决单，交负责人）；
5. 自证摘要。