# 文献核验成稿 · CIT-19

**基线**: c8f64e9a  
**批次号**: CIT-19  
**网络自检门**: arXiv API 返回码 200 (curl -sS -o /dev/null -w '%{http_code}' 'https://export.arxiv.org/api/query?id_list=2207.12005')  
**途径**: URL HTTP HEAD / GET  

---

## 269 · NASA FITS Tile Compression 文档 —— 核验态：完成

**存在性**: 已核 (URL: https://fits.gsfc.nasa.gov/registry/tilecompression/tilecompression2.3.pdf → HTTP 200 OK)  
**关联性**: 关联对 — 该 URL 在位点文档 `[S9]` 节中用于支持"FITS tile-compression 注册约定 v2.3"的存在性及其中包含的算法集合（GZIP_1/GZIP_2/RICE_1/PLIO_1/HCOMPRESS_1）的断言；实际抓取 PDF 内容与文档引文一致。  
**版本**: 未钉版次 — 文档标题含"Version 2.3, 2 July 2013"，但该验证包未在任何需要版本区分的断言中使用（即没有"v2.3 特有而早期版本没有"的条款被支撑），仅作为常规参考依据 → **未钉版次**。  

<!-- PROGRESS: 6/6 -->

## 270 · NASA FITS Registry 页面 —— 核验态：完成

**存在性**: 已核 (URL: https://fits.gsfc.nasa.gov/fits_registry.html → HTTP 200 OK)  
**关联性**: 关联对 — 该 URL 在位点文档 `[S10]` 节中用于支持"tile-compression 已被并入 FITS 标准正文 §10"的断言；实际抓取 HTML 页面显示"Tiled Image Compression"已在列表中且说明文字匹配。  
**版本**: 未钉版次 — 属持续维护的网页，无明确版本号标签；文档中引用的内容（"6 conventions were reviewed... incorporated into version 4.0"）对应的是 FITS 4.0 的既定事实，不因网页更新而改变 → **未钉版次**。  

---

## 271 · Fpack User Guide PDF —— 核验态：完成

**存在性**: 已核 (URL: https://heasarc.gsfc.nasa.gov/FTP/software/fitsio/c/docs/fpackguide.pdf → HTTP 200 OK)  
**关联性**: 关联对 — 该 URL 在位点文档 `[S11]` 节中用于支持"` .fz` 扩展名是 fpack 工具的默认输出命名习惯，而非标准条款"的断言；文档引用的 §F 原文"gzip compressed output file name is usually constructed by appending '.fz'"确能支撑该结论。  
**版本**: 未钉版次 — 文档标题为"fpack Users Guide (HEASARC / CFITSIO)"但未标注具体版本号； `.fz` 作为工具默认行为的经验法则在不同版本间保持稳定 → **未钉版次**。  

---

## 272 · Aladin Hipsgen Reference Manual —— 核验态：完成

**存在性**: 已核 (URL: https://aladin.cds.unistra.fr/hips/HipsgenReferenceManual.html → HTTP 200 OK)  
**关联性**: 关联对 — 该 URL 在位点文档 `[S13]` 节中用于支持"Hipsgen 官方命令行开关 `-gzip` 正式受支持"的断言；文档引用"Available options"列表中的"-gzip : [TILES,CONCAT,APPEND] Gzip FITS tiles"与实际页面内容一致。  
**版本**: 未钉版次 — 文档标题含"(v12.646)"作为版本号，但本批次未在该 URL 支撑的任何断言中使用版本敏感条款（如"只有 v12.646+ 才有-gzip 开关"）→ **未钉版次**。  

---

## 273 · Aladin Desktop User Manual PDF —— 核验态：完成

**存在性**: 已核 (URL: https://aladin.cds.unistra.fr/java/AladinManual.pdf → HTTP 200 OK)  
**关联性**: 关联对 — 该 URL 在位点文档 `[S14]` 节中用于支持"Aladin Desktop 官方声明支持的压缩格式列表（HCOMP/FITS-RICE/FITS-GZIP/GZIP）及'按内容识别'原则"的断言；文档引用的表格与原文逐行匹配。  
**版本**: 文档标题含"June 2022"，该年份版本在文档中有具体条款支撑（§8.1 的压缩支持表、命令行参数等），但这些属于**稳定功能声明**而非"v2.0 才有的新特性"；未发现需要区分"2022 vs 其他版本"的内容 → **未钉版次**。  

---

## 274 · Alasky Web App URL —— 核验态：完成

**存在性**: 已核 (URL: https://alasky.cds.unistra.fr → HTTP 200 OK，响应 HTTP/2 200)  
**关联性**: UNPROVEN — 在位点文档中，该 URL 出现在三个位置：  
- [S17] "CDS HiPS 服务器 HTTP 行为实测"：用 `https://alasky.cds.unistra.fr` 作为测试靶点抓取自研响应头，**URL 本身不是被引证据，而是测试端点**；  
- [S18] 两块真实 tile 取样：用的是完整 tile URL (`.../Npix487.fits`)，非根域名；  
- 未能核实项 #2：ESAC/Planck HiPS URL 全部失败，此处提到 Alasky 作为对比参照。  
**因此该 URL 从未被用来支撑某句断言，它只是实验对象或占位符** → **关联 UNPROVEN（角色未绑定）**。  

---

<!-- PROGRESS: 6/6 -->

---

## 本批三态计数

**存在性**: 已核=6 / 不存在=0 / UNPROVEN=0  
**关联性**: 关联对=5 / 关联错=0 / 角色错绑=0 / UNPROVEN=1  
**版本**: 版本对=0 / 版本错=0 / 未钉版次=6  

---

## UNPROVEN 清单

| 序号 | 标识符类型 | 用过哪些标识符/URL | 走过哪些途径 | 失败原因 |
|---|---|---|---|---|
| 274 | URL | https://alasky.cds.unistra.fr | curl HEAD → HTTP 200 OK | 该 URL 在文档中作为**测试靶点**而非**被引证据**使用，无明确断言支撑需求 → 关联性无法判定 |

---

## 新发现的缺陷形态

**URL 作为占位符而非被引证据**：条目 274 暴露一种特殊形态——某些 URL 在文档中是**测试目标**或**占位符**，并非真正"引用某外部文献以支撑断言"。这种情况在 CIT 池重建时可能误判为"P-1 A 类全套三字段"，实际需要单独标记为"关联性 UNPROVEN（角色未绑定）"。建议在下轮台账修订中增加"引用角色"分类：support / test-target / placeholder。

---

## 覆盖率自报

**本批已完成 6/6 分配条数**（全部 6 条均为 P-1 A 类，存在性未核，需全套三字段核验）。
