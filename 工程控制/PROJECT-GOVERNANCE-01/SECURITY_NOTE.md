# SECURITY_NOTE — 节点接入信息的暴露面收敛（2026-09-16）

> 本文件**只写路径与定性**，不复制任何凭据、私钥内容或主机值。依据 AGENTS.md §5 与负责人授权。

## 结论

- 负责人已授权读取 `FATDUCK_ACCESS.md`：内容为 **Windows 验证节点（Fatduck）的接入说明**，**无明文口令、无 token、无私钥内容**；私钥实体在仓库外。⇒ **不需要轮换凭据**。
- 结构核验：全仓无 `BEGIN OPENSSH PRIVATE KEY` 形态、无 `id_ed25519`/`id_rsa`/`.pem`/`.key` 入库。

## 同类暴露面（比单个文件更大）

用节点标识串全仓检索发现：**20 个 tracked 文件**含主机标识；其中**可执行脚本**直接写了 `ssh -i <私钥路径> <用户>@<主机>` 形态 —— 这类脚本可被直接复制执行，风险高于说明文档。

| 处置 | 对象 | 理由 |
|---|---|---|
| **已删除** | `reports/REAUDIT_V3/scripts/*.sh`（9 个） | 旧控制包（REAUDIT_V3）时代的远程执行脚本，含私钥路径与主机形态；属作废世代产物 |
| **已删除** | `reports/v6/final-audit/evidence/fatduck_probe.log` | 节点探活日志（含连接细节） |
| **保留** | `reports/evidence/WIN00{1,2,3}_verification.md`、`reports/v6/**` 的评审/审计报告、`docs/archive/history/**` | 属**评审结论与历史记录**，非可执行凭据；按 DOC-001 的文档收敛流程处置 |
| **保留（运营必需）** | `FATDUCK_ACCESS.md` | Windows 验证节点接入文档，被 CI/评审面引用；通过审计包白名单收窄暴露面（见下） |

## 已实施的结构性修复

1. `tools/pack_audit_package.py`：`ROOT_FILES` 移除 `FATDUCK_ACCESS.md`，并新增 `DENY_PATHS` / `DENY_NAME_RE` **排除保证**（即使被显式指定也不入包）——审计包是**外发**产物，这是本次最实质的收敛；
2. `.gitignore`：加精确条目作为「误加回」护栏（**须知：忽略规则对已 tracked 文件无效**，故它不解决问题，只是防线）；
3. `CHK-SECRET-HYGIENE`：新增检查项（白名单全量敏感形态扫描 + fail-closed + 能红能绿 + 无值泄漏哨兵），**注册待批**（等 CI-001 的注册表 ID 收敛落定后由前台套用）。

## 待办（交给后续任务）

- `git rm --cached FATDUCK_ACCESS.md` 与否：由负责人决定（保留文件可读性与仓库卫生的取舍）；
- `.github/workflows/fatduck*.yml` 的引用方式复核（属下一轮工程包）；
- 其余 17 个含节点标识的**报告类**文件随 DOC-001 收敛。
