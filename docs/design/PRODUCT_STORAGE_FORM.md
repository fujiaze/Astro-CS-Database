# 产品落盘形态：裸 HiPS 与 zstd 归档包

> 上游：ASTROCS_DESIGN.md §10（I/O 与原子产品）、§4.4 / §5 / §6（三阶段输出合同）、附录 B（外部标准与文献）

文档 ID：`DESIGN-STORAGE-001`
状态：`TARGET_NORMATIVE`
上位：`ASTROCS_DESIGN.md`（§0 权威链，最高设计）
下游：`docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md`、`docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md`、`docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md`、`docs/plugins/infrastructure/17_aio.md`、`docs/design/PHASE1_DETAILED_DESIGN.md` / `PHASE2_DETAILED_DESIGN.md` / `PHASE3_DETAILED_DESIGN.md`

## 1. 两种落盘形态

一个 HiPS 产品在磁盘上有且仅有两种落盘形态：

| 形态 | 落盘名 | 类型 | 语义 |
|---|---|---|---|
| **裸形态**（bare） | `<name>.hips/` | 目录 | 产品以目录树直接可读：`properties` + `NorderK/DirD/NpixN.fits` + 可选 `Moc.fits` / `metadata.fits` |
| **归档形态**（archive） | `<name>.hips.zst` | 单文件 | 同一产品的 tar 流按成员边界切成独立 zstd 帧后串接；**解压后与裸形态逐字节一致** |

`<name>` 是产品名（Phase1：块名；Phase2：`<块名>.mosaic`）。两种形态**互斥**：同一 `<name>` 在磁盘上只出现一种；同时存在即产品歧义，读端 fail-closed。

两形态都由**产品级索引** `<name>.hips.index.json` 伴随（§5）。索引是产品的组成部分，不是可选的调试附属物。

**标准依据**：IVOA HiPS 1.0 §4.1 与 §5.1 规定 HiPS 的**实现形态不是义务**，义务是"以目录与文件的形式可见"：

> "The actual implementation of HiPS as directories and files is not an obligation, only the view as directories and files is required. … Internally, a HiPS may be stored in a data base, or any other appropriate packaging (tar or zip files…) rather than in a basic file system directory structure."

⇒ 归档形态合法，**前提是解压后得到目录/文件视图**（"解压后合法"）；归档内容因此必须是**完整合法 HiPS**，`properties` 必须与裸形态逐字节一致（§8）。逐瓦片压缩（`.fits.zst` / `ZCMPTYPE=ZSTD`）**不在本设计内**：它违反 HiPS 1.0 §4.2.1.3 的扩展名 MUST 且 `hips_tile_format` 词表无对应 token（证据见 `docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md` 与 `docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md`）。

## 2. 形态判据：由访问模式决定，不由全局一刀切

形态选择是**按用途**的决定，判据是下游对该产品的访问模式：

| 访问模式 | 要求 | 形态 |
|---|---|---|
| **下游按天区随机取瓦片**（Phase2 读 Phase1、Phase3 读 Phase2） | 瓦片必须可独立寻址；覆盖查询不得触发整包解压 | 两形态都满足：裸形态天然可寻址；归档形态靠**产品级索引的定位表**直接 `pread` 单帧解压 |
| **产物自身被随机读取用于服务** | 每次读取不得引入解压延迟 | **裸形态** |
| **产物是交付物，由外部工具直接打开** | 用户拿到即可打开 | **裸形态**（FITS 不套壳） |
| **产物主要被整体搬运、长期保存，且写入侧是批处理** | 省磁盘优先，读取开销可接受 | **归档形态** |

按此判据，三阶段的默认形态是：

| 阶段 | 产物性质 | 默认形态 | 可选形态 |
|---|---|---|---|
| **Phase1 normalize** | 逐帧单帧产品，数量大，写入批处理；下游按天区查 | **`<name>.hips.zst`** | 裸 `<name>.hips/`（`storage_form=bare` 显式切换） |
| **Phase2 mosaic** | 叠加后的天球数据库，被随机读取用于服务 | **裸 `<name>.hips/`** | 无（服务面不接受归档形态） |
| **Phase3 export** | 平面 FITS 导出交付物 | **裸 FITS（不压缩、不套壳）** | 无 |

- Phase1 归档形态的**下游读取代价必须可接受**：归档定位表的随机瓦片读取实测与本机冷盘直读同量级，且读盘字节数比裸形态少约三分之一（`run/HIPS-PACK-01/REPORT.md` §3）。
- Phase3 产物是**交付物**：外部工具（DS9 / astropy / 浏览器）必须能直接打开，任何套壳都会把"打开"变成"先解压"。Phase3 不产出 HiPS，因此不使用 `.hips` / `.hips.zst` 命名。

## 3. 命名与内容布局

```text
<output_dir>/                          # 一次运行（一块）的输出根
├── <name>.hips.zst                    # 归档形态（Phase1 默认）
│                                      #   tar 成员 = <name>.hips/ 的内容：
│                                      #   signal/properties、signal/NorderK/DirD/NpixN.fits、
│                                      #   support/…、variance/…、ivar/…、manifest.json
├── <name>.hips/                       # 裸形态（与上一行互斥；Phase2 默认）
├── <name>.hips.index.json             # 产品级索引（两形态都产出，不压缩）
├── coverage.index.json                # 数据集级覆盖索引（每次运行产出，不压缩）
├── p1_products.json                   # 逐帧产品清单（含 storage_form / index_path）
├── manifest.json                      # 运行完成清单（含 storage 段）
└── logs/
```

- 归档的 tar 根 = 产品根的内容（成员名形如 `signal/Norder9/Dir1370000/Npix1372036.fits`），解压即得 `<name>.hips/` 的等价树。
- tar 成员按**确定性顺序**（路径字典序）写入，帧边界 = 成员边界（§7）。
- 归档内**不含**产品级索引与数据集级索引：索引是 AstroCS 的查询面，不是 HiPS 内容。归档内只放"解压后构成合法 HiPS"的内容。

## 4. Phase1 产物的完整形态

Phase1 一次运行（一块）产出 N 帧，每帧一个产品；产物由四部分组成，压缩策略不同：

| 组成 | 文件 | 压缩 | 理由 |
|---|---|---|---|
| **像素数据** | `<name>.hips.zst` 内的瓦片 | **压缩**（zstd，默认 level 3） | 体积占绝对多数；写入侧批处理，解压开销可摊 |
| **产品级索引** | `<name>.hips.index.json` | **不压缩** | 被随机查询；体积约为产品的 10⁻⁴ 量级 |
| **数据集级覆盖索引** | `coverage.index.json` | **不压缩** | 阶段2 启动时一次性载入并逐块查询 |
| **元数据/清单** | `p1_products.json` / `manifest.json` / `logs/` | 不压缩 | 小、需直接读、需可审计 |

查询怎么走（阶段2 按天区查阶段1 产物）：

```mermaid
flowchart LR
    Q["输出叶块 ipix"] --> CI["coverage.index.json<br/>（不压缩，常驻内存）"]
    CI --> FS["该块的帧集合 + 覆盖分数"]
    FS --> PI["各帧产品级索引<br/>（不压缩）"]
    PI --> LOC["瓦片在归档中的定位<br/>coff/csize/doff/dsize"]
    LOC --> RD["pread 单帧 → 解压 → 交付瓦片"]
    PI -.裸形态.-> RD2["直接按 NorderK/DirD/NpixN.fits 读"]
```

- **覆盖查询完全不触碰归档**：块 → 帧集合来自不压缩的数据集级索引；瓦片是否存在来自不压缩的产品级索引。
- **归档形态的瓦片读取 = 一次 `pread` + 一次单帧解压**，不解压整包、不落中间文件。
- 归档形态**不允许**把"解压整包到缓存再读"作为生产路径：阶段2 的访问顺序是输出块主序，全部帧同时在线，整包解压会使峰值磁盘占用等于 Phase1 全量未压缩体积，与本形态的目的相反。

## 5. 索引：块粒度，两层

索引的登记粒度 = **一个 HiPS 叶 tile**（`hips_tile_width` = 512 ⇒ 512×512 像素一块），**不是逐像素**。依据：阶段2 的最小可分单位是逐像素，但实际工作流按块批处理（生产里一块 = 一个叶 tile 的像素跨度）；索引粒度与工作流粒度对齐后，记录数相对逐像素降低 `tile_width²` 倍，而可用性不损失——索引负责**剪枝**，像素级裁决仍由既有 support/validity/rejection 语义执行（§6）。

块粒度同时等于既有几何：稀疏层控制点间隔 Δ = tile_width/8 的父级、EXP-02 mesh 的父级。**不引入第二套几何**：块的 ipix 就是 `hips_order` 阶的 NESTED 单元号。

### 5.1 产品级索引 `<name>.hips.index.json`

```text
{
  "index_schema": "astrocs.hips-index/v1",
  "product": "<name>",
  "storage_form": "archive" | "bare",
  "archive": {"name": "<name>.hips.zst", "bytes": N, "sha256": "<64hex>",
              "frame_unit": "tar_member"} | null,
  "subproducts": [
    {"name": "signal",
     "hips_order": 9, "hips_tile_width": 512, "hips_tile_format": "fits",
     "n_leaf_tiles": 523,
     "coverage": {"encoding": "runs", "leaf_ipix_runs": [[0,1],[12,3]],
                  "frac_quant": 255, "leaf_frac": [255, 128]},
     "tiles": [{"ipix": 0, "coff": 123456, "csize": 65432, "usize": 1054208,
                "doff": 512, "dsize": 1054080}]}
  ]
}
```

- `coverage`：该产品（该帧）**存在的叶块集合**，`runs` 为按 ipix 升序的 `[start, count]` 段；`leaf_frac` 是与展开顺序一一对应的**覆盖分数**，量化为 u8（`frac_quant`=255 ⇒ 分辨率 1/255）。
- `tiles`：归档定位表。`coff/csize` 是帧在归档中的压缩偏移与长度；`doff/dsize` 是瓦片文件数据在**解压后帧内**的偏移与长度。裸形态下 `tiles` 省略（路径即定位）。
- 索引**必须可由产品内容重算**：`coverage` 由瓦片树枚举重算，`tiles` 由归档帧表重算。重算不一致 ⇒ 产品损坏，fail-closed。

### 5.2 数据集级覆盖索引 `coverage.index.json`

```text
{
  "index_schema": "astrocs.coverage-index/v1",
  "granularity": {"unit": "hips_leaf_tile", "tile_width": 512, "hips_order": 9},
  "frames": ["f00", "f01"],
  "blocks": [{"ipix": 1372036, "frames": [{"f": "f00", "frac": 255}, {"f": "f03", "frac": 64}]}]
}
```

- 载体：**单个不压缩的 UTF-8 JSON 文件**，全量载入内存 + 现场建倒排。选择理由：无自定义二进制格式、无新第三方依赖（C++ 侧已有 vendored JSON 解析器）、可审计可 diff；记录数在本项目量级下远低于载体切换阈值。
- 该索引是**派生产物**：可由各产品级索引重算。缺失时的规定回退是"读入全部产品级索引并现场倒排"（有依据的回退，不静默降级为逐瓦片探测）。
- 覆盖范围登记**存在性 + 覆盖分数**，不登记"完全覆盖"单一布尔：叶块中约一成是**部分覆盖**（真实产物实测），只登记"完全覆盖"会漏掉这些块，只登记"有任何覆盖"又无法让阶段2 跳过近乎空的块。

## 6. 部分覆盖与查询语义

- **登记语义**：`present`（该帧在该块存在瓦片）+ `frac`（该块被该帧覆盖的面积比例，按有效域包围盒计，量化 u8）。
- **查询语义**：阶段2 按块取到的是**候选帧集合**，用于剪枝与调度；**不是**像素级裁决。块内逐像素的有效帧集合可以不同（部分覆盖、掩膜、缺 tile），像素级裁决仍由既有语义执行：support/validity 掩膜、排异路由的 N 取该像素的几何覆盖帧数、覆盖级缺数置 NaN 并强制计数。
- **缺口与处理**：索引**不足以**决定块内每个像素怎么叠加——这是设计上的分工，不是缺陷。分工边界是：**索引决定读哪些 (帧, 块)；数据决定每个像素怎么叠加**。因此索引不得被当作有效性来源：索引声明覆盖而瓦片缺失/非法时，按既有 `MISSING` / `INVALID` 语义处理（fail-closed），不得静默当有效。
- 索引只登记叶级（`hips_order`）块；低阶 hierarchy 瓦片不参与覆盖查询（它们由产品自身携带，读端不做父阶回退）。

## 7. 写路径

- **归档形态写出**：产品先按裸形态在 run 私有 stage 写出并逐瓦片校验（结构 + DATASUM），再按确定性顺序打包为 tar 流，**按 tar 成员边界切分为独立 zstd 帧**（每成员一帧），串接写出 `<name>.hips.zst`；同时写出产品级索引。归档、索引、完成 manifest 全部 `fsync` 后按 IO-003 的原子发布语义落位，**完成 manifest 最后落**，它是唯一完成标记。
- **帧边界 = 成员边界**：使"瓦片 → 单帧"成为恒等映射，随机访问一次解压即得一个完整瓦片；也保证标准工具（`zstd -dc | tar -xf`）能还原完整 tar 流。
- **裸形态写出**：与现状相同（临时区 → 校验 → 哈希 → 原子改名 → 完成清单），额外写出产品级索引。
- **形态切换**：Phase1 由配置显式选择；Phase2 固定裸形态（服务面）。形态不影响科学结果——同一输入下两形态的产品内容逐字节一致（哈希口径见 §8）。
- **写入侧的合法性证据**：归档形态在打包前对**裸瓦片**执行既有校验，归档解压后的合法性因此在写入侧已被证明，而不是留给读端发现。

## 8. properties 与哈希口径

### 8.1 properties 不许撒谎

- 归档内的 `properties` **与裸形态逐字节一致**，声明的仍是标准 HiPS（`hips_tile_format=fits`、`hips_version`、`hips_order`、`hips_tile_width`、`hips_frame`）。
- **归档形态不得让 properties 撒谎**：不得出现 `zstd` / `fits.zst` / 任何非标准 `hips_tile_format` token。形态事实只写在 AstroCS 自己的清单与索引（`storage_form`），不写进 HiPS properties。
- 判据：同一产品两形态的 `properties` 字节相同；`hips_tile_format` 必须是既有读端接受的取值。

### 8.2 哈希口径

| 哈希 | 对象 | 用途 |
|---|---|---|
| **产品身份哈希** `tree_hash` | **解压后的 HiPS 内容**：`sorted[{path,size,sha256}]` 的规范 JSON 的 sha256（IO-003 口径） | 产品身份、跨阶段交换、可重算校验 |
| **容器指纹** `archive_sha256` | `<name>.hips.zst` 的字节 | 容器完整性、缓存键；**不得**用作产品身份 |
| **索引指纹** `index_sha256` | `<name>.hips.index.json` 的字节 | 索引完整性；且索引须可由产品内容重算 |

- **产品身份取解压后内容，不取压缩包字节**：压缩档位、帧切分、tar 顺序都是打包参数，不改变科学产品；若身份随打包参数变化，同一科学内容会有多个身份，跨运行可比性与可复现性同时失效。两形态因此**同身份**。
- `manifest.json` 的 `tree` 记录**解压后内容**的条目（与裸形态相同），另设 `storage` 段记录形态、容器指纹与索引指纹。

## 9. 与 TRIM 的关系

TRIM（WD-HiPS-2.0 §4.3.2，`TRIM1/TRIM2/ONAXIS1/ONAXIS2`）在**瓦片内容层**裁掉有效域之外的边距，整包 zstd 在**文件内容层**压缩。两者作用层不同但**收益重叠**：被 TRIM 裁掉的区域是常量 NaN，zstd 对它的压缩率接近无穷。

- **对归档形态：不实施 TRIM。** 实测在归档压缩之后再施加 TRIM，压缩包体积几乎不变（收益被 zstd 完全吸收），却引入草案关键字与读端补 NaN 的实现定义风险。
- **对裸形态：TRIM 有独立收益，但当前不实施。** 裸形态没有 zstd 兜底，TRIM 的省量是真实的；但它由 WD 草案定义、生态支持面窄，属独立待裁决项，不随本设计默认启用。

## 10. 边界

- 本设计不引入逐瓦片压缩，不引入自定义容器格式（不使用 zstd skippable frame 混装"不压缩区"）：单文件内混装会使标准工具解压后的内容缺块，与"解压后合法"冲突。
- 本设计不改科学公式、容差、权重与归约顺序；形态只影响磁盘表示与 I/O 路径。
- 归档形态**不**作为 Phase2 的服务形态；Phase3 **不**套壳。
- 索引不承载有效性判定，只承载块级候选与定位。
