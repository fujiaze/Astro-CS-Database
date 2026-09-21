# RETIREMENT_LEDGER —— 旧世代打包/审计工具退役登记

- **任务**：RETIRE-001（`工程控制/PROJECT-GOVERNANCE-01/tasks/RETIRE-001.md`，层 L1）
- **执行**：2026-09-16（SubAgent 仅改工作区，**零 git 写**；由前台原子提交）
- **权威依据**：
  1. `ASTROCS_DESIGN.md §0`（权威链：**旧世代控制包产物不构成判据**）、`§12`（Alpha 前不含任何版本信息）；
  2. `ENGINEERING_SPEC.md §8`（每项检查必须能红能绿；**坏掉即红的门要退役或修好，不允许静默坏掉**）、`§7`（根白名单/落位）；
  3. `docs/ci/01_CHECKS.md` §1-§5（注册项语义与变更流程）；
  4. 负责人裁决：历史版本控制包全部作废；`artifacts/` 不归档、不保留（commit `b1290525`）；
     GAP-032（`TRACEABILITY-CODE` 退役）；GAP-033 附注（`registered_local_retention` 的 7 条悬空登记由 RETIRE-001 清理）。
- **复原锚点提交**（本任务退役前的最后提交）：`01754fab8618`（`01754fab8618313bc39a4da3014e78cd0ad4e2a3`）
- **复跑证据目录**：`run/PROJECT-GOVERNANCE-01/RETIRE-001/logs/`（gitignore，不入库）

---

## 0. 计数摘要（与 `docs/ci/01_CHECKS.md §2.2` 一致）

| 分类 | 计数 | 明细 |
|---|---|---|
| **本任务新增退役工具** | **4** | `eng/tools/assemble_audit.py`、`eng/tools/make_capsule.py`、`eng/tools/make_rev2_capsule.py`、`eng/tools/pack_audit_package.py`（**仅打包入口**） |
| 其中「部分退役」（函数保留为活动依赖） | 1 | `eng/tools/pack_audit_package.py`：`allowed()/denied()/EXCLUDE_EXT` 仍被 `CHK-SECRET-HYGIENE` 消费 |
| 已退役（本任务仅复核，**未改**） | 1 | `eng/tools/check_traceability.py`（2026-09-16 按 GAP-032 退役，见 §2.1 退役记录） |
| 经实证**不退役**（保留原样） | 1 | `eng/tools/quality/known_failures_baseline.py`（被 `CHK-KNOWN-FAILURES-BASELINE` 的 3 个 step 消费） |
| **注册项退役** | **0** | 4 个退役工具**均不在 `eng/ci/checks.json` 注册**（无 command / changed_paths 引用）⇒ 无需注册项退役；CI-001 独占窗口内 `eng/ci/checks.json` **一行未动** |
| 登记订正 | 2 处文件（3 项） | `eng/ci/INVENTORY_REPORT.md`（1 条读数）、`eng/ci/root_manifest.json`（7 条悬空登记）、`docs/ci/01_CHECKS.md`（新增 §2.2 工具层退役记录） |
| **删除文件** | **0** | 全部只加退役抬头，文件本体保留（任务卡硬约束 + AGENTS.md §5） |

---

## 1. 实测对照：`git grep -l 'artifacts/prerelease_v5' -- tools ci tests`

- 退役前（`logs/01_grep_before.txt`，10 个文件）：
  `eng/ci/checks.json`、`eng/ci/tests/test_impact_map.py`、`tests/api/test_cli_protocol.py`、`tests/quality/test_docchk002_mutation.py`、
  `eng/tools/assemble_audit.py`、`eng/tools/check_traceability.py`、`eng/tools/make_capsule.py`、`eng/tools/make_rev2_capsule.py`、
  `eng/tools/pack_audit_package.py`、`eng/tools/quality/known_failures_baseline.py`
- 退役后（`logs/03_grep_after.txt`，11 个文件）：`diff` 仅多出 `eng/ci/INVENTORY_REPORT.md`（本任务的订正注记）。

**结论：受影响工具集合前后完全一致（10 → 10，无增无减）**；新增文件不是工具。
退役抬头本身仍会命中该字符串 —— 那是**退役说明的文本**（与既有先例 `eng/tools/check_traceability.py` 一致），
**不是活动输入**：4 个退役工具的 `main()` 已不再读取/写出任何该路径下的内容（见 §3 实跑证据）。

---

## 2. 逐条判定（先实证：输入是否存在 / 是否仍被消费 / 是否有活动替代）

| # | 文件 | 引用形态 | 输入/输出是否仍存在 | 是否仍被 CI/脚本消费 | 活动替代 | 判定 |
|---|---|---|---|---|---|---|
| 1 | `eng/tools/assemble_audit.py` | 输入常量 `CP`/`TAB`，输出 `SRC`/`OUT` | **否**：`工程控制/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828/`、`artifacts/prerelease_v5/tables`、`audit_src`、`AUDIT_REVIEW` 全不存在 | 否（`git grep assemble_audit` 仅自引用） | 无（发布打包门为 `CHK-PACKAGE`） | **退役** |
| 2 | `eng/tools/make_capsule.py` | 输出目录 `artifacts/prerelease_v5/capsules` | **否**（该树随 `artifacts/` 删除） | 仅 `tests/cli/test_iso_acr_gpu_isolation.py::test_04` **静态扫描其路径**（扫描体为空操作）；不在注册表 | 无（逐提交复核由 git 历史承担） | **退役**（文件保留） |
| 3 | `eng/tools/make_rev2_capsule.py` | 输入三表（`isfile` **静默跳过**）+ 输出目录 | **否**（tables/ 与 capsules/ 均不存在） | 否 | 无 | **退役** |
| 4 | `eng/tools/pack_audit_package.py` | 输出 `OUT`、白名单前缀、排除前缀 | **否**（输出目录随 `artifacts/` 删除） | **是**：`eng/tools/quality/check_secret_hygiene.py:53`（`DEFAULT_PACKER`）→ 注册项 `CHK-SECRET-HYGIENE`；`tests/quality/test_secret_hygiene.py` | 打包入口的替代 = `CHK-PACKAGE`；白名单函数**自身即活动替代** | **部分退役**：入口退役，白名单/排除函数保留 |
| 5 | `eng/tools/check_traceability.py` | 退役抬头 + fail-closed 文案 | 否（同一个已删表） | 否（`TRACEABILITY-CODE` 已退役，§2.1） | 有：显式 CSV 入参仍按 R1-R7 全量校验 | 已退役（**本任务未改**，仅实跑复核 rc=2） |
| 6 | `eng/tools/quality/known_failures_baseline.py` | 历史 finding `F-028` 的 repro/evidence **记录字符串** | 该字符串只是历史记录文本 | **是**：`CHK-KNOWN-FAILURES-BASELINE` 的 3 个 step（仍写 `artifacts/KNOWN_FAILURES_BASELINE.json`） | — | **保留（不退役）** |
| 7 | `eng/ci/checks.json` | `dirty_ignore_prefixes` 2 处（`checks[12]`=UT-BACKEND 顶层与其 step） | 路径已被 ISA-* 旁路重建（**非原语义**） | 是（注册表自身） | — | **本任务不动**（CI-001 独占写窗口；登记为后续处置，见 §5） |
| 8 | `eng/ci/tests/test_impact_map.py` | 注释（退役依据说明） | — | 是（`CI-BINDING-TESTS` step） | — | 保留（注释即登记） |
| 9 | `tests/api/test_cli_protocol.py` | 注释（断链修复说明）；夹具已重接线到 tracked `docs/api/CLI_PROTOCOL_V1.md` | — | 是（UT-API） | 有 | 保留（TEST-CLI-SYNC 域） |
| 10 | `tests/quality/test_docchk002_mutation.py` | 注释；基线改为 tracked `tests/quality/fixtures/docchk002_claims_fixture.csv` | — | 是（UT-QUALITY） | 有 | 保留（非本任务文件域） |

---

## 3. 退役项明细（工具 → 退役理由 → 复原方法）

统一处置形态（全部 4 项一致）：
1. **加退役抬头**（文件 docstring 内：依据 + 日期 + 复原命令 + 退役后行为 + 登记位置）；
2. **入口改「显式失败」**：原实现整体保留并更名为 `legacy_main()`，新增 `main()` 打印 `<TOOL>_RETIRED` 说明并 **exit 2**（fail-closed，不伪装绿、不 traceback 崩）；
3. **文件本体不删**，可复跑性由 `git show` 复原命令保证。

### 3.1 `eng/tools/assemble_audit.py`
- **退役理由**：唯一输入源 `工程控制/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828/` 已不存在，
  `artifacts/prerelease_v5/tables/` 随 `artifacts/` 整体删除（`b1290525`）；工具语义是「V5 控制包审核包装配」，
  属设计 §0 已废止世代。
- **退役前实测**（`logs/10_before_assemble_audit.out`）：`rc=1`，未捕获
  `FileNotFoundError: .../工程控制/RELEASE_V5/.../00_READ_FIRST.md`，并**残留空目录** `artifacts/prerelease_v5/audit_src/`
  （探针残留已由本任务清理，`artifacts/` 现状仅剩 ISA-001/002/003 与本任务外的 `eng/ci/`、`KNOWN_FAILURES_BASELINE.json`）。
- **退役后实测**（`logs/20_after_assemble_audit.out`）：`rc=2`，输出 `ASSEMBLE_AUDIT_RETIRED: ...`，**无 traceback**。
- **复原方法**：`git show 01754fab8618:eng/tools/assemble_audit.py > eng/tools/assemble_audit.py`；复原输入见 §5 注（V5 控制包不在库）。

### 3.2 `eng/tools/make_capsule.py`
- **退役理由**：输出目录 `artifacts/prerelease_v5/capsules/` 随 `artifacts/` 删除；胶囊是旧世代控制包的交付形态，
  负责人裁决「历史版本控制包全部作废」+ 设计 §12「Alpha 前不含任何版本信息」⇒ 口径废止。
- **退役前实测**：无参 `logs/13_before_make_capsule_noargs.out` → `rc=1`、未捕获 `IndexError: list index out of range`；
  带参 `logs/14_before_make_capsule_args.out` → `rc=0` 且**静默把 zip 写回已退役路径**
  （`artifacts/prerelease_v5/capsules/RETIRE-001-PROBE_01754fab8618.zip`，3771 B；探针产物已清理）。
- **退役后实测**：无参/带参均为 `rc=2` + `MAKE_CAPSULE_RETIRED`（`logs/20_after_make_capsule.out`、`logs/21_after_make_capsule_args.out`）。
- **保留原因**：`tests/cli/test_iso_acr_gpu_isolation.py::test_04` 仍按路径静态扫描本文件，删除会改变测试输入面。
- **复原方法**：`git show 01754fab8618:eng/tools/make_capsule.py > eng/tools/make_capsule.py`。

### 3.3 `eng/tools/make_rev2_capsule.py`
- **退役理由**：REV-002 是旧世代控制包任务；输入三表被第 119-121 行 `isfile` **静默跳过**（不报错、不留痕），
  输出目录已删且脚本无 `makedirs`。
- **退役前实测**（`logs/12_before_make_rev2_capsule.out`）：`rc=1`，未捕获
  `FileNotFoundError: .../artifacts/prerelease_v5/capsules/REV-002_01754fab8618.zip`（**静默输入 + 崩溃输出**双重缺陷）。
- **退役后实测**（`logs/20_after_make_rev2_capsule.out`）：`rc=2` + `MAKE_REV2_CAPSULE_RETIRED`。
- **复原方法**：`git show 01754fab8618:eng/tools/make_rev2_capsule.py > eng/tools/make_rev2_capsule.py`。

### 3.4 `eng/tools/pack_audit_package.py`（**部分退役**）
- **退役理由（仅打包入口）**：产物落已退役 `artifacts/prerelease_v5/AUDIT_PACKAGE_<c12>.zip`，且包内含
  `VERSION`/`CHANGELOG` 口径，与设计 §12（Alpha 前不含版本信息）冲突。
- **退役前实测**（`logs/11_before_pack_audit_package.out`、`logs/15_before_pack_probe_listing.txt`）：目录被旁路重建时
  `rc=0` **静默产出 11.2 MB / 2709 条目**（`code` 2002 + `evidence` 691），且
  `zip_bytes=10.67 MiB` 超过自身 `<10MB target` 仍 `exit 0`；目录不存在时会 `FileNotFoundError` traceback。
  ⇒ 两条路径都属「静默坏掉」。
- **不退役部分（活动依赖，必须保留）**：`allowed()`/`denied()`/`EXCLUDE_EXT`/`DENY_PATHS`/`DENY_NAME_RE`
  是审计包收录白名单与凭据排除保证的**唯一真源**，被活动门 `CHK-SECRET-HYGIENE` 直接 import
  （`check_secret_hygiene.py` 的 `pack_files()` 调 `allowed()`），并由 `tests/quality/test_secret_hygiene.py` 覆盖。
- **退役后实测**：`rc=2` + `PACK_AUDIT_PACKAGE_RETIRED`（`logs/20_after_pack_audit_package.out`，无 traceback）；
  作为模块 import 的语义回归 `logs/22_after_packer_module.out` = **VERDICT: PASS**（5 例 `allowed()` + `denied()` 全部与原语义一致）；
  `logs/40_active_consumer_regression.out`：`check_secret_hygiene.py --scope pack` **rc=0**、`test_secret_hygiene.py` **22/22 OK**。
- **附带修复**：为使退役入口可用，补 `import sys`（原文件未导入；不加则退役入口自身抛 `NameError`）。
- **复原方法**：`git show 01754fab8618:eng/tools/pack_audit_package.py > eng/tools/pack_audit_package.py`。

---

## 4. 登记不一致的订正

### 4.1 `eng/ci/INVENTORY_REPORT.md`（`TRACEABILITY-CODE(PASS)` 陈旧读数）
- **问题**：§2「追溯类」表仍把 `TRACEABILITY-CODE` 记为 `PASS`，而该注册项已于 2026-09-16 按 GAP-032 **退役**
  （`docs/ci/01_CHECKS.md §2.1`、`eng/ci/checks.json` 已移除该注册项），属「陈旧状态冒充活动状态」（ENGINEERING_SPEC §8）。
- **订正**：本报告是 base SHA `b4f923cc` 的**当时快照**，**不回改历史读数**；在报告头部加「订正记录（2026-09-16, RETIRE-001）」，
  并在原单元格就地加注 `~~PASS~~ **已于 2026-09-16 退役**`（含 `§2.1` 指针）。
- **复原方法**：`git show 01754fab8618:eng/ci/INVENTORY_REPORT.md > eng/ci/INVENTORY_REPORT.md`。

### 4.2 `eng/ci/root_manifest.json`（`registered_local_retention` 7 条悬空登记）
- **问题**：`registered_local_retention` 仍列 7 条**物理上已不存在**的根条目：`CHANGELOG.md`、`REVIEW.md`、
  `ASTROCS_PROJECT_CONSTITUTION.md`、`AstroCS_ENGINEERING_CONSTRAINTS.md`、`evidence`、`CS`、`worktrees`。
  登记悬空即陈旧声明（ENGINEERING_SPEC §8）。依据 GAP-033 附注：「`registered_local_retention` 的 7 条悬空登记由 RETIRE-001 清理」。
- **订正**：删除上述 7 条（台账**只减不增**方向清理，**非白名单放宽**）；条目数 22 → 15；其余 15 条逐条实测仍存在于根目录
  （`VERSION`/`FATDUCK_ACCESS.md`/`VISUAL_CHECK_README.md`/`HANDOVER.md`/`问题扫描`/`cli`/`runtime`/`providers`/`modules`/`AstroCS.wiki`/`p8..p15a-files.patch`）；
  并在 `notes.retention_cleanup` 留下清理依据与复原坐标（`json.load` 校验通过）。
- **复原方法**：
  - 前四条（随 commit `01db973b` ROOT-007 删除）：`git show 01db973b^:CHANGELOG.md > CHANGELOG.md`（其余同理；
    `REVIEW.md`/`ASTROCS_PROJECT_CONSTITUTION.md`/`AstroCS_ENGINEERING_CONSTRAINTS.md` 均已实测可取回）；
  - `evidence/`（树，`01db973b^` 下 624 条 tracked 路径）：`git archive 01db973b^ evidence | tar -x`；
  - `CS/`、`worktrees/`：**从未入库的 0 字节空目录**（`git log -- CS`/`-- worktrees` 无输出）⇒ **无 git 坐标可复原**，如需重建仅能 `mkdir`（无内容）；
  - 整个文件：`git show 01754fab8618:eng/ci/root_manifest.json > eng/ci/root_manifest.json`。

### 4.3 `docs/ci/01_CHECKS.md`（新增 §2.2 工具层退役记录）
- **订正**：在 §2.1（注册项退役记录）之后新增 **§2.2 工具层退役记录**：4 条退役工具 ×（依据/处置/可复跑性）+ 实测复核 +
  「经实证不退役」清单 + 指向本台账。
- **关于 §5「执行器」**：**本任务未修改 §5**。`docs/ci/01_CHECKS.md §5` 与 `AGENTS.md:36` 属**新权威文档集**，
  其 `python3 eng/ci/run_checks.py …` 是**规范要求**；仓库当前 tracked 的执行器是 `eng/ci/run.py`，二者差异属 **GAP-016**
  登记的「规范要求、实现缺失」缺口，由 **CI-001** 交付 `eng/ci/run_checks.py` 并一并处置 → **不改规范，登记为实现缺口**。
- **复原方法**：`git show 01754fab8618:docs/ci/01_CHECKS.md > docs/ci/01_CHECKS.md`。

---

## 5. 本任务未处置（越界 / 待后续裁决）项

| 项 | 位置 | 为何不动 | 建议 |
|---|---|---|---|
| `dirty_ignore_prefixes: ["artifacts/prerelease_v5/"]`（2 处） | `eng/ci/checks.json` `checks[12]`(UT-BACKEND) 顶层与 step | **CI-001 独占收敛窗口**（上级明令「一行都不要碰」）；且其为容错白名单前缀，不是命令/输入，不会静默坏 | CI-001 窗口结束后随 UT-BACKEND 收敛一并清理 |
| 三处注释引用 `artifacts/prerelease_v5` | `tests/api/test_cli_protocol.py`、`tests/quality/test_docchk002_mutation.py`、`eng/ci/tests/test_impact_map.py` | 均为**已完成的断链修复说明**（夹具已重接线到 tracked 文件），且 `tests/**` 属 TEST-CLI-SYNC 活动域 | 无需动作；作为历史说明保留 |
| 其它提到 `工程控制/RELEASE_V5` 的工具 | `eng/tools/make_linux_release.py:88`（许可证来源文案）、`eng/tools/validate_cpu_profile.py:88`（注释） | 不在本任务 grep 判据内（不引用 `artifacts/prerelease_v5`），且 `make_linux_release.py` 属活动打包链路 | 另立任务按同一实证口径判定 |
| V5 控制包本体 | `工程控制/RELEASE_V5/**`（已整体不在 tracked 工作区） | 负责人裁决「历史版本控制包全部作废」⇒ 4 个退役工具**不具备就地复跑条件**（复原命令仅恢复代码，不恢复输入） | 如需重跑旧世代装配线，须同时从旧控制包归档取回输入 |

---

## 6. 复跑证据索引（`run/PROJECT-GOVERNANCE-01/RETIRE-001/logs/`）

| 日志 | 内容 |
|---|---|
| `01_grep_before.txt` / `03_grep_after.txt` | `git grep -l 'artifacts/prerelease_v5' -- tools ci tests` 前后对照 |
| `02_git_status_before.txt` | 退役前工作区签名（含并发的 ARCH-001/TEST-CLI-SYNC 改动，非本任务） |
| `10..15_before_*.out` / `15_before_pack_probe_listing.txt` | 四个工具的「退役前实跑」原始输出（含 traceback / 静默产物结构） |
| `20_after_*.out`、`21_after_make_capsule_args.out` | 四个工具的「退役后实跑」（明确退役文案 + rc=2，无 traceback） |
| `22_after_packer_module.out` | `allowed()/denied()` import 语义回归（VERDICT: PASS） |
| `30_secret_hygiene_pack.json`、`40_active_consumer_regression.out` | `CHK-SECRET-HYGIENE` 实跑 rc=0；`test_secret_hygiene.py` 22/22 OK；`eng/ci/validate_registry.py --strict` `error_count=0` |
| `41_affected_checks.out` | `CHK-ROOT-CLEAN`、`CHK-SECRET-HYGIENE(tracked)`、`CON-COMMENTS`、`CON-DOC-SYMBOLS`、`DOC-INDEX --strict`、`UT-QUALITY` 受影响面复跑 |
| `自证摘要.md`（上一级目录） | 本任务自证摘要 |

## 7. 验收门自查（对照任务卡 §验收门）

- [x] 受影响工具清单完整（`git grep` 前后对照，工具集合 10 → 10 不变）
- [x] 每个退役项：退役抬头 + 依据 + 复原命令，且**文件仍在**（`git status` 无删除、无 `git rm`）
- [x] `eng/ci/validate_registry.py --registry eng/ci/checks.json --strict` → `error_count=0`（**只读**，未改 `eng/ci/checks.json`）
- [x] 退役项实跑输出为**明确退役文案 + rc=2**（无未捕获 traceback）
- [x] 本台账覆盖全部退役项（4 = `docs/ci/01_CHECKS.md §2.2` 表行数）
- [x] 未越界改动：仅 `eng/tools/{assemble_audit,make_capsule,make_rev2_capsule,pack_audit_package}.py`、`eng/ci/INVENTORY_REPORT.md`、
      `eng/ci/root_manifest.json`、`docs/ci/01_CHECKS.md`、本报告（+ `run/` 日志）；未触 `eng/ci/checks.json`、`lib/**`、`cli/**`、
      `docs/science/**`、`docs/algorithms/**`、`docs/plugins/**`、`tests/**`
