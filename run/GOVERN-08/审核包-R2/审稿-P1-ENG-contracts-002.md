# 审稿 P1 · ENG-contracts-002 · G08-05 对抗审稿第 1 遍

- 片号：`ENG-contracts-002`
- 层：`eng/contracts`
- 基线：`850a9edefd47434b9ab71bc907c3de1e0814b323`
- 日期：2026-10-02
- 姿态：红队。判据代码不作为正确性证据；一切结论来自本人读完原文 + 独立重算 + 构造反例。

---

## 1. 读完了吗

| 口径 | 数值 |
|---|---|
| 成员份数（片清单权威版） | 28 |
| 实际读完份数 | **28** |
| 成员总行数（片清单 `实际行数`） | 8158 |
| 实测行数（`wc -l` 逐份） | **8158**（与清单一致） |
| 实际读了多少行 | **8158** |
| **覆盖率** | **28/28 份 = 8158/8158 行 = 100%** |

**未读完的：无。** 28 份成员全部由本人用 `read` 工具逐行读完（含本片最大一份 4050 行的 `product_family_field_constraints.schema.json`，分 4 段读完）。

`eng/contracts/schemas/product_family_field_constraints.schema.json` 占本片 4050/8158 = **49.6%**，已完整通读，未抽样。

---

## 2. 本片判定：**需修**（含 3 条阻断级事实，集中在「退役声明不实」与「门宣称但结构上不可能翻红」）

### 最重的 3 条

1. **【阻断】退役对象 `psfsw_robust_weight` 的「已删除」声明为假，且生产实现仍活**
   `port_contract.schema.json:22` 明写「canonical schema 与**example 已删除**」，`unified_object_compatibility_map_v1.json:142` 明写该 token「**仅存于** clause_registry.json」。两份声明都被本片成员自身证伪，且生产代码仍在调用。

2. **【阻断】`product_family_field_constraints.schema.json` 的四条量纲/互逆门结构上不可能翻红**
   `G-WINFO-UNIT` / `G-Q-UNIT` / `G-FLUX-UNIT` / `G-WINFO-VAR-INVERSER` 的「期望量」只写在 `x-acsd` 注解里，实际 `$defs/quantity.units` 是裸 `{"type":"string"}`（`:2129-2131`）。其中 `Var(F_hat)==1/W_info` 是负责人点名的**代数恒等式型**自洽断言——schema 只宣称、不检查。

3. **【阻断】`config_separation_anchors.json:220-223` 的 `machine_assertions` 指向两个恒真门**
   两条判据只校验本登记表自己的字面量自洽，从不读任何真实 config schema。（**公平修正**：真门确实存在于 `test_cfg001_contracts.py:130-166` 与 `gate_assertions.py:91-93`，但登记面没有列它们——缺陷是**指错**，不是「无门」。）

---

## 3. 逐文件清单（28 份）

| # | 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `schemas/product_family_field_constraints.schema.json` (4050) | 全 4 段通读 | 10 个产品族 `$defs`；大量 `x-acsd.gate` 只作注解不作约束；`quantity.units` 裸 string；`:3258-3267` 死约束 | **阻断** B2 |
| 2 | `schemas/unified/port_contract.schema.json` (597) | 全读 | 13 对象枚举；N4/N5 的真实判红条款；`:22` 退役迁移提示 | **阻断** B1 |
| 3 | `schemas/unified/ivar.schema.json` (498) | 全读 | `required` 含 `ivar_value` 但字段说「二者至少其一」；顶层 integrated_flux 门死分支 | 须修 M6 / M8 |
| 4 | `anchors/anchor_contract.json` (466) | 全读 | 29 resolvers + 4 exemptions + 41 bindings；1 条 resolver 指向不存在文档；4 条 exemption 全空转 | **阻断** M1 / M2 |
| 5 | `data/unified_object_compatibility_map_v1.json` (381) | 全读 | 9 活跃条目 + 1 retired + 7 legacy；2 条条目指向已删文件；policy 反称「仍在用」 | **阻断** B1 / M3 |
| 6 | `schemas/unified/source_snr.schema.json` (329) | 全读 | 无顶层 `allOf`；`units.allOf` 与 ivar 同构 | 参考 |
| 7 | `schemas/unified/point_information.schema.json` (320) | 全读 | `bunit` 钉死 `ADU^-2`、`pixel_semantics` 钉死 `dimensionless`，3 条 units 门 2 死 1 致枚举不可用 | 须修 M7 |
| 8 | `data/phase_product_exchange.schema.json` (247) | 全读 | role↔type_id 有实现；role↔format **无**；status=COMPLETE **无**；`role_binding` 死定义 | 须修 M12 |
| 9 | `data/config_separation_anchors.json` (224) | 全读 | `machine_assertions` 指两个恒真门；三类字段名表；`phase_config.schema.json` 不存在 | **阻断** B3 |
| 10 | `data/examples/psfsw.example.json` (134) | 全读 | 退役对象的**活正例**：`"weight_mode":"psfsw_robust"`、`"kind":"psfsw_robust_weight"` | **阻断** B1 |
| 11 | `data/examples/units.example.json` (123) | 全读 | 单位表；`:76`/`:120` 仍带 `psfsw_robust_weight` | 阻断佐证 |
| 12 | `config/module_lifecycle_contract.schema.json` (95) | 全读 | **不是 JSON Schema**（无 `$schema`/`type`/`properties`）；数据实例 | 须修 M10 |
| 13 | `ledgers/ledger_schema.py` (76) | 全读 | 5 道形态门，全部 `raise`（fail-closed 成立）；零恒真/零恒红/零自愈；`read_json` 死 label | 通过（附注） |
| 14 | `schemas/dual_line_file_domain.schema.json` (62) | 全读 | draft-07；`x-doc` 指针未见问题 | 通过 |
| 15 | `schemas/traceability_matrix.schema.json` (61) | 全读 | 八层追溯，`minLength:1` 禁空串 | 通过 |
| 16 | `data/examples/covariance.example.json` (51) | 全读 | `forbidden_variance_sources` 含 `psfsw_robust_weight` | 阻断佐证 |
| 17 | `data/examples/phase1_product_v1.example.json` (44) | 全读 | `status:"COMPLETE"`、`format:"hips"` 合法 | 通过 |
| 18 | `data/examples/phase2_mosaic_v1.example.json` (44) | 全读 | 同上 | 通过 |
| 19 | `negative/n5_...psfsw_robust_weight.schema-violation.json` (43) | 全读 | 退役对象接端口；4 处 enum 判红，**是真负例** | 通过 |
| 20 | `negative/n4_source_snr_into_variance_port.schema-violation.json` (42) | 全读 | 被 `allOf[1]` 的 `const` 判红（非 enum），**是真负例** | 通过 |
| 21 | `schemas/task_result.schema.json` (41) | 全读 | `task_id` 模式 `^[A-Z0-9]+-[0-9]{3}$` 收不下 `SCI-ANCHOR-001` 这类两段 ID | 建议 |
| 22 | `schemas/unified/examples/depth_m5.example.json` (36) | 全读 | 合规 | 通过 |
| 23 | `schemas/unified/examples/source_snr.example.json` (36) | 全读 | 合规 | 通过 |
| 24 | `config/cli_modules_list.schema.json` (35) | 全读 | 退役命令的孤儿合同，零生产者 | 须修 M11 |
| 25 | `schemas/unified/examples/signal.example.json` (35) | 全读 | 合规 | 通过 |
| 26 | `schemas/unified/examples/support.example.json` (34) | 全读 | 合规（`missing_repr:"zero"`） | 通过 |
| 27 | `data/examples/point-information.example.json` (29) | 全读 | 与 `$defs/point_information` 口径一致 | 通过 |
| 28 | `data/examples/signal.example.json` (25) | 全读 | 与 `unified/examples/signal` 重复变体 | 建议（重复登记） |

---

## 4. 发现清单

### 阻断

**B1. 退役对象 `psfsw_robust_weight`：「已删除」「仅存于」两处声明均为假，且生产实现仍活**

- `port_contract.schema.json:22`：「canonical schema 与 **example 已删除**，不存在等价替代对象」
- `unified_object_compatibility_map_v1.json:142`：「v6 词表 token psfsw_robust_weight **仅存于** v6 设计档案（clause_registry.json#weight_vocabulary）」

证伪（本片成员自身即反证）：
- `eng/contracts/data/examples/psfsw.example.json:4` `"weight_mode": "psfsw_robust"`、`:6` `"kind": "psfsw_robust_weight"` —— **文件在位，134 行完整正例**
- `data/examples/units.example.json:76` `"symbol": "psfsw_robust_weight"`、`:120` `"psfsw_robust_weight": 0`
- `data/examples/covariance.example.json:45` 在 `forbidden_variance_sources` 里

生产活调用者（**「退役对象仍有活调用者」检查项命中**）：
- `lib/algorithms/integration/phase1_product/src/phase1_product.cpp:431` → `p1psfw::compute_psfsw_weights({cv})`
- `lib/algorithms/photometry/cpp/src/psfsw.cpp:330`（实现）、`lib/algorithms/photometry/include/acsd/psfsw.h:171`（声明）

判据：canonical schema 确已删除（`git ls-files | grep psfsw` 无 schema 文件）——这一半为真；**「example 已删除」与「仅存于」为假**。

**B2. `product_family_field_constraints.schema.json` 四条量纲/互逆门结构上不可能翻红**

| 门 | 登记位置 | 声称规则 | 实际约束 |
|---|---|---|---|
| `G-WINFO-UNIT` | `:1836-1842` | `units != ADU^-2 -> REJECT` | `W_info` → `$ref:#/$defs/quantity`（`:1835`）；`quantity.units` = `{"type":"string"}`（`:2129-2131`），**无 enum/const** |
| `G-Q-UNIT` | `:1844-1852` | `units != ADU^-1 -> REJECT` | 同上 |
| `G-FLUX-UNIT` | `:1854-1862` | `units != ADU -> REJECT` | 同上 |
| `G-WINFO-VAR-INVERSER` | `:1864-1872` | `Var(F_hat) != 1/W_info -> REJECT` | **完全无对应约束**；`W_info` 与 `flux_variance` 是两个独立 `type:number` |

⇒ 写 `W_info.units = "banana"`、`flux_variance.value = 1e9`（而 `W_info.value=0.0125`，二者互逆关系早已破坏）都能通过本 schema。

`G-WINFO-VAR-INVERSER` 是本片**最典型的「自洽式断言」**：用「Var 是 W 的逆函数」这条定义式本身当期望量，而判据体根本不存在。

同类恒真死约束（登记为检查、实为空转）：
- `:3258-3267` `pixel_area_power: {"type":"integer","allOf":[{"not":{"const":null}}]}` —— `type:"integer"` 已排除 `null`，`not:{const:null}` **结构上不可能失败**。

**B3. `config_separation_anchors.json:220-223` 的 `machine_assertions` 指向两个恒真门**

实现（`eng/tests/contracts/test_unified_object_contract.py:660-687`）：
- `test_field_name_sets_are_pairwise_disjoint`：只遍历本文件自己写的 `spec["examples"]` 字面量，验证「我写的例子匹配我写的 regex」。
- `test_hardware_names_forbidden_in_phase_config`：`assertIn(bad, phase["forbidden_field_names"])`，而 `:53-64` 与 `:191-201` 是**同一份字面量列表的复制** ⇒ 除非有人只改其中一处，永不翻红。
- `:65` 宣称的「phase_config 出现 cpu_profile 字段即 REJECT」在这两条判据下**无任何判据能因违反它而翻红**。

**公平修正（本人主动降级）**：真正读真实 schema 的判别门存在——
`eng/tests/config/test_cfg001_contracts.py:130-166` `TestCrossClassDisjointness.setUp` 真读 `PHASE_SCHEMAS`/`CPU_SCHEMA`/`MANIFEST_SCHEMA` 的 `property_names`；`eng/tests/config/gate_assertions.py:91-93` 门 G4 同。**但这两条不在 `machine_assertions` 里。** ⇒ 缺陷定性为「登记面指错 + 弱判据被冒充为强判据」，不是「分离规则无门」。

### 须修

**M1. `anchor_contract.json` 4 条 exemptions 全部空转**（子代理提出，本人独立复核成立）
- `:186-192`/`:194-200`/`:202-208`：raw `integrate.cpp:10-79`、`integrate.h:1-75` 在 `docs/science/algorithms/PHASE2_INTEGRATION.md` 中**零命中**（`grep -n "integrate\.\(cpp\|h\):[0-9]"` 无输出）。
- `:210-216`：raw `photutils/utils/errors.py:91-92` 实际位于 `docs/science/NOISE_MODEL.md:393`，evidence 写 `:204`（**偏 189 行**）。
- reason 自述「实际 76 行」「实际 74 行」；实测 `lib/algorithms/coverage/src/integrate.cpp` = **89 行**、`.../integrate.h` = **83 行**。

**M2. resolver R09 消歧恰好选反 + 连带悬空 binding**（子代理提出，本人独立复核成立）
- `anchor_contract.json:63-68` 把 `STAR_DETECTION_ALGORITHMS.md` 的裸 `star_detector.h` 解析到 `wrapper_phase1/star_detector.h`。
- 该文档唯一裸锚在 `:239`，句子讲「现行 **SDET_EXPORT** 面一致」。实测：`include/star_detector.h` SDET_EXPORT=8 / StarSource=0；`wrapper_phase1/` SDET_EXPORT=0 / StarSource=2 ⇒ 应解析到 `include/`，**R09 选反**。
- 连带 binding `STARDET-SRCS`（`:447-452`）把符号 `StarSource` 绑到该文档，但该文档对 `StarSource` **全篇 0 命中** ⇒ 悬空 binding。
- 旁证：同文件 `:123-128` 已正确登记 `PUBLIC_API.md → include/star_detector.h`，理由明写「wrapper_phase1/star_detector.h 无这些符号」。

**M3. 兼容映射 6 处悬空路径**（本人先发现，子代理复核一致）
- `unified_object_compatibility_map_v1.json:97` `eng/contracts/data/v6_data_dictionary_v1.json` **不存在**
- `:108` `eng/contracts/data/v6_migration_map_v1.json` **不存在**
- 且 policy `:7` 反称「v6 生成物与 runtime artifact_store 校验器**仍在用它们**」⇒ 已删文件被登记为活依赖。
- `old_contract_face` 6 处指向已不存在的文档：`:169`/`:200` `docs/detail/calibration.md`、`:274` `docs/detail/healpix_drizzle.md`、`:323` `docs/detail/phase2_samp.md`、`:324` `docs/detail/phase2_upm.md`、`:352` `docs/detail/gaia_xpsd_client.md`（registry 化后未同步）。

**M4. `anchor_contract.json:148-152` resolver 指向不存在的文档 `docs/detail/photometric_calib.md`**
全仓无此文件（现存为 `docs/detail/registry/acsd.phase1.photometry.md`）。同 basename+path 已由 `:136-140` 正确登记 ⇒ 半迁移残留。

**M5. `anchor_contract.json:4`「同目录 ANCHOR_CONTRACT.md」为伪指**
实际在 `docs/detail/anchors/ANCHOR_CONTRACT.md`，**不在** `eng/contracts/anchors/`（该目录只有 `anchor_contract.json` + `unresolved_registry.json`）。属「伪引」——逐字引用了「同目录」而事实相反。

**M6. `ivar.schema.json:457-496` 顶层门为死分支**
注册门「`declared_via_provenance + integrated_flux ⇒ pixel_area_power=0`」，但嵌套 `units.allOf[1]`（`:142-166`）已规定 `bunit_semantics=declared_via_provenance` 时 `pixel_semantics` **必为** `surface_brightness` ⇒ `if` 条件永假 ⇒ **门结构上不可触发**（`source_snr` 无顶层 allOf，故此死分支仅存在于 ivar）。

**M7. `point_information.schema.json` 三条 units 门：两死一废**
- `bunit` = `const "ADU^-2"`（`:81`）、`pixel_semantics` = `const "dimensionless"`（`:92`）。
- `units.allOf[0]`（`:108-132`）`if bunit pattern "/sr"` — `"ADU^-2"` 永不含 `/sr` ⇒ **死分支**。
- `units.allOf[2]`（`:158-176`）`if pixel_semantics=="integrated_flux"` — const `dimensionless` ⇒ **死分支**。
- `units.allOf[1]`（`:133-157`）强制 `declared_via_provenance ⇒ surface_brightness`，与 `:92` 的 const 冲突 ⇒ 枚举值 **`declared_via_provenance`（`:88` 宣告合法）对 point_information 完全不可用**。

**M8. `ivar.schema.json:25` 与 `:305` 自相矛盾**
`required` 无条件要求 `ivar_value`；`:304-308` 却声明 `ivar_plane_ref`「与 ivar_value **二者至少其一**」。后者被架空 ⇒ 纯平面 ivar 文档必被拒，而字段描述说它合法。

**M9. `test_abi002_lifecycle.py` 权威源侧零强制**（子代理提出，本人独立复核成立）
`test_30`（`:283-288`）/ `test_31`（`:290-308`）都执行 `schema, text = self._load()`，但 **`text` 绑定后从未使用**；期望量是测试内硬编码 `STATES`/`OPS`/`expect_to`。断言消息写「schema states != **头枚举**」「与**头文件**转移表不一致」，实际比的是测试字面量。⇒ 把 `lifecycle_v1.h:121` 的 `ACS_LC_STATE_EXECUTING = 2` 改成 `5`，全套仍绿。

**M10. `module_lifecycle_contract.schema.json` 不是 JSON Schema 却登记为 schema**
顶层键全为数据（`contract_schema`/`contract_id`/`states`/`transitions`/…），**无 `$schema`/`type`/`properties`/`required`**；同目录 `cli_modules_list`/`cli_selftest`/`module_dll_contract` 三份是真 JSON Schema。任何按「该目录 = JSON Schema」加载的通用校验器会因「无 type 约束」对任意实例**恒真**。登记册 `config_registry.json:2495` 与 `CONFIG_CONTRACT.md:223` 宣称「全为 schema」。

**M11. `cli_modules_list.schema.json` 是退役命令的孤儿合同**
`acsd_modules_list` 全仓只命中 schema 自身 `:11`，零生产者；`test_cli_v7_surface.py:37` 把 `["modules","list","--json"]` 列入 `LEGACY_SURFACE`，`:98` 断言必须 `rc=2`（退役）。

**M12. `phase_product_exchange.schema.json` 三处「宣称但未实现」**
- `:229` 宣称「format 必须与 product_role 匹配：phase1/phase2 → hips；phase3 → fits」——`geometry.format` 仅有 `enum:["hips","fits"]`（`:203`），**无任何 if/then 绑定**（`:101-128` 只绑 role↔type_id）⇒ phase3 产品声明 `format:"hips"` 可过。
- `:47` 宣称「交换资格要求 status=COMPLETE」——`:86` 的 enum 含 `FAILED`/`CANCELLED`/`PENDING`，**无 `const:COMPLETE`** ⇒ FAILED 产物可通过 schema 层。
- `$defs/role_binding`（`:129-153`）**零 `$ref`**（全文仅 `:97`/`:234` 两处 `$ref`，指向 `product_content`/`plane`）⇒ 死定义；即便生效，`role` 与 `type_id` 是两个独立 enum，允许 3×3 叉积，与 `:131`「一一对应」自述矛盾。

### 建议

- `task_result.schema.json:10` `task_id` 模式 `^[A-Z0-9]+-[0-9]{3}$` 收不下 `SCI-ANCHOR-001` 这类两段 ID（本仓确有此类 ID）。
- `data/examples/signal.example.json`（25 行）与 `schemas/unified/examples/signal.example.json`（35 行）内容重复，同一对象两个正例登记。
- `config_separation_anchors.json:16` `schema_path_reserved` = `eng/contracts/schemas/phase_config.schema.json` **不存在**（另两个类均存在）。
- `anchor_contract.json:184-217` 中 `:186-192` 与 `:194-200` 是**完全重复**的 exemption（同 doc + 同 raw + 同 evidence）。

---

## 5. 我主动构造的反例

| # | 构造 | 想推翻什么 | 结果 |
|---|---|---|---|
| C1 | 取 `eng/contracts/data/examples/psfsw.example.json`，逐字比对 `port_contract.schema.json:22` 的「example 已删除」 | 退役声明为真 | **推翻成功**。文件在位 134 行，`:4`/`:6` 携带退役 token |
| C2 | 取 `units.example.json:76`/`:120`、`covariance.example.json:45`，比对 compat map `:142` 的「仅存于」 | 退役 token 只在 clause_registry | **推翻成功**。本片 3 份成员即反证 |
| C3 | 构造 `point_information` 文档：`bunit_semantics="declared_via_provenance"`（`:88` 宣告合法） | 该枚举值可用 | **推翻成功**。`:88` 允许，但 `:92` const dimensionless 与 `:144-156` 冲突 ⇒ 必拒 |
| C4 | 构造 `provenance` 文档：`units.pixel_area_power = null` | `:3260-3266` 的 `not:{const:null}` 会判红 | **推翻失败（该门恒真）**。`:3259` 的 `type:"integer"` 先拒 null，`not` 分支结构上不可达 |
| C5 | 构造 `point_information` 文档：`W_info.units="banana"`, `flux_variance.value=1e9`（与 `W_info.value=0.0125` 互逆关系已破） | `G-WINFO-UNIT`/`G-WINFO-VAR-INVERSER` 会 REJECT | **推翻成功**。`:2129-2131` `units` 裸 string，两个 value 无跨字段约束 ⇒ 全绿 |
| C6 | 取 `STAR_DETECTION_ALGORITHMS.md`，grep 裸 `star_detector.h` + 比对 SDET_EXPORT/StarSource 在两个候选文件的命中 | R09 选 `wrapper_phase1/` 正确 | **推翻成功**。`:239` 讲 SDET_EXPORT（include 8 : wrapper 0）⇒ 选反 |
| C7 | 取 `PHASE2_INTEGRATION.md` grep `integrate.cpp:N`；取 `NOISE_MODEL.md:393` | 3 条 exemption 的 raw 在文档中存在、evidence 行号正确 | **推翻成功**。raw 零命中；evidence 偏 189 行 |
| C8 | 检查 `module_lifecycle_contract.schema.json` 顶层键 | 它是 JSON Schema | **推翻成功**。无 `$schema`/`type`/`properties`/`required` |
| C9 | 检查 `$defs/role_binding` 的 `$ref` 计数 | 它参与校验 | **推翻成功**。零 `$ref`，死定义 |
| C10 | 检查 `phase_product_exchange.schema.json` 中 role↔format 与 status=COMPLETE 的 `if/then` 或 `const` | 两项约束已实现 | **推翻成功**。均只有散文声明，无实现 |
| C11 | **自证：尝试把 lifecycle 缺失转移当阻断**（`transitions` 无 `EXECUTING→CREATED`） | 状态机不可回到 CREATED | **推翻失败，已否决**。`lifecycle_v1.h:155-157` 显式写「本判定为纯函数, **不含该完成转移**」，头文件 `:31-32` 的转移图明写 `EXECUTING --(完成)--> CREATED`。⇒ 我的初判错，**不计入发现** |
| C12 | 检查 `test_abi002_lifecycle.py:284/291` 的 `text` 变量是否被使用 | 头文件权威源受检 | **推翻成功**。`text` 绑定后从未使用 |

---

## 6. 盲复算

遮住既往结论，独立重新取证（不查任何既往审稿件）后：

| 项 | 既有口径 | 本人盲复算 | 判定 |
|---|---|---|---|
| N4 是真负例 | — | `allOf[1]` 的 `const` 判红（非 enum） | **一致** |
| N5 是真负例 | — | 4 处 enum 判红 | **一致** |
| 负样本判据存在 | — | `test_unified_object_contract.py:98,492` 有消费者 | **一致**（子代理补充：HEAD 上无 CI 执行面） |
| 三类配置分离有真门 | — | `test_cfg001_contracts.py:130-166` + G4 | **偏严→修正**：我最初据 `machine_assertions` 判「无门」，盲复算发现真门存在，故把 B3 从「阻断」改述为「登记面指错」 |
| lifecycle 状态机缺转移 | （我初判） | 头文件明写该转移不在本表范围 | **推翻自己，已否决（C11）** |
| 量纲门无判别力 | — | `quantity.units` 裸 string，四门恒绿 | **一致，且为本片最强新发现** |

**结论：盲复算整体与独立读原文所得一致；唯一修正来自我自己——把 lifecycle 候选项否决。**

---

## 7. 子代理派发记录

**派出 5 个**（纪律要求 3–5）：

| ID | 任务 | 状态 |
|---|---|---|
| `5f6825e0` | 审 `product_family_field_constraints.schema.json` | 未回（截止本件写作） |
| `995d1e01` | 同上（**派单重复，笔误**） | 未回（截止本件写作） |
| `32d899d1` | 审 N4/N5/EXPECTED 及其执行器 | 已回 |
| `44f05c94` | 审 anchor/对象映射/配置分离/交换合同 | 已回 |
| `83cf8029` | 审 `ledger_schema.py` + lifecycle + cli_modules_list | 已回 |

**诚实声明**：5 个派出中 3 个回执；其中 2 个（`5f6825e0`/`995d1e01`，同一任务重复派发）**未回**。故本片最大一份文件（4050 行，占 49.6%）的核验**由本人亲自通读承担，未依赖子代理**。

### 逐条复核与否决

| 子代理结论 | 本人复核 | 处置 |
|---|---|---|
| `44f05c94` D1：v6 两文件已删而 policy 称「仍在用」 | ✅ 独立复现（`git ls-files` 零命中） | **采纳** → M3 |
| `44f05c94` D2：4 条 exemptions 全悬空、evidence 偏 189 行、reason 行数错 | ✅ 独立复现（grep 零命中；393 vs 204；89/83 vs 76/74） | **采纳** → M1 |
| `44f05c94` D3：R09 消歧选反 + `STARDET-SRCS` 悬空 | ✅ 独立复现（SDET_EXPORT 8:0 / StarSource 0:2） | **采纳** → M2 |
| `44f05c94` D8：config 分离两条判据只校验登记表自洽 | ✅ 复核成立，**但本人主动降级** | **部分采纳** → B3（补公平修正：真门存在于 test_cfg001_contracts.py） |
| `44f05c94` D7：`$defs/role_binding` 死定义 | ✅ 独立复现（零 `$ref`） | **采纳** → M12 |
| `44f05c94` D5：`examples/` 登记称「10 个对象族」实有 15 个 | ⚠️ 部分复核（结构上 `covers_object` 只允许单值，登记不完整成立）；未逐个点数 | **采纳为建议**，未升级 |
| `44f05c94` 「resolvers/bindings 纯数据无消费者」 | ⚠️ 未复核 | **不采纳**（未验证） |
| `83cf8029` D6：`test_30/test_31` 的 `text` 未使用，权威源零强制 | ✅ 独立复现（源码逐行） | **采纳** → M9 |
| `83cf8029` D5：lifecycle 合同不是 JSON Schema | ✅ 本人读原文即确认 | **采纳** → M10 |
| `83cf8029` D7：`cli_modules_list` 是退役命令孤儿合同 | ✅ 独立复现（`LEGACY_SURFACE`） | **采纳** → M11 |
| `83cf8029` D1：消费检测器命名空间盲（`rotation_deg`/`precision`） | ⚠️ 未逐行复核（涉及 `lib/**` 与模板，超本片） | **不采纳**，转交他片 |
| `83cf8029` D4：10 处 `eng/ci/ledgers/` 悬空路径 | ⚠️ 未复核 | **不采纳**，转交他片 |
| `32d899d1` 「N4 由 const 而非 enum 判红」 | ✅ 本人独立读 `port_contract.schema.json:164-199` 得出同一结论 | **采纳**（确认 N4/N5 均为真负例） |
| `32d899d1` 「HEAD 上无 CI 执行面，判据只能人工跑」 | ⚠️ 未复核 `.github`/`eng/ci` | **不采纳**（未验证） |
| **本人自建** C11 lifecycle 状态机 | ✅ 复核 | **否决自己的初判**，已从发现清单移除 |

**否决/不采纳合计 6 条**（4 条未复核 + 1 条降级 + 1 条否决自己）。

---

## 8. 恒真门三型 · 双向体检（本片专项）

| 型 | 结论 | 证据 |
|---|---|---|
| 代数恒等式型 | **命中 2 处** | `product_family_field_constraints.schema.json:1864-1872` `G-WINFO-VAR-INVERSER` 宣称 `Var=1/W` 但无实现；`ivar.schema.json:457-496` 顶层门死分支 |
| 结构对称型 | 未发现严格意义命中 | — |
| 往返自证型 | **命中 1 处** | `config_separation_anchors.json:220-223` → `test_field_name_sets_are_pairwise_disjoint`（自己验自己） |
| 恒红门 | **未发现** | 逐条核对本片全部 `if/then`、`const`、`enum`；无「两量逐位相同却判 ≤ 阈值」 |
| 自愈判据 | **未发现** | 无判据读取会被自身复现动作覆写的文件 |
| fail-closed 伪装（读到缺失就判绿） | **未发现** | `ledger_schema.py:39,43,47,51,56` 五道门全部 `raise`，`__main__:73-75` 转 `SystemExit(1)` |
| 筛掉真信号 | 未发现 | — |
| 判据读的是桩 | 未发现 | `ledger_schema.py` 读真实 JSON |
| 恒真死约束 | **命中 1 处** | `product_family_field_constraints.schema.json:3258-3267` `{"type":"integer","allOf":[{"not":{"const":null}}]}` |
| 登记但无判据（恒绿） | **命中 4 处** | G-WINFO/Q/FLUX-UNIT、G-WINFO-VAR-INVERSER、ivar 顶层 integrated_flux 门、point_information 两条 units 门 |

---

## 9. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线与本片分母
git -c core.quotepath=false rev-parse HEAD          # 850a9edefd47434b9ab71bc907c3de1e0814b323
sed -n '1006,1041p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml   # 成员清单 28 份 / 8158 行

# B1 退役声明不实（两份证伪）
grep -n 'example 已删除' eng/contracts/schemas/unified/port_contract.schema.json      # :22
grep -n '仅存于' eng/contracts/data/unified_object_compatibility_map_v1.json            # :142
ls -l eng/contracts/data/examples/psfsw.example.json                                    # 在位 134 行
grep -n 'psfsw_robust' eng/contracts/data/examples/psfsw.example.json \
                  eng/contracts/data/examples/units.example.json \
                  eng/contracts/data/examples/covariance.example.json
grep -n 'compute_psfsw_weights' lib/algorithms/integration/phase1_product/src/phase1_product.cpp  # :431 生产活调用

# B2 量纲门恒绿
sed -n '1834,1873p;2118,2140p' eng/contracts/schemas/product_family_field_constraints.schema.json
#   → :1835/:1845/:1855/:1865 全是 $ref quantity；:2129-2131 units 仅 {"type":"string"}
sed -n '3256,3268p' eng/contracts/schemas/product_family_field_constraints.schema.json     # 恒真死约束

# B3 config 分离恒真门 + 真门修正
sed -n '220,223p' eng/contracts/data/config_separation_anchors.json
sed -n '660,688p' eng/tests/contracts/test_unified_object_contract.py                     # 弱门
sed -n '130,167p' eng/tests/config/test_cfg001_contracts.py                               # 真门
sed -n '88,94p'   eng/tests/config/gate_assertions.py                                     # 门 G4

# M1/M2 anchor_contract
grep -n 'integrate\.\(cpp\|h\):[0-9]' docs/science/algorithms/PHASE2_INTEGRATION.md || echo "ZERO HITS"
sed -n '393p' docs/science/NOISE_MODEL.md
wc -l lib/algorithms/coverage/src/integrate.cpp lib/algorithms/coverage/include/astro/phase2/integrate.h
sed -n '239p' docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md
for f in lib/algorithms/star_detection/include/star_detector.h \
         lib/algorithms/star_detection/wrapper_phase1/star_detector.h; do
  printf '%s SDET_EXPORT=%s StarSource=%s\n' "$f" "$(grep -c SDET_EXPORT $f)" "$(grep -c StarSource $f)"; done
grep -c StarSource docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md      # 0

# M3 兼容映射悬空
ls eng/contracts/data/v6_data_dictionary_v1.json eng/contracts/data/v6_migration_map_v1.json 2>&1 | tail -2
git -c core.quotepath=false ls-files eng/contracts/data/ | grep -c v6_

# M4/M5 anchor 悬空与伪指
test -e docs/detail/photometric_calib.md || echo "MISSING photometric_calib.md"
ls eng/contracts/anchors/                      # 无 ANCHOR_CONTRACT.md
ls docs/detail/anchors/                        # 在此

# M6/M7 死分支
sed -n '142,166p;457,496p' eng/contracts/schemas/unified/ivar.schema.json
sed -n '80,93p;107,177p' eng/contracts/schemas/unified/point_information.schema.json

# M9 lifecycle 权威源零强制
sed -n '283,308p' eng/tests/abi/test_abi002_lifecycle.py   # text 绑定后未使用
sed -n '151,157p' lib/include/acsd/abi/lifecycle_v1.h       # 明写不含完成转移（C11 否决依据）

# M10/M11 lifecycle 非 schema、cli 孤儿
head -12 eng/contracts/config/module_lifecycle_contract.schema.json   # 无 $schema/type/properties
grep -n 'modules", "list' eng/tests/cli/test_cli_v7_surface.py
grep -rn 'acsd_modules_list' -- . ':!run/GOVERN-08'    # 只命中 schema 自身 :11

# M12 交换合同死定义与未实现约束
grep -n '\$ref' eng/contracts/data/phase_product_exchange.schema.json   # 仅 :97/:234，role_binding 零引用
grep -n 'COMPLETE' eng/contracts/data/phase_product_exchange.schema.json # :47 散文 + :86 enum，无 const
sed -n '198,230p' eng/contracts/data/phase_product_exchange.schema.json # format 无 role 绑定

# 覆盖自查：28 份行数合计
while read -r f; do wc -l < "$f"; done <<'EOF' | awk '{s+=$1} END{print "TOTAL",s}'
eng/contracts/schemas/product_family_field_constraints.schema.json
eng/contracts/schemas/unified/port_contract.schema.json
eng/contracts/schemas/unified/ivar.schema.json
eng/contracts/anchors/anchor_contract.json
eng/contracts/data/unified_object_compatibility_map_v1.json
eng/contracts/schemas/unified/source_snr.schema.json
eng/contracts/schemas/unified/point_information.schema.json
eng/contracts/data/phase_product_exchange.schema.json
eng/contracts/data/config_separation_anchors.json
eng/contracts/data/examples/psfsw.example.json
eng/contracts/data/examples/units.example.json
eng/contracts/config/module_lifecycle_contract.schema.json
eng/contracts/ledgers/ledger_schema.py
eng/contracts/schemas/dual_line_file_domain.schema.json
eng/contracts/schemas/traceability_matrix.schema.json
eng/contracts/data/examples/covariance.example.json
eng/contracts/data/examples/phase1_product_v1.example.json
eng/contracts/data/examples/phase2_mosaic_v1.example.json
eng/contracts/schemas/unified/negative/n5_retired_psfsw_robust_weight.schema-violation.json
eng/contracts/schemas/unified/negative/n4_source_snr_into_variance_port.schema-violation.json
eng/contracts/schemas/task_result.schema.json
eng/contracts/schemas/unified/examples/depth_m5.example.json
eng/contracts/schemas/unified/examples/source_snr.example.json
eng/contracts/config/cli_modules_list.schema.json
eng/contracts/schemas/unified/examples/signal.example.json
eng/contracts/schemas/unified/examples/support.example.json
eng/contracts/data/examples/point-information.example.json
eng/contracts/data/examples/signal.example.json
EOF
```

---

## 10. 未核实 / 超出本片

- 全部「会红/会绿」结论均为**静态代码路径与 schema 语义推导**；本轮**未执行任何测试、编译或脚本**（纪律要求）。若门当前为绿，证明的是断言缺失而非环境问题。
- `83cf8029` 提出的 `rotation_deg`/`precision` 命名空间盲消费检测器（涉 `lib/**` 与 `eng/packaging/config/templates/**`）、10 处 `eng/ci/ledgers/` 悬空路径，均**超出本片成员**，本轮未复核，建议转相关车道。
- `44f05c94` 称 resolvers/bindings 无机器消费者、basename 歧义度（不排除 `run/` 快照时 `build.ps1` 达 78 个），未复核。
- `dead_config_keys.json`、`variance.schema.json` 全文、`n1`/`n3`/`n6` 负样本、`product_family/` 子套件当前状态，均不在本片或本轮未读。