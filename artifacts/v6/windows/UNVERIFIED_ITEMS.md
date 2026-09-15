# WIN-VERIFY-001 — 未验证项清单

> 全部条目**未在 Windows 节点执行/确认**。本文件不构成 Windows 通过证据。
> 基线 HEAD `8f5ef3e9c84590895ebaadbb30c0e6a7ab72d467` ｜ 状态 **AWAITING_WINDOWS_VALIDATION**

## A. 环境级未验证（已实测为不可用/未满足）

| 项 | 实测状态 | 证据 |
|---|---|---|
| Fatduck SSH 可达性 | **UNREACHABLE**（ssh `Connection timed out`, rc 255） | `logs/01_ssh_connecttimeout.log`、`FATDUCK_REACHABILITY.json` |
| Fatduck TCP:22 | **UNREACHABLE**（20s 超时, rc 124） | `logs/03_tcp22.log` |
| Fatduck ICMP | **UNREACHABLE**（100% 丢包, rc 1） | `logs/04_ping.log` |
| Fatduck Tailscale 在线态 | **offline, last seen 3h ago** | `logs/05_tailscale.log` |
| 基线 SHA 的 Windows 候选 | **不存在**：Windows CI run 35012779853 failure（step 5），Upload candidate skipped | `GITHUB_ACTIONS_EVIDENCE.json` |
| self-hosted runner 接单 | 未接单：3 次 fatduck-validate 的 self-hosted job 均 skipped（select-candidate 无候选先失败） | `logs/11_*`、`logs/12_*` |
| Windows CI 失败根因 | **UNKNOWN**（job logs/artifact 下载需认证，REST 403） | `GITHUB_ACTIONS_EVIDENCE.json.limitation` |

## B. 任务四项：未执行用例（32/32）

| 分组 | 用例数 | 执行数 | 通过数 | 状态 |
|---|---|---|---|---|
| SAME_SHA_CANDIDATE（同 SHA 候选安装 + sha256 清单） | 5 | 0 | 0 | UNAVAILABLE |
| ABI（导出符号/依赖 DLL/运行库版本/静态动态链接面） | 8 | 0 | 0 | UNAVAILABLE |
| LONG_PATH（>260 字符，路径与文件名边界） | 7 | 0 | 0 | UNAVAILABLE |
| BIG_FILE（>2 GiB，写入/重开/CHECKSUM-DATASUM/偏移与计数） | 7 | 0 | 0 | UNAVAILABLE |
| REAL_PRODUCT（真实产物/真实数据复验） | 5 | 0 | 0 | UNAVAILABLE |

- 无任何 Windows 实测数值、无任何 Windows sha256（候选 sha256 全部留空，`candidate/CANDIDATE_SHA256.csv`）。
- 参考 harness `checks/run_windows_verification.ps1` **未执行、未在 PowerShell 下解析验证**（Linux 控制节点无 pwsh）；须在节点上先做 parse 校验再运行。

## C. 须在 Windows 节点执行/确认的具体清单（供负责人/运维/控制器）

前置（由控制器/CI 侧完成，不属于 Fatduck 动作）：
1. **让 Windows CI 在 `main` 上 green 于目标 SHA**（当前 V6 线全部 failure/cancelled；最近 success 为 2026-09-08）。Windows CI green 后 GitHub 才会产出 `astrocs-windows-candidate-<sha>`；否则一切 Windows 复验无从开始。
2. 同 SHA Linux CI green（`ci/select_candidate.py` 要求双绿 + artifact 存在）。
3. 由 `ci/select_candidate.py` 或 fatduck.yml 通道产出 **候选 artifact digest** 与 **`AstroCS-candidate.zip` 成员摘要锚**。

Windows 节点（Fatduck）动作（宪章 §15.3：只下载候选、跑固定本地 harness；不 checkout/不编译/不跑仓库脚本）：
4. 下载 `astrocs-windows-candidate-<sha>`，解压 `AstroCS-candidate.zip`。
5. 计算并记录 `AstroCS-candidate.zip` 与全部 16 个候选成员的 **真实 sha256**，填入 `candidate/CANDIDATE_SHA256.csv`（当前全为 UNAVAILABLE）。
6. 校验 `SHA256SUMS` 自洽；校验 `BUILD_PROVENANCE.json.source_sha == <sha>`；记录 MSVC/SDK patch 与实际工具链。
7. ABI：`dumpbin /EXPORTS|/DEPENDENTS|/HEADERS`；确认导出族 `acs_artifact_*` / `acs_fio_*` 在位；确认 CRT 动态（VCRUNTIME140/MSVCP140）、zlib/GSL 静态（无 zlib/gsl DLL 依赖）；`modules list`/`modules verify`/`selftest`/`version --json`。
8. 长路径：按 `checks/VERIFICATION_CHECKLIST.md` §L-01..L-07 构造 >260 字符路径与 255/256 文件名分量，执行并留证；**失败即 FAIL**。
9. 大于 2 GiB：按 §B-01..B-07 以 Phase3 大图导出产生 >2^31 B FITS，重开、CHECKSUM/DATASUM、>2^31 偏移与元素计数；截断/翻转负向必须能红。
10. 真实产品：用本地只读真实数据（数据根由本机只读配置提供，不上传、不打印）复验；与 Linux REAL-SCIENCE-001 §4/§5 参考值比对；跨平台重开 Linux 产物。
11. 运行 `run_windows_verification.ps1`（或已部署的固定 `D:\AstroCSRunner\harness\run_validation.ps1`），产出脱敏 summary；**只在真机证据到手后才可写 PASS/FAIL**。
12. 公开白名单：只上传授权 JPG/脱敏指标；原始 FITS/HiPS/索引/私有路径永不上传。

## D. 须控制器/负责人处理的阻塞与欠账（不在本任务 write_scope）

1. **FD-F-003（P0，仍 OPEN）**：唯一跑 Windows C++ 单测的门长期"0 用例 PASS"（`No tests were found!!!`）。在恢复"用例数>0"之前，Windows C++ 单测面**不得作为发布证据引用**；本任务只登记，不改 `ci/`（越界）。
2. **§17.12 发布门 7 未满足**：GitHub Linux/Windows 同 SHA CI 通过——当前 Windows CI 红、Linux CI 未完成，门未过。
3. **§17.12 发布门 8 未满足**：当前候选未在 Fatduck 完成 Windows 复验。
4. **候选缺失的根因**：Windows CI 失败细则需认证后下载 `windows-ci-<sha>` artifact 与 job logs（本机无 gh/token）。建议控制器在有 token 的环境取回 `run/ci/win-stage-*.log`、`win-package-summary.json` 定位。
5. **跨平台真实产物传输**：R-01 需要把 Linux 侧产物送 Windows 节点比对；是否允许由负责人/数据政策裁定（原始 testdata 不上传公开面）。
6. **参考 harness 落地**：`run_windows_verification.ps1` 需节点运维部署到 `D:\AstroCSRunner\harness\` 并锁定 sha256（harness.lock.json）。

## E. 明确声明

- 未把 Linux 侧结果冒充 Windows 通过；
- 未宣布发布，未提升 `VERSION`/alpha 编号；
- 未编造任何 Windows 侧结果或 sha256；
- 未修改任何生产源码/测试/docs/contracts/ci（仅写 `artifacts/v6/windows/`、`reports/v6/windows/`）。
