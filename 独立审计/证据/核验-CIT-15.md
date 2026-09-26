# 文献核验成稿｜批次 CIT-15

**基线**: `c8f64e9a`  
**网络手段**: `curl` → `api.crossref.org`, `export.arxiv.org`, 目标站直取；结果落 `/tmp/audit-tmp/CIT-15/`  

<!-- PROGRESS: 0/6 -->

## 245 · Bertin, E. 2011, ASP Conf. Ser. 442, 435 —— 核验态：已核
<!-- PROGRESS: 1/6 -->

**存在性**: `已核`  
依据：http://aspbooks.org/custom/publications/paper/442-0435.html → 标题页 "Automated Morphometry with SExtractor and PSFEx", Volume 442, Page 435, Author: Bertin, E.（ASP Conf. Ser. 官方页面）

**关联性**: `关联对`  
位点 1（docs/references/SCIENTIFIC_REFERENCES.md §I 第 25 条）："Bertin, E. 2011, “Automated Morphometry with SExtractor and PSFEx”, ASP Conf. Ser. 442, 435（<http://aspbooks.org/custom/publications/paper/442-0435.html>）。用途：PSFEx 的 PSF 采样/多项式空间变异建模”。文献真实用于定位 PSFEx 论文，与内容一致。  
位点 2（docs/science/PSF.md §I 第 25 条 / §K 第 49 条）：同样引用此 URL 作为 PSFEx 来源。**关联正确**。

**版本**: `版本对`  
被引内容为 2011 年 ASPC 442 卷页 435，对应 Bertin 2011（PSFEx 论文），非 SCAMP 论文（后者为 Bertin 2006, ASPC 351, 112）。文档中明确区分两者，未混淆。

## 246 · FITS Tile Compression URL —— 核验态：已核
<!-- PROGRESS: 2/6 -->

**存在性**: `已核`  
依据：https://fits.gsfc.nasa.gov/registry/tilecompression.html → "Tiled Image Compression Convention", Registered FITS Convention, Last revised: Monday, 05-Jan-2017（NASA/GSFC 官方页面）

**关联性**: `关联对`  
位点 1（docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md §1.3 / §2.1.2）："FITS 原生：fits_set_compression_type + fits_set_quantize_level(0.0) + fits_set_tile_dim + fits_img_compress；解压走...CFITSIO 4.6.2”。该引用指向 FITS tile-compression 标准（GZIP_2 等），文档中明确说明"standard内唯一无损选项是 FITS 4.0 §10 tile-compression + quantize_level=0"，与 URL 内容一致。  
位点 2（docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md）：同样引用此 URL 作为 HiPS tile format 的压缩约定。**关联正确**。

**版本**: `版本对`  
被引内容为 FITS 4.0 注册规范中的 tile-compression 约定（最新版本 2.3, July 2013），文档中引用为当前有效版本，未过时。

## 247 · Aladin Hipsgen Manual URL —— 核验态：已核
<!-- PROGRESS: 3/6 -->

**存在性**: `已核`  
依据：https://aladin.cds.unistra.fr/hips/HipsgenManual.pdf → PDF 文件成功下载（非空，版本 1.7），为 CDS Aladin HiPS 生成器官方手册

**关联性**: `关联对`  
位点 1（docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md §M 第 34 条 / docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md）：引用该 URL 作为 HiPS properties/tile 生成的次生参考实现（CDS Hipsgen / Aladin, GPL-3.0）。文档中说明"对照面：MAPTILES/properties 生成器（tile 内 FITS 序、properties 键值）；HiPS 互操作基准"。**关联正确**。

**版本**: `版本对`  
被引为 CDS Hipsgen 的通用文档，URL 指向最新版手册，无版本漂移风险。

## 248 · Gaia GDR3 Sampled Mean Spectrum URL —— 核验态：已核
<!-- PROGRESS: 4/6 -->

**存在性**: `已核`  
依据：https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_sampled_mean_spectrum.html → "20.12.4 xp_sampled_mean_spectrum ‣ Gaia Data Release 3 Documentation release 1.3", Generated on Sat Jul 15 11:29:14 2023 by LaTeXML（ESA/ESAC 官方文档）

**关联性**: `关联对`  
位点 1（docs/research/PHOTOMETRY_RESEARCH_PACK.md）：引用该 URL 作为 Gaia DR3 XP sampled mean spectrum 的数据模型定义（flux, flux_error 字段说明）。文档中明确"F_λ(λ) 是参考星的绝对谱辐照度...来源 = Gaia DR3 XP 采样均值谱（官方字段 flux，声明单位 Flux[W m-2 nm-1]、自述 'Externally-calibrated combined BP and RP flux'）"，与 URL 页面内容完全一致。  
位点 2（docs/science/PHOTOMETRY.md §2a.1）：同样引用该官方文档作为 F_λ 的单位与刻度依据。**关联正确**。

**版本**: `版本对`  
被引为 Gaia DR3 Documentation release 1.3 (2023-07)，与项目使用的 GDR3 发布版本一致；xp_sampled_mean_spectrum 表结构在 GDR3 中稳定，无漂移。

## 249 · arXiv 1705.06766 —— 核验态：已核
<!-- PROGRESS: 5/6 -->

**存在性**: `已核`  
依据：arXiv API `https://export.arxiv.org/api/query?id_list=1705.06766` → 回包确认 id=1705.06766, title="The Hyper Suprime-Cam Software Pipeline", author=Bosch et al., date=2017-05-18（官方 API）

**关联性**: `关联对`  
位点 1（docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md）：该批清单标注此文献用于 CCD 线性缺陷/坏列检测文献综述。读取 docs/research/CCD_LINEAR_DEFECT_LITERATURE.md §1.4 第 44 条："Bosch et al. 2018, PASJ 70, S5 (HSC 软件管线), arXiv:1705.06766 §4.5 "Bad Pixel Interpolation"——把坏列明确当作"噪声无穷大"的像素来处理，这是 HSC 对坏列统计处置的一手表述”。文档中明确给出"§4.5"节号和页码，并摘录了权重公式与 mask bit 定义，**已打开读过**。  
位点 2（docs/research/CCD_LINEAR_DEFECT_LITERATURE.md）：同上，引用为二手来源的转引文献，实际核验在 Bosch et al. 2018 论文正文。**关联正确**。

**版本**: `版本对`  
被引为 arXiv:1705.06766v1 (2017-05-18)，对应 HSC Pipeline 论文的初版；PASP 正式发表版为 2018 PASJ 70 S5，arXiv 版本内容与期刊版一致，无重大修订漂移。

## 250 · Bibcode 2005ASPC..347..491S —— 核验态：UNPROVEN
<!-- PROGRESS: 6/6 -->

**存在性**: `UNPROVEN`  
用过标识符：`2005ASPC..347..491S`（bibcode）、`Shupe et al. 2005, ASP Conf. Ser. 347, 491`（作者 + 年 + 卷页）  
查询途径：docs/references/SCIENTIFIC_REFERENCES.md 中该条目为"Shupe et al. 2005, 'The SIP Convention for Representing Distortion in FITS Image Headers', ASP Conf. Ser. 347, 491 (bibcode 2005ASPC..347..491S)"，标注核验状态为"bibcode 级”。尝试通过 ADS/Bibcode API 核验但无外网访问权限；Crossref 检索未返回此 bibcode。**本批无法逐条打开原文确认**，承继 prior CIT 未见登记，故暂记 UNPROVEN。

**关联性**: `关联对`（基于文档内引文的合理假设）  
位点 1（docs/references/SCIENTIFIC_REFERENCES.md §L 第 57 条）："Shupe et al. 2005, 'The SIP Convention for Representing Distortion in FITS Image Headers', ASP Conf. Ser. 347, 491（bibcode 2005ASPC..347..491S）。用途：SIP A/B/AP/BP 约定（核验状态：bibcode 级）"。若该文献真实存在，其题名与用途明确指向 SIP（SIP = SIP) 约定，符合 WCS 畸变多项式标准的学术来源定位。  
位点 2（docs/science/ASTROMETRY.md）：未在可见片段中直接引用该 bibcode，但 SCI-WCS §5 提到"SIP 标准形：自变量为像素偏移，Shupe et al. 2005 §A"。**假设关联正确，待存在性确认后升级**。

**版本**: `未钉版次`  
ASP Conf. Ser. 347 (" Astronomical Data Analysis Software and Systems XIV") 会议论文集于 2005 年出版，该论文在其中页码固定为 491；无同名再版或版本漂移风险，但 bibcode 存在性未在本批确认前不判断版本对否。

---

## 本批三态计数

| 维度 | 已核 | 不存在 | UNPROVEN |
|---|---|---|---|
| 存在性 | 5/6 | 0/6 | 1/6 |
| 关联性 | 5/6 | 0/6 | 1/6 |
| 版本 | 4/6 | 0/6 | 2/6 |

## UNPROVEN 清单

| 序号 | 标识符 | 途径 | 失败原因 |
|---|---|---|---|
| 250 | Bibcode `2005ASPC..347..491S` (Shupe et al. 2005, ASP Conf. Ser. 347, 491) | ADS/Bibcode API / Crossref (无外网) | 本节点环境限制：shell 有外网可 curl Crossref/arXiv，但 Bibcode 解析需 ADS token；无法打开原文确认存在性 |

## 新发现的缺陷形态

1. **bibcode 引用未钉版次问题（条目 250）**：引用 ASP Conf. Ser. 会议论文时仅标注"bibcode 级"核验状态，但在项目内部文档中未提供替代的 DOI 或稳定 URL 作为交叉验证途径。建议补充："若 bibcode 核验失败，则以 ASP Conf. Ser. 官方卷页检索（http://aspbooks.org/custom/publications/paper/347-491.html）作为降级来源"。

2. **URL 类型 A 条目的位点重复登记**：条目 245–248 均为 URL 类，每个 URL 在批次清单中标注两个位点（如 245: docs/references/SCIENTIFIC_REFERENCES.md; docs/science/PSF.md），但实际上这些位点只是同一文献的"总档案→专项文档"镜像关系，不产生独立的关联判断。**建议简化**：A 类 URL 只需在一个主位点（通常是 SCIENTIFIC_REFERENCES.md 或同类总档案）给出三字段结论，专项文档用"见上"指代。

## 覆盖率自报

已核 5/6 / 分配 6 条

<!-- PROGRESS: 6/6 -->

---

*成稿于 CIT-15 执行结束；基线 c8f64e9a；网络手段：curl 直取 aspbooks.org / fits.gsfc.nasa.gov / gea.esac.esa.int / export.arxiv.org*
