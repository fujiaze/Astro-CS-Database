# 参考文献记录 - P2 跨帧绝对 SNR（路线 1）

**审计员**: 独立科学研究路线 1  
**日期**: 2026-09-26  
**用途**: 本模块所有文献腿核验的可追溯记录

---

## 一、闭式数学恒等式（无需外部文献）

### C1. P-CST-03: MAD→σ scaling constant = 1.482602218505602

**声明**: `1 / Φ⁻¹(0.75)`, 其中 Φ 为标准正态累积分布函数

**推导**:
- MAD (Median Absolute Deviation) 定义为: `MAD = median(|X - median(X)|)`
- 对于标准正态 `N(0,1)`, MAD 的理论值为 `Φ⁻¹(0.75) ≈ 0.6744897501960817`
- 因此 σ 的无偏估计为: `σ̂ = MAD / 0.6744897501960817 = MAD × 1.482602218505602`

**实验验证**: `results/p2_cst03_mad_sigma.json` (10 runs, N=10⁶, seed=42)
- Mean relative error: -94.8 ppm
- Max |error|: 1396 ppm
- Within O(1/√N) statistical bounds ✅

**结论**: 该常数为闭式数学恒等式，标注为"公式导出"正确，无需 DOI/arXiv 引用。

---

### C2. P-CST-04: Gaussian FWHM↔σ factor = 2.3548200450309493

**声明**: `2√(2ln2)`, 高斯分布 FWHM 与标准差的转换系数

**推导**:
```
FWHM = 2 × σ × √(2 × ln(2))
     = 2 × σ × √1.386294361...
     ≈ 2.3548200450309493 × σ
```

**来源**: 概率论教科书（Abramowitz & Stegun 第 26 章，或任何标准统计教材）

**结论**: 闭式数学恒等式，标注为"公式导出（高斯分布性质）"正确。

---

### C3. P-CST-07: Median position SE coefficient = √(π/2) = 1.2533141373155001

**声明**: `(π/2)^½`, 正态分布中位数位置的标准误系数

**推导**:
- 对于 i.i.d. 样本 `X_i ~ N(μ, σ²)`, 样本中位数的渐近方差为:
  `Var(median) ≈ 1 / (4n f(μ)²)`, 其中 `f(μ) = 1/(σ√(2π))`
- 代入得: `SE(median) ≈ σ × √(π/2) / √n = σ × 1.2533141373155001 / √n`

**结论**: 闭式数学恒等式，标注为"公式导出（闭式精确值）"正确。

---

## 二、项目标定常数（需澄清来源档）

### C4. P-CST-05: Moffat4 FWHM↔σ factor = 1.230310

**声明**: Moffat β=4 轮廓的 FWHM↔σ 等效转换系数

**交叉检索**:
- Crossref API 查询 `10.1086/132801`: Signal-to-noise considerations for sky-subtracted CCD data
- 该文贡献在于 CCD 数据处理噪声分析，**非** Moffat 拟合系数来源

**实际来源**: `SCI-PSF-001` (项目内部冻结常量), 相对差 +1.91e-6

**建议标注修改**:
```
原: "文献值＋本仓自订混合"
改: "实验标定（冻结实现常量 per SCI-PSF-001; 相对差 +1.91e-6）"
```

**理由**: 该值为项目实测拟合得出，不应混用"文献值"表述引发歧义。

---

### C5. P-CST-09: Robust clipping σ multiplier = 5.0

**声明**: 5σ裁剪阈值（Tukey biweight 内点筛选）

**来源档**: `docs/science/NOISE_MODEL.md` §6 稳定性分析

**建议标注修改**:
```
原: "本仓自订（SCI 冻结，**非外部文献**）"
改: "本仓自订（SCI 冻结，理由：NOISE_MODEL.md §6 稳定性分析；禁止外推为通用文献值）"
```

**说明**: 必须在登记面明确这是"仓库自订"而非通用天文实践值。

---

### C6. P-CST-10: Correlation kernel multiplier = 1.4

**声明**: 相关核倍数，原始标定值 1.3883 取整至 1.4

**来源档**: `eng/contracts/data/v6_clause_registry_v1.json` #line 1722

**建议标注修改**:
```
原: "实验标定（标定值 1.3883，取整登记）" 混用 "文献值"
改: "实验标定（标定值 1.3883，取整登记；适用域：见 DATA_SEMANTICS.md §31.5）"
```

**说明**: 明确标定值和取整操作，避免与"文献值"混淆。

---

## 三、系统性幻觉锚清理

### V1. "《已确立》§3 行 X" 引用体系

**问题**: 审查 -05-② -科学性 -1.md 发现 **所有** `《已确立》§3 行 X` 引用均无效

**原因**: 《已确立》文件不存在此节编号体系

**订正方案**:
1. 删除所有此类引用
2. 替换为实际存在的文档锚定位点：
   - 常数定义 → `docs/science/NOISE_MODEL.md` 对应章节
   - 算法步骤 → `docs/plugins/algorithms_phase1/07_noise_snr.md`
   - 产品合同 → `eng/contracts/schemas/unified/*.schema.json`

**示例订正文本**:
```
> 在 05 开头增加"**幻觉锚清理条款**"：所有对《已确立》的引用必须经三步核验——
> (a) 打开原文核对行号是否存在该处；(b) 该行内容是否确为所声称的值；(c) 
> 是否真属于"§3 常数表"。**目前 05 中所有"《已确立》§3 行 X"引用均无效，必须重写锚引用体系**。
```

---

## 四、文献核验方法说明

### API 调用记录

1. **Crossref API**: `https://api.crossref.org/works/{DOI}`
   - Timeout: 10s
   - Rate limit handling: 429 退避 20s, 403 重试一次
   - User-Agent: `ACSD-Audit-Route1/1.0`

2. **arXiv API**: `http://export.arxiv.org/api/query?search={query}`
   - 用于 arXiv 可收录文献的补充检索
   - 注意：1969 年 Moffat 原始论文不在 arXiv（pre-arXiv）

### UNRESOLVED 清单

以下文献无法通过 API 核验（标记为 unresolved，不影响科学有效性）：

- P-CST-05 原始出处（Moffat 1969 A&A 3 455）：Crossref 未返回匹配，因年代过早（1969）不在现代 DOI 体系覆盖范围，但可通过图书馆渠道查阅

---

## 五、总结

**文献腿核验结论**:

✅ **无需外部文献**（闭式数学恒等式）:
- P-CST-03: MAD→σ (1/Φ⁻¹(3/4))
- P-CST-04: Gaussian FWHM (2√(2ln2))
- P-CST-07: Median SE (√(π/2))

⚠️ **需澄清来源档**（项目标定值）:
- P-CST-05: Moffat4 → "实验标定 (SCI-PSF-001)"
- P-CST-09: 5σ → "本仓自订 (NOISE_MODEL.md §6)"
- P-CST-10: 1.4 kernel → "实验标定 (1.3883→1.4)"

📝 **幻觉锚清理**:
- 移除所有 `《已确立》§3 行 X` 引用
- 改用实际文档定位符

**下一步**: 实验腿验证（standalone Python 脚本），小论文式报告撰写。

---

**附录：实验结果文件**

- `results/p2_cst03_mad_sigma.json` — P-CST-03 蒙特卡洛验证
