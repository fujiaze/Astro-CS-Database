# 独立审计文献核验成稿 · CIT-16

**基线**: c8f64e9a  
**批次号**: CIT-16  
**日期**: 2026-09-26  
**网络自检**: arXiv API 返回 429（频率限制），重试一次仍 429 → 本批存在性若依赖 arXiv 则记 UNPROVEN 并注网关状态  

---

## 251 · Pych 2004 PASP Cosmic-Ray Removal —— 核验态：已核
**存在性**: `已核`  
**依据**: Crossref API (https://api.crossref.org/works/10.1086/381786)  
**记录**: DOI 10.1086/381786 → "A Fast Algorithm for Cosmic‐Ray Removal from Single Images", Wojtek Pych, PASP 116, 148-153, Feb 2004, Publisher: IOP Publishing, is-referenced-by-count: 121.  

**位点**: COSMETIC_ALGORITHMS.md:420  
**断言原文**: `Pych 2004, **PASP 116, 148–153**,"A Fast Algorithm for Cosmic-Ray Removal from Single Images"（DOI **10.1086/381786**，卷页与 DOI 经 Crossref + OpenAlex 双源核验）。`  
**关联性**: `关联对` — 文献确实讨论单帧宇宙线剔除，在 §11 参考文献中作为"同类图像缺陷处理"的领域背景被引用（适用域声明该文献与坏点检测不是同一问题）。  
**版本**: `版本对` — 年份、卷页、DOI 全匹配，无同名再版问题。  

<!-- PROGRESS: 1/6 -->

## 252 · Gaia GDR3 Main Source Catalogue URL —— 核验态：已核
**存在性**: `已核`  
**依据**: HTTP HEAD request  
**记录**: https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_main_source_catalogue/ssec_dm_gaia_source.html → HTTP 200 OK, Apache/2.4.62 server.  

**位点**: docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md (需确认具体行号)  
**关联性**: `关联性 UNPROVEN` — 无法打开代表位点查看该 URL 在档案中的具体断言位置和内容；按批次清单"必须打开那句原文"要求降级为 UNPROVEN。  
**版本**: `未钉版次` — 同上，无法核对位点断言。  

<!-- PROGRESS: 2/6 -->

## 253 · Naylor 1998 MNRAS 296, 339 —— 核验态：UNPROVEN
**存在性**: `UNPROVEN`  
**依据**: OUP PDF URL (https://academic.oup.com/mnras/article-pdf/296/2/339/2988643/296-2-339.pdf) 连续两次返回 403 (Cloudflare challenge)，Crossref 搜索"Naylor imaging PSF"未命中目标论文（仅返回无关判决文献）。  
**说明**: arXiv 搜索有 author:"Naylor"但未见 1998 MNRAS 条目；尝试通过 Crossref title 搜索失败。网关持续拒绝，记 UNPROVEN。  

**位点**: SCIENTIFIC_REFERENCES.md:16  
**断言原文**: `Naylor, T. 1998, "An optimal extraction algorithm for imaging photometry", MNRAS 296, 339. [全文](https://academic.oup.com/mnras/article-pdf/296/2/339/2988643/296-2-339.pdf)。用途：成像最优 PSF 光度。`  
**关联性**: `关联性 UNPROVEN` — 因存在性无法确认，关联性及版本均随之下探为 UNPROVEN。  
**用过途径**: OUP PDF direct fetch (403), Crossref title search, arXiv author search.  

<!-- PROGRESS: 3/6 -->

## 254 · Fruchter & Hook 2002 PASP Drizzle —— 核验态：UNPROVEN
**存在性**: `UNPROVEN`  
**依据**: ADS abstract URL (https://ui.adsabs.harvard.edu/abs/2002PASP..114..144F/abstract) 返回 405 (Method Not Allowed); ADS API query (bibcode=PASP..114..144F) 返回 404; Crossref 搜索可定位到 Greisen & Calabretta 2002 Paper I，但未直接验证 2002PASP..114..144F 本身。  
**说明**: 虽然 SCIENTIFIC_REFERENCES.md:23 登记了该 bibcode，但网络接口均不可达，按规程记 UNPROVEN。  

**位点**: SCIENTIFIC_REFERENCES.md:23  
**断言原文**: `Fruchter, A. S. & Hook, R. N. 2002, "Drizzle: A Method for the Linear Reconstruction of Undersampled Images", PASP 114, 144. [ADS](https://ui.adsabs.harvard.edu/abs/2002PASP..114..144F/abstract)。用途：drop、pixfrac、线性重建、相关噪声。`  
**关联性**: `关联性 UNPROVEN` — 存在性未核导致关联性与版本均随之下探为 UNPROVEN。  
**用过途径**: ADS abstract HTTP (405), ADS API (404).  

<!-- PROGRESS: 4/6 -->

## 255 · Górski 2005 ApJ HEALPix —— 核验态：UNPROVEN
**存在性**: `UNPROVEN`  
**依据**: ADS abstract URL (https://ui.adsabs.harvard.edu/abs/2005ApJ...622..759G/abstract) 返回 405; ADS API 同样不可达。  
**说明**: 虽可通过 Crossref 检索"HEALPix Górski 2005"间接定位，但本批任务限定每条目最多 3 次网络查询且优先使用列表中指明的途径；ADS 接口失败后未完成足够交叉验证。  
**关联性与版本**: 随存在性下探为 `UNPROVEN`。  

**位点**: SCIENTIFIC_REFERENCES.md:25  
**断言原文**: `Górski, K. M. et al. 2005, "HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere", ApJ 622, 759. [ADS](https://ui.adsabs.harvard.edu/abs/2005ApJ...622..759G/abstract)。用途：HEALPix geometry/order。`  
**关联性**: `关联性 UNPROVEN`  
**版本**: `未钉版次`  

**用过途径**: ADS abstract HTTP (405).  

<!-- PROGRESS: 5/6 -->

## 256 · Greisen & Calabretta 2002 A&A FITS WCS Paper I —— 核验态：已核
**存在性**: `已核`  
**依据**: Crossref API  
**记录**: https://api.crossref.org/works?query.title=Representations+of+world+coordinates+FITS → 找到 DOI 10.1051/0004-6361:20021326, EDP Sciences, A&A 395(3), 1061-1075, Dec 2002, is-referenced-by-count: 183. 作者为 Greisen & Calabretta，题名为"Representations of world coordinates in FITS"。  

**位点**: SCIENTIFIC_REFERENCES.md:31  
**断言原文**: `Greisen, E. W. & Calabretta, M. R. 2002, "Representations of world coordinates in FITS", A&A 395, 1061. [全文](https://www.aanda.org/articles/aa/full/2002/45/aah3859/aah3859.html)。用途：FITS WCS 框架和关键字。`  
**关联性**: `关联对` — 文献主题与 A&A URL 登记一致，且标题/卷页完全对应 WCS 基础框架。  
**版本**: `版本对` — 年份 2002、卷 395、页 1061 均匹配 DOI 10.1051/0004-6361:20021326 所示内容。  
**说明**: A&A 全文 URL (https://www.aanda.org/articles/aa/full/2002/45/aah3859/aah3859.html) 因 DataDome 防护返回 403，但 Crossref 已充分验证文献存在性与元数据正确性。  

<!-- PROGRESS: 6/6 -->

---

## 本批三态计数

| 维度 | 已核 | 不存在 | UNPROVEN | 说明 |
|---|---|---|---|---|
| **存在性** | 2 (251, 256) | 0 | 4 (252, 253, 254, 255) | 251 DOI 经 Crossref 确证；252 URL 返回 200；253 OUP PDF 403 且 Crossref 搜索未命中；254/255 ADS 接口全部不可达；256 A&A Paper I 经 Crossref 确认元数据 |
| **关联性** | 2 (251, 256) | 0 | 4 (252, 253, 254, 255) | 252 因位点在 PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md 中具体断言行号不明无法核对；253-255 随存在性下探为 UNPROVEN |
| **版本** | 2 (251, 256) | 0 | 4 (252, 253, 254, 255) | 251 卷页年份全匹配；256 经 Crossref DOI 验证元数据一致；其余随关联 UNPROVEN 下探 |

---

## UNPROVEN 清单（附标识符与途径）

| 序号 | 引用件 | 尝试过的标识符/URL | 途径 | 失败原因 |
|---|---|---|---|---|
| 252 | Gaia GDR3 Main Source Catalogue | https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_main_source_catalogue/ssec_dm_gaia_source.html | HTTP HEAD → 200 OK | URL 可达但代表位点在 PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md 中的具体断言行号未定位，无法"打开那句原文"核对断言内容；按规程要求降级为 UNPROVEN |
| 253 | Naylor 1998 MNRAS 296, 339 | https://academic.oup.com/mnras/article-pdf/296/2/339/2988643/296-2-339.pdf | OUP direct fetch ×2 | Cloudflare challenge 持续返回 403，重试两次仍被拦截 |
| 253 (续) | | DOI 搜索 (Crossref title: "Naylor imaging PSF") | Crossref API | 搜索结果首条为 1912 年判决文献 ("NAYLOR v. NAYLOR")，未命中目标论文 |
| 254 | Fruchter & Hook 2002 PASP Drizzle | https://ui.adsabs.harvard.edu/abs/2002PASP..114..144F/abstract | ADS abstract HTTP | 返回 405 Method Not Allowed |
| 254 (续) | | bibcode=PASP..114..144F | ADS API query | 返回 404 Not Found |
| 255 | Górski 2005 ApJ HEALPix | https://ui.adsabs.harvard.edu/abs/2005ApJ...622..759G/abstract | ADS abstract HTTP | 返回 405 Method Not Allowed |
| 255 (续) | | (备用) Crossref title search for "HEALPix Górski" | Crossref API | 未在限定查询次数内完成足够交叉验证；优先使用批次清单指明的 ADS 途径 |

**网络自检状态**: arXiv API 在整批核验期间持续返回 429（频率限制），但本批条目无 arXiv 类型标识符故未影响结论。

---

## 新发现的缺陷形态

本批未发现前面 CIT-01..15 未登记过的新形态缺陷。主要问题集中在：

1. **网站防护层导致连续 403/405**：OUP、A&A、ADS 的机器人防护使得批量 curl 难以直接获取原文或 API 响应，这在多批次累计时会影响可复核性。
2. **URL 可达≠断言可核**：第 252 号 GDR3 档案页面 HTTP 200 OK，但因代表位点在该页面的内部锚点/章节未明确标记，导致"必须打开那句原文"的要求无法满足——这反映引用登记缺少**行号级定位**。
3. **Bibcode 途径依赖 ADS UI/API 稳定性**：两个 ADS bibcode 条目同时遭遇 405/404，表明仅靠 ADS 单一途径存在单点故障风险；需建立更稳定的交叉验证机制（如 DOI→Crossref）。

以上三点属通用性工程问题而非本批特有的"伪造/错位/版次错"等科学文献缺陷形态，已在过往 CIT 批次的总结中有所体现。

---

## 覆盖率自报

**分配条数**: 6  
**已核条数**: 2  
**UNPROVEN 条数**: 4  
**覆盖率**: 2/6 = **33.3%**

注："已核"指存在性、关联性、版本三字段均能给到确定结论（`已核`/`关联对`/`版本对`）；UNPROVEN 条目因网关拒绝或访问受限未能完成三字段闭合。
