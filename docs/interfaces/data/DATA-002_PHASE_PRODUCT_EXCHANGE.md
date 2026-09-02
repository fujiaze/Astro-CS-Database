# DATA-002 三阶段产品交换合同（Phase Product Exchange）

> 文档 ID：DOC-DATA-PRODUCT-EXCHANGE-001
> 状态：ACTIVE_NORMATIVE（DATA-002 冻结）
> Owner：SA-DATA-06 ｜ 冻结基线：`b7b2dea70dbcdacdcf6eb762609a908abdeab697` ｜ 冻结日期：2026-09-02
> 上游权威：AstroCS_ENGINEERING_CONSTRAINTS.md §A（产品与阶段）、
> `docs/contracts/DATA_SEMANTICS.md`（跨阶段唯一数据合同）、
> `docs/science/PHASE3_HIPS_TO_FITS.md`（SCI-P3 units/planes）、
> `contracts/data/artifact_types.registry.json` + `contracts/data/artifact_manifest.schema.json`（DATA-001 冻结）
> 机器形态：`contracts/data/phase_product_exchange.schema.json`（schema）、
> `contracts/data/phase_product_exchange_matrix.json`（兼容矩阵真源）、
> `runtime/artifact_store/phase_product_exchange_validator.py`（执行校验器，无第三方依赖）
> 下游：DATA-003（生产 ArtifactStore 接线）、RT-002（phase-isolated runtime）、IO-002/IO-003（HiPS 输入/原子输出）

## 0. 目的与范围

本任务（DATA-002）在三阶段隔离运行（Phase1 / Phase2 / Phase3 各自独立进程，约束 §A3/A4）
之上冻结**产品级交换合同**：定义三个阶段产品角色
`phase1_product_v1` / `phase2_mosaic_v1` / `phase3_planar_fits_v1` 的输入/输出兼容矩阵，
并声明跨 Phase **仅磁盘交换**。

本文档与下列文件共同构成可机器校验的冻结合同：

| 文件 | 角色 |
|---|---|
| `contracts/data/phase_product_exchange.schema.json` | 交换对象文档形态（JSON Schema，权威文档形态） |
| `contracts/data/phase_product_exchange_matrix.json` | 兼容矩阵真源（role 绑定 / edges / 拒绝条件；validator 读取） |
| `runtime/artifact_store/phase_product_exchange_validator.py` | 执行校验器（与 schema 一一对应；须同步修改） |
| `contracts/data/examples/*.example.json` | 示例（phase1/phase2/phase3 产品 + 外部 fixture） |

## 1. 阶段产品角色与 type 绑定（role ↔ type）

角色命名（本任务冻结）：`phase1_product_v1` / `phase2_mosaic_v1` / `phase3_planar_fits_v1`。
角色是**交换语义名**；type_id 是 DATA-001 已登记的类型化产物标识。二者**强绑定**——
role 不允许与 type 解耦（禁止同名不同 type / 同 type 不同 role 的歧义）。

| product_role | type_id（registry） | registry schema_version | producer_phase | 最小平面集 | content format |
|---|---|---|---|---|---|
| `phase1_product_v1` | `astrocs.phase1.frame_hips.v1` | 1 | phase1 | signal, support, variance, mask | hips |
| `phase2_mosaic_v1` | `astrocs.phase2.mosaic_hips.v1` | 1 | phase2 | signal, support, mask | hips |
| `phase3_planar_fits_v1` | `astrocs.phase3.planar_fits.v1` | 1 | phase3 | signal, support, mask | fits |

- type_id 本体登记于 `contracts/data/artifact_types.registry.json`（DATA-001），本文档/本 schema
  只做绑定，不重复登记；未知/未登记 type_id 由 DATA-001 registry 拒绝并传播。
- 内容语义（最小平面集）：signal=科学表面亮度、support=覆盖/有效支持度 [0,1]、
  variance=逐像素随机方差（信号单位²，Drizzle 传播）、mask=坏点/质量位掩码；
  units 权威 = properties BUNIT / DATA_ARTIFACTS.md / DATA_SEMANTICS.md（本层强制显式声明）。

### 1a. 内容格式与角色绑定

- `phase1_product_v1`、`phase2_mosaic_v1` → 内容格式 **hips**（HEALPix NESTED，
  唯一允许 ordering；tile_width 默认 512，`leaf_order=tile_order+9`，DATA_SEMANTICS §2/§3）。
- `phase3_planar_fits_v1` → 内容格式 **fits**（平面 WCS FITS，TAN 投影唯一，
  CUNIT=deg、CRPIX/CRVAL/CD-only、1-based；SCI-P3 §3a/§9a）。
- 交换对象 `product_content.geometry.format` 必须与 role 绑定一致（validator 拒绝 format/role 不匹配）。

## 2. 交换对象（exchange object）结构

跨阶段交换的对象不是裸文件或裸 manifest，而是**交换对象文档**（schema：
`contracts/data/phase_product_exchange.schema.json`）：

```jsonc
{
  "exchange_schema": "astrocs.phase-product-exchange/v1",
  "exchange_version": 1,
  "product_role": "phase1_product_v1",        // phase2_mosaic_v1 | phase3_planar_fits_v1
  "type_id": "astrocs.phase1.frame_hips.v1",  // 与 role 绑定一致
  "schema_version": 1,                        // 与 registry 一致
  "origin": "astrocs",                        // astrocs | external_fixture
  "artifact_manifest": { /* DATA-001 完整 manifest（15 必填字段） */ },
  "product_content": {
    "content_schema": "astrocs.phase-product-content/v1",
    "coordinate": { "frame": "icrs", "ra_unit": "deg", "dec_unit": "deg" },
    "geometry": { "format": "hips", "hips": { "ordering": "nested", "tile_width": 512 } },
    "planes": [
      { "plane_id": "signal", "units": "ADU", "dtype": "float32", "invalid_policy": "nan_or_support_le_0" },
      { "plane_id": "support", "units": "dimensionless", "dtype": "float32", "invalid_policy": "nan_or_support_le_0" },
      { "plane_id": "variance", "units": "ADU^2", "dtype": "float32", "invalid_policy": "nan_or_support_le_0" },
      { "plane_id": "mask", "units": "bitmask", "dtype": "u8", "invalid_policy": "nan_or_support_le_0" }
    ],
    "invalid_policy": "nan_or_support_le_0"
  }
}
```

### 2a. 组成块

- **artifact_manifest**：DATA-001 冻结合并 manifest（`artifact_manifest.schema.json` 15 必填字段 +
  严格语义：未知 type 拒、digest sha256/64hex、status 枚举等）。交换资格另要求
  `status=COMPLETE` 且 `content_digest` 完整。
- **product_content**：**units 载体**。DATA-001 manifest 顶层无 units 字段，因此交换层把
  `units/coordinate/dtype/planes/invalid` 作为产品内容证据显式声明：
  - `coordinate`：frame 必须 `icrs`（唯一允许；galactic/ecliptic 显式拒绝，
    DATA_SEMANTICS §1 + SCI-P3 §3a）；RA/Dec 单位 `deg`。
  - `geometry`：format + 结构子块（hips→ordering/tile_width；fits→projection/wcs）。
  - `planes`：每平面显式 `plane_id/units/dtype/invalid_policy`；plane_id 集合
    `{signal, support, variance, ivar, mask}`；units 非空、禁止占位/空串/首尾空白。
  - `invalid_policy`：全局 `nan_or_support_le_0`（NaN 或 support<=0 视为无效；
    DATA_SEMANTICS §4）。
- **origin**：`astrocs`（本产品任一 AstroCS phase run 原子发布产物）或
  `external_fixture`（AstroCS 之外生成、完整满足证据要求的合同兼容 HiPS/FITS 测试/审核对象）。
  origin 只描述来源，**不放松任何证据要求**。

### 2b. 最小平面集

- `phase1_product_v1`：signal + support + variance + mask（Phase1 输出单帧标准化 HiPS 含
  SCI/variance/support/mask 四平面；13 标准 §2）。
- `phase2_mosaic_v1`：signal + support + mask（马赛克产物信号/覆盖/质量；13 标准 §2）。
- `phase3_planar_fits_v1`：signal + support + mask（SCI/SUPPORT/MASK + WCS；13 标准 §2）。

## 3. 跨 Phase 仅磁盘交换（R-DISK-ONLY）

```text
Phase1 ──(原子发布: 磁盘 HiPS + manifest/hash/provenance)──> [磁盘] ──> Phase2
Phase2 ──(原子发布: 磁盘 mosaic HiPS + manifest/hash/provenance)──> [磁盘] ──> Phase3
Phase3 ──(原子发布: 磁盘 planar FITS + manifest/hash/provenance)──> [磁盘] ──> 外部消费者
```

- **rule_id `R-DISK-ONLY`**：任一 Phase 进程只接受另一 Phase 通过原子发布 + 完整 manifest
  （hash/provenance）产生的**磁盘产品**；禁止进程内对象 / ArtifactHandle / run 上下文直传；
  禁止单进程自动串联（约束 §A4：无 `--phases 1,2,3`；RT-002 phase-isolated runtime）。
- 磁盘交换是唯一跨 Phase 通道：无共享内存、无进程内 registry 直连、无隐式文件路径猜测。
- 交换对象文档中的 `artifact_manifest.run.run_id` **仅溯源**，绝不作为接收方进程内匹配依据。

## 4. 兼容矩阵（machine-readable 真源）

机器可校验矩阵真源：`contracts/data/phase_product_exchange_matrix.json`
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
| `R-DISK-ONLY` | 仅磁盘交换 | §3 |
| `R-NO-RUN-BINDING` | 无 run ID 依赖 | 接收方不得要求输入 `producer.run.run_id` / `artifact_id` 与自身 run/session 相同或可解析；消费资格仅依据 manifest 完整性 + 内容证据。**Phase2 不要求 Phase1 run ID；Phase3 不要求输入来自 Phase2** |
| `R-NO-NAME-BINDING` | 无隐式 artifact name binding | 输入资格、角色识别、单位/坐标/平面语义**绝不根据文件名/目录名/storage_uri 尾段/路径猜测**；产品角色由 `exchange.product_role` + `artifact_manifest.type_id` 判定；科学语义只来自 manifest 与 product_content 显式字段。artifact_id 是稳定标识，不是输入资格或语义来源 |
| `R-EVIDENCE-REQUIRED` | 证据齐备才接受 | 缺 manifest / 缺 hash / 缺 schema（role↔type 不一致或未登记）/ 缺 units → 拒绝 |

## 5. 拒绝条件（缺 manifest / hash / schema / units 拒绝）

交换资格判定 = 结构校验（DATA-001 manifest 全量 + 交换层字段）→ 全部通过才可消费。

| rejection_id | 条件 | 对应验收 |
|---|---|---|
| `X-NO-MANIFEST` | 缺 `artifact_manifest`（或缺 DATA-001 必填字段） | 缺 manifest 拒绝（D4a） |
| `X-NO-HASH` | manifest 缺 `content_digest`（或非 sha256/64hex），或 `status != COMPLETE` | 缺 hash 拒绝（D4b） |
| `X-NO-SCHEMA` | `type_id` 未登记 registry，或 role↔type_id↔schema_version 不一致 | 缺 schema 拒绝（D4c） |
| `X-NO-UNITS` | `product_content` 缺失，或任一必需 plane 缺 units（空/占位/空白） | 缺 units 拒绝（D4d） |
| `X-NAME-BINDING` | 任何把语义绑定到 artifact_id/storage_uri/文件名的尝试 | 无隐式 name binding（D5） |

校验器（`phase_product_exchange_validator.py`）只读交换对象文档字段，**绝不读取/猜测任何
文件路径或 storage_uri 尾段**；不访问磁盘内容；storage_uri 仅做 DATA-001 词法校验
（禁裸路径）。

## 6. 验收映射（tasks/03_RUNTIME_DATA_IO_TASKS.md DATA-002）

| 验收 | 实现 |
|---|---|
| D1 分别定义三阶段产品输入/输出兼容矩阵 | `roles[]` + `compatibility_edges[]`（matrix.json）+ §4 表 |
| D2 Phase3 可接受外部 fixture | `origin=external_fixture`；edge `E-FIXTURE-P3-IN`；示例 `external_fixture_hips.example.json`；测试 `test_external_fixture_phase3_accepted` |
| D3 Phase2 不要求 Phase1 run ID | `R-NO-RUN-BINDING`；edge `E-P2-IN-NO-RUN-BIND`；validator 无 run_id 匹配路径；测试 `test_phase2_input_no_run_id_dependency`（跨 run_id 输入通过） |
| D4a 缺 manifest 拒绝 | `X-NO-MANIFEST`；测试 `test_missing_manifest_rejected` |
| D4b 缺 hash 拒绝 | `X-NO-HASH`；测试 `test_missing_hash_rejected` |
| D4c 缺 schema 拒绝 | `X-NO-SCHEMA`；测试 `test_missing_schema_rejected`（role↔type 解耦 / 未登记 type） |
| D4d 缺 units 拒绝 | `X-NO-UNITS`；测试 `test_missing_units_rejected` |
| D5 无隐式 artifact name binding | `R-NO-NAME-BINDING`；validator 无路径/名称派生代码；测试 `test_no_implicit_name_binding`（artifact_id 任意稳定标识、storage_uri 不参与资格判定均通过；校验不读文件系统） |
| D6 跨 Phase 仅磁盘交换 | `R-DISK-ONLY`；§3；RT-002 在运行时隔离后由进程边界强制（DATA-003/RT-002 接线） |

测试：`tests/artifact/test_phase_product_exchange.py`（正/负测，无第三方依赖）。

## 7. 边界与禁止

- 本任务**不改科学公式/单位/坐标/平面语义**——units 只做显式声明与强制呈现，不发明单位
  （`ADU`/`ADU^2`/`dimensionless`/`bitmask` 源自 DATA_ARTIFACTS.md / DATA_SEMANTICS.md）；
  不新增 registry type（沿用 DATA-001 三产品 type + calibrated_frame 内部类型）。
- 禁止把 `support`/`coverage` 当科学权重（DATA_ARTIFACTS.md §1）；禁止 flux-per-pixel
  冒充 surface brightness（SCI-P3 §9a.8）——本合同只要求显式声明与强制校验，不重定义科学。
- 禁止同进程自动串联；禁止把另一 phase 的 run 上下文当输入；禁止以文件名/路径识别角色。
- 非目标（alpha 拒绝项延续 SCI-P3 §1）：多通道/RGBA/lossy HiPS、variance 输入 Phase3
  （Phase3 不支持 variance/ivar 输入产品，显式拒绝，不做静默丢弃）。

## 8. 文档追溯

`SCI-P3 / SCI-DRZ / DATA-SEMANTICS` → `DATA-002 交换合同（本文档 + schema + matrix）` →
`phase_product_exchange_validator.py` → `test_phase_product_exchange.py` →
`TASK_RESULT.json / TEST_EVIDENCE.json`（DATA-002 证据）。
