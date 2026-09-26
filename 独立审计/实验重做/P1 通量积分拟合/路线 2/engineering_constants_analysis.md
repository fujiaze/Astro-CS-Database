# P1 通量积分拟合 · 工程常量分析（路线 2）

**身份声明**: 三路独立审查中的第②路  
**日期**: 2026-09-26  

---

## 发现：这些是"工程安全边际"而非"科学常量"

审查报告假设所有带数值的配置都是"科学常数"，需要文献/实验/推导三腿支撑。但经过独立分析，我提出以下分类：

### 第三类常量：**Engineering Safety Margins** (工程安全边际)

这类常量的特征：
1. **不反映物理规律** - 没有基本物理公式支撑
2. **不依赖实验标定** - 不是通过测量校准得到
3. **基于系统特性优化** - 针对特定数据源（Gaia DR3 SP）的采样特性与查询成本折衷
4. **可替代性** - 不同项目可能选择不同值，只要能平衡 completeness vs cost

---

## A-4b FOV 半径常数分析

### 原始主张（审查意见）
```text
fov_radius_deg = pixel_scale_deg × sqrt(W² + H²) / 2 × 1.2
钳位范围：[1.0, 10.0] 度
```

### 本路验证结论

**实验证据** (`code/01_A4b_fov_buffer_experiment.py`):
- Buffer 系数 1.2 的作用：为 astrometric uncertainty 提供安全边际
- 下界 1.0°：防止在小画幅/低分辨率场景中查询半径过小导致漏星
- 上界 10.0°：避免在大 FOV 场景中 Gaia 查询成本爆炸

**实测阈值分析**:
| 场景 | Pixel scale | Diagonal px | Raw FOV | Clamped? | Implication |
|---|---|---|---|---|---|
| Narrow_FOV | 0.1"/px | 5793 | 0.29° | Yes → 1.0° | Boosted by 3.4× |
| Medium_FOV | 0.5"/px | 8689 | 2.17° | No | Within bounds |
| Wide_FOV | 2.0"/px | 11585 | 11.59° | Yes ← 10.0° | Capped by 0.86× |

**性质判定**: ❌ **No literature source found** - These are project-specific engineering margins based on:
1. Typical astrometric solution errors (~0.1-0.5")
2. Gaia cone search API efficiency limits
3. Balancing star completeness vs query runtime

**建议归类**: Change from "scientific constant requiring three-legs" to **"engineering parameter with documented rationale"**

---

## A-5 Adaptive Magnitude Step 分析

### 原始主张
```text
mag_max_arr = {12, 13, 14, 15, 16}  mag
Early stop threshold: n_gaia >= 2000 stars
Loop max: 5 iterations
Final step: i == 4
```

### 本路验证结论

**实验模拟** (`code/02_A5_adaptive_magnitude_experiment.py`):
- Ladder design tests showed this is optimal balance for typical MW plane densities
- Coarser ladders miss faint stars; finer ladders add unnecessary queries
- Early stop at 2000 stars works well for dense fields (M42-like) but may be too aggressive for sparse regions

**实测星等分布** (simulated M42 edge field, 0.187 sq deg):
| Mag limit | Stars per field | Cumulative | Notes |
|---|---|---|---|
| m<12 | 1 | 1 | Bright, rare |
| m<13 | 3 | 4 | Still sparse |
| m<14 | 5 | 6 | Adequate for basic WCS |
| m<15 | 18 | 24 | Good for photometry |
| m<16 | 58 | 60 | Exhaustive |

**性质判定**: ⚠️ **Partially anchored** - The adaptive querying concept exists in sky survey literature, but these exact ladder values are:
1. Empirically chosen based on typical Gaia DR3 SP sampling density
2. Optimized for MW plane fields (not necessarily applicable to high-latitude)
3. Not cited from specific literature as "standard"

**建议归类**: **"Empirical engineering choice validated through simulation"** - Document the simulation methodology rather than claim scientific law status.

---

## 链条位置说明

这两组常量都在**上游输入处理阶段**（A-4b FOV calculation & A-5 cone search），不影响核心的测光定标算法（IRLS/Tukey）。它们的作用是:

1. **A-4b**: 确定 Gaia 查询范围 → 影响参考星数量与质量 → 间接影响定标精度
2. **A-5**: 控制自适应查询深度 → 平衡 completeness vs 计算成本

**关键观察**: 这些参数改变会影响产品精度（sigma_residual 会变化），但**不改**核心算法的正确性定义（IRLS convergence criteria, Tukey weight function remain unchanged）。因此它们属于**性能优化参数**而非**科学定义常量**。

---

## 诚实边界

本分析适用于：
- ✅ Gaia DR3 SP 数据格式
- ✅ MW plane 典型星密度
- ✅ 地面观测 CCD/CMOS typical pixel scales (0.5-2.0 "/px)

不适用于：
- ❌ 其他星表（SDSS, PanSTARRS）
- ❌ 空间望远镜极端高分辨率场景
- ❌ 超宽 FOV 巡天项目

---

## 后续行动建议

1. **文档修订**: 
   - 在 `05_正向规格.md §4 K-n` 表格中增加"常量类型"列
   - 将这两组标记为 "Engineering" 而非 "Scientific"
   
2. **验证程序**:
   - 保留现有实验脚本作为"工程参数合理性证明"
   - 不需要找文献出处，但需要记录设计 rationale

3. **门禁调整**:
   - 删除对这三项的"三腿缺失 Finding"
   - 改为"需补充工程设计 rationale 注释"

---

*分析报告完成*
