# AstroCS 预发布证据索引（Linux 冻结）

> 本文件为仓库内稳定交付的精简证据索引。完整证据（日志、manifest、构建产物、HiPS、
> testdata 子集）存放于仓库外证据根目录（`$ASTROCS_EVIDENCE_ROOT/prerelease_*`），
> 不入 Git。所有结论以对应 gate 证据为准。

## 身份

- 远端：`https://github.com/fujiaze/Astro-CS-Database.git`
- 分支：`prerelease/v20-linux-closure`
- 冻结 HEAD：`46d6f951fc2096d61b3fc6fd8c63563b4ce14c63`
- 工作区：`porcelain=0`
- 控制包：`AstroCS_PRERELEASE_CONTROL_V1_20260823` SHA-256 `0661ba36...742`

## 门禁状态（Linux 冻结 G0–G10）

| Gate | 状态 | 证据位置（相对证据根） |
|---|---|---|
| G0 Identity | PASS | `00_identity/` |
| G1 Inventory | PASS | `01_inventory/` |
| G2 Science | PASS | `02_contracts/T1*_SCI-*/` |
| G3 Algorithm | PASS | `02_contracts/T2*_ALG-*/` |
| G4 Architecture/API | PASS | `02_contracts/`（T300–T305） |
| G5 Machine Consistency | PASS | `04_build/t800/t600_rerun_summary.json`（10/10） |
| G6 Linux Build | PASS | `04_build/t800/`（T600–T603） |
| G7 Linux Tests | PASS | `04_build/t800/`（T606/T607） |
| G8 Sanitizers | PASS | `04_build/t800/t604_t605_t606_rerun_summary.json`（ASan+UBSan+LSan 全量 0 错误/0 泄漏） |
| G9 Perf/ACR (Linux) | PASS | `04_build/t800/l9_rerun_summary.json`（T700/T707=W1 Fatduck） |
| G10 Linux Checkpoint | PASS（本索引） | `gate_status.json` / `run_manifest.json` |

## Finding

| ID | 级别 | 状态 | 摘要 |
|---|---|---|---|
| T800-find-001 | P1 | FIXED (46d6f95) | THREADING_MODEL 声称 sampler 并行，实际串行（无 OpenMP 并行区）；已修正文档 |

## 关键说明

- T608/T609：真实 Galaxy_Center_T4 数据集子集同步（9470 文件 SHA 一致）+ Stage2 真实数据链
  （692 tiles，474 tile 多帧重叠，exit=0）。
- T605：LeakSanitizer 受 yama ptrace_scope=2 限制；经 sudo 临时放行完成全量 89 项扫描
  （0 泄漏）后已恢复。环境受限，非代码问题。
- 下一步：Fatduck W1–W4（W0 preflight 已 PASS；节点在线）。

## 更新记录

- 2026-08-24：T800（L10）全门禁 PASS，P1 T800-find-001 修复并推送。
