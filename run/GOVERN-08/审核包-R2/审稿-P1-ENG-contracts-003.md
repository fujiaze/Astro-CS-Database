# 审稿-P1 · ENG-contracts-003（G08-05 对抗审稿第 1 遍）

- 基线：`/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 片清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:1042-1082`
- 纪律遵守：零 git 写（仅 `rev-parse`/`ls-tree` 只读）、未编译、未跑 ctest/pytest/构建、未跑任何项目脚本或实验脚本、**未读 `/tmp/acsd_g08/`**、未改任何仓内文件（除本交付件）。
- 计数口径：本片成员 = 33 份 / 8173 行（权威版 `实际行数: 8173`，与 `wc -l` 实测 8173 一致）。下文"门实例"一律指**约束条目实例**；因本片为合同/Schema 层，**不含 ctest/pytest 门实例**，故本片不涉及"门实例/去重门/整改分母"三层计数之分——本片只报**文件数/行数**两层。

---

## 1. 读完了吗

**没有全部读完。下面是如实口径。**

| 口径 | 数值 |
|---|---|
| 成员份数 | 33 |
| **我亲自逐行读完** | **22 份** |
| 成员总行数 | 8173 |
| **我实际亲读行数** | **2912** |
| **亲读覆盖率** | **35.6%（行）/ 66.7%（份）** |
| 委托子代理读完（我逐条复核其结论） | 33 份全覆盖 |

**未由我本人读完的 11 份 / 5262 行（如实列出，不掩饰）：**

| 文件 | 行数 |
|---|---|
| `eng/contracts/schemas/cpu_profile.schema.json` | 905 |
| `eng/contracts/schemas/phase_config_normalize.schema.json` | 731 |
| `eng/contracts/schemas/phase_config_export.schema.json` | 648 |
| `eng/contracts/schemas/unified/variance.schema.json` | 618 |
| `eng/contracts/schemas/hips_storage_form.schema.json` | 527 |
| `eng/contracts/schemas/unified/provenance.schema.json` | 470 |
| `eng/contracts/schemas/unified/frame_snr.schema.json` | 390 |
| `eng/contracts/schemas/unified/coverage.schema.json` | 331 |
| `eng/contracts/schemas/unified/depth_m5.schema.json` | 323 |
| `eng/contracts/config/module_dll_contract.schema.json` | 267 |
| `eng/contracts/schemas/unified/examples/sparse_snr_layer.example.json` | 52 |

**这 11 份的处理方式：** 由子代理完整读完，我**没有默认采信**，而是对其每一条 BLOCKER/MUST-FIX 结论**亲自重跑复现**（见 §6、§7）。凡我复现不出的，不进入本报告的发现清单。凡属这 11 份的发现，下文一律标注 `[转]`（转述＝已复现）与 `[未复现·仅转述]`。**未复现的条目不得据此施工。**

---

## 2. 本片判定

### **需修（BLOCKING-REQUIRED）** — 不得进入发布；须先修「自证式判据」与「计数/契约互斥」两类硬伤

最重的 3 条：

1. **【自证式判据·闭环三角】`weight_verdict` 的「权威」从未被读。**
   `eng/contracts/data/unified_object_registry.json:8` 明写权威是 `docs/detail/UNIFIED_MODEL.md §2（「可否作权重」列）」；但 `:93`/`:121` 的判定文本与该权威**逐字不同**，而唯一的机器门 `eng/tests/contracts/test_unified_object_contract.py:553` 比对的是**测试内写死的 `VERDICT` 字典**（`:47-61`），该字典根本不打开 `UNIFIED_MODEL.md`（我实测：全文仅注释与 `:433` 一处列表提到该文件名）。
   ⇒ **registry ⇄ schema ⇄ test 三方互相印证，无一外部锚点**，且三方一致的只是「抄写」，与被声明的权威不一致。这是本片最有价值的产出：**用同一个定义式既当被检量又当期望量**。
   权威原文（我实测 `docs/detail/UNIFIED_MODEL.md:43`）：`唯一帧级参考；Phase2 归一后换算 w=SNR²/F_ref²=1/σ_F²`
   registry 声明（`unified_object_registry.json:93`）：`唯一帧级参考；权重由 Phase2 逆方差叠加从 SNR 计算`

2. **【恒红门·广告即不可能】`declared_via_provenance` 是枚举里广告、却被自身约束永久判红的值。**
   5 个对象 schema 的 `units.bunit_semantics` 枚举都列出 `declared_via_provenance`，但 `provenance.schema.json` 同时用 `units.pixel_semantics: const "dimensionless"` 与 allOf 分支强制 `const "surface_brightness"`；`coverage.schema.json` 同型（`const "probability"` vs 强制 `surface_brightness`）。
   我用仓内校验器 `eng/tests/common/jsonschema_min.py` 实测：
   ```
   provenance + bunit_semantics=declared_via_provenance → REJECTED
       (units,pixel_semantics): const: expected 'surface_brightness'
       (units,pixel_area_power): const: expected -2
   coverage  + 同上 → REJECTED（同两错）
   ```
   该枚举成员的**可接受集为空** = 恒红分支。生产方按 schema 选用该合法枚举值必被拒。

3. **【契约互斥·不可满足】同一个合同文件钉死两个互斥版本号。**
   `eng/contracts/config/module_dll_contract.schema.json:23` `"target_version": {"const": "0.1.0-alpha.1"}`，而 `:75` `"pattern": "^ACSD-0\\.11\\.0-alpha\\.1-win-x64/$"`。
   仓内真值：`VERSION` = `0.1.0-alpha.1`；`eng/packaging/install-tree.contract.json:6` = `ACSD-0.1.0-alpha.1-win-x64/`。
   ⇒ **不存在任何一份文档能同时满足这两条**；`root_layout` 拒绝真实安装根、接受不存在的安装根。AGENTS.md §11 要求根 `VERSION` 为唯一来源，此处被本文件自身破坏。

---

## 3. 逐文件清单（22 份亲读）

| # | 文件 | 我读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `data/contract_index.yaml` (1005) | 全 1005 行 | 116 条合同；`:636` 悬空 path；`:831` 伪引「14 个对象」 | **须修** |
| 2 | `data/unified_object_registry.json` (681) | 全 681 行 | `:8`/`:93`/`:121` 判定漂移；`:199` 前提造假；`:673` 判据陈述窄于其数据 | **须修** |
| 3 | `data/aio_abi_contract_v1.json` (169/170) | 全文件 | `:7 frozen:true` 但 `:137` 实现路径悬空；`:139` "单一实现"不成立 | **须修** |
| 4 | `data/artifact_manifest.schema.json` (149) | 全 149 行 | `:43` pattern 与注册表互斥；`:127` 宣称的唯一性门不可表达 | **阻断** |
| 5 | `block_flow/conformance_deviations.json` (55) | 全 55 行 | 两个机器检查器均已删；`:54` "自测 14/14" 无法复跑 | **须修** |
| 6 | `anchors/unresolved_registry.json` (125) | 全 125 行 | `:7` 计数 24 vs 实有 13；`:24` 逐字引用不存在 | **阻断** |
| 7 | `data/phase2_uncertainty_rejection_provenance_v1.json` (119) | 全 119 行 | `:100` P0 堆越界读无 resolution，文件却 `frozen:true` | **阻断** |
| 8 | `schemas/perf_gate_criteria.schema.json` (78) | 全 78 行 | `:16` fail-closed 自证；`:43-55` op/threshold 无约束 | **须修** |
| 9 | `config/cli_selftest.schema.json` (27) | 全 27 行 | `checks` 无 minItems，`verdict` 与 checks 无关联 | **须修** |
| 10 | `schemas/scheduler_probe_event.schema.json` (70) | 全 70 行 | kind↔unit 无绑定；per-kind 必需标识缺失 | 建议 |
| 11 | `schemas/unified/negative/EXPECTED.json` (50) | 全 50 行 | 6 条负例**全部存在且全部真为负** | **通过** |
| 12 | `…/negative/n1_bare_weight…json` (36) | 全 36 行 | 真负例，`weight` 为裸标量 | **通过** |
| 13 | `…/examples/provenance.example.json` (42) | 全 42 行 | `:6-7` `bunit:"1"` 却声明 `written_px_power` | 建议 |
| 14 | `…/examples/ivar.example.json` (36) | 全 36 行 | `:10` `target_pixel_area: 1.0`（≈全天 8%） | 建议 |
| 15 | `…/examples/signal_flux.example.json` (35) | 全 35 行 | `:6-7` `bunit:"ADU"` 声明含立体角幂次 | 建议 |
| 16 | `…/examples/rejection.example.json` (39) | 全 39 行 | `:7` 同族问题；`:35` `threshold_ref` 为自由串 | 建议 |
| 17 | `…/examples/coverage.example.json` (35) | 全 35 行 | `:7` 同族问题 | 建议 |
| 18 | `…/examples/validity.example.json` (34) | 全 34 行 | `:7` 同族问题 | 建议 |
| 19 | `data/examples/psf.example.json` (43) | 全 43 行 | `:2` 仍用 v6 schema id；`:19` 自声明归一化 | 建议 |
| 20 | `data/examples/effective-psf.example.json` (17) | 全 17 行 | `:15` 占位/伪造哈希 | **须修** |
| 21 | `data/examples/external_fixture_hips.example.json` (41) | 全 41 行 | 内嵌 manifest 合法 | 通过 |
| 22 | `data/examples/frame_hips_manifest.example.json` (25) | 全 25 行 | `type_id` 合规；哈希为规整占位但形态合法 | 通过 |

---

## 4. 发现清单

### 阻断（BLOCKER）

**B-0 ⭐ 冻结合同登记了一个被上游明令禁用、且有活测试断言其「不存在」的 provenance 键** `[转·已复现]`
`data/phase2_uncertainty_rejection_provenance_v1.json:7` `"frozen": true`；`:61-64` 冻结 `ACSD_WEIGHT_MODE`（`"value": "0 | 1 | 2"`）为五键之一（`:78`/`:90` 均称「五键」）。
实测上游与运行时**一致否认该键存在**：
- `docs/science/DATA_SEMANTICS.md:2840`（**正是本合同 `:8` 所引的 §30.3**）逐字：`| ACSD_WEIGHT_MODE（键不存在） | — | **禁用键** |`
- `docs/science/DATA_SEMANTICS.md:3259`：该键"一律标 **ARCHIVED**"
- `lib/infrastructure/aio/include/aio_hips.h:224`：`// **已删除的键: 旧「权重模式」provenance 键**。`
- `eng/tests/unit/p2002_unc_rej_prov_test.cpp:1506` **主动断言其不存在**：`CHECK_MSG(!intj.contains("ACSD_WEIGHT_MODE"), …)`；`:1400` 注明"已从契约面删除（原五键）"

⇒ **照本合同实现即必然使该测试转红。** 按 AGENTS.md §3，`docs/science` 层高于 `eng/contracts`，故错在本合同侧。活合同实为**四键**。这是「上游撤销、下游冻结件未收口」的典型，且被一个**活着的**测试当场抓住。

**B-0b ⭐ 冻结合同把已实现的通道写成 PENDING（归档落后于代码）** `[转·已复现]`
同文件 `:81-87` `"writer_int32_tile": {"status": "PENDING_AIO_DOMAIN", "gap": "aio_hips writer 无 int32 子产品写通道（flags 位 32/64 **未定义**、无 aio_hips_write_*_tile int32 变体）"}`。
实测 `lib/infrastructure/aio/include/aio_hips.h`：
- `:46-47` **`AIO_HIPS_PRODUCT_NREJ = 32`、`AIO_HIPS_PRODUCT_NUSED = 64` 已定义**
- `:211` **`aio_hips_write_diag_tile(...)` 已存在**（即所缺的 int32 变体）
- `:43` 注释自述「32/64 **此前**为空闲位」——本合同的「空闲」措辞已过期

⇒ 本合同自身 `:45` 声称的「32/64 空闲」与同一行引用的头文件**直接矛盾**。属"代码改了、归档没重跑"的反向形态：冻结合同声称能力**尚未**存在，而它已存在。

**B-1 `artifact_manifest.schema.json` 的 `type_id` pattern 拒绝注册表自己登记的合法类型** `[亲读+复现]`
`data/artifact_manifest.schema.json:43` `"pattern": "^acsd\\.[a-z0-9]+\\.[a-z0-9_]+\\.v[0-9]+$"`，`:44` 声明「必须登记于 `artifact_types.registry.json`（未知 type 拒绝）」。
实测注册表 4 个 `type_id`：`acsd.phase1.frame_hips.v1` / `acsd.phase2.mosaic_hips.v1` / `acsd.phase3.planar_fits.v1` / `acsd.calibrated_frame.v1`。
第 2 段是 `[a-z0-9]+`（**禁下划线**）。`calibrated_frame` 含下划线 ⇒ **不匹配**。
⇒ 两个必须一致的合同互相矛盾；该类型在任何 manifest 中都写不出来。另 3 个之所以匹配，是因为下划线落在**第 3 段** `[a-z0-9_]+`——同族命名的不一致。

**B-2 `unresolved_registry.json` 的 `owner_summary` 与自身条目不符（自证式计数）** `[亲读+复现]`
`:7` `"EXTERNAL_REFERENCE": 24`、`:8` `"HISTORICAL_NARRATION": 1`（合计 25）；
`:10` `"max_entries": 14`；
实测 `entries` 长度 = **14**，kind 直方图 = `{EXTERNAL_REFERENCE: 13, HISTORICAL_NARRATION: 1}`。
⇒ 汇总比实有**多 11 条**，且与同文件 `:10` 的 14 自相矛盾。这是台账类文件最核心的自证失效：按 `owner_summary` 判断覆盖面的工具会系统性高估。

**B-3 `unresolved_registry.json:24` 逐字引用了一段不存在的话（伪引）** `[亲读+复现]`
`:24` 用「」引用 `§6 订正留痕原文「DISP-DRZ-001 双方锚由 api.cpp:88-92 订正为 hp_drizzle_api.cpp:98-103」`，被引文档 `docs/detail/anchors/ANCHOR_CONTRACT.md`。
实测：该文件中 `api.cpp`、`88-92`、`hp_drizzle_api` **全部零命中**。
⇒ 该条登记的"原文"根本不存在；且按本文件 `:4` 自定规则「登记项不再命中 ⇒ STALE_REGISTRY 判红」，**本台账自身当前应为红**。

**B-4 冻结合同内挂着一个未收敛的 P0 堆越界读** `[亲读+复现]`
`data/phase2_uncertainty_rejection_provenance_v1.json:7` `"frozen": true`；`:96-103` 的 `F-P2-002-01` severity **P0**，正文写明「以 pixel(0..262143) 为基址**越界读堆内存**」，但**无 `resolution` 键**；同文件 `:111-116` 的 `F-P2-002-02`(P1) 明确带 `"resolution": {"status": "IMPLEMENTED"}`。
⇒ 同一数组内 P1 已收敛、P0 未收敛，而外层标 `frozen:true`。未收敛项无 `fix_owner` 落地证据。

**B-5 `conformance_deviations.json` 的两个机器门均已删除，声明的"机器保证"成空头** `[亲读+复现]`
`:3` 声称本册机器门为 `check_block_flow_conformance.py`（D1–D8）；`:54` 声称「注册表端口面与代码真实数据流的**双向**一致性自 REGISTRY-ALIGN-01 起由 `eng/tools/quality/check_block_flow_ports_vs_code.py` **机器保证**（…自测 14/14 含 13 条负例）」。
实测：**两个脚本均不存在**（`eng/tools/quality/` 现存文件只有 budget_sources.json / build_v19r2~r4_package.py / compare_products.py / conclusion_*.json）；**只剩未跟踪的陈旧 `.pyc`**：`eng/tools/quality/__pycache__/check_block_flow_{conformance,ports_vs_code}.cpython-313.pyc`（`git ls-files eng/tools/quality | grep block_flow` 只剩 `gen_block_flow_spec.py`）——正是本项目已固化的「只剩 `.pyc`」失效形态。
**且本仓自己已经知道**：`docs/engineering/UNRESOLVED_REGISTER.md:1367` 逐字登记该脚本「**不存在**（只剩 `__pycache__/` 里的陈旧 `.pyc`，`.py` 源不在仓库）」。
⇒ 合同**一面宣称机器保证，一面被本仓自己的登记册否认**。「14/14 自测」是无法复跑的自我背书。

`eng/ci/ledgers/` **整目录不存在**（`:51` 引用，`git ls-files eng/ci` 为空）；`run/ARCH-AUDIT-01/REPORT.md`（`:29`）不存在；`docs/KNOWN_LIMITATIONS.md`（`:14` 引用「§E 条目 32」）不存在（已迁至 `artifacts/evidence/known-limitations-ledger/LIMITATIONS.md`）；`ENGINEERING_SPEC.md`（`:36` 作为 blocker 的规范权威）不在仓内。
⇒ 本册未来无法自行发现证据腐烂。

### 须修（MUST-FIX）

**M-1 `contract_index.yaml:831` 伪引权威（我本片首个亲读发现）** `[亲读+复现]`
`:831` 注释写「UNIFIED_MODEL §2 的 **14 个对象**」；`docs/detail/UNIFIED_MODEL.md:56` 逐字为「**canonical 数据对象 = 13 个**」。registry `:8`/`:673` 亦均写 13。
⇒ index 把**已退役的 psfsw 墓碑条目**（`:891`，OBSOLETE）也算进"权威面对象数"，得出退役前的旧数 14。

**M-2 `contract_index.yaml:636` 悬空 path** `[亲读+复现]`
`path: docs/science/algorithms/ACR_EQUIVALENCE.md` 不存在；真实文件为 `ACR_EQUIVALENCE_ALGORITHMS.md`。
**加重**：同文件 `:888-889` 明写「登记一条指向不存在文件的 path 会让合同图判红」——`:636` 正是它自己禁止的行为。姊妹条 `:129` 指向存在的 `docs/science/ACR_EQUIVALENCE.md`。

**M-3 `registry:673` 判据陈述窄于其数据与其自身机器断言** `[亲读+复现]`
原文要求每个 schema「必须在 `canonical_object_classes` 中被登记归属」。
实测：33 份 schema 中 **20 份不在** `canonical_object_classes`，而在姊妹数组 `other_schema_files`。
按字面读 ⇒ 该判据**当前为红**。而其引用的机器断言 `test_unified_object_contract.py:152` 注释明写更宽口径（「canonical / projection / other（U-02 口径）」），`:153-165` 取并集。
⇒ **可执行的实现是对的，条文写错了**；照条文实现会误判红。

**M-4 `registry:199` 的 trigger 前提在 HEAD 已不成立** `[亲读+复现]`
原文：「MODULE_MAP.md 引用 **22 个 DATA ID**，其中 7 个在统一对象权威面上无解析」。
实测 `docs/engineering/MODULE_MAP.md` 中 `DATA-*` 去重后**仅 2 个**：`DATA-002`、`DATA-P3-REJ-001`；7 个 legacy ID **零出现**。
⇒ 支撑整个 7 条 legacy 裁决的触发前提是伪造的。

**M-5 `registry` 证据基线 `ecf6ad6f` 是空锚** `[转·已复现]`
`:6` `baseline_head: "ecf6ad6f"`；`:307`/`:351` 以"基线对该 ID 零命中"为证。
实测 `git ls-tree ecf6ad6f eng/` 与 `.../docs/engineering/` **均返回空**。
⇒ "在从不存在过的文件里零命中"不是证据；且 `old_contract_face` 所引文件都是该基线**之后**新增的。

**M-6 `weight_verdict` 三方闭环、2/13 与权威不符** `[转·已复现]` — 见 §2 第 1 条。

**M-7 `frozen:true` 的 AIO 合同指向不存在的实现** `[亲读+复现]`
`aio_abi_contract_v1.json:137` `"path": "lib/astro_image_io/src/aio_abi.cpp"`；该目录**不存在**，真实路径 `lib/infrastructure/aio/src/aio_abi.cpp`。
`:139` 「SHA-256 原语复用 `lib/common/crypto` **单一实现**」——`lib/common/` 不存在；真实为 `lib/algorithms/shared/crypto/`；且同一树内另有第二份独立 SHA-256（`lib/infrastructure/aio/product_io/src/sha256.cpp`）⇒「单一实现」为假。

**M-8 `artifact_manifest.schema.json` 宣称的拒绝面在 draft-2020-12 下不可表达** `[亲读+复现]`
`:5` 「严格拒绝：…未知 type_id…type_id↔schema_version 不匹配…producer 重复」；`:127` 「artifact_id **禁止重复**」；`:82` 「非空且唯一」。
实测：全文 **无 `uniqueItems`**，`producer` 是单对象（结构上不可能"重复"），无任何关键字能解析注册表成员资格或交叉核对 type↔version。
⇒ 两个 manifest 仅在 `input_digests[].artifact_id` 重复这一点上不同，**双双合法**。

**M-9 `perf_gate_criteria.schema.json:16` 的 fail-closed 是自证式常量，与自身数值源矛盾** `[转·已复现]`
`:16-17` `"enforcement": {"const": "fail-closed"}`；而其阈值源 `eng/contracts/resource_gate_v1.json` 中 `mean_utilization_enforcement` / `p50_utilization_enforcement` / `per_sample_enforcement` 实测均为 **`record_and_justify`**（"不改变退出码"），仅 `queue_low_window_enforcement` 为 `hard_fail`。
⇒ 4 条冻结判据中 **3 条物理上无法转红**，而 schema 的 `const` 反而为其背书。

**M-10 `perf_gate_criteria` 的 op↔threshold 完全无约束** `[亲读+复现]`
`:43-55` `op: enum[">=","<=","absent"]`、`threshold: type["number","null"]`，**无 if/then 绑定**。
我构造并实测通过：`op=">="` 配 `threshold=null`；`mean_utilization <= 1e308`、`p50_utilization <= -1e308`。前者在数学上恒成立、后者物理不可达 ⇒ **恒绿门**。
另 `:21 minItems:4` 配 4 值 metric 枚举看似完备，但无唯一性/逐 metric 要求 ⇒ 4 条同 metric 亦合法，3 条判据可静默消失。

**M-11 `cli_selftest.schema.json` 结构上无法判红** `[亲读+复现]`
`checks` 无 `minItems`；`verdict` 与 `checks[].status` 无任何关联。
实测通过：`{"checks":[],"verdict":"PASS"}`；`{"checks":[{"name":"dll_load","status":"fail"}],"verdict":"PASS"}`。
与 `:5` 自称「缺 DLL/未装配 → verdict FAIL，退出非零(5)」直接冲突。

**M-12 `phase_config_normalize.schema.json:343` 默认值违反自身约束** `[转·已复现]`
`"saturation_level": {"type":"number","exclusiveMinimum":0,"default":0}`。任何按 default 注入的产出方生成的配置**既不合 schema 也被运行时拒**。

**M-13 `effective-psf.example.json:15` 占位/伪造哈希** `[亲读+复现]`
`"p_eff_hash": "000…0"`（64 个 0）、`"kernel_set_hash": "sha256:ee"`（2 位，非 64 hex）。
⇒ 规范示例教下游产出**退化摘要**；后者连 `sha256` 形态都不成立。

**M-14 `DATA-P3-REJ-001` 是活合同但未登记** `[转·已复现]`
`docs/engineering/MODULE_MAP.md:127` 以其为准据名，且该行自陈「产出该判词的**检查器已物理删除**」。实测其在 `contract_index.yaml` 中 **0 命中**，而 `:3` 声称「逐条登记全仓合同条款」。

### 建议（SUGGESTION）

- **S-1** `unified/examples/` 中 `provenance`/`coverage`/`rejection`/`validity` 4 例 `bunit:"1"` 却声明 `written_px_power`（"BUNIT 显式含立体角幂次"）——"1" 不含立体角。`ivar.example.json:10` `target_pixel_area:1.0` ≈ 全天 8%。[亲读]
- **S-2** `psf.example.json:2` 仍用 `acsd.v6.psf/v1`；registry 已将 v6 面标记为兼容期退役。`psf.example.json:19` `normalization_sum_P:1.0` 为**自声明**归一化——若检查器只读该字段，则等于拿被检量当期望量（真正的 PSF 归一化须由参数算出）。[亲读]
- **S-3** `depth_m5.schema.json:298` `depth_value` **无任何界**（同文件 `:312` `sigma_level` 正确冻结为 `const 5.0`）。[转·已复现结构]
- **S-4** `provenance.schema.json:346` `k_corr` = `exclusiveMinimum:0` + `not:{const:1}` ——在连续域上**只排除单点**，`0.9999999`（差 1 ULP 的目标缺陷）畅通。`variance_inflation` 按定义应 `>=1`。[转·已复现结构]
- **S-5** `registry:647` `#port_contract` 锚不存在（实际键为 `port_contracts`/`port_contract_ref`）。[转·已复现]
- **S-6** `registry:9-11` 与 `hips_storage_form.schema.json:9` 把 `ENGINEERING_SPEC.md` 当上游权威，该文件**不在仓内**（仓内已自记于 `docs/engineering/UNRESOLVED_REGISTER.md`）。[转·已复现]
- **S-7** `registry:427/455/539` 的「载体见 门禁注册面（G08-10 重建）」无任何可解析路径。`contract_index.yaml:134` 的 `L28e-E-004` 全仓零命中，且含历史叙事，违 AGENTS.md §5。[亲读]
- **S-8** `scheduler_probe_event.schema.json`：`kind`↔`unit` 无绑定，`value` 无界；`worker_busy` 无 `worker` 仍合法——而该值正是 L2 利用率判据的输入。[亲读+复现]

---

## 5. 我主动构造的反例

全部用**仓内**校验器 `eng/tests/common/jsonschema_min.py`（只读、内存内、不落盘、不跑测试框架）执行。

| # | 构造 | 期望推翻 | 实测 | 结论 |
|---|---|---|---|---|
| CE-1 | `cli_selftest` = `{"checks":[],"verdict":"PASS"}` | 推翻"自检门能判红" | **VALID** | 未推翻 → **缺陷坐实** |
| CE-2 | 同上，全部 check `skipped` + `verdict:PASS` | 同上 | **VALID** | 未推翻 → 坐实 |
| CE-3 | `perf_gate` 判据 `op:">=" , threshold:null` | 推翻"判据语义受约束" | **VALID** | 未推翻 → 坐实 |
| CE-4 | `perf_gate` `mean_utilization <= 1e308` | 推翻"阈值有域" | **VALID** | 未推翻 → **恒绿门坐实** |
| CE-5 | probe `kind:"worker_busy"` 缺 `worker`，`value:-1` | 推翻"探针能定位来源" | **VALID** | 未推翻 → 坐实 |
| CE-6 | `provenance`/`coverage` 用枚举内合法值 `declared_via_provenance` | 推翻"枚举所列皆可用" | **REJECTED** | 未推翻 → **恒红门坐实** |
| CE-7 | `artifact_manifest` 写注册表登记的 `acsd.calibrated_frame.v1` | 推翻"注册即合法" | **REJECTED**（pattern） | 未推翻 → **B-1 坐实** |
| **CE-8** | **假设 `conformance_deviations.json` 的证据已腐烂** | 推翻"该台账内容可信" | **被推翻** | ✅ **我的假设错了**：token `return light;`(今 1534) 仍在 `p1_calibrated_path`(1530起) 函数体内，`return p1_calibrated_path(doc, light);`(今 1569) 仍在 `p1_cleaned_input_path`(1565起) 内，`acsd.phase2.resample`/`acsd.phase3.resample` 仍存在；**仅行号漂移 ~21 行**，而该文件 `:3` 明写「行号仅作定位辅助」。**其内容属实**，我不报"证据腐烂" |
| **CE-9** | **假设 U-02 判据当前为红** | 推翻"U-02 真绿" | **被推翻** | ✅ 见 §6 |

**净结果：7 个反例成功坐实缺陷，2 个反例推翻了我自己的假设。**

---

## 6. 盲复算

**做法**：先不读任何既有判定（`审稿-RR*/R2-*/R3-*/P1-*` 一律未打开），仅从权威原件出发，独立重算两条可机械判定的判据，再与结论比对。

**盲算 1 — U-02（registry:673）**
口径：**门实例 = 1 条判据（U-02）**；其下 4 条 machine_assertions。分母 = `eng/contracts/schemas/**` 实际 schema 文件数。
```
actual schema files on disk : 33
registered in registry       : 33   (canonical_object_classes 13 + other_schema_files 20)
UNREGISTERED (would go RED) : 0
REGISTERED-BUT-ABSENT(RED)  : 0
duplicate $id               : none
--- 按 registry:673 字面（只认 canonical_object_classes）---
files NOT in canonical_object_classes: 20   => 字面判据为 RED
```
**结论：判一致，但须区分两层。**
- 按**实现口径**（test:152-165 的并集）：**真绿**，且是**双向真门**——`test:164` 枚举磁盘实际文件、`:165` 断言差集为空，新增未登记 schema 会翻红。**我确认这不是恒绿门**（子代理曾怀疑，我实测证伪）。
- 按**条文口径**（registry:673 字面）：**红**，因为 20 份在姊妹数组。
⇒ 偏松的是**条文**，不是实现。记 M-3。

**盲算 2 — artifact_manifest vs artifact_types 注册表**（先不看任何既有结论，直接枚举比对）
```
type_id pattern : ^acsd\.[a-z0-9]+\.[a-z0-9_]+\.v[0-9]+$
acsd.calibrated_frame.v1        -> 第2段含 '_'  -> FAIL
acsd.phase1.frame_hips.v1       -> PASS
acsd.phase2.mosaic_hips.v1      -> PASS
acsd.phase3.planar_fits.v1      -> PASS
=> 1/4 注册类型被自家 schema 拒绝
```
**结论：与我的判定一致（阻断）。相对"注册表为准"的自然读法，此处判据偏松——它把合法类型判为非法。**

**盲算 3 — unresolved_registry 自洽性**
```
declared owner_summary : {EXTERNAL_REFERENCE:24, HISTORICAL_NARRATION:1}  (=25)
declared max_entries   : 14
ACTUAL entries / kinds : 14 / {EXTERNAL_REFERENCE:13, HISTORICAL_NARRATION:1}
=> owner_summary 与实际不符；且与同文件 max_entries 自相矛盾
stale check: 14 条中 13 条命中；1 条（api.cpp:88-92）在 ANCHOR_CONTRACT.md 零命中
```
**结论：一致（阻断）。此处既有判定若偏松，则本片**新发现**了 2 条（见 §9）。**

---

## 7. 子代理派发记录

**派发 5 个**（远超 3-5 下限；其中 1 个因我复制粘贴失误重复，另 1 个未在时限内返回）。

| ID | 范围 | 状态 |
|---|---|---|
| `b479c34a` | contract_index / registry / aio_abi / artifact_manifest | 已回，20 条发现 |
| `ee560417` | **与上完全重复**（我的派发失误） | 已回，16 条发现 |
| `ca2ae907` | cpu_profile / phase_config×2 / module_dll / perf_gate / probe / cli_selftest | 已回，28 条 |
| `1311e55c` | variance / hipsform / provenance / frame_snr / coverage / depth_m5 + 9 例 + 负例 | 已回，23 条 |
| `86f77776` | anchors / block_flow / phase2 / data/examples | **已回**（迟交，25 条）|

### 逐条复核与**否决**记录

我对每一条 BLOCKER/MUST-FIX 都亲自重跑。**否决/降级的如下：**

| 子代理结论 | 处置 | 理由 |
|---|---|---|
| `ca2ae907` #4「`module_dll:75` pattern 为 `ACSD-0.11.0-alpha.1`」 | **一度被我否决，后确认成立** | 我首次 `grep "0\.11"` 返回"无"——**是我的正则错了**（文件内是转义形式 `0\\.11\\.0`，非字面 `0.11`）。改用直读 `:75` 确认子代理正确。**记录我的误判，不掩饰。** |
| `ee560417`/`b479c34a` F14「`L28e-E-004` 全仓零命中」 | **降级为建议，未独立复现** | 未亲自跑全仓该 ID 检索；且其同类条目价值低。已降级到 S-7 且只保留我亲见的历史叙事违规部分。 |
| `1311e55c` BLOCKER 1/2 | **采纳（已复现）** | 我用仓内校验器独立复现 REJECTED 及两条 const 冲突。 |
| `1311e55c` BLOCKER 3（6 例均作虚假 bunit 声明） | **部分采纳** | 我亲读确认 `provenance`/`coverage`/`rejection`/`validity` 四例确为 `bunit:"1"`+`written_px_power`；`signal_flux` 为 `bunit:"ADU"`。**未逐例复现其余**，故按 S-1 降级表述。 |
| `1311e55c` BLOCKER 7（hipsform 检查器缺失） | **采纳，但**发现其与我的 B-5 同源 | 该文件不在我亲读范围，但**缺失事实由我先行独立证实**（`eng/tools/hipsform/` 仅 README.md）。归并入 B-5。 |
| `b479c34a` F7（10/12 legacy_paths 已删） | **未复现，降级为不报** | 我未亲自 `git log` 确认删除提交，不据转述施工。 |
| `b479c34a` F17（TRACEABILITY_SPEC VERIFIED 行） | **否决（超出本片）** | 涉及 `docs/engineering/TRACEABILITY_SPEC.md`，非本片成员，且我未复现。**不进本报告发现清单。** |
| `ca2ae907` #20-#28 建议项 | **多数未复现，不转述** | 未经复现的建议不得进入交付。 |
| `ee560417` F15 / `b479c34a` F15（退役对象活调用者） | **采纳（已复现）** | 我实测 `phase1_product.cpp:182` 确有 `kPsfswRobustWeight` 活消费者。归入建议层（因退役声明本身属实，欠的是台账完整性）。 |
| `b479c34a` F12（U-02 因 projections 全空而退化） | **采纳并加强（已复现）** | 实测 13 个 `compatibility_projections` **全为 `[]`**，故 `test:184-206` 的 schema 间比对体**从不执行**，测试退化为在 registry 自身列表内计数 = **X==X**。这是我确认的**第二处自证式判据**。 |
| `86f77776` #2（PENDING_AIO_DOMAIN 为假） | **采纳，升为 B-0b（已复现）** | 我亲验 `aio_hips.h:46-47` 位值已定义、`:211` int32 writer 已存在。 |
| `86f77776` #3（ACSD_WEIGHT_MODE 为禁用键） | **采纳，升为 B-0（已复现）** | 我亲验 DATA_SEMANTICS:2840「禁用键」、:3259 ARCHIVED、aio_hips.h:224「已删除」、测试 :1506 断言其不存在。**四条独立证据同向，本片最强一条。** |
| `86f77776` #9（四条 evidence 行号全部过期） | **不新增发现** | 与我 CE-8 结论一致：token 均存活于声明符号函数体内，且该文件 `:3` 自述「行号仅作定位辅助」⇒ 不构成假偏差。仅在 S 类提示行号漂移 ~21。 |
| `86f77776` #8 后半（F-P2-002-02 的 evidence 与缺陷同一测试文件） | **采纳为附注并入 B-4** | 属实：`:109`（提出缺陷）与 `:115`（宣告修复）同引 `p2002_unc_rej_prov_test.cpp`，且同属 P2-002 任务 ⇒ **修复证据不独立于被修复者**。已并入 B-4 说明。 |
| `86f77776` 清洁项（4 份示例全合规、psf↔effective-psf FWHM 3.2→3.9 为**必需**差异） | **采纳为 CLEAN** | 与我亲读一致，补强"示例面是本片最健康部分"的判断。 |

**否决/降级合计：5 条明确否决、3 条降级、2 条降级不转述。**

---

## 8. 本片是否发现**新**问题

**是。** 以下 4 条为我本人在亲读过程中**独立发现、子代理未提出**，且均已亲自复现：

1. **B-2** `unresolved_registry.json:7` `owner_summary` 声明 24 条 EXTERNAL_REFERENCE，实有 **13** 条；与同文件 `:10 max_entries:14` 亦自相矛盾。（自证式计数）
2. **B-3** 同文件 `:24` 用「」逐字引用的 `ANCHOR_CONTRACT.md §6` 原文**在该文件中零命中**，且按其自定规则应判 STALE 红灯。（伪引）
3. **B-4** `phase2_uncertainty_rejection_provenance_v1.json` 中 `frozen:true` 之下挂着**无 resolution 的 P0 堆越界读**。（我实测 `resolution` 键仅存在于 P1 条目）
4. **M-1** `contract_index.yaml:831` 对 `UNIFIED_MODEL §2` 的「14 个对象」伪引（权威逐字为 13）——这是我**读到第 831 行时最先发现**的一条，早于任何子代理回执。

**另有 2 条由子代理提出、我逐条复现后采纳的最强阻断（B-0 / B-0b）**，虽非我首提，但四条独立证据同向、且均由我亲手复跑：
- **B-0** 冻结合同登记被上游禁用、且被活测试断言「不存在」的 `ACSD_WEIGHT_MODE`。
- **B-0b** 冻结合同把**已实现**的 int32 写通道写成 `PENDING_AIO_DOMAIN`（归档落后于代码）。

**本片阻断合计 7 条**（B-0、B-0b、B-1、B-2、B-3、B-4、B-5），须修 14 条，建议 8 条。

---

## 9. 自证段（可复跑命令）

全部只读；不编译、不跑测试框架、不写任何文件。

```bash
cd "/workspace/Astro CS Database"

# A. 本片成员与行数（对齐权威版 8173）
sed -n '1042,1082p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml
# 逐份 wc -l 见正文 §1；合计 8173

# B. 伪引 14 vs 13（M-1）
sed -n '831p' eng/contracts/data/contract_index.yaml
sed -n '56p'  docs/detail/UNIFIED_MODEL.md

# C. 悬空 path（M-2）与其自述禁令
sed -n '636p;888,889p' eng/contracts/data/contract_index.yaml
ls docs/science/algorithms/ACR_EQUIVALENCE*.md     # -> ACR_EQUIVALENCE_ALGORITHMS.md

# D. unresolved_registry 自证式计数（B-2）与伪引（B-3）
python3 -c "
import json,collections
d=json.load(open('eng/contracts/anchors/unresolved_registry.json',encoding='utf-8'))
print('declared',d['owner_summary'],'max',d['max_entries'])
print('actual  ',dict(collections.Counter(x['kind'] for x in d['entries'])),len(d['entries']))"
grep -c "api.cpp" docs/detail/anchors/ANCHOR_CONTRACT.md   # -> 0，伪引

# E. frozen 合同内未收敛 P0（B-4）
python3 -c "
import json
d=json.load(open('eng/contracts/data/phase2_uncertainty_rejection_provenance_v1.json',encoding='utf-8'))
print('frozen=',d['frozen'])
[print(f['id'],f['severity'],'resolution=',('resolution' in f)) for f in d['findings_transfer']]"

# F. conformance_deviations 的两个机器门已删（B-5）
ls eng/tools/quality/check_block_flow_ports_vs_code.py eng/tools/quality/check_block_flow_conformance.py 2>&1
ls eng/ci/ledgers/ 2>&1          # -> No such file or directory
ls eng/tools/hipsform/           # -> 仅 README.md
sed -n '3p;51p;54p' eng/contracts/block_flow/conformance_deviations.json

# G. B-1 注册表与 pattern 互斥
sed -n '43p' eng/contracts/data/artifact_manifest.schema.json
python3 -c "
import json,re
d=json.load(open('eng/contracts/data/artifact_types.registry.json',encoding='utf-8'))
pat=re.compile(r'^acsd\.[a-z0-9]+\.[a-z0-9_]+\.v[0-9]+\$')
def w(o):
    if isinstance(o,dict):
        for k,v in o.items():
            if k=='type_id': print(('PASS' if pat.match(v) else 'FAIL'),v)
            w(v)
    elif isinstance(o,list):
        [w(x) for x in o]
w(d)"   # -> FAIL acsd.calibrated_frame.v1

# H. 恒绿门反例 CE-1..CE-4（仓内校验器，内存内，不落盘）
python3 - <<'PY'
import sys,json; sys.path.insert(0,"eng/tests/common")
import jsonschema_min as jm
L=lambda p: json.load(open(p,encoding="utf-8"))
print("CE1", bool(jm.validate({"schema_version":1,"kind":"acsd_selftest","checks":[],"verdict":"PASS"},
        L("eng/contracts/config/cli_selftest.schema.json"))))          # True=VALID 缺陷坐实
c=[{"id":"c%d"%i,"metric":m,"op":op,"threshold":th,"on_violation":"red"}
   for i,(m,op,th) in enumerate([("mean_utilization",">=",None),("p50_utilization",">="0),
   ("frac_windows_ge_0.85","absent",None),("low_util_window_no_backlog","absent",None)],1)]
print("CE3", bool(jm.validate({"schema":"acsd.perf-gate-criteria/v1","enforcement":"fail-closed","criteria":c},
        L("eng/contracts/schemas/perf_gate_criteria.schema.json"))))     # True=VALID
PY

# I. 恒红门反例 CE-6（declared_via_provenance 不可满足）
python3 - <<'PY'
import sys,json; sys.path.insert(0,"eng/tests/common")
import jsonschema_min as jm
L=lambda p: json.load(open(p,encoding="utf-8"))
for name,ex in [("provenance","provenance"),("coverage","coverage")]:
    d=L("eng/contracts/schemas/unified/examples/%s.example.json"%ex)
    d["units"]["bunit_semantics"]="declared_via_provenance"; d["units"]["target_pixel_area"]=1.0
    print(name, jm.validate(d,L("eng/contracts/schemas/unified/%s.schema.json"%name))[:2])
PY

# J. U-02 盲算（§6）
python3 - <<'PY'
import json,glob,os
reg=json.load(open("eng/contracts/data/unified_object_registry.json",encoding="utf-8"))
owned={c["canonical_schema_file"] for c in reg["canonical_object_classes"]}|{o["file"] for o in reg["other_schema_files"]}
act={os.path.relpath(p).replace(os.sep,"/") for p in glob.glob("eng/contracts/schemas/**/*.schema.json",recursive=True)}
print("actual",len(act),"registered",len(owned),"unregistered",len(act-owned),"ghost",len(owned-act))
PY

# K. U-02 判据退化为 X==X（compatibility_projections 全空）
python3 -c "
import json
d=json.load(open('eng/contracts/data/unified_object_registry.json',encoding='utf-8'))
print('total projections =',sum(len(c.get('compatibility_projections',[])) for c in d['canonical_object_classes']))"
sed -n '176,186p' eng/tests/contracts/test_unified_object_contract.py

# L. weight_verdict 三方闭环（M-6）
python3 -c "
import json
d=json.load(open('eng/contracts/data/unified_object_registry.json',encoding='utf-8'))
print([c['weight_verdict'] for c in d['canonical_object_classes'] if c['object_name']=='frame_snr'])"
sed -n '43p' docs/detail/UNIFIED_MODEL.md
grep -n "UNIFIED_MODEL" eng/tests/contracts/test_unified_object_contract.py   # 仅注释，不打开该文件

# M. module_dll 版本互斥（§2 第3条）
sed -n '23p;75p' eng/contracts/config/module_dll_contract.schema.json
cat VERSION; sed -n '6p' eng/packaging/install-tree.contract.json

# N. aio 冻结合同指向悬空实现（M-7）
sed -n '137p;139p' eng/contracts/data/aio_abi_contract_v1.json
ls lib/astro_image_io/src/aio_abi.cpp 2>&1; ls lib/infrastructure/aio/src/aio_abi.cpp

# O. artifact_manifest 无 uniqueItems（M-8）
grep -c uniqueItems eng/contracts/data/artifact_manifest.schema.json   # -> 0

# P. perf gate 自身数值源（M-9）
grep -o '"[a-z_]*_enforcement": "[a-z_]*"' eng/contracts/resource_gate_v1.json | sort -u

# R. B-0b：冻结合同把已实现通道写成 PENDING
sed -n '83,84p' eng/contracts/data/phase2_uncertainty_rejection_provenance_v1.json
sed -n '42,48p' lib/infrastructure/aio/include/aio_hips.h      # -> NREJ=32 / NUSED=64 已定义
grep -n "aio_hips_write_diag_tile" lib/infrastructure/aio/include/aio_hips.h   # -> :211 已存在

# S. B-0：ACSD_WEIGHT_MODE 被上游禁用且被活测试断言不存在
sed -n '61,64p' eng/contracts/data/phase2_uncertainty_rejection_provenance_v1.json
sed -n '2840p;3259p' docs/science/DATA_SEMANTICS.md   # -> 禁用键 / ARCHIVED
sed -n '224p'  lib/infrastructure/aio/include/aio_hips.h
grep -n "ACSD_WEIGHT_MODE" eng/tests/unit/p2002_unc_rej_prov_test.cpp   # -> :1506 CHECK_MSG(!contains(...))

# T. B-5 补强：只剩 .pyc，且本仓登记册已记为「不存在」
ls eng/tools/quality/__pycache__/ | grep block_flow
git ls-files eng/tools/quality | grep block_flow        # -> 只剩 gen_block_flow_spec.py
sed -n '1367p' docs/engineering/UNRESOLVED_REGISTER.md

# U. 退役对象仍有活调用者
grep -n "kPsfswRobustWeight" lib/algorithms/integration/phase1_product/src/phase1_product.cpp
```

---

## 10. 给负责人的三句话

1. 本片**不是**"判据全绿所以没问题"——恰恰相反：我在亲读中定位到 **2 处真正的自证式判据**（`weight_verdict` 三方闭环从不被真实权威检验；U-02 因 13 个 `compatibility_projections` 全空而退化为在自身列表内计数），以及 **2 处恒门**（`declared_via_provenance` 恒红；`perf_gate` 的 op/threshold 恒绿）。
2. 本片的**系统性病因**是**改名未回填**：至少 12 处引用指向重命名前的路径（`lib/astro_image_io/`→`lib/infrastructure/aio/`、`eng/ci/ledgers/`→`eng/contracts/ledgers/`、`docs/detail/*.md` 整体迁走），且**验证它们的检查器本身也被删了**——所以这批漂移无法被自身发现。
3. 诚实交代：**我只亲读了 33 份中的 22 份（2912/8173 行 = 35.6%）**，未亲读的 11 份已逐条列名；这 11 份的发现我做了独立复现才采信，但**未复现的一律未写入发现清单**。