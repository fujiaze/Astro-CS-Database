# 落盘形态合同：扩展名、归档布局、索引 schema 与哈希口径

> 上游：ASTROCS_DESIGN.md §10（I/O 与原子产品）、docs/design/PRODUCT_STORAGE_FORM.md（DESIGN-STORAGE-001）

> doc_id: DOC-CONTRACT-STORAGE-001
> doc_status: ACTIVE_NORMATIVE
> 上位：`ASTROCS_DESIGN.md` §10；`docs/design/PRODUCT_STORAGE_FORM.md`
下游：`docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md`、`docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md`、`docs/plugins/infrastructure/17_aio.md`、`docs/plugins/algorithms_phase1/08_drizzle.md`、`docs/plugins/algorithms_phase2/09_coverage.md`
机器事实源：`eng/contracts/schemas/hips_storage_form.schema.json`（索引文件 schema）
机器检查器：`eng/tools/hipsform/check_hips_storage_form.py`（CI 检查 `CHK-HIPS-STORAGE-FORM`）

## 1. 冻结面

本合同冻结四件事，四者都是**机器可校验**的：

1. **命名与扩展名**：产品落盘名的唯一两种形态（§2）；
2. **归档容器布局**：tar 流、帧边界、可被标准工具还原（§3）；
3. **索引 schema**：产品级索引与数据集级覆盖索引的字段与不变式（§4）；
4. **哈希口径**：产品身份哈希与容器指纹的分工（§5）；
5. **裸形态体积削减口径**：打洞与包围盒 TRIM 的生效面、失败语义与判据（§7）；
6. **形态的输入配置与输出清单字段**：形态选择键、缺省/留空的 warn 语义、产物自报的索引路径与指纹、清单 storage 段（§10）。

不在此冻结：压缩档位（默认值见 §3.3，可配置）、tar 实现、索引载体的未来演进（§4.5）。

## 2. 命名与扩展名（MUST）

| 规则 | 内容 |
|---|---|
| N1 | 裸形态落盘名 = `<name>.hips`，且必须是**目录** |
| N2 | 归档形态落盘名 = `<name>.hips.zst`，且必须是**常规文件**（不是目录、不是符号链接） |
| N3 | 产品级索引落盘名 = `<name>.hips.index.json`，与产品同父目录 |
| N4 | 数据集级覆盖索引落盘名 = `coverage.index.json`，位于运行输出根 |
| N5 | 同一 `<name>` 的两形态**不得共存**；共存 ⇒ 产品歧义，读端 fail-closed |
| N6 | 归档形态**必须**有产品级索引；索引缺失 ⇒ 产品不完整，读端 fail-closed（不得回退为扫描归档或逐瓦片探测） |
| N7 | 裸形态**允许**无产品级索引；此时读端按目录枚举重建覆盖，并在 provenance 记降级 |
| N8 | `.hips` / `.hips.zst` 中缀只用于 HiPS 产品；非 HiPS 产物（如 Phase3 平面 FITS）不得使用 |

`<name>` 的取值由阶段与块决定，且必须与运行清单中的产品名一致。

## 3. 归档容器布局（MUST）

### 3.1 tar 层

| 规则 | 内容 |
|---|---|
| A1 | 归档内容 = 一个 tar 流的压缩结果；tar 根 = 产品根的内容（成员名以子产品名开头，不含前导 `./`） |
| A2 | tar 成员集合 = 裸形态产品树的**全部常规文件**（`properties`、`NorderK/DirD/NpixN.fits`、`Moc.fits`、`metadata.fits`、产品集 `manifest.json` 等），不含目录项之外的额外文件 |
| A3 | 成员顺序 = 成员路径的**字典序**（确定性）；成员头字段（mtime/uid/gid/uname/gname/mode）取固定值，使同一内容的两次打包字节一致 |
| A4 | 归档内 `properties` 与裸形态 `properties` **逐字节一致**；`hips_tile_format` 必须是既有读端接受的取值（`fits`），**不得**出现 `zstd` / `fits.zst` / 任何非标准 token |
| A5 | 归档内**不得**包含产品级索引或数据集级覆盖索引 |

### 3.2 zstd 层

| 规则 | 内容 |
|---|---|
| Z1 | 归档 = **N 个独立 zstd 帧的串接**；帧边界 = tar 成员边界（一个成员恰好一个帧） |
| Z2 | 帧必须使标准工具可还原完整 tar 流：`zstd -dc <archive>` 的输出必须与打包前的 tar 流**逐字节一致** |
| Z3 | **禁止**使用 zstd skippable frame 在流内混装"不压缩区"：解码器会跳过该区内容，标准工具解压后的产物将缺块，与"解压后合法"冲突 |
| Z4 | **禁止**对瓦片数据做 byte-shuffle 等解压侧需二次反变换的预变换：标准工具解压后必须是合法 FITS |
| Z5 | 归档不得使用需要解压侧额外参数的帧参数（如长距离匹配窗口）：解压侧只允许默认参数 |

### 3.3 压缩档位

- 默认 **zstd level 3**。依据：归档层 level 1→3 的体积差 < 0.1%、level ≥ 9 收益 < 1% 而压缩时间约 7 倍（`docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md`）。
- 档位是**打包参数**，不进产品身份哈希（§5）。

## 4. 索引 schema（MUST）

机器事实源：`eng/contracts/schemas/hips_storage_form.schema.json`。索引文件是 UTF-8 JSON，**不压缩**。同一 schema 还承载形态的输入配置键与输出清单字段（`$defs.frame_storage` / `$defs.coverage_index_ref` / `$defs.manifest_storage`，见 §10）与字段词表 `x-astrocs-field-vocabulary`。

### 4.1 产品级索引 `<name>.hips.index.json`

| 字段 | 类型 | 不变式 |
|---|---|---|
| `index_schema` | string | 恒 `astrocs.hips-index/v1` |
| `product` | string | = `<name>` |
| `storage_form` | enum | `archive` 或 `bare`，必须与磁盘实际形态一致 |
| `archive` | object / null | `archive` 形态必填：`name`（= `<name>.hips.zst` 的 basename）、`bytes`、`sha256`、`frame_unit` 恒 `tar_member`；`bare` 形态恒 null |
| `subproducts[]` | array | 每个子产品一条；`name` 唯一 |
| `subproducts[].hips_order` / `hips_tile_width` / `hips_tile_format` | int / int / string | 必须与归档内（或裸目录内）同名 `properties` 一致 |
| `subproducts[].n_leaf_tiles` | int | = `coverage` 展开后的叶块数 |
| `subproducts[].coverage` | object | `encoding` 恒 `runs`；`leaf_ipix_runs` 为按 ipix 升序、互不重叠、不相邻的 `[start, count]` 段；`frac_quant` 恒 255；`leaf_frac` 与展开顺序一一对应，取值 0..255 |
| `subproducts[].tiles[]` | array | **`archive` 形态必填**：每条 `{ipix, coff, csize, usize, doff, dsize}`；`ipix` 集合必须与 `coverage` 展开集合**完全相同**；`doff+dsize ≤ usize`；`bare` 形态省略 |

不变式（任一不满足 ⇒ 产品损坏，读端 fail-closed）：

- I1 `coverage` 展开集合 = 该子产品在 `hips_order` 阶实际存在的叶瓦片 ipix 集合；
- I2 `tiles[].ipix` 集合 = I1 的集合；
- I3 每条 `tiles` 的 `coff/csize` 必须落在归档字节范围内，且解压 `csize` 字节后长度 = `usize`；
- I4 索引可由产品内容**重算**并逐字段一致。

### 4.2 数据集级覆盖索引 `coverage.index.json`

| 字段 | 类型 | 不变式 |
|---|---|---|
| `index_schema` | string | 恒 `astrocs.coverage-index/v1` |
| `granularity.unit` | string | 恒 `hips_leaf_tile`（**块粒度，禁止逐像素**） |
| `granularity.tile_width` / `hips_order` | int | 与参与产品一致 |
| `frames[]` | array[string] | 参与本次运行的帧标识，升序且唯一 |
| `blocks[].ipix` | int | 叶级 NESTED ipix，升序且唯一 |
| `blocks[].frames[]` | array | `{f, frac}`；`f` ∈ `frames[]`；`frac` ∈ 0..255 |

- 登记语义 = **存在性 + 覆盖分数**（不登记"完全覆盖"单一布尔）。
- 该索引是**派生产物**：可由各产品级索引重算；缺失时的规定回退 = 读入全部产品级索引并现场倒排。

### 4.3 查询语义

- 阶段2 按块查得**候选帧集合**，用于剪枝与调度；**不是**像素级裁决。
- 索引**不得**被当作有效性来源：索引声明覆盖而瓦片缺失/非法 ⇒ 按既有 `MISSING` / `INVALID` 语义处理，不得静默当有效。
- 低阶 hierarchy 瓦片不参与覆盖查询。

### 4.4 粒度依据

登记粒度 = 一个叶 tile（`tile_width`² 像素）。相对逐像素登记，记录数降低 `tile_width²` 倍；且与阶段2 的块级工作流粒度、稀疏层控制点间隔 Δ = tile_width/8 的父级、EXP-02 mesh 父级同构，**不引入第二套几何**。

### 4.5 载体演进

当前载体 = 单个不压缩 UTF-8 JSON 文件（可审计、无新依赖）。当记录数超过 `10^7` 条时允许切换载体（列存/定长二进制表），**但字段模型与不变式不变**；载体切换是独立变更，不在本合同内。

## 5. 哈希口径（MUST）

| 哈希 | 对象 | 用途 | 禁止 |
|---|---|---|---|
| `tree_hash`（**产品身份**） | 解压后的 HiPS 内容：`sorted[{path,size,sha256}]` 的规范 JSON 的 sha256 | 产品身份、跨阶段交换、可重算校验 | — |
| `archive_sha256`（容器指纹） | `<name>.hips.zst` 的字节 | 容器完整性、缓存键 | **不得**用作产品身份 |
| `index_sha256` | `<name>.hips.index.json` 的字节 | 索引完整性 | **不得**用作产品身份 |

- H1 同一产品两形态的 `tree_hash` **必须相同**（身份与打包参数无关）；
- H2 `manifest.json` 的 `tree` 记录**解压后内容**的条目；`storage` 段记录 `storage_form` / `form_source` / `index_path` / `index_sha256` / `archive_bytes` / `archive_sha256`（逐产品一条，字段与不变式见 §10.3）；
- H3 压缩档位、帧切分、tar 头字段变化**不得**改变 `tree_hash`。

## 6. 读路径不变式（MUST）

| 规则 | 内容 |
|---|---|
| R1 | 形态由落盘名判定（后缀 `.hips` / `.hips.zst`），调用方传入的路径无需改写 |
| R2 | 调用方不感知形态：`hips_paths` / `source.hips_dir` 的元素可以是任一形态，语义相同 |
| R3 | 归档形态的瓦片读取 = 一次 `pread` + 一次单帧解压；不得整包解压 |
| R4 | 归档形态的覆盖查询不得触碰归档字节 |
| R5 | 归档形态缺索引 ⇒ fail-closed；裸形态缺索引 ⇒ 目录枚举重建 + provenance 记降级 |
| R6 | 形态解析失败、索引不一致、归档截断 ⇒ 显式错误码，不静默回退 |

## 7. 体积削减（裸形态）

**两种机制，冻结口径与判据如下**（实测与推导见 `docs/design/PRODUCT_STORAGE_FORM.md` §9、`run/RULING-DOC-01/REPORT.md` 裁决 C）：

| 机制 | 作用层 | 冻结规则 | 失败语义 | 判据（机器可校验） |
|---|---|---|---|---|
| **T1 文件系统打洞** | 文件分配层，**字节不变** | 只对 **4 KiB 对齐的整块全零区域**打洞；打洞在 `fsync` 之后、算哈希与原子发布之前完成；`st_size` 必须不变 | **不 fail-closed**：跳过、保留完整 `.fits`、provenance 记 `trim=skipped(reason)` | ① 打洞前后整文件 `sha256` 相同；② cfitsio 与 astropy 两路读器逐 HDU header+像素全等；③ `DATASUM`/`CHECKSUM` 自洽；④ 跨洞边界 `pread` 全等；⑤ `st_blocks` 必须下降（未下降 ⇒ 判红，不静默通过） |
| **T2 包围盒 TRIM**（WD-HiPS-2.0 §4.3.2；FITS 关键字 `TRIM1`/`TRIM2`/`ONAXIS1`/`ONAXIS2`） | 瓦片内容层，**改 FITS 结构** | 仅当产品显式声明该形态（`properties`/provenance 记 TRIM 关键字与原始 `ONAXIS1/ONAXIS2`）且下游读端 TRIM-aware；**默认不启用** | 读端不认 TRIM 关键字 ⇒ **fail-closed**（拒绝），禁止按缩小后的 NAXIS 静默继续 | ① 补边后像素与未 TRIM 同源瓦片逐字节全等（补边位模式 = IEEE NaN）；② `ONAXIS1/ONAXIS2` == 未 TRIM 的 NAXIS1/NAXIS2；③ 负例：边距改 0.0 或错位 1 像素必须判红 |

- **生效面**：仅**裸形态** `<name>.hips/`。**归档形态 `<name>.hips.zst` 两种都不实施**（收益被 zstd 吸收）。
- **产品身份不受影响（T1）**：字节不变 ⇒ 产品哈希不变。**T2 改变内容** ⇒ 启用 T2 的产品与未 TRIM 的同源产品**不同身份**，必须在 `properties` 与 manifest 的 `storage` 段显式区分（不得让同一科学内容因是否 TRIM 而静默产生两个身份）。
- **不得用 T1 的收益口径描述 T2，反之亦然**：T1 实测整产物 2.77%（Linux）/2.10%（Windows），signal/variance/ivar 三层 **0.0000%**（NaN 边距非零字节）；T2 的 **13.31%** 来自缩小 NAXIS。
- **降级**：T1 在卷不支持稀疏（`EOPNOTSUPP` 等）时跳过，产品保持完整可读；T2 在读端不支持时拒绝，不降级为"读小图"。

## 8. 错误语义

| 情形 | 结果 |
|---|---|
| 两形态共存 | 产品歧义错误（fail-closed） |
| 归档形态缺产品级索引 | 产品不完整错误（fail-closed） |
| 索引 `coverage` 与 `tiles` 集合不一致 | 索引损坏错误（fail-closed） |
| 索引声明覆盖但瓦片缺失 | 既有 `MISSING` 语义（不得当有效） |
| 归档截断 / 帧解压长度不符 | 既有 `INVALID` / I/O 错误（fail-closed） |
| 归档内 `properties` 声明非标准 `hips_tile_format` | 既有读端拒绝（`UNSUPPORTED`） |


## 9. 机器校验

- 索引 schema：`eng/contracts/schemas/hips_storage_form.schema.json`；
- 检查器：`python3 eng/tools/hipsform/check_hips_storage_form.py --root .`（exit 0 = PASS；同时跑逐层字段口径一致性判据）；
- 逐层口径：`python3 eng/tools/hipsform/check_hips_storage_form.py --root . --doc-consistency`（每层文档必须出现词表登记的字段名与取值，且不得出现禁用同义名）；
- 负例自检：`python3 eng/tools/hipsform/check_hips_storage_form.py --self-test`（恒 0 = 全部内置正/负例符合预期；覆盖形态键缺省/留空的默认+warn、逐帧索引字段、清单 storage 段、mosaic/export 形态键 REJECT、层间口径不一致）；
- CI 登记：`eng/ci/checks.json` 的 `CHK-HIPS-STORAGE-FORM`。

## 10. 形态的输入配置与输出清单字段（MUST）

形态是**输入配置项**，不是运行期开关；产物必须**自报形态与索引路径**，使不同批次 Phase1 的输出 JSON 可以合并而不丢索引。

字段词表唯一源 = `eng/contracts/schemas/hips_storage_form.schema.json#x-astrocs-field-vocabulary`：字段名、取值、段名与文件名后缀只在那里定义一次，本文档与各层文档只引用，**不得**另立同义名。逐层口径一致性由 `CHK-HIPS-STORAGE-FORM --doc-consistency` 机器断言（缺登记词、出现禁用同义名、取值口径漂移都判红）。

### 10.1 Phase1 输入配置键 `storage_form`

| 项 | 内容 |
|---|---|
| 键名 | `storage_form` |
| 取值 | `archive`（默认）\| `bare` |
| 落点 | Phase1（normalize）输入 JSON 的**块内**键（多块形态 `blocks[].storage_form`）与平铺单块简写的顶层键 |
| 缺省语义 | **键缺失、空串 `""` 或 `null` ⇒ 取默认 `archive`，并报一条 `level=warn` / `event=warn` 事件**（日志合同 `docs/contracts/LOG_AND_ERROR_CONTRACT.md` §2 与 LOG-001 事件模型）；**禁止静默取默认** |
| 显式语义 | 显式给出 `archive` / `bare` ⇒ 按该形态落盘，**不报** warn |
| 形态来源留痕 | 运行完成清单 `manifest.json#storage.form_source` = `config`（显式）/ `default`（缺省）；`default` 是 warn 必须存在的机器证据（§10.3 M2） |
| 禁止 | 该键**只**属 Phase1。Phase2 / Phase3 的输入合同不设该键，出现即 REJECT（§10.5） |

不变式：

- **F0** 缺省 / 留空 ⇒ 解析结果必须是登记的默认值，**且**必须存在一条点名该键的 warn 事件；显式声明 ⇒ 不得存在该 warn。两者缺一即判红（静默取默认与误报 warn 都是故障）。
- 形态**不改变科学结果**：同一输入下两形态的产品内容逐字节一致（H1）。

### 10.2 Phase1 输出清单 `p1_products.json`（加性）

逐帧条目（`frames[]`，与既有 `frame_id` / `hips_path` 同层）新增四个字段：

| 字段 | 类型 | 不变式 |
|---|---|---|
| `storage_form` | enum | `archive` \| `bare`，必须与磁盘实际形态一致 |
| `index_path` | string | 该帧**产品级索引**路径 = `<name>.hips.index.json`（与 `hips_path` 同父目录） |
| `index_sha256` | string | 索引文件字节的 sha256（64hex 小写） |
| `archive_sha256` | string / null | 归档容器指纹；`bare` 形态恒 `null` |

运行级新增 `coverage_index`（加性）：

```json
"coverage_index": {"path": "coverage.index.json", "sha256": "<64hex>",
                   "n_frames": 16, "n_blocks": 523}
```

- **F1** 四个字段一个不少——缺任一 ⇒ 该帧的形态与索引不可追溯，读端 fail-closed。
- **F2** `storage_form=bare` ⇔ `archive_sha256=null`。
- **F3** `index_path` 的 basename 必须等于 `<产品名>.hips.index.json`。
- **F4** `index_sha256` 必须与磁盘索引字节一致；索引必须可由产品内容重算（§4.1 I4）。
- `archive_sha256` **不得**用作产品身份（身份 = `tree_hash`，取解压后内容，§5）。
- 机器事实源：`$defs.frame_storage`（逐帧）与 `$defs.coverage_index_ref`（运行级）。

### 10.3 运行完成清单 `manifest.json#storage`（加性）

```json
"storage": {
  "storage_form": "archive",
  "form_source": "config",
  "products": [
    {"product": "f00", "storage_form": "archive",
     "index_path": "f00.hips.index.json", "index_sha256": "<64hex>",
     "archive_bytes": 12345, "archive_sha256": "<64hex>", "tree_hash": "<64hex>"}
  ],
  "coverage_index": {"path": "coverage.index.json", "sha256": "<64hex>",
                     "n_frames": 16, "n_blocks": 523}
}
```

- 字段名与 §10.2 **同词表**（`storage_form` / `index_path` / `index_sha256` / `archive_sha256`），另加 `archive_bytes` 与产品身份 `tree_hash`。
- **M1** 运行级 `storage_form` 与每个 `products[].storage_form` 一致；`bare` 条目的 `archive_bytes` / `archive_sha256` 必须为 `null`。
- **M2** `form_source=default` ⇒ 必须存在点名 `storage_form` 的 warn 事件；`form_source=config` ⇒ 不得存在。
- **M3** `coverage_index` 非 `null` 时 `path` 的 basename 必须是 `coverage.index.json` 且 `n_blocks` ≥ 1；不产出覆盖索引的运行（Phase3）恒 `null`。
- **M4** `products[].index_path` 的 basename 必须等于 `<product>.hips.index.json`。
- `tree` 记录**解压后内容**的条目（与裸形态相同），`tree_hash` 与 §5 同口径；形态事实只写本段与产品级索引，**不得**写进 HiPS `properties`（§6.1）。
- 机器事实源：`$defs.manifest_storage`。

### 10.4 Phase2 输入：索引引用是加性可选键

- `hips_paths` 的元素**保持字符串**（不做元素对象化）：逐帧产品级索引路径由命名规则派生 —— `<name>.hips` / `<name>.hips.zst` → `<name>.hips.index.json`（与 `hips_path` 同父目录）。
- 额外的「总索引」引用用**加性可选键** `coverage_index`（字符串路径，指向数据集级 `coverage.index.json`）。存在 ⇒ 阶段二启动时载入它做块级查询；缺失 ⇒ 规定回退 = 读入全部产品级索引现场倒排（§4.2）。
- 该键**不**承载形态选择：Phase2 产物固定裸形态（§10.5）。
- 机器事实源：`$defs.coverage_index_ref`。

### 10.5 Phase2 / Phase3：形态键必须 REJECT

| 阶段 | 产物形态 | 输入合同是否含 `storage_form` | 出现该键的后果 |
|---|---|---|---|
| Phase2 mosaic | 固定裸 `<name>.hips/`（服务面，被随机读取） | **不含** | REJECT |
| Phase3 export | 固定裸 FITS（不压缩、不套壳，不使用 `.hips` 中缀） | **不含** | REJECT |

- REJECT 在**两处**生效：schema 面（块内 `additionalProperties:false` / 平铺 `propertyNames`）与 CLI 面（块内未知键门，`validate_config_full`）。
- **为什么不做成「值域只允许 `bare`」**：运行期配置门是**键白名单**，不是 JSON Schema；只收窄值域会让 `storage_form: "archive"` 在运行期**静默透传成 no-op** —— 正是本合同要禁止的静默失效。

### 10.6 合并语义（为什么索引路径必须显式进输出 JSON）

不同批次的 Phase1 输出 JSON 合并成一个数据集时，逐帧 `index_path` / `index_sha256` 随条目一起搬移 ⇒ 索引不会丢、不会指向错产品；运行级 `coverage_index` 是**派生产物**，合并后按 §4.2 由各产品级索引重算，不需要跨批次拼接。
