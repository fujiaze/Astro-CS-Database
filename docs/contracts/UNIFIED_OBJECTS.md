# 统一数据对象合同（UNIFIED_MODEL §2 的 **13** 个对象）

> 上游：ASTROCS_DESIGN.md §3.1（数据对象）

> **退役与身份（DOC-203 / Q2 前置裁决 2026-09-20；GAP_AUDIT §4.1/§4.5）**
> ① canonical 数据对象 = **13 个**（原 14 个）；`psfsw_robust_weight` **已真删**（变更 claim `CHG-2026-09-20-PSFSW-RETIRE`；退役记录
> `eng/contracts/data/unified_object_compatibility_map_v1.json#retired_entries`）。上位正本 =
> `docs/design/UNIFIED_MODEL.md` §2。
> ② **V6 合同层的语义已自解释合并进现行合同链**（`CHG-2026-09-22-V6-CONTRACT-MERGE`）：条款注册表见
> `docs/contracts/DATA_SEMANTICS.md` §31.10，字段级判据落点见本文 §4；生效与退役条件由**变更编号**决定，
> **不得**用版本号窗口表达（`ASTROCS_DESIGN.md` §12）。
> ③ v6 内的 `weight_mode` 家族已按 §9.73 A44 作废（作废键面：删键 / 改写 / 加作废留痕），作废留痕见
> `docs/contracts/DATA_SEMANTICS.md` §31.3。

- 性质：**索引与语义登记**。本文不改任何科学定义、公式、阈值、容差或推导；对象身份/单位/无效值/精度/可否作权重一律以 canonical schema 为准。

## 1. 唯一事实源声明（U-02 口径）

`eng/contracts/schemas/` 是数据合同的**唯一事实源**（ENGINEERING_SPEC §4.7）。对 UNIFIED_MODEL §2 的 **13** 个对象（\(psfsw_robust_weight\) 已于 2026-09-20 退役，见文首）：

```text
canonical 定义 = eng/contracts/schemas/unified/<对象名>.schema.json
                $id = https://astrocs.local/schemas/unified/<对象名>/v1   （每对象恰 1 个，全局唯一）
其它 schema     = 只能是「产品族专用投影」或「兼容期映射」，必须在
                  docs/contracts/unified_object_registry.json#canonical_object_classes 登记归属，
                  且不得与 canonical 重复定义对象判别字段（除 schema_version 外 required/properties 词表不得重叠）。
```

**U-02 结论（唯一性判定标准）**：单一合同链的唯一性判定标准：对 UNIFIED_MODEL §2 的 13 个对象，eng/contracts/schemas/unified/<对象名>.schema.json 是唯一 canonical 定义（每对象恰 1 个 $id，全局不重复）；任何其它 schema 只能是「产品族专用投影」或「兼容期映射」，不得与 canonical 重复定义对象判别字段（除 schema_version 外 required/properties 词表不得重叠），且 eng/contracts/schemas/** 下每个 schema 文件都必须在 docs/contracts/unified_object_registry.json 的 canonical_object_classes 中被登记归属。

机器可复跑断言（`python3 -m unittest discover -s eng/tests/contracts -t eng/tests/contracts`）：

- `eng/tests/contracts/test_unified_object_contract.py::TestCanonicalObjectSchemas::test_every_schema_loads_and_has_unique_global_id`
- `eng/tests/contracts/test_unified_object_contract.py::TestCanonicalObjectSchemas::test_object_name_has_exactly_one_canonical_file`
- `eng/tests/contracts/test_unified_object_contract.py::TestCanonicalObjectSchemas::test_every_object_schema_is_in_the_ownership_registry`
- `eng/tests/contracts/test_unified_object_contract.py::TestCanonicalObjectSchemas::test_no_two_equivalent_schemas_per_object`

## 2. 13 个对象 → canonical schema → schema ID（对照表）

| 对象 | schema ID | canonical 文件 | 单位（BUNIT 语义） | 无效值 / 缺失表示 | 精度 | 可否作权重（UNIFIED_MODEL §2 原文） | DataArtifact 登记 |
|---|---|---|---|---|---|---|---|
| `signal` | `https://astrocs.local/schemas/unified/signal/v1` | `eng/contracts/schemas/unified/signal.schema.json` | **标度由承载面声明**（`docs/standards/NUMERIC_STANDARD.md` 标度词表）：面亮度域 = `ADU/sr`；像素域帧面 = `ADU`（`calibrated_adu` 或 `photo_scaled_adu`，后者含逐帧因子 α）；`BUNIT(声明)` 只在产品显式写出 BUNIT 时成立，且须与 `provenance` 的像素语义声明一致（`DATA_SEMANTICS` §31.2） | NaN / null | float32|float64 | 否 | `DATA-OBJ-SIGNAL-001` |
| `variance` | `https://astrocs.local/schemas/unified/variance/v1` | `eng/contracts/schemas/unified/variance.schema.json` | **量纲随承载它的 signal 估计量**：面亮度域 = `ADU^2/sr^2`；像素域帧面（Phase1 `variance` 块）与 UPM 控制点 = `ADU^2`（**无 sr 幂**）⇒ 同名对象的三种承载面**不得互相代入**，消费侧必须先判定标度类别（`docs/standards/NUMERIC_STANDARD.md`） | 无覆盖 = `null`（对象级）/ `NaN`（Phase1 产品面，与 signal 同态）；**有覆盖但无方差信息 = `0`（显式不可用）**；负值 = 损坏 | float32|float64 | 对该估计目标可以 | `DATA-OBJ-VARIANCE-001` |
| `ivar` | `https://astrocs.local/schemas/unified/ivar/v1` | `eng/contracts/schemas/unified/ivar.schema.json` | **1/signal单位^2**：面亮度域 = `sr^2/ADU^2`；像素域帧面 = `ADU^-2`（无 sr 幂）；与同承载面的 variance 严格互倒（有限域） | `0` = 显式不可用（禁 `1/0→Inf`）；`null` = 缺失；无覆盖 = `NaN`（同 signal） | float32|float64 | 对该估计目标可以 | `DATA-OBJ-IVAR-001` |
| `source_snr` | `https://astrocs.local/schemas/unified/source_snr/v1` | `eng/contracts/schemas/unified/source_snr.schema.json` | 1（F_hat/sigma_F 无量纲） | null | float32|float64 | 不直接作帧权重 | `DATA-OBJ-SOURCE-SNR-001` |
| `depth_m5` | `https://astrocs.local/schemas/unified/depth_m5/v1` | `eng/contracts/schemas/unified/depth_m5.schema.json` | mag | null | float32|float64 | 摘要，不作权重 | `DATA-OBJ-DEPTH-M5-001` |
| `frame_snr` | `https://astrocs.local/schemas/unified/frame_snr/v1` | `eng/contracts/schemas/unified/frame_snr.schema.json` | 1（真实信号/噪声比） | null | float32|float64 | 唯一帧级参考；权重由 Phase2 逆方差叠加从 SNR 计算 | `DATA-OBJ-FRAME-SNR-001` |
| `point_information` | `https://astrocs.local/schemas/unified/point_information/v1` | `eng/contracts/schemas/unified/point_information.schema.json` | ADU^-2（=1/Var(F_hat)，点源通量口径；**不是**面亮度 `signal^-2`——后者为 sr^2/ADU^2，见 DATA_SEMANTICS §31.1a） | null | float32|float64 | 点源目标的严格权重 | `DATA-OBJ-POINT-INFORMATION-001` |
| ~~`psfsw_robust_weight`~~ **已退役** | ~~`https://astrocs.local/schemas/unified/psfsw_robust_weight/v1`~~ | ~~`eng/contracts/schemas/unified/psfsw_robust_weight.schema.json`~~（**已删除**） | — | — | — | **对象已不存在**（14→13；`ASTROCS_DESIGN.md` §3.1:173-174 + §9.73 A44） | ~~`DATA-OBJ-PSFSW-ROBUST-WEIGHT-001`~~（INDEX 已置 `OBSOLETE`） |
| `sparse_snr_layer` | `https://astrocs.local/schemas/unified/sparse_snr_layer/v1` | `eng/contracts/schemas/unified/sparse_snr_layer.schema.json` | 1 | null | float32|float64 | 帧内精细参考 | `DATA-OBJ-SPARSE-SNR-LAYER-001` |
| `support` | `https://astrocs.local/schemas/unified/support/v1` | `eng/contracts/schemas/unified/support.schema.json` | 1（[0,1]） | 0=无覆盖 | float32|float64|integer | 否 | `DATA-OBJ-SUPPORT-001` |
| `coverage` | `https://astrocs.local/schemas/unified/coverage/v1` | `eng/contracts/schemas/unified/coverage.schema.json` | 1（几何有效域） | 0=无覆盖（空域） | float32|float64|integer | 否 | `DATA-OBJ-COVERAGE-001` |
| `validity` | `https://astrocs.local/schemas/unified/validity/v1` | `eng/contracts/schemas/unified/validity.schema.json` | 1（状态量） | missing=显式缺失态 | integer | 门，不是权重 | `DATA-OBJ-VALIDITY-001` |
| `rejection` | `https://astrocs.local/schemas/unified/rejection/v1` | `eng/contracts/schemas/unified/rejection.schema.json` | 1（门/概率） | 0=未拒绝（无覆盖=0） | float32|float64|integer | 门/概率，不是 coverage | `DATA-OBJ-REJECTION-001` |
| `provenance` | `https://astrocs.local/schemas/unified/provenance/v1` | `eng/contracts/schemas/unified/provenance.schema.json` | 1（元数据，无量纲） | unavailable.{flag,reason,scope} 显式登记 | integer | —— | `DATA-OBJ-PROVENANCE-001` |

> 「可否作权重」列逐字照抄 `docs/design/UNIFIED_MODEL.md` §2，机器以 `object_weight_verdict`（const）+ `object_weight_capability`（const）双字段固化，不得自行改判定。

## 3. 模糊字段名禁令（机器门）

UNIFIED_MODEL §2 末条：**禁止**用一个模糊的 `weight/value/mask/snr` 字段承载多个含义。落法：

- 每个 canonical schema 的 `propertyNames.pattern` 拒绝裸名 `weight`/`value`/`mask`/`snr`；
- 合格写法必须带对象全名或类型前缀：`frame_snr_value`、`depth_m5_value`、`psfsw_weight_value`、`variance_value`、`validity_state`、`support_plane_ref` 等；
- canonical 对象 schema **不得**直接暴露名为 `weight/value/mask/snr` 的属性（`additionalProperties: false` + 守卫，二者同时生效）。

负例（各自必败，见 `eng/contracts/schemas/unified/negative/`）：

| 负例 | 文件 | 判红门 |
|---|---|---|
| ① 模糊字段 `weight` | `eng/contracts/schemas/unified/negative/n1_bare_weight.schema-violation.json` | `signal.schema.json` 的 `propertyNames` |
| ② `snr` 冒充 `variance` | `eng/contracts/schemas/unified/negative/n2_source_snr_as_variance.schema-violation.json` | `variance.schema.json` 的 `unified_object` / `variance_value` / `object_schema_id` |
| ③ `coverage` 当 `rejection` | `eng/contracts/schemas/unified/negative/n3_coverage_as_rejection.schema-violation.json` | `rejection.schema.json` 的 `unified_object` / `pollution_inference` |
| ④ 跨对象错误连接（`source_snr` 接进要求 `variance` 的端口） | `eng/contracts/schemas/unified/negative/n4_source_snr_into_variance_port.schema-violation.json` | `unified_object_registry.json#port_contract` 的 `accepts_object` / `accepts_schema_id` / `connected_object_document` |

## 4. 归属归一与产品族字段级约束落点

`eng/contracts/schemas/` 是数据合同的**唯一事实源**。对象身份/单位/无效值/精度/可否作权重一律以
`eng/contracts/schemas/unified/` 的 13 个 canonical 对象 schema 为准；任何其它 schema 只能是
「产品族专用投影」或「兼容期映射」，必须在 `docs/contracts/unified_object_registry.json#canonical_object_classes`
登记归属，且不得与 canonical 重复定义对象判别字段（除 `schema_version` 外 required/properties 词表不得重叠）。

V6 合同层的语义已按 `CHG-2026-09-22-V6-CONTRACT-MERGE` **自解释合并进现行合同链**，其字段级判据按两层落点承载，判据强度不变：

| 判据面 | 现行落点 | 归属类型 | 变更编号 |
|---|---|---|---|
| canonical 对象级判据（BUNIT 量纲可判、`k_corr != 1`、对角表示 ⇒ 必带相关核/算子描述、`W_info` 单位锚、`validity.reason` 白名单、模糊字段名禁令） | `eng/contracts/schemas/unified/*.schema.json` 的 `allOf` | object_contract | CHG-2026-09-22-V6-CONTRACT-MERGE |
| 产品族记录级判据（`units`/`signal`/`covariance`/`psf`/`effective-psf`/`point-information`/`weight-mode`/`psfsw`/`provenance`/`phase3` 十类记录的字段级约束） | `eng/contracts/schemas/product_family_field_constraints.schema.json`（`$defs` 逐件） | product_family_field_constraints（**非对象**合同，**不**定义对象判别字段） | CHG-2026-09-22-V6-CONTRACT-MERGE |
| 条款注册表 / 单位表 / 词表 / 迁移映射 / 待签与开放项登记 | `eng/contracts/data/v6_clause_registry_v1.json` | machine_registration_table | CHG-2026-09-22-V6-CONTRACT-MERGE |
| 条款注册表、签字项与开放项的**正文承载页** | `docs/contracts/DATA_SEMANTICS.md` §31.10（+ §31.1–§31.9、§28.6） | human_readable_contract | CHG-2026-09-22-V6-CONTRACT-MERGE |
| 产品族正例 | `eng/contracts/data/examples/v6/` | positive_fixtures | CHG-2026-09-22-V6-CONTRACT-MERGE |
| 独立 Oracle + 负向 mutation 验证面 | `eng/tests/contracts/product_family/` | verification | CHG-2026-09-22-V6-CONTRACT-MERGE |
| 共享校验器 | `eng/tests/common/jsonschema_min.py` | shared_validator | CHG-2026-09-22-V6-CONTRACT-MERGE |

> 产品族记录级合同与被其引用的 canonical 对象合同**不等价**（前者含 49 条 `PENDING_OWNER_SIGNOFF` 条款，
> fail-closed），因此以「非对象合同」身份在 ownership 索引中登记；其读写规则、fail-closed 门与词表不变。
>
> **生效与退役条件**：由**变更编号**决定，**不得**用版本号窗口表达（`ASTROCS_DESIGN.md` §12）。
> **`weight_mode` 家族**：已按 §9.73 A44 作废（作废键面：删键 / 改写 / 加作废留痕），作废留痕见
> `docs/contracts/DATA_SEMANTICS.md` §31.3；`psfsw_robust_weight` 对象已真删
> （14→13；`CHG-2026-09-20-PSFSW-RETIRE`），其负例见 `eng/contracts/schemas/unified/negative/n5_retired_psfsw_robust_weight.schema-violation.json`。

## 4b. `sparse_snr_layer` 的重建声明面（算子词表与层几何）

`sparse_snr_layer` 是 Phase1 产出、Phase2 消费的标准层。除控制点值本身（`sparse_snr_semantics` 冻结为绝对通量型 SNR）外，**该层还必须能自解释「怎么由控制点重建稠密场」**，否则同一份落盘数据在不同消费实现下会得到不同的稠密场。两个声明面（均可选，缺省语义在 schema description 内冻结）：

| 声明 | 键 | 词表 / 值域 | 缺省 | 机器判据 |
|---|---|---|---|---|
| 重建算子 | `reconstruction_operator` | `natural_bicubic_spline_clip_v1`（默认档）/ `natural_bicubic_spline_clip_mesh_median_v1`（高对比域档）/ `bilinear_regular_grid_v1`（对照·回退）/ `nearest_control_point_v1`（散点层） | `natural_bicubic_spline_clip_v1` | schema `enum`；消费侧未识别 token ⇒ fail-closed（不得回退默认） |
| 层几何 | `control_point_geometry` | `spacing_px`（>0）、`node_placement`（`const: cell_center_v1`）、`origin_x`/`origin_y`（缺省 0） | origin 0 + `cell_center_v1` | schema `const`；消费侧「节点—cell 中心」一致性门 ⇒ 角点锚定（半 cell 相位）fail-closed |

- **为什么把「是否开 3×3 mesh 中值前置滤波」编码进算子标识，而不是另开一个布尔字段**：① **值域钳制不是可选项**——去掉它，光滑插值类在病态控制网格上的权重效率损失 E 由 1.007 / 0.294 升到 1.44e4 / 2.48e4，并会给出负的 σ；把钳制与核绑成一个标识后，「无钳制的样条」在合同层**不可表达**；② **mesh 中值滤波只在特定域必需**——HST 类高对比域必需，默认目标域（地面/seeing-limited）有害（真实地面帧上劣 39–74%），因此必须是**按数据来源的显式开关**且默认关；③ 独立布尔可组合出**从未实测**的配置（如双线性+滤波），标识化后不可表达。算子标识随层入 manifest（`SparseReconstruction.operator_id`）。
- **选择规则**：可判定 cell 内含未分辨点源（空间高分辨率 / HST 类）⇒ `..._mesh_median_v1`；地面 seeing-limited 与一般情形 ⇒ 默认档；无法判断 ⇒ 默认档。**不得**从控制网格自身推断该判定（两个候选标量诊断都不能把「滤波有益」与「滤波有害」的域分开，且在 16-bit 整数真实数据上失效）。
- **几何**：控制点坐标是像素中心坐标；规则网格下节点落在**所属 Δ×Δ cell 的中心**（cell i 覆盖 `[origin_x + i·Δ, origin_x + (i+1)·Δ − 1]`）。把节点当 cell 角点会使重建场整体平移半个 cell（Δ=64 时 31.5 px）。层定义域 = 层覆盖的 cell 并集，越出即消费侧 fail-closed（不外推、不回退帧级）。
- 依据：`实验/absolute-snr` EXP-04 §2.7/§4.1/§4.3/§4.5；算子定义、钳制与滤波的实测代价见 `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.2/§4.5。合同机器门：`eng/tests/contracts/test_unified_object_contract.py`（对象级）与 `lib/algorithms/integration/v6/oracle/recon_contract_gate.py`（本次新增声明面）。

## 4a. 旧合同 ID → 统一对象映射（MODULE_MAP 引用面，负责人裁决）

MOD-001 的 `docs/modules/MODULE_MAP.yaml` 引用 22 个 DATA ID，其中 7 个在统一对象权威面上无法解析。负责人裁决：权威=统一对象合同，`MODULE_MAP` 不得引用权威面之外的 ID。逐条结论：

| 旧 ID | 结论 | canonical 映射 | 关系 | 旧合同面（证据位置） |
|---|---|---|---|---|
| `DATA-P1-CAL` | mapped | `eng/contracts/schemas/unified/signal.schema.json` | single_object | `docs/contracts/DATA_SEMANTICS.md:133（§9 ID 定义节，:135 状态 CONTRACT_READY）`<br>`docs/contracts/PUBLIC_API.md:102`<br>`docs/modules/registry/astrocs.phase1.calibration.md:23（端口 calibrated）`<br>`docs/modules/registry/astrocs.phase1.cosmetic.md:34（端口 calibrated）`<br>`docs/modules/registry/astrocs.phase1.drizzle.md:35（端口 calibrated）`<br>`docs/modules/registry/astrocs.phase2.resample.md:22（端口 calibrated）`<br>`docs/modules/calibration.md:8/:53` |
　└ 本次新增索引（`new_index_added_by_DATA001`，不计入旧面证据）：`docs/contracts/INDEX.yaml#DATA-OBJ-* 映射段逐行 '# DATA001-LEGACY-ID: DATA-P1-CAL'`；`docs/contracts/INDEX.yaml#legacy_contract_id_map`
| `DATA-P1-COS` | mapped | `eng/contracts/schemas/unified/signal.schema.json` | single_object | `docs/contracts/DATA_SEMANTICS.md:181（§10 ID 定义节，:182 状态 CONTRACT_READY）`<br>`docs/contracts/PUBLIC_API.md:152/:177`<br>`docs/modules/registry/astrocs.phase1.cosmetic.md:35（端口 cleaned）/:38-40（权威名说明）`<br>`docs/modules/calibration.md:12` |
　└ 本次新增索引（`new_index_added_by_DATA001`，不计入旧面证据）：`docs/contracts/INDEX.yaml#DATA-OBJ-* 映射段逐行 '# DATA001-LEGACY-ID: DATA-P1-COS'`；`docs/contracts/INDEX.yaml#legacy_contract_id_map`
| `DATA-P1-COSMETIC` | mapped | `eng/contracts/schemas/unified/signal.schema.json` | superseded_placeholder | `docs/modules/registry/astrocs.phase1.cosmetic.md:39（descriptor 占位名说明）`<br>`docs/modules/registry/astrocs.phase1.star-psf.md:33（端口 cleaned）`<br>`docs/modules/registry/astrocs.phase1.star-detection.md:38（端口 image）` |
　└ 本次新增索引（`new_index_added_by_DATA001`，不计入旧面证据）：`docs/contracts/INDEX.yaml#DATA-OBJ-* 映射段逐行 '# DATA001-LEGACY-ID: DATA-P1-COSMETIC'`；`docs/contracts/INDEX.yaml#legacy_contract_id_map`
| `DATA-P1-DRZ` | mapped | `eng/contracts/schemas/unified/signal.schema.json`；`eng/contracts/schemas/unified/variance.schema.json`；`eng/contracts/schemas/unified/ivar.schema.json`；`eng/contracts/schemas/unified/support.schema.json`；`eng/contracts/schemas/unified/coverage.schema.json` | composite | `docs/contracts/DATA_SEMANTICS.md:226（§11 ID 定义节，:228 状态 CONTRACT_READY）`<br>`docs/contracts/PUBLIC_API.md:214`<br>`docs/modules/registry/astrocs.phase1.drizzle.md:7（upstream 引用）/:39（模块级数据合同说明）`<br>`docs/modules/registry/astrocs.phase1.hips-writer.md:7/:34（端口 stacked）`<br>`docs/modules/healpix_drizzle.md:32/:62` |
　└ 本次新增索引（`new_index_added_by_DATA001`，不计入旧面证据）：`docs/contracts/INDEX.yaml#DATA-OBJ-* 映射段逐行 '# DATA001-LEGACY-ID: DATA-P1-DRZ'`；`docs/contracts/INDEX.yaml#legacy_contract_id_map`
| `DATA-P1-SOURCES` | no_canonical | **无 canonical 对应**（descriptor_port_catalog） | orchestration_port_catalog_name | `docs/contracts/DATA_SEMANTICS.md:444（§14 下游计数行 n_sources 的引用）`<br>`docs/contracts/DATA_SEMANTICS.md:471（§14 端口链 psf→sources→fluxes）`<br>`docs/contracts/DATA_SEMANTICS.md:531（§15 端口链）`<br>`docs/contracts/PUBLIC_API.md:636（photometry descriptor 端口编目，带data_id=DATA-P1-SOURCES）`<br>`docs/modules/MODULE_MAP.yaml:151（psf 模块）` |
　└ 本次新增索引（`new_index_added_by_DATA001`，不计入旧面证据）：`docs/contracts/INDEX.yaml#legacy_contract_id_map（本任务新增；基线无该 ID）`
| `DATA-P2-SMP` | no_canonical | **无 canonical 对应**（module_io_contract） | module_io_contract_without_object_semantics | `docs/contracts/DATA_SEMANTICS.md:1407（§23 ID 定义节，:1409 状态 CONTRACT_READY）`<br>`docs/contracts/PUBLIC_API.md:1447/:1556/:1701`<br>`docs/modules/phase2_samp.md:74/:76/:189`<br>`docs/modules/phase2_upm.md:52/:73/:90/:101/:249`<br>`docs/modules/registry/astrocs.phase2.sample.md:63/:64/:164`<br>`docs/modules/registry/astrocs.phase2.upm-fit.md:46` |
　└ 本次新增索引（`new_index_added_by_DATA001`，不计入旧面证据）：`docs/contracts/INDEX.yaml#DATA-OBJ-* 映射段逐行 '# DATA001-LEGACY-ID: DATA-P2-SMP'`；`docs/contracts/INDEX.yaml#legacy_contract_id_map`
| `DATA-GAIA-001` | no_canonical | **无 canonical 对应**（external_catalog_contract） | external_catalog_service_contract | `docs/contracts/DATA_SEMANTICS.md:87（§8 ID 定义节，:88 状态 CONTRACT_READY）`<br>`docs/contracts/PUBLIC_API.md:71`<br>`docs/contracts/DATA_ARTIFACTS.md:29（登记行）`<br>`docs/traceability/TRACEABILITY_MATRIX.csv 第 2 行（DATA=DATA-GAIA-001, VERIFIED）`<br>`docs/modules/gaia_xpsd_client.md:23/:40` |
　└ 本次新增索引（`new_index_added_by_DATA001`，不计入旧面证据）：`docs/contracts/INDEX.yaml#legacy_contract_id_map（本任务新增；基线无该 ID）`

逐条判定依据（含证据原文）：`docs/contracts/unified_object_registry.json#legacy_contract_ids`；机器可读副本：`eng/contracts/data/unified_object_compatibility_map_v1.json#legacy_contract_id_map`；INDEX.yaml 同段：`legacy_contract_id_map`（含逐行 `# DATA001-LEGACY-ID: <旧ID> -> ...`）。

小结：**mapped = 4**（DATA-P1-CAL / DATA-P1-COS / DATA-P1-COSMETIC / DATA-P1-DRZ），**无 canonical 对应 = 3**（DATA-P1-SOURCES / DATA-P2-SMP / DATA-GAIA-001）。机器断言：`eng/tests/contracts/test_unified_object_contract.py::TestRegistryIndex::test_legacy_ids_are_decided_not_pending` 与 `::test_legacy_id_map_written_into_index_and_compat_map`。

## 5. 三类配置分离锚点（配置 schema 本体归 CFG-001）

机器索引：`docs/contracts/config_separation_anchors.json`（`IDX-CONFIG-SEPARATION`）。本任务**不建** `eng/packaging/config/**`、**不建** phase_config schema。

| 配置类 | 语义 | schema owner | 字段名命名空间 | 禁止混入 |
|---|---|---|---|---|
| `phase_config` | 科学参数 / 输入输出 / 算法选择；可跨机器复现；禁止出现任何硬件调优字段 | CFG-001 | `^sci_[a-z0-9_]+$`；`^(input|output)_[a-z0-9_]+$`；`^algorithm_[a-z0-9_]+$`；`^phase_name$` | `isa`、`isa_level`、`workers`、`worker_count`、`block`、`block_size`、`cpu_model`、`cpu_vendor`、`thread_budget`、`affinity_mask` |
| `cpu_profile` | ISA / workers / block / 机器绑定；仅 benchmark 生成；缓存于程序安装目录；无 profile → 保守运行不阻塞 | CFG-001 | `^isa(_[a-z0-9_]+)?$`；`^(workers|worker_count)$`；`^block(_[a-z0-9_]+)?$`；`^cpu_[a-z0-9_]+$`；`^thread_budget$`；`^profile_hash$` | `sci_algorithm_id`、`sci_rejection_profile`、`algorithm_choice`（原示例 `sci_weight_mode` 已删 （已按 §9.73 A44 作废：键不存在；权重是派生量）） |
| `run_manifest` | 本次运行冻结：源码 SHA / 配置哈希 / 输入输出哈希 / 工具链版本；不承载科学参数与硬件调优 | CFG-001 | `^manifest_(input|output)_hashes$`；`^software_sha$`；`^config_hash$`；`^toolchain_[a-z0-9_]+$`；`^run_id$` | `isa`、`workers`、`block`、`sci_algorithm_id`（原示例 `sci_weight_mode` 已删 （已按 §9.73 A44 作废：键不存在；权重是派生量）） |

硬规则：三类配置的字段名模式两两互斥（不得共用字段名承载不同含义）；`cpu_profile` 的硬件字段出现在 `phase_config` 即 REJECT。
机器断言：`eng/tests/contracts/test_unified_object_contract.py::TestConfigSeparationAnchors`。

## 6. 边界声明

- 本任务只写 `eng/contracts/schemas/**`、`eng/contracts/data/**`、`eng/contracts/config/**`、`docs/contracts/**`、`eng/tests/contracts/**`；
- 未改 `lib/**`、`lib/infrastructure/cli/**`、`eng/tools/**`、`eng/ci/**`、`runtime/**`、`eng/packaging/config/**`、`docs/science/**`、`docs/algorithms/**`、`docs/plugins/**`、根 `CMakeLists.txt`、仓库根条目、`工程控制/**`、`reports/**`、`.github/**`；
- 未改任何公式、阈值、默认容差、排异规则或归约顺序；未 commit / push / 建分支 / stash / reset / clean / checkout。

