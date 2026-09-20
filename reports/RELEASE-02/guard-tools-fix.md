# GUARD-TOOLS-FIX（守护工具修复分片）收口报告

- 分片：`GUARD-TOOLS-FIX`（AstroCS RELEASE-02）
- 依据：`reports/RELEASE-02/conformance/CONFORM-SWEEP-3.json` 条目 **017**（守护工具失效）、**018**（ALG 文档行号锚过期）
- 测量时刻：2026-09-19 23:2x（+0800）；`lib/**` 同时被并行分片 **CONFORM-FIX-B** 修改（见 §6 风险）
- 硬约束遵守：**零 git 写**（`git log/show/ls-files/status` 只读）；**未跑** ninja/cmake/ctest；**未改** `lib/**`、`docs/**`；**未改** `ci/checks.json`（负责人本轮令：靶子静止）；`TMPDIR=/dev/shm/astrocs_gtf` 已清理

---

## 1. 工具失效根因（审计 017）

`tools/config_consistency_check.py` 的四条事实源路径**全部失效**，原因是**两次仓库结构变更**，工具未随之更新（且从未在 `ci/checks.json` 注册，故无人发现）：

| # | 旧路径（工具内硬编码） | 现状 | 根因 |
|---|---|---|---|
| 1 | `lib/phase2/include/astro/phase2/stage2_common.h` | → `lib/algorithms/coverage/include/astro/phase2/stage2_common.h` | 模块并联放置搬迁（AGENTS.md §6；CMakeLists 同步改名） |
| 2 | `lib/phase2/src/stage2_common.cpp` | → `lib/algorithms/coverage/src/stage2_common.cpp` | 同上 |
| 3 | `工程控制/schemas/stage2.schema.json` | **工作树中已删除** | GOV-002 归档 commit `b7b2dea7`「docs(governance): GOV-002 归档非当前工程文档」 |
| 4 | `工程控制/configs/stage2.template.json` | **工作树中已删除** | 同上；`engineering/control/` 目录亦已清空 |

- 事实源 1/2 是**搬迁**，事实源 3/4 是**退役**（内容仍存于 git 对象库 `b7b2dea7^:<path>`）。
- `tools/check_p2_symbol_map.py` 根因同源：`MAP = docs/refactor/P2_SYMBOL_MAP.md` 被 GOV-002 移入 `docs/archive/refactor/`；`SRC = lib/phase2/src` 已搬迁 ⇒ 裸抛 `FileNotFoundError`。
- **后果（审计 017 的判断成立）**：C4（默认值不符）与文档行号锚漂移在全仓无机器门。

---

## 2. 改动清单（file:line）

| 文件 | 状态 | 关键行 |
|---|---|---|
| `tools/config_consistency_check.py` | 重写（968 行） | 事实源 `:60-77`；判据表 `:80-124`（含 `SENTINELS`）；具名常量解析 `:147`；主判定 `:428`；台账装载 `:671`；`--self-test` `:835` |
| `tools/check_p2_symbol_map.py` | 重写（320 行） | `MAP_REL/SRC_REL/INDEX_REL` `:48-50`；`POST_ARCHIVE_ADDITIONS` `:60`；`check_root` `:85`；`--self-test` `:248` |
| `tools/doccheck/check_alg_line_anchors.py` | **新增**（418 行） | `DOC_GLOB` `:51`；行数锚 `:59`；符号表扫描 `:122`；`check_root` `:219`；`emit` `:256`；`--self-test` `:332` |
| `tools/fixtures/config_consistency_known_divergences.json` | **新增**（34 行） | 已登记差异台账（fail-closed） |
| `ci/checks.json` | **未改**（本轮令） | 接入方案见 §5 |

`lib/**`、`docs/**`、`config/**`、`contracts/**` **零改动**。

---

## 3. 修复后判据（三条腿）

### 3.1 `config_consistency_check.py`（C4 默认值一致性）

- **L0 输入面 fail-closed**：活体事实源（header/parser/docs）必须存在；归档事实源必须**确实已退役**（复活 ⇒ `fact_source_resurrected`）；退役声明必须**可验证**（`git log --diff-filter=D` 必须等于登记 commit，否则 `retirement_unverified`）；归档 blob 必须可读；parser 提取数 = 0 ⇒ `parser_extraction`；具名常量解析不到 ⇒ `macro_unresolved`（禁止静默丢键）。
- **L1** struct ↔ parser（原判据保留，新增具名常量解析） · **L2** 归档 template ↔ 归档 schema · **L3/L3b** 归档 schema ↔ 现行 struct（`auto` 值逐条验证落点） · **L4** `docs/development/CONFIG_SCHEMA.md` 自述默认 ↔ 现行实现 · **L5** docs 条件式默认 `key(auto→v)` ↔ parser 实际赋值 · **L6** parser 条件默认 ↔ struct 默认 · **L7** parser 哨兵值（`0 = 按 profile 运行时解析`）↔ docs/struct 固定值。
- **已登记差异台账**（`tools/fixtures/…known_divergences.json`）：未登记差异一律判红；登记项**不再复现即判红**（`registry_stale`，豁免必须存活）；缺 `kind/reason/owner/audit_ref/exit_condition` ⇒ exit 2；`--strict` 忽略台账（审计用）。
- **保留了 legacy 输出键** `checked_keys/mismatches/pass`，下游 `tools/api_doc_consistency.py` 复跑验证通过（`schema_vs_parser=True`、`defaults_vs_template=True`，此前因工具崩溃恒为 `False`）。

### 3.2 `check_p2_symbol_map.py`（映射/ACR 禁止迁移）

R1 锚存活（`ANCHOR_STALE: <常量名> <路径>` 点名）· R2 映射面非空（≥8 行）· R3 映射不悬空 · R4 现行源分类闭合（归档后新增源必须登记 `owner/evidence`；`sky_plane.cpp` 已登记）· R5 ACR 禁止迁移声明完整 · R6 SCI 引用登记在 `docs/contracts/INDEX.yaml`。
**不覆盖（如实声明）**：归档表是 `ARCHIVED_NON_NORMATIVE` 历史记录，其内容正确性不再由本门保证；现行模块映射由 `tools/quality/check_module_map.py` + `docs/architecture/MODULE_MAP.md` 承担。

### 3.3 `check_alg_line_anchors.py`（新增，审计 018）

- **L0 fail-closed**：文档根缺失 / 0 篇文档 / 行数锚 0 / 符号锚 0 / git 不可用 ⇒ 判红（禁止空转判绿）。
- **L1 行数锚**：`<file>（N 行` / `<file> N 行` 的 N 必须 == 目标文件实测行数（去尾换行）。
- **L3 逐符号行号锚**：markdown 表中「列头声明了文件」的行，其首列符号必须**逐字出现在该锚的行范围内**；符号整体不在目标文件 ⇒ `L3_symbol_absent`。
- **L2 目标解析**：锚目标必须解析到唯一**被 git 跟踪**的仓库文件。
- **不覆盖（如实声明）**：显式 `file:N-M` 的**界内**判定由兄弟 step `DOC-LINE-ANCHORS`（`docs/algorithms/anchors/check_doc_line_anchors.py`）承担，本门不重复实现；散文正文里**不带文件列的裸行号**（如代码注释式 `# :555`）在无符号绑定的前提下**无法自动核对**，本门不计入判据（清单见 §4.2）。

---

## 4. 红绿证据（复跑输出）

### 4.1 三条腿的 self-test（全绿）

```text
$ python3 tools/config_consistency_check.py --self-test
CONFIG_CONSISTENCY_SELFTEST_PASS: 16 组（正例 1 + 注入红/恢复绿 + fail-closed）全部符合预期
  N0  正例（活体源一致 + 归档腿可验证）        verdict=PASS
  N0' --strict 暴露已登记差异                verdict=FAIL keys=doc_vs_parser:profile,struct_vs_parser_conditional:smoothing
  N1  注入 parser 默认值不符                  verdict=FAIL keys=struct_vs_parser:robust_mad_clip.max_iterations
  N1' 恢复后回绿                            verdict=PASS
  N2  注入 struct 默认值不符                  verdict=FAIL keys=struct_vs_parser:robust_mad_clip.max_iterations
  N2' 恢复后回绿                            verdict=PASS
  N3  注入 docs 默认值不符                    verdict=FAIL keys=doc_vs_parser:control_grid_per_tile
  N3' 恢复后回绿                            verdict=PASS
  N4  parser 提取面塌缩                      verdict=FAIL keys=parser_extraction
  N4' 恢复后回绿                            verdict=PASS
  N5  归档事实源复活                          verdict=FAIL keys=fact_source_resurrected:schema,fact_source_resurrected:template
  N5' 移除复活文件后回绿                       verdict=PASS
  N6  归档 commit 不符                       verdict=FAIL keys=retirement_unverified:schema,retirement_unverified:template
  N7  台账条目失活                            verdict=FAIL keys=registry_stale:ghost_key
  N8  台账缺必填字段                          verdict=FAIL keys=registry_invalid
  N8' 恢复后回绿                             verdict=PASS

$ python3 tools/check_p2_symbol_map.py --self-test
P2_SYMBOL_MAP_SELFTEST_PASS: 10 组（正例 1 + 负例/恢复 7 类）全部符合预期

$ python3 tools/doccheck/check_alg_line_anchors.py --self-test
ALG_LINE_ANCHORS_SELFTEST_PASS: 11 组（正例 1 + 负例/恢复 6 类）全部符合预期
  N1  行数锚漂移      verdict=FAIL codes=L1_count_mismatch      N1' 恢复 ⇒ PASS
  N2  逐符号锚漂移    verdict=FAIL codes=L3_symbol_drift        N2' 恢复 ⇒ PASS
  N3  符号整体缺失    verdict=FAIL codes=L3_symbol_absent
  N4  锚目标不存在    verdict=FAIL codes=L2_target_unresolved
  N5  扫描面塌缩      verdict=FAIL codes=L0_scan_floor
  N6  文档根缺失      verdict=FAIL codes=L0_scan_floor
  N7  锚指向未跟踪文件 verdict=FAIL codes=L2_target_unresolved   N7' 恢复 ⇒ PASS
```

三条腿的 self-test **均为自造夹具**（`tempfile` 下含真实 git 删除 commit 的独立小仓），**不读真仓**，因此不受并行分片改动影响、零副作用。

### 4.2 真仓复跑

```text
$ python3 tools/config_consistency_check.py
CONFIG_CONSISTENCY_PASS: legs=L0:0,L1:0,L2:0,L3:0,L4:0,L5:0,L6:0,L7:1 checked=83 registered=1
  REGISTERED_DIVERGENCE parser_sentinel:underdetermined_n [L7] audit=CONFORM-SWEEP-3-008
      exit_condition: 订正 docs/development/CONFIG_SCHEMA.md:26 为「underdetermined_n(2；astrocs_adaptive_pixel 档 3)」…

$ python3 tools/config_consistency_check.py --strict      # 审计面：暴露全部差异
CONFIG_CONSISTENCY_FAIL: 1 findings (legs=L7:1)
  [L7] parser_sentinel:underdetermined_n doc_default=2 parser_default=0 struct_default=0

$ python3 tools/check_p2_symbol_map.py
P2-001_PASS: 13 映射行 / 11 现行源，ACR 禁止迁移声明完整，SCI 全部登记

$ python3 tools/doccheck/check_alg_line_anchors.py
ALG_LINE_ANCHORS_FAIL: 140 findings (by_code=L1_count_mismatch:35,L3_symbol_absent:1,L3_symbol_drift:104)
```

**注意**：`ALG-LINE-ANCHORS` 当前**必然红** —— 这正是审计 018 的存量缺陷，修复面（重锚文档）归前台。**未加任何豁免、未登记 SKIP**。

---

## 5. 待办（本分片**未做**，明确移交）

1. **`ci/checks.json` 接入（本轮负责人令「靶子静止」，故未改）**。
   建议作为 `CHK-SCI-REF` 的**新 step**（与兄弟 step `DOC-LINE-ANCHORS` 同层），**不要**新增顶层 id：
   `CHK-REGISTRY-DOC-SYNC` 的 R1 要求 `checks.json` 的**顶层 id** 必须出现在 `docs/ci/01_CHECKS.md` §2，而本分片禁改 `docs/**`，新增顶层 id 会立刻让 P0 门变红。step 形态则不需要改文档。
   现成片段（插入 `ci/checks.json` → `CHK-SCI-REF` → `steps[]` 内，位置紧随 `DOC-LINE-ANCHORS`）：
   ```json
   {
     "id": "ALG-LINE-ANCHORS",
     "profiles": ["fast", "linux-main", "windows-main"],
     "platform": "any",
     "command": ["python3", "tools/doccheck/check_alg_line_anchors.py",
                 "--json-out", "run/ci/doc-anchors/alg_line_anchors.json"],
     "timeout_seconds": 300,
     "heavy": false,
     "mutates_workspace": false,
     "outputs": ["run/ci/doc-anchors/alg_line_anchors.json"],
     "waivable": false,
     "changed_paths": ["docs/algorithms/**", "lib/**", "tests/**",
                       "tools/doccheck/check_alg_line_anchors.py"],
     "requires_monitor": false
   }
   ```
   另建议（可选、需改 `ci/impact_map.json`，本分片未改）：把 `ALG-LINE-ANCHORS` 加进 `docs/**` 规则的 checks 列表，使其在 changed-path 选择模式下也跑。当前 `DOC-LINE-ANCHORS` 亦未在 impact_map 中，属既有状况。
2. **ALG 文档重锚（审计 018，归前台；本分片只建门禁 + 出清单）**。
3. `config_consistency_check.py` 与 `check_p2_symbol_map.py` 目前仍**未注册**进 `ci/checks.json`（与原状一致）。若要注册，同样建议走 step 形态；`config_consistency_check.py` 基线为绿（1 条已登记差异），可直接接入。

### 5.1 需人工重锚清单 —— L1 行数锚（35 处，8 份文档）

| 文档:行 | 目标源文件 | 文档值 → 实测 |
|---|---|---|
| `PHASE2_COVERAGE.md:12` | `lib/algorithms/coverage/include/astro/phase2/coverage.h` | 168 → **169** |
| `PHASE2_MOSAIC_WRITE.md:9,431` | `lib/algorithms/coverage/tools/stage2.cpp` | 1762 → **1965** |
| `PHASE2_REJECTION.md:4,58,662` | `lib/algorithms/coverage/src/rejection.cpp` | 2076 → **2857** |
| `PHASE2_REJECTION.md:6,58,663` | `lib/algorithms/coverage/include/astro/phase2/rejection.h` | 329 → **579** |
| `PHASE2_SAMPLER.md:14,62` | `lib/algorithms/coverage/src/sampler.cpp` | 1156 → **1529** |
| `PHASE2_SAMPLER.md:15,62` | `lib/algorithms/coverage/include/astro/phase2/sampler.h` | 136 → **286** |
| `PHASE2_SESSION.md:21,354` | `lib/phase2_session/p2_session.cpp` | 298 → **318** |
| `PHASE2_UPM_IMPL.md:9,68` | `lib/algorithms/coverage/src/upm.cpp` | 1565 → **2740** |
| `PHASE2_UPM_IMPL.md:10,68` | `lib/algorithms/coverage/include/astro/phase2/upm.h` | 184 → **380** |
| `PHASE3_FITS_IMPL.md:10,50` | `lib/algorithms/fits_output/p3_output.cpp` | 556 → **560** |
| `PHASE3_FITS_IMPL.md:11,50` | `lib/algorithms/fits_output/p3_output.h` | 64 → **99** |
| `PHASE3_FITS_IMPL.md:50` | `lib/algorithms/projection/p3_wcs.h` | 50 → **66** |
| `PHASE3_PROJ_IMPL.md:309` | `lib/algorithms/projection/tests/p3wcs/p3_wcs_test.cpp` | 90 → **451** |
| `PHASE3_RESAMPLE.md:3` | `lib/algorithms/resample/p3_resample.cpp` | 239 → **519** |
| `PHASE3_RSMP_IMPL.md:13,54` | `lib/algorithms/resample/p3_resample.h` | 58 → **150** |
| `PHASE3_RSMP_IMPL.md:14,55` | `lib/algorithms/resample/p3_resample.cpp` | 239 → **519** |
| `PHASE3_RSMP_IMPL.md:56` | `lib/phase3_session/p3_session.cpp` | 343 → **441** |
| `PHASE3_RSMP_IMPL.md:59,290` | `tests/backend/test_p3_resample.py` | 156 → **164** |
| `PHASE3_RSMP_IMPL.md:60,294` | `tests/backend/test_p3003_parallel_resampler.py` | 104 → **139** |

> 审计 018 点名的三个数字（1156 / 1565 / 2076）均已在本门命中；`sampler.cpp` 实测已从审计时的 1520 变为 **1529**、`upm.cpp` 从 2732 变为 **2740**（并行分片在改 `lib/**`，见 §6）。

### 5.2 需人工重锚清单 —— L3 逐符号行号锚（105 处：漂移 104 + 符号整体缺失 1）

| 文档 | 目标源文件 | 漂移锚数 | 偏移量 min/中位/max | 备注 |
|---|---|---|---|---|
| `PHASE2_UPM_IMPL.md` | `lib/algorithms/coverage/src/upm.cpp` | 32 | −1279 / +81 / +317 | §3 逐符号表 + §13 参数表 |
| `PHASE2_UPM_IMPL.md` | `lib/algorithms/coverage/include/astro/phase2/upm.h` | 18 | −100 / +38 / +39 | §3 声明列 |
| `PHASE2_REJECTION.md` | `lib/algorithms/coverage/src/rejection.cpp` | 18 | −738 / +61 / +269 | 另有 1 处 `INTERNAL_ERROR` **整体不存在** |
| `PHASE2_SESSION.md` | `lib/phase2_session/p2_session.cpp` | 15 | −268 / −59 / +4 | |
| `PHASE2_REJECTION.md` | `lib/algorithms/coverage/include/astro/phase2/rejection.h` | 8 | −85 / +25 / +99 | |
| `PHASE2_SAMPLER.md` | `lib/algorithms/coverage/src/sampler.cpp` | 7 | −1029 / +1 / +110 | 含审计 018 点名的 `cvar` |
| `PHASE2_SAMPLER.md` | `lib/algorithms/coverage/include/astro/phase2/sampler.h` | 5 | +1 / +6 / +36 | |
| `PHASE2_COVERAGE.md` | `lib/algorithms/coverage/src/coverage.cpp` | 1 | +41 | `p2_coverage_free` |

- 偏移量**不是常数**（同一文件内 −1279…+317），必须**逐符号**重锚，不能整表平移。
- `PHASE2_INTEGRATION.md`（已重锚先例）与 `PHASE3_*` 多数表已通过，可作重锚格式参照。
- 完整逐条清单（doc:line / 符号 / 目标 / 声明区间 / 实测行）在 `run/ci/doc-anchors/alg_line_anchors.json` 的 `anchors[]`（`status != "OK"`）。

### 5.3 本门**无法自动核对**的锚（如实声明，不假装覆盖）

- **散文正文里不带文件列的裸行号**（如 `PHASE2_UPM_IMPL.md:175` 的 `# :553-559（1e-12 :555）`、`PHASE2_SAMPLER.md` §5.4/§5.5 代码块里的 `# :595` `# :737` 等）：无符号绑定即无法判定「:N 处应当是什么」。审计 018 点名的 `cvar :840-842`、`veto :849-850`、`归一化门 :555/:1341`、`module_adapters.cpp:3152-3176` 属此类，需人工核对（本分片已用一次性探针实测确认它们**确实漂移**，但未固化为门禁）。
- **`module_adapters.cpp` 的行号锚**：该文件不在本门 ALG 文档符号表抽取面内（`PHASE2_UPM_IMPL.md` 正文散锚），人工核对项。
- **`DOC-LINE-ANCHORS` 已有的 9 条 EXEMPT**（冻结 SCI 引述原文 / 外部参考 / run 产物）：本门不重复判定，保持原状。

---

## 6. 未完成项与风险

1. **未完成：`ci/checks.json` 未接入**（本轮令「靶子静止」）。片段已备（§5.1），接入后 `CHK-SCI-REF` 会立即变红（红因 = §5.1/§5.2 存量锚漂移），前台重锚后即绿。**建议顺序：先重锚 → 再接入**，避免 P0 门长时间红。
2. **未完成：`docs/development/CONFIG_SCHEMA.md:26` 的 `underdetermined_n` 订正**（审计 008，归前台）。当前该差异已登记（`parser_sentinel:underdetermined_n`），订正后须**同步删除台账条目**，否则 `registry_stale` 判红（这是设计行为，不是故障）。
3. **风险：并行分片在改同一批文件**。测量期间 `lib/algorithms/coverage/src/{sampler,upm,stage2_common}.cpp` 与 `include/astro/phase2/{sampler,upm}.h` 的 mtime/行数持续变化（`sampler.cpp` 1520→1529、`upm.cpp` 2732→2740、`stage2_common.cpp` 24709→26204 字节）。因此：
   - §5.1/§5.2 的**实测数字是快照**，重锚前请以门禁复跑输出为准；
   - `config_consistency_check.py` 的台账条目按 **2026-09-19 23:2x 的树状态**标定：并行分片 CONFORM-FIX-B-007/009 已把 `profile`、`smoothing` 两条差异修掉，故对应台账条目已按其 `exit_condition` 移除；若这些改动被回退，门禁会以**未登记差异**判红（fail-closed，属预期）。
4. **风险：`config_consistency_check.py` 依赖 git 只读命令**（`git log --diff-filter=D`、`git show`）验证归档事实源。若在无 git 或浅克隆环境运行，会判 `retirement_unverified`（exit 2）—— 这是有意的 fail-closed，不是环境噪声。
5. **风险：`L3` 符号绑定面**基于 markdown 表「列头声明文件」的机械抽取（严格表头识别：分隔行 `|---|` 的上一行）。若未来文档改用非标准表头，抽取面会塌缩并被 `L0_scan_floor` 判红（不会静默漏判）。已人工抽验 4 处漂移（`cvar`、`ACS_ERR_PARAM`、`OK`、`evaluate_c_field`）均为真阳性。
6. **已清理**：`run/tmp/guard_tools_fix/`（3 个一次性补丁脚本）已删除；`/dev/shm/astrocs_gtf` 已清空。**保留**的门禁证据产物：`run/ci/doc-anchors/alg_line_anchors.json`、`run/ci/p2-symbol-map/p2_symbol_map.json`、`run/temp/p2_v15/evidence/config_consistency.json`（均在 gitignore 的 `run/` 下）。

---

## 7. 自洽性检查（本分片终态）

```text
$ python3 -m py_compile tools/config_consistency_check.py tools/check_p2_symbol_map.py \
      tools/doccheck/check_alg_line_anchors.py
COMPILE OK

$ python3 -c "import json;json.load(open('tools/fixtures/config_consistency_known_divergences.json'))"
JSON OK
```

- 无 TODO / 无注释掉的代码 / 无临时调试输出（`grep -n "TODO\|FIXME\|XXX\|print(\"DBG" ` = 0 命中）；
- 三个工具均为纯 stdlib、只读、确定性输出（无时间戳），跨 cwd 复跑一致；
- 未触碰 `lib/**`、`docs/**`、`config/**`、`contracts/**`、`ci/**`；**零 git 写**。
