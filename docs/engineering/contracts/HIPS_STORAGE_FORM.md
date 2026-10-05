# 落盘形态与归档容器合同

上游：最高设计的 I/O 与原子产品一章；`docs/detail/infrastructure/` 的产品存储形态设计。

本文冻结六件事，每件都机器可校验：产品落盘名与扩展名、归档容器布局、产品级与数据集级索引的
schema 与不变式、哈希口径、裸形态体积削减口径、形态的输入配置键与输出清单字段。压缩编码的
选型依据与实测读数落实验域的压缩编码评估证据面。

> 上游：`../../ACSD_DESIGN.md` 的「I/O 与原子产品」一章；`docs/detail/infrastructure/` 的产品存储形态设计
> 机器事实源：`eng/contracts/schemas/hips_storage_form.schema.json`（索引文件 schema）
> 判据载体：逐层字段口径核对（见下文「形态核对与负例」一节），由人工对抗审核执行
> 下游：science 分册的 HiPS 输入接口面、`ATOMIC_PUBLISH.md`、`../../detail/infrastructure/17_aio.md`

## 冻结面

本合同冻结六件事，六者都是**机器可校验**的：

1. **命名与扩展名**：产品落盘名的唯一两种形态（「命名与扩展名」一节）；
2. **归档容器布局**：tar 流、帧边界、可被标准工具还原（「归档容器布局」一节）；
3. **索引 schema**：产品级索引与数据集级覆盖索引的字段与不变式（「索引 schema」一节）；
4. **哈希口径**：产品身份哈希与容器指纹的分工（「哈希口径」一节）；
5. **裸形态体积削减口径**：打洞与包围盒 TRIM 的生效面、失败语义与判据（「体积削减」一节）；
6. **形态的输入配置与输出清单字段**：形态选择键、缺省/留空的 warn 语义、产物自报的索引路径与指纹、清单 storage 段（「形态的输入配置与输出清单字段」一节）。

不在此冻结：压缩档位（默认值见「压缩档位」一节，可配置）、tar 实现、索引载体的未来演进（见「载体演进」一节）。

## 命名与扩展名

| 规则 | 内容 |
|---|---|
| N1 | 裸形态落盘名 = `<name>.hips`，且必须是**目录** |
| N2 | 归档形态落盘名 = `<name>.hips.zst`，且必须是**常规文件**（不是目录、不是符号链接） |
| N3 | 产品级索引落盘名 = `<name>.hips.index.json`，与产品同父目录 |
| N4 | 数据集级覆盖索引落盘名 = `coverage.index.json`，位于运行输出根 |
| N5 | 同一 `<name>` 的两形态**互斥**；共存 ⇒ 产品歧义，读端 fail-closed |
| N6 | 归档形态**必须**有产品级索引；索引缺失 ⇒ 产品不完整，读端 fail-closed（判定只走索引面：扫描归档与逐瓦片探测不在读路径内） |
| N7 | 裸形态**允许**无产品级索引；此时读端按目录枚举重建覆盖，并在 provenance 记降级 |
| N8 | `.hips` / `.hips.zst` 中缀只用于 HiPS 产品[1]；非 HiPS 产物（如 export 平面 FITS）的落盘名取自其产品族命名 |

`<name>` 的取值由阶段与块决定，且必须与运行清单中的产品名一致。

## 归档容器布局

### tar 层

| 规则 | 内容 |
|---|---|
| A1 | 归档内容 = 一个 tar 流的压缩结果；tar 根 = 产品根的内容（成员名以子产品名开头，不含前导 `./`） |
| A2 | tar 成员集合 = 裸形态产品树的**全部常规文件**（`properties`、`NorderK/DirD/NpixN.fits`、`Moc.fits`、`metadata.fits`、产品集 `manifest.json` 等；FITS 文件的 HDU 与关键字语法依 FITS 标准 4.0[2]），不含目录项之外的额外文件[1] |
| A3 | 成员顺序 = 成员路径的**字典序**（确定性）；成员头字段（mtime/uid/gid/uname/gname/mode）取固定值，使同一内容的两次打包字节一致 |
| A4 | 归档内 `properties` 与裸形态 `properties` **逐字节一致**；`hips_tile_format` 取**两档词表**的登记值 —— **Image 产品**子产品（`signal`/`support`/`variance`/`ivar`）= `fits`；**HiPS 目录（catalogue）**子产品 `snr/` = `tsv`（承载 SNR-PREC-001 `%.9g/%.17g` 精度锚，该锚的正本在 science 分册算法卷的 HiPS 写出器设计，不是数据语义卷——数据语义卷全篇不含该精度锚）。逐子产品档位表 = 机器事实源 `x-acsd-field-vocabulary.hips_tile_format_two_tiers.by_subproduct`；档位不符（`snr` 写 `fits`、Image 子产品写 `tsv`）或任何非标准 token（`zstd` / `fits.zst` / …）一律判红 |
| A5 | 产品级索引与数据集级覆盖索引位于归档之外（产品同父目录 / 运行输出根）；归档形态的发布次序由 `ATOMIC_PUBLISH.md`[5] 冻结 |

### zstd 层

zstd 帧格式与 `application/zstd` 媒体类型依 RFC 8878[3]。

| 规则 | 内容 |
|---|---|
| Z1 | 归档 = **N 个独立 zstd 帧的串接**；帧边界 = tar 成员边界（一个成员恰好一个帧）[3] |
| Z2 | 帧必须使标准工具可还原完整 tar 流：`zstd -dc <archive>` 的输出必须与打包前的 tar 流**逐字节一致** |
| Z3 | 流内每个区段都是常规压缩帧（skippable frame 会跳过其内容，使标准工具解压后的产物缺块，与"解压后合法"冲突） |
| Z4 | 瓦片数据只做标准 zstd 压缩（byte-shuffle 等需解压侧二次反变换的预变换一律判红）：标准工具解压后必须是合法 FITS |
| Z5 | 归档的帧参数 = 解压侧默认参数（长距离匹配窗口等需额外参数的帧参数判红）：解压侧只允许默认参数 |

### 压缩档位

- 默认 **zstd level 3**；档位依据（各档体积与压缩时间的实测读数与取舍）见 `实验域的压缩编码评估证据面`，本节只冻结默认档位。
- 档位是**打包参数**，不进产品身份哈希（「哈希口径」一节）。

## 索引 schema

机器事实源：`eng/contracts/schemas/hips_storage_form.schema.json`。索引文件是 UTF-8 JSON，**不压缩**。同一 schema 还承载形态的输入配置键与输出清单字段（`$defs.frame_storage` / `$defs.coverage_index_ref` / `$defs.manifest_storage`，见 「形态的输入配置与输出清单字段」一节）与字段词表 `x-acsd-field-vocabulary`。

### 产品级索引 `<name>.hips.index.json`

| 字段 | 类型 | 不变式 |
|---|---|---|
| `index_schema` | string | 恒 `acsd.hips-index/v1` |
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

### 数据集级覆盖索引 `coverage.index.json`

| 字段 | 类型 | 不变式 |
|---|---|---|
| `index_schema` | string | 恒 `acsd.coverage-index/v1` |
| `granularity.unit` | string | 恒 `hips_leaf_tile`（**块粒度**，值域 = 该单一取值） |
| `granularity.tile_width` / `hips_order` | int | 与参与产品一致 |
| `frames[]` | array[string] | 参与本次运行的帧标识，升序且唯一 |
| `blocks[].ipix` | int | 叶级 NESTED ipix，升序且唯一 |
| `blocks[].frames[]` | array | `{f, frac}`；`f` ∈ `frames[]`；`frac` ∈ 0..255 |

- 登记语义 = **存在性 + 覆盖分数**（不登记"完全覆盖"单一布尔）。
- 该索引是**派生产物**：可由各产品级索引重算；缺失时的规定回退 = 读入全部产品级索引并现场倒排。

**生产者与发布路径**：

- **指定生产者** = 写出产品集清单的**同一条命令（`mosaic`）的发布步**：在运行输出**根层**写出 `coverage.index.json`，与产品集清单一并**同一次原子发布**（同批、同原子序，见 「形态核对与负例」一节；原子发布步序见 `ATOMIC_PUBLISH.md`[5]）。
- **裁决口径不变**：仍**单判据**（本轮只裁 `coverage.index.json` 一项，兄弟面不随之进入白名单）、**不设豁免名单**；`ATOMIC_PUBLISH.md`「发布流水线（原子语义）」一节的「发布清单必含 `properties`」**不得放宽**，产品集判定**不得**改为白名单。
- **生产者尚未接线**：登记面 = `eng/contracts/ledgers/dead_config_keys.json#dead_config_key:coverage_index`；**不得静默留白**。
- **落地判据**：该发布路径实现时，须同时提供**正例**（索引存在且同批清单含 `properties`、随清单原子落盘）与**负例**（索引缺失或与清单不同批 ⇒ 判红）；在此之前不得声称覆盖索引可用。

### 查询语义

- 阶段2 按块查得**候选帧集合**，用于剪枝与调度；**不是**像素级裁决。
- 索引的用途 = 剪枝与调度；有效性裁决走瓦片实际状态：索引声明覆盖而瓦片缺失/非法 ⇒ 按既有 `MISSING` / `INVALID` 语义处理，结果具名登记。
- 低阶 hierarchy 瓦片不参与覆盖查询。

### 粒度依据

登记粒度 = 一个叶 tile（`tile_width`² 像素；HEALPix 的 leaf 像素化定义见 HEALPix 论文的 NESTED 索引与像素化约定[4]）。相对逐像素登记，记录数降低 `tile_width²` 倍；且与阶段2 的块级工作流粒度、稀疏层控制点间隔 Δ = tile_width/8 的父级、mesh 父级同构，**不引入第二套几何**。

### 载体演进

当前载体 = 单个不压缩 UTF-8 JSON 文件（可审计、无新依赖），承载的是 HiPS 产品树本身[1]。当记录数超过 `10^7` 条时允许切换载体（列存/定长二进制表），**但字段模型与不变式不变**；载体切换是独立变更，不在本合同内。

## 哈希口径

| 哈希 | 对象 | 用途 | 边界外用途 |
|---|---|---|---|
| `tree_hash`（**产品身份**） | 解压后的 HiPS 内容：**条目三元组数组** `[[path,size,sha256], …]`（非对象数组）按 `(path,size,sha256)` 字典序升序后做 `sha256(canonical_json)`；规范序列化参数**冻结** = UTF-8、`ensure_ascii=false`、分隔符 `(",",":")`（无空白） | 产品身份、跨阶段交换、可重算校验 | — |
| `archive_sha256`（容器指纹） | `<name>.hips.zst` 的字节 | 容器完整性、缓存键 | 用途限于容器面（产品身份 = `tree_hash`） |
| `index_sha256` | `<name>.hips.index.json` 的字节 | 索引完整性 | 用途限于索引面（产品身份 = `tree_hash`） |

- H1 同一产品两形态的 `tree_hash` **必须相同**（身份与打包参数无关）；
- H2 `manifest.json` 的 `tree` 记录**解压后内容**的条目；`storage` 段记录 `storage_form` / `form_source` / `index_path` / `index_sha256` / `archive_bytes` / `archive_sha256`（逐产品一条，字段与不变式见「运行完成清单 `manifest.json#storage`（加性）」一节）；
- H3 压缩档位、帧切分、tar 头字段的取值与 `tree_hash` 无关（身份取解压后内容）。

## 读路径不变式

| 规则 | 内容 |
|---|---|
| R1 | 形态由落盘名判定（后缀 `.hips` / `.hips.zst`），调用方传入的路径无需改写 |
| R2 | 调用方不感知形态：`hips_paths` / `source.hips_dir` 的元素可以是任一形态，语义相同 |
| R3 | 归档形态的瓦片读取 = 一次 `pread` + 一次单帧解压（读取粒度 = 单帧） |
| R4 | 归档形态的覆盖查询 = 纯索引查询（归档字节不参与） |
| R5 | 归档形态缺索引 ⇒ fail-closed；裸形态缺索引 ⇒ 目录枚举重建 + provenance 记降级 |
| R6 | 形态解析失败、索引不一致、归档截断 ⇒ 显式错误码，不静默回退 |

## 体积削减（裸形态）

**两种机制，冻结口径与判据如下**（机制与推导见 `docs/detail/infrastructure/` 的产品存储形态设计 「形态核对与负例」一节；读数复现 = 按本节判据在 Linux 面实测，实测读数见 `实验/engineering-evidence/`）：

| 机制 | 作用层 | 冻结规则 | 失败语义 | 判据（机器可校验） |
|---|---|---|---|---|
| **T1 文件系统打洞** | 文件分配层，**字节不变** | 只对 **4 KiB 对齐的整块全零区域**打洞；打洞在 `fsync` 之后、算哈希与原子发布之前完成；`st_size` 必须不变 | **不 fail-closed**：跳过、保留完整 `.fits`、provenance 记 `trim=skipped(reason)` | ① 打洞前后整文件 `sha256` 相同；② cfitsio 与 astropy 两路读器逐 HDU header+像素全等；③ `DATASUM`/`CHECKSUM` 自洽；④ 跨洞边界 `pread` 全等；⑤ `st_blocks` 必须下降（未下降 ⇒ 判红，不静默通过） |
| **T2 包围盒 TRIM**（FITS 关键字 `TRIM1`/`TRIM2`/`ONAXIS1`/`ONAXIS2`[2]） | 瓦片内容层，**改 FITS 结构** | 仅当产品显式声明该形态（`properties`/provenance 记 TRIM 关键字与原始 `ONAXIS1/ONAXIS2`）且下游读端 TRIM-aware；**默认不启用** | 读端不认 TRIM 关键字 ⇒ **fail-closed**（拒绝）；缩小的 NAXIS 不作续读依据 | ① 补边后像素与未 TRIM 同源瓦片逐字节全等（补边位模式 = IEEE NaN）；② `ONAXIS1/ONAXIS2` == 未 TRIM 的 NAXIS1/NAXIS2；③ 负例：边距改 0.0 或错位 1 像素必须判红 |

- **生效面**：仅**裸形态** `<name>.hips/`。**归档形态 `<name>.hips.zst` 两种都不实施**（收益被 zstd 吸收）。
- **产品身份不受影响（T1）**：字节不变 ⇒ 产品哈希不变。**T2 改变内容** ⇒ 启用 T2 的产品与未 TRIM 的同源产品**不同身份**，必须在 `properties` 与 manifest 的 `storage` 段显式区分（两个身份都须显式具名，静默分化判红）。
- **T1 与 T2 的收益口径分立对照**：T1 的整产物收益与 T2 的缩小-NAXIS 收益是两个独立口径，各自成立，只允许在同一机制内引用；T1 的收益只在 Linux 面有可核验读数（Windows 面无可核验读数，其等价实现只有一手文档依据），signal/variance/ivar 三层的收益口径另计。**实测读数与逐层数值见 `实验/engineering-evidence/`**；收益数值的复现 = 在 Linux 面按上表 T1 判据①–⑤ 实测（`st_blocks` 下降量即整产物收益），机制与口径正本 = `docs/detail/infrastructure/` 的产品存储形态设计 「形态核对与负例」一节。
- **降级**：T1 在卷不支持稀疏（`EOPNOTSUPP` 等）时跳过，产品保持完整可读；T2 在读端不支持时拒绝，不降级为"读小图"。

## 错误语义

| 情形 | 结果 |
|---|---|
| 两形态共存 | 产品歧义错误（fail-closed） |
| 归档形态缺产品级索引 | 产品不完整错误（fail-closed） |
| 索引 `coverage` 与 `tiles` 集合不一致 | 索引损坏错误（fail-closed） |
| 索引声明覆盖但瓦片缺失 | 既有 `MISSING` 语义（以瓦片实际状态为准） |
| 归档截断 / 帧解压长度不符 | 既有 `INVALID` / I/O 错误（fail-closed） |
| `properties` 的 `hips_tile_format` 与该子产品档位不符（`signal` 写 `tsv` / `snr` 写 `fits`）或为非标准 token | 既有读端拒绝（`UNSUPPORTED`）；目录子产品 `snr` 的 `tsv` 是登记值，不在此列 |

## 形态核对与负例

- 唯一机器事实源 = 索引 schema `eng/contracts/schemas/hips_storage_form.schema.json`；
 落盘实例是否满足该 schema，按 「形态的输入配置与输出清单字段」一节 的不变式 F0–F4 / M1–M4 逐条核对；
- **逐层口径核对**：每层文档必须只使用 「形态的输入配置与输出清单字段」一节 词表登记的字段名与取值，词表外的同义名一律判红；
 词表 = `hips_storage_form.schema.json#x-acsd-field-vocabulary`；
- **负例必须能红**，覆盖面与本节判据同口径：形态键缺省/留空却无 warn、逐帧索引字段缺失、
 清单 storage 段缺字段、mosaic/export 输入含形态键却未 REJECT、层间口径不一致 ——
 每条各配一个构造样例，样例不合预期即判该判据失效；
- 核对由人工对抗审核逐条执行，证据 = 复核命令 + 实测读数或产物路径，不产出流水线判决。

## 形态的输入配置与输出清单字段

形态是**输入配置项**，不是运行期开关；产物必须**自报形态与索引路径**，使不同批次 normalize 的输出 JSON 可以合并而不丢索引。

字段词表唯一源 = `eng/contracts/schemas/hips_storage_form.schema.json#x-acsd-field-vocabulary`：字段名、取值、段名与文件名后缀只在那里定义一次，本文档与各层文档只引用，**同义名一律以词表为准**。逐层口径一致性是硬判据（缺登记词、词表外的同义名、取值口径漂移都判红，核对口径见 「形态核对与负例」一节）。

### normalize 输入配置键 `storage_form`

| 项 | 内容 |
|---|---|
| 键名 | `storage_form` |
| 取值 | `archive`（默认）\| `bare` |
| 落点 | normalize（normalize）输入 JSON 的**块内**键（多块形态 `blocks[].storage_form`）与平铺单块简写的顶层键 |
| 缺省语义 | **键缺失、空串 `""` 或 `null` ⇒ 取默认 `archive`，并报一条 `level=warn` / `event=warn` 事件**（结构化日志事件模型，`level` 取 `debug/info/warn/error`、`event` 取 `start/progress/end/warn/error/metric/checkpoint/cancel/trace`，正本 = `../resources/observability/STRUCTURED_LOGGING.md`「事件模型」一节，经 `LOG_AND_ERROR.md`「日志行格式：机器校验格式」一节转发）[6]；**取默认与 warn 事件成对出现** |
| 显式语义 | 显式给出 `archive` / `bare` ⇒ 按该形态落盘，**不报** warn |
| 形态来源登记 | 运行完成清单 `manifest.json#storage.form_source` = `config`（显式）/ `default`（缺省）；`default` 是 warn 必须存在的机器证据（见「运行完成清单 `manifest.json#storage`（加性）」一节的 M2） |
| 键域 | 该键**只**属 normalize。mosaic / export 的输入合同不设该键，出现即 REJECT（见「mosaic / export：形态键必须 REJECT」一节） |

不变式：

- F0 缺省或留空 ⇒ 解析结果必须是登记的默认值，**且**必须存在一条点名该键的 warn 事件；显式声明 ⇒ warn 面为空。两者缺一即判红（静默取默认与误报 warn 都是故障）。
- 形态**不改变科学结果**：同一输入下两形态的产品内容逐字节一致（H1）。

### normalize 输出清单 `p1_products.json`（加性）

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

- F1 四个字段一个不少——缺任一 ⇒ 该帧的形态与索引不可追溯，读端 fail-closed。
- F2 `storage_form=bare` ⇔ `archive_sha256=null`。
- F3 `index_path` 的 basename 必须等于 `<产品名>.hips.index.json`。
- F4 `index_sha256` 必须与磁盘索引字节一致；索引必须可由产品内容重算（见「索引 schema」一节的 I4）。
- `archive_sha256` 的用途限于容器面；产品身份 = `tree_hash`（取解压后内容，「哈希口径」一节）。
- 机器事实源：`$defs.frame_storage`（逐帧）与 `$defs.coverage_index_ref`（运行级）。

### 运行完成清单 `manifest.json#storage`（加性）

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

- 字段名与「形态的输入配置与输出清单字段」一节 **同词表**（`storage_form` / `index_path` / `index_sha256` / `archive_sha256`），另加 `archive_bytes` 与产品身份 `tree_hash`。
- M1 运行级 `storage_form` 与每个 `products[].storage_form` 一致；`bare` 条目的 `archive_bytes` / `archive_sha256` 必须为 `null`。
- M2 `form_source=default` ⇒ 必须存在点名 `storage_form` 的 warn 事件；`form_source=config` ⇒ warn 面为空。
- M3 `coverage_index` 非 `null` 时 `path` 的 basename 必须是 `coverage.index.json` 且 `n_blocks` ≥ 1；不产出覆盖索引的运行（export）恒 `null`。
- M4 `products[].index_path` 的 basename 必须等于 `<product>.hips.index.json`。
- `tree` 记录**解压后内容**的条目（与裸形态相同），`tree_hash` 与 「哈希口径」一节 同口径；形态事实的落点 = 本段与产品级索引（HiPS `properties` 的字段面保持科学属性，「形态的输入配置与输出清单字段」一节）。
- 机器事实源：`$defs.manifest_storage`。

### mosaic 输入：索引引用是加性可选键

- `hips_paths` 的元素**保持字符串**（不做元素对象化）：逐帧产品级索引路径由命名规则派生 —— `<name>.hips` / `<name>.hips.zst` → `<name>.hips.index.json`（与 `hips_path` 同父目录）。
- 额外的「总索引」引用用**加性可选键** `coverage_index`（字符串路径，指向数据集级 `coverage.index.json`）。存在 ⇒ 阶段二启动时载入它做块级查询；缺失 ⇒ 规定回退 = 读入全部产品级索引现场倒排（「数据集级覆盖索引 `coverage.index.json`」一节）。
- 该键**不**承载形态选择：mosaic 产物固定裸形态（「mosaic / export：形态键必须 REJECT」一节）。
- 机器事实源 = `eng/contracts/schemas/phase_config_mosaic.schema.json#/$defs.coverage_index_path`（**字符串**路径键）。
- **同名异型消歧**：运行级清单引用取名 `coverage_index_ref`、在本合同机器事实源里是**对象**（`$defs.coverage_index_ref` = `{path, sha256, n_frames, n_blocks}`，见「运行完成清单 `manifest.json#storage`（加性）」一节）；mosaic 输入侧的路径键取名 `coverage_index_path`、是**字符串**（`eng/contracts/schemas/phase_config_mosaic.schema.json#/$defs.coverage_index_path`）。同名异型属机器可读面的最坏形态，故两侧按「输出清单引用 / 输入配置路径」分名：属性名 `coverage_index` 两侧不变（唯一词表 `x-acsd-field-vocabulary.mosaic_input_index_ref_key.name`）；引用本键的注释面（`lib/infrastructure/cli/parser.cpp`、`session_commands.h`）与死键台账（`eng/contracts/ledgers/dead_config_keys.json`）按此名对齐。

### mosaic / export：形态键必须 REJECT

| 阶段 | 产物形态 | 输入合同是否含 `storage_form` | 出现该键的后果 |
|---|---|---|---|
| mosaic mosaic | 固定裸 `<name>.hips/`（服务面，被随机读取） | **不含** | REJECT |
| export export | 固定裸 FITS（不压缩、不套壳，不使用 `.hips` 中缀） | **不含** | REJECT |

- REJECT 在**两处**生效：schema 面（块内 `additionalProperties:false` / 平铺 `propertyNames`）与 CLI 面（块内未知键门，`validate_config_full`）。
- **为什么值域取「`archive` / `bare`」两个取值**：运行期配置门是**键白名单**，不是 JSON Schema；只收窄值域会让 `storage_form: "archive"` 在运行期**静默透传成 no-op** —— 正是本合同的静默失效判红面。

### 合并语义（为什么索引路径必须显式进输出 JSON）

不同批次的 normalize 输出 JSON 合并成一个数据集时，逐帧 `index_path` / `index_sha256` 随条目一起搬移 ⇒ 索引不会丢、不会指向错产品；运行级 `coverage_index` 是**派生产物**，合并后按「数据集级覆盖索引 `coverage.index.json`」一节 由各产品级索引重算，不需要跨批次拼接。
## 参考文献

[1] IVOA. HiPS — Hierarchical Progressive Survey, Version 1.0. REC-HIPS-1.0.
https://www.ivoa.net/documents/HiPS/

[2] FITS 工作组. FITS 标准 4.0. IAU, 2018. https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf

[3] Y. Collet. RFC 8878: Zstandard Compression and the 'application/zstd' Media Type. IETF, 2021.
https://www.rfc-editor.org/info/rfc8878/

[4] Górski K. M., et al. HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of
Data Distributed on the Sphere. ApJ, 2005, 622: 759–771. https://doi.org/10.1086/427976

[5] 内部文档 `ATOMIC_PUBLISH.md`，原子发布合同。

[6] 内部文档 `LOG_AND_ERROR.md`，日志与错误合同。
