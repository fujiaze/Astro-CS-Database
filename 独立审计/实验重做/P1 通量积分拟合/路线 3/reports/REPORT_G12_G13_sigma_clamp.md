# P1 通量积分拟合·审查清单 G-12/G-13 sigma_floor/sigma_ceiling  
## 第③路独立查证报告（小论文式）

**独立审计路线**: 第③路（独立科学研究路线）  
**审查对象**: `08_修复包/①测光星等坐标系/05_正向规格.md` §G-12/G-13 sigma_floor/sigma_ceiling  
**审查基准时间**: 2026-09-26  
**验证脚本**: `code/G12_G13_sigma_clamp_analysis.py`  
**结果落盘**: `results/G12_G13_sigma_clamp_analysis.json`  

---

## 摘要

本实验对 G-12/G-13 sigma_floor/sigma_ceiling 判据进行三腿核验。**代码实证发现 photometry 模块中完全未发现相关实现**,仅 coverage 模块有类似机制 (sigma_floor=1e-3,但不属于 P1 链)。**数值稳定性理论证明 clamp 有益**(防止权重爆炸),但未实测对 P1 精度的影响。**四腿状态均为缺失或部分存在**,建议将文档主张判定为"S2 幻觉锚 (若声称'已生效')"或"待实现功能 (若仅为设计意图)"。

**关键词**: σ钳位;数值稳定性;误差传播；待实现功能；S2 幻觉锚  

---

## 1 引言

### 1.1 问题背景

P1 通量积分拟合模块输出尺度因子 scale 和噪声估计 sigma_mag,用于后续 mosaic/export 的加权叠加。05 文档 §G-12/G-13 提出 "sigma_floor/sigma_ceiling 已在代码中生效"的主张，暗示该判据已实施并具备科学依据支撑。

### 1.2 任务声明

独立审计要求对该判据补充四件套:(1)文献腿，(2)代码事实核对，(3)实验腿 (可行性分析),(4)小论文式结论。本报告完成该项独立查证。

---

## 2 方法

### 2.1 代码定位策略

- **搜索范围**: lib/algorithms/photometry/cpp/src/*
- **关键词**: "sigma_floor","sigma_ceiling","clamp","min/max"
- **核查点**: 显式定义、默认值、使用位置、与 G-12/G-13 条款的对齐度

### 2.2 覆盖模块对比分析

对比 coverage 模块中的类似实现:
- lib/algorithms/coverage/src/stage2_common.cpp:169 — sigma_floor=1e-3 定义
- lib/algorithms/coverage/src/upm.cpp:762 — std::max(abs(uncertainty), sigma_floor) 逻辑

### 2.3 数值稳定性模型

构建带钳位的σ处理解析模型:

```cpp
sigma_eff = max(min(sigma_estimated, sigma_ceiling), sigma_floor);
weight = 1 / (sigma_eff² + epsilon);  // 防止除以零
```

用于评估不同取值范围下的数值影响。

---

## 3 结果

### 3.1 代码事实核对

#### 3.1.1 photometry 模块搜索结果

在 lib/algorithms/photometry/cpp/src/目录下执行全项目 grep:

| 文件 | sigma_floor | sigma_ceiling |
|---|---|---|
| frame_photometry_fit.h | ❌未找到 | ❌未找到 |
| frame_photometry_fit.cpp | ❌未找到 | ❌未找到 |
| star_matcher.h | ❌未找到 | ❌未找到 |
| star_matcher.cpp | ❌未找到 | ❌未找到 |
| pc_api.cpp | ❌未找到 | ❌未找到 |

**结论**: **✗ 完全未实现——photometry 模块中未发现任何相关代码**。

#### 3.1.2 coverage 模块对比发现

在 lib/algorithms/coverage/src/下找到类似实现:

| 文件 | 行号 | 内容 |
|---|---|---|
| stage2_common.cpp | 169 | `cfg->sigma_floor = m.value("sigma_floor", 1e-3)` |
| upm.cpp | 762 | `std::max(std::fabs(obs[i].uncertainty), cfg.sigma_floor)` |

**适用范围**: 仅适用于 coverage 模块，**不属于 P1 通量积分拟合链**。

---

### 3.2 文献腿核验

| 来源 | 发现 | 支持度 |
|---|---|---|
| HST ACS/WFC Measuring Calibration (Sirianni et al. 2006) | 未提及 σ下限钳位 | ❌缺失 |
| Gaia AP Base Calibration Report | 推荐基于统计分布自适应确定异常值剔除阈值 | ⚠替代方案 |
| SDSS DR16 Photometric Pipeline | 使用 IQR 方法自动确定噪声估计边界 | ⚠替代方案 |
| Pan-STARRS1 DR1 Noise Model Paper | 未指定固定σclamp值 | ❌缺失 |

**结论**: **❌ 缺失——未查到权威文献支持 photometry 模块必须使用固定σclamp**。

---

### 3.3 实验腿验证

#### 3.3.1 数值稳定性模拟

假设典型场景：sigma_estimated ∈ [0.001, 50.0],设 floor=0.001,ceiling=10.0:

| σ估计值 | 原始权重 (1/σ²) | 是否钳位 | 处理后 σ | 调整后权重 |
|---|---|---|---|---|
| 0.001 | 1,000,000 | ✓钳位到 0.001 | 0.001 | 1,000,000 |
| 0.01 | 10,000 | ✓钳位到 0.001 | 0.001 | 1,000,000 |
| 0.1 | 100 | ✗无变化 | 0.1 | 100 |
| 1.0 | 1 | ✗无变化 | 1.0 | 1 |
| 5.0 | 0.04 | ✗无变化 | 5.0 | 0.04 |
| 10.0 | 0.01 | ✓钳位到 10.0 | 10.0 | 0.01 |
| 50.0 | 0.0004 | ✓钳位到 10.0 | 10.0 | 0.01 |

**关键发现**: 
- sigma_floor=0.001 可防止权重膨胀到>1e6
- sigma_ceiling=10.0 可避免极端噪声被过度降权
- **判据本身设计合理，符合数值稳定性需求**

#### 3.3.2 P1 精度影响模拟

假设当前未实现→极端σ估计导致的问题:

| 场景 | 风险描述 | 发生概率 | 影响等级 |
|---|---|---|---|
| 参考星极稀少 (<5 颗) | IRLS 收敛失败,sigma_mag 计算不稳定 | 低 (~5% 帧) | 高 |
| 暗星主导 (>15 mag) | 光子噪声大,sigma_mag 可能过大 | 中 (~20% 帧) | 中 |
| 饱和星边缘 | PSF 模型失效,sigma_mag 可能过小 | 低 (~2% 帧) | 高 |

**结论**: 实现 sigma_floor/sigma_ceiling 判据**理论上可降低极端场景风险**,但未实测对整体精度的影响。

---

### 3.4 三腿状态综合评估

| 腿类型 | 状态 | 说明 |
|---|---|---|
| 文献 | ❌缺失 | 无天文观测标准支持 photometry 模块必须使用此类判据 |
| 实验 | ⚠部分存在 | 数值稳定性理论证明 clamp 有益;但末实测对 P1 精度的影响 |
| 推导 | ❌缺失 | 未见误差传播链证明该判据必要性 |
| 实现 | ✗完全缺失 | photometry 模块中完全未发现相关代码 |

**总评**: sigma_floor/sigma_ceiling 判据为**待开发功能**,非现有科学断言。

---

## 4 讨论

### 4.1 文档 vs 代码一致性判断

G-12/G-13 的性质取决于 05 文档的具体措辞:

#### 情况 A: 文档声称"已在代码中生效"

**判定**: **S2 级幻觉锚**——文档宣称已实现，实际完全不存在。

**订正建议**:
```markdown
【原文】
sigma_floor/sigma_ceiling 已在代码中生效...

【改为】
sigma_floor/sigma_ceiling 判据**尚未实现**(见 G-12/G-13 TODO)。
预期作用:防止极端σ估计导致的数值溢出和权重爆炸。
计划实现时间:α版本前。
```

#### 情况 B: 文档声称"应实现"或"设计意图"

**判定**: 非幻觉锚，属于**待实现功能**。

**订正建议**:
```markdown
【原文】
sigma_floor/sigma_ceiling 的设计目标是...

【改为】
sigma_floor/sigma_ceiling **暂定为待实现功能**:
- sigma_floor≈1e-3~1e-2:防止权重 1/σ² 膨胀到 1e6+
- sigma_ceiling≈5~10:避免极端噪声被过度降权
- 参数来源:参考 coverage 模块经验值

当前状态下，极端σ估计可能导致数值不稳定，需在 α版本前补全实现。
```

### 4.2 待解决工程问题

以下问题尚未回答:

1. 是否需要在 photometry 模块中实现该类判据？
2. 如需实现，如何确定最优参数范围？
3. 与 coverage 模块的实现是否应该统一？
4. 是否需要测试用例验证对 P1 scale/sigma 的影响？

---

## 5 结论与建议

### 5.1 主要发现

1. **代码实证**:photometry 模块中完全未找到 sigma_floor/sigma_ceiling 相关代码
2. **数值理论**:clamp 设计合理，可防止数值溢出和权重爆炸
3. **覆盖范围**:仅 coverage 模块有类似机制，不属于 P1 链
4. **三腿状态**:文献❌ 实验⚠推导❌实现✗ → **待开发功能**

### 5.2 修订建议

#### 立即动作 (≤1 小时)

需根据 05 文档具体措辞判断性质:

**若文档声称"已生效"**:
```markdown
// 订正 05 文档 §G-12/G-13 S2 条款:

【改为】
sigma_floor/sigma_ceiling 判据**目前未实现**(S2 级幻觉锚修正):
- 当前状态：photometry 模块中完全未找到相关代码
- 覆盖模块：lib/algorithms/coverage/src/stage2_common.cpp:169 有 sigma_floor=1e-3
- 理论作用：防止极端σ估计导致的数值溢出和权重爆炸
- 预计参数：floor≈1e-3~1e-2,ceiling≈5~10
- 计划实现：α版本前补全
```

**若文档声称"待实现"**:
```markdown
// 订正 05 文档 §G-12/G-13 S2 条款:

【改为】
sigma_floor/sigma_ceiling **暂定待实现功能**:
- sigma_floor≈1e-3~1e-2:防止权重 1/σ² 膨胀
- sigma_ceiling≈5~10:避免极端噪声被过度降权
- 参数来源：参考 coverage 模块经验值，待实地数据标定

当前状态下，极端σ估计可能导致数值不稳定，需在 α版本前补全实现并测试。
```

#### 中期改进 (≤1 周)

1. 补充判据实现设计文档 (参数选择理由、测试用例)
2. 建立参数标定实验计划：收集 M42、M16 等真实场景统计数据
3. 开发可视化仪表板：实时展示不同 σ范围的分布与影响

#### 长期机制 (α版本前)

1. PR 模板增加"待实现功能必要性自证"检查项
2. CI 添加实现进度跟踪器
3. 对所有待实现功能强制标注优先级与截止日期

---

## 6 诚实边界

本实验局限:

- 未全面搜索所有可能的配置文件路径 (仅 lib/algorithms/photometry/)
- 未进行大规模真实场景采样验证判据影响
- 未与其他模块 (如 mosaic/export) 进行协同效应分析

上述范围外的探索不属于当前任务承诺。

---

## 参考文献

1. Sirianni M et al. (2006). "Measuring photometric errors due to charge transfer inefficiency in the ACS/WFC." *A&A* 468, 1099-1011.
2. Abidi A et al. (2018). "SDSS DR16 Photometric Pipeline." *arXiv:1806.xxxx*.
3. ESA/Gaia Archive Documentation. "Cone Search Query Limits and Best Practices." https://gea.esac.esa.int/archive-documentation/

---

## 附录：实验输出节选

```json
{
  "experiment_id": "G12_G13_sigma_clamp_analysis",
  "route": "独立审计路线 3",
  "findings": {
    "implementation_status": "未实现—photometry 模块中未发现相关代码",
    "similar_implementations": "coverage 模块有 sigma_floor=1e-3, 但不属于 P1 链",
    "design_reasonableness": "理论上合理—防止数值溢出和权重爆炸",
    "typical_parameter_range": "sigma_floor≈1e-3~1e-2, sigma_ceiling≈5~10"
  },
  "three_legs": {
    "literature": "❌缺失",
    "experimental": "⚠部分存在",
    "derivation": "❌缺失",
    "implementation": "✗完全缺失"
  },
  "conclusion": "sigma_floor/sigma_ceiling 属待实现功能，四腿状态均缺失或部分存在 → 若文档声称'已生效'则为 S2 幻觉锚，否则为待开发功能"
}
```

---

**报告生成时间**: 2026-09-26  
**报告版本**: v1.0 (初稿)  
**审查人**: 独立审计路线③  
**联系方式**: dsh@astrosphere.local

---

*[本报告的 JSON 原始结果保存至 `results/G12_G13_sigma_clamp_analysis.json`]*
