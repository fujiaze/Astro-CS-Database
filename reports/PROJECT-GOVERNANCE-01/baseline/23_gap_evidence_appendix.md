
---

## 5. BASE-001 证据补全（2026-09-16 实测；**只补证据，不改判定口径、不删条目**）

原始输出：`reports/PROJECT-GOVERNANCE-01/baseline/01..22_*` + `SHA256SUMS`；执行日志：`run/PROJECT-GOVERNANCE-01/BASE-001/logs/`；复跑脚本：`run/PROJECT-GOVERNANCE-01/BASE-001/evidence-scan{,2,3,4}.sh`。

### 5.1 基线三 SHA 不一致（新增 U-08）

| 引用 | SHA |
|---|---|
| `HEAD` | `ecf6ad6fe55102c08a156564cc0dd30cea3a7cdc` |
| `main` | `ecf6ad6fe55102c08a156564cc0dd30cea3a7cdc` |
| `origin/main` | `f96dff61e6ae9cc052a925713f9864f5d6bac3bb` |

- 本地 main 领先 origin/main **1 个提交**：`ecf6ad6f docs(governance): 新增根目录清洁任务 ROOT-001/002/003（29 任务）`；origin/main 无领先提交。
- 本文件抬头声明基线 `a861d8f6…`、派发提示词声明 `f96dff61`，**均 ≠ 当前 HEAD**；三者互不相同 → 新增 `U-08`（见 §5.4）。
- 证据：`reports/PROJECT-GOVERNANCE-01/baseline/{01,05}_*` 与 `19_gap_evidence.txt` 末节。

### 5.2 §0 三项红灯独立复跑（结论与编制时一致）

| 检查项 | 命令 | 编制时 | BASE-001 实测 | 证据 |
|---|---|---|---|---|
| AGENTS-GOV | `python3 tools/check_agents_gov.py` | rc=1 | **rc=1** | `17_gap_sec0_redlights.txt` |
| ENG-CONSTRAINTS | `python3 tools/doccheck/check_engineering_constraints.py` | rc=1 | **rc=1** | 同上 |
| DOC-INDEX | `python3 tools/doccheck/check_doc_index.py --strict` | rc=1 | **rc=1** | 同上 |

`§2` 复核表 7 条亦逐条复跑，结果与编制时一致；新增实测：`ls ci/run_checks.py` **rc=2（文件不存在）**，`python3 ci/run_checks.py --help` **rc=2** → GAP-016「入口名不符」证据升级为文件级（`18_gap_sec2_gates.txt`）。

### 5.3 逐条 GAP 证据（含实测数字修正）

| GAP | BASE-001 实测证据（可复跑） | 与编制时差异 |
|---|---|---|
| GAP-001 | 5 项 tracked 且不在 §7 白名单：`ASTROCS_PROJECT_CONSTITUTION.md`(39903B)、`AstroCS_ENGINEERING_CONSTRAINTS.md`(10061B)、`REVIEW.md`(20936B)、`HANDOVER.md`(9216B)、`VERSION`(15B)；另 `CHANGELOG.md`(16478B)、`FATDUCK_ACCESS.md`(2153B)、`VISUAL_CHECK_README.md`(2271B) | 一致 |
| GAP-002 | `README.md:12`、`REVIEW.md:118`、`HANDOVER.md:7`、`memory.md:10` 引旧宪章；`HANDOVER.md:22` 引 `工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909`（`exists=NO`）；`memory.md:36/48/81/87/112` 自报旧基线 `da3c4b4a` | 一致（补行号） |
| GAP-003 | `lib/algorithms`/`lib/infrastructure`/`config` 均不存在；`lib/` 顶层 **31** 项；根 `cli/`(22 tracked)、`runtime/`(32)、`providers/`(10)、`modules/`(16) 在位 | 一致 |
| GAP-004 | 非注释 `add_library` **29** 条 = SHARED **3** + STATIC **25** + `INTERFACE` **1**（`astrocs_contracts`，CMakeLists.txt:95）；三 Session 仍 STATIC（:573/:582/:588） | **数字修正**：编制时写「25 条 / STATIC 22」与实际口径不符（是否含 INTERFACE 与注释行）；结论（整阶段 Session 仍 STATIC、SHARED 仅 3）不变 |
| GAP-005 | kRules 唯一命令路径 **27**；`phase1/2/3 run` 注册于 `cli/parser.cpp:57/62/68`；`normalize`/`mosaic`/`export` 在 `cli/{parser,commands,main}.cpp` 排除 `export-mode` 后 **零命中**（全部 6 处命中均为 `--export-mode` 旗标子串，原文见 `21_gap_evidence_scan3.txt §S1`）；`--template` 注册数 **0**；`cmd_stub` 兜底 `commands.cpp:202/2627` | 一致（补命中原文） |
| GAP-006 | `config` 不存在；4 层内无 `defaults.json`/`filters.json`；`contracts/` 无 `*phase_config*`/`*template*` | 一致 |
| GAP-007 | `contracts/` 4 子域：`config data proposals schemas`（含 `schemas/v6`）；`docs/contracts/` 并存 `DATA_SEMANTICS.md`、`DATA_ARTIFACTS.md`、`INDEX.yaml`、`v6/` | 一致 |
| GAP-008 | `module.yaml` **23** 份（`lib/` 21 + `modules/conformance/{echo,noop}` 2）；`docs/modules/registry/*.md` **26** 份；id 词汇：module.yaml `id: MOD-astrocs-phase1-calibration` + `module_id: astrocs.p1.calibration` + 端口 `p1.frames`；registry 文件名 `astrocs.phase1.*`、正文引用 `astrocs.p1.*`/`p2.*`/`p3.*` 端口词 | 一致（补双向词汇实样，见 `22_gap_evidence_scan4.txt §T1/§T2`） |
| GAP-009 | `p1_session` 四段 `io_read→calibrate→cosmetic→io_write`（阶段字符串计数 8/9/16/10）；`lib/astro_image_io/` 内 `FRAMESNR` **0 命中**；`lib/snr_estimator` tracked **24** 文件；根 CMakeLists 注释 :210/:239/:492-498 与 `git ls-files` 事实冲突；`lib/core/src/module_adapters.cpp:2620-2621,2795-2796,2826` 自述「不再输出整帧 SNR 标量」 | **证据细化**：`lib/astro_image_io` 内 `sparse` 命中 **72** 处，全部属 UPM sparse 模型（`aio_upm.cpp`）与 HiPS occupancy `SPARSE_LIST` 模式（`hiss_writer.cpp`/`hiss_reader.cpp`），**不是** §3.4 的帧级稀疏层 → 「帧级 SNR 未入文件头、帧级稀疏层不存在」判定加强而非削弱 |
| GAP-010 | 三权重 token 在 `lib/phase2_int/v6/src/phase2_integrate.cpp:193-195,208-209`；`ivar=1/variance` 在 :516-518；`p2_session` 有 `s->stage(` 调用 **18** 次、stage 名 **4** 个（coverage/persist/sample/upm_build） | **数字修正**：编制时写「仅 1 处 stage coverage」；仍为单会话组织，判定不变 |
| GAP-011 | `p3_projection_registry_table` `*count = 4`（`p3_projection.cpp:280`）、边界 `if (i < 0 \|\| i >= 4)`（:294/:300）；`p3_proj_v6.h:52-55` `ProjectionId` 仅 kTAN/kSIN/kCAR/kAIT；`STG/MOL/CEA/ZEA` 在 `lib/phase3_proj` 内 **5 处命中全在注释/文档**（module.yaml:31、README.md:28、memory.md:21/101/155），零实现 | 一致；**附带宽差**：module.yaml/memory.md 自述「TAN/SIN/**ZEA**/CAR/AIT」与实测四投影（TAN/SIN/CAR/AIT）不一致，且 ZEA 无实现 |
| GAP-012 | `lib/phase3_session`/`lib/phase3_proj`/`lib/phase3_fits` 均**无自有 CMakeLists**；根仅 `add_subdirectory(lib/phase3_rsmp)`（CMakeLists.txt:697，`lib/phase3_rsmp` 有 CMakeLists） | 一致 |
| GAP-013 | 并存：`lib/astro_image_io`（`aio_fits.cpp`/`hiss_writer.cpp` 在位、**无 CMakeLists**）、`lib/io/src/io_adapter.cpp`（tracked；注意路径在 `src/` 下）、`lib/hips`（有 CMakeLists）、`lib/hips_p2`、`lib/healpix_db`、`runtime/io`（`hips_output_store.py`/`fits_verify.py`/`fits_core.c`/`hips_core.c`）、`runtime/artifact_store` | 一致（补精确路径） |
| GAP-014 | `runtime/pipeline`、`runtime/monitoring`（monitor.py/runner.py/windows_pdh_etw.py/linux_procfs.py/trace_feed.py）、`runtime/core`、`cli/runtime_client.{h,cpp}`、`lib/core/src/scheduler.cpp` 全部在位；`lib/orchestrator` **无 CMakeLists** | 一致 |
| GAP-015 | `providers/` 仅 `cpu`；`lib/backend_host` **27** 文件；`ci/resource_monitor.py` 头注释自述为 `tools/monitoring/run_monitored.py` 的桥接 shim（单一事实源在 tools/monitoring）；`ci/checks.json` 的 `ENG-CONSTRAINTS.changed_paths` 含 `AstroCS_ENGINEERING_CONSTRAINTS.md`、`AGENTS-GOV.changed_paths` 含 `memory.md` | 一致（补 shim 原文） |
| GAP-016 | `ci/run_checks.py` **不存在**（rc=2）；`ci/run.py` 在位；`ci/checks.json` `schema_version=1`、`checks` **145** 项、`CHK-*` **0** 项；`VERSION-CONSISTENCY.command = [python3, ci/check_version.py, --expected, 0.11.0-alpha.2]` | 一致 |
| GAP-017 | `VERSION=0.11.0-alpha.2`；`CMakeLists.txt:14 project(astrocs VERSION 0.11.0 …)`、:58 生成 `ASTROCS_VERSION_STRING`、:797 打印；`cli/commands.cpp` 5 处使用（:174/:344/:366/:419/:681）；`module.yaml` `module_version: 0.11.0-alpha.2` | 一致（补行号） |
| GAP-018 | `packaging/astrocs.product.json`：`product_version=0.11.0-alpha.1`、`source_commit=9f6b72b5eec1f585506e57fd4fb55c15dda1bbb3`、`units=10`、status ∈ {IMPLEMENTED, SKELETON}；`packaging/install-tree.contract.json`：`target_version=0.11.0-alpha.1`、root_layout 含「Linux 技术预览」；`add_executable(astrocs` 于 CMakeLists.txt:612 | **证据修正**：`acsd` 在 packaging/cmake/docs/ci 内 **2 处命中，均为文献 URL**（`docs/science/CALIBRATION.md:129`、`docs/references/SCIENTIFIC_REFERENCES.md:11` 的 “HST ACS Data Handbook”），**不是产品名** → 「无 ACSD Cli 命名」判定成立（原写“零命中”属检索口径差异，非实质冲突） |
| GAP-019 | `工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909` 与 `工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915` 均 `exists=NO`；`engineering/` 存在且 **0 条目（空）**；仓内引用计数 **337**（排除 `run/`、`build/`） | **数字修正**：编制时写「30+ 处」（方向一致，实测 337） |
| GAP-020 | `reports/v6/release-review/` 6 份 md + `tools/`；`00_导读.md:34-35` 自述 §14.5 六步 `NOT_MET ×4 / AWAITING / NOT_MET`、`NOT_READY（NOT_RELEASED）` | 一致 |
| GAP-021 | `engineering/` 空；tracked 但不在 §7 白名单：`CHANGELOG.md`、`FATDUCK_ACCESS.md`、`VISUAL_CHECK_README.md`、`cli/`、`runtime/`、`providers/`、`modules/`、`设计大纲/`(344 tracked)、`问题扫描/`(1011 tracked)、`run/`(仅 `run/.gitkeep`)、`工程控制/`(36 tracked，§7 白名单内) | 一致（补 tracked 文件数） |
| GAP-022 | 顶层 **133** 条目 = tracked 顶层 **47** + untracked 顶层 **86**；`astrocs_run_*.json` **56** 个（合计 36109 B，mtime 2026-09-12 18:49 ~ 2026-09-14 05:28）；`p*-files.patch` **5** 个（104929 B）；`build/` 6.0G；`run/` 102G；`/workspace` 503G/293G 已用 | **数字修正**：编制时写 tracked 46 / untracked 87 / `astrocs_run_*.json` 62 个。tracked 顶层 +1 因 `工程控制/` 已随 `ecf6ad6f` 入库；`astrocs_run_*.json` 由 62 → 56（有 6 个已在本包编制后被移走/删除，本任务未删任何根条目） |

> 全部数字口径：`tracked 顶层 = git -c core.quotepath=false ls-tree HEAD --name-only`；`顶层 untracked = ls -A - tracked 顶层`；`astrocs_run_*.json` 用 `ls -1 astrocs_run_*.json | wc -l`。避免用 `git status --porcelain` 的 `??` 行数当顶层计数（它按目录折叠且含子路径）。

### 5.4 UNRESOLVED 增补

| ID | 无法判定的内容 | 缺什么才能判定 | 归属任务 |
|---|---|---|---|
| U-08 | 本次治理应以哪个 SHA 为基线：`HEAD=main=ecf6ad6f` 与 `origin/main=f96dff61` 不一致（本地领先 1 个提交），且与本文件抬头 `a861d8f6` 亦不同 | 负责人确认「基线 = 本地 HEAD 并先 push」还是「基线 = origin/main 并先同步」；在确认前任何「基线一致」声明不成立 | GOV-001 / 前台 |
| U-07（补证据，判定仍待裁决） | `run/` 应 gitignore 还是 tracked | 实测 `git ls-files run` **仅 `run/.gitkeep`**，`.gitignore` 有 `run/*` + `!run/.gitkeep`；§7 白名单本身列有 `run/（gitignore：临时产物/日志）`。故冲突点仅为「目录存在 vs 被 gitignore」，证据已足；仍需负责人裁决是否保留 `.gitkeep` 占位口径 | GOV-001 |

### 5.5 本轮新发现（追加为 GAP-023，不修改既有条目）

**GAP-023　缺口：部分散落根条目未被 `.gitignore` 覆盖，长期污染 `git status`**

- **权威依据**：ENGINEERING_SPEC §7（禁止散落根目录）、§8（每项检查能红能绿）；ROOT-002 步骤 3。
- **证据（`git check-ignore -v` 实测，见 `22_gap_evidence_scan4.txt §T7`）**：以下顶层条目 **ignored=NO** 且 untracked，会长期显示在 `git status`：`run_context.json`、`p8-files.patch`、`p9-files.patch`、`p10-files.patch`、`p11-files.patch`、`p15a-files.patch`、`astrocs_p1sess_neg`、`astrocs_p1sess_perf`、`astrocs_p1sess_props`、`astrocs_p1sess_test`、`worktrees/`、`CS/`、`Database/`、`engineering/`（空目录，git 不跟踪空目录故无影响）、`run/`（目录本身）。
- **已覆盖对照**：`build/`(`.gitignore:22`)、`out/`(121)、`logs/`(18)、`Testing/`(120)、`.pytest_cache/`(91)、`graph/`(130)、`AstroCS.wiki/`(122)、`astrocs_run_*.json`(119)、`alloc_report.json`(128)、`alloc_samples.csv`(129)、`resource_samples.csv`(123)、`resource_summary.json`(124)、`worker_balance.csv`(125)、`BASS DR3/`(7)、`GaiaDR3/`(3)、`GaiaDR3SP/`(4) —— 均已 ignored。
- **治理任务**：ROOT-002（补 `.gitignore` + 机器门）。
