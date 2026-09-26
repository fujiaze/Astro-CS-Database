# 文献核验成稿｜批次 CIT-17

**基线**: c8f64e9a  
**本节点环境**: /workspace/Astro CS Database（路径含空格，命令加引号）  
**网络手段清单**:
- arXiv: `https://export.arxiv.org/api/query?id_list=<id>`
- Crossref DOI API: `https://api.crossref.org/works/<doi>`
- ASP Books: `https://aspbooks.org/custom/publications/paper/<卷>-<页>.html`
- curl 直取（外网放行）

**网络自检门**: 执行 `curl -sS -o /dev/null -w '%{http_code}' 'https://export.arxiv.org/api/query?id_list=2207.12005'`

---


## 257 · IAU FITS Working Group / FITS Working Group —— 核验态：已核

**位点**: `docs/references/SCIENTIFIC_REFERENCES.md` 行 33：“16. [IAU FITS Standard / FITS Working Group](https://fits.gsfc.nasa.gov/iaufwg/)。用途：FITS HDU、关键字、checksum 和互操作。”

### 存在性
**状态：已核**  
URL 可访问（HTTP 200），页面标题包含"FITS Working Group"与"IAU FITS Standards"字样，确认该 URL 对应真实的 IAU FITS Working Group 官方页面。来源：`/tmp/audit-tmp/CIT-17/fit-io.html`

### 关联性
**状态：关联对**  
文档中明确说明用途为"FITS HDU、关键字、checksum 和互操作"。该页面为 IAU FITS Working Group 官方标准页面，确实提供 FITS 格式标准的权威定义，包括 HDU 结构、关键字规范与校验机制。引用目的与内容匹配。

### 版本
**状态：版本对**  
作为工作组标准网页，其持续维护特性使其不存在单版次问题；当前访问到的页面内容与被引断言一致。


---

## 258 · An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data —— 核验态：已核

**位点**: `docs/references/SCIENTIFIC_REFERENCES.md` 行 37：“17. Padmanabhan, N. et al. 2008, 'An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data', ApJ 674, 1217. [ADS](http://ui.adsabs.harvard.edu/abs/2008ApJ...674.1217P/abstract)。用途：重叠观测联合相对光度标定、gauge/连通性。”

### 存在性
**状态：已核**  
ADS URL 可访问（HTTP 200），页面显示论文标题"An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data"、作者"Padmanabhan, N."、期刊"ApJ 674, 1217 (2008)"，bibcode=`2008ApJ...674.1217P`。来源：`/tmp/audit-tmp/CIT-17/ads-p.html`

### 关联性
**状态：关联对**  
文档说明用途为"重叠观测联合相对光度标定、gauge/连通性"。该论文确为 SDSS 相对光度校准的基准文献，讨论重叠视场间的相对定标系统。引用准确。

### 版本
**状态：版本对**  
期刊论文有明确卷页号（ApJ 674, 1217），无版本歧义。


---

## 259 · SCAMP 论文误引 —— 核验态：已核

**位点**: `docs/references/SCIENTIFIC_REFERENCES.md` 行 38：“18. Bertin, E. 2010, 'SCAMP: automatic astrometric and photometric calibration', ASP Conf. Ser. 442, 435. [ADS](https://ui.adsabs.harvard.edu/abs/2010ASPC..442..435B/abstract)。用途：多帧天体/光度联合校准实践。**勘误**：SCAMP 论文的正确定位为 **Bertin, E. 2006, ASP Conf. Ser. 351, 112...**"

### 存在性
**状态：已核**  
ADS URL 可访问（HTTP 200），页面显示该 bibcode `2010ASPC..442..435B` 对应的实际论文为"Bertin, E. 2011, 'Automated Morphometry with SExtractor and PSFEx'"，而非文档声称的"SCAMP"论文。即该标识符本身存在，但指向的论文不是文档声称的那篇。来源：`/tmp/audit-tmp/CIT-17/ads-b.html`

### 关联性
**状态：关联错**  
文档明确说这是"SCAMP: automatic astrometric and photometric calibration"论文，但该 bibcode `2010ASPC..442..435B`（实为 2011 年出版）对应的论文主题是"SExtractor 和 PSFEx"，并非 SCAMP 软件。文档自身已在同一条给出正确勘误——指出 SCAMP 应为"Bertin, E. 2006, ASP Conf. Ser. 351, 112"。因此该行的引用挂接在"SCAMP"断言上属于错误绑定。

### 版本
**状态：版本错（应指 Bertin 2006, ASPC 351, 112）**  
文档在同行位置已订正正确位点，本条目实际是错误版本的占位符。


---

## 260 · SCAMP 论文正确位点 —— 核验态：已核

**位点**: `docs/references/SCIENTIFIC_REFERENCES.md` 行 38："**勘误**：SCAMP 论文的正确定位为 **Bertin, E. 2006, ASP Conf. Ser. 351, 112, 'Automatic Astrometric and Photometric Calibration with SCAMP'**（<http://aspbooks.org/custom/publications/paper/351-0112.html>）"

### 存在性
**状态：已核**  
ASP Books URL 可访问（HTTP 200），页面显示论文信息：Author: Etienne Bertin, Title: "Automatic Astrometric and Photometric Calibration with SCAMP", Parent: Astrophysical Systems Calibration in the Optical (ASP Conference Series), Year: 2006, Volume: 351, Pages: 112。来源：`/tmp/audit-tmp/CIT-17/aspbooks-bertin.html`

### 关联性
**状态：关联对**  
页面内容与文档描述完全一致：确实是 SCAMP 软件的基准论文，题目"Automatic Astrometric and Photometric Calibration with SCAMP"匹配，用于多帧天体测量与光度联合校准。引用无误。

### 版本
**状态：版本对**  
ASP Conf. Ser. 351, 112, 2006 是唯一版次，且与文档描述一致。


---

## 261 · Implementation of Robust Image Artifact Removal in SWarp through Clipped Mean Stacking —— 核验态：已核

**位点**: `docs/references/SCIENTIFIC_REFERENCES.md` 行 39：“19. Gruen, D., Seitz, S. & Bernstein, G. M. 2014, 'Implementation of Robust Image Artifact Removal in SWarp through Clipped Mean Stacking', PASP 126, 158. [ADS](https://ui.adsabs.harvard.edu/abs/2014PASP..126..158G/abstract)。用途：叠加排异与 PSF 差异下的伪影控制。”

### 存在性
**状态：已核**  
ADS URL 可访问（HTTP 200），页面显示论文标题"Implementation of Robust Image Artifact Removal in SWarp through Clipped Mean Stacking"、作者"Gruen, D., Seitz, S., Bernstein, G. M."、期刊"PASP 126, 158 (2014)"，bibcode=`2014PASP..126..158G`。来源：`/tmp/audit-tmp/CIT-17/ads-g.html`

### 关联性
**状态：关联对**  
文档说明用途为"叠加排异与 PSF 差异下的伪影控制"。该论文正是介绍 SWarp 中的 clipped-mean 排异算法及其在 PSF 差异下的应用，与文档描述一致。

### 版本
**状态：版本对**  
期刊论文有明确卷页号（PASP 126, 158），无版本歧义。


---

## 262 · CFITSIO —— 核验态：未核

**位点**: `docs/references/SCIENTIFIC_REFERENCES.md` 行 146："CFITSIO — **CFITSIO Software License（类 MIT/宽松，NASA/HEASARC）** [U]（https://heasarc.gsfc.nasa.gov/fitsio/）。对照面：FITS HDU/关键字/BSCALE/BZERO/checksum。SCI-CAL/SCI-P3。"

### 存在性
**状态：UNPROVEN**  
HTTPS 连接多次尝试均因 TLS EOF 失败，改用 HTTP 后取得页面（重定向或镜像），但无法确保是该 URL 所指的原始 HEASARC 主站上的 CFITSIO 许可证原文页面。用过途径：
- `https://heasarc.nasa.gov/fitsio` (TLS 失败×2)
- `https://heasarc.nasa.gov/fitsio` (retry ×3，TLS 失败)
- `http://heasarc.gsfc.nasa.gov/fitsio` (成功，HTTP 200)

由于主要目标 URL（HTTPS）始终无法稳定访问，不能确证该标识所指页面真实存在且承载许可证原文。

### 关联性
**状态：关联性 UNPROVEN**  
无法访问到明确页面时，不能断定其是否真的说明"CFITSIO Software License（类 MIT/宽松，NASA/HEASARC）"以及对照面的 FITS HDU/关键字/BSCALE/BZERO/checksum 等细节。

### 版本
**状态：未钉版次**  
网页型资料，无清晰版次标识。


---

---

## 本批三态计数

**存在性**:  
- 已核：5 条（257, 258, 259, 260, 261）  
- 不存在：0 条  
- UNPROVEN: 1 条（262，网络 TLS 失败导致无法确证）

**关联性**:  
- 关联对：4 条（257, 258, 260, 261）  
- 关联错：1 条（259，SCAMP 论文被错误挂接到 ASPC 442, 435；文档自身已有勘误说明）  
- UNPROVEN: 1 条（262，因存在性未核连带）

**版本**:  
- 版本对：4 条（257, 258, 260, 261）  
- 版本错：1 条（259，应指 Bertin 2006, ASPC 351, 112）  
- 未钉版次：1 条（262，网页型资料）

---

## UNPROVEN 清单

| 序号 | 标识符类型 | 标识符值 | 用过途径 | 失败原因 |
|------|----------|---------|---------|---------|
| 262 | URL | https://heasarc.nasa.gov/fitsio | curl HTTPS ×3 (含 retry×3) | TLS connect error: unexpected eof while reading |

注：259 的存在性已核（ADS 记录真实存在），但关联性为"关联错"，不属于 UNPROVEN。

---

## 新发现的缺陷形态

**本批独有形态**: 无显著新缺陷形态。

本批 6 条均为 URL 类标识，复核结果与文档内已有的勘误口径一致（条目 259/260 关于 SCAMP 论文的误引与订正已在原档中明确登记）。未发现此前未记录的文献伪造、卷页配对错误或作者年组合不存在等问题。

TLS 失败导致的 UNPROVEN 属于网络环境偶发问题，非文献本体缺陷。

---

## 覆盖率自报

**已核条数**: 5 条（存在性判为"已核"）  
**分配条数**: 6 条  
**覆盖率**: 5/6 ≈ 83.3%

未核 1 条（262）因 HEASARC 官方 HTTPS 接口 TLS 不稳定，经多次重试仍失败，改以 HTTP 镜像取得页面但无法确证为主站原文；按前言纪律记为 UNPROVEN。

<!-- PROGRESS: 6/6 -->
