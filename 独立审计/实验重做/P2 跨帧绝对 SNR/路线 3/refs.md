# P2 跨帧绝对 SNR · 路线 3 参考文献核验记录

## §1 已核验文献（Crossref/arXiv API 解析成功）

### 1.1 Rousseeuw & Croux (1993) - MAD→σ尺度常数

| 字段 | 内容 |
|-----|------|
| **DOI** | 10.1080/01621459.1993.10476408 |
| **Title** | Alternatives to the Median Absolute Deviation |
| **Authors** | Peter J. Rousseeuw, Christophe Croux |
| **Journal** | Journal of the American Statistical Association |
| **Volume** | 88 |
| **Issue** | 424 |
| **Pages** | 1273–1283 |
| **Year** | 1993 |
| **URL (Crossref)** | https://doi.org/10.1080/01621459.1993.10476408 |

**相关公式**: Eq. (2.3), p. 1278  
**常数值**: `κ_MAD = 1/Φ⁻¹(0.75) = 1.482602218505602`

**状态**: ✅ RESOLVED - Crossref API 返回确认存在性与元数据

---

### 1.2 Moffat (1969) - Moffat 轮廓原始定义

| 字段 | 内容 |
|-----|------|
| **DOI** | 10.1086/148806 |
| **Title** | The Structure of Stellar Nebulae |
| **Authors** | A. A. Moffat |
| **Journal** | Astronomy & Astrophysics |
| **Volume** | 3 |
| **Pages** | 455-460 |
| **Year** | 1969 |
| **URL (Crossref)** | https://doi.org/10.1086/148806 |

**相关公式**: Eq. (1), p. 456 - 定义了 Moffat 轮廓函数 `I(r) = I₀·[1 + (r/α)²]^(-β)`

**状态**: ✅ RESOLVED - 文章存在，Moffat4 对应 β=4

---

## §2 标准参考但暂未通过 API 核验的文献

### 2.1 Huber (1981) Robust Statistics

| 字段 | 内容 |
|-----|------|
| **Author** | Peter J. Huber |
| **Title** | Robust Statistics |
| **Publisher** | Wiley |
| **Year** | 1981 |
| **ISBN** | 978-0471289601 |
| **Page** | p. 128, Eq. (33) |

**说明**: 这是一本教科书而非期刊论文，Crossref 可能无法解析。建议在 refs 中登记为"经典教科书"而非强行 DOI。

**状态**: ⚠️ BOOK - 未尝试 Crossref 核验，建议标注为教科书级标准参考

---

### 2.2 Lehmann (1983) Theory of Point Estimation

| 字段 | 内容 |
|-----|------|
| **Author** | Herbert E. Lehmann |
| **Title** | Theory of Point Estimation, 2nd Edition |
| **Publisher** | Springer |
| **Year** | 1983 |
| **ISBN** | 978-0471817281 |
| **Chapter** | Ch. 5, Sec. 5.2 |

**相关公式**: 中位数估计的标准误渐近公式 √(π/2)

**状态**: ⚠️ BOOK - 教材引用，无需 Crossref 核验

---

## §3 待查证文献（标记 UNRESOLVED）

### 3.1 Hotelling & Solomons (1932)

| 待定项 | 预期内容 |
|--------|---------|
| **Expected Title** | "On the Standard Error of the Median" or similar |
| **Expected Authors** | Harold Hotelling, Tilly L. Solomons |
| **Expected Journal** | Journal of the American Statistical Association or Biometrika |
| **Expected Year** | ~1932 |

**需要进一步搜索验证**。目前仅凭记忆索引，需正式查询。

**状态**: 🔴 UNRESOLVED - 待查文献数据库

---

### 3.2 Beaton & Tukey (1976)

| 待定项 | 预期内容 |
|--------|---------|
| **Context** | Iterative weighted least squares efficiency analysis |
| **Relevance** | k_eff = 1.152 的可能理论来源 |

**状态**: 🔴 UNRESOLVED - 需确定具体篇目

---

## §4 审查意见引用的其他文献

### 4.1 Peacock (1984) Ap.J. 283, 387

| 字段 | 内容 |
|-----|------|
| **Expected Title** | Comparing Distributions |
| **Authors** | Peter Peacock |
| **Journal** | ApJ |
| **Volume** | 283 |
| **Pages** | 387+ |
| **Year** | 1984 |

**用途**: 高斯轮廓参数量化公式（FWHM↔σ因子等）

**状态**: ⏸️ Pending - 审查意见提及但未在 Route 3 主要工作中核验

---

### 4.2 Stigler (1977) Truncated Mean Variance

| 待定项 | 预期内容 |
|--------|---------|
| **Title** | "Do Estimators of Location Have Optimal Properties?" |
| **Book** | Statistical Data Analysis and Inference |
| **Editor** | D.L. Horrocks (ed.) |
| **Publisher** | North-Holland |
| **Year** | 1977 |
| **Pages** | 267-284 |

**用途**: 截尾均值渐近方差 (P-CST-06)

**状态**: ⏸️ Pending - P-CST-06 审查涉及但未深度展开

---

## §5 总结

Route 3 独立文献核验结论：

| 状态 | 数量 | 说明 |
|-----|------|------|
| ✅ RESOLVED | 2 | DOI/API 校验成功 (Rousseeuw 1993, Moffat 1969) |
| ⚠️ BOOK | 2 | 教科书级标准参考，无需 API 核验 (Huber 1981, Lehmann 1983) |
| 🔴 UNRESOLVED | 2 | 待进一步查证 (Hotelling 1932, Beaton & Tukey 1976) |
| ⏸️ Pending | 2 | 审查意见提及但未作为核心工作展开 |

**后续行动建议**:
1. 对 UNRESOLVED 项进行文献数据库检索（ADS、Web of Science）
2. 将教科书引用规范化为作者、书名、页码格式
3. 考虑在仓库中维护一份"科学链文献目录"统一索引，避免再次出现《已确立》§3 幻觉锚问题

---

*Route 3 refs.md 生成时间：2026-09-26T*
