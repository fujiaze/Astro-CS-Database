# IVOA HiPS 标准对瓦片（tile）文件格式的规定

> 上游：ASTROCS_DESIGN.md §4.4（输出合同：HiPS 文件）、附录 B（外部标准与文献）。
> 生态实测时点：2026-09-22（客户端与源码分支状态随上游演进；规范版本见各条引文）。

**方法**：全部结论来自实际抓取的原文（PDF / HTML / HTTP 响应 / 源码），逐条给出 URL 与英文原文引文；
条款号按仓库规范写作 `§X.Y.Z`；引文内以 `[…]` 标注的省略均在原处披露被省略片段；
未能核实者见第三节，未作推测性填充。

---

## 一、结论摘要

1. **当前有效的 HiPS 规范是 IVOA Recommendation 1.0（2017-05-19）；2.0 仍是 Working Draft（2026-05-01），不可作为引用依据**——但 2.0 WD 是唯一明确讨论"压缩 tile"的 IVOA 文本。[S1][S6][S7]
2. **HiPS 1.0 允许的 image tile 格式只有三种：FITS / JPEG / PNG，且只是 MAY 档**；真正的强制条款是"**扩展名必须与格式对应**：FITS→`.fits`、JPEG→`.jpg`、PNG→`.png`，且必须小写"（MUST）。[S1][S2]
3. **HiPS 1.0 全文不含 gzip / zstd / fpack / `.fz` 任何一个词**；唯一涉及"压缩"的是 §5.1 的一条 Note，指的是 **HTTP 传输层压缩（Content-Encoding）**，不是 `.fits.gz` 文件名。[S4][S17]
4. **HiPS 2.0 WD 明确开了口子**："A HiPS can use any another formats if this proves more appropriate"，并点名 **RICE FITS** 与 **WEBP** 为例；脚注给出判据——**RICE 保持 `.fits` 扩展名，因为"该压缩已被整合进 FITS 标准"**。[S6]
5. **FITS tile-compression（fpack，RICE_1 / GZIP_1 / GZIP_2 / PLIO_1 / HCOMPRESS_1）已是 FITS 标准 4.0 正文 §10 的一部分**（2018-08-13 批准），`ZCMPTYPE` 取值表是**穷举表**，新增算法"must be registered with the IAUFWG"——**zstd 不在表内**。[S8][S9][S10]
6. **CDS 官方生成器 Hipsgen 支持 `-gzip`，但其手册明说这类方法"not standardized by the IVOA … currently only recognised by Aladin Desktop"**；gzip 后**仍保留 `.fits` 扩展名**，由客户端"用文件头的 magic code"自行探测。全生态**无任何一处提到 zstd**。[S12][S13][S21]

---

## 二、逐条证据

### [S1] IVOA HiPS 1.0 Recommendation —— 允许的 tile 格式与扩展名

- **文档名 / 版本**：*HiPS – Hierarchical Progressive Survey*, Version 1.0, **IVOA Recommendation 19 May 2017**
- **URL**：<https://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf>（索引页 <https://www.ivoa.net/documents/HiPS/20170519/index.html>）
- **章节**：§4.2 "HiPS tile formats"；§4.2.1.3 "Format of tiles"；§4.2.3 "Cube tile format"；§4.2.2.1 "Format of tiles"（catalogue）

**原文引文（§4.2，p.9）**：

> "The tile format depends on the survey data type: FITS, JPEG, PNG for image or cube surveys; TSV for catalogues. These basic tile formats have been especially chosen in order to facilitate checking of the tile content with basic file tools and editors."

**原文引文（§4.2.1.3，p.12 —— 本节是回答本问题的核心条款）**：

> "Three image formats may be used to package the HiPS tiles for images: FITS [6], PNG or JPEG. An image tile may be stored in FITS format in order to keep the full dynamic range of the original data values. Tiles stored in JPEG format provide good file compression, and tiles stored in PNG format provides the capability to support transparency channel. **The tile file extension must correspond to the format: .fits for FITS, .jpg for JPEG, .png for PNG. These extensions must be in lowercase.**"

> "An image HiPS may be delivered simultaneously in various formats."

**原文引文（§4.2.3）**：

> "As with image HiPS, the format of each cube frame tiles may be FITS, JPEG and/or PNG."

**原文引文（§4.2.2.1，catalogue）**：

> "A catalogue HiPS tile must be stored in an UTF-8 file … The tile file extension must be ".tsv" in lowercase."

**解读**：
- 枚举是 **FITS / JPEG / PNG**（image 与 cube）、**TSV**（catalogue），**没有别的**。所谓 "HIPS legacy" 格式在 HiPS 1.0 里**不存在**——这个说法在规范原文中查无实据。
- 档位：格式选择句用的是**小写非粗体 "may"**，按 §3 的定义（见 [S2]）属 **MAY（可选）**；而扩展名那句用的是 **"must"**，属 **MUST（强制）**。也就是说"你选哪个格式"是可选的，"扩展名必须与所选格式严格对应且小写"是强制的。
- 对 **Image HiPS** 子类型，允许的就是 FITS / JPEG / PNG 三种，可同时提供多种。

---

### [S2] IVOA HiPS 1.0 §3 —— must / should / may 的档位定义

- **URL**：同 [S1]，§3 "HiPS principle" 末尾（p.6）
- **原文引文**：

> "In these three sections, the keywords "must", "required", "should", and "may" used in this document are to be interpreted as described in the W3C specifications RFC 2119 [7]. Mandatory elements are indicated as must, recommended elements as should, and optional elements as may or simply "may" without the bold face font."

**解读**：HiPS 1.0 全文用 RFC 2119 语义。凡未出现 must / should 的表述即为 MAY 或纯描述性文字。判定 [S1] 中"三种格式"为 MAY、扩展名对应关系为 MUST，依据即在此。

---

### [S3] IVOA HiPS 1.0 §4.4.1 —— `hips_tile_format` 是必填项且词表封闭

- **URL**：同 [S1]，§4.4.1 "The properties file"（p.17–19）
- **原文引文（9 个强制关键字之一）**：

> "7. hips_tile_format: Tile formats – Format: list of different HIPS tiles format supported by the survey, space separated (one or many of "fits", "jpeg", "png" for image/cube HiPS and "tsv" for catalog HiPS)"

- **原文引文（属性表）**：

> "hips_tile_format R List of available tile formats. The first one is the default suggested to the client – Format: list of word blank separated: "jpeg", "png", "fits", "tsv""

**解读**：这是对 zstd 方案的**致命条款**。`hips_tile_format` 是 properties 文件里 **R（required）** 的 9 个必填关键字之一，其取值被限定为 `fits / jpeg / png / tsv` 四选若干。**规范中不存在任何 token 可以声明"本 HiPS 的 tile 是 zstd 压缩的"**。客户端据 `hips_tile_format` 的首项决定默认请求格式（Aladin Lite 的实现见 [S15]）。

---

### [S4] IVOA HiPS 1.0 §5.1 —— 唯一涉及"压缩"的条款（关键，且极易被误读）

- **URL**：同 [S1]，§5.1 "HiPS server"（p.22）
- **原文引文（逐字，含原文的语法瑕疵 "up the clients"）**：

> "Note: According to the HTTP site configuration, the tiles, notably the FITS tiles, may or may not be compressed. So it is up the clients to ensure that the required uncompression step is performed (as can be done transparently by the HTTP libraries)"

**解读（这是本次查证中最需要澄清的一点）**：
- 这句话出现在 **§5.1 "HiPS server"**，主语是"HTTP site configuration"，括号里给出实现机制"**as can be done transparently by the HTTP libraries**"——这是指 **HTTP 的 `Content-Encoding: gzip` 传输层压缩**，即服务器按 `Accept-Encoding` 协商、由 HTTP 库透明解压，**文件本身仍是标准 `.fits`，URL 不变、扩展名不变**。
- 实测验证：CDS 主 HiPS 服务器对 `.fits` tile 在默认请求下返回 `content-type: application/fits`、无 `content-encoding`；当请求头带 `Accept-Encoding: gzip, deflate, br, zstd` 时，**同一 URL 返回 `content-encoding: gzip`**（见 [S17]）。
- 因此 **§5.1 不能作为"`.fits.gz` 文件名合法"的依据**。它管的是传输层，不是磁盘 / URL 层的文件命名。

---

### [S5] IVOA HiPS 1.0 §6.1 / §6.2 / Appendix —— `{.ext}` 占位符与省略号

- **URL**：同 [S1]，§6.1（p.27）、§6.2（p.28）、Appendix（p.32）
- **原文引文（§6.2 Remote access）**：

> "Tiles => baseURL /NorderK/DirD/NpixN{.ext}
> where K is the order, N tile index, D=(N/10000)*10000 (integer division), {.ext} is .fits, .jpg, .png, …"

- **原文引文（Appendix）**：

> "Tile N in order K → NorderK/DirD/NpixN{.ext}
> where D=(N/10000)*10000 (integer division),
> and {.ext} is depending of the file format of the tiles (.fits, .jpg, .png …)"

**解读**：§6.1 / §6.2 / Appendix 的列表带省略号"…"，看似开放；但这是**非规范性的路径推导说明**（"Tiles and properties … are directly derived"），而 §4.2.1.3 的扩展名对应关系是**规范性的 MUST**。两处冲突时以 §4.2.1.3 为准。请勿用这个省略号论证 `.fits.gz` 合法。

---

### [S6] IVOA HiPS 2.0 Working Draft —— 唯一正面讨论"压缩 tile"的 IVOA 文本

- **文档名 / 版本**：*HiPS – Hierarchical Progressive Survey*, Version 2.0, **IVOA Working Draft 01 May 2026**（Editor: Pierre Fernique）
- **URL**：<https://www.ivoa.net/documents/HiPS/20260501/WD-HiPS-2.0-20260501.html>（索引 <https://www.ivoa.net/documents/HiPS/20260501/index.html>）
- **状态原文**："This is an IVOA Working Draft for review by IVOA members and other interested parties. … It is inappropriate to use IVOA Working Drafts as reference materials or to cite them as other than "work in progress"."
- **章节**：§4.1.2 "Tile formats" → 子节 "Format of image tiles"；§4.3.1；§4.3.2；Appendix C "Version History"

**原文引文（§4.1.2，基本格式句已放宽）**：

> "The tile format depends on the survey data type: FITS, JPEG, PNG for image; TSV for catalogues. **These basic tile formats are not an obligation, but are suggested** in order to facilitate checking of the tile content with basic file tools and editors."

**原文引文（"Format of image tiles"）**：

> "Three image formats are suggested in this document to package the HiPS tiles for images: FITS (FITS Working Group, n.d.), PNG, JPEG. … **The tile file extension must correspond to the format: ".fits" for FITS, ".jpg" for JPEG, ".png" for PNG. These extensions must be in lowercase.**"

**原文引文（"Format of image tiles"，紧接上句 —— 本文最重要的一段）**：

> "**A HiPS can use any another formats if this proves more appropriate, bearing in mind that this will reduce the number of clients able to access such a HiPS. A good example is the RICE FITS compression alternative** (1993, 14)Rice, Yeh and Miller **or WEBP alternative** (Zern and Bankoski et al., 2024). In this last case, it should be prudent to add rather than replace the collection of tiles in this alternative format 1"

**原文引文（同页脚注 1，逐字）**：

> "1For the WEBP alternative, the file extension will logically be ".webp." **In the case of RICE FITS, the extension should remain ".fits" because it is a compression mode that does not change the nature of the file since this compression has been integrated into the FITS standard** (FITS Working Group, n.d.)."

**原文引文（§4.3.1 "WCS solution for FITS tiles"）**：

> "Although each tile, thanks to its HiPS address based on order and index, is perfectly located spatially on the sphere, **it may be useful to add** the spatial calibration WCS corresponding to this location in the header of the FITS tiles, following the FITS convention for HEALPIX (HPX) projection (FITS reference + Calabretta paper). This way, any FITS manipulation tool, even outside the HiPS context, will be able to spatially manipulate these HiPS FITS tiles."

**原文引文（§4.3.2 "Trim reduction for FITS tiles"）**：

> "Unlike traditional compression algorithms such as GZIP or RICE, this method is trivial and does not add any additional processing time, either for writing or reading. Plus, it retains direct pixel access property."

**原文引文（Appendix C "Version History"）**：

> "Alternatives for tile encoding, notably WEBP and RICE FITS, have been mentioned, along with the associated constraints"

**解读**：
- 2.0 WD 把"三种格式"从**硬枚举**降级为 "suggested / not an obligation"，并显式允许 "any another formats"，**同时保留扩展名 MUST**。
- 它给出的**判据非常清晰**：新增编码方式要么**换扩展名**（WEBP → `.webp`），要么**保持 `.fits`——前提是该压缩"已被整合进 FITS 标准"**（RICE 就是这一类）。
- 按这个判据：**zstd 若作为 FITS 内部压缩（ZIMAGE / ZCMPTYPE），必须先进 FITS 标准（见 [S8]）；若作为外部容器压缩，则必须换扩展名并需要一个能声明的格式 token（规范未定义）**。两条路当前都走不通。
- 注意 §4.3.2 把 **GZIP 与 RICE 并列**称为 "traditional compression algorithms"，说明作者把它们都视为"压缩算法"，而不是"格式"。

---

### [S7] HiPS 2.0 WD §5.1 —— 1.0 的压缩 Note **已被删除**

- **URL**：同 [S6]，§5.1 "HiPS node"
- **事实**：2.0 WD §5.1 段落从 "…rather than in a basic file system directory." 直接接 "Additionally, a HiPS node must implement a dedicated URL…"，**1.0 §5.1 那条 "may or may not be compressed" 的 Note 在 2.0 WD 中不存在**。我对 2.0 WD 全文 HTML 做 `compress` 全文检索，仅命中两处（"good file compression" 讲 JPEG；"traditional compression algorithms such as GZIP or RICE" 讲 TRIM），**均与 tile 文件压缩无关**。

**解读**：这条 Note 被删，可能是编辑疏漏，也可能是有意为之。无论如何，**它不应被当作"HiPS 允许压缩 tile 文件"的规范依据**——它的原意（HTTP 传输层）在 [S4][S17] 已证。

---

### [S8] FITS Standard 4.0 §10 —— tile-compression 是 FITS 标准正文的一部分

- **文档名 / 版本**：*Definition of the Flexible Image Transport System (FITS)*, **Version 4.0, 13 August 2018**（IAU FITS Working Group 2016-07-22 批准）
- **URL**：<https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf>（索引 <https://fits.gsfc.nasa.gov/fits_standard.html>）
- **章节**：§10 "Representations of compressed data"；§10.1 "Tiled image compression"；§10.1.1 "Required keywords"；§10.4 "Compression algorithms"；Table 36；§10.4.2 "Gzip compression"；§G.3 "File extensions"

**原文引文（§10 引言）**：

> "The following sections describe compressed representations of data in FITS images and BINTABLE extensions that preserve metadata and allow for full or partial extraction of the original data as necessary. The resulting FITS file structure is independent of the specific data-compression algorithm employed. The implementation details for some compression algorithms that are widely used in astronomy are defined in Sect. 10.4, but other compression techniques could also be supported. See the FITS convention by White et al. (2013) for details of the compression techniques, but beware that the specifications in this Standard shall supersede those in the registered convention."

**原文引文（§10.1）**：

> "The following describes the process for compressing n−dimensional FITS images and storing the resulting byte stream in a variable-length column in a FITS binary table, and for preserving the image header keywords in the table header."

**原文引文（§10.1.1 Required keywords）**：

> "In addition to the mandatory keywords for BINTABLE extensions […] the following keywords are reserved for use in the header of a FITS binary-table extension to describe the structure of a valid compressed FITS image. **All are mandatory.**"
>
> （`[…]` 为省略标注：被省略片段是指向 BINTABLE 强制关键字一节的交叉引用，原文作 `Sect. §7.3.1`。）

（随后定义 `ZIMAGE`、`ZCMPTYPE`、`ZBITPIX`、`ZNAXIS`、`ZNAXISn`；§10.1.2 定义可选的 `ZTILEn`、`ZNAMEi`、`ZVALi`、`ZMASKCMP`、`ZQUANTIZ`、`ZDITHER0`。）

**原文引文（§10.4，Table 36 标题与取值 —— 穷举表）**：

> "Table 36: Valid mnemonic values for the ZCMPTYPE and ZCTYPn keywords"

| ZCMPTYPE 取值 | 原文描述（逐字） | 条款 |
|---|---|---|
| `'RICE 1'` | Rice algorithm for integer data | §10.4.1 |
| `'GZIP 1'` | Combination of the LZ77 algorithm and Huffman coding, used in GNU Gzip | §10.4.2 |
| `'GZIP 2'` | Like 'GZIP 1', but with reshuffled byte values | §10.4.2 |
| `'PLIO 1'` | IRAF PLIO algorithm for integer data | §10.4.3 |
| `'HCOMPRESS 1'` | H-compress algorithm for two-dimensional images | §10.4.4 |
| `'NOCOMPRESS'` | The HDU remains uncompressed | —（Table 36 内无独立小节） |

**原文引文（§10.4，紧随 Table 36 —— 新增算法的门槛）**：

> "The name of the permitted algorithms for compressing FITS HDUs, as recorded in the ZCMPTYPE keyword, are listed in Table 36; **if other types are later supported, they must be registered with the IAUFWG to reserve the keyword values.** Keywords for the parameters of supported compression algorithms have also been reserved… If alternative compression algorithms require keywords beyond those defined below, they must also be registered with the IAUFWG to reserve the associated keyword names."

**原文引文（§10.4.2 Gzip compression）**：

> "When ZCMPTYPE = 'GZIP 1', the Gzip algorithm shall be used for data (de)compression. There are no algorithm parameters, so the keywords ZNAMEn and ZVALn should not appear in the header."

**原文引文（§G.3 File extensions —— 回答"FITS 是否规定扩展名"）**：

> "The FITS Standard originated in the era when files were stored and exchanged via magnetic tape; **it does not prescribe any nomenclature for files on disk.** … In the absence of other information it is reasonably safe to presume that a file name ending in '.fits' is intended to be a FITS file."

**解读**：
- tile-compression **是 FITS 标准的一部分**（不是"某个第三方约定"），位于 §10；`fpack` / `funpack` 是它的参考实现（§10 脚注 15 直接给出 https://heasarc.gsfc.nasa.gov/fitsio/fpack/ ）。
- `ZCMPTYPE` 是**穷举表**，且明确"其他算法必须向 IAUFWG 注册"。**zstd 不在其中**，因此"用 ZCMPTYPE='ZSTD' 做 FITS 内部 zstd 压缩"**当前不是合法 FITS**。
- §G.3 说明 **FITS 标准本身不管文件扩展名**——扩展名的约束**完全来自 HiPS 规范**（[S1][S6]）。所以"`.fits` 必须是未压缩 FITS"这句话在 FITS 标准里没有，在 HiPS 规范里也没有；HiPS 规范有的是"扩展名必须与格式对应"，而 RICE / GZIP tile-compression 后的文件**仍然是 FITS 文件**（§10.1.1 的 BINTABLE + ZIMAGE），故仍叫 `.fits`。

---

### [S9] 注册 FITS 约定：Tiled Image Compression Convention v2.3

- **文档名 / 版本**：*Tiled Image Convention for Storing Compressed Images in FITS Binary Tables*, **Version 2.3, 2 July 2013**（White, Greenfield, Pence, Tody, Seaman）
- **URL**：<https://fits.gsfc.nasa.gov/registry/tilecompression.html>；PDF：<https://fits.gsfc.nasa.gov/registry/tilecompression/tilecompression2.3.pdf>
- **原文引文（registry 页 Description）**：

> "A convention for compressing FITS images and storing the compressed byte stream in a variable-length array column in a FITS binary table. Optionally, the image may be divided into rectangular tiles, and each tile is compressed separately. Any number of different compression algorithms can be supported. **Current implementations support GZIP, RICE, H-Compress, and the IRAF pixel list compression algorithms.** The fpack FITS file compression utility supports this table compression convention."

- **原文引文（registry 页 Submitted / Registered）**：

> "Submitted: Version 2.0 in November-2006 by W. Pence, NASA/GSFC (on behalf of the authors). Registered: March 2007"

- **原文引文（v2.3 文档 §2 Keywords）**：

> "ZCMPTYPE (required keyword) The value field of this keyword shall contain a character string giving the name of the algorithm that must be used to decompress the image. **Currently, values of GZIP 1, GZIP 2, RICE 1, PLIO 1, and HCOMPRESS 1 are reserved**, and the corresponding algorithms are described in a later section of this document. The value RICE ONE is also reserved as an alias for RICE 1."

**解读**：与 [S8] 一致，算法集合就是 RICE_1 / GZIP_1 / GZIP_2 / PLIO_1 / HCOMPRESS_1。**没有 zstd**。

---

### [S10] FITS Registry 页 —— tile-compression 已被并入 FITS 4.0

- **URL**：<https://fits.gsfc.nasa.gov/fits_registry.html>
- **原文引文**：

> "Conventions that have been reviewed and incorporated into the FITS Standard: The following 6 conventions were reviewed by the IAU FITS Working Group in 2016 and have been officially incorporated into version 4.0 of the FITS Standard. … **Tiled Image Compression** divides image into a grid of tiles, and stores the compressed tiles in a variable length array column of a binary table. **Tiled Table Compression** convention for compressing FITS binary tables, analogous to the tiled image compression convention."

**解读**：确认 tile-compression 的**法律地位**——从"注册约定"升级为"标准正文"（[S8] §10）。

---

### [S11] `.fz` 扩展名的来源 —— fpack，**不是** FITS 标准，也**不是** HiPS 规范

- **文档名**：*fpack Users Guide*（HEASARC / CFITSIO）
- **URL**：<https://heasarc.gsfc.nasa.gov/FTP/software/fitsio/c/docs/fpackguide.pdf>
- **原文引文（§F "Parameters that affect the input and output files"）**：

> "The compressed output file name is usually constructed by appending ".fz" to the input file name, and the input file is not deleted, but this behavior may be modified with the following parameters:"

**解读**：`.fz` 是 **fpack 这个工具的默认输出命名习惯**，是工具约定，不是标准条款。FITS 4.0 全文（做 `\.fz` 检索）**未出现 `.fz`**；HiPS 1.0 / 2.0 WD 全文**也未出现 `.fz`**。所以**"`.fz` 在 HiPS 里被允许吗"的准确回答是：HiPS 规范从未提及 `.fz`**；而且按 HiPS 的扩展名 MUST 条款，`.fz` 会**违反** "`.fits` for FITS"（除非把 `fits.fz` 当成一个新的 `hips_tile_format` token——见 [S15]，Aladin Lite 确实这么做了，但那是**客户端扩展**，不是规范）。

---

### [S12] Hipsgen 用户手册 —— CDS 官方生成器对 gzip 与 fpack 的立场（**最贴近本决策的一手证据**）

- **文档名 / 版本**：*Hipsgen – User manual*，作者 Pierre Fernique（CDS），"Related to versions 12.135 and following – includes HiPS3D"，Jan 2023 起多次增补至 2026
- **URL**：<https://aladin.cds.unistra.fr/hips/HipsgenManual.pdf>

**原文引文（"Tile size reduction" 段首 —— 总立场）**：

> "Hipsgen offers two possibilities to reduce the size of FITS tiles: edge removal and/or compression. **These two methods should only be used when strictly necessary. They are not standardized by the IVOA and are still subject to change, and the FITS tiles produced are currently only recognised by Aladin Desktop.**"

**原文引文（"External GZIP compression" 段 —— gzip 的具体行为）**：

> "**External GZIP compression:** Hipsgen allows you to apply GZIP compression to FITS tiles, either on the fly with the "-gzip" option, or via the "GZIP" action. **This action will gzip all FITS tiles in the HiPS hierarchy, excluding the deepest level**, and the "Allsky.fits" file if it has been generated32. This is a very efficient operation in terms of volume, but it is time consuming, of the same order of magnitude as the generation of the FITS tiles themselves. Conversely, via the GUNZIP action you can unzip FITS tiles that have been gzipped previously."

**原文引文（该页脚注 32 —— 直接回答"扩展名 + 客户端如何探测"）**：

> "32 **Note that FITS tiles, whether gzipped or not, will keep their ".fits" extension. It is up to the client to detect a possible compression thanks to the magic code at the beginning of the file, rather than using the file extension.**"

**原文引文（"Internal FITS 4.0 compressions" 段 —— fpack / RICE 的合规性判断）**：

> "**Internal FITS 4.0 compressions:** Finally, the latest version of the FITS standard describes several modes of compression using "internal" FITS compression. These compressions can be implemented on FITS tiles produced by Hipsgen using the "fpack33" utility in post-processing. In this way, it will be possible to apply RICE34 compression, which is more efficient than simple external GZIP compression as described in the previous paragraph. **This alternative method produces HiPS that complies with the IVOA 1.0 standard. However, tiles compressed in this way are currently only supported by HiPS Aladin Desktop clients version >= 12.61.** Just like with GZIP, compression using fpack will take about the same amount of time as generating the tiles."

**原文引文（"Compression" 段 —— Hipsgen 读输入压缩图像）**：

> "Compression: If your images are compressed, either externally by GZIP, BZIP2 or ZIP, or internally by RICE, GZIP1, GZIP2 or HCOMPRESS compression, you don't need to decompress them beforehand, Hipsgen will do it progressively, saving you the time and disk space needed to decompress the entire survey."

**解读（本文最有决策价值的一段）**：
- CDS 官方生成器**确实支持** gzip 压缩 FITS tile，但：
  1. **不换扩展名**——仍是 `.fits`（脚注 32）。所以 **`.fits.gz` 这个命名方式，连 Hipsgen 自己都不用**。
  2. **客户端必须靠 magic code 探测**，不能靠扩展名。
  3. Hipsgen 手册把"edge removal + compression"整体定性为 **"not standardized by the IVOA"**，且"**currently only recognised by Aladin Desktop**"。
  4. 对 **fpack / RICE** 给出了明确合规判定："**produces HiPS that complies with the IVOA 1.0 standard**"，代价是"currently only supported by HiPS Aladin Desktop clients version >= 12.61"。
- 注意措辞差别：**合规声明只给了 fpack / RICE，没给外部 gzip**。这与 [S6] 脚注 1 的判据完全吻合（RICE 因"已被整合进 FITS 标准"而保持 `.fits`）。
- "excluding the deepest level" 这个细节值得注意：gzip 只作用于**非最深阶**的 tile。

---

### [S13] Hipsgen 参考手册（v12.646）—— 命令行开关

- **URL**：<https://aladin.cds.unistra.fr/hips/HipsgenReferenceManual.html>
- **原文引文（Available options）**：

> "-notrim : [TILES,CONCAT,APPEND] Do not 'trim' FITS tiles
> -trim : [TILES,CONCAT,APPEND] 'trim' FITS tiles
> **-gzip : [TILES,CONCAT,APPEND] Gzip FITS tiles**"

- **原文引文（ACTION CLEANWEIGHT）**：

> "ACTION CLEANWEIGHT - Delete all WEIGHT tiles
> DESCRIPTION Removes weight tiles. Weight tiles are generated by the incremental=true option. They allow HiPS updates to be made while respecting the weighting on the pixels. Without scheduled updates, these tiles are not needed and double the size of the HiPS unnecessarily."

**解读**：`-gzip` 是**受支持的正式开关**（不是 hack）。另外附带一条与AstroCS相关的观察：Hipsgen 对"每个像素的权重 / 贡献计数"是**放在独立的 `HpxCounter` / WEIGHT tile 目录**里的，不是塞进主 tile——这可能是 signal / support / variance / ivar 四子产品设计的一个参考先例（**但这是工具约定，不是 IVOA 规范**；规范里 `dataproduct_type` 只有 image / catalog / cube）。

---

### [S14] Aladin Desktop 用户手册 —— 官方声明支持哪些格式与压缩

- **文档名 / 版本**：*Aladin – User Manual*，Pierre Fernique，**June 2022**，CDS
- **URL**：<https://aladin.cds.unistra.fr/java/AladinManual.pdf>
- **章节**：§8.1 "The types of data supported"（p.100–101）；命令行参数节

**原文引文（§8.1 首段）**：

> "Aladin supports most of the formats used in astronomy, whether for images, catalogues or data "groupings". In addition, it takes into account the most widespread compression algorithms."

**原文引文（§8.1 表格，逐行）**：

> "HCOMP | FITS image compression | Applicable to FITS images only
> FITS-RICE | FITS image compression | Applicable to FITS images only
> FITS-GZIP | FITS image compression | Applicable to FITS images only"
>
> "HiPS image | Progressive image survey | Pre-standards and IVOA standard (FITS,JPEG,PNG tile support)"
>
> "HiPS catalogue | Progressive catalogue survey | IVOA standard (TSV tile)"
>
> "HiPS cube | Cube progressive survey | Pre-standards and IVOA standard (FITS,JPEG,PNG tiles)"
>
> "GZIP | Compression | Applicable to all other formats"

**原文引文（§8.1 末尾 —— 直接回答"客户端如何探测格式"）**：

> "**Aladin automatically recognises the nature of the data based on the content: the extension of the file name or the presence of a "Content-type" for an http stream does not affect the recognition of the file.**"

**原文引文（命令行节）**：

> "The files specified in the command line can be :
> - images: **FITS (gzipped,bzipped,RICE,MEF,...)**, HEALPix maps, JPEG,GIF,PNG"

**解读**：
- Aladin Desktop 官方文档明确写 **HiPS image tile 支持 FITS / JPEG / PNG**（与规范一致），并**额外**把 `FITS-GZIP`、`FITS-RICE`、`HCOMP` 列为 "FITS image compression" 支持项。
- **"based on the content"** 这句是关键：Aladin **按内容（magic number）识别格式，扩展名与 Content-Type 都不影响识别**。这正好印证 Hipsgen 脚注 32 的设计（靠 magic code 探测 gzip）。

---

### [S15] Aladin Lite v3 源码 —— 实际支持的格式 token 与 URL 拼接

- **仓库 / 分支**：<https://github.com/cds-astro/aladin-lite>（`develop`，本次抓取于 2026-09-22）
- **原文引文（`src/core/al-api/src/hips.rs:179-186`）**：

> pub enum ImageExt {
>     Fits,
>     Jpeg,
>     Png,
>     Webp,
>     #[serde(alias = "fits.fz")]
>     FitsFz,
> }

- **原文引文（`src/core/al-api/src/hips.rs:198-208`，Display 实现 → 实际扩展名）**：

> ImageExt::FitsFz => write!(f, "fits.fz"),
> ImageExt::Fits => write!(f, "fits"),
> ImageExt::Png => write!(f, "png"),
> ImageExt::Jpeg => write!(f, "jpg"),
> ImageExt::Webp => write!(f, "webp"),

- **原文引文（`src/js/HiPS.js:637-657`，格式选择优先级）**：

> const chooseTileFormat = (formats) => {
>     if (formats.indexOf("webp") >= 0) { return "webp"; }
>     else if (formats.indexOf("png") >= 0) { return "png"; }
>     else if (formats.indexOf("jpeg") >= 0) { return "jpeg"; }
>     else if (formats.indexOf("fits") >= 0) { return "fits"; }
>     else if (formats.indexOf("fits.fz") >= 0) { return "fits"; }
>     else { throw ("Unsupported format(s) found in the properties: " + formats); }
> };

- **原文引文（`src/core/src/downloader/query.rs:102`，URL 拼接）**：

> let url = format!("{hips_url}/Norder{depth}/Dir{dir_idx}/Npix{idx}.{ext}");

- **原文引文（`src/core/src/tile_fetcher.rs:72`，fits.fz 解码状态）**：

> ImageExt::FitsFz => todo!(),

- **原文引文（`src/core/src/renderable/hips/config.rs:74-76`，格式必须已在 properties 中声明）**：

> if !properties.get_formats().contains(&img_ext) {
>     return Err(js_sys::Error::new("HiPS format not available").into());
> }

- **原文引文（`src/core/src/app.rs:856`，唯一使用 gzip 解压的位置）**：

> let gz = fitsrs::gz::GzReader::new(Cursor::new(bytes))
>     .map_err(|_| JsValue::from_str("Error creating gz wrapper"))?;

（该函数为 `add_fits_image`，即**独立 FITS 图像加载路径**；对整个 `src/` 做 `GzReader` 检索，**仅命中 app.rs 这三行**，HiPS tile 下载路径 `downloader/request/tile.rs` 不经过它。）

- **原文引文（`src/core/al-core/src/image/fits.rs:49` 起，tile 解码支持两种 HDU）**：

> match hdu {
>     HDU::XImage(hdu) | HDU::Primary(hdu) => { … }
>     HDU::XBinaryTable(hdu) => { … if let Some(TileCompressedImage { z_bitpix: bitpix, z_naxisn: naxis, .. }) = &bin_table.get_z_image() { … } }
> }

**解读**：
- Aladin Lite v3 的 tile 格式 token 集合是 **`fits` / `jpg` / `png` / `webp` / `fits.fz`**——**已经超出 IVOA 的 `fits / jpeg / png` 词表**（`webp`、`fits.fz` 都是客户端侧扩展）。
- 格式**完全由 properties 里的 `hips_tile_format` 驱动**（config.rs 的报错分支），URL 就是 `…/Npix{idx}.{ext}`。**没有 zstd token，也没有 `.fits.zst` / `.fits.gz` 路径**。
- `FitsFz => todo!()` 说明 `fits.fz` 在 v3 里**尚未实现解码**。
- tile 路径**不做 gzip magic 探测**；gzip 解压只出现在独立的 FITS 图像加载路径。也就是说：**gzip 压缩的 HiPS tile 在 Aladin Lite v3 上很可能读不了**（需注意这是对 `develop` 分支代码的静态判读，不是运行验证——见"未能核实"节）。
- tile 解码支持 `ZIMAGE` 的 BINTABLE——即 **fpack / RICE tile-compressed FITS 在 Aladin Lite v3 的架构里是被支持的**（与 Hipsgen 手册"only Aladin Desktop"的说法相比，Lite 已跟进）。

---

### [S16] fitsrs —— Aladin Lite 使用的 FITS 读取库，明确列出支持 / 不支持的压缩

- **仓库**：<https://github.com/cds-astro/fitsrs>（`master`）
- **原文引文（README，第一段）**：

> "This parser was initiated for reading FITS images mapped onto HEALPix cells in the sky (**See the HiPS IVOA standard**) in order to use it in the Aladin Lite web sky atlas."

- **原文引文（README）**：

> "A very new support of binary table extension has been added. This has been done mainly for supporting the **tiled compressed image convention** that describes the storing of tile images in variable length arrays of a binary table."

- **原文引文（README，Features 列表）**：

> "* [X] Tiled image convention for storing compressed images in FITS binary tables
>     - [X] **Compression supported, GZIP, GZIP2 and RICE on u8, i16, i32 and f32.**
>     - [ ] **H_compress and PLI0 compressions**
>     - [X] Dithering techniques for floating point images. Not well tested"

- **源码事实**：`src/gz.rs` 使用 `flate2::read::GzDecoder` 实现外部 gzip 读取；`src/hdu/data/bintable/tile_compressed/rice.rs` 注释写明 "This code is a port in Rust of CFITSIO's ricecomp.c"。

**解读**：CDS 自家 FITS 库支持 **外部 gzip + GZIP_1 / GZIP_2 / RICE_1 内部压缩**，不支持 HCOMPRESS / PLIO，**完全没有 zstd**。

---

### [S17] CDS HiPS 服务器 HTTP 行为实测 —— 证实 §5.1 的 Note 是传输层压缩

- **实测命令与响应**（2026-09-22，`https://alasky.cds.unistra.fr`，Apache/2.4.67）：

  默认请求：
  > HTTP/2 200 … content-type: application/fits … vary: Accept-Encoding … content-length: 527168

  带 `Accept-Encoding: gzip, deflate, br, zstd`：
  > HTTP/2 200 … content-encoding: gzip … content-type: application/fits … etag: "80b40-4c4b0d85b6880-gzip"

**解读**：**同一个 `.fits` URL，服务器按协商结果返回 gzip 传输编码**。这就是 HiPS 1.0 §5.1 Note 所说的场景（"as can be done transparently by the HTTP libraries"）。**它是 HTTP 层特性，不改变 tile 的文件格式与扩展名**，因此**与"`.fits.gz` 文件命名是否合法"是两个不同的问题**。

---

### [S18] 实测真实 HiPS FITS tile 的 FITS 头 —— 规范并未强制任何 HEALPix 关键字

- **取样 1**：`https://alasky.cds.unistra.fr/DSS/DSS2Merged/Norder3/Dir0/Npix487.fits`（527168 字节，BITPIX=16）
  完整头：
  > SIMPLE  =                    T
  > BITPIX  =                   16
  > NAXIS   =                    2
  > NAXIS1  =                  512
  > NAXIS2  =                  512
  > BLANK   =             -32768.0
  > END

- **取样 2**：`https://alasky.cds.unistra.fr/2MASS/J/Norder3/Dir0/Npix487.fits`（1054080 字节，BITPIX=-32）
  完整头：
  > SIMPLE  =                    T
  > BITPIX  =                  -32
  > NAXIS   =                    2
  > NAXIS1  =                  512
  > NAXIS2  =                  512
  > CPYRIGHT= 'See HiPS properties file'
  > COMMENT = 'HiPS FITS tile generated by Aladin/Hipsgen v11.023'
  > ORDER   =                    3
  > NPIX    =                  487
  > CRPIX1  =              -4607.5
  > CRPIX2  =              -4095.5
  > CD1_1   = -1.0986328125000E-02
  > CD1_2   = -1.0986328125000E-02
  > CD2_1   =  1.0986328125000E-02
  > CD2_2   = -1.0986328125000E-02
  > CTYPE1  = 'RA---HPX'
  > CTYPE2  = 'DEC--HPX'
  > CRVAL1  =                   0.
  > CRVAL2  =                   0.
  > PV2_1   =                    4
  > PV2_2   =                    3
  > END

**解读（直接回答第 3 问的前半）**：
- **HiPS 规范对 tile 的 FITS 头关键字没有任何强制要求。** 对 HiPS 1.0 REC 与 2.0 WD 全文检索 `PIXTYPE / ORDERING / COORDSYS / NSIDE / FIRSTPIX / LASTPIX / HIPSTILEWIDTH`——**零命中**（2.0 WD 只有 §4.3.1 建议性地提到 WCS / HPX 关键字，用词是 "it may be useful to add"）。
- 实测的两块 CDS tile 也确实**不含**上述任何一个关键字。DSS2 的 tile 连 `ORDER` / `NPIX` 都没有。
- 用户问题里列的 **`PIXTYPE` / `ORDERING` / `COORDSYS`** 实际属于 **MOC 标准**（见 [S19]），不是 HiPS tile 的要求；**`HIPSTILEWIDTH`** 在 IVOA 文档、FITS 文档、Hipsgen 手册、Aladin 手册中**均未找到任何出处**（见"未能核实"节）。

---

### [S19] IVOA MOC 2.0 Recommendation §6 —— PIXTYPE / ORDERING / COORDSYS 的真正归属

- **文档名 / 版本**：*MOC – Multi-Order Coverage map*, Version 2.0, **IVOA Recommendation 27 July 2022**
- **URL**：<https://www.ivoa.net/documents/MOC/20220727/REC-moc-2.0-20220727.html>
- **原文引文（§6 FITS keywords）**：

> "For the binary representations which are packaged in binary FITS table, we define a set of FITS keywords, their possible values and set when those fields are required, optional or recommended in Table 3 … Since MOC 1.1 (Fernique and Boch et al., 2019) the PIXTYPE = "HEALPIX" keyword/value is no longer required, and should be omitted."

- **原文引文（Table 3 部分行）**：

> "MOCVERS | The version of the MOC encoding standard. … | NA | mandatory
> MOCDIM | Physical(s) dimension(s). … | NA | mandatory
> ORDERING | The packaging method used. It is either NUNIQ (V1.1 or V2.0 SMOC) or RANGE (V2.0). | mandatory | mandatory
> COORDSYS | The coordinate system in use. The value must be 'C' for SMOC (ICRS). | mandatory | mandatory
> PIXTYPE | 'HEALPIX' | mandatory | NA"

**解读**：`PIXTYPE` / `ORDERING` / `COORDSYS` 是 **`Moc.fits` 文件**（HiPS 根目录下的覆盖图）的关键字，由 **MOC 标准**规定，**与 tile 无关**。`NSIDE` / `FIRSTPIX` / `LASTPIX` 则属于更早的 MOC 1.0 显式编码与 HEALPix FITS 约定（我未在 MOC 2.0 正文中检索到这三个词的强制用法）。**不要把 MOC 的关键字要求误加到 tile 上。**

---

### [S20] 补充：Hipsgen 的 WEIGHT tile 先例（与AstroCS多子产品设计相关）

见 [S13] 的 `CLEANWEIGHT` 引文。Hipsgen 把"每像素权重 / 贡献计数"放在 **独立的 `HpxCounter` / WEIGHT tile 集合**中，而非嵌入主 tile。这是**工具约定，不是 IVOA 规范**（HiPS 1.0 的 `dataproduct_type` 仅 image / catalog / cube）。

---

### [S21] 负面结论：**没有任何 IVOA 文档讨论过 zstd**

对以下文本做 `zstd|zstandard`（大小写不敏感）全文检索，**全部 0 命中**：

| 文本 | 命中数 |
|---|---|
| HiPS 1.0 Recommendation（PDF 提取全文） | 0 |
| HiPS 2.0 Working Draft（HTML 全文） | 0 |
| Hipsgen User Manual（PDF 提取全文） | 0 |
| IVOA 文档总索引 <https://ivoa.net/documents/> | 0 |

并且对 IVOA 文档总索引页做 `compress` 检索也是 **0 命中**——索引页里**没有任何标题含 "compression" 的 IVOA 规范或 Note**。唯一含 "FITS" 的条目是 "FITS Headers for Scans of Photographic Plates 1.0"（与本议题无关）。

**解读**：截至本次查证，**IVOA 体系内不存在任何关于 zstd 的规范、Note 或讨论记录**（至少在文档标题层面与三份关键 HiPS 文档全文层面）。

---

## 三、未能核实的项

1. **`HIPSTILEWIDTH` 关键字的出处**：未找到。检索范围：HiPS 1.0 REC 全文、HiPS 2.0 WD 全文、FITS 4.0 全文、Hipsgen 手册 / 参考手册全文、Aladin 手册全文、MOC 2.0 全文、两块真实 CDS tile 的实际头。**均为 0 命中**。可能是某个第三方 HiPS 工具的私有关键字，或提问方的记忆偏差。**建议不要把 `HIPSTILEWIDTH` 当作规范关键字使用。**
2. **ESAC / Planck HiPS 官方文档**：抓不到。尝试过的 URL 全部失败：
   - `https://www.cosmos.esa.int/web/esdc/hips` → HTTP 404
   - `https://www.cosmos.esa.int/web/esac-science-archive/hips` → HTTP 404
   - `https://www.cosmos.esa.int/web/esac-science-archive/hips-and-mocs` → HTTP 404
   - `https://www.cosmos.esa.int/web/esasky/hips` → HTTP 404
   - `https://www.cosmos.esa.int/web/planck/hips` → HTTP 404
   - `https://sky.esa.int/` → HTTP 200 但为纯 JS 单页应用，静态 HTML 无内容（"Your web browser must have JavaScript enabled"）
   **因此本文不对 ESAC / Planck HiPS 的 tile 格式支持作任何断言。**
3. **IVOA wiki**：抓不到。`https://wiki.ivoa.net/twiki/bin/view/IVOA/Hips` → HTTP 404；`.../HiPS2` → HTTP 404；搜索接口 `.../search/IVOA?search=zstd` → HTTP 403（需鉴权）。**因此"IVOA wiki 上是否讨论过 zstd"未能核实**（只能确认官方文档体系内没有）。
4. **hips2fits 的 tile 解码实现**：只抓到服务端 API 文档页（<https://alasky.cds.unistra.fr/hips-image-services/hips2fits>），其参数说明只有输出格式："Allowed values are fits (default), jpg and png"——这是**输出**格式，不是 tile 读取能力。hips2fits 的**源码仓库未找到**（`cds-astro/hips2fits`、`cds-astro/hips` 均返回 404）。**因此 hips2fits 能否读 gzip 压缩 tile，未能核实。**
5. **Aladin Lite 是否真能读 gzip 压缩的 HiPS tile**：本节给出的是 `develop` 分支源码的**静态判读**（`GzReader` 只出现在独立 FITS 图像路径 `app.rs:856`，tile 路径不经过它），**未做运行验证**。这是"很可能不支持"的强推断，不是实测结论。
6. **HiPS 1.0 的 `.doc` 版本**：未抓取（只用了 PDF 与提取文本）。PDF 为权威发布格式，已逐字核对关键句。
7. **FITS 4.0 与注册约定 v2.3 的差异**：FITS 4.0 §10 明确 "the specifications in this Standard shall supersede those in the registered convention"，但**未逐条比对二者差异**（例如 v2.3 保留的 `RICE ONE` 别名是否仍被 FITS 4.0 接受）。这对手头的 zstd 判断无影响。

---

## 四、对"zstd 压缩 FITS tile 是否符合 HiPS 标准"的直接判断

### 判断：**规范未规定 + 生态不支持（且按现行 HiPS 1.0 的扩展名 MUST 条款，属不符合）**

理由分四层：

**第 1 层 —— 现行 HiPS 1.0（Recommendation，唯一可引用版本）：不符合。**
- §4.2.1.3 的强制条款是 "The tile file extension **must** correspond to the format: `.fits` for FITS, `.jpg` for JPEG, `.png` for PNG"。zstd 压缩后的字节流**不再是 FITS**（没有 `SIMPLE` 卡、不是 2880 字节块结构），所以叫 `.fits` 违反该 MUST；叫 `.fits.zst` 同样违反（扩展名不对应 FITS / JPEG / PNG 三者之一）。
- `hips_tile_format` 是 properties 的 **R（必填）** 关键字，词表封闭为 `fits / jpeg / png / tsv`——**没有 token 能声明 zstd**。客户端按此决定请求，无法得知该取什么。
- HiPS 1.0 全文**不含 gzip / zstd / fpack / `.fz`**；§5.1 的 "may or may not be compressed" 经 [S4][S17] 证实是 **HTTP 传输层**语义，不能援引。

**第 2 层 —— FITS 标准侧：zstd 作为"FITS 内部压缩"也不合法。**
- FITS 4.0 §10.4 Table 36 的 `ZCMPTYPE` 取值是**穷举表**（`RICE 1` / `GZIP 1` / `GZIP 2` / `PLIO 1` / `HCOMPRESS 1` / `NOCOMPRESS`），且明文 "if other types are later supported, **they must be registered with the IAUFWG**"。
- **zstd 不在表内，也没有 IAUFWG 注册记录可查**。因此无法走"`ZIMAGE` + `ZCMPTYPE='ZSTD'` 但仍是 `.fits`"这条路。

**第 3 层 —— HiPS 2.0 WD 给了理论口子，但需要规范补丁，当前仍是"未规定"。**
- 2.0 WD 明说 "A HiPS **can use any another formats**"，并给出判据：RICE 保持 `.fits` 因为 "**this compression has been integrated into the FITS standard**"；WEBP 换 `.webp`。
- 按此判据，zstd 要走通需要**二者之一**：(a) zstd 被纳入 FITS 标准（IAUFWG 注册 `ZCMPTYPE`）；或 (b) 定义一个新扩展名（如 `.fits.zst`）**并**在 `hips_tile_format` 词表中加入对应 token。**当前两件事都没有发生**，所以 2.0 WD 的这句话**不构成对 zstd 的授权**，只是承认"未来可以扩"。
- 而且 2.0 是 **Working Draft**，其状态声明明文禁止作为引用依据。

**第 4 层 —— 生态：无任何客户端支持 zstd tile。**
- Aladin Desktop：官方支持列表是 `HCOMP` / `FITS-RICE` / `FITS-GZIP` + `GZIP (Applicable to all other formats)`，**无 zstd**。
- Aladin Lite v3：格式 token 集合 `fits / jpg / png / webp / fits.fz`（且 `fits.fz` 解码还是 `todo!()`），**无 zstd**。
- fitsrs（CDS 的 FITS 库）：支持外部 gzip + `GZIP_1` / `GZIP_2` / `RICE_1`，HCOMPRESS / PLIO 未实现，**无 zstd**。
- Hipsgen：有 `-gzip`，**无 zstd**；且手册明说这类体积优化手段 "**not standardized by the IVOA** … currently only recognised by Aladin Desktop"。
- CDS 服务器：只在 **HTTP `Content-Encoding`** 层提供 gzip（实测确认），不提供 `.fits.zst` 资源。

### 对 AstroCS 的可执行建议（按推荐度排序）

1. **首选：不做文件级压缩，改用 HTTP 传输层压缩。** 服务器开 `Content-Encoding`（gzip / br / zstd 均可，由 Web 服务器协商），文件仍是标准 `.fits`，扩展名、`hips_tile_format`、客户端全部无需改动。这是 HiPS 1.0 §5.1 Note 覆盖的做法，**完全合规**。
2. **次选：若要降低磁盘 / 带宽占用且必须文件级压缩，用 FITS 4.0 §10 的 tile-compression（fpack，`RICE_1` 或 `GZIP_2`）。** 它**是 FITS 标准的一部分**，HiPS 2.0 WD 脚注 1 明确认可其保持 `.fits` 扩展名，Hipsgen 手册明确判定 "**complies with the IVOA 1.0 standard**"。代价：客户端支持面窄（Hipsgen 手册称需 Aladin Desktop ≥ 12.61；Aladin Lite v3 架构上支持 `ZIMAGE` BINTABLE，但 `fits.fz` token 路径未实现）。
3. **若确要 gzip 外部压缩**：**不要用 `.fits.gz` 文件名**——Hipsgen 的做法是**保持 `.fits` 扩展名、靠 magic code 探测**（其手册脚注 32）。但要清楚这**已被 CDS 自己定性为 "not standardized by the IVOA"**，且仅 Aladin Desktop 确认支持。
4. **不要采用 zstd 压缩 tile**：无规范依据（HiPS 1.0 扩展名 MUST 直接冲突；FITS `ZCMPTYPE` 穷举表不含）、无声明手段（`hips_tile_format` 无 token）、无客户端支持（Aladin / Aladin Lite / fitsrs / Hipsgen 全部为 0）。若将来要推动，正确路径是**先向 IAU FITS Working Group 注册 `ZCMPTYPE` 取值**，再推动 IVOA HiPS 把 token 写进 `hips_tile_format` 词表。

### 补充：多子产品（signal / support / variance / ivar）的标准归属

AstroCS的四个子产品（signal / support / variance / ivar）在 HiPS 1.0 中**没有对应的标准机制**：`dataproduct_type` 只有 `image` / `catalog` / `cube`，`hips_tile_format` 只管编码格式不管"同一像素的多个物理量"。Hipsgen 处理"权重"的方式是**另开目录**（`HpxCounter` / WEIGHT tiles，见 [S13]），且同样**不是 IVOA 规范**。这意味着"四个子产品各自成一个 HiPS 目录树、各有一份 `properties`"是当前唯一有先例可循的做法；**把四者塞进同一 tile 的多 HDU 或改成 zstd 容器，都既无规范依据也无客户端支持**。这一点建议在决定压缩方案时一并纳入评估。

---

*本文所有引文均来自实际抓取的原文（PDF 经 pdftotext 提取、HTML 经标签剥离后核对、源码经 GitHub raw 抓取、HTTP 行为经 curl -I 实测）。凡未能核实者已在第三节明确标注，未作任何推测性填充。*
