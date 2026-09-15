# WIN-VERIFY-001 — Windows 正式节点复验报告

- 任务：WIN-VERIFY-001（V6 并行包 Wave 11）
- 基线 HEAD：`8f5ef3e9c84590895ebaadbb30c0e6a7ab72d467`（只读 `git rev-parse` 复核；与任务给定一致）
- 控制节点：Linux amd64（宪章 §15.1），未在 Windows 执行任何用例
- 结论标注：**AWAITING_WINDOWS_VALIDATION**（Fatduck 不可达 + 基线 SHA 无候选）
- 纪律声明：未 commit/push/add/分支/worktree/stash/reset/clean；未派生子代理；未越界写（仅 `artifacts/v6/windows/`、`reports/v6/windows/`）；未读取/复制/打印 SSH 私钥内容；未编造任何 Windows 侧结果或 sha256；未宣布发布，未提升 `VERSION`。

---

## 1. 任务与范围

按宪章在 Fatduck 复验同 SHA 候选安装、ABI、长路径（>260 字符）、大于 2 GiB、真实产品；不得宣布发布。本任务同时承载缺陷账本 AR-046（R7 边界校验半做 + Windows 盲区，须让 Windows 盲区检查不恒零）。

**Fatduck 边界（§15.3）**：不 checkout 源码、不编译、不运行仓库任意脚本；只下载 GitHub Windows CI 候选并运行固定本地 harness。故本包采取"**预构建候选 + 最小、自包含、可审计的验证步骤**"，并优先复用既有 `.github/workflows/fatduck.yml` 的 self-hosted runner 通道。

---

## 2. 方法

1. **可达性实测**：ssh（ConnectTimeout + BatchMode）、TCP:22、ICMP、Tailscale 状态；全部带 `timeout` 并落盘日志。私钥只以 `-i` 路径引用。
2. **GitHub Actions 核查**：本机无 `gh` CLI、无 token，改用无认证公开 REST API（仓库 `fujiaze/Astro-CS-Database` 为 public），逐条拉取基线 SHA 的 run/job/artifact/check-run/annotation 并落盘。
3. **验证包构建**：从冻结合同 `packaging/install-tree.contract.json` 经 `tools/quality/ci_windows_driver.py::expected_windows_artifacts` 本地只读推导候选成员清单；按任务四项 + 摘要链 + 负向门编写 32 用例判定清单与自包含参考 harness。
4. **留存**：所有命令的真实输出、退出码与 API 响应进入 `artifacts/v6/windows/logs/`，并在 `evidence/EVIDENCE_INDEX.json` 登记 sha256。

---

## 3. Fatduck 可达性实测（真实命令与输出）

| # | 命令（节选） | 退出码 | 真实输出 |
|---|---|---|---|
| P1 | `timeout 30 ssh -i /home/dsh/.ssh/id_ed25519_fatduck -o ConnectTimeout=15 -o BatchMode=yes fujia@100.104.10.71 "hostname"` | **255** | `ssh: connect to host 100.104.10.71 port 22: Connection timed out` |
| P2 | `ssh -v ...` | **255** | `debug1: Connecting to 100.104.10.71 [100.104.10.71] port 22.` / `debug1: connect to address 100.104.10.71 port 22: Connection timed out` |
| P3 | `timeout 20 bash -c 'exec 3<>/dev/tcp/100.104.10.71/22'` | **124** | （20s 超时，无输出） |
| P4 | `timeout 20 ping -c 3 -W 3 100.104.10.71` | **1** | `3 packets transmitted, 0 received, 100% packet loss` |
| P5 | `tailscale status | grep -i fatduck` | **0** | `100.104.10.71  fatduck  fujiaze04@  windows  offline, last seen 3h ago` |

环境：`Linux 6.12.107+deb13-amd64`；`OpenSSH_10.0p2`；`gh: NOT FOUND`；`GH_TOKEN/GITHUB_TOKEN/GITHUB_PAT: UNSET`。

**结论：Fatduck 当前不可达（UNREACHABLE）。** 按 §15.3，不阻塞 Linux 开发/CI/Linux 真实数据终验，但 Windows 复验只能标 `AWAITING_WINDOWS_VALIDATION`。

---

## 4. GitHub Actions 核查（Fatduck self-hosted runner 通道）

对基线 SHA `8f5ef3e9…` 的 run 共 5 条（`logs/28_gh_runs_final.json`）：

| run | workflow | 结论 | 关键事实 |
|---|---|---|---|
| 35012779853 | AstroCS Windows CI | **failure** | 失败步 = `5 Run MSVC tests and package candidate`（`ci/run.py --profile windows-main`，exit 1）；`Upload candidate` skipped；证据 artifact `windows-ci-8f5ef3e9c84590895ebaadbb30c0e6a7ab72d467`（204188 B，digest `sha256:5617018f7b027614c2aa1a00f0c5b4b8fa403a43c4157f61829e2088fcc46aee`） |
| 35012779858 | AstroCS Linux CI | in_progress | —— |
| 35012788684 / 35012969329 / 35013505132 | AstroCS Fatduck Validation | **failure** | `Select candidate via GitHub API` exit 3，annotation：`no-candidate：本轮无合格候选（CI-001 fail-closed）`；self-hosted job **skipped** |

历史窗口事实：

- **最近一次成功的 Windows CI 在 main 上为 2026-09-08**（run 34230322150，sha `2c998e9aa7a0`）；V6 线（09-13..09-15）Windows CI 全部 failure/cancelled。
- Fatduck Validation 最后成功为 run 34597817449（sha `026717fd3142`，2026-09-11T12:13:27Z）——说明 self-hosted runner 曾在 09-11 在线工作，当前离线。
- 无认证下无法下载 job logs/artifact（REST 403 "Must have admin rights to Repository."），故 Windows CI 失败的**步骤级**已知（step 5, exit 1）、**细则未知**。

**推论（对上游/控制器的关键阻塞）**：即使 Fatduck 立刻恢复，也**没有候选可装**——Windows CI 未产出 `astrocs-windows-candidate-<sha>`。Windows 复验的前置是 Windows CI 在目标 SHA green。

---

## 5. 验证包与四项覆盖

候选身份（期望，来自冻结合同）：artifact `astrocs-windows-candidate-8f5ef3e9…`，内含 `AstroCS-candidate.zip`，zip 根为候选根，16 个契约成员（`astrocs.exe`、`astrocs_runtime.dll`、`astrocs_io.dll`、5 个 module DLL、`providers/astrocs_cpu_baseline.dll`、licenses/schemas/astrocs.product.json）加 `README.txt`、`BUILD_PROVENANCE.json`、`SOURCE_MANIFEST.json`、`SHA256SUMS`。

| 覆盖项 | 用例 | 判定要点 | 当前判定 |
|---|---|---|---|
| 同 SHA 候选安装（sha256 清单 + HEAD 对应） | S-01..S-05 | artifact digest 锚 ↔ `AstroCS-candidate.zip`；`SHA256SUMS` 自洽；`BUILD_PROVENANCE.source_sha == HEAD`；16 成员在位；`version --json` | **UNAVAILABLE（无候选，sha256 留空）** |
| ABI（导出符号/依赖 DLL/运行库/静态动态链接面） | A-01..A-08 | `dumpbin /EXPORTS` 非空；`acs_artifact_*`/`acs_fio_*` 在位；`abi_version==1`；x64；CRT 动态 /MD；zlib/GSL 静态（无 DLL 依赖）；模块加载注册；selftest | **UNAVAILABLE（未在 Windows 执行）** |
| 长路径 > 260 字符 | L-01..L-07 | >260 安装树加载；>260 输入/输出；259/260/261 边界；分量 255/256；manifest 路径完整；负向 fail-closed。**失败即 FAIL** | **UNAVAILABLE** |
| 大于 2 GiB | B-01..B-07 | >2^31 B 写入与重开；CHECKSUM/DATASUM；>2^31 字节偏移 64 位读；元素计数不回绕；截断/翻转负向必须能红 | **UNAVAILABLE** |
| 真实产品复验 | R-01..R-05 | Linux 产物跨平台重开字节一致；数值对比 REAL-SCIENCE-001 §4/§5；FWHM<0.5 px；1/4/16 确定性 digest；公开白名单 | **UNAVAILABLE** |

参考值（Linux 侧，供比对，非 Windows 结果）：确定性 digest `v6_p1=46e62599…258f`(4)、`v6_p2=9ef8cbfd…f33d`(17)、`v6_p3=23ca5bf1…f912`(9)；M42 equal MF-SNR 13862.2 / W_info 14470.0 / PSFSW 13476.3、背景 1178.1±16.3 ADU、FWHM equal 3.144 px。

三项负向门（L-07 / B-06 / B-07）与零用例门（`executed_cases>0`）写入判定清单，直接对应 AR-046 "Windows 盲区检查不得恒零" 与 FD-F-003。

---

## 6. 未验证项 / 须在 Windows 节点执行的清单

完整清单见 `artifacts/v6/windows/UNVERIFIED_ITEMS.md` / `.json`。要点：

**前置（CI/控制器侧）**

1. 使 Windows CI 在目标 SHA green（当前 V6 线全红，最近 success 2026-09-08），产出候选 artifact。
2. 同 SHA Linux CI green（`ci/select_candidate.py` 双绿要求）。
3. 产出候选 digest 与成员摘要锚。

**Fatduck 侧（只下载候选 + 固定本地 harness）**

4. 下载/解压候选；5. 计算并回填 zip 与 16 成员**真实 sha256**；6. 校验 SHA256SUMS、source_sha、工具链 patch；7. ABI（dumpbin 三类 + modules/selftest/version）；8. 长路径 L-01..L-07；9. 大于 2 GiB B-01..B-07；10. 真实产品 R-01..R-05；11. 产出脱敏 summary；12. 公开白名单校验。

**控制器/负责人侧欠账**

- **FD-F-003（P0，仍 OPEN）**：Windows C++ 单测门长期"0 用例 PASS"（`No tests were found!!!`）。**恢复"用例数>0"前，Windows C++ 单测面不得作为发布证据引用**；本任务不改 `ci/`（越界），仅登记。
- Windows CI 失败根因（需 token 下载 `win-stage-*.log` / `win-package-summary.json`）。
- 跨平台真实产物传输政策（R-01）。
- 参考 harness 落地与 `harness.lock.json` 锁定。

---

## 7. 结论与发布声明

- **Windows 复验结论：AWAITING_WINDOWS_VALIDATION。** 未取得任何 Windows 侧证据；32/32 用例未执行。
- **不建议 PASS**：任务正文四项均未在 Windows 节点取得结果；候选不存在。
- **不得据此发布**：§17.12 发布门 7（同 SHA Linux/Windows CI 通过）与门 8（Fatduck Windows 复验）均未满足；宪章 §15.3 明文禁止据此发布 Windows 版本。Agent 无权宣布发布，`VERSION`/alpha 编号未提升。
- **AR-046 状态**：本包提供了可执行的 Windows 盲区用例与非零/负向门设计，但**盲区是否真实闭合取决于 Windows 节点执行结果**；当前仍为 OPEN，待 Fatduck 恢复后回填。

---

## 8. 复现命令清单（Linux 侧，全部带 timeout；退出码为实测）

    cd "/workspace/Astro CS Database"
    git rev-parse HEAD                                            # rc 0 -> 8f5ef3e9…
    tailscale status | grep -i fatduck                            # rc 0 -> offline, last seen 3h ago
    timeout 30 ssh -i /home/dsh/.ssh/id_ed25519_fatduck -o ConnectTimeout=15 -o BatchMode=yes       fujia@100.104.10.71 "hostname"                              # rc 255 Connection timed out
    timeout 20 ping -c 3 -W 3 100.104.10.71                       # rc 1   100% packet loss
    command -v gh                                                 # rc 1   not found
    curl -sS https://api.github.com/repos/fujiaze/Astro-CS-Database/actions/runs?head_sha=8f5ef3e9…   # rc 0 (public)
    # Windows CI run 35012779853 -> failure (step 5); Fatduck select exit 3 no-candidate

    # 复验包自检（本地）
    python3 -c "import json;d=json.load(open('artifacts/v6/windows/checks/win_verify_cases.json'));print(d['counts'])"   # rc 0

Windows 节点侧复现步骤见 `checks/VERIFICATION_CHECKLIST.md` §3（`gh run download …` → `Expand-Archive` → `run_windows_verification.ps1`）。

---

## 9. 产出清单

`artifacts/v6/windows/`：`README.md`、`FATDUCK_REACHABILITY.json`、`GITHUB_ACTIONS_EVIDENCE.json`、`UNVERIFIED_ITEMS.md/.json`、`candidate/CANDIDATE_MANIFEST.json`、`candidate/CANDIDATE_SHA256.csv`、`candidate/CANDIDATE_PROVENANCE_CHAIN.json`、`checks/win_verify_cases.json`、`checks/VERIFICATION_CHECKLIST.md`、`checks/run_windows_verification.ps1`、`evidence/EVIDENCE_INDEX.json`、`logs/*`。

`reports/v6/windows/`：本报告 `WIN-VERIFY-001.md`。

## 10. 纪律与诚实性声明

- 未 commit/push/add/分支/worktree/stash/reset/clean（无任何 git 写操作）；
- 未越界写（仅 `artifacts/v6/windows/`、`reports/v6/windows/`；临时文件在 `/tmp`）；
- 未派生子代理；
- 未读取/复制/打印 SSH 私钥内容（仅 `-i` 路径引用）；
- 未把 Linux 结果或"0 用例"冒充 Windows 通过（FD-F-003 明确登记）；
- 未编造 Windows 侧结果或 sha256（候选 sha256 全部 UNAVAILABLE）；
- 未宣布发布、未提升 `VERSION`/alpha；未改生产源码/测试/docs/contracts/ci。
