# 变更 claim：Q2-VERSION-WINDOW-RETIRE — 统一对象合同登记件「删版本号退役窗口，改变更编号 / 负责人裁决哨兵」

- 控制包：RELEASE-03 / 任务 **Q2-VERSION-WINDOW-RETIRE**（承接 `工程控制/RELEASE-03/GAP_AUDIT.md` §4.1 Q2 裁决的**合同登记件 + 契约测试收口面**；文档侧由 DOC-203 先行完成）
- 日期：2026-09-20
- 依据（最高权威）：`ASTROCS_DESIGN.md` **§12**——「**禁止**用版本号作为生效或退役条件（改用日期或变更编号）」
- 依据（裁决正本）：`工程控制/RELEASE-03/GAP_AUDIT.md` **§4.1 Q2**（逐字见 §1）
- 依据（订正清单）：`run/RELEASE-02/design-merge/DESIGN-DRAFT.md` **§3.2-R05**（`unified_object_registry.json#deprecation` 等「合同层引入版本号与带版本号的退役窗口 ⇒ **改为日期 / 变更编号**」）+ **§1.16**（口径：文档/合同内部修订号可留，但**不得**作为生效/退役条件）
- 依据（同类改造口径）：`工程控制/RELEASE-03/change-claims/DOC-203-PREREQ-Q2Q4Q6Q8-SYNC-001.md` §1-Q2（`docs/contracts/UNIFIED_OBJECTS.md` §4 的 12 行版本窗口 → `CHG-2026-09-16-DATA001` + 「待定变更编号（**禁止**用版本号窗口表达）」）
- 依据（工程流程）：`ENGINEERING_SPEC.md` §3（变更 claim + 一致性回归）、§6/§7/§8；`AGENTS.md` §1.1/§4/§5/§8；`CONTROL_PACK_SPEC.md` §5/§6/§7
- 状态：**已落地**（合同登记件 + 契约测试 + 回归同步完成）
- 影响类：**文档 / 合同登记级**（零科学公式、零默认容差、零 SCI/ALG 冻结定义、零数值结论改动；未改 `ci/checks.json`；未挂 waiver；**零 git 写**）
- 文件域（唯一）：`contracts/data/unified_object_compatibility_map_v1.json`、`docs/contracts/unified_object_registry.json`、
  `tests/contracts/test_unified_object_contract.py`、本 claim。`tools/check_version_consistency.py` 与 `tests/version/test_version_consistency.py` **评估后未改**（理由与实测见 §5）。
  **未动**：`docs/**`（DOC-203/202 域）、`lib/**`、`ci/**`、`config/**`、`contracts/schemas/**`。

---

## 1 裁决原文（逐字，不得改写）

> **裁决：在位保留，身份归一为「设计档案 / 产品族专用投影（非生产目标态）」，删除版本号退役窗口，weight_mode 家族按 A44 作废。**
> —— `工程控制/RELEASE-03/GAP_AUDIT.md` §4.1 Q2

同节处置表逐字（本任务对应行）：

> | `UNIFIED_OBJECTS.md:72-84`、`unified_object_registry.json#deprecation`、`INDEX.yaml:4` 的 `0.11.0-alpha.2 → 0.12.0` 版本窗口 | 违反 §12 | **版本窗口 → 日期 / 变更编号**（如 `RELEASE-03 / CHG-2026-09-20-01`） |

订正清单逐字（DESIGN-DRAFT §3.2-R05）：

> | R05 | `docs/contracts/INDEX.yaml:4`；`unified_object_registry.json#deprecation`；`UNIFIED_OBJECTS.md:72-84`；`API-001.md:3`、`ARCH-001.md:3`、`RT-001.md:3`、`TEST_MATRIX.md:3` | 合同层引入版本号与带版本号的退役窗口 | **改为日期 / 变更编号**（§1.16；口径待确认 §4.2-Q7） |

> 注：Q2 中的 `CHG-2026-09-20-01` 是**形态示例**（原文「如」），非指定值。本任务取值见 §2 取证。

---

## 2 取证过程：变更编号 `CHG-2026-09-16-DATA001` 的一手来源

**结论**：这批旧路径/旧 ID 的废弃登记变更编号 = **`CHG-2026-09-16-DATA001`**（形态匹配 `^CHG-\d{4}-\d{2}-\d{2}-[A-Z0-9-]+$`）。

**取证步骤（全部可复核，只读）**：

1. **仓内命名空间普查**：`grep -rn "CHG-[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}-[A-Z0-9-]\+" .` ⇒ 现存 9 个变更编号
   （`CHG-2026-09-20-PSFSW-RETIRE`、`CHG-2026-09-20-DOC203`、`CHG-2026-09-16-DATA001`、`CHG-2026-09-20-01`、`CHG-2026-09-20-REJ-SMALLN`、
   `CHG-2026-09-20-UPM-CTRLWEIGHT`、`CHG-2026-09-20-NAN-MASK`、`CHG-2026-09-20-NOISE-A`），其中与「统一对象合同旧路径废弃窗口」同源者**只有** `CHG-2026-09-16-DATA001`。
2. **DOC-203 先例**：`grep -rn "CHG-2026-09-16-DATA001"` ⇒ **12 处**，全部在 `docs/contracts/UNIFIED_OBJECTS.md` §4 登记表的「废弃登记（**变更编号**）」列（:82–:94），
   即 DOC-203 已按 Q2 裁决把**同一批** 12 条旧路径的版本窗口换成该编号 ⇒ 本任务沿用同一变更编号，保证「同一废弃事件只有一个编号」。
3. **一手提交溯源（`git log` 只读）**：
   - `git log --format="%h|%ad|%s" --date=short -- docs/contracts/unified_object_registry.json contracts/data/unified_object_compatibility_map_v1.json` ⇒ 最早条目
     `678fd51a|2026-09-16|feat(contracts): DATA-001 统一对象合同（14 canonical schema + 端口合同 + 兼容映射）`；
   - `git show --no-patch --format="commit=%H%nauthor_date=%ad" --date=iso-strict 678fd51a` ⇒
     `commit=678fd51a50a18ed07c21cf892e72aff72ef61b22`、`author_date=2026-09-16T17:44:37+08:00`（commit date 同）；
   - `git show 678fd51a -- docs/contracts/unified_object_registry.json | grep -n "deprecated_at\|retire_after"` ⇒ 全部为 `+` 行（`+++ b/...` 与 `--- /dev/null` 表明**该文件由本提交新建**），
     即 `"deprecated_at": "0.11.0-alpha.2"` / `"retire_after": "0.12.0"` **系 2026-09-16 的 DATA-001 变更引入**（兼容映射文件同证：`+++ b/contracts/data/unified_object_compatibility_map_v1.json`、`--- /dev/null`）；
   - `git log -S"deprecated_at" -- <两文件>` ⇒ 仅 3 个提交：`678fd51a`（引入）、`832c1b3a`/`5f034dfc`（DOC-202 / FIX-209 的后续改动），**无更早来源**。
4. **编号构造**：`CHG-` + 引入日 `2026-09-16` + 任务号 `DATA-001` 归一为 `DATA001`（去连字符，避免与日期段混淆）。
   **未编造**：该串在仓内已有 12 处先例，且与一手提交同日、同任务（`DATA-001`）。

> **退役条件取值**：`"OWNER_DECISION"` —— 哨兵，语义 = 「退役不得由版本号触发，须由负责人裁决并给出变更编号」；
> 与 DOC-203 在 `UNIFIED_OBJECTS.md` §4「退役条件」列写的「待定变更编号（**禁止**用版本号窗口表达；Q2 裁决 2026-09-20）」**语义等价**（字面不同，见 §7-4）。

---

## 3 逐字段改造表（旧值 → 新值 → 取证）

### 3.1 `contracts/data/unified_object_compatibility_map_v1.json`（32 个字段改名 + 16 条 `retire_note`）

| 对象定位 | 条数 | 旧字段 / 旧值 | 新字段 / 新值 | 取证 |
|---|---|---|---|---|
| `entries[0..8]` | 9 | `"deprecated_at": "0.11.0-alpha.2"` | `"deprecated_at_change": "CHG-2026-09-16-DATA001"` | §2（一手提交 `678fd51a`，2026-09-16 DATA-001） |
| `entries[0..8]` | 9 | `"retire_after": "0.12.0"` | `"retire_after_change": "OWNER_DECISION"` + `"retire_note"` | `ASTROCS_DESIGN.md` §12 + Q2 裁决 |
| `legacy_contract_id_map[0..6]` | 7 | `"deprecated_at": "0.11.0-alpha.2"` | `"deprecated_at_change": "CHG-2026-09-16-DATA001"` | 同上 |
| `legacy_contract_id_map[0..6]` | 7 | `"retire_after": "0.12.0"` | `"retire_after_change": "OWNER_DECISION"` + `"retire_note"` | 同上 |

`retire_note`（16 条，逐字同一句）：

> 依 ASTROCS_DESIGN §12，版本号不得作为退役条件；本项退役须由负责人裁决并登记变更编号。

条目身份（可复核）：`entries` 9 条 = `aio_abi_contract_v1.json` / `artifact_manifest.schema.json` / `artifact_types.registry.json` /
`phase2_uncertainty_rejection_provenance_v1.json` / `phase_product_exchange_matrix.json` / `phase_product_exchange.schema.json` /
`v6_data_dictionary_v1.json` / `v6_migration_map_v1.json` / `examples/`；
`legacy_contract_id_map` 7 条 = `DATA-P1-CAL` / `DATA-P1-COS` / `DATA-P1-COSMETIC` / `DATA-P1-DRZ` / `DATA-P1-SOURCES` / `DATA-P2-SMP` / `DATA-GAIA-001`。

> **口径说明（任务书「9 条」的边界）**：任务书写「9 条 `entries[].deprecated_at`/`retire_after`」，但验收门要求该文件内 `0.11.0-alpha.2|0.12.0` **零命中**；
> 实测该文件另有 `legacy_contract_id_map[]` **7 条**同形字段（改造前 :168/:198/:227/:270/:294/:320/:344 各行对）⇒ **同批一并改名**（共 16 对）。未改名则验收门必红，且会留下半套版本号窗口。

### 3.2 `docs/contracts/unified_object_registry.json`（30 个字段改名 + 18 条 `retire_note`）

| 对象定位 | 条数 | 旧字段 / 旧值 | 新字段 / 新值 | 取证 |
|---|---|---|---|---|
| `deprecation.retire_after`（顶层，改造前 :587） | 1 | `"retire_after": "0.12.0"` | `"retire_after_change": "OWNER_DECISION"` + `"retire_note"` | `ASTROCS_DESIGN.md` §12；Q2 裁决 |
| `deprecation.legacy_paths[0..11].deprecated_at`（:609–:686） | 12 | `"deprecated_at": "0.11.0-alpha.2"` | `"deprecated_at_change": "CHG-2026-09-16-DATA001"` | §2 |
| `deprecation.legacy_paths[0..11].retire_after`（:610–:687） | 12 | `"retire_after": "0.12.0"` | `"retire_after_change": "OWNER_DECISION"` + `"retire_note"` | §12 + Q2 |
| `canonical_object_classes[*].compatibility_projections[*].retire_after`（:31/:53/:75/:139/:231） | 5 | `"retire_after": "0.12.0"` | `"retire_after_change": "OWNER_DECISION"` + `"retire_note"` | §12 + Q2「5 条 `FROZEN_COMPATIBILITY` 产品族专用投影**在位保留**，不得写版本号退役窗口」 |

- `legacy_paths` 12 条身份（`old_path` | `kind`）：`contracts/data/v6_data_dictionary_v1.json`|data_dictionary_index、
  `contracts/data/v6_weight_vocabulary_v1.json`|weight_vocabulary、`contracts/data/v6_migration_map_v1.json`|migration_map、
  `contracts/schemas/v6/astrocs.v6.signal.v1.schema.json`|object_contract、`...v6.covariance.v1.schema.json`|object_contract（variance 面）、
  `...v6.covariance.v1.schema.json`|object_contract（ivar 面）、`...v6.point-information.v1.schema.json`|object_contract、
  `...v6.provenance.v1.schema.json`|object_contract、`contracts/data/phase2_uncertainty_rejection_provenance_v1.json`|product_contract、
  `contracts/data/phase_product_exchange.schema.json`|exchange_planes、`contracts/proposals/v6/data/`|proposal_design_archive、
  `docs/contracts/v6/data/`|human_readable_design_archive。
- 5 条投影身份：`signal`/`variance`/`ivar`/`point_information`/`provenance` → 对应 `contracts/schemas/v6/astrocs.v6.*.v1.schema.json`，
  `status` 全部保持 `FROZEN_COMPATIBILITY`（**在位保留**，Q2）。
- `retire_note` 18 条（与 §3.1 同一句，逐字）。

### 3.3 汇总（机器复算）

| 文件 | `deprecated_at_change` | `retire_after_change` | `retire_note` | 精确旧键（`retire_after`/`deprecated_at`）残留 | 版本号字面量 |
|---|---|---|---|---|---|
| `contracts/data/unified_object_compatibility_map_v1.json` | 16 | 16 | 16 | **0** | **0** |
| `docs/contracts/unified_object_registry.json` | 12 | 18 | 18 | **0** | **0** |

> 复算命令（键级、非子串）：`python3 -c` 递归收集 JSON 键并断言 `retire_after`/`deprecated_at` 精确键为 0、`re.findall(r"\b0\.\d+\.\d+\b", json.dumps(d))` 为空（实测两文件均 `[]`）。
> 说明：`"retire_after_change"` 含子串 `retire_after`，故只能用**键级**判据，不能用子串 grep。

---

## 4 测试同步（收窄，不放宽）

### 4.1 `tests/contracts/test_unified_object_contract.py::TestRegistryIndex`

| 位置（改造前） | 旧断言 | 新断言 | 性质 |
|---|---|---|---|
| `:539` `test_registry_has_deprecation_window` | `assertRegex(dep["retire_after"], r"^\d+\.\d+\.\d+(-[a-z]+\.\d+)?$")` | `assertEqual("OWNER_DECISION", dep["retire_after_change"])` + `assertIn("版本号不得作为退役条件", dep["retire_note"])`；**新增**：12 条 `legacy_paths[]` 逐条 `assertRegex(deprecated_at_change, r"^CHG-\d{4}-\d{2}-\d{2}-[A-Z0-9-]+$")` + `assertEqual("OWNER_DECISION", retire_after_change)` + `assertIn(retire_note)`；**新增**：5 条 `compatibility_projections[]` `assertEqual("FROZEN_COMPATIBILITY", status)` + 哨兵 + 说明句 | 收窄（1 条 → 17 条断言） |
| `:571-572` `test_contracts_data_is_compatibility_only` | `for key in ("deprecated_at","retire_after"): assertRegex(e[key], r"^\d+\.\d+\.\d+(-[a-z]+\.\d+)?$")` | `assertRegex(e["deprecated_at_change"], CHG 正则)` + `assertEqual("OWNER_DECISION", e["retire_after_change"])` + `assertIn(retire_note)`；**新增**：7 条 `legacy_contract_id_map[]` 同款逐条断言（原本零断言） | 收窄（2 条 → 3 + 21 条断言） |
| **新增用例** `test_no_version_window_literals_in_registration_files` | —（无） | `assertNotRegex(json.dumps(reg, ensure_ascii=False), r"\b0\.\d+\.\d+\b", …)` + 对 `unified_object_compatibility_map_v1.json` 同锁 | 新增反向锁（+1 用例） |

- **未删任何测试、未放宽任何断言**；用例数 **67 → 68**（+1），断言数净增。
- 保留原门：`assertGreaterEqual(len(dep["legacy_paths"]), 10)`、`legacy_paths_are_compatibility_only`、`no_second_equivalent_schema` 全部在位。
- 反向锁对**两个**登记件都施加（任务书给的是 `reg` 单条；括注「证明版本号已彻底不在**这些**登记件里」为复数 ⇒ 兼容期映射同锁，属收窄，不是放宽）。

### 4.2 `:539` 改写的**口径偏差说明**（如实登记，未擅自造字段）

任务书要求 `:539` 改为「`assertRegex(dep["deprecated_at_change"], …)` + `assertEqual("OWNER_DECISION", dep["retire_after_change"])`」两条。
**实测**：`deprecation` 对象**只有** `retire_after`（改造前 :587），**没有** `deprecated_at` ⇒ 在 `dep` 上断言 `deprecated_at_change` 必然 `KeyError`（等于凭空新增一个登记字段，超出「逐字段改名」目标形态）。
故按「逐字段改名」口径落地：`dep` 只锁**退役哨兵 + 说明句**；**CHG 正则**在同一测试内对 12 条 `legacy_paths[].deprecated_at_change` 逐条施加（比原断言**更强**）。两条要求（CHG 正则 / OWNER_DECISION 哨兵）均已落到测试里，无一遗漏。

### 4.3 反向锁红绿证据（能红能绿）

日志：`run/RELEASE-03/logs/Q2-VERWIN-injection-redgreen.log`（注入前先 `cp` 备份，`trap` 保证还原；还原后 `sha256` 逐字节复核）

| 轮次 | 注入 | 结果 | 证据摘要 |
|---|---|---|---|
| GREEN（未注入） | — | **1 passed** | `1 passed, 39 deselected` |
| RED-1 | `registry.…compatibility_projections[0].retire_after_change` `OWNER_DECISION` → `0.12.0` | **2 failed** | `FAILED …::test_no_version_window_literals_in_registration_files`、`FAILED …::test_registry_has_deprecation_window`（`AssertionError: 'OWNER_DECISION' != '0.12.0'`） |
| RED-2 | `compat_map.entries[0].deprecated_at_change` `CHG-2026-09-16-DATA001` → `0.11.0-alpha.2` | **2 failed** | `Regex matched: '0.11.0' matches '\b0\.\d+\.\d+\b'`；`FAILED …::test_contracts_data_is_compatibility_only`、`FAILED …::test_no_version_window_literals_in_registration_files` |
| GREEN（还原后） | — | **68 passed**；`reg sha equal: YES`、`map sha equal: YES`；`VERSION_CONSISTENCY_PASS` | `ddcb9b0d2d12fd802f395877bcc51a3a34ee5db6ce28a5d7cae8bf7fc422ea09`（reg）/ `a7ac472bd90beda0959bb3308652547b4ed385135a02c98586df3da5fe264b0d`（map） |

### 4.4 本轮自查发现并自修的一条红（如实登记）

反向锁 docstring 初稿写了字面量示例 `"0.12.0"` ⇒ 被**通用**检查器 `VERSION-CONSISTENCY`（`SCAN_ROOTS` 含 `tests`）判红：
`tests/contracts/test_unified_object_contract.py:565: 未知版本字面量 0.12.0 != 唯一源基础号 0.11.0`（`CHK-UNIT` → `UT-VERSION` 红）。
**自修**：docstring 改为不含数字三元组的措辞（`…改回版本号字面量`）。该红**只存在于两次全量复跑之间**，末次全量 `CHK-UNIT` 已转 PASS（见 §6）。
副产品：这条红反过来证明「登记件/相关测试内不得出现版本号字面量」的口径由**独立**检查器复算，不是本任务自证。

---

## 5 `tools/check_version_consistency.py` 与 `tests/version/test_version_consistency.py` —— 评估结论：**不改**

任务书：这两个文件「**仅当它们读取上述字段**；若只是通用版本号一致性检查则不动，并在回执说明」。逐条评估：

1. **是通用检查器**：`SCAN_ROOTS = ["docs", "contracts/schemas", "tests"]`，把扫描面内任何 `X.Y.Z` 字面量与唯一版本源 `VERSION` 比较；**不解析**本任务两个登记件的字段语义（不读 `deprecation.*`、不读 `entries[*].*`）。
2. **字段名只是通用键表的一项**：`LIFECYCLE_KEYS = ("retire_after", "退役窗口", "introduced_in", "deprecated_in", "removed_in", "since", "until")`，
   豁免形态收窄为 `"<键>": "<X.Y.Z>"` 的**值本身**（`LIFECYCLE_BOUNDARY_RE`）。`"retire_after_change"` **不匹配**该形态（键名后紧跟 `_change`，正则要求键名后即引号+冒号）⇒ 改名后不会误命中，也不会漏判。
3. **自证用例与本任务字段解耦**：`TestLifecycleBoundary.test_16/17/18` 用 `tempfile` 合成夹具（自写 `{"retire_after": "9.9.9"}`、`| 退役窗口 |` 表头），不读真仓登记件。
4. **实测（本任务末次）**：`python3 tools/check_version_consistency.py` → `VERSION_CONSISTENCY_PASS base=0.11.0-alpha.2`，rc=0；
   `python3 -m pytest tests/version -q` → **31 passed**；`python3 -B -m unittest discover -s tests/version -t tests/version` → `Ran 31 tests … OK`。
   ⇒ **改登记件不需要同步改该检查器**。
5. **残留（`tools/**` 域，本任务未授权，登记待前台决定）**：该检查器 docstring `:12-22` 与注释 `:126-137`（W4-A3 口径）以**这两个登记件**的
   `retire_after: "0.12.0"`（原文「`unified_object_registry.json`(20 处) 与 `unified_object_compatibility_map_v1.json`(17 处) … 真仓恒 FAIL(49 条)」）为**现行事由**；
   本任务后真仓**已无**任何 `"<生命周期键>": "<X.Y.Z>"` 形态命中（实测 `grep -rnE "\"(retire_after|introduced_in|deprecated_in|removed_in|since|until)\"\s*:\s*\"[0-9]+\.[0-9]+\.[0-9]+\"" docs contracts/schemas tests` ⇒ 仅命中 `tests/version/test_version_consistency.py` 的合成夹具行）⇒ 该豁免在真仓进入**休眠**（机制与用例仍在，被合成夹具覆盖）。
   属**注释陈旧面**（不影响判据、不产生红），按「文件域唯一」不在本任务授权内。

---

## 6 验收门（命令 + rc + 输出）

| # | 门 | 命令 | rc | 结果 |
|---|---|---|---|---|
| 1 | 版本号清零 grep | `grep -rn "0\.11\.0-alpha\.2\|0\.12\.0" docs/contracts/ docs/design/ docs/api/ docs/development/ contracts/data/unified_object_compatibility_map_v1.json` | 1（无匹配） | **输出为空** ✅（日志 `Q2-VERWIN-final-grep_gate.log`） |
| 2 | 契约测试 | `python3 -m pytest tests/contracts -q` | 0 | **68 passed**（基线 67；+1 反向锁用例，**0 failed**）✅ |
| 3 | 版本测试 | `python3 -m pytest tests/version -q` | 0 | **31 passed** ✅（`python3 -B -m unittest discover -s tests/version -t tests/version` → `Ran 31 tests … OK`） |
| 4 | 反向锁红绿 | 见 §4.3（注入 → 判红；还原 → 判绿） | — | GREEN 1 passed / RED-1 2 failed / RED-2 2 failed / 还原后 68 passed + sha256 一致 ✅ |
| 5 | 全量机器门（fast） | `python3 ci/run_checks.py --all --profile fast` | 1 | `verdict=FAIL entries=40 steps=86 **pass=80 fail=6**`（**无新增红**，见下）✅ |
| 6 | A44 权重门 | `python3 ci/check_no_weight_mode.py` | 0 | `CHK-NO-WEIGHT-MODE_PASS: files=346 lines=57812 无未留痕命中` ✅ |
| 7 | CFG002 自证 | `python3 tests/config/check_cfg002_registry.py --self-test` | 0 | `CFG002_SUMMARY checks=11 pass=11 fail=0 selftest=PASS`（`SELF_TEST PASS injections=21 problems=0`）✅ |
| 8 | claim 落盘 | 本文件 | — | `工程控制/RELEASE-03/change-claims/Q2-VERSION-WINDOW-RETIRE.md` ✅ |

**受影响检查项逐个复跑（额外证据）**：

| 步骤 | 命令 | rc | 结果 |
|---|---|---|---|
| `UT-CONTRACTS` | `python3 -B -m unittest discover -s tests/contracts -t tests/contracts` | 0 | `Ran 68 tests … OK` ✅ |
| `CON-TEST-CONTRACTS` | `python3 tools/quality/contracts/check_test_contracts.py` | 0 | `status=PASS tst_count=41` ✅ |
| `CONTRACT-GRAPH` | `python3 tools/check_contract_graph.py` | 0 | `CONTRACT_GRAPH_PASS contracts=116` ✅ |
| `DATA-ARTIFACTS` | `python3 tools/check_data_artifacts.py` | 0 | `DATA_ARTIFACTS_PASS schemas=31` ✅ |
| `PY-TEST-INDEX-VERDICTS` | `python3 ci/check_test_index.py` | 0 | PASS ✅ |
| `VERSION-CONSISTENCY` | `python3 tools/check_version_consistency.py` | 0 | `VERSION_CONSISTENCY_PASS base=0.11.0-alpha.2` ✅ |

**「无新增红」的对照证据**：

| 运行 | 时间 | 结果 | 红项 |
|---|---|---|---|
| 任务书给定基线 | 本任务前 | `pass=79 fail=7` | 7 项（含 `CHK-SECRET-HYGIENE`；DOC-203 §5/§6 同名清单） |
| 本任务中段基线（改动**前**启动，`Q2-VERWIN-baseline-run_checks.log`） | 20:33–20:38（首个文件改动 20:37:12） | `pass=80 fail=6` | CHK-MODULE-MANIFEST / CHK-SCI-REF / CHK-CONTRACT-TEST / CHK-DANGLING / CHK-CONFIG-DEFAULTS / CHK-SPEC-NAMED-IMPL-ON-PROD-PATH |
| 本任务末次（改动**后** + claim 落盘后，`Q2-VERWIN-final3-run_checks.log`） | 20:58–21:03 | `pass=80 fail=6` | **与中段基线同名同数**（`diff` 为空）⇒ **零新增红**；`CHK-SECRET-HYGIENE` 由他人在飞任务转绿，故比任务书基线少 1 红（改动后未落 claim 的 `-final2-` 轮同为 `pass=80 fail=6`） |
| （过程红，已自修） | 20:45 全量 | `pass=79 fail=7` | 多出 `CHK-UNIT`（`UT-VERSION`）—— 本任务 §4.4 引入并当轮自修；末次已消失 |

既有红均为**本任务前已红**且原因不在本域：
`CON-FULL-INTEGRATION` = `check_doc_symbols` FAIL（`INTEG-P1-FAIL`/`check_doc_symbols`；DOC-203 §6-8 已登记，DOC-202 域）；
`DOC-LINE-ANCHORS`（`lib/**` 行锚）、`CTEST-REGISTRATION`、`CHK-CONFIG-DEFAULTS`、`SPEC-NAMED-IMPL-ON-PROD-PATH`、`CON-DOC-SYMBOLS` 同理。

日志清单（全部 `run/RELEASE-03/logs/Q2-VERWIN-*.log`）：
`-baseline-run_checks` / `-baseline-contracts` / `-baseline-version` / `-baseline-no_weight_mode` / `-baseline-cfg002` / `-baseline-verchk` / `-baseline-grep_gate` /
`-injection-redgreen` / `-after-UT-VERSION`（过程红证据） / `-final-contracts` / `-final-version_pytest` / `-final-version_unittest` / `-final-no_weight_mode` / `-final-cfg002` / `-final-grep_gate` /
`-final-step-*`（受影响检查项逐个复跑） / `-final2-run_checks`（改动后全量） / `-final3-run_checks`（claim 落盘后末次全量）。

---

## 7 未决与残留（穷尽列出）

1. **`tools/check_version_consistency.py` W4-A3 豁免休眠**（§5-5）：机制与用例保留，但真仓已无命中；其 docstring/注释仍以这两个登记件的 `retire_after: "0.12.0"` 为「现行事由」⇒ 注释陈旧。`tools/**` + `tests/version/**` 不在本任务「文件域唯一」授权内，**上呈前台**决定是否另派任务清理（清理不得删机制与用例，且必须同批改 docstring/注释）。
2. **反向锁失败信息冗长**：`assertNotRegex` 在失败时把整个 JSON 字符串（~20 KB）打进断言消息（unittest 的 `_formatMessage` 会拼接默认消息 + 自定义 `msg`）。为与任务书指定的判据**逐字一致**，未改成自定义 helper；如需收敛可后续改为「先 `re.findall` 报告命中字面量，再断言」。不影响判红判绿。
3. **`:539` 口径偏差**（§4.2）：`deprecation` 无 `deprecated_at`，故未在该对象上造 `deprecated_at_change`；CHG 正则改锁 12 条 `legacy_paths[]`。若前台要求 `deprecation` 也带一个「整体废弃登记编号」字段，须先有规范（本任务不擅自新增登记字段）。
4. **跨文件字面量未统一（`docs/**` 禁改）**：`docs/contracts/UNIFIED_OBJECTS.md` §4「退役条件」列字面量 = 「待定变更编号（**禁止**用版本号窗口表达；Q2 裁决 2026-09-20）」，与本 JSON 的 `"OWNER_DECISION"` 哨兵**语义等价、字面不同**。若要逐字一致，须另派 docs 任务（DOC-203 域）。
5. **未跑 Windows 腿 / heavy 档**：本任务为文档-合同登记级改动，`--profile fast` 已覆盖相关门（`CHK-CONTRACT-TEST` 含 `UT-CONTRACTS`）。`ci/checks.json` 未改（`CHK-CONTRACT-TEST` 的 `changed_paths` 已含 `contracts/**`、`docs/**`、`tests/**` ⇒ 无需改注册表）。
6. **工作区并发**：测得基线/末次全量时工作区有 78+ 处**他人在飞**改动（`git status --porcelain`）；两次全量在同一工作区测得，已按「红项逐名对照」判「无新增红」（§6）。
7. **未动 `contracts/schemas/unified/port_contract.schema.json`**：该文件不含 `deprecated_at`/`retire_after` 字段（FIX-209 已把退役记录写成 `x-astrocs-object.retired_objects`，用 `retired_at_change` 口径）⇒ 无需同步。

---

## 8 规范依据（逐条）

| 依据 | 条款 | 本任务落点 |
|---|---|---|
| `ASTROCS_DESIGN.md` | **§12**：文档/合同内部修订号不进入程序/代码/产物；**禁止用版本号作为生效或退役条件（改用日期或变更编号）** | 全部 62 个字段改名 + 34 条 `retire_note` |
| `工程控制/RELEASE-03/GAP_AUDIT.md` | **§4.1 Q2**（逐字见 §1）：删版本号退役窗口 | 两登记件 32 + 30 处 |
| `run/RELEASE-02/design-merge/DESIGN-DRAFT.md` | **§3.2-R05**（改为日期/变更编号）、**§1.16**（口径） | §2 取证、§3 改造表 |
| `ENGINEERING_SPEC.md` | §3（变更 claim + 一致性回归）、§5（测试规范）、§6（提交）、§7（目录）、§8（机器门：能红能绿 / fail-closed / 锚存活） | 本 claim、§4 反向锁、§6 门 |
| `CONTROL_PACK_SPEC.md` | §5（任务模板：目标/依据/文件域/验收门/禁止）、§6（SubAgent 零 git 写 + 自证材料）、§7（独立验证三层） | 文件域唯一、零 git 写、§6 证据 |
| `AGENTS.md` | §1.1（先定规范再动手）、§4（最小改动面）、§5（硬禁令）、§8（自查/负例注入/能红能绿） | §4.3 注入证据、§4.4 自修 |
| `docs/contracts/UNIFIED_OBJECTS.md`（DOC-203 先例，只读） | §4 表头「废弃登记（**变更编号**）/ 退役条件（**变更编号 / 日期**）」+ 12 行 `CHG-2026-09-16-DATA001` | §2 取证、编号一致性 |

---

## 9 改动文件清单

| # | 文件 | 变更 |
|---|---|---|
| 1 | `contracts/data/unified_object_compatibility_map_v1.json` | 32 字段改名（16 `deprecated_at_change` + 16 `retire_after_change`）+ 16 `retire_note` |
| 2 | `docs/contracts/unified_object_registry.json` | 30 字段改名（12 `deprecated_at_change` + 18 `retire_after_change`）+ 18 `retire_note` |
| 3 | `tests/contracts/test_unified_object_contract.py` | 2 处断言同步（收窄）+ 新增反向锁用例（67 → 68 用例） |
| 4 | `工程控制/RELEASE-03/change-claims/Q2-VERSION-WINDOW-RETIRE.md` | 本 claim |

**未改**：`ci/checks.json`、`ci/**`、`config/**`、`lib/**`、`contracts/schemas/**`、`docs/**`、`tools/**`、`tests/version/**`；未挂 waiver；未做任何 git 写操作。
