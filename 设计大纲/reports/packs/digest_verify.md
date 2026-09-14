# Digest 与包本体实测比对报告（digest_verify）

- 复核对象：`设计大纲/_evidence/packs/digest/<身份>.md` 记录的实例清单 vs 存形态包本体实测
- 复核日期：2026-09-15（会话内实测）
- 复核人：AstroCS 控制包取证复核（digest 比对专项）
- 仓库根：`/workspace/Astro CS Database`

## 1. 抽样方法与结果

按任务规定，用 `python3` 执行 `random.seed(20260915)` 后按层顺序依次 `random.choice(candidates)`（候选顺序即任务书给定顺序），六个代际层各抽 1 包：

| 层 | 候选 | 抽中身份 | digest 文件 | 是否存在 |
|---|---|---|---|---|
| 前史开发包层 | engineering_v1.2 / engineering_v1.3 / AstroCS_CLI_Core_Development_Pack | **AstroCS_CLI_Core_Development_Pack** | digest/AstroCS_CLI_Core_Development_Pack.md | 是 |
| ACR 专项层 | ACR_FOCUSED_CONTROL_PACKAGE_V2 / ACR_FOCUSED_CONTROL_PACKAGE_V4 / acr | **acr** | digest/acr.md | 是 |
| 审计复审层 | AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827 / AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z / AstroCS_CP0 | **AstroCS_CP0** | digest/AstroCS_CP0.md | 是 |
| V5 发布层 | AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828 / prerelease_v5__AUDIT_REVIEW | **AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828** | digest/同名.md | 是 |
| V6/V7 重构层 | AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830 / AstroCS_V6_1_REWORK_CONTROL_20260831 / AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3 | **AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3** | digest/同名.md | 是 |
| V8/宪章/救援层 | AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905 / AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909 / AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912 | **AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905** | digest/同名.md | 是 |

6 个抽中身份的 digest 文件全部存在，**无需换候选**。

## 2. 实测口径

- **文件数 / 字节量级**：文件系统实例用 python `os.walk` 全量统计（含 `_PROVENANCE.json` 复原注入文件——该口径与 digest 生成口径一致，见 §4 观察 O-1）；zip 实例用 `zipfile` 统计**非目录条目数**与**解压后总字节**。KB = 字节/1024 保留 1 位小数。
- **台账行数与状态字面量分布**：python `csv` 模块解析（引号内嵌换行安全），"行数"= 数据行数（不含表头，与 digest 生成口径吻合，实测总物理行数恒为 +1）；状态分布按列名字面量为 `status` 的列统计（digest 记"状态列 None"者以表头实测复核）。
- **包内 SHA256SUMS 核对**：解析包根 SHA256SUMS(`.txt`) 的 `<hash>  <相对路径>` 行，逐行对包内实际文件计算 SHA256，计 ok（存在且相等）/ mismatch（存在但不等）/ missing（列出但文件不存在），并比对 mismatch_list。
- **zip sha256**：对 zip 文件本体 `hashlib.sha256`（等价 `sha256sum`）。
- 复原件一律实测 `设计大纲/_evidence/packs/history/...`、`history_zip/...` 下的现存副本；worktree 实例直接统计工作区。全程只读，未解压任何 zip 进仓库、未修改任何既有文件。

## 3. 逐包逐实例比对表

### 3.1 AstroCS_CLI_Core_Development_Pack（1 个实例）

#### 实例 1【recovered_from_history】设计大纲/_evidence/packs/history/036a3bb5/_new_pack_v1.1/AstroCS_CLI_Core_Development_Pack

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数 | 93 | 93 | 一致 |
| 字节量级 | 112.6 KB | 112.6 KB（115,280 B） | 一致 |
| SHA256SUMS 核对 | 文件 SHA256SUMS.txt；ok=91 / mismatch=0 / missing=0 / list=[] | 文件存在同名；ok=91 / mismatch=0 / missing=0 / list=[] | 一致 |

### 3.2 acr（2 个实例）

#### 实例 1【recovered_from_history】设计大纲/_evidence/packs/history/49ea5c1e/工程控制/tasks/acr

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数 | 4 | 4（checklist.md / spec.md / tasks.md / _PROVENANCE.json） | 一致 |
| 字节量级 | 46.1 KB | 46.1 KB（47,160 B） | 一致 |

（digest 未记该实例的 SHA256SUMS 与台账，实测目录内亦无 SHA256SUMS 文件，无缺项。）

#### 实例 2【worktree】engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/tasks/acr

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数 | 50 | 50 | 一致 |
| 字节量级 | 144.6 KB | 144.6 KB（148,117 B） | 一致 |
| SHA256SUMS 核对 | 文件 SHA256SUMS.txt；ok=48 / mismatch=0 / missing=1 / list=[] | ok=48 / mismatch=0 / missing=1 / list=[] | 一致 |

### 3.3 AstroCS_CP0（2 个实例，均为 zip）

#### 实例 1【zip】reports/REAUDIT_V3/cp0_out/AstroCS_CP0.zip

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数（zip 非目录条目） | 18 | 18（总条目 18，无目录条目） | 一致 |
| 字节量级（解压总字节） | 51.8 KB | 51.8 KB（53,050 B；zip 本体 18,839 B） | 一致 |
| zip sha256 | c79f50d7cdf8e9348d96b516cdead13bf94b2c649612a0591c4240fcb84f8e82 | 同左（逐字符相等） | 一致 |
| 台账行数（含表头列清单） | v3_cp0/TASK_LEDGER.csv 表头 ['task_id','gate','depends_on','scope','required_commit','required_push','checkpoint','status']；行 62 | 表头 8 列逐列相等；数据行 62（物理 63 行含表头） | 一致 |

（digest 未记状态分布；实测参考 {PASS:3, NOT_STARTED:59}，与该包 00_READ_FIRST"ID-001..003 PASS、其余 NOT_STARTED"自洽。zip 内 v3_cp0/SHA256SUMS 实测参考 17/17 ok，digest 未记此项，不计入比对。）

#### 实例 2【zip_recovered_from_history】设计大纲/_evidence/packs/history_zip/AstroCS_CP0.zip

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数（zip 非目录条目） | 18 | 18 | 一致 |
| 字节量级（解压总字节） | 51.8 KB | 51.8 KB（53,050 B；zip 本体 18,839 B） | 一致 |
| zip sha256 | c79f50d7…fcb84f8e82 | 同实例 1，逐字符相等 | 一致 |
| 台账行数 | 行 62，表头 8 列（同上） | 数据行 62，表头逐列相等 | 一致 |

### 3.4 AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828（2 个实例）

#### 实例 1【recovered_from_history】设计大纲/_evidence/packs/history/f99e80d8/工程控制/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数 | 38 | 38 | 一致 |
| 字节量级 | 98.1 KB | 98.1 KB（100,484 B） | 一致 |
| 台账行数 + 状态分布 | 02_TASK_LEDGER.csv 行 98，status 列 {PASS:1, NOT_STARTED:97}；任务号样例 BASE-001…ALG-004 | 数据行 98；分布 {PASS:1, NOT_STARTED:97}；前 16 任务号样例逐位相等 | 一致 |
| SHA256SUMS 核对 | 文件 SHA256SUMS；ok=35 / mismatch=1 / missing=0 / list=[02_TASK_LEDGER.csv] | ok=35 / mismatch=1 / missing=0 / list=[02_TASK_LEDGER.csv] | 一致 |

#### 实例 2【worktree】engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数 | 37 | 37 | 一致 |
| 字节量级 | 97.1 KB | 97.1 KB（99,444 B） | 一致 |
| 台账行数 + 状态分布 | 行 98，{PASS:88, BLOCKED:1, REVIEW_PENDING:1, IN_PROGRESS:1, NOT_STARTED:7} | 数据行 98；分布逐项相等（88/1/1/1/7，合计 98） | 一致 |
| SHA256SUMS 核对 | SHA256SUMS；ok=35 / mismatch=1 / missing=0 / list=[02_TASK_LEDGER.csv] | ok=35 / mismatch=1 / missing=0 / list=[02_TASK_LEDGER.csv] | 一致 |

### 3.5 AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3（1 个实例）

#### 实例 1【worktree】工程控制/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数 | 99 | 99 | 一致 |
| 字节量级 | 388.5 KB | 388.5 KB（397,833 B） | 一致 |
| 台账行数 + 状态列 | TASK_LEDGER.csv 行 191，状态列 None，分布 {}；样例 CTL-001…BLD-003 | 数据行 191（物理 192 含表头）；表头 11 列（task_id,wave,owner_id,kind,depends_on,mutex_lock,resource_class,commit_subject,spec_ref,acceptance,alias_of）确无 status 列，分布为空；前 16 任务号样例逐位相等 | 一致 |
| SHA256SUMS 核对 | SHA256SUMS；ok=97 / mismatch=1 / missing=0 / list=[TASK_LEDGER.csv] | ok=97 / mismatch=1 / missing=0 / list=[TASK_LEDGER.csv] | 一致 |

### 3.6 AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905（3 个实例）

#### 实例 1【recovered_from_history】设计大纲/_evidence/packs/history/a4fdee3f/engineering/control/active/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数 | 57 | 57 | 一致 |
| 字节量级 | 377.7 KB | 377.7 KB（386,724 B） | 一致 |
| SHA256SUMS 核对 | SHA256SUMS；ok=51 / mismatch=3 / missing=0 / list=[baseline/REVIEW_CLAIMED_TASK_STATE.csv, baseline/REVIEW_FINDINGS.csv, baseline/V7_1_STATIC_TASK_LEDGER.csv] | ok=51 / mismatch=3 / missing=0 / mismatch_list 三项逐一相等 | 一致 |

#### 实例 2【worktree】工程控制/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数 | 56 | 56 | 一致 |
| 字节量级 | 377.6 KB | 377.6 KB（386,677 B） | 一致 |
| SHA256SUMS 核对 | SHA256SUMS；ok=54 / mismatch=0 / missing=0 / list=[] | ok=54 / mismatch=0 / missing=0 / list=[] | 一致 |

#### 实例 3【zip】工程控制/_control_packs/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905.zip

| 指标 | digest 值 | 实测值 | 判定 |
|---|---|---|---|
| 文件数（zip 非目录条目） | 56 | 56（总条目 62，含 6 个目录条目） | 一致 |
| 字节量级（解压总字节） | 377.6 KB | 377.6 KB（386,677 B；zip 本体 296,398 B） | 一致 |
| zip sha256 | 77a4b03a622c63cc3bf9622351dc2363a0f35adef46ca79aea4e749b48fd6eff | 同左（逐字符相等） | 一致 |

（zip 内 SHA256SUMS 实测参考 54/54 ok，digest 未记，不计入比对；zip 顶层目录名与"zip 内 marker 含 00_READ_FIRST.md、START_PROMPT.txt"实测亦相符。）

## 4. 不一致项清单

**无。** 37 项规定口径的比对指标全部一致，无差异数值可列。

以下为不影响判定的一致性观察（非不一致项）：

- **O-1 文件数口径含复原件**：4 个 recovered_from_history 实例（CLI_hist、acr_hist、V5_hist、V8_hist）的文件数/字节在"含复原注入的 _PROVENANCE.json"口径下与 digest 精确相等（如 CLI_hist：含=93/112.6 KB，不含=92/112.1 KB），证明 digest 生成时按复原目录全量统计。对应 worktree/zip 实例（V5_wt 37 文件、V8_wt 56 文件）无此文件，差值恰为 1 文件 ≈ 0.4–0.5 KB，与两形态间 digest 记录差异自洽。
- **O-2 台账行数口径**：所有"行 N"（62/98/191）实测均为 csv 数据行数（不含表头；物理行数各 +1），digest 与实测口径统一。
- **O-3 SHA256SUMS mismatch 是"记录内"状态**：V5 两实例与 V7 实例的 02_TASK_LEDGER.csv / TASK_LEDGER.csv mismatch、V8_hist 三个 baseline CSV mismatch，digest 均已如实记录且 mismatch_list 逐文件相等；V8_wt 已收敛为 0 mismatch（digest 同步记录 ok=54），体现执行期台账更新未回写 sums 的真实历史形态，取证价值完好。
- **O-4 acr_wt 的 missing=1 实体**：SHA256SUMS.txt 列出 `CHECKLIST.md`，包内实际为小写 `checklist.md`（大小写漂移），digest 只记计数未记文件名，实测无矛盾。
- **O-5 CP0 双 zip 副本字节级一致**：reports/REAUDIT_V3/cp0_out 原件与 history_zip 复原件 sha256 完全相同（c79f50d7…），复现无损。

## 5. 总结计数

| 统计项 | 数值 |
|---|---|
| 抽中包数（层数） | 6 / 6 层 |
| 覆盖实例数 | 11（digest 记录的全部存形态实例，无缺失路径） |
| 比对指标总数 | **37** |
| 一致 | **37** |
| 不一致 | **0** |
| 需换候选次数 | 0（6 个抽中身份的 digest 均存在） |

结论：本次抽样的 6 个代际层包，其 digest 记录的实例清单与包本体实测在文件数、字节量级、台账行数与状态字面量分布、SHA256SUMS 核对（ok/mismatch/missing/list）、zip sha256 六类口径上**全部一致**，digest 可信；未发现任何指标性偏差。
