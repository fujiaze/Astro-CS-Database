# 阶段间产品交换

上游：最高设计的顶层结构与 I/O 与原子产品两章。

阶段之间磁盘产品的角色与类型强绑定、输入面校验、无效值处置与兼容性的正本。

## 目的与范围

本合同在三阶段隔离运行（Phase1 / Phase2 / Phase3 各自独立进程；最高设计 ：一次 CLI 调用只驱动一个阶段、无 `--phases 1,2,3`；：跨阶段只交换磁盘产品）
之上定义**产品级交换合同**：定义三个阶段产品角色
`phase1_product_v1` / `phase2_mosaic_v1` / `phase3_planar_fits_v1` 的输入/输出兼容矩阵，
并声明跨 Phase **仅磁盘交换**。

本文档与下列文件共同构成可机器校验的冻结合同：

| 文件 | 角色 |
|---|---|
| `eng/contracts/data/phase_product_exchange.schema.json` | 交换对象文档形态（JSON Schema，权威文档形态） |
| `eng/contracts/data/phase_product_exchange_matrix.json` | 兼容矩阵真源（role 绑定 / edges / 拒绝条件；validator 读取） |
| `lib/infrastructure/aio/runtime/artifact_store/phase_product_exchange_validator.py` | 执行校验器（与 schema 一一对应；须同步修改） |
| `eng/contracts/data/examples/*.example.json` | 示例（phase1/phase2/phase3 产品 + 外部 fixture） |

下游接线：`ARTIFACT_STORE.md`（生产 ArtifactStore 接线）、
`../architecture/ARCHITECTURE.md`（阶段隔离运行时）、`../../science/IO_002_HIPS_INPUT_INTERFACE.md` 与
`../contracts/ATOMIC_PUBLISH.md`（HiPS 输入 / 原子输出）。

## 阶段产品角色与 type 绑定（role ↔ type）

角色命名：`phase1_product_v1` / `phase2_mosaic_v1` / `phase3_planar_fits_v1`。
角色是**交换语义名**；type_id 是 `eng/contracts/data/artifact_types.registry.json` 已登记的类型化产物标识。二者**强绑定**——
role 与 type 逐一对应（同名必同 type / 同 type 必同 role，无歧义）。

| product_role | type_id（registry） | registry schema_version | producer_phase | 最小平面集 | content format |
|---|---|---|---|---|---|
| `phase1_product_v1` | `acsd.phase1.frame_hips.v1` | 1 | phase1 | signal, support, variance, mask | hips |
| `phase2_mosaic_v1` | `acsd.phase2.mosaic_hips.v1` | 1 | phase2 | signal, support, mask | hips |
| `phase3_planar_fits_v1` | `acsd.phase3.planar_fits.v1` | 1 | phase3 | signal, support, mask | fits |

- type_id 本体登记于 `eng/contracts/data/artifact_types.registry.json`，本文档/本 schema
 只做绑定，不重复登记；未知/未登记 type_id 由该 registry 拒绝并传播。
- 内容语义（最小平面集）：signal=科学表面亮度、support=覆盖/有效支持度 [0,1]、
 variance=逐像素随机方差（信号单位²，Drizzle 传播）、mask=坏点/质量位掩码；
 units 权威 = properties BUNIT / ARTIFACTS.md / DATA_SEMANTICS.md（本层强制显式声明）。

### a. 内容格式与角色绑定

- `phase1_product_v1`、`phase2_mosaic_v1` → 内容格式 **hips**（HEALPix NESTED，
 唯一允许 ordering；tile_width 默认 512，`leaf_order=tile_order+9`，DATA_SEMANTICS 「交换对象」一节/「跨 Phase 仅磁盘交换」一节）。
- `phase3_planar_fits_v1` → 内容格式 **fits**（平面 WCS FITS，TAN 投影唯一，
 CUNIT=deg、CRPIX/CRVAL/CD-only、1-based；SCI-P3 「跨 Phase 仅磁盘交换」一节a/a）。
- 交换对象 `product_content.geometry.format` 必须与 role 绑定一致（validator 拒绝 format/role 不匹配）。

## 交换对象（exchange object）结构

跨阶段交换的对象不是裸文件或裸 manifest，而是**交换对象文档**（schema：
`eng/contracts/data/phase_product_exchange.schema.json`）：

```jsonc
{
 "exchange_schema": "acsd.phase-product-exchange/v1",
 "exchange_version": 1,
 "product_role": "phase1_product_v1", // phase2_mosaic_v1 | phase3_planar_fits_v1
 "type_id": "acsd.phase1.frame_hips.v1", // 与 role 绑定一致
 "schema_version": 1, // 与 registry 一致
 "origin": "acsd", // acsd | external_fixture
 "artifact_manifest": { /* 完整 manifest（15 必填字段；形态 = eng/contracts/data/artifact_manifest.schema.json） */ },
 "product_content": {
 "content_schema": "acsd.phase-product-content/v1",
 "coordinate": { "frame": "icrs", "ra_unit": "deg", "dec_unit": "deg" },
 "geometry": { "format": "hips", "hips": { "ordering": "nested", "tile_width": 512 } },
 "planes": [
 { "plane_id": "signal", "units": "ADU/sr", "dtype": "float32", "invalid_policy": "nan_or_support_le_0" },
 { "plane_id": "support", "units": "dimensionless", "dtype": "float32", "invalid_policy": "nan_or_support_le_0" },
 { "plane_id": "variance", "units": "ADU^2/sr^2", "dtype": "float32", "invalid_policy": "nan_or_support_le_0" },
 { "plane_id": "mask", "units": "bitmask", "dtype": "u8", "invalid_policy": "nan_or_support_le_0" }
 ],
 "invalid_policy": "nan_or_support_le_0"
 }
}
```

### a. 组成块

- **artifact_manifest**：跨阶段合并 manifest（正本形态 = `eng/contracts/data/artifact_manifest.schema.json`，15 必填字段 +
 严格语义：未知 type 拒、digest sha256/64hex、status 枚举等）。交换资格另要求
 `status=COMPLETE` 且 `content_digest` 完整。
- **product_content**：**units 载体**。合并 manifest 顶层无 units 字段，因此交换层把
 `units/coordinate/dtype/planes/invalid` 作为产品内容证据显式声明：
 - `coordinate`：frame 必须 `icrs`（唯一允许；galactic/ecliptic 显式拒绝，
 DATA_SEMANTICS 「阶段产品角色与 type 绑定」一节 + SCI-P3 「跨 Phase 仅磁盘交换」一节a）；RA/Dec 单位 `deg`。
 - `geometry`：format + 结构子块（hips→ordering/tile_width；fits→projection/wcs）。
 - `planes`：每平面显式 `plane_id/units/dtype/invalid_policy`；plane_id 集合
 `{signal, support, variance, ivar, mask}`（机器真源 = `eng/contracts/data/phase_product_exchange.schema.json`
 的 `plane_id` enum 与 `lib/infrastructure/aio/runtime/artifact_store/phase_product_exchange_validator.py`
 的 `_PLANE_ID_SET`）；units 非空且取实义字符串（占位/空串/首尾空白一律拒绝）。

 - **稀疏绝对 SNR 控制点层（`sparse_snr_layer`，可选标准层）**：**不是**交换对象 `planes` 枚举的一员，
 而是插入 HiPS 文件内的标准层；对象形态与机器校验承载于
 `eng/contracts/schemas/unified/sparse_snr_layer.schema.json`（统一对象 `sparse_snr_layer`，
 对象语义见 「交换对象」一节）：
 - 层语义（最高设计 ，强制）：控制点值 = 该点的**绝对**通量型 SNR `SNR_c = F_ref / σ_F,c`
 （与帧级 SNR 同物理定义、同逐帧参考通量 `F_ref`），`units` = `dimensionless`（无量纲信噪比）。
 消费时由控制点**直接重建**为稠密 SNR 场 `SNR(p)`（重建算子显式声明、在控制点处精确复现节点值、
 并返回预测方差）；该层只作 SNR 表示、不进权重面（最高设计 ：HiPS 里只**存**
 **帧级 SNR** 与**稀疏绝对 SNR 控制点**；权重是阶段二现场派生量 `w(p) = SNR(p)²/F_ref²`）。
 - 该层**可选**（`sparse_snr_layer=true` 时存在，默认 true），**不属于** 「阶段产品角色与 type 绑定」一节 的最小平面集，
 也不进交换对象 `product_content.planes`。

 - `invalid_policy`：全局 `nan_or_support_le_0`（NaN 或 support<=0 视为无效；
 DATA_SEMANTICS 「兼容矩阵」一节）。
 - `invalid_handling`（**产品内容证据块**；规则依据 `../../ACSD_DESIGN.md` ，drizzle 侧正本见
 `docs/science/drizzle/DRIZZLE.md`）：

 ```jsonc
 "invalid_policy": "nan_or_support_le_0",
 "invalid_handling": {
 "rule_id": "NAN-SAMPLE-MASK-COVERAGE-NAN",
 "aggregation": "sample_level_mask_with_renormalisation",
 "zero_eligible_samples": "nan_signal_support_le_0",
 "zero_substitution": "forbidden",
 "rejection_counting": "mandatory",
 "count_field": "n_rejected_nonfinite"
 }
 ```

 **定义（精确定义）**

 | 术语 | 定义 |
 |---|---|
 | **合格样本** | 参与聚合的源样本满足：值有限（`isfinite(x_j)`）且几何上覆盖该输出像素（`w_jp > 0`）。**方差是否可用不参与合格性判定**（上游授权 = `../../ACSD_DESIGN.md` 「NaN 采用样本级掩膜」，该条只对 NaN 授权；「每一层只由它的上一层推出」） |
 | **方差可用样本** | 合格样本中 `V_j` 有限且 `V_j > 0` 者：其 `V_j` 计入 `Var_p` |
 | **方差不可用样本** | 合格样本中 `V_j` **有限且 `V_j ≤ 0`** 者（"有覆盖但无方差信息"）：**信号与几何权重照常计入 `F_p`、`W_p`**（保信号、保覆盖），方差项**不计入** `Var_p`；`V_j` 一律取原值，常数、地板或哨兵值一律不参与 |
 | **方差面损坏** | `V_j` 非有限（NaN/±Inf）：按 「兼容矩阵」一节a「NaN/负只表示产品损坏」处置 —— 该样本按不合格样本剔除并计入 `n_rejected_nonfinite_variance`，类别恒为「损坏」并与「方差不可用」分列 |
 | **零合格样本** | 某输出像素的全部候选样本都不合格（或根本没有候选样本） |
 | **无覆盖** | 该输出像素没有任何候选样本（几何无覆盖） |
 | **无效输出** | `signal = NaN` **且** `support ≤ 0`；两者**必须同时**成立（互推） |
 | **有效输出** | `signal` 有限 **且** `support > 0` |

 **分母符号与量纲（正向约束，先于下列规则生效）**

 **规则 1 / 1a / 2 中的「分母」逐分支取值，不是全分支同一个量**（下标 `p` 恒指输出像素）：

 | 分支 | 分母符号 | 分母定义 | 分母量纲 | 方差传播式 |
 |---|---|---|---|---|
 | Phase 1 drizzle drop | **`D_p`** | `D_p = Σ_j a_jp` = `covered_area` | **`sr`（球面面积）** | `variance_p = Σ_j v_j·w_jp² / D_p²`（`DATA_SEMANTICS` 「兼容矩阵」一节a / a） |
 | Phase 2 帧间集成 | **`W_p`** | `W_p = Σ_{合格} w_i`（帧索引 `i`） | `w_i` 的量纲（逆方差权重 ⇒ `1/(signal 量纲)²`） | `Var_p = Σ_{方差可用} V_i·w_i² / W_p²` |
 | Phase 3 投影重采样 | **`W_p`** | `W_p = Σ_{合格} w_j`（邻域样本索引 `j`） | **无量纲**（双线性几何权重） | `Var_p = Σ_{方差可用} u_j·w_j² / W_p²`（`u_j` = 该样本输入方差） |

 **适用域边界（强制）**：规则 1 / 1a / 2 的「分母」按上表**逐分支**取值。
 **`W_p` 在 Phase 1 drizzle drop 分支不成立** —— 该分支的分母是**球面面积** `D_p`
 （量纲 `sr`，= 本文件与 `DATA_SEMANTICS` 的 `covered_area`），**不是**无量纲权重和。
 `D_p` 与 `W_p` 量纲不同，**各自具名、不可互换**：`D_p` 只表示 `covered_area[sr]`，
 权重和一律用 `W_p`（符号唯一性强制与错误代入的量级差见 `DATA_SEMANTICS` 「兼容矩阵」一节a）。

 **处置规则（样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数）** —— 对**每一个聚合算子**
 （Phase 1 的 drizzle drop、Phase 2 的帧间集成、Phase 3 的投影重采样）：

 1. **样本级掩膜（只作用于不合格样本）**：不合格样本（值非有限）**从该输出像素的
 分子、分母、方差三项中一并剔除**（`F_p = Σ_合格 x_j w_jp`、`W_p = Σ_合格 w_jp`），
 并**重新归一**。**方差不可用样本不属此列**：它计入 `F_p` 与 `W_p`（保信号、保覆盖），
 只是不计入 `Var_p`。单个不合格样本**只**作用于该样本自身的项；
 分母**只**含合格样本的权重（保留被剔除样本的权重会引入系统性偏低）。

 1a. **方差项的乘积表达**：`Var_p = Σ_{方差可用} V_j w_jp² / W_p²`，其中 `W_p` **含方差不可用
 样本的 `w_jp`**（**分母不缩小**，避免方差系统性偏高）。输出像素的 `variance/ivar` 按
 `DATA_SEMANTICS` 「兼容矩阵」一节a 表达为 **`variance=0 ∧ ivar=0`（显式不可用）**；该情形只写 0
 —— NaN 保留给「无覆盖」（/ ）。
 2. **覆盖级 NaN**：仅当 `W_p = 0`（零合格样本）时，输出 `signal = NaN`、`variance = NaN`、
 `support = 0`。NaN 是**无效的唯一表示**；`0`、`±Inf` 或任意哨兵值一律不表示无效。

 3. **强制计数（剔除一律显式计数）**：每个输出像素**必须**同时暴露被剔除样本的计数
 `n_rejected_nonfinite`（按原因分类：值非有限 / 方差非有限 / 权重非正）。
 计数为 0 与「字段缺失」**必须可区分**；满足本规则的判据 = 该计数字段在场。
 **Phase3 承载面（冻结）**：`docs/science/unified/DATA_SEMANTICS`
 —— 诊断统计平面 `<p3 out_dir>/p3_rejection.bin`
 （int32、W×H、行主序，0 即「无」、禁 −1 哨兵）+ `p3_resampled.json`
 顶层 `diagnostic_planes.n_rejected_nonfinite`（`count_field` 逐字等于
 `n_rejected_nonfinite`、`per_pixel=true`）+ `n_rejected_nonfinite_total`。
 **不进** science planes 枚举（沿用 阶段二 `nused`/`nrej` 诊断平面
 先例：诊断平面由 artifact manifest 声明描述，science 枚举零改动）。
 判据 = 强制剔除计数合同判据（六条 G1–G6）。

 4. **帧间集成**：某帧在该像素的输出非有限时，该帧作为**候选被剔除并计数**；
 判 `INVALID_INPUT` 的条件 = 全部候选非有限，单帧非有限只剔除该帧（`integrate.cpp` 合同：仅
 「全部候选非有限」才报无效）。零合格候选 ⇒ 规则 2。

 **与两类输入的对应**

 | 输入情形 | 处置 | 产品表现 |
 |---|---|---|
 | 无覆盖（帧足迹外、drop 未触及） | 无候选样本 | `NaN / support=0`，`n_rejected=0` |
 | 无信息（该源像素在**全部**帧都非有限） | 有候选、零合格 | `NaN / support=0`，`n_rejected>0` |
 | 坏点 / 坏列 / 饱和 / 宇宙线（部分帧或部分样本坏） | 掩膜 + 重归一 | **有限值** + `support>0` + `n_rejected>0` + `variance>0`（该像素无方差不可用样本，见下一行） |
 | 有覆盖但该源像素方差不可用（`V_j` 有限且 `V_j ≤ 0`） | 计入 `F_p`/`W_p`，不计入 `Var_p` | **有限值** + `support>0` + `variance=0 ∧ ivar=0`（方差面显式不可用） |
 | 方差面损坏（`V_j` 非有限） | 该样本按不合格样本剔除并计数 | `n_rejected_nonfinite_variance>0`（「兼容矩阵」一节a：非有限 = 产品损坏） |
 | 全部样本合格 | 无剔除 | 有限值 + `support>0` + `n_rejected=0` + `variance>0` |

 **下游可判定性**：下游可仅凭 `(isnan(signal), support, n_rejected, variance==0 ∧ ivar==0)`
 四元组（`n_rejected` 取无效值处置规则 3 的该像素计数）把上表六行**完全分开**
 （判据 = `eng/tests/artifact/test_phase_product_exchange.py` 的逐行正/负例，
 受 「兼容矩阵」一节a 三态表与本节规则 1/1a/2 约束；「有覆盖但方差不可用」行由
 `signal` 有限 ∧ `support>0` ∧ `variance=0 ∧ ivar=0` 与「全部样本合格」行的
 `variance>0` 区分，与「无覆盖」行的 `signal=NaN` 区分，与「坏样本」行的
 `n_rejected>0` 区分）。因此「无覆盖」与「有覆盖但全坏」在**诊断层**可区分，在
 **科学语义层**同为「无效」；「有覆盖但方差不可用」在**科学语义层**为**有效**
 （`signal` 有限、`support>0`），只在**方差面**显式不可用 —— 这与
 「兼容矩阵」一节a 三态表一致（无覆盖 ⇒ `NaN`；有覆盖但方差不可用 ⇒
 `variance=0 ∧ ivar=0`），并与该文件 （`ivar==0` = 合法零权重、`variance==0` = 无信息）一致。

 **一句话版本**：**NaN 在重采样与集成中按「样本级掩膜、重归一、覆盖级 NaN、强制计数」处置**：
 不合格样本从聚合的分子/分母/方差中一并剔除并重新归一（无效面的表示限于覆盖级；
 剔除项一律显式计数）；仅当零合格样本时输出 `signal=NaN ∧ support≤0`（该形态下 `support` 取 0），
 且每个输出像素必须暴露被剔除样本计数。
 **方差可用性是独立通道**：`V_j` 有限且 `≤ 0` 的合格样本照常贡献 `F_p`/`W_p` 与覆盖，
 方差面写 `variance=0 ∧ ivar=0`（显式不可用，取值与 clamp/常数/地板结果可区分）。

 **口径归属**：NaN 处置以 `rule_id = NAN-SAMPLE-MASK-COVERAGE-NAN` 为准（`../../ACSD_DESIGN.md` ）；
 `../standards/NUMERIC.md` 的必须遵守条目与标准符合性登记
 （D.drizzle `DISP-DRZ-004`）引用同一份文字。
 **机器形态**：`invalid_handling` 是本节规则块的名称（`rule_id = NAN-SAMPLE-MASK-COVERAGE-NAN`），
 **不是**交换对象的文档键；机器强制面 = 诊断统计平面的 `diagnostic_planes.n_rejected_nonfinite`
- **origin**：`acsd`（本产品任一 Astro Celestial Sphere Database（ACSD） phase run 原子发布产物）或
 `external_fixture`（ACSD 之外生成、完整满足证据要求的合同兼容 HiPS/FITS 测试/审核对象）。
 origin 只描述来源，**不放松任何证据要求**。

### b. 最小平面集

- `phase1_product_v1`：signal + support + variance + mask（Phase1 输出单帧标准化 HiPS 含
 SCI/variance/support/mask 四平面；13 标准 「交换对象」一节）。
- `phase2_mosaic_v1`：signal + support + mask（马赛克产物信号/覆盖/质量；13 标准 「交换对象」一节）。
- `phase3_planar_fits_v1`：signal + support + mask（SCI/SUPPORT/MASK + WCS；13 标准 「交换对象」一节）。

## 跨 Phase 仅磁盘交换（R-DISK-ONLY）

```text
Phase1 ──(原子发布: 磁盘 HiPS + manifest/hash/provenance)──> [磁盘] ──> Phase2
Phase2 ──(原子发布: 磁盘 mosaic HiPS + manifest/hash/provenance)──> [磁盘] ──> Phase3
Phase3 ──(原子发布: 磁盘 planar FITS + manifest/hash/provenance)──> [磁盘] ──> 外部消费者
```

- **rule_id `R-DISK-ONLY`**：任一 Phase 进程只接受另一 Phase 通过原子发布 + 完整 manifest
 （hash/provenance）产生的**磁盘产品**；跨 Phase 传输面限于磁盘产品（进程内对象 / ArtifactHandle / run 上下文不承载交换）；
 单进程串联不在合同面内（最高设计 ：一次 CLI 调用只驱动一个阶段、无 `--phases 1,2,3`；
 阶段隔离运行时见 `../architecture/ARCHITECTURE.md`）。
- 磁盘交换是唯一跨 Phase 通道：无共享内存、无进程内 registry 直连、无隐式文件路径猜测。
- 交换对象文档中的 `artifact_manifest.run.run_id` **仅溯源**，绝不作为接收方进程内匹配依据。

## 兼容矩阵（machine-readable 真源）

机器可校验矩阵真源：`eng/contracts/data/phase_product_exchange_matrix.json`
（`compatibility_edges[]` + `roles[]` + `rejection_conditions[]`）。

| edge_id | 方向 | medium | 绑定规则 | 语义 |
|---|---|---|---|---|
| `E-P1-OUT-P2-IN` | phase1_product_v1 → phase2_input | disk | R-DISK-ONLY, R-NO-RUN-BINDING, R-NO-NAME-BINDING, R-EVIDENCE-REQUIRED | Phase2 输入集合 = 任意数量合同兼容 frame HiPS |
| `E-P2-OUT-P3-IN` | phase2_mosaic_v1 → phase3_input | disk | 同上 | Phase3 接受任一合同兼容 HiPS（含 phase2 mosaic） |
| `E-P1-OUT-P3-IN` | phase1_product_v1 → phase3_input | disk | 同上 | phase3 输入 = 任一合同兼容 HiPS（phase1 frame 或 phase2 mosaic） |
| `E-FIXTURE-P3-IN` | external_fixture → phase3_input | disk | 同上 | **Phase3 可接受外部 fixture**（验收 D2） |
| `E-P2-IN-NO-RUN-BIND` | phase1_product_v1 → phase2_input | disk | R-NO-RUN-BINDING | **Phase2 不要求 Phase1 run ID**（验收 D3） |
| `E-P3-OUT-EXT` | phase3_planar_fits_v1 → external_consumer | disk | R-EVIDENCE-REQUIRED | Phase3 输出面向外部消费 |

矩阵规则（rule_ids 全表）：

| rule_id | 名称 | 内容 |
|---|---|---|
| `R-DISK-ONLY` | 仅磁盘交换 | 「跨 Phase 仅磁盘交换」一节 |
| `R-NO-RUN-BINDING` | 无 run ID 依赖 | 接收方对输入 `producer.run.run_id` / `artifact_id` 与自身 run/session 的异同不做要求；消费资格仅依据 manifest 完整性 + 内容证据。**Phase2 不要求 Phase1 run ID；Phase3 不要求输入来自 Phase2** |
| `R-NO-NAME-BINDING` | 无隐式 artifact name binding | 输入资格与科学语义**绝不根据文件名/目录名/路径猜测**（见 「拒绝条件」一节 展开） |
| `R-EVIDENCE-REQUIRED` | 证据齐备才接受 | 缺 manifest / 缺 hash / 缺 schema（role↔type 不一致或未登记）/ 缺 units → 拒绝 |

## 拒绝条件（缺 manifest / hash / schema / units 拒绝）

交换资格判定 = 结构校验（合并 manifest 全量 + 交换层字段）→ 全部通过才可消费。

| rejection_id | 条件 | 对应验收 |
|---|---|---|
| `X-NO-MANIFEST` | 缺 `artifact_manifest`（或缺其必填字段） | 缺 manifest 拒绝（D4a） |
| `X-NO-HASH` | manifest 缺 `content_digest`（或非 sha256/64hex），或 `status != COMPLETE` | 缺 hash 拒绝（D4b） |
| `X-NO-SCHEMA` | `type_id` 未登记 registry，或 role↔type_id↔schema_version 不一致 | 缺 schema 拒绝（D4c） |
| `X-NO-UNITS` | `product_content` 缺失，或任一必需 plane 缺 units（空/占位/空白） | 缺 units 拒绝（D4d） |
| `X-NAME-BINDING` | 任何把语义绑定到 artifact_id/storage_uri/文件名的尝试 | 无隐式 name binding（D5） |
| `COUNT_FIELD_MISSING` | 产品缺 `diagnostic_planes.n_rejected_nonfinite`（计数为 0 与字段缺失必须可区分） | 强制计数（「交换对象」一节a 规则 3） |

`R-NO-NAME-BINDING` 的判定面：输入资格、角色识别、单位/坐标/平面语义**绝不根据
文件名 / 目录名 / `storage_uri` 尾段 / 路径猜测**；产品角色由 `exchange.product_role` +
`artifact_manifest.type_id` 判定，科学语义只来自 manifest 与 `product_content` 显式字段。
`artifact_id` 是稳定标识，不是输入资格或语义来源；validator 无路径/名称派生代码，
校验不读文件系统（验收 `TestNoImplicitNameBinding.test_artifact_id_arbitrary_does_not_affect_qualification`）。

校验器（`phase_product_exchange_validator.py`）只读交换对象文档字段，**绝不读取/猜测任何
文件路径或 storage_uri 尾段**；不访问磁盘内容；storage_uri 仅做合并 manifest 词法校验
（禁裸路径）。

## 验收映射（依据：「兼容矩阵」一节 + `../../ACSD_DESIGN.md` ）

| 验收 | 实现 |
|---|---|
| D1 分别定义三阶段产品输入/输出兼容矩阵 | `roles[]` + `compatibility_edges[]`（matrix.json）+ 「兼容矩阵」一节 表 |
| D2 Phase3 可接受外部 fixture | `origin=external_fixture`；edge `E-FIXTURE-P3-IN`；示例 `external_fixture_hips.example.json`；测试 `test_external_fixture_phase3_accepted` |
| D3 Phase2 不要求 Phase1 run ID | `R-NO-RUN-BINDING`；edge `E-P2-IN-NO-RUN-BIND`；validator 无 run_id 匹配路径；测试 `test_phase2_input_no_run_id_dependency`（跨 run_id 输入通过） |
| D4a 缺 manifest 拒绝 | `X-NO-MANIFEST`；测试 `test_missing_manifest_rejected` |
| D4b 缺 hash 拒绝 | `X-NO-HASH`；测试 `test_manifest_bad_hash_rejected` |
| D4c 缺 schema 拒绝 | `X-NO-SCHEMA`；测试 `test_missing_schema_rejected`（role↔type 解耦 / 未登记 type） |
| D4d 缺 units 拒绝 | `X-NO-UNITS`；测试 `test_missing_units_rejected` |
| D5 无隐式 artifact name binding | `R-NO-NAME-BINDING`；validator 无路径/名称派生代码；测试 `TestNoImplicitNameBinding.test_artifact_id_arbitrary_does_not_affect_qualification`（详见 「拒绝条件」一节） |
| D6 跨 Phase 仅磁盘交换 | `R-DISK-ONLY`；「跨 Phase 仅磁盘交换」一节；阶段隔离运行时由进程边界强制（见 `../architecture/ARCHITECTURE.md` 与 `ARTIFACT_STORE.md`） |

测试：`eng/tests/artifact/test_phase_product_exchange.py`（正/负测，无第三方依赖）。

## 边界与拒绝面

- 本合同**不重定义科学公式/单位/坐标/平面语义**——units 只做显式声明与强制呈现，不发明单位
 （`ADU`/`ADU^2`/`dimensionless`/`bitmask` 源自 ARTIFACTS.md / DATA_SEMANTICS.md）；
 不新增 registry type（沿用 registry 已登记的三产品 type + calibrated_frame 内部类型）。
- `support`/`coverage` 属覆盖/有效性面，不承载科学权重（「阶段产品角色与 type 绑定」一节）；flux-per-pixel
 与 surface brightness 语义分列（SCI-P3 a.8）——本合同只要求显式声明与强制校验，不重定义科学。
- 三阶段各自独立进程；输入只取磁盘产品，不取另一 phase 的 run 上下文；角色识别只依据 manifest 字段。
- 非目标（alpha 拒绝项延续 SCI-P3 「阶段产品角色与 type 绑定」一节）：多通道/RGBA/lossy HiPS。
- **阶段三接受域（最高设计 ）**：export **接受任一合同兼容 HiPS**（含来自 `normalize`
 的单帧产品，见 「兼容矩阵」一节 `E-P2-OUT-P3-IN`/`E-P1-OUT-P3-IN`）；**variance/ivar 按显式消费并传播**
 （输出 `VARIANCE`/`IVAR` 扩展 HDU），两者皆无时**显式 `unavailable`**（不静默）。
 （`weight`/`support` 冒充方差/逆方差仍**显式拒绝**；flux-per-pixel 输入仍显式拒绝。）

## 文档追溯

SCI-P3（`../../science/PHASE3_HIPS_TO_FITS.md`）/ SCI-DRZ（`../../science/algorithms/DRIZZLE_GEOMETRY.md`）/
`docs/science/unified/DATA_SEMANTICS` → 本交换合同（本文档 + schema + matrix） →
`phase_product_exchange_validator.py` → `test_phase_product_exchange.py`。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/data/ARTIFACTS.md`，同层相关正本。
[3] 内部文档 `docs/engineering/standards/NUMERIC.md`，同层相关正本。
