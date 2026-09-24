# Drizzle / Spherical Resampling Science (SCI-DRIZZLE)

> 上游：ASTROCS_DESIGN.md §5.2（固定科学流程）、§6.3（投影算法）

> ID: SCI-DRZ-001  集合: SCI-DRZ-001,014,015,016  状态: FROZEN（冻结定义）  上游: SCI-SCOPE-001  下游 ALG: ALG-DRZ-001..  模块: healpix_drizzle

## 1 目的与非目标

- **目的**：将多帧抖动观测经球面 Drizzle 线性重建到公共 HEALPix 网格，**保持面亮度**（`S_p=Σ_j B_j a_jp/Σ_j a_jp`）、按条件不变量守恒通量（`Σ_p F_p=pixfrac²·Σ_j x_j`），并传播方差/协方差语义（不存完整矩阵），为 HiPS 信号/权重/支撑度提供重采样基础。
- **非目标**：不提供完整协方差矩阵产品（仅方差传播，协方差文档化）；不处理超越 Fruchter & Hook 线性模型的非线性探测器效应。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `x_j` | 源像素 j 信号 (ADU) | `drizzle_engine` |
| `v_j` | 源像素方差 (ADU²) | `variance_propagation` |
| `a_jp` | drop∩目标 p 的球面交叠面积 | `spherical_overlap` |
| `A_drop,j` | drop 内总面积 `= pixfrac²·A_pixel,j` | 同上 |
| `A_pixel,j` | 源像素 j 面积（球面立体角等价） | WCS/SIP 四角 |
| `w_jp` | `a_jp/A_pixel,j` 面亮度保持权重（`= pixfrac²·a_jp/A_drop,j`） | 重建式 |
| `F_p, D_p, S_p` | 累积通量/覆盖/归一信号 | 同上 |
| `sumVarNum` | `Σ v_j·w_jp²` 方差分子 | `TileLeafAccumulator` |
| `hp_res` | HEALPix 像素尺度 `√(π/3)/nside rad` | `spherical_overlap.cpp:40` |
| `pixfrac` | drop 收缩因子 (0,1]（`drizzle.pixfrac`，默认 0.8） | `drizzle_engine:half=0.5*pixfrac` |
| `C=π/2, C45=π/(2√2)` | 极区 Lipschitz 常数 | 极区 prune |

## 3 物理量和单位

- `S,F,x`: ADU/e⁻；`D,a,A_drop,A_pixel`: px²（球面立体角等价）；`v,variance`: ADU²；`ivar`: ADU⁻²；`w`: 无量纲；`hp_res, max_angle`: rad；`pixfrac`: 无量纲；`nside, order`: 无量纲（`nside=2^order`）。

## 4 输入有效域

- `w>0,h>0`, WCS 有效，`0<pixfrac<=1` 否则 `NO_DATA`（`drizzle:拒绝非法pixfrac`），NESTED 唯一 ordering，`nside=2^order` 为 2 的幂。
- `target_order` 由 MOC union 决定；`source` 像素四角经 `pixelToSky` 映射到球面多边形（`half=0.5·pixfrac` 收缩）。
- 极区 `|dec|>45°` 走保守 prune，`θ_q+radius>90°` 或跨边界走 `boundary_fallback`。

## 5 连续定义

```text
Fruchter & Hook 线性重建 (SCI-DRZ-001; 面亮度保持归一):
  源像素 j → 目标像素 p:
    B_j  = x_j / A_pixel,j          # 源像素积分通量 → 源像素面亮度
    w_jp = a_jp / A_pixel,j         # 面亮度保持权重 (= pixfrac²·a_jp/A_drop,j)
    F_p  = Σ_j x_j · w_jp = Σ_j B_j · a_jp
    D_p  = Σ_j a_jp
    S_p  = F_p / D_p = Σ_j B_j a_jp / Σ_j a_jp   # 最终信号为面亮度

  等价形式 (Fruchter & Hook 2002 PASP 114,144 式(5); 按输出像素面积归一):
    I_p = Σ_i d_i · a_ip · w_i · s² / Σ_i a_ip · w_i,   s² = A_out/A_in
    Astro Celestial Sphere Database（ACSD） 取 w_i=1、a_jp = drop∩目标 球面交叠面积、B_j = x_j/A_pixel,j
    ⇒ S_p = Σ_j B_j a_jp / Σ_j a_jp
    drop 面积 A_drop,j 在分子分母相消(均匀 drop 尺度下), 只决定 footprint;
    按 A_drop,j 归一的权重 a_jp/A_drop,j 满足 w_jp = pixfrac²·(a_jp/A_drop,j)，
    故两种归一下的信号相差 1/pixfrac²：**禁用**按 A_drop,j 归一
    （w_jp=a_jp/A_drop,j；负例判据 1/pf²−1，pf=0.8 → +56.25%）。
    正确口径 w_jp=a_jp/A_pixel,j，见
    drizzle_engine.cpp 的 processPixelSharedTiled「pixel_area」段与
    spherical_overlap.cpp 的 polygon_area_consistent；回归门 p1drz_disp009。

语义固定（SCI-003: flux vs surface-brightness 二选一）:
  【输入 x_j = 源像素积分通量】(单位统一为 ADU/e⁻), 由天体面亮度场 B(Ω) 对像素积分:
     x_j = ∫_{pixel_j} B(Ω) dΩ = B0 × A_pixel_j   (常数面亮度场 B0)
  【输出 S_p = 面亮度】(ADU/sr); 因此 S_p = F/D 把通量按覆盖面积归一为面亮度。
  【禁止】把"每像素常量 ADU"(常数通量 x_j=C 任意等值) 与"常量天空面亮度"
  (B0 恒定 → x_j=B0×A_pixel_j 随像素面积变化) 混为一谈 —— 前者经 S_p=F_p/D_p 得
  S_p=C/A_pixel（均匀源像素面积）≠ C, 仅后者才满足"无空间调制"。常量场 oracle 应按
  **面亮度** B0 构造。

方差传播 (SCI-DRZ-014):
    sumVarNum += v_j · w_jp²
    variance_p = sumVarNum / D_p²
    ivar_p     = 1 / variance_p
  缩放律: x' = α·x ⇒ var' = α²·var, ivar' = ivar/α² (SNR-002)
  sumVarNum 为 TileLeafAccumulator 中间分子，归一在 sink/writer finalize
  权重一致性: 信号与方差必须用同一 w_jp=a_jp/A_pixel,j（S_p=ΣB_j a_jp/Σa_jp 的
    组合系数 c_jp=a_jp/D_p, Var(S_p)=Σ v_j c_jp²/A_pixel,j²）。按 a_jp/A_drop,j
    归一的权重使 variance_p 额外乘 1/pixfrac⁴（信号乘 1/pixfrac²），
    故 SNR 不变但绝对面亮度标度错（DISP-DRZ-009）。

球面几何 (ALG-DRZ-GEOM):
  drop 多边形: pixelToSky((x±0.5·pixfrac, y±0.5·pixfrac)) → Vec3 单位向量
  目标边界: NESTED leaf 4 角（赤道菱形/极区退化），nside≥256 用 boundary4，
            低 nside 自适应细分；面积经 Sutherland–Hodgman 球面裁剪 +
            Van Oosterom & Strackee 扇形三角剖分
  通量守恒条件不变量：Σ_p w_jp·A_drop,p 与输入总通量在容差内一致（§7）。
缓冲三层 (spherical_overlap.cpp:40, HP_CIRCUMRADIUS_FACTOR=1.25·hp_res):
  1) overlap quick-reject:  lim = max_angle + 1.25·hp_res
  2) candidate 保守查询圆: query_radius = max_angle + 3.0·hp_res
  3) fast 枚举: buffer = 1.25·hp_res, 赤道 delta×1.15 畸变系数, 极冠回退
```

与 `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp:40,573,773-931` 及 `drizzle_engine.cpp:100,736-762` 一致。

## 6 假设

- 线性叠加且源像素噪声独立；几何 WCS 已解；球面裁剪外接圆半径 `1.25·hp_res` 覆盖赤道对角线 `1.532·res` 与极区最坏 `1.044·res` + 裕量。

## 7 独立不变量

- **通量守恒（条件不变量，FZ-COND-FLUX-CONSERV）**：`Σ_p F_p = Σ_j x_j·(A_drop,j/A_pixel,j)
  = pixfrac²·Σ_j x_j`（重建线性性）。`pixfrac=1` 时严格 `Σ_p F_p = Σ_j x_j`；
  `pixfrac<1` 时总输出通量按 `pixfrac²` 衰减，用于绝对通量/孔径换算必须乘
  `provenance.flux_conservation_factor = pixfrac²`，否则产生 `1/pixfrac²` 光度零点偏差。
- **常量场不变量（面亮度语义）**：常数**面亮度**场 `B(Ω)=B0` 时源像素通量
  `x_j=B0×A_pixel_j`（随像素面积变化，**非每像素常量 ADU**），输出 `S_p=B0`（面亮度），
  **对全部 `pixfrac∈(0,1]` 成立**（drop 面积在分子分母相消）；`variance_p=V·(Σ w_jp²/D_p²)`
  量纲一致，无空间调制偏差。特例：对每像素常量 ADU `x_j=C`，由 `S_p=F_p/D_p=(C/A_pixel)`
  **≠ C**（随源像素面积变化）——故"每像素常量 ADU ⇒ S=C"不成立；常量场 oracle 必须按
  面亮度 B0 构造（`FZ-GATE-CONST-SB`，门 `|S_p/B0−1|<1e-3`）。
- **零漏选不变量**：`candidate_oracle_test` 9003 例全枚举下 `false_negative=0`（12 face×边/角×RA跨0×极区×4 pixfrac×5 尺度×7 nside）。
- **缩放律**：`x→αx` 时 `var→α²var`，`ivar→ivar/α²` 精确成立（`variance_propagation_test`）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `pixfrac` 非法 `<=0/>1` | 拒绝 `NO_DATA` | `drizzle:拒绝非法pixfrac` |
| RING ordering | 拒绝（HISS 统一 NESTED） | `drizzle:拒绝RING` |
| 多通道图像 | 拒绝 | `drizzle:拒绝多通道` |
| WCS 无效/尺度非法 | 拒绝 `compute_auto_nside` 失败 | `drizzle_engine.cpp:631` |
| 源像素 NaN/Inf（值） | **样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数**（`rule_id = NAN-SAMPLE-MASK-COVERAGE-NAN`；唯一口径文字 = `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` §2a）：不合格样本从 `F_p`、分母、方差三项中一并剔除并**重新归一**——**禁止**让单个不合格样本使整个输出像素变为 NaN，**禁止**保留被剔除样本的权重在分母里；仅当 `D_p = 0`（零合格样本）时输出 `signal = NaN ∧ support ≤ 0`（NaN 是无效的**唯一**表示，**禁止**用 0/±Inf 冒充），且每个输出像素**必须**暴露被剔除样本计数 `n_rejected_nonfinite`（**禁止**静默剔除）。 | `spherical_overlap.cpp:192`（几何 NaN 显式拒绝） |
| 无覆盖/几何退化 | `NO_DATA`，不产伪信号 | `drizzleTiled` |
| 微小交集 `max_angle<1e-3 rad` | 切平面面积近似保持交叠面积 `a_jp` 一致（权重分母 A_pixel,j 与 drop_area 走**同一分支同一例程**：θ<1e-3 时同为切平面 2D 面积，见 `spherical::polygon_area_consistent`） | `spherical_overlap.cpp` `planar_polygon_area_n` / `polygon_area_consistent` |
| RA 跨0/极区/face边界 | `boundary_fallback` 保守 queryDisc，`false_negative=0` | `spherical_overlap` |
| 极区 `θ_q+radius>90°` | 保守不剪枝，遍历极冠树 | `gaia_client.c` 复用 |

## 9 精度策略

- FP64 累积 `F_p/sumVarNum/D_p`（`TileAccumulator sumFlux/sumArea/sumVarNum/nContrib` 线程局部后串行合并）；面积用 `double` 角点计算防 `float` 0.05% 偏差；`arc-chord <1e-6·hp_res ≈3e-6"`。

## 10 不可接受变化

- 改变 `pixfrac` 语义或 `HP_CIRCUMRADIUS_FACTOR=1.25` 保守半径而不重跑 `9003` 例零漏选；
- 将 `S_p` 定义为 `F_p`（漏 `D_p` 归一）；
- 将 `S_p` 用 `w_jp=a_jp/A_drop,j` 归一计算并宣称绝对面亮度（`pixfrac<1` 偏 `1/pixfrac²`，FZ-FORMULA-DRIZZLE-SB / DISP-DRZ-009）；
- 将方差传播写为 `var_p=Σ v_j·w_jp`（漏 `²` 与 `/D²`）；
- 在微小区用 `float` 面积致 0.05% 偏差。

## 11 验证 Oracle

- **零漏选门**：`candidate_oracle_test` 9003 例 `false_negative=0`（`TEST-DRZ-CAND-001`）。
- **方差缩放律**：`α` 缩放输入的 `variance_propagation_test` 逐像素 `variance_p` 按 `α²` 精确（`TEST-DRZ-VAR-001`）。
- **常量面亮度门（FZ-GATE-CONST-SB）**：按 `B0` 构造 `x_j=B0·A_pixel,j`，对全部
  `pixfrac∈(0,1]` 断言 `|S_p/B0−1|<1e-3` 全像素；负向注入（按每像素常量 ADU 构造、
  `S_p=F_p` 漏归一、`w=a/A_drop` 归一）必须判红。
- **Python 参考**：`healpy` 球面多边形面积对同 `drop` 的 `a_jp` 复算（`rtol 1e-9`）。
- **几何缓存等价**：`TargetGeomCache` 命中/未命中结果 `max_abs==0`（`DRIZZLE_TARGETED`）。

## 12 关联 ALG ID

- `ALG-DRZ-CAND` 候选零漏选与包围圆三层缓冲
- `ALG-DRZ-OVERLAP` Sutherland–Hodgman 球面裁剪 + Van Oosterom & Strackee 扇形三角剖分
- `ALG-DRZ-VAR` 方差传播 `sumVarNum/D²`
- `ALG-DRZ-GEOM-CACHE` LRU 8192 bounded target-ipix cache

## 13 追溯与测试

- 权威文件: `docs/science/DRIZZLE.md` (SCI-DRZ-001,014,015,016)
- 实现: `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp` (40,573,773-931), `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` (100,736-762, sink finalize), `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` (finalize_tile)
- 公开 API: `processPixelSharedTiled, compute_overlap_area_g_ctx, drizzleTiled, compute_auto_nside`
- 测试: `TEST-DRZ-CAND-001` 9003例零漏选、`TEST-DRZ-VAR-001` 缩放律、常数场、`TargetGeomCache` 等价（`candidate_oracle_test.cpp, variance_propagation_test.cpp`）

## 3a 坐标 frame

- 天球：ICRS/J2000（与 WCS/Gaia 同系）；目标网格为 **HEALPix NESTED**，`nside=2^order`（GLOSSARY `healpix_ordering`）。
- 面积量 `a_jp, A_drop, A_pixel, D` 以像素平面面积 px² 表达并在球面立体角等价下使用（§3）；源像素四角经 `pixelToSky`（SCI-WCS）映射为球面多边形。

## 9a 专属问题回答（SCI-004 指定问题逐项）

- **drizzle footprint**：源像素 j 的 footprint=四角球面多边形经 `half=0.5·pixfrac` 收缩后的 drop；`A_drop,j`=drop 内总面积，`a_jp`=源像素 j 与目标 p 的球面重叠面积（§2/§5）。
- **pixfrac**：drop 收缩因子，有效域 `(0,1]`，非法值显式 `NO_DATA`（§4），不静默夹逼。
- **surface brightness/flux**：输入 `x_j`=源像素积分通量，输出 `S_p=F_p/D_p=Σ_j B_j a_jp/Σ_j a_jp`=面亮度（`B_j=x_j/A_pixel,j`，`w_jp=a_jp/A_pixel,j`）；禁止把"每像素常量通量"与"常量天空面亮度"混为一谈（§5 语义固定，GLOSSARY `surface_brightness`）。
- **support**：`D_p=Σ_j a_jp` 为目标像素覆盖面积；HiPS 产品 `support∈[0,1]`（覆盖度）语义冻结于 DATA_SEMANTICS §4，由 `D_p` 与目标像素面积归一导出。
- **variance/covariance**：`variance_p=sumVarNum/D_p²`（§5，独立像素方差传播）；**不存完整协方差矩阵**——相邻像素相关性已文档化（UNCERTAINTY_AND_COVARIANCE.md），协方差产品为非目标（§1）。
- **边界**：极区 `|dec|>45°` 保守 prune、`θ_q+radius>90°`/跨边界 `boundary_fallback`（§4a）；非法 `pixfrac`/`nside` 显式拒绝。

## 14 Primary literature（引用定位声明）

1. Fruchter, A. S. & Hook, R. N. 2002, PASP, 114, 144, "Drizzle: A Method for the Linear Reconstruction of Undersampled Images"（DOI 10.1086/338393，bibcode 2002PASP..114..144F；§5 面亮度归一依据其 §2 式(5)，见 §14a；球面 HEALPix 实施为 Project-defined 迁移）。
2. pixfrac 语义（drop 与像素之比、pixfrac=1 等价 overlap、缩小时权重场变化）：同上文献；实践语义另见 [DrizzlePac Handbook](https://www.stsci.edu/files/live/sites/www/files/home/scientific-community/software/drizzlepac/_documents/drizzlepac-handbook-v1.pdf)（STScI，节级定位）。
3. HEALPix 网格：Górski et al. 2005, ApJ 622, 759（bibcode 2005ApJ...622..759G，文章级；NESTED/`nside=2^order` 语义见 DATA_SEMANTICS §2，逐式核验留 SCI-P3/ALG-007）。

## 14a 参考文献与参考代码库（含许可证）

> 本节只补出处与参考实现，不改动 §5/§7 的公式、常数与容差。

- **Drizzle 线性重建/drop/pixfrac**：Fruchter, A. S. & Hook, R. N. 2002, PASP 114, 144（DOI 10.1086/338393；arXiv:astro-ph/9808087v2 §2 式(2)-(5)）。式(5) 为**一致加权均值**
  `I_p = Σ_i d_i a_ip w_i s² / Σ_i a_ip w_i`（`a_ip`=drop 与目标像素的分数交叠、`s²=A_out/A_in`），
  `A_drop` 同时出现在分子与分母、在均匀 drop 尺度下相消；对常数面亮度场输出面亮度 `=B0`，
  与 pixfrac 无关。**ACSD 取 w_i=1、按输出像素面积归一 ⇒ §5 的 `S_p=Σ_j B_j a_jp/Σ_j a_jp`**
  （`B_j=x_j/A_pixel,j`）。**差异**：原始 Drizzle 在切平面上实施；ACSD 在球面 HEALPix 上实施（§5），属 Project-defined 迁移。
  **独立实现对照**：drizzlepac `src/cdrizzlebox.c` `update_data()`（`output=(output·vc+dow·d)/(vc+dow)`）与 `do_kernel_square()` 的
  `dover/=jaco`（`jaco=A_drop`）后 `dow=dover·w`——drop 面积同入分子分母，是一致加权均值；
  DrizzlePac Handbook §2.3.2（p.17）“the weights of the individual output pixels … are independent of the choice of p [pixfrac]”；
  SWarp `src/resample.c` 以 `A_out/A_in` 面积比保面亮度（无 drop/pixfrac 概念）。
  **禁用** **按 `A_drop,j` 归一的混合式**（分子 `x_j·a_jp/A_drop,j`、分母 `Σ a_jp`）：它给出 `S_p=B0/pixfrac²`，仅 pixfrac=1 正确（`DISP-DRZ-009` 负例判据）。正确口径 = `w_jp=a_jp/A_pixel,j`（`drizzle_engine.cpp` `processPixelSharedTiled` 的 `pixel_area` 段），回归门 `p1drz_disp009`（pixfrac∈(0,1] 常量面亮度门 + "分母取 A_drop 必判红"的负例控制）。
- **Drizzle 实践与相关噪声**：DrizzlePac Handbook（STScI）；drizzlepac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- **HEALPix 几何/order**：Górski, K. M. et al. 2005, ApJ 622, 759（DOI 10.1086/427976）；独立实现 astropy-healpix（BSD-3-Clause）、healpy（GPL-2.0，只对照不复制）。
- **球面多边形面积**：Van Oosterom, A. & Strackee, J. 1983, IEEE Trans. Biomed. Eng. 30, 125（DOI 10.1109/TBME.1983.325207）——本模块按其平面三角形立体角式实现
  Sutherland–Hodgman 球面裁剪 + 扇形三角剖分（spherical_overlap.cpp:152-186,218）。
- **多边形裁剪**：Sutherland, I. E. & Hodgman, G. W. 1974, “Reentrant Polygon Clipping”, Comm. ACM 17, 32（DOI 10.1145/360767.360802）——平面算法原型（原文只处理平面多边形与平面窗口）；本模块的球面逐边裁剪是它的推广。
- **HiPS/MOC 层级**：IVOA HiPS 1.0（https://www.ivoa.net/documents/HiPS/）；IVOA MOC 1.0（https://www.ivoa.net/documents/MOC/）；Fernique et al. 2015, A&A 578, A114。
- **HP_CIRCUMRADIUS_FACTOR=1.25、三层缓冲**：Project-defined（§5/§8），以 9003 例零漏选门承载。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- WCSLIB（LGPL-3.0）/ CFITSIO（宽松许可，NASA/HEASARC）：WCS 与 FITS 独立读取器。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

## 15 Acceptance

- §11 Oracle 全过（常量场/解析场/方差传播/边界，以 §11 列门为准）；
- §7 不变量门全过；
- `eng/tools/science_contract_lint.py` PASS（15 节+合同 ID+锚点）；
- 解析不变量→SYN-004 转换：常数/点源/梯度/旋转/亚像素 shift/pixfrac 扫描/tile boundary 用例，flux 或 brightness/support/variance/coverage 不变量全过（§15 SYN-004 数据与不变量表）。
