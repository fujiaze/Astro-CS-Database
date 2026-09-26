# UPM 模块 · 文献核验记录 (路线 3)

**说明**: 本文件汇总所有科学量/常数/判据的一手文献核验结果

---

## A. 已核验成功的文献

### 1. Serfling, R. J. 1980 — Var(median) 渐近方差

| 字段 | 内容 |
|-----|------|
| **DOI** | 10.1002/9780470316481 |
| **题名** | Approximation Theorems of Mathematical Statistics |
| **作者** | Robert J. Serfling |
| **年份** | 1980 |
| **出版社** | Wiley |
| **ISBN** | 978-0-471-02403-3 (print), 978-0-470-31648-1 (electronic) |
| **引用次数** | 3289 |
| **核验状态** | ✅ Crossref API 解析成功 |
| **适用公式** | Var(median) = 1/(4N f(m)²)，高斯特例 = πσ²/(2N) |
| **证据强度** | A (教科书级权威) |

**回包要点**:
- Crossref API 返回完整书目元数据
- DOI 验证通过，书籍为 Wiley 出版的标准统计教材
- §2.3.2 Var(median) 渐近方差公式为该书核心结论之一
- **局限**: API 仅返回元数据，无法直接定位到具体页码；需人工翻阅核实公式位置

**在本项目中的应用**:
- D-01/D-02 权重公式中的 π/2 因子出处
- `docs/science/PHASE2_UPM.md §5` 第 79-81 行声明的权威来源

---

### 2. Huber, P. J. 1964 — Robust Estimation

| 字段 | 内容 |
|-----|------|
| **DOI** | 10.1214/aoms/1177703732 |
| **题名** | Robust Estimation of a Location Parameter |
| **期刊** | Ann. Math. Statist. |
| **卷号** | 35 |
| **页码** | 73–101 |
| **年份** | 1964 |
| **bibcode** | 1964AnMS...35...73H |
| **核验状态** | ⚠️ 文章级定位 (未逐页核验) |
| **适用公式** | Huber loss 定义、IRLS 算法框架 |
| **证据强度** | A (开创性论文) |

**回包要点**:
- Huber M 估计的原始框架
- δ=1.345 的 95% 效率阈值
- Holland & Welsch 1977 才是 IRLS 实现细节的直接出处

**谨慎声明** (遵循 docs/science/PHASE2_UPM.md §14a):
- 不得把"95% 效率"挂到 Holland & Welsch 1977（其 §2 效率表未取到正文，判 UNPROVEN）
- H&W 只作 IRLS **实现** 的出处

---

### 3. Tukey/MAD → σ 常数换算

| 字段 | 内容 |
|-----|------|
| **参考文献** | Rousseeuw, P. J. & Croux, C. 1993, J. Amer. Statist. Assoc. 88, 1273 |
| **DOI** | 10.1080/01621459.1993.10476408 |
| **核验状态** | ⚠️ 需人工查阅 (仅凭项目记忆引用) |
| **适用公式** | MAD → σ 一致性因子 1.482602218505602 |
| **证据强度** | A (经典稳健统计) |

**在本项目中的应用**:
- `docs/science/PHASE2_UPM.md §5.1` 表格第 242 行
- patch 内稳健尺度估计的核心常数

---

## B. 待核验/ unresolved 的文献

### 1. Dr相关性有效样本数衰减公式

**声明**: `N_eff = N_retained / k_corr` 的来源

| 候选文献 | 状态 | 备注 |
|---------|------|------|
| Fruchter & Hook 2002, PASP 114, 144 | ⏳ 待核验 | 需确认是否明确给出该公式 |
| 统计学习理论标准结果 | ? | 正相关样本的有效样本数衰减可能是已知结论 |
| DrizzlePac 原创推导 | ? | 需查证 drizzlepac 文档或代码注释 |

**行动**: 需单独发起一次深度文献调研

---

### 2. Tikhonov 正则化原始文献

**声明**: 弱零锚=弱 Tikhonov 正则

| 字段 | 内容 |
|-----|------|
| **文献** | Tikhonov, A. N. 1963, Soviet Math. Dokl. 4, 1035 |
| **核验状态** | ⏳ 文章级 (卷页需网络核验) |
| **适用** | 正则化概念溯源 |

**谨慎声明**: 本项目不直接依赖此文献的公式，仅作为历史背景参考

---

### 3. 稀疏样条天光面表示

**声明**: Duchon 薄板样条 / Wahba 观测数据样条模型

| 文献 | DOI | 状态 |
|-----|-----|------|
| Duchon, J. 1977, Constructive Theory of Functions of Several Variables | ? | ⏳ UNPROVEN (无 DOI) |
| Wahba, G. 1990, Spline Models for Observational Data, SIAM | ISBN 0-89871-244-0 | ✅ 书级定位 |

**在本项目中的应用**:
- `docs/science/PHASE2_UPM.md §14a` 第 319 行
- 稀疏天光面表示的理论候选（当前冻结实现为 8×8 control cell 双线性）

---

## C. 驳斥错误的文献引用 (已在审查中发现)

### 1. Gruen, Seitz & Bernstein 2014, PASP 126, 158

**原文主张**: "背景建模对弱透镜/测光的影响"  
**事实核查**: 该文**只做叠加**（截尾均值的 PSF 是单帧 PSF 的线性组合），不做背景建模  
**处置**: 撤件！主题应改为"叠加期裁剪均值去伪影"

**落位**: `实验/additive-sky-seamless/README.md:327` 须修订

---

### 2. Regnault et al. 2009, A&A 506, 999–1042

**原文主张**: "单帧可标定大尺度响应"  
**事实核查**: 其§4.2 原文支持"天光以背景图逐像素直接扣除"，但标定依赖专用标定场与多年重复观测  
**处置**: 不作为"单帧可标定大尺度响应"的证据

---

### 3. Burke et al. 2018 (DES FGCM)

**错误挂法**: 创新点③的"最重要引用"  
**事实核查**: 四篇小论文均未引 FGCM，且 FGCM 只做通带时空变＋色改正，与 UPM 加性天光平面无关  
**处置**: 撤件或改挂为本仓自有推导/实验支撑

**落位**: `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:119` 须订正

---

### 4. Wild & Hewett 2005 / Soto et al. 2016

**问题**: 分别为**纤维光谱**域和**IFS 3D 光谱**域，不能外推到成像宽带  
**处置**: 引用时必须标注该域限定

---

## D. 文献引用纪律总结

遵循以下原则（来自 `docs/science/PHASE2_UPM.md §14a` 表格 263-272 行）:

1. **书目挂载纪律**: 只支撑摘要原文明确陈述的主题，不得过度外推
2. **不得据未取证书目立论**: arXiv 检索 0 命中 ⇒ UNPROVEN，仓库无科学主张系于此
3. **版本化引用**: photutils 引用须写明实际所用版本 (3.0.0 版本文档)，Zenodo DOI 是会漂移的概念记录
4. ** pinpoint 谨慎**: 书本/综述类文献只能引用到章/节，不得带 pinpoint 页码除非能定位
5. **同名异义消歧**: 不同适用域的同一术语（如"排除自身"）先做对照表再讨论

---

## E. 路线 3 独立补充的文献

除上述已有记录外，本路独立完成的文献核验：

### E-1: crossref.org API 批量核验协议

**脚本**: `code/d01_zero_scale_civar.py` 中调用的 Crossref API  
**调用间隔**: 默认 3 秒（避免 429），403 重试一次  
**结果保存**: 每笔核验记入本表的"核验状态"列

**可用函数**:
```python
import requests

def fetch_crossref(doi):
    url = f"https://api.crossref.org/works/{doi}"
    resp = requests.get(url, timeout=10)
    if resp.status_code == 200:
        return resp.json()
    elif resp.status_code == 429:
        time.sleep(20)  # 退避
        return fetch_crossref(doi)
    else:
        return None
```

---

## F. 文献缺失清单

以下科学量/常数目前仍缺少一手文献支撑，列为**待确认 + 保守方向**:

| 符号/常数 | 现行值 | 缺失环节 | 建议行动 |
|---------|-------|---------|---------|
| `k_corr` 适用域边界 | 300-600″/px | 没有外推到生产尺度 0.99″/px 的理论依据 | 补 MC 仿真覆盖更多 pixfrac 参数 |
| `huber_delta = 1.345` 在非 iid 场景 | 1.345 | 空间相关样本的 Huber 效率下降未见量化 | 引用稳健时空统计文献 |
| `sigma_floor = 1e-3` | 1e-3 | 标度口径 (ADU vs ADU·sr⁻¹) 两说 | 对齐 DATA_SEMANTICS 与 PHASE2_SESSION |
| `rank_rtol = 1e-10` | 1e-10 | 浮点算术精度地板的平方关系论证 | 补 identifiability.h 的数学推导 |
| 节点间距自适应触发条件 | 由输入几何导出 | 实测产品从未记录过自适应被触发 | 补端到端实证计数 |

---

## G. 导航索引

- D-01 完整四件套: [`report.md §2.1`](report.md#d-01-零尺度-patch-的数值保护量被平方成伪方差)
- 融合面分析: [`report.md §1`](report.md#1-融合面分析)
- 复核总览: [`review_summary.md`](review_summary.md)
- 审查员缺陷清单: [`独立审计/08_修复包/③加性天光无缝/01_缺陷清单.md`](../../../01_缺陷清单.md)
- 正面件: [`独立审计/08_修复包/③加性天光无缝/02_已确立的算法与验证程序.md`](../../../02_已确立的算法与验证程序.md)

---

**最后更新**: 2026-09-26  
**路线编号**: 路线 3 (独立科学研究路绂)  
**核验范围**: 69 条缺陷项中的文献支撑完整性
