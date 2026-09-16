# EXPOSURE_NOTE —— ROOT-006 暴露面说明与处置建议（**不轮换**）

> 本文件替代任务卡原定的 `ROTATION_DECISION.md`（负责人 2026-09-16 裁决：**不轮换任何凭据**，因为没有凭据）。
> 输出纪律：只写**暴露面类别、路径、布尔、行号、流程**；**不复写任何凭据值、口令、token，也不复写基础设施标识字面量**（节点地址/用户名/私钥名一律以「形态」描述）。
> 证据：`run/PROJECT-GOVERNANCE-01/ROOT-006/logs/`（含 `FINAL_EVIDENCE.txt`、`infra_exposure.json`）。

---

## 1. 结论（一句话）

**不需要轮换。** 该文件正文是 **Windows 验证节点的接入说明**（主机与身份、常用 ssh/scp 命令、在线时间窗、离线处理策略、使用范围），**不含明文口令 / token / 私钥内容**；私钥实体在验证节点本机（`/root/.ssh/` 下），**不在本仓库**。因此本次处置是 **「缩小暴露面」而不是「吊销凭据」**。

---

## 2. 暴露面事实（全部为元数据/结构判定，未读该文件正文）

| # | 事实 | 判据（可复跑） |
|---|---|---|
| E1 | 文件**处于 tracked** 状态至今 | `git ls-files --error-unmatch FATDUCK_ACCESS.md` → rc=0；`git ls-files -s` → blob 100644 |
| E2 | 首次且**唯一一次**入库：`2f20a99d`（2026-08-29），此后未修改 | `git log --follow --oneline -- FATDUCK_ACCESS.md` → 1 条；工作区 mtime 与本地文件 0600 权限 |
| E3 | **已推送远端**：该提交是 `origin/main` 的祖先 | `git merge-base --is-ancestor 2f20a99d origin/main` → rc=0 |
| E4 | **已扇出到外发审核包**：`artifacts/prerelease_v5/AUDIT_PACKAGE_587fe0e341a7.zip`（2026-09-04 构建，来源提交 `587fe0e341a7` 含 E2）条目名含该文件（1699 条目；当时以 `zipfile.namelist()` 实测，只列条目名） | 打包器白名单 `ROOT_FILES` 收录 → 本次已修（见 §4） |
| E5 | 引用面：**52 个 tracked 文件**提及该路径，**内容引用 0**（结构判据 + 负责人授权确认） | `git grep -l -F`；详见 `SECRET_HYGIENE_REPORT.md` §3 |
| E6 | 文件本体 15 类凭据形态扫描**全 false**（无 hex32/40/64、无 b64、无私钥头、无 `sk-`、无 `password=`） | `check_secret_hygiene.py --files FATDUCK_ACCESS.md --report-only` → rc=0 |
| E7 | 全仓**无私钥实体文件**入库：文件名匹配「id_ 前缀族」或扩展名 .pem／.key／.ppk／.p12／.pfx／.env／.netrc 的 tracked 文件 = **0 个**；全仓无 PEM 私钥头形态（0 文件）。注：私钥**文件名串**仍会以文本形式出现在「检测规则常量」「本报告」「测试夹具」中（属检测规则与说明，非文件本体） | `git ls-files` 形态过滤；`--scope tracked` 扫描 `files_with_critical=0` |
| E8 | **同类暴露面**：另有 11 个 tracked 文件含节点地址/私钥路径/ssh 目标形态（作废世代证据、v6 报告、`docs/archive/history` 运维日志、治理台账） | `enum_infra_exposure.py` + 报告 §4 表；处置并入 ROOT-007 清运（负责人已定） |
| E9 | 仓库里出现该节点信息的历史脚本（`reports/REAUDIT_V3/scripts/*.sh`，内含 `ssh -i <私钥路径> <用户>@<节点地址>` 形态）**已不在 tracked** | `git ls-files reports/REAUDIT_V3/scripts` → 0（ROOT-007 批次已收走） |

---

## 3. 为什么不需要轮换

1. **没有可轮换的对象**：正文无明文口令/token/私钥（E6 + 负责人授权阅读确认）；E7 证明仓库内无私钥实体。
2. **私钥实体从未入仓**：密钥只存在于验证节点本机路径（E7 的反证），仓库里的只是**路径字符串**。
3. **残留信息属「侦察信息」而非「凭据」**：主机可达地址、登录用户名、私钥落点路径、sshd 配置要点 —— 这些能**降低攻击者前期的信息成本**，但不能直接用于认证（无口令、无私钥）。
4. **代价/收益**：轮换（换用户名/换私钥/改端口/重建节点信任）会打断 `.github/workflows/fatduck*.yml` 依赖的验证节点链路与既有证据链，而它并不能消除 E8 里已经散落的同类信息 —— 收益有限、代价明确。
5. 若负责人后续判定仍要轮换（例如节点暴露面另有隐情），**轮换清单**是：节点 ssh 私钥对（生成新对 + 更新本机 `authorized_keys` + 更新调用方私钥落点）、sshd 端口/来源限制、（可选）更换登录用户名；同步项见 §5 清单。

---

## 4. 已实施的机制修复（本次改动，不依赖轮换）

| 修复 | 位置 | 效果 |
|---|---|---|
| 审计包白名单移除 | `tools/pack_audit_package.py`（`ROOT_FILES`） | 不再进入外发 zip |
| **排除保证** | 同文件 `DENY_PATHS` + `DENY_NAME_RE` + `denied()`，`allowed()` 第一句判定 | **即使白名单被误加回 / 被显式指定也不入包**（负例 8/8 PASS） |
| 误加回护栏 | `.gitignore` 精确条目 | 防未来 `git add` 重新纳入 |
| 机器门 | `tools/quality/check_secret_hygiene.py` + 18 项单测 | 凭据形态在「审计包范围/全 tracked」判红；凭据类文件名升级判定；fail-closed |
| 基础设施形态可见 | 同上（INFO 级） | 节点地址/私钥路径/ssh 目标**只报告不判红**，为清运批次提供清单 |

**⚠️ 准确口径**：`.gitignore` 条目 **不解决存量** —— 忽略规则对已跟踪文件无效（`git check-ignore -v FATDUCK_ACCESS.md` 现在 rc=1；加 `--no-index` 才命中 `.gitignore:149`）。存量是否解除取决于 §6 的裁决与执行。

---

## 5. 引用同步清单（解除 tracked / 未来轮换时要核对的对象）

| 引用/依赖 | 是否受影响 | 处置建议 |
|---|---|---|
| `.github/workflows/fatduck.yml`、`fatduck-admin.yml` | **不受影响**：它们引用的是**节点**，**0 处**提及该文件路径 | 不动（本任务禁止改 workflows） |
| `docs/DOCUMENT_INDEX.yaml`（L42 提及） | 文件仍在本地 → 索引仍然有效 | 不动 |
| `ci/root_manifest.json`（`registered_local_retention`，class F2，注「核对后定去留」） | 解除 tracked 后**根目录整洁门仍绿**（门按物理条目判，该条已登记） | 建议由 ROOT-004 把「去留」结论写实 |
| `ci/impact_map.json`（根文件触发项） | 语义上仍是根条目 | 由该文件负责人决定是否调整触发映射 |
| `reports/**` 32 处 + `工程控制/**` 9 处 | 历史记录/台账，引用的是路径 | 不改（历史留痕） |
| `tools/pack_audit_package.py` | 已由本次移除 + 加排除保证 | 完成 |
| 若轮换节点身份（用户名/私钥/端口） | 则 11 个含节点标识的 tracked 文件 + 引用该节点的 workflows/报告都需复核 | 先做 ROOT-007 清运，再评估 |

---

## 6. 前台待执行命令（**原样给出，本 Agent 未执行任何 git 写操作**）

### 6.1 解除 tracked（保留本地文件）

```bash
cd "/workspace/Astro CS Database"

# 1) 只解除索引，不删除本地文件（AGENTS.md §5 禁读内容，但允许路径级操作）
git rm --cached -- FATDUCK_ACCESS.md

# 2) 核验：不再 tracked / 本地仍在 / 忽略规则现在生效
git ls-files --error-unmatch FATDUCK_ACCESS.md; echo "rc=$?   # 期望 1（已解除）"
test -f FATDUCK_ACCESS.md && echo "local_file_kept=yes"
git check-ignore -v FATDUCK_ACCESS.md; echo "rc=$?   # 期望 0，命中 .gitignore 条目"
```

### 6.2 提交计划（一个 commit = 一个目的）

```bash
# ⚠️ 并发提醒：.gitignore 里还含 ROOT-002 线未提交的 6 条（/p*-files.patch、/run_context.json、
#    /astrocs_p1sess_*/、/CS/、/Database/、/worktrees/ 与注释块）。若要只提交本任务改动，
#    用 git add -p 或先把 ROOT-002 的条目落定；否则本 commit 会连带提交 ROOT-002 的条目。

git add -- .gitignore tools/pack_audit_package.py tools/quality/check_secret_hygiene.py \
           tests/quality/test_secret_hygiene.py reports/PROJECT-GOVERNANCE-01/security/ \
           ci/checks.json            # ← ci/checks.json 仅在负责人批准注册后才 add，未批准请删除此项
git rm --cached -- FATDUCK_ACCESS.md
git commit -m "security(root): ROOT-006 凭据文件脱离 tracked + 审计包排除保证 + CHK-SECRET-HYGIENE" \
           -m "依据: AGENTS.md §5（不读取/打印密钥）、ENGINEERING_SPEC §7/§8（根条目纪律、唯一注册表、能绿能红）、ROOT-006 任务卡。" \
           -m "不轮换：正文为节点接入说明且无明文凭据（见 EXPOSURE_NOTE.md）。"

# 3) 推送后核对三 SHA 一致（AGENTS.md §7）
git push origin main
git fetch
git rev-parse HEAD main origin/main
```

### 6.3 提交后复跑（验收）

```bash
python3 tools/quality/check_secret_hygiene.py --scope tracked --quiet; echo "rc=$?"     # 期望 0
python3 -m unittest tests.quality.test_secret_hygiene; echo "rc=$?"                    # 期望 0（18 tests）
git check-ignore -v FATDUCK_ACCESS.md; echo "rc=$?"                                    # 期望 0（现在才生效）
```

---

## 7. 解除 tracked 之后的连带影响核对（已预先验证，避免踩 ROOT-002 的门）

| 影响面 | 核对结果 |
|---|---|
| 根目录整洁门 `CHK-ROOT-CLEAN` | 该条目已在 `ci/root_manifest.json` 的 `registered_local_retention` 中登记 → 解除 tracked 后**不会**因此判红（门按物理根条目判定，不看 tracked/gitignore） |
| 打包器 | 该文件本就被排除保证拦下 → 解除 tracked 后行为不变 |
| CHK-SECRET-HYGIENE | 解除 tracked 后其不再进入 `--scope tracked` 扫描集；门仍绿（当前两种范围均 PASS） |
| workflows | 不引用该路径 → 无影响 |
| 负责人阅读路径 | 本地文件保留（`git rm --cached` 不删文件），接入说明仍可用 |

---

## 8. 需要负责人裁决

1. **是否执行 §6.1/§6.2**（`git rm --cached` + 提交）。未执行前，该文件**仍是 tracked 且仍可被 push**；`.gitignore` 条目**不构成修复**。
2. **旧审核包外发事实**：`AUDIT_PACKAGE_587fe0e341a7.zip`（含该文件）**此前是否已发给第三方**？若已外发，是否需要一份「包内含节点接入说明」的说明函（内容级决定，不在本 Agent 权限内）。该 zip 现已不在工作区。
3. **E8 同类暴露面**（11 个含节点标识的旧证据/台账文件）并入 ROOT-007 清运批次的**范围与顺序**（负责人已定方向，需确认清单）。
4. **未来若在仓库内新增任何真实凭据**，必须走：① 不入库（只路径引用/环境变量/密钥管理）；② 若必须落盘则入 `.gitignore` + 打包器 `DENY_PATHS`；③ 提交前跑 `CHK-SECRET-HYGIENE`（`--scope tracked`）；④ 密钥轮换与吊销登记到本文件；⑤ 泄漏时按「先吊销后清理历史」处理（本仓禁止历史重写，故只能轮换 + 记录）。
