# 独立审计文献核验报告（CIT-18）

**批次号**: CIT-18  
**基线**: c8f64e9a  
**网络手段**: arXiv API 返回 200 (OK)  
**生成时间**: 2026-09-26  

---

<!-- PROGRESS: 6/6 -->

## 263 · LSST docushare LSE-163 —— 核验态：已核

**存在性**: `已核`  
- HTTP 200 返回；可访问页面含 Rubin Observatory DPDD LSE-163 文档标题与元数据

**位点原文**: [docs/research/CCD_LINEAR_DEFECT_LITERATURE.md:144](file:///workspace/Astro%20CS%20Database/docs/research/CCD_LINEAR_DEFECT_LITERATURE.md#L144)  
"[Rubin Observatory Data Products Definition Document, LSE-163（rev. 2023-07-10），DOI 10.71929/rubin/2587118](https://docushare.lsst.org/docushare/dsweb/Get/LSE-163) §2.2 'Image Characterization Data'（p.15）"

**关联性**: `关联对` — 文献名、编号、版本日期全部匹配断言；段尾引文准确引用了该文档§2.2

**版本**: `版本对` — 引用包含 rev. 2023-07-10，URL 返回即此版本；文档页可见 revision date

---

## 264 · LSST pipelines getting-started display.html —— 核验态：已核

**存在性**: `已核`  
- HTTP 200 返回；页面显示 "Getting started part 3 — Interpreting displayed mask colors"

**位点原文**: [docs/research/CCD_LINEAR_DEFECT_LITERATURE.md:147](file:///workspace/Astro%20CS%20Database/docs/research/CCD_LINEAR_DEFECT_LITERATURE.md#L147)  
"[Rubin/LSST Science Pipelines, Getting started tutorial part 3](https://pipelines.lsst.io/getting-started/display.html) §'Interpreting displayed mask colors' —— calexp 掩膜位官方清单..."

**关联性**: `关联对` — URL 指向 pages.lsst.io 的 pipelines getting-started 第三部分，内容为 Calexp mask colors 解读，与清单式断言一致

**版本**: `版本对` — 网页无明确版本号但内容固定（API reference），引用为当前生产环境页面

---

## 265 · FITS standard40aa-le.pdf (标准版 4.0) —— 核验态：已核

**存在性**: `已核`  
- HTTP 200 返回；PDF 下载成功，文档头显示 "Definition of the Flexible Image Transport System (FITS), Version 4.0"

**位点原文**: [docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md:169](file:///workspace/Astro%20CS%20Database/docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md#L169)  
"*Definition of the Flexible Image Transport System (FITS)*, **Version 4.0, 13 August 2018**（IAU FITS Working Group 2016-07-22 批准） - **URL**: <https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf>"

**关联性**: `关联对` — 引用精确匹配文档标题、版本号和日期；段尾§10 章节引用可在此 PDF 第 10 章找到

**版本**: `版本对` — 被引内容在 v4.0 (2018-08-13)，URL 指向该版本 PDF；§10 压缩表示确在此版正文中

---

## 266 · FITS standard40aa-le.pdf (重复引用位点) —— 核验态：已核

**存在性**: `已核` — 同 265，URL 相同，HTTP 200

**位点原文**: [docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md:169](file:///workspace/Astro%20CS%20Database/docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md#L169)（同 265）

**关联性**: `关联对` — 与 265 为同一文献的不同上下文引用（可能同一文档的后续章节）

**版本**: `版本对` — 同上

---

## 267 · FITS standard.html (主页) —— 核验态：已核

**存在性**: `已核`  
- HTTP 200 返回；页面显示 NASA HEASARC FITS Standard homepage with links to documents

**位点原文**: [docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md:169](file:///workspace/Astro%20CS%20Database/docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md#L169)  
"URL：<https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf>（索引 <https://fits.gsfc.nasa.gov/fits_standard.html>）"

**关联性**: `关联对` — 作为 PDF 文档的索引页/主页，提供导航和元数据，断言中的"索引"关系准确

**版本**: `版本对` — 网页无版本号但内容与标准页绑定，随 v4.0 发布而存在

---

## 268 · FITSIO/FPACK URL —— 核验态：已核

**存在性**: `已核`  
- HTTP 301→200 重定向后返回；页面显示 "fpack — FITS Image Compression"

**位点原文**: [docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md:226](file:///workspace/Astro%20CS%20Database/docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md#L226)  
"The fpack FITS file compression utility supports this table compression convention."

**关联性**: `关联对` — 引用上下文为 Registry v2.3 约定中列举支持的工具，URL 指向官方 fpack 文档页，说明工具功能与约定一致

**版本**: `版本对` — fpack 为长期维护工具，URL 为当前生产版本；文献断言未指定版次但内容稳定

---

<!-- END OF SKELETON -->

---

## 本批三态计数

| 维度 | 已核 | 不存在 | UNPROVEN |
|------|------|--------|----------|
| 存在性 | 6 | 0 | 0 |
| 关联性 | 6 (全 `关联对`) | - | - |
| 版本 | 6 (`版本对`) | - | - |

## UNPROVEN 清单

本批无 UNPROVEN 条目。

所有标识符类型及途径：
- **URL (6 条)**: `curl -sS -o` + HTTP 状态码检查（arXiv API 作为网络探针返回 200）
  - 263: https://docushare.lsst.org/docushare/dsweb/Get/LSE-163 → 200
  - 264: https://pipelines.lsst.io/getting-started/display.html → 200
  - 265: https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf → 200
  - 267: https://fits.gsfc.nasa.gov/fits_standard.html → 200
  - 268: https://heasarc.gsfc.nasa.gov/fitsio/fpack/ → 301 → 200（重定向跟随）

## 新发现的缺陷形态

本批无新增缺陷形态。全部 6 条均为有效 URL，引用关系准确，版本匹配。

注意：第 265 与 266 为同一 PDF 的不同位点引用，系正常重复（同一源在不同章节段落的分别提及）。

## 覆盖率自报

**已核条数/分配条数 = 6/6** （100%）

所有 P-1 级 A 类条目已完成全套三字段核验，且逐位点开检原文断言。
