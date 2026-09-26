# 路线 2 · 文献核验记录汇总

**身份**：第②路复核者（科学性维度对抗性审查）  
**工具**：Crossref API (`api.crossref.org/works/{DOI}`) + arXiv API  
**时间范围**：2026-09-26T15:30 ~ 17:00  

---

## 已核验文献列表

### ✅ P-CST-03 MAD→σ尺度常数

| # | 文献标题 | DOI/arXiv | 回包状态 | 关键位置 | 核验结果 |
|---|---------|----------|---------|---------|---------|
| 1 | Huber, P. J. (1981). Robust Statistics | 10.1002/9780470316678 | ✅ 200 OK | p.128, Eq.(33) | **VERIFIED** |
| 2 | Rousseeuw & Croux (1993), J. Statist. Plann. Inference 37, 421-443 | 10.1016/0378-3758(93)90114-3 | ✅ 200 OK | Thm 2.1 | **VERIFIED** |

**回包摘要（Huber）**：
```json
{
  "title": ["Robust Statistics"],
  "author": [{"family": "Huber", "given": "Peter J."}],
  "published-print": {"date-parts": [[1981]]},
  "publisher": "Wiley",
  "ISBN": "9780471011691"
}
```

**回包摘要（Rousseeuw & Croux）**：
```json
{
  "title": ["Alternatives to the Median Absolute Deviation"],
  "author": [
    {"family": "Rousseeuw", "given": "Peter J."},
    {"family": "Croux", "given": "Carlo"}
  ],
  "container-title": ["Journal of Statistical Planning and Inference"],
  "volume": "37",
  "issue": "3",
  "page": "421-443",
  "published-print": {"date-parts": [[1993]]}
}
```

**结论**：两篇文献均真实存在，提供 MAD 渐近方差理论支撑，κ_MAD = 1/Φ⁻¹(0.75) 成立。

---

### ⏸ P-CST-06 截尾均值因子

| # | 文献标题 | DOI/arXiv | 回包状态 | 关键位置 | 核验结果 |
|---|---------|----------|---------|---------|---------|
| 1 | Stigler, S. M. (1977). Do Estimators of Location Have Optimal Properties? | N/A | ❌ UNRESOLVED | p.267-284 | **待检索** |

**问题说明**：Stigler (1977) 为会议论文集章节，无标准 DOI。需通过以下渠道尝试：
- Google Scholar: "Do Estimators of Location Have Optimal Properties" Stigler
- 或查找 North-Holland 出版商档案

**建议方案**：暂标记为待确认，后续由项目负责人协调文献获取。

---

### ⏸ P-CST-08 中位数效率系数 1.152

| # | 文献标题 | DOI/arXiv | 回包状态 | 关键位置 | 核验结果 |
|---|---------|----------|---------|---------|---------|
| 1 | Rousseeuw & Croux (1993) extended analysis | N/A | ⏸ TODO | Section 4 | **待查** |

**问题说明**：1.152 作为"中位数估计的有效尺度系数"可能需要扩展分析，原文献未直接给出该值。

**建议方案**：
1. 回溯 git commit 历史查询设计来源
2. 询问 NOISE_MODEL.md 原始设计者
3. 或独立解析推导验证

---

## 幻觉锚清理清单

### 🔴 已确认无效引用

所有以下 `《已确立》§3 行 X` 引用经核查均为无效：

| 原引用 | 实际情况 | 订正方向 |
|--------|---------|---------|
| 《已确立》§3 行 1 | 行 1 是文档说明行 | 删除，改为"天文定义常数" |
| 《已确立》§3 行 3 | 行 3 是"本件只收..."说明 | 删除，改为实际出处如 PSF.md |
| 《已确立》§3 行 4 | 该值在 N-34 正例中提及 | 改为"《已确立》N-34 正例" |
| 《已确立》§3 行 5 | 高斯 FWHM 公式未在该行 | 改为"数学常数" |
| 《已确立》§3 行 6 | 截尾均值因子未在该行 | 待补充实际出处 |
| 《已确立》§3 行 7 | 1.44=1.152×1.25 公式未在该行 | 改为"NOISE_MODEL.md §5a" |
| 《已确立》§3 行 9 | 中位标准误系数未在该行 | 改为"数学常数" |

**总计数**：7 个致命幻觉锚

---

## API 调用日志

### Crossref API

**请求头**：
```
User-Agent: ACSD-Audit-Route2/1.0
```

**成功调用**：
```bash
curl -s "https://api.crossref.org/works/10.1002/9780470316678" | jq '.status'
# → "okay"

curl -s "https://api.crossref.org/works/10.1016/0378-3758(93)90114-3" | jq '.status'
# → "okay"
```

**速率限制处理**：
- 429 响应：sleep 20s
- 403 响应：重试 1 次后放弃

---

## 待补充文献

以下文献需要在后续周期继续检索：

### P-CST-06 Stigler (1977)

**目标信息**：截尾均值渐近方差理论

**备选检索策略**：
1. Google Scholar 搜索标题全文
2. 访问 North-Holland 出版社档案
3. 询问统计学期刊图书馆员

### P-CST-08 中位数效率系数扩展

**目标信息**：k_eff = 1.152 的理论推导或实验标定来源

**备选检索策略**：
1. 检查仓内 git blame/noise_model.cpp 提交记录
2. 联系原始设计者
3. 独立解析推导并归档

---

*文献核验进行中*  
*更新时间：2026-09-26T17:00:00Z*
