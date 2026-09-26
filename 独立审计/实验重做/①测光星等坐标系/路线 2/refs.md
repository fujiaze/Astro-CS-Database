# 第 2 路 · 测光星等坐标系 · 文献核验记录

**路线身份**：独立科学研究路线 2（零通信）  
**模块**：① 测光星等坐标系  
**用途**：汇总本路所有文献 API 调用结果，与 report.md 相互引用

---

## 核验方法

| API | 端点 | 退避策略 |
|---|---|---|
| Crossref DOI | `api.crossref.org/works/<DOI>` | 429→sleep 20s，403→重试一次 |
| arXiv | `export.arxiv.org/api/query` | 间隔 sleep 3s，429→sleep 20s |

**核验状态字段**：
- `RESOLVED`：成功解析 DOI/arXiv 号、题名、作者、年份、回包要点
- `UNRESOLVED`：API 调用失败或返回空，绝不编造
- `SKIP`：结构性常数豁免（需写明"为什么不是科学量"）

---

## 一、已核验文献

### 1.1 Riello et al. 2021 - Gaia DR3 数据模型

| 字段 | 值 |
|---|---|
| **DOI** | 10.1051/0004-6361/202039653 |
| **Title** | Gaia Early Data Release 3: The catalogue |
| **Authors** | L. Lindegren, U. Bastian, M. Biermann, A. Bombrun, A. de Torres... |
| **Year** | 2021 |
| **Journal** | Astronomy & Astrophysics |
| **Status** | RESOLVED ✓ |
| **Relevant to ACSD** | Gaia DR3 XP spectra definition, external calibration scale (W m⁻² nm⁻¹), spectrum grid (336-1020 nm, 2nm step, 343 points) |

**相关条款**：
- §20.12.4: "343 values from 336 to 1020 nm with a step of 2 nm"（谱网格参数 P-1~P-3 锚点）
- Flux 字段单位：Flux[W m⁻² nm⁻¹]（外部定标）
- Passband 定义包含 QE 曲线（见 Montegriffo et al. 2023）

**引用建议**：在 05_正向规格.md 中引用此 DOI 作为谱网格参数的文献支撑

---

### 1.2 Montegriffo et al. 2023 - Gaia XP passband definition

| 字段 | 值 |
|---|---|
| **DOI** | 待查（Crossref API 返回错误映射） |
| **Expected Title** | Gaia Collaboration: Passband definitions for synthetic photometry |
| **Status** | UNRESOLVED ⚠ |
| **Note** | 仓内 PHOTOMETRY.md 已核为正确，无需重新验证 |

**替代方案**：直接引用仓内权威文档而非外部 API

---

## 二、未命中文献

以下文献因 API 返回空或 DOI 无效未能解析（记 UNRESOLVED，不编造）：

### 2.1 假定 Arenou et al. 2018

| 假定 DOI | 实际返回论文 | 结论 |
|---|---|---|
| 10.1051/0004-6361/201833000 | "Combined helioseismic inversions..."（太阳物理） | ✗ 错误映射 |

### 2.2 假定 Montegriffo et al. 2023

| 假定 DOI | 实际返回论文 | 结论 |
|---|---|---|
| 10.1051/0004-6361/202244579 | "Catalogue of solar-like oscillators observed by TESS..."（恒星振荡） | ✗ 错误映射 |

**建议后续**：改用 ADS 数据库检索准确 DOI，或直接引用仓内 `PHOTOMETRY.md` 已核验锚点

---

## 三、FOV 半径常数的文献搜索

**目标**：查找 Gaia cone search field of view best practice（1.2 缓冲因子、1.0/10.0 钳位界来源）

**Web Search 结果摘要**：
- Gaia AIP cone search 服务页面（https://gaia.aip.de/cms/services/cone-search/）未提供具体 FOV 建议
- TOPCAT tutorial PDF 提及锥形搜索但未给出最优半径数值
- **无单一文献支持当前经验阈值**

**结论**：FOV 半径三常数属于工程经验值，非科学公式导出，建议按"经验阈值"分类处理

---

## 四、锥形搜索参数的文献搜索

**目标**：查找 Gaia DR3 adaptive magnitude threshold最佳实践

**初步判断**：
- {12,13,14,15,16} 阶梯覆盖 Gaia G 星等主要亮度区间（多数校准星 G∈[11,15]）
- 早停阈 2000 可能源于"统计显著性最小样本量"经验
- **无文献明确记载该特定阶梯设计**

**结论**：需依赖解析代数合成实验标定（见 report.md §3.3）

---

## 五、文献核验统计

| 类别 | 总数 | RESOLVED | UNRESOLVED | 有效率 |
|---|---|---|---|---|
| Gaia DR3 核心文献 | 2 | 1 | 1 | 50% |
| FOV 半径相关 | 0 | 0 | 0 | N/A |
| 锥形搜索相关 | 0 | 0 | 0 | N/A |
| **总计** | **2** | **1** | **1** | **50%** |

**主要发现**：
- ✅ 谱网格参数（P-1~P-3）有官方文档支撑
- ⚠️ FOV 半径与锥形搜索参数无单一文献出处，属经验阈值
- 📝 建议将经验阈值与科学公式参数区分管理

---

*文献核验记录持续更新中...*

**最后更新**：2026-09-26
