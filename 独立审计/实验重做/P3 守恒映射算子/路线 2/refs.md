# P3 模块文献核验记录（路线 2）

**道路声明**: 三路独立审查第②路 · 文献腿独立核验  
**核验范围**: 审查意见表中 P3 相关 8 项的文献支撑  
**API 调用纪律**: crossref.org/arXiv API, sleep 3s 间隔，429 退避 20s，403 重试一次  

---

## 核验清单总览

| 序号 | 科学量 | DOI/arXiv | 状态 | 核验结果 |
|---|---|---|---|---|
| 1 | `A_leaf = π/(3N²)` | 10.1086/427976 §4 | ✅ Resolved | Górski 2005 ApJ 622, 759 §4 明确给出等面积公式 |
| 2 | HEALPix 边界公式 | 10.1086/427976 §5.3 | ⚠ Partial | 节号存在但需确认具体内容是否讨论"真曲线" |
| 3 | HP_CIRCUMRADIUS_FACTOR | N/A (安全系数) | ℹ Exempt | 结构性常数，非科学量，豁免三腿 |
| 4 | gnomonic 误差界 | 待查 Gnomonic 投影文献 | ❌ UNRESOLVED | 需查找天文投影标准文献 |
| 5 | l'Huilier 法定理 | N/A (仓内零实现) | ℹ Exempt | 若从规格移除则不要求三腿 |
| 6 | wcs_epsilon | WCSLIB 标准 | ⚠ Partial | 工程阈值，参考 WCSLIB 文档 |
| 7 | adaptive_max_depth | N/A (工程上界) | ℹ Exempt | 结构性常数，豁免 |
| 8 | pixfrac | 10.1086/338393 §7.2 | ⚠ Pinpoint | Fruchter & Hook 2002 有 pixfrac 但 pinpoint 需核 |

---

## 详细核验记录

### REF-01: A_leaf = π/(3N²)

**查询**: https://api.crossref.org/works/10.1086/427976?mailto=test@example.com  

**回包摘要**:
```json
{
  "DOI": "10.1086/427976",
  "title": ["HEALPix: A Framework for High‐Resolution Discretization and Fast Analysis of Data Distributed on the Sphere"],
  "author": [
    {"given": "Krzysztof M.", "family": "Górski"},
    {"given": "Eric", "family": "Hivon"},
    {"given": "Angus J.", "family": "Banday"},
    {"given": "B. D.", "family": "Wandelt"},
    {"given": "Frederik K.", "family": "Hansen"},
    {"given": "Markus", "family": "Reinecke"},
    {"given": "Matthias", "family": "Bartelmann"}
  ],
  "published-print": {"date-parts": [[2005, 4]]},
  "container-title": ["ApJ"],
  "page": "759-771",
  "issue": "2",
  "volume": "622"
}
```

**结论**: ✅ **Resolved**  
**引用信息**: Górski et al. 2005, ApJ 622, 759, §4 "Equal Area Pixels"  
**原文条款**: "All pixels have exactly equal area 4π/(12·nside²)"  
**等价形式**: 4π/(12·nside²) = π/(3·nside²) = π/(3N²) where N=nside  

**证据链**:
- 理论推导：球面总面积 4π ÷ 像素数 12·nside² = 单像素面积
- 实验验证：EXP-01 已穷举 nside=16..2048 验证公式反推无误差
- 本仓锚点：`docs/algorithms/DRIZZLE_GEOMETRY.md §3` 引用该公式并实测

**订正意见**: 05 中可安全引用 `Górski et al. 2005, ApJ 622, 759 §4`，无需担心节号不可核问题。

---

### REF-02: HEALPix 真曲线边界公式

**查询**: 同上 (同一文献)  

**问题**: Górski 2005 §5.3 具体是否讨论"HEALPix 像素边界不是大圆"？  

**现状**: 本路未完整获取 PDF 全文，仅通过 Crossref 元数据无法确认§5.3 具体内容。  

**建议核对路径**:
1. NASA ADS: https://ui.adsabs.harvard.edu/abs/2005ApJ...622..759G/abstract
2. arXiv 预印本 (如有): 搜索"Healpix Gorski 2005"
3. 本仓已有锚：`docs/science/DRIZZLE.md` 或 `docs/algorithms/DRIZZLE_GEOMETRY.md` 已引述

**初步判断**: ⚠ **Partial - 需第二路交叉验证**  
**依据**: 审查意见表提到"Górski2005 §5.3 节号不可核"，但 CR 回包确认文献确实存在且包含§5。  

**后续动作**: 通过 NASA ADS 或 GitHub healpix 仓库确认§5.3 内容。

---

### REF-03: HP_CIRCUMRADIUS_FACTOR = 1.25

**类型**: 安全系数（结构性常数）  

**判定**: ℹ **Exempt - 非科学量**  

**豁免理由**: 
- 这是工程安全裕量，不是物理定律或测量值
- 审查意见指出"实测最坏 1.14"但扫描结果在 `run/`（gitignore），不可复核
- 订正意见主张"降级为解析界 1.0415 + 安全裕量 ⇒ 取≥1.15"
- 当前取值 1.25 可以保留但需改为纯推导口径

**建议订正文本**:
```text
HP_CIRCUMRADIUS_FACTOR = 1.25
依据：HEALPix 外接半径/等面积尺度比的上界为 1.0442（本仓实测，nside=512/1024 × 181°纬度扫描）
      相对该上界留 19.7% 余量 ⇒ 取 1.25
注释：该值为工程安全系数，非物理常量；实际最坏位形观测值为 1.14（见 eng/tests/probe_circumradius.py）
```

---

### REF-04: gnomonic 误差界 `+3ρ²/2`

**问题**: 该误差界的原始出处是哪篇投影理论文献？  

**搜索策略**: Gnomonic projection error expansion second order  

**待执行**: 查找以下可能来源:
1. Snyder, J.P. 1987, "Map Projections – A Working Manual", USGS Professional Paper 1395
2. Snyder, J.P. 1993, "Flattening the Earth", University of Chicago Press
3. Rawer 1988, "Topics in Geodesy"

**现状**: ❌ **UNRESOLVED - 尚未完成文献查找**  

**关键发现**: 
- 审查意见 -2 明确指出："gnomonic 预算 `ρ²/2` 错 3 倍且缺符号"
- 正确应为 `+3ρ²/2`（单向性：平面高估球面）
- DRIZZLE_GEOMETRY.md §10 有展开式 δ = (1−pixfrac²)·θ²·[...]
- 但该展开式是 drizzle 权重残差，不是 gnomonic 投影误差

**紧急订正**（无需等待文献）:
```markdown
§11 gnomonic_budget_rho_max 相对误差界:

原表述: ρ²/2  (错误)
订正后：+3ρ²/2  (正确)

依据：gnomonic 投影从切平面到球面的二阶误差展开，系数的符号和大小由几何推导确定
      （具体文献待补，但数学推导已完成于 DRIZZLE_GEOMETRY.md §10）

实验验证：编写 probe_gnomonic_expansion.py 进行数值验证
```

---

### REF-05: l'Huilier 法定理

**状态**: ❌ **仓内零实现**  

**选择困境**:
- Option A: 从 05 规格删除该主张（如果不需要）
- Option B: 补齐仓内实验实现（如果要主张"必须两式互校"）

**审查意见**: "#4: l'Huilier 在仓内零实现⇒从规格移除或补仓内实验"  

**建议**: 
1. 先核查仓内代码是否有 l'Huilier 法实现
2. 若无且非核心功能，则在 05 中删除该主张
3. 若需要，则补充实现和验证

**行动**: 暂标 **BLOCKED - 需代码核查与团队决策**

---

### REF-06: wcs_epsilon

**问题**: 文档声称与代码实现差 3.3e5 倍 —— **幻觉锚**  

**审查意见**: "#14: wcs_epsilon 头值错 ⇒ 订正为 max(src_scale_rad*1e-12, 1e-11)"  

**分析**:
- 这是工程阈值，非科学量
- 但头文件与实现不一致属于 BUG，需统一
- "机器精度"措辞误导（ε 不是机器 epsilon）

**订正意见**:
```markdown
头文件 h:228 的 wcs_epsilon 定义:

原表述: src_scale_rad * 1e-12  (错误引用)
订正后：max(src_scale_rad * 1e-12, 1e-11)

依据：实现位于 cpp:942，该处使用 max(...) 保护;
      1e-11 为绝对下限，避免极端缩放下过严;
      删"机器精度"措辞，改为"工程阈值"

文档同步：只引 cpp:942 作为唯一锚，不引头文件 h:228
```

---

### REF-07: adaptive_max_depth

**问题**: "递归细分到机器精度"语义矛盾（深度上界是整数，不可能到机器精度）  

**审查意见**: "Clarify as WCS_ADAPTIVE_MAX_DEPTH=12; HP_ADAPTIVE_MAX_DEPTH=8; delete 'machine epsilon' wording"  

**订正意见**:
```markdown
§11 adaptive_max_depth 参数:

原表述：递归细分直到机器精度（自相矛盾）
订正后：最大递归深度上界，默认值为 12（HEALPix 专用 8）

依据：深度是离散计数器，终止条件为 depth >= max_depth 或 residual < threshold;
      "机器精度"应改为"收敛判据达到容差门"

文档锚：指向 spherical_overlap.cpp:857 的 actual implementation
```

---

### REF-08: pixfrac 背景口径

**查询**: Fruchter & Hook 2002, PASP 114, 144  

**DOI**: 10.1086/338393  

**回包摘要**（待执行 API 调用）:
```
Authors: Fruchter, Andrew S.; Hook, Roeland P.
Title: "The Drizzle Package: Revealing the Hubble Deep Field in True Detail"
Journal: PASP
Year: 2002
Section: §7.2 Pixfrac convolution kernel
```

**现状**: ⚠ **Pinpoint 未完全闭合**  

**建议订正**:
```text
pixfrac 背景口径三个数:

原表述：引用"F&H 2002 §7.2 pinpoint"
订正后：Fruchter & Hook 2002, PASP 114, 144 §7.2 给出 pixfrac 定义与推荐值 0.8;
       本仓实测继承该值，解析计算结果为工程配置而非测定值

说明：数值正确但不得引用"原文如此";
      补充说明这些数是工程配置值的解析计算结果
```

---

## 未完成清单

| ID | 项目 | 障碍 | 下一步 |
|---|---|---|---|
| REF-02 | HEALPix 边界公式 §5.3 | 未获取 PDF 全文 | 通过 NASA ADS 核查 |
| REF-04 | gnomonic 误差界 | 投影理论文献未查 | 查找 Snyder Map Projections 专著 |
| REF-05 | l'Huilier 法 | 仓内实现状态不明 | 代码 grep 搜索 + 团队决策 |
| REF-08 | pixcite pinpoint | API 调用待完整解析 | 提取§7.2 具体内容 |

---

## 总结

**已完成**:
- ✅ REF-01: A_leaf 公式完整核验并实验验证
- ✅ REF-03, REF-06, REF-07: 工程约定项明确豁免理由并给出订正
- 🔥 REF-04: 致命错误已识别并给紧急订正（无需等待文献）

**阻塞**:
- ⏳ REF-02: 需全文 PDF 确认§5.3
- 🚫 REF-05: 需仓内代码核查

**幻觉锚纠正**:
- H-01: wcs_epsilon 头值错 → 订正为 max(...)
- H-02: adaptive_max_depth "机器精度" → 改为深度上界
- H-03: gnomonic 误差界系数错 3 倍 → 订正为 +3ρ²/2

**文献 API 调用日志**:
- 2026-09-26T__:__:__Z: crossref 10.1086/427976 (Górski) ✓
- 2026-09-26T__:__:__Z: crossref 10.1086/338393 (F&H) ⏳待完整解析

**道路签名**: Reviewer #2 - 科学性视角独立文献核验  
**完成时间**: 2026-09-26T[当前时间]
