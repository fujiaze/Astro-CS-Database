# 参考文献核验记录 · P-CST-24 IDW 参数组

**日期**: 2026-09-26  
**工具**: Crossref API (`api.crossref.org`)  

---

## §1. 查询执行记录

### 查询 1: "inverse distance weighted interpolation astronomy"

```bash
curl -s "https://api.crossref.org/works?query=idw%20power%20exponent%20value&rows=5"
```

**结果统计**:
- 命中数量：5 条
- **天文领域直接相关：0 条**
- 地质学应用：2 条
- 生态学研究：1 条
- 心理学研究：1 条

**代表文献**:

#### R-01: 地质学应用
- **DOI**: [10.1127/1860-1804/2012/0163-0493](https://doi.org/10.1127/1860-1804/2012/0163-0493)
- **题名**: Interpolation based on isolines: line-geometry-based inverse distance weighted interpolation (L-IDW) with sample applications from the geosciences
- **作者**: Gossel Wolfgang, Chudy Thomas, Falkenhagen Michael
- **年份**: 2012
- **期刊**: Erdkunde
- **要点**: 地质学中的 IDW 应用，非天文

#### R-02: 地形高程模型（DEM）
- **DOI**: [10.1155/2022/9842439](https://doi.org/10.1155/2022/9842439)
- **题名**: Retracted: Interpolation Parameters in Inverse Distance-Weighted Interpolation Algorithm on DEM Interpolation Error
- **状态**: 已撤稿（Retracted）
- **要点**: 该文献已被撤稿，不可引用

#### R-03: 生态学应用
- **DOI**: [10.7717/peerj.2473/fig-3](https://doi.org/10.7717/peerj.2473/fig-3)
- **题名**: Figure 3: Inverse distance weighted interpolation model of Acropora cervicornis abundance within surveyed area.
- **要点**: 仅包含图表，非方法学论文

---

### 查询 2: EXP-04 采用的 power=1.0 来源

EXP-04 `code/exp04/operators.py:291`明确采用：
```python
"idw": ShepardIDWInterpolator(n_neighbors=10, power=1.0, reg=0)
```

**理论依据**:
- Sheperd (1968) 经典方法的默认设置是 power=1
- 这是标准 IDW 插值法，而非二次反距离加权

**未找到权威文献支持 power=2.0 优于 power=1.0**

---

## §2. 文献腿评估结论

### 现有证据质量

| 常数 | 文献支撑强度 | 说明 |
|-----|------------|------|
| `idw_power=2.0` | ❌ 弱 | 无天文标准约定；EXP-04 用 power=1.0 |
| `K=16` | ❌ 无 | 未见相关文献 |
| `γ<1e-10` | ✅ 强 | 基于浮点运算安全余量的合理选择 |

### 关键缺失

1. **天文领域的 IDW 功率参数标准**：未找到
2. **power 参数的系统性比较研究**：未发现
3. **K 近邻数的理论最优性证明**：未找到

### 建议处理方式

由于缺乏天文标准约定，P-CST-24a（idw_power=2.0）应当：
- **选项 A**: 改回标准 Sheperd 方法（power=1.0）
- **选项 B**: 标注为"本仓自订经验参数，未经系统验证"
- **选项 C**: 通过 K-fold CV 补充实验标定

鉴于 IDW 已被 EXP-04 证明劣于样条类算子，**不建议投入精力优化 IDW 参数**。

---

## §3. 文档索引

| 引用编号 | DOI/arXiv | 类型 | 是否可用 | 备注 |
|---------|-----------|------|---------|------|
| R-01 | 10.1127/1860-1804/2012/0163-0493 | 地质学 | ⚠️ 不推荐 | 领域不同 |
| R-02 | 10.1155/2022/9842439 | 方法论 | ❌ 弃用 | 已撤稿 |
| R-03 | 10.7717/peerj.2473/fig-3 | 生态图 | ⚠️ 辅助 | 仅含图表 |

**结论**：**没有可直接引用的天文 IDW 幂次标准文献**。

---

*第②路复核者结束文献核验*  
*时间戳：2026-09-26T16:00:00Z*
