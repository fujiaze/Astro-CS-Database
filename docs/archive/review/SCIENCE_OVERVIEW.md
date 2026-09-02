> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/owner/SCIENCE_OVERVIEW.md（GOV-004 建立）

# SCIENCE_OVERVIEW.md — AstroCS 科学定义概览（L0 负责人层）

> 生成: V6 重构 (SCI-001/002/003)  目标版本: 0.10.0-alpha.2
> 本文件只汇总权威合同（`docs/science/*.md`），不重复维护底层定义。
> 所有公式、单位、假设、边界与不变量以对应 SCI 合同为唯一权威。

## 1. 科学目标

AstroCS 从多帧天文 CCD 图像估计统一的天球辐射场（HiPS signal）及其不确定性
（variance/ivar），并输出标准 IVOA HiPS 产品。

处理链：单帧校准 → 星点/PSF/astrometry/photometric → 噪声模型 → 球面 Drizzle →
Phase2 (coverage → 控制采样 → 加性 UPM → 排异 → ivar 加权积分) → HiPS。
Phase3 (HiPS 球面 → 平面 WCS FITS) 为独立施工项，见 §4。

## 2. Phase1 科学冻结摘要（SCI-001）

权威: SCI-CAL-001 / SCI-WCS-001 / SCI-PHOT-001 / SCI-PSF-001 / SCI-NOISE-001 / SCI-DRZ-001

| 模块 | 合同 | 核心定义 | 单位 | 关键不变量 |
|---|---|---|---|---|
| Calibration | SCI-CAL-001 | dark_opt=0: `cal=(raw−dark)/flat_norm`; dark_opt=1: `cal=(raw−bias−K(dark−bias))/flat_norm`; `flat_norm=max(flat/median(flat),0.1)` | ADU | 常量场/空平场/幂等归一/确定性 |
| Cosmetic | SCI-CAL-001 §3 | 坏点稀疏假设, 连通域分离, 只修 bad_mask=1 | — | 不把真实点源当坏点 |
| Star detect | SCI-SCOPE + ALG | 背景/噪声阈值, 连通域, 确定性 tie | px | completeness/false-positive 有界 |
| PSF | SCI-PSF-001 | centroid/FWHM/ellipticity 参数, 拟合权重 | px, deg | 失败不输出伪有效值 |
| PlateSolve | SCI-WCS-001 | pixel↔sky WCS, 投影/畸变, 匹配残差 | deg | roundtrip; 无解显式失败 |
| Photometry | SCI-PHOT-001 | aperture/PSF flux, 背景, 曝光/增益, 误差 | e⁻, mag | flux conservation; 饱和拒 |
| Noise/SNR | SCI-NOISE-001 | `Var[e−]=source+sky+dark+N_read·σ²_read`; variance/ivar 分离 | e⁻², 1/e⁻² | ivar=0 对 invalid; 单位平方 |
| NSIDE | SCI-DRZ-001 | 由输入尺度/过采样合同计算, 不硬编码 2048 | — | 冻结 1–2× 过采样范围 |
| Drizzle | SCI-DRZ-001 | `N_o=Σa_io w_i s_i; W_o=Σa_io w_i; I_o=N_o/W_o` | surface/px | constant/flux/centroid/support; FP64 accum |

### 失败语义（允许继续 vs 必须失败）

- Calibration: 参数错误 `AC_ERR_PARAM` 必须失败; NaN 不传播伪有效值。
- PlateSolve: 空星表/奇异线性/越界 → 显式错误码, 不允许伪成功。
- Photometry: 饱和星拒绝并计 `rejected_quality`, 不进入定标。
- Noise: 空 patch/NaN 拒绝。
- Drizzle: WCS 无效/尺度非法 → `compute_auto_nside` 失败。
- 数据质量标记: 各模块输出 quality 字段, 失败不留下貌似有效的空 catalog。

### 边界与不允许的解释

- 不加 pedestal、不 clamp 负值（DATA_SEMANTICS §4 负值保留）。
- variance 与 ivar 不混; 质量权重不得冒充 inverse variance。
- NSIDE 不硬编码; Drizzle 输出是 surface brightness, 总通量产品需独立 DATA 合同。

## 3. Phase2 科学冻结摘要（SCI-002）

权威: SCI-UPM-001 / SCI-CW-001 / SCI-REJ-001 / SCI-INT-001

- 接缝只允许**加性**背景校正; 背景模型与叠加权重分离。
- 加性 UPM 拟合: `min_b Σ w_ijk[(y_i−b_i)−(y_j−b_j)]² + λR(b)`, gauge 固定一幅或零和;
  断连覆盖图按连通分量独立处理并标 provenance。
- 每种 rejection 有适用条件/样本下限/统计量/确定性 tie rule; 自动选择输出 reason code。
- Integration: `signal=Σw·x/Σw, support=max`, 权重单位清楚; frame identity 不丢失。
- UPM 不能制造未覆盖像素、黑洞或跨真实天体结构的过拟合。

## 4. Phase3 科学设计（SCI-003）

权威: SCI-P3-001 (DRAFT→ACTIVE 随实现推进)

- 定义: HiPS/HEALPix 球面采样 → 用户指定平面 WCS FITS。
- 方向: output pixel center → WCS sky → HEALPix sampling; 插值核/support/coverage/RA wrap/极区/无效值/单位传播均有合同。
- **单位**: 不允许默认 `Jy/beam`; 单位未知则保留 UNKNOWN 并拒绝需要物理单位的转换。
- 现状: 现有 `phase3_session` 为 **PROTOTYPE_NOT_PRODUCTION**（单线程、单位硬编码、
  provenance 假值）; 不得把现状标已实现。

## 5. 科学冲突登记

- 无未登记 SCI/ALG 冲突。若发现冲突按 00 规则标 `SCIENCE_CONFLICT` 并停止相关科学任务。

## 6. 引用文献

Fruchter & Hook 2002 (Drizzle); Górski et al. 2005 (HEALPix); Greisen & Calabretta 2002
(FITS WCS); Jacob et al. 2010 (Montage, 工程参考); Maples et al. 2018 (RCR, 仅当实现时)。
各合同引用"采用的具体章节/公式/语义与差异"，不以论文标题替代项目合同。
