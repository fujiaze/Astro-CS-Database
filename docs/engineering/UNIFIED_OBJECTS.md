# 统一数据对象合同（13 个 canonical 数据对象与 canonical schema 的对照登记）

> 上游：docs/ACSD_DESIGN.md 「数据对象」一节（数据对象）。
> 上位正本：`docs/detail/UNIFIED_MODEL.md` 的「数据对象与字段歧义消解」一节——该节登记 13 个 canonical 数据对象，逐对象给出语义与「可否作权重」判定，并规定 `weight`/`value`/`mask`/`snr` 各字段各自具名、一个字段只承载一个含义。本文是**索引与语义登记**，不改任何科学定义、公式、阈值、容差或推导；对象身份/单位/无效值/精度/可否作权重一律以 canonical schema 为准。
> 现行对象集 = **13 个**；全链没有「权重模式」这一可选概念，`weight_mode` 家族与 `sci_weight_mode` 键**不存在**（叠加权重是阶段二按该天球像素对应帧集合现场换算的派生量，见 `docs/ACSD_DESIGN.md` 「数据对象」一节（数据对象））。

## 1. 唯一事实源声明

`eng/contracts/schemas/` 是数据合同的**唯一事实源**（`docs/ACSD_DESIGN.md` 「数据对象」一节）。对上位正本 `docs/detail/UNIFIED_MODEL.md`「数据对象与字段歧义消解」一节的 **13** 个 canonical 对象：

```text
canonical 定义 = eng/contracts/schemas/unified/<对象名>.schema.json
                $id = https://acsd.local/schemas/unified/<对象名>/v1   （每对象恰 1 个，全局唯一）
其它 schema     = 只能是「产品族专用投影」或「兼容期映射」，必须在
                  eng/contracts/data/unified_object_compatibility_map_v1.json#entries 登记归属，
                  且不得与 canonical 重复定义对象判别字段（除 schema_version 外 required/properties 词表不得重叠）。
```

**唯一性判定标准**：对上述 13 个对象，`eng/contracts/schemas/unified/<对象名>.schema.json` 是唯一 canonical 定义（每对象恰 1 个 `$id`，全局不重复）；任何其它 schema 只能是「产品族专用投影」或「兼容期映射」，其对象判别字段与 canonical 分立（除 `schema_version` 外 required/properties 词表两两不重叠），且 `eng/contracts/schemas/**` 下每个 schema 文件都必须在 `eng/contracts/data/unified_object_compatibility_map_v1.json` 的 entries 中被登记归属。

本节所列的唯一性判据有四种，逐条对应一条人读判据与一个机器可核事实：

| 判据 | 人读判据 | 机器可核事实 |
|---|---|---|
| 每 schema 可加载、`$id` 全局唯一 | 13 个 canonical 文件逐个能被 JSON 解析 | `eng/contracts/schemas/unified/` 下 13 个文件的 `$id` 互不相同 |
| 每个对象名恰一个 canonical 文件 | 对象名与文件名一一对应，无第二份同名 canonical | `eng/contracts/schemas/unified/<对象名>.schema.json` 存在且唯一 |
| 每个 schema 都在归属登记表内 | `eng/contracts/schemas/**` 下每个文件都能在 compatibility map 的 entries 里找到归属 | `eng/contracts/data/unified_object_compatibility_map_v1.json#entries` |
| 同一对象无两份等价 schema | 产品族投影与兼容期映射不与 canonical 重复定义对象判别字段 | 除 `schema_version` 外 required/properties 词表两两不重叠 |

本节不声明执行器：仓内 `eng/tests/` 只有 `conformance/`（noop 骨架）与 `validation/`（实验域脚本）两个目录，上表四条判据当前**没有可执行载体**，按人读对抗性审核逐条核对。补执行器属代码侧订正，见 `governance/UNRESOLVED.md`。

## 2. 13 个对象 → canonical schema → schema ID（对照表）

| 对象 | schema ID | canonical 文件 | 单位（BUNIT 语义） | 无效值 / 缺失表示 | 精度 | 可否作权重（上位正本「数据对象与字段歧义消解」一节原文） | DataArtifact 登记 |
|---|---|---|---|---|---|---|---|
| `signal` | `https://acsd.local/schemas/unified/signal/v1` | `eng/contracts/schemas/unified/signal.schema.json` | **标度由承载面声明**（`docs/engineering/standards/NUMERIC` 标度词表）：面亮度域 = `ADU/sr`；像素域帧面 = `ADU`（`calibrated_adu` 或 `photo_scaled_adu`，后者含逐帧因子 α）；`BUNIT(声明)` 只在产品显式写出 BUNIT 时成立，且须与 `provenance` 的像素语义声明一致（`docs/science/unified/DATA_SEMANTICS.md`「标度类别与线性标度律」一节） | NaN / null | float32|float64 | 否 | `DATA-OBJ-SIGNAL-001` |
| `variance` | `https://acsd.local/schemas/unified/variance/v1` | `eng/contracts/schemas/unified/variance.schema.json` | **量纲随承载它的 signal 估计量**：面亮度域 = `ADU^2/sr^2`；像素域帧面（Phase1 `variance` 块）与 UPM 控制点 = `ADU^2`（**无 sr 幂**）⇒ 同名对象的三种承载面**各自独立取值、互不代入**，消费侧必须先判定标度类别（`docs/engineering/standards/NUMERIC`） | 无覆盖 = `null`（对象级）/ `NaN`（Phase1 产品面，与 signal 同态）；**有覆盖但无方差信息 = `0`（显式不可用）**；负值 = 损坏 | float32|float64 | 对该估计目标可以 | `DATA-OBJ-VARIANCE-001` |
| `ivar` | `https://acsd.local/schemas/unified/ivar/v1` | `eng/contracts/schemas/unified/ivar.schema.json` | **1/signal单位^2**：面亮度域 = `sr^2/ADU^2`；像素域帧面 = `ADU^-2`（无 sr 幂）；与同承载面的 variance 严格互倒（有限域） | `0` = 显式不可用（禁 `1/0→Inf`）；`null` = 缺失；无覆盖 = `NaN`（同 signal） | float32|float64 | 对该估计目标可以 | `DATA-OBJ-IVAR-001` |
| `source_snr` | `https://acsd.local/schemas/unified/source_snr/v1` | `eng/contracts/schemas/unified/source_snr.schema.json` | 1（F_hat/sigma_F 无量纲） | null | float32|float64 | 不直接作帧权重 | `DATA-OBJ-SOURCE-SNR-001` |
| `depth_m5` | `https://acsd.local/schemas/unified/depth_m5/v1` | `eng/contracts/schemas/unified/depth_m5.schema.json` | mag | null | float32|float64 | 摘要，不作权重 | `DATA-OBJ-DEPTH-M5-001` |
| `frame_snr` | `https://acsd.local/schemas/unified/frame_snr/v1` | `eng/contracts/schemas/unified/frame_snr.schema.json` | 1（真实信号/噪声比，`SNR = F_ref/σ_F`，**无量纲**；分子 `F_ref` 取**逐帧参考通量**，不是逐源实测通量，口径定义处是 `docs/ACSD_DESIGN.md` §2.2（P2 跨帧绝对信噪比））。**对象身份的两个必要条件（正向约束）**：① 分子**必须已扣独立估计的局部背景**，天光**只作为噪声项**进入 `σ_F`（红线见 `docs/detail/registry/acsd.phase1.noise-snr.md`「帧级 SNR（frame_snr）」一节）——未扣背景的比值**不是** `frame_snr`，本对象只接受已扣背景的比值；② 本对象是**点源（PSF）**量，与面亮度 SNR **各自独立、互不宣称等价**。参考通量基准（`reference_baseline`）见 `docs/science/unified/DATA_SEMANTICS.md`「帧级信噪比的参考通量基准」一节 | null | float64 | 唯一帧级参考；权重由 Phase2 逆方差叠加从 SNR 计算（SNR 本身**不是**权重） | `DATA-OBJ-FRAME-SNR-001` |
| `point_information` | `https://acsd.local/schemas/unified/point_information/v1` | `eng/contracts/schemas/unified/point_information.schema.json` | ADU^-2（=1/Var(F_hat)，点源通量口径；**不是**面亮度 `signal^-2`——后者为 sr^2/ADU^2，见 `docs/science/unified/DATA_SEMANTICS.md`「面亮度单位的推导」一节）。**标度 = 与 `F_hat` 同承载面**（帧面为 `photo_scaled_adu` 时随 1/α² 换算） | null | float32|float64 | 点源目标的严格权重。**估计域（正向约束）**：`Var(F_hat)` 是**PSF 拟合域**内的通量估计方差（`F_hat` 单位 ADU），**不含**像素间相关核的贡献；把本对象当权重消费前，消费侧必须确认 ① 估计域与目标一致（点源、非面亮度）、② 是否已含相关核（未含时按 `docs/science/unified/DATA_SEMANTICS.md`「溯源最小集」一节的 `k_corr ≠ 1` 条款补核或拒绝）、③ 与 `W_info` 消费面（`docs/science/unified/DATA_SEMANTICS.md`「单位与量纲表」一节）的量纲一致。三条缺一即 fail-closed，默认代入一律判红 | `DATA-OBJ-POINT-INFORMATION-001` |
| `sparse_snr_layer` | `https://acsd.local/schemas/unified/sparse_snr_layer/v1` | `eng/contracts/schemas/unified/sparse_snr_layer.schema.json` | 1 | null | float32|float64 | 帧内精细参考 | `DATA-OBJ-SPARSE-SNR-LAYER-001` |
| `support` | `https://acsd.local/schemas/unified/support/v1` | `eng/contracts/schemas/unified/support.schema.json` | 1（[0,1]） | 0=无覆盖 | float32|float64|integer | 否 | `DATA-OBJ-SUPPORT-001` |
| `coverage` | `https://acsd.local/schemas/unified/coverage/v1` | `eng/contracts/schemas/unified/coverage.schema.json` | 1（几何有效域） | 0=无覆盖（空域） | float32|float64|integer | 否 | `DATA-OBJ-COVERAGE-001` |
| `validity` | `https://acsd.local/schemas/unified/validity/v1` | `eng/contracts/schemas/unified/validity.schema.json` | 1（状态量） | missing=显式缺失态 | integer | 门，不是权重 | `DATA-OBJ-VALIDITY-001` |
| `rejection` | `https://acsd.local/schemas/unified/rejection/v1` | `eng/contracts/schemas/unified/rejection.schema.json` | 1（门/概率） | 0=未拒绝（无覆盖=0） | float32|float64|integer | 门/概率，不是 coverage | `DATA-OBJ-REJECTION-001` |
| `provenance` | `https://acsd.local/schemas/unified/provenance/v1` | `eng/contracts/schemas/unified/provenance.schema.json` | 1（元数据，无量纲） | unavailable.{flag,reason,scope} 显式登记 | 非数值元数据（字符串 / 键值：来源链、输入哈希、运行标识、产品类型） | —— | `DATA-OBJ-PROVENANCE-001` |

**精度列的读法**：本列逐对象标注**应归属的精度**，取自权威顶点的精度归属律——稠密大面（图像面、球面累加器、方差/覆盖面、HiPS tile）默认单精度；稀疏与元数据（帧级信噪比、WCS 解、星表匹配、测光定标、manifest）全程双精度；JSON 显式指定位深时以 JSON 为准。列中只给一个 token 表示该对象的应归属档；给出多个 token 表示该对象的归属档可由 JSON 显式指定覆盖（第三款）。`float32` / `float64` / `integer` 只对数值面有意义，非数值对象写其载荷形态、不套数值 dtype。稀疏与元数据清单第五项在权威顶点与其下级 science 正本之间不一致，登记为 `governance/UNRESOLVED.md` 的方向裁决条目，裁决前本列按权威顶点原文取 `manifest`。

**精度列与另外两个同名项不是同一断言，不得互相代入**：① canonical schema `eng/contracts/schemas/unified/<对象名>.schema.json` 的 `precision` 属性是**值域**（该属性取值只能是 `float32` / `float64` / `integer` 三个 token 之一），它不逐对象指派档位；② `../data/ARTIFACTS.md` 统一对象合同登记表的 `scalar` 列是 **DataArtifact 承载面的标量形态**，问的是该对象在承载面上的线格式，不是应归属档位。本列是逐对象应归属档位的唯一正本。

**精度面当前的实现缺口（需负责人裁决）**：最高设计要求「稠密大面单精度」与「稀疏与元数据双精度」**同时成立**（两个可各自独立的归属），但机器侧统一 I/O 只有一个**全局精度模式位**——`lib/infrastructure/aio/src/aio_api.cpp` 的 `g_aio_precision_mode_fp64` 与 `aio_set_precision_mode(int is_fp64)`，模块内查询接口 `aio_internal_is_fp64()` 读同一变量，其头注自述 `PrecisionContext` 单例在动态库边界不共享，须显式设置。单个全局位**无法表达**「稠密 FP32 且稀疏/元数据 FP64」这一组合。因此：本表按对象语义标注的是**应归属**的精度，不是当前机器已强制执行的精度；「拆成两个独立精度量」还是「把最高设计的精度归属收敛为一个全局模式」属未决项，登记在 `governance/UNRESOLVED.md`。

> 「可否作权重」列逐字照抄 `docs/detail/UNIFIED_MODEL.md` 的「数据对象与字段歧义消解」一节，机器以 `object_weight_verdict`（const）+ `object_weight_capability`（const）双字段固化，判定只取自该列原文。

## 3. 模糊字段名禁令（机器门）

上位正本「数据对象与字段歧义消解」一节末条：**一个字段只承载一个含义**（模糊名 `weight`/`value`/`mask`/`snr` 的多义承载一律判红）。落法：

- 每个 canonical schema 的 `propertyNames.pattern` 拒绝裸名 `weight`/`value`/`mask`/`snr`；
- 合格写法必须带对象全名或类型前缀：`frame_snr_value`、`depth_m5_value`、`psfsw_weight_value`、`variance_value`、`validity_state`、`support_plane_ref` 等；
- canonical 对象 schema 的属性名一律带对象全名或类型前缀；`weight/value/mask/snr` 裸名由 `additionalProperties: false` + 守卫两处同时拒绝。

负例（各自必败，见 `eng/contracts/schemas/unified/negative/`）：

| 负例 | 文件 | 判红门 |
|---|---|---|
| ① 模糊字段 `weight` | `eng/contracts/schemas/unified/negative/n1_bare_weight.schema-violation.json` | `signal.schema.json` 的 `propertyNames` |
| ② `snr` 冒充 `variance` | `eng/contracts/schemas/unified/negative/n2_source_snr_as_variance.schema-violation.json` | `variance.schema.json` 的 `unified_object` / `variance_value` / `object_schema_id` |
| ③ `coverage` 当 `rejection` | `eng/contracts/schemas/unified/negative/n3_coverage_as_rejection.schema-violation.json` | `rejection.schema.json` 的 `unified_object` / `pollution_inference` |
| ④ 跨对象错误连接（`source_snr` 接进要求 `variance` 的端口） | `eng/contracts/schemas/unified/negative/n4_source_snr_into_variance_port.schema-violation.json` | `eng/contracts/schemas/unified/port_contract.schema.json` 的 `accepts_object` / `accepts_schema_id` / `connected_object_document` |

## 4. 归属归一与产品族字段级约束落点

`eng/contracts/schemas/` 是数据合同的**唯一事实源**。对象身份/单位/无效值/精度/可否作权重一律以
`eng/contracts/schemas/unified/` 的 13 个 canonical 对象 schema 为准；任何其它 schema 只能是
「产品族专用投影」或「兼容期映射」，必须在 `eng/contracts/data/unified_object_compatibility_map_v1.json#entries`
登记归属，且对象判别字段与 canonical 分立（除 `schema_version` 外 required/properties 词表两两不重叠）。

合同条款的字段级判据按两层落点承载：

| 判据面 | 现行落点 | 归属类型 |
|---|---|---|
| canonical 对象级判据（BUNIT 量纲可判、`k_corr != 1`、对角表示 ⇒ 必带相关核/算子描述、`W_info` 单位锚、`validity.reason` 白名单、模糊字段名禁令） | `eng/contracts/schemas/unified/*.schema.json` 的 `allOf` | object_contract |
| 产品族记录级判据（`units`/`signal`/`covariance`/`psf`/`effective-psf`/`point-information`/`weight-mode`/`psfsw`/`provenance`/`phase3` 十类记录的字段级约束） | `eng/contracts/schemas/product_family_field_constraints.schema.json`（`$defs` 逐件） | product_family_field_constraints（**非对象**合同，**不**定义对象判别字段） |
| 条款注册表 / 单位表 / 词表 / 迁移映射 / 待签与开放项登记 | `eng/contracts/data/clause_registry.json` | machine_registration_table |
| 条款注册表、签字项与开放项的**正文承载页** | 本文件「归属归一与产品族字段级约束落点」一节 | human_readable_contract |
| 产品族正例 | `eng/contracts/data/examples/` | positive_fixtures |
| 独立 Oracle + 负向 mutation 验证面 | `eng/tests/contracts/product_family/`（当前无执行器，见下注） | verification |
| 共享校验器 | 无仓内载体；JSON Schema 校验随产品族记录级合同走 `eng/contracts/schemas/product_family_field_constraints.schema.json` 的词表（`shared_validator` 待补） | shared_validator |

> 产品族记录级合同与被其引用的 canonical 对象合同**不等价**（前者含 48 条 `PENDING_OWNER_SIGNOFF` 条款，
> fail-closed），因此以「非对象合同」身份在 ownership 索引中登记；其读写规则、fail-closed 门与词表不变。
> `psfsw_robust_weight` 对象的负例见 `eng/contracts/schemas/unified/negative/n5_retired_psfsw_robust_weight.schema-violation.json`。
> **验证面执行器状态**：上表「独立 Oracle + 负向 mutation 验证面」与「共享校验器」两行的仓内执行器当前都不存在——`eng/tests/` 只有 `conformance/`（noop 骨架）与 `validation/`（实验域脚本）两个目录。判据本身仍生效，执行器待补，属代码侧订正。

## 4b. `sparse_snr_layer` 的重建声明面（算子词表与层几何）

`sparse_snr_layer` 是 Phase1 产出、Phase2 消费的标准层。除控制点值本身（`sparse_snr_semantics` 冻结为绝对通量型 SNR）外，**该层还必须能自解释「怎么由控制点重建稠密场」**，否则同一份落盘数据在不同消费实现下会得到不同的稠密场。两个声明面（均可选，缺省语义在 schema description 内冻结）：

| 声明 | 键 | 词表 / 值域 | 缺省 | 机器判据 |
|---|---|---|---|---|
| 重建算子 | `reconstruction_operator` | `natural_bicubic_spline_clip_v1`（默认档）/ `natural_bicubic_spline_clip_mesh_median_v1`（高对比域档）/ `bilinear_regular_grid_v1`（对照·回退）/ `nearest_control_point_v1`（散点层） | `natural_bicubic_spline_clip_v1` | schema `enum`；消费侧未识别 token ⇒ fail-closed（默认档不作回退目标） |
| 层几何 | `control_point_geometry` | `spacing_px`（>0）、`node_placement`（`const: cell_center_v1`）、`origin_x`/`origin_y`（缺省 0） | origin 0 + `cell_center_v1` | schema `const`；消费侧「节点—cell 中心」一致性门 ⇒ 角点锚定（半 cell 相位）fail-closed |

- **为什么把「是否开 3×3 mesh 中值前置滤波」编码进算子标识，而不是另开一个布尔字段**：① **值域钳制不是可选项**——去掉它，光滑插值类在病态控制网格上的权重效率损失 E 急剧放大并会给出非正的 σ；把钳制与核绑成一个标识后，「无钳制的样条」在合同层**不可表达**；② **mesh 中值滤波只在特定域必需**——HST 类高对比域必需，默认目标域（地面/seeing-limited）有害，因此必须是**按数据来源的显式开关**且默认关；③ 独立布尔可组合出未经验证的配置（如双线性+滤波），标识化后不可表达。算子标识随层入 manifest（`SparseReconstruction.operator_id`）。逐域实测代价见 `实验/absolute-snr`。
  - **① 的判据强度（正向约束，防单判据过强）**：权重效率损失 `E = Var_w/Var_opt − 1` 对**整体乘性缩放完全相消**（`w = 1/σ̂²` 的比值定义）⇒ **E 单独不足以**排除水平偏差：两臂可以 E 相同而水平偏差差出数倍。因此「钳制不可省」的结论必须**同时**报 E 与水平偏差（正本 = `docs/science/noise_snr/NOISE_SNR.md`「判据不得恒真」一节）；**「E 相同」只允许推出「E 相同」**（两臂等价须另有水平偏差证据）。
  - **σ 正值守卫（强制）**：重建场必须满足 `isfinite(σ) ∧ σ > 0`；任何产生非正或非有限 σ 的配置一律 fail-closed 具名拒绝，**防线 = 门本身**（「实测会给出负 σ」是观测事实，不构成门）。
- **选择规则**：可判定 cell 内含未分辨点源（空间高分辨率 / HST 类）⇒ `..._mesh_median_v1`；地面 seeing-limited 与一般情形 ⇒ 默认档；无法判断 ⇒ 默认档。该判定只取自声明面（控制网格自身不是判据来源：两个候选标量诊断都不能把「滤波有益」与「滤波有害」的域分开，且在 16-bit 整数真实数据上失效）。
  - **判据来源与责任方（强制，缺一即 fail-closed）**：域判定的**许可证据只有两类**——① 上游数据来源标识（空间高分辨率任务 / HST 类，随输入产品 provenance 携带）；② 由**操作者显式声明**（配置或命令行）。**第三类** = 从控制点值、控制网格几何或任何帧内标量诊断**推断**该判定：一律判红。责任方 = 声明该判定的操作者/上游 provenance；消费侧只做「已声明即采信、未声明即取默认档」的二值路由；自行推断属判红面。
  - **默认档的适用域（正向约束）**：`natural_bicubic_spline_clip_v1` 的适用域 = **地面 seeing-limited 与一般情形**。在 HST 类高对比域上，其 Δ*（E 首次劣于帧级臂的最小控制点间隔）小于生产控制点间隔，落于失效区 ⇒ 该域**必须**显式改用 `..._mesh_median_v1`（其 Δ* 大于生产控制点间隔，处于有效区）。默认档是**回退**，不是「域无关的安全选择」；未声明域时取默认档属**显式降级**，必须随层入 manifest 可追溯。逐域 Δ* 与失效倍数见 `实验/absolute-snr`。
- **几何**：控制点坐标是像素中心坐标；规则网格下节点落在**所属 Δ×Δ cell 的中心**（cell i 覆盖 `[origin_x + i·Δ, origin_x + (i+1)·Δ − 1]`）。把节点当 cell 角点会使重建场整体平移半个 cell（Δ/2）。层定义域 = 层覆盖的 cell 并集，越出即消费侧 fail-closed（不外推、不回退帧级）。
  - **与 Phase2 UPM 控制网格的关系（强制，同一约定）**：默认 Δ = `hips.tile_width / 8 = 512 / 8 = 64` px 复用 Phase2 UPM 的 8×8/tile 控制网格 ⇒ **两处必须是同一套几何约定**：节点 = 所属 cell 的**中心**、cell 编号自 `origin` 起、`origin` 缺省 0、层定义域 = cell 并集。稀疏层一侧的约定按本行；UPM 一侧按 `docs/science/algorithms/PHASE2_UPM_IMPL.md`「离散公式 F1-F6（公式语义与 UPM_SOLVER.md §2 一致，逐条实现锚）」一节（控制点几何）——**两处不一致即 fail-closed**：同一套几何约定在两处逐项相同，换算不在消费侧发生。
- 依据：`实验/absolute-snr` EXP-04 §2.7/§4.1/§4.3/§4.5；算子定义、钳制与滤波的实测代价见 `docs/detail/registry/acsd.phase1.noise-snr.md`「稀疏帧内层几何与重建算子」一节。合同判据分两面：对象级由 `eng/contracts/schemas/unified/sparse_snr_layer.schema.json` 的 `enum` / `const` 冻结，声明面由本节表格约束；两面的机器门当前均无仓内执行器，按人读对抗性审核逐条核对。

## 4a. 合同 ID → 统一对象映射（MODULE_MAP 引用面）

`docs/engineering/architecture/MODULE_MAP` 引用 22 个 DATA ID，其中 7 个在统一对象权威面上无法直接解析，本节逐条给出其统一对象映射；`MODULE_MAP` 引用的 ID 一律取自统一对象权威面。逐条结论：

| 合同 ID | 结论 | canonical 映射 | 关系 | 合同面（条款位置） |
|---|---|---|---|---|
| `DATA-P1-CAL` | mapped | `eng/contracts/schemas/unified/signal.schema.json` | single_object | `docs/detail/registry/acsd.phase1.calibration.md`（ID 定义节、端口 calibrated）；`docs/engineering/api/PUBLIC_API`（calibration 条款）；`docs/detail/registry/acsd.phase1.cosmetic.md`（端口 calibrated）；`docs/detail/registry/acsd.phase1.drizzle.md`（端口 calibrated）；`docs/detail/registry/acsd.phase2.resample.md`（端口 calibrated） |
| `DATA-P1-COS` | mapped | `eng/contracts/schemas/unified/signal.schema.json` | single_object | `docs/detail/registry/acsd.phase1.cosmetic.md`（ID 定义节、端口 cleaned、权威名说明）；`docs/engineering/api/PUBLIC_API`（cosmetic 条款） |
| `DATA-P1-COSMETIC` | mapped | `eng/contracts/schemas/unified/signal.schema.json` | superseded_placeholder | `docs/detail/registry/acsd.phase1.cosmetic.md`（descriptor 占位名说明）；`docs/detail/registry/acsd.phase1.star-psf.md`（端口 cleaned）；`docs/detail/registry/acsd.phase1.star-detection.md`（端口 image） |
| `DATA-P1-DRZ` | mapped | `eng/contracts/schemas/unified/signal.schema.json`；`eng/contracts/schemas/unified/variance.schema.json`；`eng/contracts/schemas/unified/ivar.schema.json`；`eng/contracts/schemas/unified/support.schema.json`；`eng/contracts/schemas/unified/coverage.schema.json` | composite | `docs/detail/registry/acsd.phase1.drizzle.md`（ID 定义节、upstream 引用、模块级数据合同说明）；`docs/engineering/api/PUBLIC_API`（drizzle 条款）；`docs/detail/registry/acsd.phase1.hips-writer.md`（端口 stacked） |
| `DATA-P1-SOURCES` | no_canonical | **无 canonical 对应**（descriptor_port_catalog） | orchestration_port_catalog_name | `docs/detail/registry/acsd.phase1.noise-snr.md`（下游计数行 n_sources 的引用）、`docs/detail/registry/acsd.phase1.photometry.md`（端口链 psf→sources→fluxes）、`docs/detail/registry/acsd.phase1.star-psf.md`（端口链）；`docs/engineering/api/PUBLIC_API`（photometry descriptor 端口编目，带 data_id）；`docs/engineering/architecture/MODULE_MAP`（psf 模块） |
| `DATA-P2-SMP` | no_canonical | **无 canonical 对应**（module_io_contract） | module_io_contract_without_object_semantics | `docs/detail/registry/acsd.phase2.sample.md`（ID 定义节）；`docs/engineering/api/PUBLIC_API`（sampling 条款）；`docs/detail/registry/acsd.phase2.upm-fit.md` |
| `DATA-GAIA-001` | no_canonical | **无 canonical 对应**（external_catalog_contract） | external_catalog_service_contract | `docs/science/unified/DATA_SEMANTICS.md`「星表行语义」一节（ID 定义节）；`docs/engineering/api/PUBLIC_API`（目录访问条款）；`docs/engineering/data/ARTIFACTS`（登记行）；`docs/engineering/governance/TRACEABILITY` 的模块与源码追溯矩阵（DATA 行）；`docs/detail/infrastructure/22_gaia_xpsd_client.md` |

逐条判定依据（含证据原文与逐行锚）：本节「合同 ID → 统一对象映射（MODULE_MAP 引用面）」的表与 `eng/contracts/data/unified_object_compatibility_map_v1.json#legacy_contract_id_map`（7 条，含 `decision` / `relation` / `canonical_objects` / `contract_faces`）。

小结：**mapped = 4**（DATA-P1-CAL / DATA-P1-COS / DATA-P1-COSMETIC / DATA-P1-DRZ），**无 canonical 对应 = 3**（DATA-P1-SOURCES / DATA-P2-SMP / DATA-GAIA-001）。机器判据：`eng/contracts/data/unified_object_compatibility_map_v1.json#legacy_contract_id_map` 的 7 条 `decision` 字段不得为 pending，且该映射须与 ownership 索引逐条一致；当前无仓内执行器，按人读核对。

## 5. 三类配置分离锚点（配置 schema 本体归 CFG-001）

人读正本 = 本节表格；机器可读副本：`eng/contracts/data/config_separation_anchors.json`（`IDX-CONFIG-SEPARATION`）。`eng/packaging/config/**` 与 phase_config schema 的建立见 `docs/engineering/contracts/CONFIG`。

| 配置类 | 语义 | schema owner | 字段名命名空间 | 混入即 REJECT 的字段 |
|---|---|---|---|---|
| `phase_config` | 科学参数 / 输入输出 / 算法选择；可跨机器复现；无硬件字段（硬件字段归 cpu_profile） | CFG-001 | `^sci_[a-z0-9_]+$`；`^(input|output)_[a-z0-9_]+$`；`^algorithm_[a-z0-9_]+$`；`^phase_name$` | `isa`、`isa_level`、`workers`、`worker_count`、`block`、`block_size`、`cpu_model`、`cpu_vendor`、`thread_budget`、`affinity_mask` |
| `cpu_profile` | ISA / workers / block / 机器绑定；仅 benchmark 生成；缓存于程序安装目录；无 profile → 保守运行不阻塞 | CFG-001 | `^isa(_[a-z0-9_]+)?$`；`^(workers|worker_count)$`；`^block(_[a-z0-9_]+)?$`；`^cpu_[a-z0-9_]+$`；`^thread_budget$`；`^profile_hash$` | `sci_algorithm_id`、`sci_rejection_profile`、`algorithm_choice`、`sci_weight_mode`（该键不存在；权重是阶段二现场派生量） |
| `run_manifest` | 本次运行冻结：源码 SHA / 配置哈希 / 输入输出哈希 / 工具链版本；不承载科学参数与硬件调优 | CFG-001 | `^manifest_(input|output)_hashes$`；`^software_sha$`；`^config_hash$`；`^toolchain_[a-z0-9_]+$`；`^run_id$` | `isa`、`workers`、`block`、`sci_algorithm_id`、`sci_weight_mode`（该键不存在；权重是阶段二现场派生量） |

硬规则：三类配置的字段名模式两两互斥（同名即同义）；`cpu_profile` 的硬件字段出现在 `phase_config` 即 REJECT。
机器判据：`eng/contracts/data/config_separation_anchors.json`（`IDX-CONFIG-SEPARATION`）的命名空间与混入即 REJECT 字段集逐条对读；当前无仓内执行器，按人读核对。

