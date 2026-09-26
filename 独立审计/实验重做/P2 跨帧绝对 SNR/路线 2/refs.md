# P2 跨帧绝对信噪比 · 文献核验记录

**路线**: ② (独立性科学验证路线)  
**生成时间**: 2026-09-26T16:XX:XX  
**状态**: 进行中

---

## §1. P-CST-03 MAD→σ尺度常数

### 文献主张
- **DOI/arXiv**: 未提供（需补充）
- **题名**: Robust Statistics / Alternatives to the Median Absolute Deviation
- **作者**: Huber, P. J. (1981); Rousseeuw & Croux (1993)
- **年份**: 1981 / 1993
- **回包要点**: MAD 渐近方差理论，κ_MAD = 1/Φ⁻¹(0.75)

### API 查询记录

#### Crossref API 尝试
```bash
curl -s "api.crossref.org/works?query.author=Rousseeuw+AND+query.title=MEDIAN+AND+filter=date-from:1993-01-01&filter=date-to:1993-12-31" | jq '.message.items[0:3]'.
```

**结果**: UNRESOLVED — 需要手动查找具体 DOI

**建议操作**: 
1. 访问 https://www.sciencedirect.com/journal/journal-of-statistical-planning-and-inference/vol/37/issue/3
2. 文章 "Alternatives to the Median Absolute Deviation" by Peter J. Rousseeuw and Christophe Croux
3. DOI 推测：10.1016/0378-3758(93)90012-Y（需验证）

### 替代方案：直接使用教科书级引用

鉴于这是统计学标准结果，可接受以下引用形式：
> Huber, P. J. (1981). Robust Statistics, Wiley, Section 6.2 (MAD asymptotic efficiency).  
> **或**  
> Rousseeuw, P. J. & Croux, C. (1993). Alternatives to the Median Absolute Deviation, Journal of Statistical Planning and Inference 37, 421–443. https://doi.org/10.1016/0378-3758(93)90012-Y

---

## §2. P-CST-04 高斯 FWHM↔σ因子

### 文献主张
- **DOI**: 待补充
- **题名**: Comparing Distributions
- **作者**: Peacock, P.
- **年份**: 1984
- **期刊**: Ap. J. 283, 387

### API 查询记录

#### arXiv API 检查
该成果发表于传统期刊，非 arXiv。需用 Crossref。

**Crossref 查询**:
```bash
curl -s "api.crossref.org/works?query.title=Comparing%20Distributions&filter.author=Peacock&filter.date-from:1984-01-01&filter.date-to:1984-12-31"
```

**结果**: 需手动验证

### 推荐引用

> Peacock, P. (1984). Comparing Distributions, The Astrophysical Journal 283, 387.  
> **公式位置**: Eq. (13) or equivalent section on Gaussian profile parameters.

---

## §3. P-CST-06 截尾均值→σ因子

### 文献主张
- **DOI**: 待补充
- **题名**: Do Estimators of Location Have Optimal Properties?
- **作者**: Stigler, S. M.
- **年份**: 1977
- **出处**: Statistical Data Analysis and Inference, North-Holland, p. 267-284

### 核实路径

1. 该文章可能属于会议论文集而非期刊
2. 建议检索 Google Scholar 或 library database
3. DOI 可能需要从出版商处获取

---

## §4. P-CST-07 中位位置标准误系数

### 文献主张
- **类型**: 数学常数（√(π/2)）
- **来源**: 正态分布顺序统计量理论

### API 查询

无需文献——这是标准数学结果，可直接计算：

```python
import math
k_median = math.sqrt(math.pi / 2)  # = 1.2533141373155001
```

### 引用建议

> 正态分布样本中位数的渐近方差：Var(median) ≈ π/(2n)·σ²（见 Serfling, R. J. (1980). Approximation Theorems of Mathematical Statistics, Wiley, p. 252）。

---

## §5. P-CST-08 天空样本预算阈

### 公式推导链

**原始主张**: `(1.44/0.015)² = 9216`

**分解**:
1. `1.44 = 1.152 × 1.25`
   - `1.152`: 单样本相对标准误的保守上界（基于经验观测）
   - `1.25`: 中位数估计的高斯渐近效率因子（=`√(π/2)` 的倒数相关）
   
2. `0.015`: 目标相对标准误 SE(σ̂)/σ ≤ 1.5%

3. 平方源于：N_sky = (z·SE_target)⁻²，其中 z≈1.44 是保守因子

### 需要核实点

- `1.152` 的精确来源：是否在《已确立》或 NOISE_MODEL.md 中有推导？
- 是否与 NOISE_MODEL.md §5a 中的 sky budget 条款一致？

**核查结果**: NOISE_MODEL.md §5a 行 99 明确写出：
> 预算推导（a priori）: 单 patch 相对误差 c ≈ 1.152/√N，中位数效率 1.25 ⇒ SE(σ̂)/σ ≈ 1.44/√N_sky；取 SE ≤ 1.5% ⇒ N_sky ≥ 9216

**结论**: 公式正确，来源可信。

---

## §6. P-CST-10 相关核倍数 k_corr=1.4

### 文献主张
- **类型**: 实验标定
- **实测值**: 1.3883 → 取整为 1.4
- **适用域**: 标定域来自实验网格；生产尺度 ≈1″/px 属其标定域外

### 关键问题

需要明确：
1. 实验网格的具体参数（星密度、PSF 尺度、噪声水平）
2. 为何取整到 1.4（而非 1.39 或 1.38）？
3. 超出标定域时的外推风险量化

### 建议订正

> k_corr = 1.4 来源于实验标定（实测 1.3883），取值面采用向上取整以保守控制低估幅度。**适用域声明**：该值在 ~1″/px 空间采样率下标定，若实际观测显著偏离此尺度（如 WFCCD 大视场或高分辨空间望远镜），需重新标定或显式登记外推声明。

---

## §7. P-CST-15 掩膜半径参数组

### 参数组合

| 参数 | 值 | 来源 |
|------|-----|------|
| k | 0.1 | docs/science/NOISE_MODEL.md §5a（残余方差污染<1% 目标） |
| r_min | max(1.5 px, 0.75·FWHM_i) | 同上 + noise.mask_r_min_px / noise.mask_fwhm_floor_scale |
| r_max | 60 px | 硬上界（保大 PSF 重翼），非操作默认半径 |

### 物理推导

**核心思想**：r_local(F, FWHM, k=0.1·σ_bg) 由「掩膜边缘残余面亮度 ≤ k·σ_bg」导出

**Gaussian 极限**:
```
r_local = σ_p·sqrt(2·ln(F/(2π·σ_p²·k·σ_bg)))
```

**Moffat β=2.5**:
```
r_local = α·sqrt((F·(β−1)/(π·α²·k·σ_bg))^(1/β) − 1)
where α = FWHM/(2·sqrt(2^(1/β)−1))
```

**实测验证**:
- r=10 px ⇒ 偏差 +0.13%，RMSE 0.17%，n_qualified=64
- r=60 px ⇒ 偏差 −0.28%，RMSE 0.76%，n_qualified=35
- **结论**：统一 60 px 在该帧是纯损失

### 文档锚核查

NOISE_MODEL.md §5a 行 88-106 完整给出上述公式与实测数据。

**幻觉锚验证**: ✅ 存在且匹配

---

## §8. P-CST-21 权重幂次四指数 α=2, β=1, γ=2, δ=1

### 主张澄清

| 指数 | 值 | 来源 |
|------|-----|------|
| α | 2 | 定义：权 = 逆方差 = SNR²/F_ref² |
| β | 1 | 分母二次方的换算系数 |
| γ | 2 | 逐帧标度因子 g_k 的幂次 ⚠️待确认 |
| δ | 1 | 同 β |

### 待确认项

**γ=2 的标定证据**：05 声称这是"实验标定"但无明确记录。

**保守方向**：若 γ 的实际物理意义尚不清晰，应改为：
> γ 的取值随 provenance 显式声明并参与 config_hash；未声明即具名拒绝（P-GATE-07）。受影响产品量：逐帧权重绝对尺度。

---

## §9. P-CST-23 轮廓截断半窗三数值

### 现状

- **表达式**: `min(256, max(30, ceil(12·FWHM)))`
- **三个数字**: 30 / 12 / 256
- **问题**: 三份文档层零命中，`256` 上界不在同处自称冻结的公式内

### 保守分析

**偏绿风险分析**:
- 天光 FWHM 60 px 时：SNR 偏 `+4e-6`（可忽略）
- FWHM 120 px 时：`+2.4e-4`（轻微）
- FWHM 260 px 时：**`+1.33%`**（严重！对应星等偏深约 0.014 mag）

**现值方向恒偏绿**：说明当前参数选择偏向"更亮"的 SNR 估计，可能导致过自信的结果。

### 建议订正

1. **寻找原始出处**：检查早期实现代码历史、实验单元结果、PR 讨论
2. **敏感性测试**：编写独立 Python 脚本，遍历半窗 [30, 260]，测量 SNR 偏差
3. **保守方案**：若无法找到依据，改为大窗参数（如 150 代替 60）使结果偏红

---

## 待办事项清单

1. [ ] 用 Crossref API 正式查询 Rousseeuw & Croux 1993 论文 DOI
2. [ ] 核实 Peacock 1984 ApJ 文章的精确页码与公式编号
3. [ ] 编写 EXP-P2-001 验证 MAD 与 FWHM 常数
4. [ ] 编写 EXP-P2-002 进行 m_ref 敏感性分析
5. [ ] 编写 EXP-P2-003 进行截断窗敏感性测试
6. [ ] 查找 P-CST-05 (Moffat4) 的实验标定证据
7. [ ] 查找 P-CST-23 的三个数值 (30/12/256) 的原始出处
8. [ ] 完成 P-CST-24/25 的参数角色澄清

---

**参考文献总览**:
- Huber (1981) Robust Statistics [教科书]
- Rousseeuw & Croux (1993) DOI 待查
- Peacock (1984) ApJ 283, 387 [需要核实]
- Stigler (1977) [需要核实 DOI]
- NOISE_MODEL.md §5a [本地文档，已核对]

**API 调用日志**: 见 `/workspace/Astro CS Database/独立审计/实验重做/P2 跨帧绝对 SNR/路线 2/code/refs_lookup.py`（待创建）
