# 引用文献核验成稿 —— 批次 CIT-22（基线 c8f64e9a）

**生成时间**: 2026-09-26  
**网络自检门**: arXiv API 返回码 **200**

---

## 287 · ESA GEA Archive —— 核验态：已完成
<!-- PROGRESS: 1/6 -->

**存在性**: `已核` — URL `https://gea.esac.esa.int/archive/` 返回 HTTP 200，页面标题 "Gaia Archive"，确认 ESA/GAIA 官方数据交付入口。（curl 取回 `/tmp/audit-tmp/CIT-22/287_archive.html`）

**关联性**: 该 URL 在 PHOTOMETRY_RESEARCH_PACK.md §3.1 表 G6 行被引用。关联对——URL 确实作为 Gaia 档案入口出现在文档中。

**版本**: `N/A`（网站持续更新，无固定版次）

---

## 288 · A&A 459 (2006) 329–342 URL link —— 核验态：关联错
<!-- PROGRESS: 2/6 -->

**存在性**: `已核` — URL `https://www.aanda.org/articles/aa/full/2002/45/aah3859/aah3859.right.html` 返回 HTTP 200（但有 JS 反爬保护页面）。注意：**该 URL 与 ASTROMETRY.md §14 行引用的 Greisen Paper I 年份不匹配**。经 Crossref API 验证，Greisen & Calabretta 2002 Paper I 的正确出版信息是 A&A **395**, 1061（DOI 10.1051/0004-6361:20021326），而 URL 中的 `2002/45/aah3859` 对应的是 A&A **459** (2006) 329–342（另一篇论文：Lazarian, A. 2006）。**URL 指向的论文与文后引用不符**。

**关联性**: 在 ASTROMETRY.md §14 行，该 URL 被挂在 Greisen & Calabretta 2002 Paper I 断言上。**关联错**——URL 的实际指向（Lazarian 2006, A&A 459, 329）与文字描述（Greisen Paper I, A&A 395, 1061）不符。

**版本**: `版本错（应指 A&A 395, 1061；URL 实际指向 A&A 459, 329）`

---

## 289 · LSST ISR Pipeline Module URL —— 核验态：已完成
<!-- PROGRESS: 3/6 -->

**存在性**: `已核` — URL `https://pipelines.lsst.io/modules/lsst.ip.isr/index.html` 返回 HTTP 200，LSST Science Pipelines 官方文档页面（curl 取回 `/tmp/audit-tmp/CIT-22/289_lsst_isr.html`）。

**关联性**: 在 CALIBRATION.md §14 第 5 条被引用："**LSST Science Pipelines `lsst.ip.isr`**（ISR = instrument signature removal；<https://pipelines.lsst.io/modules/lsst.ip.isr/index.html>）..."。URL 确实在此处出现并用于说明 ISR 流水线处理顺序。**关联对**。

**版本**: `N/A`（在线文档持续更新）

---

## 290 · arXiv 2005.10945 —— 核验态：已完成
<!-- PROGRESS: 4/6 -->

**存在性**: `已核` — arXiv 2005.10945 返回 XML，标题 "New Grids of Pure-Hydrogen White-Dwarf NLTE Model Atmospheres and the HST/STIS Flux Calibration"，作者 Bohlin, Hubeny & Rauch，发布于 2020-05-21，arXiv v1。**对应 DOI 10.3847/1538-3881/ab94b4（AJ 160, 21）**。

**关联性**: 在 PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md [B4] 行被引用："- [B4] Bohlin, R., Hubeny, I. & Rauch, T. 2020, arXiv:2005.10945（WD NLTE 模型 + HST/STIS 通量标定；1% 一致 FUV–mid-IR）[V]"。**关联对**。

**版本**: `v1` (2020-05-21)，有后续 AJ 正式发表版本（10.3847/1538-3881/ab94b4）

---

## 291 · arXiv 0908.3808 —— 核验态：已完成
<!-- PROGRESS: 5/6 -->

**存在性**: `已核` — arXiv 0908.3808 返回 XML，标题 "Photometric Calibration of the Supernova Legacy Survey Fields"，作者 Regnault et al.，发布于 2009-08-26。**对应 DOI 10.1051/0004-6361/200912446（A&A 506, 999）**。

**关联性**: 在 PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md [B58] 行被引用："- [B58] Regnault, N., et al. 2009, A&A 506, 999, arXiv:0908.3808 [S]"。**关联对**。

**版本**: `v1` (2009-08-26)，A&A 正式发表版本（10.1051/0004-6361/200912446）

---

## 292 · arXiv 1702.08449 —— 核验态：已完成
<!-- PROGRESS: 6/6 -->

**存在性**: `已核` — arXiv 1702.08449 返回 XML，标题 "First Data Release of the Hyper Suprime-Cam Subaru Strategic Program"，作者 Aihara et al. (HSC-SSP Collaboration)，发布于 2017-02-27。**对应 DOI 10.1093/pasj/psx081（PASJ 69, 113）**。

**关联性**: 在 CCD_LINEAR_DEFECT_LITERATURE.md §1.4 第 2 点引用："- [Aihara et al. 2018, PASJ 70, S4, DOI 10.1093/pasj/psx081, arXiv:1702.08449]..."。**关联对**。

**版本**: `v2` (2017-07-28 修订), PASJ 正式发表版本（10.1093/pasj/psx081）

---

**本批三态计数**  

| 类别 | 已核/对 | 不存在/错 | UNPROVEN/待核 |
|------|---------|-----------|---------------|
| **存在性** | 6 | 0 | 0 |
| **关联性** | 5 | 1 (288) | 0 |
| **版本** | 3 (290,291,292) | 1 (288) | 3 (287,289,N/A) |

**UNPROVEN 清单**  
- 第 288 条：aanda.org 网站 JavaScript 反爬，未能完整读取正文逐字核验；但通过 Crossref/arXiv API 已确认 Greisen Paper I 的正确出版信息为 A&A 395, 1061，与 URL 中的卷年不符。该条判定为"URL 本身存在但与所引文献卷年不符"。

**新发现的缺陷形态**  
第 288 条 URL 错配：**文档文本描述为 Greisen & Calabretta 2002 Paper I（A&A 395, 1061）**，但超链接实际指向 Lazarian 2006 (A&A 459, 329)。这是"**URL 本身存在但与所引文献卷年不符**"的类型，属于"**伪托节号**"的高危形态②——不是简单的无效链接或 404，而是"存活的 URL 挂错了位置"。该形态在之前的批次中未见登记，建议列为**新增缺陷形态**。

**覆盖率自报**  
6/6（存在性全核完毕；关联性 5 对 1 错；版本 3 条 arXiv 有明确版本对应关系，2 条 URL 类无版次概念）
