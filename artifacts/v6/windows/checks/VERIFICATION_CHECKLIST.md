# WIN-VERIFY-001 — Windows 验证步骤与判定清单

> 状态：**AWAITING_WINDOWS_VALIDATION**（Fatduck 不可达 + 基线 SHA 无候选；本清单未在 Windows 节点执行）
> 基线 HEAD：`8f5ef3e9c84590895ebaadbb30c0e6a7ab72d467` ｜ 产品版本：`0.11.0-alpha.2`
> 机器可读版：`checks/win_verify_cases.json`（32 用例）；参考执行器：`checks/run_windows_verification.ps1`

## 0. 边界（宪章 §15.3，强制）

Fatduck 是 Windows 10+ amd64 **正式验证节点**，不得作为开发节点：

- 允许：下载 GitHub Windows CI 生成并校验的候选、运行与本二进制绑定的 `cpu_profile`、用本地只读真实数据复验、生成资源曲线/运行图/manifest/数值摘要/图像预览、上传授权 JPG 与脱敏指标。
- **禁止**：checkout 源码、编译、运行仓库任意脚本、把原始 FITS/HiPS/索引/私有数据上传。
- 本包提供的 `run_windows_verification.ps1` 是**固定本地 harness 的参考实现**，只读候选产物与本地数据；它不是仓库业务脚本，须由节点运维部署到 `D:\AstroCSRunner\harness\` 后调用（既有 `fatduck.yml` 通道调用 `D:\AstroCSRunner\harness\run_validation.ps1`）。

## 1. 前置条件与通过门（fail-closed）

| 门 | 条件 | 不满足时 |
|---|---|---|
| P0 候选存在 | `astrocs-windows-candidate-<sha>` 产物存在且 `AstroCS-candidate.zip` 可下载 | 本轮为 **no_candidate**，不得进行任何"安装/ABI/长路径/大文件/真实产品"用例，不得判 PASS |
| P1 同 SHA 绑定 | `BUILD_PROVENANCE.json.source_sha` == 待验 HEAD SHA | FAIL（候选与目标不同 SHA） |
| P2 摘要链 | artifact digest 锚与 `Get-FileHash AstroCS-candidate.zip` 逐位相等 | FAIL |
| P3 非零用例 | `executed_cases > 0` 且 `skipped_cases < executed_cases` | rc=2，不算 PASS（FD-F-003） |
| P4 负向可红 | L-07 / B-06 / B-07 注入后必须 rc!=0 | 门无效，本轮不得 PASS |

> **当前 P0 不满足**：Windows CI run 35012779853 于 2026-09-15T19:25:07Z failure（失败步 = `Run MSVC tests and package candidate`），Upload candidate skipped。故本清单全部用例 `executed=false, verdict=UNAVAILABLE`。

## 2. 用例清单

### SAME_SHA_CANDIDATE（S-01..S-05）— 同 SHA 候选安装

| 用例 | 方法 | PASS | FAIL |
|---|---|---|---|
| S-01 | `ci/select_candidate.py` 的 artifact digest 锚 vs `Get-FileHash AstroCS-candidate.zip` | 逐位相等 | 空/不等（fail-closed） |
| S-02 | 解压 candidate，逐成员与 `SHA256SUMS` 比对 | 不一致数=0 | 任一缺失/不匹配 |
| S-03 | `BUILD_PROVENANCE.json.source_sha` == `git rev-parse HEAD` | 相等 | 不等 |
| S-04 | 安装树 16 项对照 `CANDIDATE_MANIFEST.expected_candidate_members` | 16/16 | required 缺失 |
| S-05 | `astrocs.exe version --json` | rc=0 且 name=astrocs/version 非空 | 否则 FAIL |

安装步骤与预期退出码：解压 zip → 校验 SHA256SUMS（rc 0）→ 从候选根运行 `astrocs.exe selftest`（rc 0）→ `astrocs.exe modules verify`（rc 0）→ `astrocs.exe version --json`（rc 0）。任何 rc≠0 即 FAIL。

### ABI（A-01..A-08）

| 用例 | 方法 | PASS | FAIL |
|---|---|---|---|
| A-01 | `dumpbin /EXPORTS` 4 个受检 DLL | 4/4 rc=0 且符号数>0 | 任一 rc≠0 或 0 符号 |
| A-02 | grep `acs_artifact_*`(runtime) / `acs_fio_*`(io) | 两族各≥1 | 任一 0 命中 |
| A-03 | `version --json` 的 `abi_version` vs `common_abi_v1.h` `ACS_ABI_VERSION_V1=1` | 相等 | 不等 |
| A-04 | `dumpbin /HEADERS` 机器类型/链接器版本 | x64 + 记录 MSVC/SDK patch | 非 x64 |
| A-05 | `dumpbin /DEPENDENTS` CRT 依赖 | VCRUNTIME140/MSVCP140 在位（/MD） | 缺 CRT/静态 CRT 冲突 |
| A-06 | `dumpbin /DEPENDENTS` grep zlib1/gsl DLL | 0 命中（static-md） | 命中即 FAIL |
| A-07 | `modules list` / `modules verify` | 均 rc=0 且枚举非空 | 任一 rc≠0/空 |
| A-08 | `selftest` | rc=0 | rc≠0 |

静态/动态链接面结论：`CMakePresets` preset `win-msvc-17.14.39-x64` + vcpkg `x64-windows-static-md` ⇒ zlib/GSL **静态**；MSVC CRT **动态 /MD**。A-05/A-06 正是这一面的红路径。

### LONG_PATH（L-01..L-07）— > 260 字符

| 用例 | 方法 | PASS | FAIL |
|---|---|---|---|
| L-01 | candidate 复制到总路径 >260 处，从该目录运行 `version --json` | rc=0（DLL 长路径解析成功） | rc≠0 |
| L-02 | >260 输入 FITS：`phase1 validate/plan` | rc=0 | 路径长度错误/未处理异常 |
| L-03 | >260 输出目录：`phase1 run` + `inspect/verify` | rc=0 且重开成功 | 写盘/重开失败 |
| L-04 | 259/260/261 边界三连 | 261 亦成功；否则记录 False 判定与注册表开关 | 任一档路径错误即 FAIL |
| L-05 | 文件名分量 255/256 | 255 成功；256 明确错误码干净拒绝 | 崩溃/256 静默接受 |
| L-06 | manifest/provenance 路径字段 | 完整无截断 | 截断 |
| L-07 | 负向非法长路径 | 受控非 0 且无半成品 | rc=0 或残留 |

判定纪律：**路径长度导致的任何失败一律 FAIL**（不得以"Windows 默认限制"为由降级）。

### BIG_FILE（B-01..B-07）— > 2 GiB

| 用例 | 方法 | PASS | FAIL |
|---|---|---|---|
| B-01 | Phase3 导出/大图配置产生 >2147483648 B FITS | rc=0；尺寸>2^31 且 2880 对齐 | rc≠0/尺寸不达标/未对齐 |
| B-02 | `phase3 inspect` / `verify --run-manifest` 重开 | rc=0 且字节数正确 | rc≠0/字节数回绕 |
| B-03 | CHECKSUM/DATASUM + 独立流式重算 | DATASUM 一致、CHECKSUM 标准格式 | 缺失/不等/格式非法 |
| B-04 | >2^31 字节偏移处读已知字 | 逐位一致（64 位偏移） | 错误块/溢出/异常 |
| B-05 | NAXIS1×NAXIS2 > 2^31 元素 | 64 位计数正确、尺寸自洽 | 负值/回绕 |
| B-06 | 截断 2880 B 后重开 | 非 0 退出（fail-closed） | 假绿 |
| B-07 | >2^31 处翻转 1 B | DATASUM 变化且 verify 失败 | 假绿 |

### REAL_PRODUCT（R-01..R-05）

| 用例 | 方法 | PASS | FAIL |
|---|---|---|---|
| R-01 | Linux 真实产物跨平台重开 + 数据数组 SHA-256 比对 | 逐字节一致、校验和通过 | 不一致 |
| R-02 | 真实数据 Phase1 数值 vs REAL-SCIENCE-001 §4/§5 参考 | 容差内 | 超容差/不可复现 |
| R-03 | effective PSF FWHM 比对 | \|Δ\|<0.5 px | ≥0.5 px 无归因 |
| R-04 | 1/4/16 worker 确定性 digest vs PERF-SCALE-001 | 跨 budget 一致且与 Linux 参考一致 | 不一致/未归因差异 |
| R-05 | 产品 manifest/资源曲线/图像预览/公开白名单 | 齐全且仅白名单 | 缺件/泄漏原始数据 |

Linux 侧参考值（来自 `reports/v6/real-science/REAL-SCIENCE-001_REPORT.md` §4/§5 与 `artifacts/v6/performance/determinism_v6_write_paths.json`）：
- 确定性 digest：`v6_p1_phase1_write=46e62599919bac0d3ef2b6eaaede608cd7a1ecb3e6171f39e7bf57de5220258f`(4 files)、`v6_p2_three_modes_write=9ef8cbfd472d0b4e4f83754cf2da259a746283e2510ca07f3ad56fc99e9df33d`(17)、`v6_p3_export_write=23ca5bf145c28b834f373f376a35a6c0ffb2a350abb92ae8d13bd10c8fcdf912`(9)。
- M42：equal MF-SNR 13862.2、W_info 14470.0、PSFSW 13476.3；背景 1178.1±16.3 ADU；FWHM equal 3.144 / ivar 3.176 / W_info 3.161 / PSFSW 3.105 px。
- Galaxy_Center：equal 5684.9 / W_info 5776.2；背景 1442.0±44.3；FWHM equal 2.252。
- LDN43：equal 7087.6 / W_info 7806.7；背景 3085.1±44.1。

> 注：REAL-SCIENCE-001 的 equal/exposure/ivar 口径为 DOCUMENTED_BASELINE（驱动内实现），W_info/PSFSW 走冻结库函数；该验收**不是**全链生产路径端到端验收。R-01 的跨平台重开是对"磁盘产物"的独立补充。

## 3. 复现步骤（Windows 节点）

```powershell
# 0) 部署参考 harness（节点运维，一次性）
Copy-Item .\run_windows_verification.ps1 D:\AstroCSRunner\harness\ -Force

# 1) 下载候选（既有通道；不允许 checkout）
gh run download <windows_run_id> -n astrocs-windows-candidate-<sha> -D D:\AstroCSRunner\runs\incoming
Expand-Archive D:\AstroCSRunner\runs\incoming\AstroCS-candidate.zip -DestinationPath D:\AstroCSRunner\runs\candidate\<sha>

# 2) 运行固定入口
& D:\AstroCSRunner\harness\run_windows_verification.ps1 `
  -CandidateDir 'D:\AstroCSRunner\runs\candidate\<sha>' `
  -SourceSha '<sha>' `
  -ResultDir 'D:\AstroCSRunner\runs\publish\<sha>\win-verify'
```

退出码：0=全部机器门禁通过；2=用例为零/未执行；10=candidate/provenance 错；20=数据 manifest 错；30=科学/功能错；40=资源/性能错；50=公开白名单错；60=环境错。任何失败仍生成脱敏 summary，不生成 PASS。

## 4. 未执行声明

本清单**未在 Windows 节点执行任何用例**。Fatduck 实测不可达（ssh `Connection timed out`、TCP 22 超时、ICMP 100% 丢包、Tailscale `offline, last seen 3h ago`），且基线 SHA 无候选。所有 `verdict=PASS/FAIL` 均为空；当前统一 `UNAVAILABLE`。禁止把 Linux 结果或本清单冒充 Windows 通过。
