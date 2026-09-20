# 变更 claim：DOC-205-ANCHOR-SYMBOL-001 — 悬空符号/行锚收口 + 权威链改述（任务 B/C）

- 控制包：`工程控制/RELEASE-03` / 任务 **DOC-205**（任务 B、任务 C）
- 日期：2026-09-20
- 状态：**已落地**；`CHK-DANGLING` / `CHK-SCI-REF(DOC-LINE-ANCHORS)` / `CHK-CONTRACT-TEST(CON-FULL-INTEGRATION)` 三条**已 rc=0**
- 影响类：符号命名空间登记 + 行锚订正 + 文档口径改述（**无公式/阈值/容差改动，无 waiver，未放宽任何检查器**）
- 依据（最高权威）：`ASTROCS_DESIGN.md` §0.1（只有一份权威链，下级文档不得自封权威）、§9.73 A44（权重模式作废）
- 依据（锚点规范）：`docs/algorithms/anchors/ANCHOR_CONTRACT.md` §2（锚语义）、§4.1（仅两类可豁免）、§5（**删除/改名被锚定的文件 ⇒ 同一提交更新文档锚**）
- 依据（符号规范）：`docs/architecture/DOC_SYMBOL_NAMESPACES.md` + `tools/quality/contracts/check_doc_symbols.py`（DOC-SYMBOL-REGISTRY-EVIDENCE / -STALE 两条 fail-closed 判据）
- 依据（流程）：`ENGINEERING_SPEC.md` §3；`AGENTS.md` §8

## 1 悬空符号修复表（任务 B1）

| # | 符号/引用 | 原状（实测红灯） | 处置 | 核对方式 |
|---|---|---|---|---|
| 1 | `ASTROCS_WEIGHT_MODE`（`docs/contracts/v6/W6_SCHEMA_INTEGRATION.md:61`） | 该键已按 §9.73 A44 从库头摘除；v6 设计档案仍以现行键名口吻引用 | **留痕作废**（不静默删）：追加「（**已按 §9.73 A44 作废**：ASTROCS_WEIGHT_MODE 键已从库头摘除、「权重模式 / 权重档位」概念不存在；本行只作 v6 **设计档案**的迁移留痕，**不得**据此定义生产键名/枚举/配置项）」 | `python3 ci/check_no_weight_mode.py` → `CHK-NO-WEIGHT-MODE_PASS: files=346 lines=57882 无未留痕命中` rc=0 |
| 2 | 同上（`docs/contracts/v6/data/10_migration_and_open_items.md:15`） | 同上 | 同上（逐字同款留痕） | 同上 |
| 3 | 同上（`docs/contracts/v6/data/07_provenance.md:83`） | 同上 | 同上 | 同上 |
| 4 | `p2_write_descriptor`（`docs/modules/registry/astrocs.phase2.write.md:63`） | `DOC-BAD-SYMBOL`：API-like token 不在 API inventory / 公开头 / 文档 stem / 术语表 | **登记命名空间**（非豁免）：`docs/architecture/doc_symbol_namespaces.json` 增 `{"symbol":"p2_write_descriptor","namespace":"registry_descriptor","evidence":"lib/infrastructure/scheduler/src/module_adapters.cpp:1040-1057"}`；证据行**逐字含该 token**（`static const module_descriptor_t p2_write_descriptor`）⇒ EVIDENCE/STALE 两条 fail-closed 判据同时满足 | `python3 tools/quality/contracts/check_doc_symbols.py` → `status PASS`（0 findings） |
| 5 | 页锚 `module_adapters.cpp:739-756` / `:677-694`（同页 `:63-64`、`lib/algorithms/coverage/hips_p2/README.md:99`、`module.yaml:33`） | 行锚随 ARCH-001 迁移漂移 | 订正为 `:1040-1057`（**DOC-202 已完成**，本包复核确认） | `grep -n "p2_write_descriptor" lib/infrastructure/scheduler/src/module_adapters.cpp` → `:1040` |

## 2 行锚修复表（任务 B2，`DOC-LINE-ANCHORS`）

| # | 锚（文档:行） | 旧锚 → 新锚 | 核对方式 |
|---|---|---|---|
| 1 | `docs/algorithms/PHASE3_FITS_IMPL.md:44` `P3FITS-CTYPE` | `p3_output.cpp:186-187` → `:162-163` | `grep -n "CTYPE1" lib/algorithms/fits_output/p3_output.cpp` |
| 2 | 同 `:45` `P3FITS-CUNIT` | `:189-190` → `:165-166` | 同上 |
| 3 | 同 `:41` `P3FITS-CRPIX` | `:197-198` → `:173-174` | 同上 |
| 4 | 同 `:42` `P3FITS-CRVAL` | `:199-200` → `:175-176` | 同上 |
| 5 | 同 `:43` `P3FITS-CD` | `:201-204` → `:177-180` | 同上 |
| 6 | 同 `:40` `P3FITS-BUNIT` | `:216-217` → `:191-192` | 同上 |
| 7 | 同 `:46` `P3FITS-PROV` | `:219-225` → `:195-201` | 同上 |
| 8 | 同 `:243` `P3FITS-MUTEX` | `:156` → `:132` | `grep -n "cfitsio_io_mutex" lib/algorithms/fits_output/p3_output.cpp` |
| 9 | 同 `:39` `P3FITS-BSCALE`（**由 #1 连带暴露的潜在违规**） | `:213-214` → `:188-190` | 原 `:186-187`（CTYPE 行）**偶然**覆盖了 `BSCALE` 字样，CTYPE 订正后该绑定失去覆盖 ⇒ 必须同时订正；`grep -n "BSCALE"` → 188/189/190 |
| 10 | `docs/algorithms/PHASE3_RSMP_IMPL.md:37` `P3RSMP-DESCRIPTOR` | `module_adapters.cpp:697` → `:700` | `grep -n "p3_resample2_descriptor" lib/infrastructure/scheduler/src/module_adapters.cpp` → `:700` |
| 11 | 同 `:47` 端口绑定 | `:702-706` → `:707-711` | 同上 |
| 12 | `docs/algorithms/NOISE_ESTIMATION.md:127` `NOISE-CMAKE` | `CMakeLists.txt:620-632` / `:706` → `:644-659` / `:747` | `grep -n "astrocs_phase1_noise" CMakeLists.txt` → 644/648/653/659/747 |
| 13 | 同 `:260` 同族锚 | `:620-632` → `:644-659`（`:706` → `:747`） | 同上 |
| 14 | 同 `:203` **死锚** `wrapper_phase1/noise_model.h:33` / `.cpp:207` | 文件已在 HEAD 删除（B 退役）⇒ 依 `ANCHOR_CONTRACT.md` §5「删除/改名被锚定的文件 ⇒ 同一提交更新文档锚」改为 `wrapper_phase1/noise_model.{h,cpp}`（花括号形态，两个检查器均跳过）+ **退役留痕**（原锚 `:33`/`:207` 随文件删除失效） | `ls lib/algorithms/noise_snr/wrapper_phase1/` → 无 `noise_model.*` |
| 15 | `docs/algorithms/PHASE3_FITS_IMPL.md:50` §3 标题行数声明 | `556 行 / 64 行 / 50 行`（B2-A9 后） → `520 / 99 / 66`（2026-09-20 实测）+ 行锚重定基说明 | `wc -l lib/algorithms/fits_output/p3_output.{cpp,h} p3_wcs.h` |
| 16 | `docs/algorithms/PHASE3_PROJ_IMPL.md` §16（新增） | 新增行内初写 `hips_p2/README.md:99`、`hips_p2/module.yaml:33` ⇒ 触发 `C2_anchor_resolved` **ambiguous** | 改为全路径 `lib/algorithms/coverage/hips_p2/README.md:99` / `.../module.yaml:33` | `python3 docs/algorithms/anchors/check_doc_line_anchors.py` → `DOC_LINE_ANCHORS_PASS` |

**最终判据**：`timeout 900 python3 ci/run_checks.py --check CHK-SCI-REF` → `CHK-SCI-REF PASS steps=8 rc=0`（`DOC-LINE-ANCHORS` 步内 `DOC_LINE_ANCHORS_PASS: 39 docs, 891 anchors, status=EXEMPT:9, OK:882`，豁免 9 条全部逐条点名、**未新增豁免**）。

## 3 任务 C：权威链改述

| 文件:行 | 原状 | 改为 | 依据 |
|---|---|---|---|
| `docs/algorithms/PHASE2_SAMPLER.md:301` | 「本节为并行语义**唯一权威**。」（下级文档自封权威） | 「本节是 `ASTROCS_DESIGN.md` §8（…确定性合同…线程预算唯一来源）与 §7.1（模块边界）在本模块的**落地细化**，**不另立权威**（§0.1：只有一份权威链）。」 | `ASTROCS_DESIGN.md` §0.1 |

## 4 越界授权修复（**前台 2026-09-20 明确授权**，非擅自越界）

| 文件:行 | 原状 | 改为 | 理由 |
|---|---|---|---|
| `docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md:81` | `` `lib/algorithms/noise_snr/wrapper_phase1/noise_model.cpp` `` | 同路径 + 反引号内加「（已删除：B 已按 EXP-206 退役，此处仅作路径形态示例）」 | 该文件 HEAD 已删 ⇒ `DOC-BAD-FILE`；「已删除」标记必须落在 token 内才被检查器识别（`check_doc_symbols.py:228`），**不是 waiver**、非豁免表 |
| `docs/contracts/DATA_ARTIFACTS.md:53` | `` `contracts/schemas/unified/psfsw_robust_weight.schema.json`，**文件已删除** `` | `` `contracts/schemas/unified/psfsw_robust_weight.schema.json（已删除）`，退役留痕 `` | 同上（FIX-209 真删 schema 后遗留） |
| `tests/quality/test_mod002_migration_refs.py:194,208` | `TARGET = wrapper_phase1/noise_model.cpp` + `assertTrue(src_real.is_file())` | `RETIRED` 不变量 = **该路径不得再被 Git 跟踪**（`git ls-files --error-unmatch` 非零）；负例注入目标改用**同目录仍存活**的 `wrapper_phase1/snr_frame_science.cpp` | 原断言断言一个 HEAD 已删除的文件存在，已失真；改用「未被跟踪」不变量后**不受域外 `touch()` 产生的 0 字节同名文件干扰** |

**结果**：`python3 tools/quality/contracts/check_doc_symbols.py` → `status PASS`（0 findings）；`python3 -m pytest tests/quality/test_mod002_migration_refs.py -q` → `9 passed`。

## 5 域外残留（**只登记，未改**）

| # | 位置 | 机制（已实测点名） | 建议 |
|---|---|---|---|
| 1 | `tools/check_warning_suppression.py:100-105`（`measure_build()`） | **0 字节 phantom 文件的真正产生者**：`rel_src = REPO/"lib"/"algorithms"/"noise_snr"/"wrapper_phase1"/"noise_model.cpp"; rel_src.touch()`。该路径 HEAD 已删（`1fc88989`）⇒ `touch()` **每次调用都新建一个 0 字节文件**；由 `tests/quality/test_mod002_migration_refs.py::test_t20`（正例：跑该检查器）与任何跑 `CHK-WARN`/全量门的流程触发 ⇒ 前台「删后重现」现象由此而来（实测：删除后跑一次 `pytest -k WarningSuppression` 即在 22:01 重现） | 改为 touch 一个**仍存活**的生产源（如 `lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp`）或先 `if not rel_src.exists(): return <结构性跳过>`；域 `tools/**`，本包无权限 |
| 2 | `ci/ledgers/spec_named_impl_gaps.json`（`SNI-S2-NS-01`） | 条目理由「`cpp/src/noise_model.cpp` 不在根构建图」已被 EXP-206 落地的 `CMakeLists.txt:644-659` **推翻** ⇒ `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH` 报 `ledger_stale:SNI-S2-NS-01（台账条目已不复现 ⇒ 必须删除）` | **删除该条**（台账「只减不增」，删除即合规，无需 waiver）；域 `ci/ledgers/**` 仅开放 `dormant_algorithms.json`，本包无权限 |
| 3 | `artifacts/KNOWN_FAILURES_BASELINE.json` | 跑 `ci/run_checks.py --all --profile fast` 时 `CHK-KNOWN-FAILURES-BASELINE` 会**刷新 `source_commit`**（`06216ec8` → `5311dd81`）——检查器自身副作用，非本包语义改动 | 前台提交时按需决定是否纳入该文件 |
| 4 | `reports/v19r2/**`（comment_hygiene / comment_check / file_audit_inventory / source_manifest） | `check_full_integration` 内的质量检查器**重写**这些报告（工作树副作用） | 同上 |

## 6 验收证据

- `timeout 900 python3 ci/run_checks.py --check CHK-SCI-REF CHK-DANGLING CHK-CONTRACT-TEST` → `verdict=PASS entries=3 steps=23 pass=23 fail=0` **RC=0**（`run/RELEASE-03/logs/DOC-205-gates-final.log`）；
- `timeout 1500 python3 ci/run_checks.py --all --profile fast` → `verdict=FAIL entries=40 steps=86 **pass=83 fail=3**`（基线 `pass=80 fail=6` ⇒ **fail 减少 3、无新增红**；余 3 红均为域外/既有：`CHK-MODULE-MANIFEST/CTEST-REGISTRATION`、`CHK-CONFIG-DEFAULTS`（`config/**` 域）、`CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`（残留 #2））；
- `timeout 600 python3 ci/check_no_weight_mode.py` → rc=0；`tests/config/check_cfg002_registry.py --self-test` → `SELF_TEST PASS injections=21 problems=0` + `11/11 PASS`；`docs/standards/checks/check_standards_registry.py --root .` → rc=0；`pytest tests/contracts -q` → `68 passed`。
