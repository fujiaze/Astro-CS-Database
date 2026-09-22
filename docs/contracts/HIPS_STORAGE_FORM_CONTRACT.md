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
5. **裸形态体积削减口径**：打洞与包围盒 TRIM 的生效面、失败语义与判据（§7）。

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

机器事实源：`eng/contracts/schemas/hips_storage_form.schema.json`。索引文件是 UTF-8 JSON，**不压缩**。

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
- H2 `manifest.json` 的 `tree` 记录**解压后内容**的条目；`storage` 段记录 `form` / `archive_sha256` / `index_sha256` / `archive_bytes`；
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

## 7. 体积削减（裸形态；负责人裁决 2026-09-22「TRIM 纳入产品格式」）

**两种机制，冻结口径与判据如下**（实测与推导见 `docs/design/PRODUCT_STORAGE_FORM.md` §9、`run/RULING-DOC-01/REPORT.md` 裁决 C）：

| 机制 | 作用层 | 冻结规则 | 失败语义 | 判据（机器可校验） |
|---|---|---|---|---|
| **T1 文件系统打洞** | 文件分配层，**字节不变** | 只对 **4 KiB 对齐的整块全零区域**打洞；打洞在 `fsync` 之后、算哈希与原子发布之前完成；`st_size` 必须不变 | **不 fail-closed**：跳过、保留完整 `.fits`、provenance 记 `trim=skipped(reason)` | ① 打洞前后整文件 `sha256` 相同；② cfitsio 与 astropy 两路读器逐 HDU header+像素全等；③ `DATASUM`/`CHECKSUM` 自洽；④ 跨洞边界 `pread` 全等；⑤ `st_blocks` 必须下降（未下降 ⇒ 判红，不静默通过） |
| **T2 包围盒 TRIM**（WD-HiPS-2.0 §4.3.2） | 瓦片内容层，**改 FITS 结构** | 仅当产品显式声明该形态（`properties`/provenance 记 TRIM 关键字与原始 `ONAXIS1/ONAXIS2`）且下游读端 TRIM-aware；**默认不启用** | 读端不认 TRIM 关键字 ⇒ **fail-closed**（拒绝），禁止按缩小后的 NAXIS 静默继续 | ① 补边后像素与未 TRIM 同源瓦片逐字节全等（补边位模式 = IEEE NaN）；② `ONAXIS1/ONAXIS2` == 未 TRIM 的 NAXIS1/NAXIS2；③ 负例：边距改 0.0 或错位 1 像素必须判红 |

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
- 检查器：`python3 eng/tools/hipsform/check_hips_storage_form.py --root .`（exit 0 = PASS）；
- 负例自检：`python3 eng/tools/hipsform/check_hips_storage_form.py --self-test`（恒 0 = 全部内置正/负例符合预期）；
- CI 登记：`eng/ci/checks.json` 的 `CHK-HIPS-STORAGE-FORM`。
