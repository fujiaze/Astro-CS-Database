# R0-001 工作树分类说明 (WORKTREE_CLASSIFICATION)

生成时间：2026-08-31T03:32:00Z
当前 commit：`30a3516ae8a3a1736bf8a470de56909997b1a219`（HEAD == main == origin/main）
`git status --porcelain` 共 205 项：5 个已跟踪修改 + 200 个未跟踪。

## 1. 三 SHA 身份结论

- HEAD = `30a3516ae8a3a1736bf8a470de56909997b1a219`
- main = `30a3516ae8a3a1736bf8a470de56909997b1a219`
- origin/main = `30a3516ae8a3a1736bf8a470de56909997b1a219`
- 三 SHA 相同；`git merge-base --is-ancestor b16d422a40fefedbdedab1749cfb8ebc06189736 HEAD` 返回 0，审核包自报 commit 是当前 main 祖先，符合 R0-001 验收。
- 仅一个分支 `main`，无其他本地/远端分支；remote URL 为 `https://github.com/fujiaze/Astro-CS-Database.git`，无凭据内嵌。

## 2. 已跟踪修改（5 项，均判定为前轮遗留、来源明确、非未知修改）

| 文件 | 修改时间 | 判定 |
|---|---|---|
| artifacts/prerelease_v5/ISA-001/MEASUREMENTS.csv | 2026-08-30 14:08 | V5 时代 benchmark 测量表刷新，未提交，前轮遗留。非 V6.1 生产代码。 |
| artifacts/prerelease_v5/ISA-002/MEASUREMENTS.csv | 2026-08-30 14:08 | 同上 |
| artifacts/prerelease_v5/ISA-003/MEASUREMENTS.csv | 2026-08-30 14:08 | 同上 |
| artifacts/prerelease_v5/ISA-004/MEASUREMENTS.csv | 2026-08-30 14:08 | 同上 |
| tests/cli/test_bench_cli.py | 2026-08-29 03:25 | 前轮 BENCH-005 后未提交的测试修正（binary_commit_full/prefix），来源明确。 |

以上 5 项均非本轮产生，属于“来源已知的外部未跟踪/未提交变化”。按规范不 stage、不提交，仅登记。它们不影响 G0 通过（无来源不明修改）。

## 3. 未跟踪项（200 项）分类

| 类别 | 数量 | 说明 | 来源 |
|---|---|---|---|
| astrocs_run_*.json | 162 | 前轮 CLI 运行 manifest（V6/V5 时代运行记录） | 前轮运行时输出 |
| AstroCS_* 控制包/审核包 zip+md | 9 | 历史控制包、审核包、证据请求（含本轮 V6.1 控制包） | 外部接收/前轮 |
| cpu_profile.json | 1 | benchmark 输出 profile | 前轮运行输出 |
| artifacts/prerelease_v5/audit_src/ + lnx_pkg/ + capsules/ + AUDIT_PACKAGE zip | 27 | V5 审核包快照与 Linux 包产物 | 前轮产出 |
| evidence/v6_1_rework/ | 1 | 本轮新建证据目录 | 本轮（R0-001 起） |
| 工程控制/AstroCS_V6_1_REWORK_CONTROL_20260831/ | 1 | 本轮解压的控制包目录 | 本轮解压 |

全部未跟踪项均有明确来源（前轮输出、外部包或本轮工作目录），无来源不明内容。

## 4. 冻结要求核对

- 控制包 zip SHA-256：`903018212bfb584b1aaf0dd05b318d8171d1a5b3af43014ab020834e9359ede6`
- 解压后 `validate_control.py` → `CONTROL_PASS files=47 bytes=214426 tasks=67 findings=40 prompt_chars=487`
- 台账（03_REWORK_TASK_LEDGER.csv）SHA-256：`c6f21304d1835ca0816af600c565223cbab129ada5ce7d5a84caf3d285736822`（与 MANIFEST.json 一致）
- `AstroCS.wiki` 是 index 中的 gitlink（`901725847ded7d1185a98b995df8fce9a7e20a1e`），无 `.gitmodules` 且未检出，为前轮遗留状态，不是本轮引入。

## 5. 结论

工作树所有 205 项变化均可解释、来源已知；无未知修改。R0-001 验收通过。
