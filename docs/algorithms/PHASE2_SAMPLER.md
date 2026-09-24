# Phase2 Sampler Algorithms（P2-SAMP / astrocs.p2.sampling）

> ID: ALG-P2-SMP-001  状态: CONTRACT_READY。本文件是 Phase2 控制点采样模块（control sampler）
> 算法层**唯一权威**：逐公式源码行号锚定 + 冻结容差 + 实现偏差登记。
> 上游：ASTROCS_DESIGN.md §5.4（天光平面与统一相对模型）

> 上游 SCI: SCI-UPM-001（docs/science/PHASE2_UPM.md，FROZEN，共享引用
> 不改动；页头明示"模块: phase2 (upm/sampler)"；
> descriptor 占位 SCI-P2-SMP-001⇒SCI-UPM-001 映射声明见 §11.4）。
> 辅助 SCI: SCI-UPM-WEIGHT-001（control_variance 公式权威）、
> SCI-NOISE-001（robust 统计量的科学定义上游）、SCI-SCOPE-001
> §处理链第 5 步"控制采样"（链位置）。
> 关联 ALG: ALG-UPM-CONTROL-IVAR-001（本文件 §5.4 冻结承接，见 §12）；
> ALG-UPM-001（UPM 拟合，下游消费方）。
> 模块: lib/algorithms/coverage/src/sampler.cpp（1536 行）+ 唯一权威签名头
> lib/algorithms/coverage/include/astro/phase2/sampler.h（288 行，实测）；
> DATA: DATA-P2-SMP（DATA_SEMANTICS §23）；API: API-P2-SMP-001
> （PUBLIC_API.md）；MOD: astrocs.p2.sampling（合同三件套
> lib/algorithms/sampling/，迁移目标 astrocs_p2_sampling.dll 为矩阵合同值
> 尚未存在，由 P2-SAMP-IMPL 建立，禁止声明 IMPLEMENTED）。

## 1 目的与非目标

**目的**（SCI-UPM-001 §1/§3 承接）：在 coverage union 天区 Ω 内按
几何规则布置稀疏球面光度控制点（与 SNR 几何解耦），并从实际 Phase1
HiPS 数据读取每个控制点的 background-clean patch 观测
y_ik/σ_ik/snr_ik/support_ik/quality_ik，产出 UPM 联合加性校准的
唯一输入（P2ControlObservation 流 + 全几何节点 + 统计账）。

**非目标（本模块不做）**：

- 不做 UPM 拟合/权重归一/表面求解（ALG-UPM-001，P2-UPM 域）；
- 不做星点检测（SNR 来自 Phase1 SNR Catalogue，禁止重新检测，
  sampler.h:11-12 语义冻结）；
- 不做 coverage union 计算（上游 DATA-P2-COV，P2-COV 域）；
- 不产出 per-pixel/per-leaf 科学场产品（稀疏控制点观测集才是输出）；
- 不解释 ivar 产品的科学权重语义（obs.ivar 仅诊断，弃用不进科学
  权重，upm.h:38-41 冻结）。

## 2 符号与单位（权威=本表 + DATA_SEMANTICS §23）

| 符号 | 含义 | 单位/域 | 实现锚 |
|---|---|---|---|
| Ω | coverage union（上游 MOC） | — | coverage.h（P2-COV 域） |
| tile_ipix | union tile 的 NESTED 像素号（order=target_order） | 无量纲 | coverage.h P2CoverageCell.ipix |
| grid=G | 每 tile 的 cell 网格边长（默认 8） | 无量纲 | sampler.cpp:303/:509 |
| cell_side | tile 边长/G=64（kTileWidth=512） | leaf 像素 | sampler.cpp:76/:630 |
| cell (t,gx,gy) | 控制点拓扑 = tile × 网格坐标 | — | sampler.cpp:771-780 |
| leaf_ipix | cell 中心 leaf 像素（order+9） | 无量纲 | sampler.cpp:773-774 |
| y_ik | 控制观测（patch 位置估计） | **面亮度 ADU·sr⁻¹**（与上游 Phase1 HiPS signal 层同标度；帧间标度差由 PHOTAPPL/PHOTSCAL 承载） | sampler.cpp:818-822/:870 |
| σ_bg | patch 内样本的稳健尺度（MAD×1.482602218505602）；**量的是 patch 样本离散度**，含结构分量 | ADU·sr⁻¹（同 y_ik 标度） | sampler.cpp:837-864 |
| k_corr | Drizzle 输出协方差导致的 control estimator 方差放大因子；定义域 **1 < k_corr**（无量纲） | 无量纲 | sampler.cpp:83/:874 |
| N_retained | clipping 后保留样本数；域 [min_samples, (2·background_patch_radius+1)²]，默认 [5, 289] | 无量纲 | sampler.cpp:868 |
| control_variance | 控制点估计统计方差 | (ADU·sr⁻¹)² | sampler.cpp:875 |
| control_ivar | 1/control_variance | (ADU·sr⁻¹)⁻² | sampler.cpp:877 |
| snr_available | 局部星点存在性标志 | 0/1 | sampler.cpp:893/:1056 |
| reason | 内部拒绝原因 0..5 | 无量纲 | sampler.cpp:642/:776/:1012-1022 |

**单位约束（正向）**：value / uncertainty / σ_bg / y_ik = 面亮度 **ADU·sr⁻¹**；
control_variance = **(ADU·sr⁻¹)²**；control_ivar = **(ADU·sr⁻¹)⁻²**；
ra_deg / dec_deg = 度（J2000）；snr / support / quality_flags = 无量纲；
连续数学定义见 §5，dtype/shape 唯一权威=DATA_SEMANTICS §23。

**适用域与证据**（本条是上表的量纲依据，不是约定）：

- 上游 Phase1 HiPS signal 层的量纲由写盘门冻结为面亮度族：合法 BUNIT 集 =
  {`ADU/sr`, `ADU^2/sr^2`, `sr^2/ADU^2`}，裸 `ADU` 判红（rc=-2）——
  `lib/infrastructure/aio/src/hiss_writer.cpp:335-365`（其依据注释 `:319-334`
  指向 `docs/contracts/DATA_SEMANTICS.md` §31.1a:2808-2810 / :2827-2830：全链
  signal 承载的物理量是面亮度；帧间标度由 PHOTAPPL/PHOTSCAL 承载，标度 ≠ 量纲类别）；
- 本模块逐 leaf 直读该层 tile 值、**不做任何面积乘除**（`sampler.cpp:818-822`），
  故 value / σ_bg 的量纲与该层一致；
- 面积换算只发生在下游写盘前：`area = support×A_cell`、`flux = signal×area`
  （`lib/algorithms/coverage/tools/stage2.cpp:1149-1152` / `:1368-1369`），
  即 signal 是**单位面积通量**；
- 实测闭环（本仓 `run/SCI-FIX-PHASE2-01/real/r2_m42_rawframe.py`，证据
  `run/SCI-FIX-PHASE2-01/evidence/r2_m42_raw.json:R2e_unit_closure`）：真实 M42
  300 s R 帧（raw 像素尺度 0.9890″/px，Ω_px = 2.2991e-11 sr）上 4096 个 17×17 patch
  实测 σ_bg 中位 = 14.826 ADU/px、N_retained 中位 = 287，按 §5.4 公式得
  control_variance = 1.684 (ADU/px)²、control_ivar = 0.5937 (ADU/px)⁻²；按 ADU·sr⁻¹
  标度换算（因子 (1/Ω_px)² = 1.8919e21）得 control_ivar = **3.14e-22**，与生产代码
  实测记录 control_ivar 中位 **5.6e-22**（`lib/algorithms/coverage/src/upm.cpp:644-646`）
  同量级（1.78×）。**若单位为裸 ADU，推出值应为 0.594，与生产实测相差 22 dex** ⇒
  单位是 ADU·sr⁻¹，不是 ADU。
- **退化条件**：把本表的 ADU·sr⁻¹ 当裸 ADU 消费（例如与 ADU 域孔径测光量直接
  相加/相除）会使标度类消费面整体偏 Ω_px 的幂次倍；本模块输出**不得**在未声明
  标度换算的前提下与 ADU 域量混用。

## 3 逐符号锚（sampler.cpp 1536 行 / sampler.h 288 行，实测）

**导出符号（sampler.h 声明 / sampler.cpp 实现）**：

| 符号 | 声明（sampler.h） | 实现（sampler.cpp） | 语义 |
|---|---|---|---|
| p2_sampler_default_config | :66 | :294-312 | 默认配置单一来源（15 字段） |
| p2_frame_id | :99 | :314-438 | truncated-64 canonical SHA-256 帧身份 |
| p2_stats_median | :103 | :450-457 | 共享 median（NaN 过滤） |
| p2_stats_mad | :104-105 | :449-461 | 共享 MAD×1.482602218505602（out_median 回传） |
| p2_sample_controls | :103-114 | :1236-1251 | 采样入口（frame_id 内部计算） |
| p2_sample_controls_cached | :156-168 | :1255-1271 | 采样入口（外部透传 frame_id 缓存） |

**内部符号（匿名/静态）**：

| 符号 | 锚（sampler.cpp） | 语义 |
|---|---|---|
| kTileWidth/kTileShift | :75-76 | 512×512 tile，shift=log2(512)=9 |
| kSnrCatalogMax | :78 | SNR catalogue 上限 65536 |
| kControlCorrDefault | :83 | k_corr 冻结保守默认 1.4 |
| kPiHalf | :84 | π/2 常数（UPMW-004 中位数方差） |
| kcorr_lookup | :88-109 | pixfrac×scale 双线性标定表 |
| frame_drizzle_provenance | :112-137 | 帧 properties 解析 pixfrac/scale |
| read_tile_pair | :163-180 | signal+support tile 成对读（g_aio_mu :161 串行化） |
| median_of | :191-204 | nth_element median（偶数取 [begin,mid) 最大值均值；P0-01 修复） |
| SnrIndex::build/query/any_above | :206-288 | dec 排序索引 + RA 保守窗口 + 精确角距 |
| p2_sample_controls_impl | :463-1119 | 三阶段采样管线本体 |
| CellStat | :634-647 | 每 (cell,frame) 候选统计载体 |

**配置面（P2SamplerConfig，sampler.h:33-62，声明注释 ：31）**：15 字段默认值见
§3 表 p2_sampler_default_config 实现行；`control_grid_per_tile=8`/
`patch_radius_leaf=2`/`min_samples=5`/`snr_search_radius_deg=0.05`/
`background_patch_radius=8`/`background_clip_sigma=3.0`/
`background_clip_iters=3`/`background_max_contamination=0.20`/
`background_contamination_sigma=3.0`/
`background_min_retained_fraction=0.60`/`background_tolerance=3.0`/
`background_neighbor_radius=2`/`background_catalog_veto=1`/
`control_k_corr=1.4`/`cpu_workers=1`（sampler.cpp:303-317）。
显式 cfg 覆盖路径：sampler.cpp:501（`if (cfg_in) cfg = *cfg_in;`）。

## 4 算法结构：三阶段 background-clean 采样管线

sampler.cpp:614-620 冻结注释将管线映射为
BACKGROUND_SAMPLER_SPEC.md Stage A-E；实现按三遍组织：

| 阶段 | 遍 | 锚（sampler.cpp） | 语义 |
|---|---|---|---|
| 预备 | 0 | :485-502 配置修补；:512-523 frame_id 0 拒绝；:529-546 帧打开；:547-555 per-frame k_corr；:562-578 SNR catalogue 读入；:579-581 ivar 产品（可缺） | 输入装配 |
| Stage A+B（第一遍） | 1 | :699-934 pass1_cell（lambda :699-878）+ worker 池/串行（:886-934）：:702-714 越界 tile 占位；:715-717 覆盖帧收集；:719-733 tile pair 读；:735-791 每 cell patch 收集/过滤；:793-800 min_samples 拒绝；:802-833 亮端迭代 clipping（Stage B）；:838-844 cvar 组装；:847-867 SNR 邻域+catalog veto（Stage E） | 候选统计 |
| Stage C+D（第二遍） | 2 | :952-1009 同 tile 邻域 tolerance gate（C）+contamination/retained gate（D） | 局部门控 |
| 输出（第三遍） | 3 | :1011-1063 ≥2 clean 帧 control 输出 + obs 组装 | 观测流 |
| 收尾 | — | :1071-1088 容量上限+stats 补偿；:1089-1097 异常兜底；:1098-1119 输出拷贝 | 账目 |

## 5 逐公式定义（算法级，与 SCI-UPM-001 §5/§11 同构；单位见 §2）

### 5.1 坐标/tile 映射（matrix 专项 1）

控制点几何**只由 union 几何与目标角间距决定**（sampler.h:6 语义
冻结；实现 :729-736）：

```text
cell_side   = kTileWidth / G                                  # :595, =64
cx          = gx * cell_side + cell_side/2                    # :737
cy          = gy * cell_side + cell_side/2                    # :738
center_local= xy_to_nested_local(cx, cy, kTileShift=9)        # :739
center_leaf = leaf_of_tile(tile_ipix, 9) + center_local       # :740
(ra,dec)    = pix2ang_nest(1<<(target_order+9), center_leaf)  # :742
cell 索引   = c*G² + gy*G + gx                                # :708-709/:870-871
```

- **cell 网格的角尺度（量纲与适用域，正向约束）**：`G = control_grid_per_tile` 是
  **无量纲**的每 tile 网格边长；cell 的角间距由目标 order 决定：
  `Δθ = tile 角边长 / G`，而 order=o 的 tile 角边长 = `√(4π/(12·4^o))`（弧度），
  故 `Δθ(deg) ≈ (180/π)·√(π/(3·4^o))/G`。**同一 G 在不同 order 下物理尺度不同**：
  G=8 时 o=9 → Δθ ≈ 0.01432°（51.5″），o=12 → Δθ ≈ 0.0022°。**实测控制格角距 0.014400°（51.84″）**，
  与上式一致到 0.6% —— 引用时以上式与实测为准。因此一切以「cell 数 /
  cell 间距」为判据的表述**必须**连同 target_order 给出；不得把 G=8 当作固定角尺度。
  **不得**把 cell 角间距与天光面节点间距混为一谈：后者的量纲同为角尺度但取值由
  `docs/science/PHASE2_UPM.md` §7a 的表示能力规则**由输入几何导出**（非标定常数、非 2×cell）。
  `cell_side = kTileWidth/G = 64` 的量纲是**叶像素**（order+9 层），不是角秒。
- control_id = cells 索引（uint64，稠密 0..n_union×G²-1，含空覆盖
  占位；:947 与 :1031/:1105 一致）；
- out_n_controls = n_union×G²（几何节点总数，与
  stats.accepted_controls/overlap_controls 区分；sampler.h:153-154
  注释冻结）；
- P2ControlNode 填充 :1105-1111（control_id=索引、tile_ipix、gx/gy、
  ra/dec、leaf_ipix）。

### 5.2 patch 收集与 signal/variance/weight 对齐（matrix 专项 2）

每 (cell,frame) 候选 patch = cell 中心 ±`background_patch_radius`
（默认 8 → 17×17；:596 r=cfg.background_patch_radius）：

- 逐 leaf 扫描 :775-790：`fi_idx = nested_local_to_fits_index(z, 9,
  512)`，**signal/support 同一 fi_idx 对齐读取**（:783-784）——
  signal 值与其支持度按同一 leaf 索引成对消费，禁止错位；
- 无效值过滤 :785-786：非 finite signal 剔除；非 finite support 或
  support≤0 剔除（support 域 (0,1] 上游 DATA-COV-001 §19）；
- 累计 ：787-789：vals.push(s)、sup_sum+=sp、n_valid++；
- support 输出 = patch 均值 `sup_sum/n_valid`（:844）；
- 边界处理：patch 越 tile 边界像素丢弃（:779-780 x/y 范围门），
  patch 不跨 tile 读取（每 cell 只消费本 tile pair，§9）。

### 5.3 Stage B：亮端迭代 sigma-clipping（:802-815）

对 patch 值集 vals（保留负值，仅剔非 finite）：

```text
m0 = median_of(vals)                          # :802
s0 = 1.482602218505602 × median(|v−m0|)                  # :806-809
迭代 it = 1..background_clip_iters (默认 3):   # :812
  nr = { v ∈ ret : v ≤ m0 + clip_sigma·s0 }   # :815 单侧亮端
  若 |nr| < min_samples: break                 # :816
  nm = median_of(nr)                           # :817
  若 |nm−m0| < 1e-12 × max(|m0|, 1e-12): ret=nr; break   # :818 相对收敛
  m0 = nm; ret = nr
  s0 = 1.482602218505602 × median(|v−m0|)（ret 上重算）    # :821-826
  若 s1 ≤ 0: break                             # :825
y_ik = m0（收敛集位置估计）                     # :863
σ_bg_raw = s0                                  # :864 未加地板的稳健尺度
```

细节锚：迭代序号按实际行为（先收缩后重算尺度）；`n_total` 为
过滤后 patch 原始样本数（:827）；`n_retained = |ret|`（:868）。

**MAD→σ 一致性因子 1.482602218505602 的适用域**（本常数取值的依据）：

- 该常数 = 1/Φ⁻¹(3/4)（Φ = 标准正态 CDF），是**正态族**的尺度一致性因子：
  对称分布且中位处密度 f(m)>0 连续时 MAD = F⁻¹_{|X−m|}(3/4)，σ̂ = MAD/Φ⁻¹(3/4)
  仅在 **f 为高斯** 时相合。出处：Rousseeuw, P. J. & Croux, C. 1993, JASA 88(424),
  1273-1283, DOI 10.1080/01621459.1993.10476408（MAD 的一致性因子与有限样本修正）；
  有限样本修正 b_n 表见 Croux, C. & Rousseeuw, P. J. 1992, *Computational Statistics*,
  411-428, DOI 10.1007/978-3-662-26811-7_58。
- 本仓实测（`run/SCI-FIX-PHASE2-01/exp/e1e2_domain_calibration.py`，证据
  `evidence/e1e2_domain.json:mad_factor`，R=4×10⁵）：E[MAD×1.4826]/σ_true =
  **0.9528 (N=17) / 0.9972 (N=289)**（高斯，判绿）；**1.275（均匀）**、
  **0.727（拉普拉斯）**（非高斯，判红）⇒ 因子在高斯 patch 上成立；在非高斯
  （强结构、重尾、量化平台）patch 上偏差可达 ±30% 以上，此时 σ_bg 不是 σ 的相合估计。
- **单侧亮端 clipping 的影响（实测澄清）**：本模块 clipping 只剪亮端
  （`v ≤ m0 + clip_sigma·s0`，:850）。MAD 是 50% 分位量、截断点位于 ≈3σ
  （高斯下第 99.87 百分位），故剪除亮尾对 MAD 影响可忽略：实测高斯 + 8σ 亮端污染
  （5% / 20% 像素）下 E[s0]/σ_true = **0.9951 / 0.9947**（同证据文件 `mad_factor`
  的 `gauss+sampler_clip` 行）⇒ "单侧剪裁使 MAD 系统性偏" 在 clip_sigma=3.0 配置下
  **不成立**；成立的是上一条的非高斯域偏差与 §5.4 的结构分量问题。

**σ_bg 的零尺度分支（正向约束）**：

- 判据：`σ_bg_raw == 0` ⟺ **patch 内 ≥ 半数像素取同一数值**（`median(|v−m0|) = 0`）。
  可达性实测：真实 M42 300 s R 帧 4096 个 17×17 patch 中 **2 个**命中（0.049%），
  且这 2 个 patch **没有一个是全常数**
  （`evidence/r2_m42_raw.json:R2d_sigma_zero_reachability`）⇒ 该分支在真实数据上可达，
  不是理论角落；
- 约束：该分支表示 **patch 不携带尺度信息**（数据被量化/饱和平台主导），**不得**把它
  映射成一个有限的正方差。发布口径由 §5.4 规定；1e-12 只是避免 `σ_bg²` 归零的数值
  保护量，其量纲随 σ_bg 标度而变，**不是**最小可分辨方差、**不是**读出噪声下限。

### 5.4 control estimator 方差（ALG-UPM-CONTROL-IVAR-001 冻结承接）

```text
control_variance = k_corr × (π/2) × σ_bg² / N_retained    # :875（N_ret = N_retained）
control_ivar     = 1 / control_variance（cvar>0；否则 0）   # :877
uncertainty      = sqrt(control_variance)                  # :878
# σ_bg = σ_bg_raw；σ_bg_raw==0 的分支见下方「零尺度分支发布口径」
```

**公式的精确形式与适用域（本条是 (π/2) 因子的唯一依据）**：

- 精确形式：独立同分布样本的样本中位数渐近方差为
  `Var(median) = 1 / (4·N·f(m)²)`（m = 总体中位数，f = 总体密度，要求 f 在 m 处
  连续且 f(m)>0）。高斯特例 f(m) = 1/(σ√(2π)) 代入即得 **Var(median) = πσ²/(2N)**。
  出处：Serfling, R. J. 1980, *Approximation Theorems of Mathematical Statistics*,
  Wiley, ISBN 0-471-02403-1（DOI 10.1002/9780470316481），§2.3.2 样本分位数渐近
  正态性定理（p=1/2 即中位数）；同结论亦见 Cramér, H. 1946, *Mathematical Methods
  of Statistics*, Princeton UP, **§28.5「The quantiles」（pp. 367–369）**（§28.4 为「Functions of moments」，不含本结论）。
  逐字：*"the median z of a sample of n from this distribution is asymptotically normal (m, σ√(π/(2n)))"*。
- 本仓独立复核（**就地方法**：固定 seed 的 MC，R = 4×10⁵ 次重采样，每次从指定分布抽 N 个 i.i.d. 样本、
  取样本中位数、算其样本方差，再与解析式相除取比值；判据 = 比值须随 N 增大收敛到 1）：
  把实测 Var(median) 与
  πσ²/(2N) 相除，高斯族得 **0.9149 (N=5) / 0.9722 (N=17) / 0.9938 (N=65) /
  1.0030 (N=289)** ⇒ 渐近式在 N=5 时**低估 8.5%**、N=17 时低估 2.8%、N≥65 时误差 <1%。
  同一实验对非高斯族给出**判红**结果：均匀分布比值 → 6/π = 1.9099（实测 1.9013 @N=1025）、
  拉普拉斯分布比值 → 1/π = 0.3183（实测 0.3345 @N=1025），两者都与解析值 1/(4Nf(m)²)
  一致而与 πσ²/(2N) 相差 3–6 倍 ⇒ **π/2 因子只对高斯样本成立**，与「禁止把 (π/2)
  因子解释为其他分布假设」同义。
- **适用域**（三条同时成立才可引用本公式）：① 样本 iid；② N 足够大（本仓判据：
  N ≥ 65 时相对偏差 <1%，N = min_samples = 5 时偏差 −8.5%，属保守方向）；
  ③ patch 内分布近似高斯且无结构梯度（见下条）。**退化条件**：patch 含显著空间结构时
  σ_bg 量的是样本离散度而非噪声（§5.3），本式给出的是「patch 样本离散度的 πσ²/(2N)」，
  不是 control estimator 对真值的统计方差。
- **σ_bg 的结构分量（实测）**：`run/SCI-FIX-PHASE2-01/exp/e3e4_floor_and_structure.py`
  （证据 `evidence/e3e4_floor_structure.json:E4_structure_vs_noise`）在真值 σ=2 的 patch 上
  叠加线性梯度：E[σ_bg]/σ_true = **0.9955（零梯度，判绿）/ 1.0104（0.05/px）/ 1.2214
  （0.2/px）/ 3.8098（1.0/px）/ 18.4659（5.0/px）**；真实 M42 300 s R 帧 4096 个 patch 上
  σ_bg 与高通噪声估计之比中位 **1.010**，但 **3.78% 的 patch > 2×、0.12% > 10×**
  （`evidence/r2_m42_raw.json:R2c_structure_inflation`）。⇒ 适用域 = **背景主导 patch**；
  结构主导 patch 上 control_variance 偏大（权重偏小），必须在 provenance 里可分辨。

**零尺度分支的发布口径（正向约束）**：

- `σ_bg_raw == 0`（判据见 §5.3）时，control_variance 与 control_ivar **必须**标为
  「无尺度信息」：`control_ivar = 0`、`control_variance` 取非有限值（或等价地以
  `quality_flags` 置位并计数），**禁止**用 1e-12 生成一个有限的正方差并发布。
  理由：1e-12 是量纲随标度变化的数值保护量（§5.3），把它平方除以 N 得到的是
  **伪方差**；本仓实测该伪方差 = 7.609e-27、伪 control_ivar = 1.314e26
  （`evidence/e3e4_floor_structure.json:E3b_constant_patch`），比同一标度下 float32
  可表示的最小真实离散度所对应的方差还小 **32 个数量级**（M42 ADU·sr⁻¹ 标度），
  即「任何可表示的数据都不可能支持这个精度」。
- 危害的可判定实证：把该伪方差当物理方差发布后，命中地板的观测以 1.314e26 的权重
  参与 UPM 加性面求解。合成算例（24 cell × 2 帧，λ=1 粗糙度正则，注入 +50 的零点离群；
  `evidence/e3c_weight_pollution.json`）实测：按「无信息」处置时帧间相对零点误差
  = **1.31**（噪声量级，判绿）；按现行伪方差发布时误差 = **28.56**（判红）。
- 与 `lib/algorithms/coverage/src/upm.cpp:2766-2790`（`p2_upm_control_variance`）的
  输入门一致：该函数已对 `n_retained < 1`、`σ_bg ≤ 0/非有限` 显式返回 rc=1
  ⇒ 生产侧已具备「无尺度信息即拒」的接口；采样侧当前把地板值送进该接口，
  绕过了这一语义。

- 常数权威：kPiHalf=1.57079632679489661923（:84）；k_corr 默认 1.4（:83）；
  **定义域 1 < k_corr**（k_corr = 1 ⇔ 忽略相关，`p2_upm_control_variance` 返回 rc=2；
  k_corr < 1 ⇔ N_eff > N_retained，正相关样本的有效样本量不可能大于样本数，物理不可达，
  返回 rc=1 —— `lib/algorithms/coverage/src/upm.cpp:2772-2783`）；
- **k_corr 的适用域（正向约束）**：k_corr 是「Drizzle 输出像素协方差导致的 control
  estimator 方差放大」的**现象学因子**，其数值只在**其标定域内**有实证意义。
  当前生产取值 1.4 的来源是项目自产 MC（`control_median_mc_test`，pixfrac=0.8，2000 次，
  实证 1.3883，保守上取），而**该测试未注册/不在任何 CMake/ctest/CI 面（构建孤儿）**
  ⇒ 该常数在仓内**不可复跑**。因此任何引用 `control_variance` 的陈述**必须同时**声明：
  (i) 所用 k_corr 值；(ii) 其标定域（源像素角尺度 + pixfrac）；(iii) 该标定在当前数据
  尺度上是否在域内。`upm.cpp:2265-2275` 已把这条落成写盘门（FZ-PROV-KCORR）：
  `k_corr_applicability_domain` 为空即 rc=7；k_corr ≠ 1.4 时必须另给
  `k_corr_calibration_run_id`。
- **K_CORR_DOMAIN 选项 B（逐帧标定）：仅 pixfrac 维参与标定（SC-005）**：
  仅当帧 Drizzle provenance 的源像素角尺度落在**标定域 [300,600]″/px** 才取
  kcorr_lookup(pixfrac, scale)（:93-117）；**域外一律保留 kcorr=0**（回退链
  `frames[i].kcorr>0 ? per-frame : cfg.control_k_corr`，:874），**禁止** clamp 到 300″ 档；
  lookup 表 :96-99 冻结数值 {1.2112,1.3925,1.4980 | 2.3958,2.8971,3.2035}
  （300″/600″ × pixfrac 0.5/0.8/1.0），两段分段线性插值（非均匀网格）:100-116；
  pixfrac 维仍 clamp [0.5,1.0]（表列有界性保留）；
  旧行为「域外 clamp」把静默饱和固化成合同（旧 kcorr_lookup_test.domain_clamp），
  已被 `kcorr.domain_fallback_to_frozen_default_not_clamp` 取代；
- **标定域与生产数据尺度的失配（实测，属适用域声明而非缺陷豁免）**：
  (i) 真实生产帧的源像素角尺度量级为 **≈1″/px**：本仓 M42 300 s R 帧实测
  0.9890″/px（`evidence/r2_m42_raw.json:R2a_unit`，由 FOCALLEN=1877 mm、
  XPIXSZ=9 µm 推得 206264.806×9e-6/1.877 = 0.9890″/px）；本仓 probe 产品
  `hips_pixel_scale=0.000224 deg = 0.8064″/px`
  （`run/VARIANCE-SEMANTICS-01/probe/out_probe_orig_m42_1024_1024/hips/signal/properties`）
  ⇒ 与标定域 [300,600]″/px 相差 **300× 以上**；
  (ii) 因此**逐帧标定在所有已知生产数据上恒不生效**，生产实际使用的是未在本尺度标定的
  冻结常数 1.4；任何「control_variance 已做 Drizzle 相关校正」的表述必须写明这一点；
  (iii) 域外回退的**可观测性边界**：`sampler.cpp:575-584` 只在 `pixfrac > 0 且
  尺度 ∉ [300,600]` 时打 `[sampler] k_corr 域外回退` 标；**provenance 键缺失
  （pixfrac 不可解析）时回退是静默的**——本仓 probe 产品的 signal properties 内
  **不含** `ASTROCS_DRIZZLE_*` 键（同上 properties 文件），即该产品走的是静默回退路径。
  引用回退状态时**必须**区分「已打标的域外回退」与「未打标的 provenance 缺失回退」。
  合法 provenance 的键名与量纲：`ASTROCS_DRIZZLE_PIXFRAC`（无量纲，域 (0,1]）、
  `ASTROCS_DRIZZLE_SCALE_ARCSEC`（**角秒/像素**，域 (0,∞)；写侧
  `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1770-1774`，读侧
  `sampler.cpp:135-140`）。
- UPMW-004 独立 MC 基线：Var(median) ≈ πσ²/(2N)（先例 `synthetic_gate.cpp`）；
  本式为该基线乘 k_corr 的 Drizzle 相关放大。该基线的成立条件见本节「公式的精确形式
  与适用域」：iid + 高斯 + N ≥ 65；**禁止**把 (π/2) 因子解释为其他分布假设。

### 5.5 Stage C/D/E 局部门控（:952-1009 + :847-867）

**Stage C 局部 tolerance gate（DBE-like baseline）**（:959-1007）：
对每个第一遍 accepted（reason=0）候选，收集**同 tile** 内
Chebyshev 距 ≤ background_neighbor_radius（默认 2）邻域 cells 的
该帧 cleaned median `m`：

```text
B = median(neigh)                    # :987-988（<3 → 回退 :977-985）
S = 1.482602218505602 × median(|v−B|)           # :989-992
若 m > B + background_tolerance × S:  # :993-996
  拒绝，reason=3，++rejected_bright_tolerance
邻域 <3（回退后仍 <3）: 不 gate（保守保留）          # :977/:986
```

**Stage D 双门**（:997-1006，else-if 链互斥）：

```text
bfrac = #{v > y + contamination_sigma·σ} / n_total     # :865-867
若 bfrac > background_max_contamination: reason=4，++rejected_high_contamination
若 N_retained < min_retained_fraction × n_total: reason=2，++rejected_insufficient_retained
```

（bfrac 计算在 :865-867 于 Stage B 后立即落账，gate 于第二遍判定。）

**bfrac 的适用域（正向约束）**：bfrac 是**单侧**（亮端）污染占比：分子只统计
`v > y + contamination_sigma·σ`，分母 n_total 为过滤后 patch 原始样本数。
因此它只对「亮端污染」（宇宙线、卫星线、亮星晕）敏感，对**暗端离群**
（坏列、暗斑、负向结构）恒为 0。适用域 = 亮端污染为主的 patch；
暗端污染域内本判据**不成立**（不拒绝），此时由 §5.3 的亮端 clipping 与
Stage C 的 tolerance gate 承担，两者同样只作用于亮端 ⇒ 暗端离群在本模块内
**无判别面**，必须由上游坏帧/坏列掩膜承担。

**Stage E catalogue veto**（:844-849，第一遍内）：仅当
background_catalog_veto 且该帧 SNR catalogue 非空且
frame_snr_med>0：

```text
thr = cfg.star_mask_snr_factor × frame_snr_med[frame]   # :849/:1149（默认 10.0，无量纲倍数）
rad = cfg.star_mask_radius_deg           # :514 默认 0.012°（度；已入配置面，DISP-P2SMP-004 闭环）
any_above(thr, ra, dec, rad) → reason=5 拒绝，++rejected_catalog_veto
```

frame_snr_med 为该帧 catalogue SNR 排序中位（:615-627 预计算；
:616 frame_snr_med_exact 声明、:623 精确 median_of 赋值供 fallback）。

**SNR 邻域值与可用性**（:853-867）：query(ra,dec,
snr_search_radius_deg) 收集半径内星 SNR → 中位数为 snr_val 且
snr_available=1；半径内无星 → snr_val=frame_snr_med_exact（整帧
精确中位）且 snr_available=0（**禁止以 1.0 伪装**，upm.h:51-53
冻结）；catalogue 缺失帧 → snr=0.0/available=0（:861/:862-864）。

**SnrIndex 等价性冻结**（:206-288）：dec 升序排序索引 + RA 保守
窗口（ra_win=radius/cos_guard，cos_guard=max(cos(|dec|+r),1e-4)，
:244-248 / :270-274）+ 最终判据恒为
`angular_distance_deg ≤ radius_deg`（:254-255 / :280-283）——
索引路径与全扫描**完全一致**（同一精确判据，无近似替代）。

### 5.6 frame_id 内容稳定标识（:314-438；DATA-FRAME-ID-001）

```text
frame_id = uint64(SHA-256 前缀 16 hex 大端截断)          # :422-430
SHA 流 = 9 关键 properties（creator_did/obs_title/obs_filter/
  obs_exptime/obs_date/hips_order/hips_release_date/
  hips_pixel_scale/moc_sky_fraction，缺 key 记空值；:349-352）
  + 各 tile（升序）"t=" + signal tile 原始 float 字节 + ";"（:360-379）
  + 各 tile "St=" + support tile 字节 + ";"（:381-393）
  + SNR catalogue 逐条 "i:ra,dec,snr,qf;"（max_digits10 格式化；:394-421）
路径/重命名/换根目录不变；任何科学 payload 变化 → 改变；失败/异常
→ 0（调用方 :512-523 拒绝 rc=1；禁止静默继续）。
```

禁止描述为 FNV-1a/路径派生（sampler.h:97 注释冻结）。

### 5.7 统计量共享实现（:191-204/:440-459）

- median_of：nth_element 取中，偶数 n 取 [begin,mid) 最大值均值（P0-01 修复，:196-202）；
- p2_stats_median：先过滤非 finite（:444-445），空/全 NaN 经 median_of :192 → 0.0
  （sampler.h:101 冻结）；
- p2_stats_mad：median 后 1.482602218505602×median(|x−med|)（:452-460），
  out_median 回传收敛 median；UPM 侧共享同一实现（sampler.h:102-104 导出声明；upm.h:43-48 冻结注释同源定义）。

## 6 消费链与并行语义

**生产消费（唯一消费方 stage2.cpp）**：frame_id 预计算
stage2.cpp:219-231（p2_frame_id :222）→ sccfg 组装 :256-274（14 字段显式透传；control_k_corr 未透传，零初始化经 impl :497-498 修补回退默认 1.4；cpu_workers=cfg.exec.cpu_workers，CON-004 Runtime lease 唯一来源 ：273-274）→ probe/fill 两遍调用 p2_sample_controls_cached :279-311（probe :279-281 out_obs=nullptr 查容量；计数上限拒绝 ：296-300；分配 ：301；fill :306-311）；cov 来自 coverage union（上游 P2-COV 域）。

**并行模型**（:880-930）：worker 数=cfg.cpu_workers（0 视为 1，:883；:881 无 hardware_concurrency——模块不得自行开线程，注释冻结 ：880-882）；workers>1 时 std::thread 池 + `next_c.fetch_add` 动态
领取 tile（:886-913），**每 worker 独立 AIO 句柄**（SamplerReader
rdr.init_own :894；避免共享句柄竞争），per-cell 结果写回固定槽位
cells[idx]（:870-872，无跨线程数据竞争面），veto/insufficient 经
原子计数器归并（声明 ：887-889；归并 ：901-902/:907-908）；workers=1 串行 reference 共享
主线程句柄（:916 init_shared，循环 ：917-933）。

**确定性冻结**：cell 输出按 cells 索引固定槽位、第三遍单线程顺序
扫描、SNR 中位数经排序 median——**输出 obs 序列 bitwise 与 worker
数无关**（1/N worker 等价，sampler_parallel_consistency_test.cpp:29
TEST(Phase2SamplerParallel, OneTvsTwoTDeterminism) 承载）；全局
g_aio_mu（:161 声明，read_tile_pair :166 加锁；并行路径 per-worker 独立句柄不经此锁）。

**并行路径唯一口径**：`std::thread` 为唯一并行路径（:879-880 注释）；
lib/algorithms/coverage/CMakeLists.txt:28 的 `P2_ENABLE_OPENMP` option 仅影响
旧 target 编译面，**禁用**据此启用 OpenMP 路径。
本节是 `ASTROCS_DESIGN.md` §8（资源与并行的强制条款：确定性合同 = 浮点归约顺序冻结、并行开关不改科学数值、输出不依赖线程调度；线程预算唯一来源）与 §7.1（模块边界）在本模块的**落地细化**，**不另立权威**（§0.1：只有一份权威链）。

## 7 与 SCI 的对应与偏差（如实登记）

| SCI-UPM-001 语义 | 实现现状 | 判定 |
|---|---|---|
| 控制点几何由 union 几何+角间距决定，不由 SNR 决定（sampler.h:6） | cell 网格规则布置 ：737-742；SNR 只进 veto/可信度 | 一致 |
| y_ik 从实际 Phase1 HiPS 读取 | AIO 唯一 I/O（:719-733 read_tile_pair） | 一致 |
| patch robust median/MAD 保留负值 | :802-829（无符号过滤） | 一致 |
| SNR 来自 Catalogue 禁止重检测 | :851-867 纯查询 | 一致 |
| control_variance 公式（SCI-UPM-WEIGHT-001） | :840-842 逐项一致 | 一致 |
| k_corr MC 校准非猜测（sampler.h:50-51） | :83/:89-112（选项 B 逐帧，pixfrac 维） | 常数一致（冻结 1.4 ≥ 实证）；**MC 证据源 `control_median_mc_test` 未注册（MISSING，构建孤儿）⇒ 不可复跑** |
| per-control `control_reliability`（`geometric_reliability` 为**禁用**旧名）参与归一化 | 采样器不产出 per-control 可靠度；UPM 侧实现为**配置常量 1.0**（`upm.cpp:565` 归一化消费） | 不在本模块域（UPM 侧缺陷，已登记 SC-005） |
| wiki 语义版本 34A532A2...B2EB308 | sampler.cpp:3/:85-87 注释锚定 | 一致 |

## 8 单位与 dtype 登记（唯一权威=DATA_SEMANTICS §23）

- P2ControlObservation 14 字段（upm.h:31-57）：frame_id/control_id/
  leaf_ipix u64；ra_deg/dec_deg/value/uncertainty/snr/ivar/
  control_variance/control_ivar/support f64；snr_available int；
  quality_flags u32；
- P2SampleStats 10 字段 u64（sampler.h:68-79）；
- tile payload float32（aio read_tile_f32；:359-361/:375-379）；
- 输出 dtype/shape/invalid/可空语义唯一权威=DATA_SEMANTICS §23；
  本节单位表（§2）与之一致，冲突以 §23 为准。

## 9 边界与退化（matrix 专项 4/5）

- **tile 边界/seam**：patch 采样限制在单 tile 512×512 内（:779-780
  出界丢弃），**不跨 tile 读取**——跨 tile 接缝处 patch 有效面积
  收缩，min_samples 门（:793-800）显式拒绝而非补读邻 tile；第二遍
  邻域 gate 同样按 tile 分组限定（:954-957 `tile_cells` 映射，
  邻域只遍历同 tile cells）——tile 间不共享邻域基线（设计边界，
  冻结；跨 tile 平滑语义归 UPM Laplacian，不归采样器）；
- **空覆盖/越界 tile**：union cell 无覆盖帧 → CellStat 无 frames，
  不产 obs 但计入 control_id 占位（:946-947）；tile_ipix ≥
  12·4^order → 整 tile 占位跳过 tile=-1（:702-714）；首 tile 预读
  校验（:657-664）显式 rc=1；
- **异常/极端输入**：n_union>1e6（:634-638）、cells>2×10⁸
  （:644-648/:1071-1077）、resize OOM（:649-654）、单 cell frames
  >10000（:1083-1086）→ rc=1 或跳过；MSVC /EHa 下 SEH 兜底捕获
  （:936-944）；
- **缺失与无效值**（matrix 专项 5）：signal NaN/Inf 剔除
  （:785）；support≤0/非 finite 剔除（:786）；全 NaN 统计输入 →
  median/mad 返回 0.0（:441/:444-445，经 median_of :192）；ivar 产品缺失或 leaf 无值
  → o.ivar=0.0（"不可用"，不伪装；:1039-1056）；SNR catalogue
  缺失 → snr=0/available=0（:861-864）；frame_id=0（哈希失败
  哨兵）→ rc=1（:512-523）；
- **退化数值（正向约束，口径见 §5.3/§5.4）**：
  (i) σ_bg_raw=0（patch 内 ≥ 半数像素同值）⇒ control_ivar 必须为 0 且
  control_variance 必须标为无尺度信息；**禁止**把 1e-12 数值保护量平方后当
  物理方差发布（§5.4「零尺度分支的发布口径」）；
  (ii) `N_retained = 0` **不可达**：patch 先经 `n_total ≥ min_samples` 门
  （:828-836），而 `min_samples` 被配置修补钳到 ≥1（:521），clipping 收缩也以
  `nr.size() ≥ min_samples` 为前提（:851）⇒ `|ret| ≥ min_samples ≥ 1`。
  代码中的 `n_ret = max(N_retained, 1.0)`（:873）是**死分支**（防御性写法），
  不得据此推导「无样本却有置信度」的边界行为；
  (iii) cvar ≤ 0 ⇒ civar = 0（:877），仅当 σ_bg 或 N_retained 非法时才可达，
  与 (i) 同口径：ivar=0 表示「无信息」，下游 `p2_upm_raw_weight` 对
  `use_ivar_weight=1 ∧ control_ivar ≤ 0` 显式 rc=2（`upm.cpp` / SCI-UPM §4）。
- **reason 编码**（内部）：0=ok、1=insufficient_support（tile 读
  失败/坐标越界/min_samples）、2=insufficient_retained、
  3=bright_tolerance、4=high_contamination、5=catalog_veto
  （:608 注释/:759-799/:866/:995-1005）；reason 不外发
  （P2ControlObservation 无该字段），下游唯一拒绝观测面=
  P2SampleStats 六类计数（§23 §23.4）。

## 10 已冻结禁改清单（本层不可接受变化）

1. 控制点几何解耦 SNR（sampler.h:6；改动即 SCI 违约）；
2. control_variance 公式与常数（:82-83/:840-842；含 k_corr 冻结
   默认 1.4 与标定表 :91-94 九值）；
3. frame_id canonical SHA-256 输入白名单与序（:349-421；
   DATA-FRAME-ID-001，改任一输入面即破坏持久化绑定）；
4. patch 保留负值（:787-789 无符号过滤）；
5. snr_available=0 不伪装 1.0（:853-861；upm.h:51-55）；
6. ≥2 clean 帧才入 UPM（:1013-1027；相对光度约束）；
7. 输出 obs 序列 bitwise 独立于 worker 数（:886-934 槽位设计）；
8. 禁止重新检测星点（SNR 纯查询 ：851-867）。

## 11 冻结附录（SRC-P2-SMP-001 源码实测）

### 11.1 返回码/错误语义（p2_sample_controls / p2_sample_controls_cached）

- rc=0：成功（含空 obs 输出——空覆盖 union 合法）；
- rc=1：错误（err 缓冲 8KB 文本，不区分细分码）：bad args（null
  coverage/hips_paths/out_n_obs/out_n_controls；:476-479）、
  frame_id 0 invalid（:512-523）、open frame failed（:536-546）、
  n_union too large（:634-638）、cells too large（:644-648 /
  :1071-1077）、cells resize failed（:649-654）、tile ipix out of
  range（:657-664）、pairs resize failed（lambda :721；并行 err :909-912；串行 err :919-923）、
  exception（:1089-1097）。错误粒度=rc 二值 + err 文本（编排层
  ACS_ERR 映射归 API-P2-001 编排面，不在本模块域）；
- out_obs/out_controls 容量不足**不报错**：按 capacity 截断拷贝、
  out_n_* 返回真实需求量（:1101-1117；probe/fill 协议
  sampler.h:107 冻结）；
- 并发安全：无共享可变全局态（g_aio_mu 锁仅覆盖 read_tile_pair :166；并行路径 per-worker 独立句柄），reentrant
  yes；无取消检查点（ThreadLease 接线归 P2-SAMP-IMPL 整改点，与
  DISP-COV-005 同构）。

### 11.2 现状缺陷清单（DISP-P2SMP-001..007，登记不改码）

| ID | 锚（sampler.cpp） | 内容 | 来源 |
|---|---|---|---|
| DISP-P2SMP-001 | :485-502 | 配置修补 `<=0→默认` 吞显式 0（意图"禁用"的 0 被静默改写为默认值，如 background_clip_iters=0 想关 clipping 反而得 3） | 实测 |
| DISP-P2SMP-002 | :1022 | 第三遍 ：1022 对 accepted=false 且 reason==2 的帧再次 `++rejected_insufficient_retained`，与第二遍 ：1006 递增重复——P2SampleStats.rejected_insufficient_retained 对该类拒绝双计数（统计面偏差，obs 输出不受影响；:1078-1079 注释自述曾修 double-count，此残留与其意图矛盾） | 实测 |
| DISP-P2SMP-003 | :641-643/:655/:666-672/:705-713 等 17 处 | 诊断进度日志直写 stderr（fprintf/fflush），未走结构化日志通道，err 缓冲外；生产可观测性债（静默失败排查依赖 stderr 文本） | 实测 |
| DISP-P2SMP-004 | sampler.h:58-59; sampler.cpp:318-319,513-514,1149 | **配置面口径（约束）**：星掩膜阈值与半径必须由配置面承载——`P2SamplerConfig.star_mask_snr_factor`（默认 10.0）/ `star_mask_radius_deg`（默认 0.012），`sampler.h:58-59` 声明、`sampler.cpp:318-319` 默认值、`:513-514` 配置修补、`:1149` 消费；单位：factor 无量纲（帧 SNR 中位数的倍数）、radius = 度 | 实测 |
| DISP-P2SMP-005 | :853 | clipping 相对收敛阈值 `1e-12×max(|m0|,1e-12)`：m0≈0 时阈值≈1e-24 过严，实际退化为固定 background_clip_iters 轮全迭代（结果仍确定、单调收缩、min_samples 兜底；无科学输出影响，性能观察级） | 实测 |
| DISP-P2SMP-006 | :575-584 | **域外回退的可观测性缺口**：`[sampler] k_corr 域外回退` 标只在 `pixfrac > 0 且 尺度 ∉ [300,600]″` 时打；Drizzle provenance 键缺失（pixfrac 不可解析）时回退到 `cfg.control_k_corr` 是**静默**的，provenance 无法区分两条回退路径 | 实测（provenance 无 `ASTROCS_DRIZZLE_*` 键时仍以 1.4 参与生产） |
| DISP-P2SMP-007 | :864-877 | **零尺度伪方差**：`σ_bg_raw=0` 时以 1e-12 生成有限 control_variance（7.609e-27）与 control_ivar（1.314e26）并随 obs 发布；§5.4 已冻结「无尺度信息」发布口径，**禁用**以 1e-12 之类的占位尺度生成伪有限 control_variance/control_ivar | 实测 |

### 11.3 TEST-P2-SMP-DESIGN-001 冻结测试设计（可执行 TEST-P2-SMP-001 由 P2-SAMP-TEST 落地，双面登记不冒认）

覆盖 matrix P2-SAMP 行五项科学专项；每个 threshold 引用本节，
禁止事后改：

| # | 设计面 | 冻结容差 | 现状测试锚 |
|---|---|---|---|
| F1 | 统计量单元：median odd/even/负值/重复/乱序/NaN 过滤；MAD=1.482602218505602×median 偏差 | 逐值 bitwise（EXPECT_DOUBLE_EQ） | synthetic_gate.cpp:3594 G1StatisticsCorrectness（先例在库） |
| F2 | kcorr_lookup 边界与角点：pf∈{0.5,0.8,1.0}×sc∈{300,600} 九值、**域外回退冻结默认 1.4（禁 clamp）**、provenance 缺失/尺度未知回退 1.4 | 角点值 exact；插值点 rtol 1e-12；域外 == 1.4 exact | phase2_sampler.kcorr.corner_exact / .domain_fallback_to_frozen_default_not_clamp（表值 :94-97） |
| F3 | control_variance 解析 oracle（Python 复算 k_corr×(π/2)×σ²/N_ret） | **两档分开**：(a) 解析复算 rtol 1e-12（f64 复算域，判据=逐值相等）；(b) 与 MC 实测 Var(median) 比对的**相对**判据：N=289 时 |比值/k_corr − 1| ≤ 0.02、N=17 时 ≤ 0.05（本仓实测 (a) 恒真、(b) 高斯 0.986/0.972，见 `evidence/e3e4_floor_structure.json:E3a_true_noise` 与 `evidence/e1e2_domain.json:median_variance`）。**禁止**用「3σ」这类未声明 N 与分布的统计判据 | synthetic_gate.cpp:4001/:4061/:4089（先例在库）；域判据见 `run/SCI-FIX-PHASE2-01/exp/e1e2_domain_calibration.py` |
| F4 | 坐标/tile 映射：单 tile 合成 → 64 cell (ra,dec,leaf_ipix) 对独立 HEALPix 参考实现（astropy-healpix，BSD-3-Clause） | atol 1e-9 deg（≈3.6e-6″；适用域=本模块 order ≤ 12 的 f64 pix2ang_nest，参考实现同域；**高于该 order 或跨实现差异 >1e-9 deg 时判据不成立**，须先做参考实现一致性预检再启用本门）；cell 索引单射 exact | 无（新建） |
| F5 | constant/gradient/impulse 验证面：constant patch（σ_bg_raw=0 分支）、线性梯度 patch（亮端 clipping 方向性）、单像素 impulse（bfrac=1/n_total 路径） | (a) 有尺度 patch：cvar rtol 1e-12；(b) **零尺度分支（非退化判据）**：σ_bg_raw=0 时断言 `control_ivar == 0` 且 control_variance 非有限，**且**断言该分支在 ≥50% 像素同值的 patch 上触发、在 <50% 的 patch 上不触发（正/负例各一，§5.3 判据）；(c) 梯度 patch：E[σ_bg]/σ_true 在 slope=0 时 ≤1.01、slope=1.0/σ 时 ≥3.0（能红能绿，证据 `evidence/e3e4_floor_structure.json:E4_structure_vs_noise`）；接受/拒绝判定 exact | 无（新建；公式 :864-878） |
| F6 | 边界/seam：patch 跨 tile 边界截断、相邻 tile 互不污染、第二遍邻域同 tile 限定、空覆盖 tile 占位 | obs 集合 exact；node 占位数 exact | 无（新建） |
| F7 | missing/invalid：NaN/support≤0 过滤、全 NaN patch reason=1、ivar 产品缺失 o.ivar=0、frame_id=0 rc=1、tile 读失败 rc=1 | 判定 exact；rc exact | ivar_wiring_test.cpp:223（ivar 面先例）；其余新建 |
| F8 | 串行/并行等价：1 worker vs N worker 全输出 bitwise | bitwise | sampler_parallel_consistency_test.cpp:29（先例在库，扩展 grid/worker 矩阵） |
| F9 | 拒绝计数守恒（现状口径）：candidate=Σ|frames|、六类计数与 reason 分布自洽（含 DISP-P2SMP-002 双计数现状） | 计数 exact（按 §11.2 登记现状口径） | 无（新建） |

负面矩阵：null 参数 rc=1、frame_id=0 rc=1、坏 HiPS 路径 rc=1、
n_union/cells 上限 rc=1、probe 容量协议（out_obs=null 查量→分配→
fill）。fixture 由固定 seed 合成 HiPS 树生成，不提交大二进制。

### 11.4 SCI 层状态声明（本域零 SCI 改动）

- 采样语义权威已有 FROZEN SCI：SCI-UPM-001（docs/science/
  PHASE2_UPM.md，集合 SCI-UPM-001..010 + SCI-UPM-WEIGHT-001 +
  SCI-UPM-PERSIST-001；页头明示模块 "phase2 (upm/sampler)"）。
  **共享 SCI 引用不改动**（P1-WCS SCI-WCS-001=共享 ASTROMETRY.md、
  P2-COV SCI-UPM-001/SCI-INT-001、P2-INT SCI-INT-001、P2-REJ
  SCI-REJ-001 同构）。
- matrix P2-SAMP 行 science_id=SCI-P2-SMP-001（descriptor 占位
  词汇，module_adapters.cpp:657）的语义映射由本节声明——
  **SCI-P2-SMP-001 ⇒ SCI-UPM-001**（docs/science/PHASE2_UPM.md，
  矩阵 science_doc=docs/science/PHASE2_UPM.md，
  MOD-astrocs-phase2-sample 行）。
  SCI 公式语义不在此重复定义，两处冲突时以 docs/science/ 为准并
  回改本文档（禁止反向）。辅助语义锚：control_variance 权威=
  SCI-UPM-WEIGHT-001（§5.4 承接）；robust 统计上游=SCI-NOISE-001；
  链位置=SCI-SCOPE-001 §处理链第 5 步。
- 本节禁止被编排层词汇反向改写（descriptor astrocs.phase2.sample
  由 P2-XX-INT 对齐，不作冻结依据）。

## 12 关联 ID 映射（本文件承接）

- `ALG-P2-SMP-001` = 本文档整体（逐符号锚 §3/§5；矩阵 P2-SAMP 行
  algorithm_id，INDEX.yaml path 绑定本文件）。
- `ALG-UPM-CONTROL-IVAR-001`（既有 INDEX 条目，path 绑定
  本文件）= 本文档 §5.4 + §2（control_variance/control_ivar 公式
  与 k_corr 域）；两 ID 并存不冲突——ALG-P2-SMP-001 为模块合同
  全集，ALG-UPM-CONTROL-IVAR-001 为其方差子面（UPM 权重消费方
  引用），本文件为两 ID 共同权威页（多 ID 同文档先例：
  NOISE_ESTIMATION.md 承载 ALG-NOISE-001..003）。
- 本域 ID 一律用 `ALG-P2-SMP-001` / `TEST-P2-SMP-001`；二者注册
  INDEX 与矩阵，不另设同义 ID。
- `UPMW-004/005/007`（MC 验证项词汇）⇒ 测试锚对应本文件 §11.3
  F3（UPMW-004/007）与 k_corr 校准来源（UPMW-005，§5.4）。

## 13 追溯

- 上游 SCI: SCI-UPM-001（共享 FROZEN 零改动）；
- 本层: ALG-P2-SMP-001（本文件）；ALG-UPM-CONTROL-IVAR-001（子面）；
- 下游 DATA: DATA-P2-SMP（DATA_SEMANTICS §23）；API: API-P2-SMP-001
  （PUBLIC_API.md）；MOD: MOD-astrocs-phase2-sample（registry 行，
  合同三件套 lib/algorithms/sampling/）；TEST: TEST-P2-SMP-DESIGN-001
  （§11.3 设计冻结，registry 页承载）→ TEST-P2-SMP-001（可执行，
  P2-SAMP-TEST 落地前 MISSING）；
- 矩阵行：MOD-astrocs-phase2-sample（P2-SAMP 行原位融合，
  depends_on_int=P2-COV-INT;CPU-005，MODULE_MIGRATION_MATRIX.csv
  :15 权威）。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- 控制点/背景采样与相对定标：SCAMP（GPL-3.0；Bertin 2006, ASPC 351, 112）；SExtractor（GPL-3.0；Bertin & Arnouts 1996, A&AS 117, 393）。
- 稳健尺度/clipping：Hoaglin et al. 1983；Rousseeuw & Croux 1993。
- 稀疏天光面样条（目标表示）：Duchon 1977（薄板样条）；Wahba 1990, Spline Models for Observational Data, SIAM。
- var(median)≈πσ²/(2N)：Hoaglin et al. 1983（中位数渐近方差）；本文件 §5.4 承接 ALG-UPM-CONTROL-IVAR-001。
- SNR 加权采样：UNIFIED_SCIENCE_MODEL §4；docs/plugins/algorithms_phase2/10_sampling.md §4.2。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

