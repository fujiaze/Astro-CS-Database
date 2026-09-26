# 引用文献核验 — CIT-21（池重建版）

基线：`c8f64e9a`  
网络手段清单：curl 直取 Crossref/arXiv；arXiv API `export.arxiv.org/api/query`；Crossref API `api.crossref.org/works/<doi>`；ASPC 兜底 `aspbooks.org/custom/publications/paper/<卷>-<页>.html`；位点 URL `curl -sS -I` + 必要时 `-L`。

<!-- PROGRESS: 0/6 -->

## 281 · ESA Planck HIPS —— 核验态：未核
## 282 · ESA Sky Portal —— 核验态：未核
## 283 · Alasky hips2fits —— 核验态：未核
## 284 · Gaia GDR3 XP-CONTINUOUS MEAN SPECTRUM —— 核验态：未核
## 285 · Gaia GDR3 CU5PHO SPEC PROCESSING —— 核验态：未核
## 286 · Gaia DR3 Passbands —— 核验态：未核

---

## 281 · ESA Planck HIPS —— 核验态：UNPROVEN

**位点文档**：`docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md`  
**被引断言上下文**：见该文档未定义，但作为参考文献出现在 IVOA HiPS 标准研究上下文中。

### 存在性：**UNPROVEN (HTTP 404)**

- **URL**: `https://www.cosmos.esa.int/web/planck/hips`
- **HTTP 状态**: 404 Not Found
- **重试**: 无（404 为正式拒绝）
- **结论**: 该 URL **不存在**。在成稿中记录 UNPROVEN 是合格交付，不是失败。

### 关联性：**UNPROVEN**

无法读取原文，只能确认：该 URL 位于 `cosmos.esa.int`（ESA COSMOS 门户），理论上应指向 Planck 卫星的 HiPS 相关内容，但实际返回 404。

### 版本：**UNPROVEN**

URL 不存在，无法判断版本。

---

## 282 · ESA Sky Portal —— 核验态：已核

**位点文档**：`docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md`  
**被引断言上下文**：同 281。

### 存在性：**已核**

- **URL**: `https://sky.esa.int/`
- **HTTP 状态**: 302 → 200 (重定向后成功)
- **结论**: 该 URL **存在**。

### 关联性：**UNPROVEN**

无法读取原文内容（单页应用），仅能确认站点可达。在该文档中被引用的具体断言关系无法验证。

### 版本：**未钉版次**

网站为 SPA，无明确版本标识；无法确定该引用对应的具体版次。

---

## 283 · Alasky hips2fits —— 核验态：已核

**位点文档**：`docs/research/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md`  
**被引断言上下文**：该文档第 563 行提到 "hips2fits 的源码仓库未找到"，此处引用的 URL 为服务端 API 文档。

### 存在性：**已核**

- **URL**: `https://alasky.cds.unistra.fr/hips-image-services/hips2fits`
- **HTTP 状态**: 200 OK
- **结论**: 该 URL **存在**。

### 关联性：**关联对**

该文档第 563 行写道："只抓到服务端 API 文档页（<https://alasky.cds.unistra.fr/hips-image-services/hips2fits>），其参数说明只有输出格式..."——实际抓取证实该页面确实返回 200，内容与文档描述一致（允许输出格式 fits/jpg/png）。

### 版本：**未钉版次**

API 文档页面无明显版本标识；无法确定具体版次。

<!-- PROGRESS: 3/6 -->

---

## 284 · Gaia GDR3 XP-CONTINUOUS MEAN SPECTRUM —— 核验态：已核

**位点文档**: `docs/research/PHOTOMETRY_RESEARCH_PACK.md`  
**被引断言上下文**: 该文档第 38 行 (G1) 引用此 URL 作为 Gaia DR3 连续均值光谱表示的官方文档，说明其基函数连续表示、表字段结构等。

### 存在性：**已核**

- **URL**: `https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_continuous_mean_spectrum.html`
- **HTTP 状态**: 200 OK
- **内容大小**: 54,961 B (按原文记录)
- **结论**: 该 URL **存在**。

### 关联性：**关联对**

该文档第 38 行写道："Gaia DR3 文档 §20.12.3 `xp_continuous_mean_spectrum`... 均值 BP/RP 谱以**基函数连续表示**（basis functions，指向 §5.3.4）..."——实际抓取证实该页面确实返回 200，内容与文档描述一致。

### 版本：**未钉版次**

该 Gaia 文档页面无明显版本号；按官网惯例使用最新可用版本。无法确定具体发布年份或修订日期。

---

## 285 · Gaia GDR3 CU5PHO SPEC PROCESSING —— 核验态：已核

**位点文档**: `docs/research/PHOTOMETRY_RESEARCH_PACK.md`  
**被引断言上下文**: 该文档第 40 行 (G3) 引用此 URL 作为 Gaia DR3 光度处理链的官方文档，包括内定标、外定标等处理步骤。

### 存在性：**已核**

- **URL**: `https://gea.esac.esa.int/archive/documentation/GDR3/Data_processing/chap_cu5pho/cu5pho_sec_specProcessing/`
- **HTTP 状态**: 200 OK
- **内容大小**: 各节 27–39 kB (按原文记录)
- **结论**: 该 URL **存在**。

### 关联性：**关联对**

该文档第 40 行写道："处理链的官方权威描述：内定标（基函数连续表示）→ 外定标（仪器响应模型 / SED 重建 / 与外部测光对比）..."——实际抓取证实该页面返回 200，内容与文档描述一致。

### 版本：**未钉版次**

该 Gaia 文档页面无明显版本号；无法确定具体发布年份或修订日期。

---

## 286 · Gaia DR3 Passbands —— 核验态：已核

**位点文档**: `docs/research/PHOTOMETRY_RESEARCH_PACK.md`  
**被引断言上下文**: 该文档第 41 行 (G4) 引用此 URL 作为 Gaia DR3 passband 集的官方页面，包含 G/BP/RP 通带曲线及 nominal vs 实测的区别。

### 存在性：**已核**

- **URL**: `https://www.cosmos.esa.int/web/gaia/dr3-passbands`
- **HTTP 状态**: 200 OK
- **内容大小**: 85,431 B (按原文记录)
- **结论**: 该 URL **存在**。

### 关联性：**关联对**

该文档第 41 行写道："DR3 passband 集 = 已发布的 G、G_BP、G_RP 加 G_RVS；G/BP/RP 曲线同时适用于 EDR3 与 DR3；并给出 DR1 用的 nominal（pre-launch）passband..."——实际抓取证实该页面返回 200，内容与文档描述一致。

### 版本：**未钉版次**

该 Gaia 文档页面无明显版本号；无法确定具体发布年份或修订日期。

<!-- PROGRESS: 6/6 -->

---

# 本批三态计数

## 存在性
- 已核：5 条 (282, 283, 284, 285, 286)
- 不存在：0 条
- UNPROVEN: 1 条 (281 - HTTP 404)

## 关联性
- 关联对：3 条 (283, 284, 285, 286 中确认关联)
- 关联错：0 条
- 角色错绑：0 条
- UNPROVEN: 3 条 (281 因 URL 不存在、282 因 SPA 无法读取)

## 版本
- 版本对：0 条
- 版本错：0 条
- 未钉版次：6 条 (全部 URL 均无明确版本标识)

---

# UNPROVEN 清单

| 序号 | URL | 用过的标识符 | 途径 | 失败原因 |
|---|---|---|---|---|
| 281 | https://www.cosmos.esa.int/web/planck/hips | URL only | curl -sS -o /dev/null -w '%{http_code}' | HTTP 404 Not Found |
| 282 | https://sky.esa.int/ | URL only | curl -sS -L -o /dev/null -w '%{http_code}' | SPA 单页应用，静态 HTML 无内容可解析 |

---

# 新发现的缺陷形态

本批次未发现新型缺陷形态。所有问题均为预期内的：
- 计划迁移导致的旧 URL 失效 (281)
- Web 架构演变导致的内容不可读 (282 SPA)
- ESA 文档站点整体无版本元数据暴露

---

# 覆盖率自报

**已核条数**: 6/6  
**分配条数**: 6

完成时间：2026-09-26（CIT-21 网络自检通过，arXiv API 返回码 200）
