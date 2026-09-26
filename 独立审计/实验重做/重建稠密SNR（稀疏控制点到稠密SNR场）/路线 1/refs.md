# P4 重建稠密 SNR — 文献核验记录汇总
## Route 1 — Bibliography Verification Records

> **说明**: 本文件汇总独立审计第 1 路对重建稠密 SNR 模块所有文献引用的核验结果。每条文献需记录：DOI/arXiv 号、题名、作者、年份、回包要点、核验状态。

---

## 核验方法论

**API 调用规范**:
- Crossref API: `api.crossref.org/works/{DOI}`
- arXiv API: `export.arxiv.org/api/query?query={query}`
- **间隔**: arXiv 调用间隔 `sleep(3)`，避免速率限制
- **退避**: 429 错误时退避 20 秒后重试
- **重试**: 403 错误重试一次
- **UNRESOLVED**: 解析不到真实记 UNRESOLVED，绝不编造

**核验标准**:
1. **锚定成立**: DOI 存在 + 内容匹配 ACSD 引用主张
2. **部分匹配**: DOI 存在但仅部分支持（需注明差异）
3. **UNRESOLVED**: 无法获取原文或元数据不全
4. **错误锚点**: ACSD 声称的引用不存在或与原文不符

---

## 待核验文献清单

### F-P4-L01: HEALPix 插值函数参考

**ACSD 主张**: 
- 引用 Zonca et al. 2019 (healpy), JOSS 4, 1298. DOI: 10.21105/joss.01298
- 作为"控制点场的稀疏→稠密重建算子"的软件实现依据
- 特别说明：healpy 的插值**不返回插值误差方差**，需自行提供

**核验目标**:
1. 确认 DOI 可解析
2. 核对论文是否讨论 HEALPix 网格上的插值函数
3. 确认 healpy 源码中的实际插值算法
4. 验证"不返回预测方差"的陈述

**API 调用**:
```bash
curl -s "https://api.crossref.org/works/10.21105/joss.01298" | jq '.message'
```

**核验记录**:
| 项目 | 预期 | 实测 | 状态 |
|-----|------|-----|------|
| DOI 存在性 | 是 | *待填* | *待填* |
| 题名匹配 | Healpy | *待填* | *待填* |
| 作者列表 | Zonca et al. | *待填* | *待填* |
| 年份 | 2019 | *待填* | *待填* |
| 内容相关性 | 讨论插值 | *待填* | *待填* |
| 源码地址 | github.com/healpy/healpy | *待填* | *待填* |

**结论**: *待填充*

---

### F-P4-L02: 双线性插值在天文图像重采样中的应用

**ACSD 主张**:
- 双线性插值作为三种重建口径之一
- 需要文献支撑其在天文 CCD/CMOS 图像重采样中的标准应用

**待查关键词**:
- "bilinear interpolation astronomical image resampling"
- "CCD image reconstruction bilinear"
- "HEALPix bilinear interpolation astronomy"

**候选文献**:
1. **Swarcat & Scamp**: GPL-3.0 实现，需核查其双线性权重定义
   - SWarp: https://github.com/astromatic/swarp
   - 源码路径：`src/coadd.c` 中的加权插值
   
2. **Astropy reproject**: BSD-3-Clause
   - https://github.com/astropy/reproject
   - 需核查 WCS 重采样中的线性插值实现

**核验目标**:
1. 找到双线性插值在天文图像重采样中的标准参考文献
2. 对比开源实现的数学定义
3. 确认 ACSD 实现与标准一致

---

### F-P4-L03: 自然三次样条插值的理论性质

**ACSD 主张**:
- 自然三次样条作为可选重建口径
- 需验证其节点精确复现性质与平滑性保证

**待查关键词**:
- "natural cubic spline interpolation properties"
- "cubic Hermite spline astronomical data"

**候选理论文献**:
1. **de Boor, C. (2001)**. A Practical Guide to Splines. Springer.
   - 三次样条的理论基础
   
2. **Stössel, T.** 的天文图像处理应用文献
   - 需查找具体引用

**核验目标**:
1. 确认证书在控制点处精确复现（节点特性）
2. 验证平滑性（二阶导数连续）
3. 确认边界条件处理（自然样条 vs 其他）

---

### F-P4-L04: 最近邻插值的误差界

**ACSD 主张**:
- 最近邻插值作为最简单的重建选项
- 理论上具有最大误差但计算成本最低

**待查关键词**:
- "nearest neighbor interpolation error bounds"
- "zero-order hold interpolation accuracy"

**候选理论文献**:
1. **Blinn, J. F.** 关于插值误差的分析
2. **数字图像处理标准教材**中的插值章节

**核验目标**:
1. 最近邻插值的收敛阶（O(1)？）
2. 与双线性、三次样条的对比
3. 在天文数据中的适用域（何时可用/何时不可用）

---

### F-P4-L05: 稀疏采样重建理论

**背景**: 
- ACSD 的科学链要求从稀疏星点控制点重建稠密场
- 这与信号处理中的稀疏采样定理相关

**核心理论问题**:
1. **Nyquist-Shannon 采样定理**在球面网格上的适用性
2. **压缩感知 (Compressed Sensing)** 理论是否适用？
3. **克里金插值 (Kriging)** 与确定性插值的区别

**待查文献**:
1. **Zackay & Ofek** 关于最优权重的原始论文（如引用）
   - 需核实是否存在该文献及具体内容
   
2. **Healpy/healpix 相关的采样理论**:
   - Górski et al. 2005 (HEALPix 原始论文)
   - Zonca et al. 2019 (healpy)

**关键疑问**:
- ACSD 使用的确定性插值（双线性/样条）与统计最优插值（如克里金）的差异
- 是否有文献支撑"几何插值 = 统计最优"的主张

---

### F-P4-L06: 稀疏控制点密度→重建误差关系

**审查意见提及的实验四**:
> EXP-4 analytic control-point density vs dense-SNR reconstruction error

**待查来源**:
1. 实验目录中是否有对应的分析报告
2. 该实验是否有理论推导支撑
3. 实验设计的合理性（真值如何构造？）

**分析目标**:
1. 控制点间距 Δ 与重建误差 ε 的关系（ε ∝ Δ^p？）
2. 不同重建算子的收敛阶对比
3. 球面投影带来的额外误差项

---

## 核验进度总表

| 编号 | 主题 | 状态 | 发现/结论 | 备注 |
|-----|------|-----|---------|-----|
| F-P4-L01 | healpy 引用 | *未开始* | — | DOI 待解析 |
| F-P4-L02 | 双线性插值 | *未开始* | — | 需查天文应用文献 |
| F-P4-L03 | 自然三次样条 | *未开始* | — | 理论文献待查 |
| F-P4-L04 | 最近邻误差界 | *未开始* | — | 数值分析标准结果 |
| F-P4-L05 | 稀疏采样理论 | *未开始* | — | Zackay&Ofek 文献核实 |
| F-P4-L06 | 密度 - 误差关系 | *未开始* | — | 依赖实验四报告 |

---

## 幻觉锚清单

经初步查阅，以下 ACSD 文档中的文献引用需重点核验：

### 伪锚点 1: "Zackay & Ofek" 文献主张

**ACSD 文档声称**:
- 引用 Zackay & Ofek 的最优权重公式
- 声称 `w = SNR²/F_ref²` 来自该文献的"optimal integration"

**待核实**:
1. 是否存在该组合作者的公开论文
2. 该论文是否讨论了 SNR 与逆方差权重的关系
3. 若不存在，则需寻找真正的出处（可能是 Zackay & Ofek 各自的工作或误引）

**风险等级**: 高（若为误引，影响核心科学方法的权威性）

---

### 伪锚点 2: "标准 02 §4" 多次引用

**ACSD 文档声称**:
- 多次引用"标准 02 §4"作为常数取值依据
- 如 m_ref = 6.0 的星等档选择

**待核实**:
1. "标准 02"是什么标准文档？
2. §4 是否讨论了参考星等选择？
3. 若无此标准，则属"虚构锚点"

---

## 下一步行动

1. **优先级 1**: 核验 F-P4-L01 (healpy) 与 F-P4-L05 (稀疏采样理论)
   - 这两项直接影响 P4 核心算法的科学依据

2. **优先级 2**: 执行 EXP-P4-01（三口径对账实验）
   - 实验证据比文献追溯更直接

3. **待定**: 理论推导（六线样条误差界、控制点密度关系）
   - 可能需要单独申请计算资源

---

**最后更新**: 2026-09-26  
**状态**: 框架已建，待逐项执行 API 调用
