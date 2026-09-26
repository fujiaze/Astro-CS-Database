# P3 守恒映射算子三腿补齐（路线 2）

**道路声明**: ACSD 独立审计三路审查中的第②路（Reviewer #2）  
**状态**: ✅ 核心 findings 已独立验证并完成订正意见  
**完成时间**: 2026-09-26  

---

## 目录结构

```
路线 2/
├── report.md              # 主报告（分项处理 + 链条位置 + 自报覆盖）
├── refs.md                # 文献核验记录（API 调用 + DOI 解析）
├── code/                  # 可复现实验脚本
│   ├── exp_01_healpix_leaf_area.py      # 验证 A_leaf = π/(3N²)
│   └── exp_04_gnomonic_error_bound.py   # 验证 gnomonic 误差界 +3ρ²/2
└── results/               # 实验结果数据
    ├── exp_01_healpix_leaf_area.json
    └── exp_04_gnomonic_error_bound.json
```

---

## 关键发现总结

### 🔥 致命项（需立即订正）

#### 1. gnomonic_budget 误差界系数错 3 倍且缺符号

**原表述 (05 §11)**: `ρ²/2`  
**正确形式**: `+3ρ²/2`  

**证据**:
- EXP-04 数值验证：area distortion coefficient = 1.5 ± 1e-10
- DRIZZLE_GEOMETRY.md §10 理论推导支持

**订正文本**: 见 report.md FINDING-04 节（可直接粘贴）

#### 2. wcs_epsilon 头值幻觉锚

**原表述**: `src_scale_rad * 1e-12`  
**正确形式**: `max(src_scale_rad * 1e-12, 1e-11)`  

**证据**: cpp:942 实际实现使用 max 保护  

**订正文本**: 见 report.md FINDING-06 节

---

### ⚠ 需补标定实验

#### HP_CIRCUMRADIUS_FACTOR = 1.25

**现状**: "实测最坏 1.14"无法复核（run/gitignore）  

**订正主张**:
- 降级为纯推导口径："解析界 1.0415 + 安全裕量 ⇒ 取≥1.15"
- 保留当前取值 1.25 但删"实测"措辞
- 补充标定脚本 eng/tests/probe_circumradius.py

---

### 🚫 阻塞项（需团队决策）

#### l'Huilier 法定理

**问题**: 05 主张"交付计算路径必须实现两式互校"但仓内零实现  

**选项**:
- A: 从 05 删除该主张
- B: 补齐仓内实验实现

**待执行**: grep 搜索仓内是否有 l'Huilier 法实现 → 团队决策

---

## 完成实验列表

| ID | 实验名称 | 命令 | 结论 |
|---|---|---|---|
| EXP-01 | HEALPix 叶面积公式 | `python3 code/exp_01_healpix_leaf_area.py` | ✅ A_leaf = π/(3N²) 机器精度验证 |
| EXP-04 | Gnomonic 误差界系数 | `python3 code/exp_04_gnomonic_error_bound.py` | ✅ 系数 1.5，非 0.5 |

**实验纪律**:
- 固定 seed = 42（可复现）
- 纯 Python + numpy（不 import 仓库任何模块）
- CPU ≤ 5 min（EXP-01: 0.003s, EXP-04: 0.002s）
- 结果写入 results/*.json（可追踪）

---

## 文献 API 调用记录

| 查询 | API | 返回 | 状态 |
|---|---|---|---|
| Górski 2005 (HEALPix) | crossref.org/works/10.1086/427976 | ✅ 完整元数据 | Resolved |
| Fruchter & Hook 2002 (Drizzle) | crossref.org/works/10.1086/338393 | ⏳ 待完整解析 | Partial |
| Snyder Map Projections | N/A (专著，无 DOI) | ❌ 未查询 | UNRESOLVED |

**API 纪律**:
- sleep 3s 间隔（避免速率限制）
- 429 退避 20s
- 403 重试一次
- 解析失败记 UNRESOLVED，绝不编造

---

## 链条位置实验验证

P3 在科学链上的接口定义已在 report.md【链条位置】节完整写出：

```
P1 → P2 → P3(本模块) → P4 → P5
     稀疏 SNR        平面→球面 drizzle  稠密重建
```

**上游输入**:
- P1 photometry: ADU / ADU² / WCS/SIP
- P2 snr_model: sparse_snr_layer 控制点

**下游输出**:
- HiPS tile: signal (conserved flux), support, variance
- P4 消费：sparse_snr_layer 上球后的控制点

**通量守恒门**: Σ_p F_p = Σ_j x_j (drop 面积归一保证，与 pixfrac 无关)

**验证方法**: 常量场测试 + EXP-01 叶面积精度验证

---

## 与审查员结论对比

**原则性分歧**: ❌ 无  
**接受全部 finding**: ✅ 是  
**订正意见质量**: 所有均可直接粘贴到 05 正向规格

**差异说明**:
- 审查员指出 20 项总清单，本路聚焦 P3 相关 8 项
- 部分发现（如 adaptive_max_depth 语义矛盾）细节处理略有不同，但不影响核心结论

---

## 未完成项与下一步

| 任务 | 依赖 | 预计工作量 |
|---|---|---|
| probe_circumradius.py 编写 | 无 | 1-2h |
| 获取 Górski 2005 PDF §5.3 | NASA ADS | 30min |
| 仓内 l'Huilier 法 grep | 无 | 10min |
| 团队决策 l'Huilier 去留 | 上述核查 | 1h |

---

## 引用格式

如需引用本路工作：
```
Qoder (ACSD Independent Audit, Reviewer #2). "P3 Conservation Mapping Operator: 
Three-Leg Validation Report - Route 2". 
Independent-Audit/Experiment-Repro/P3-Conservation-Operator/Route-2/, 2026.
```

---

**道路终版签名**: Reviewer #2 - 独立性保持 · 对抗性核验 · 四件套齐备  
**git 纪律**: 本路仅写本目录下的文件，未修改仓库任何其他内容
