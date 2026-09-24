# PSF Science (SCI-PSF)

> 上游：ASTROCS_DESIGN.md §2.1（测光星等坐标系）、§4.2（Phase1 节点流程）

> ID: SCI-PSF-001  状态: FROZEN  上游: SCI-SCOPE-001  下游 ALG: ALG-STARPSF-001..  模块: dynamic_psf

## 1 目的与非目标

- **目的**：描述点源响应（椭圆 Moffat4），估计 PSF 形状/位置及拟合质量代理 `q_psf`，用于 Astrometry/Photometry 的星点建模与剔星/QA。
- **非目标**：不处理超出椭圆 Moffat4 的高阶色差/空间变异（仅一阶椭率 `e,θ`）；不直接输出图像噪声 SNR（`q_psf`≠SNR，见 NOISE_MODEL）；不进入 Phase2 逐像素科学叠加权重（`q_psf` 只作诊断）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `I(r)` | 点源强度模型 `B + A/(1+Q)^4` | `dpsf_psf.cpp:13-18` |
| `Q` | 二次型 `p1·dx²+2p2·dxdy+p3·dy²` | `dpsf_psf.cpp:66-95` |
| `dx,dy` | 相对坐标 `x−(cx+x0), y−(cy+y0)` | 同上 |
| `B,A,x0,y0,sx,sy,θ` | Moffat4 7 参数 | `lm_solve` 7-vector |
| `σ,sx,sy` | 各向同性 σ / 各向异性轴尺度 (px) | FWHM 推导 |
| `e` | 离心率 `√(1−(s_min/s_max)²)` | 椭率 |
| `fwhm_x/y` | 轴向 FWHM `1.230310·s` | `MOFFAT4_FWHM_FACTOR` |
| `flux` | 解析通量 `2πA·sxsy/3` (β=4) | `dpsf_psf.cpp:368` |
| `residual_scale` | 10–90% trimmed mean \|residual\| | PSF 块第8列 |
| `robust_residual_sigma` | `residual_scale/0.7316727929211932` | Gaussian 假设 |
| `q_psf` | `A/residual_scale` 拟合质量代理 | 剔星/QA |

## 3 物理量和单位

- `I,B`: **ADU/pixel**（探测器平面上的面亮度，逐像素求值）；`A`: **ADU/pixel**
  （模型在中心处的面亮度振幅，与 `B` 同域，故 `I` 无量纲比 `A/residual_scale` 成立）；
  `residual_scale`: **ADU/pixel**（逐像素 |残差| 的 10–90% 截尾均值，与 `I` 同域）；
  `r,dx,dy,σ,sx,sy,fwhm`: px；`Q,e,q_psf`: 无量纲；
  `flux`: **ADU**（对探测器平面二维积分：`∫∫ I dA`，面积元单位 pixel²，故
  ADU/pixel × pixel² = ADU；各向同性解析值 `2πAσ²/3`）。
- **`θ` 的单位与值域**：单位 **rad**；**规范值域 = `[0, π)`**（或等价地
  `(−π/2, π/2]`）。`p1/p2/p3` 只含 `cos²θ, sin²θ, sin2θ`，周期为 π，故值域外取值
  **不改变模型值**，但**使 θ 列失去位置角语义**。实测（真实产物
  `run/RELEASE-05/vis/out/m42_p1_t2/p1_sources.json` 的 `psf_params`，n=12451）：
  θ ∈ [−100625, +118664] rad，`|θ| > π` 占 63.1%，`|θ| > 100 rad` 占 16.6%。
  ⇒ **消费方必须先把 θ 归约到 `[0, π)`** 再作位置角解释；
  直接 `θ·180/π` 得到的角度无物理意义。证据
  `run/SCI-FIX-STARPSF-01/results/e3_real_summary.txt` §C。

## 3a 坐标 frame

PSF 拟合在**像素域小窗口**内进行：相对坐标 `dx,dy=x−(cx+x0),y−(cy+y0)`；无 WCS/天球参与；窗口内像素沿用内部 0-based 约定（GLOSSARY `pixel_coordinate`）；结果回写由调用方（Astrometry/Photometry）关联天球 frame。

## 4 输入有效域

- 图像 `uint16_t`/`float32`，维度 `w>0,h>0`，拟合窗口 `fitRadius` 使 `rect` 在图像内且面积 `rw*rh > 0`，否则返回 `DPSF_FIT_INVALID_PARAMS`（= 2，`dpsf_psf.cpp:499-504`；非有限像素在采样阶段被跳过，全部非有限时同码返回，`:284-311`）。
- 初始幅度 `A0 = max_val − bkg0 > 0`，否则 `LOG_WARN Amplitude<=0` 并拒。
- `sx>0, sy>0`，否则 `Invalid fit params` 拒；`B` 受 `bkg0` 约束（`Background constraint violated`）。

## 5 连续定义

```text
I(r) = B + A / (1 + Q)^4
Q = p1·dx² + 2·p2·dx·dy + p3·dy²
p1 = cos²θ/(2sx²)+sin²θ/(2sy²)
p2 = sin2θ/(4sx²)−sin2θ/(4sy²)
p3 = sin²θ/(2sx²)+cos²θ/(2sy²)
dx = x−(cx+x0), dy = y−(cy+y0)

各向同性 sx=sy=σ ⇒ Q=0.5·r²/σ²
α=√2·σ, FWHM=2α√(2^{1/4}−1)=2√2·σ·√(2^{1/4}−1)=1.230307652590102·σ
flux = 2πA·sxsy/3   (整平面延伸假设；对任意 sx,sy,θ 成立，见下)
```

与 `lib/algorithms/psf/src/dpsf_psf.cpp:24-25,84-118,222-254,425-432` 一致。

**FWHM 因子的精确值与适用域**

- 精确值 `2√2·√(2^{1/4}−1) = 1.230307652590102`（FP64 闭式，可复算；
  独立数值反解 `(1+r²/α²)^4=2` 得 1.2303072，一致到 4e-7）。
- 实现常量 `MOFFAT4_FWHM_FACTOR = 1.230310`（`dpsf_psf.cpp:25`）与精确值
  相对差 **+1.91e-6**（FWHM 相对误差 1.9e-6，远小于 §9 的 1% 容差）。
- **适用域**：仅对 **β = 4 且各向同性**成立；β≠4 时
  `FWHM = 2α√(2^{1/β}−1)`，各向异性时按轴分别 `FWHM_x = 1.230310·sx`、
  `FWHM_y = 1.230310·sy`（`dpsf_psf.cpp:393-394`）。
- 证据：`run/SCI-FIX-STARPSF-01/results/e1_constants.txt` §A。

**`flux` 的适用域与窗口截断修正**

- `flux = 2πA·sx·sy/3` 对**任意 `sx, sy, θ`** 成立（不只是圆对称）：
  令 `M = [[p1,p2],[p2,p3]]`，`Q = dᵀMd`，则 `det M = 1/(4 sx² sy²)`（与 θ 无关），
  换元 `u = Ld`（`M = LᵀL`）后
  `∫∫ A/(1+Q)^4 dA = (πA/3)/√(det M) = 2πA·sx·sy/3`。
  推导与复算见 `run/SCI-FIX-STARPSF-01/results/e1_constants.txt`。
- **截断适用域**：上式是**整平面**（r→∞）积分；发布值对应拟合窗口半径 `r_win`
  时，窗口外通量占比有闭式
  `f_out(r_win) = (1 + r_win²/α²)^{−3}`（β=4；各向同性 `α=√2σ`）。
  典型拟合窗 `r_win = 3.7172·σ`（`sdet_api.cpp:2071-2074` 的 `s_factor`）
  ⇒ `r_win/α = 2.629`、`f_out = 2.02e-3`，即发布 flux 相对窗内积分通量
  **偏高 0.20%**。**当 `r_win/α < 1`（`r_win < 1.41σ`）时 `f_out > 3.1%`**，
  该域下 flux **不得**当作全通量使用。

## 6 假设

- 视场内 PSF 缓变（块状拟合共享假设）；星点不饱和、采样充足（FWHM/px 合理范围）；残差在 `robust_residual_sigma` 换算时近似 Gaussian。

## 7 独立不变量

- **FWHM 缩放不变量**：各向同性 Moffat4 的 `FWHM/σ` 比值恒为 `1.230310`，与 `A,B` 无关。
- **积分一致性**：各向同性 `σ` 的解析 `flux` 在数值积分（足域）内与 `A,σ` 的 `2πAσ²/3` 比例一致（误差仅离散域/截断）。
- **旋转简并不变量**：`θ` 四候选 `{θ,π/2−θ,π/2+θ,π−θ}` 中以 trimmed-mad 最小者消歧后，`fwhm_x/y` 与方向无关（`dpsf_psf.cpp:352-363`）。
- **平移不变量**：整帧平移 `Δ` 后拟合中心 `cx+x0` 同步平移 `Δ`（子像素插值误差内）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 空 `rect`/越界 | 返回错误 `DPSF_ERR_PARAM` | `dpsf_fit:446 empty rect` |
| `A<=0` / `max<=bkg` | `WARN Amplitude<=0` 拒 | `dpsf_psf.cpp:310` |
| `sx<=0`/`sy<=0`/非有限 | `WARN Invalid fit params` 拒 | `dpsf_psf.cpp:333` |
| FWHM 超窗 | `WARN FWHM exceeds rect` | `dpsf_psf.cpp:343` |
| LM 不收敛 | 返回非零 `status`，成本 `cost` 上报 | `dpsf_psf.cpp:186` |
| 无星/密集混淆 | 上游采样为空 → 显式 NO_DATA | 调用方 |

## 9 精度策略

- FP64 拟合 LM 求解器 `lm_solve`（`dpsf_psf.cpp:120-208`），仅 7 参数 Moffat4 路径；`kTrimMeanToSigma=0.7316727929211932` 解析常数（`noise_model.cpp:95`）用于 `robust_residual_sigma`。
- **`kTrimMeanToSigma` 的闭式推导（可独立复算）**：设残差 `r ~ N(0, σ²)`，
  `|r|` 服从半正态。10% / 90% 分位点
  `a = Φ⁻¹(0.55) = 0.125661346855·σ`、`b = Φ⁻¹(0.95) = 1.644853626951·σ`；
  截尾均值 `E[mean(|r|), a<|r|<b] = 2(φ(a)−φ(b))/0.8 = 0.7316730952806134·σ`。
  实现常量 `0.7316727929211932` 与该闭式相对差 **4.13e-7**（σ 换算偏差
  +0.00004%，可忽略）。复算：`run/SCI-FIX-STARPSF-01/results/e1_constants.txt` §B。
- **`kTrimMeanToSigma` 的适用域（必须随换算同写）**：该常数是**高斯专属**标准化因子。
  实测（4e6 样本/分布）：残差分布为 Gaussian / Uniform / Laplace / Student-t(5) 时
  `σ̂ = residual_scale/0.7316727929211932` 相对真值之比 = 1.0004 / 1.1837 / 0.8024 /
  0.8648 ⇒ **非高斯残差下偏差可达 ±18%**。残差含未建模源/宇宙线/邻星时
  `robust_residual_sigma` **不得**当作噪声 σ 的绝对标度。证据同上 §C。
- **实现截尾边界的有限-m 效应**：`compute_trimmed_mad` 取
  `lo = int(0.1·m)`、`hi = int(0.9·m)`（`dpsf_psf.cpp:248-249`），两端裁剪
  **不对称**（`m` 非 10 的整数倍时上端多裁）。实测 `E[该统计量]/σ`：
  `m`=121 → 0.72448（**−0.98%**）、`m`=169 → 0.72822（−0.47%）、
  `m`=441 → 0.72959（−0.28%）、`m`=1024 → 0.73090（−0.11%）、
  `m`=9 → 0.66800（−8.70%）。**适用域 = `m ≥ 441`（|偏差| < 0.3%）**；
  `m < 441` 时 `robust_residual_sigma` 相对常数隐含的标度**偏低**，
  偏差量级见上表。证据 `run/SCI-FIX-STARPSF-01/results/e1_constants.txt` §D。

## 9a 专属问题回答（SCI-002 指定问题逐项）

- **WCS frame/pixel convention**：不涉及；像素域窗口拟合（§3a），中心 `(cx+x0, cy+y0)` 供 Astrometry 质心域。
- **PSF 参数**：七参数含义/单位见 §2/§3；`FWHM=1.230310·σ`（各向同性不变量 §7）；椭率一阶 `e,θ`。
- **aperture/flux/background**：解析通量 `flux=2πA·sxsy/3`，单位 **ADU**（`I,B` 为 ADU/pixel，对探测器平面二维积分后为 ADU；β=4 整平面延伸假设，Project-defined 推导；§3）；背景 `B` 模型内联合拟合，无独立孔径 annulus。
- **photometric scale 与不确定度**：`q_psf=A/residual_scale` 为拟合质量代理，**不是 SNR/光度不确定度**（§1 非目标）；`robust_residual_sigma=residual_scale/0.7316727929211932` 为高斯假设换算（10–90% trimmed mean）。

## 10 不可接受变化

- 改变 Moffat β≠4 或 `FWHM_FACTOR` 而无 SCI 变更；
- 将 `q_psf` 当 SNR 进入 Phase2 权重；
- 在**本模块内**（`lib/algorithms/psf`，§13 实现面）引入未文档的高斯备选拟合路径作为主路径（检测侧 `lib/algorithms/star_detection` 的椭圆高斯母函数不属本条范围：SCI-P1-STAR-001 §3、DISP-STAR-007）；

## 11 验证 Oracle

- **解析解**：各向同性 `FWHM/σ` 与 `flux` 公式的解析一致性（`max_abs==0`）。
- **Python 参考**：`scipy` / NumPy 对同参数 Moffat4 图像块做 `curve_fit` 复算，位置 `≤0.05px`、FWHM `≤1%`（合成无噪声谱）。
- **不变量门**：FWHM 缩放、旋转简并、平移三门。
- **失败注入**：空窗/非正幅度/非有限尺度返回显式错误码。

## 12 关联 ALG ID

- `ALG-STARPSF-001` Moffat4 拟合（LM 7 参数）
- `ALG-STARPSF-002` 几何常数（FWHM/通量）与 θ 消歧

## 13 追溯与测试

- 权威文件: `docs/science/PSF.md` (SCI-PSF-001)
- 实现: `lib/algorithms/psf/src/dpsf_psf.cpp` (`dpsf_fit/batch, lm_solve, MOFFAT4_FWHM_FACTOR, compute_trimmed_mad`), `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:95`
- 公开 API: `lib/algorithms/psf/include/dynamic_psf.h` (`dpsf_fit, dpsf_fit_batch`)
- 测试: `TST-PSF-001` 解析一致性、`TST-PSF-INV-*` 三门、`TST-PSF-FAIL-*` 参数校验（新增/映射见 `docs/TRACEABILITY.csv`）

## 14 Primary literature（引用定位声明）

1. Moffat, A. F. J. 1969, A&A 3, 455（"A Theoretical Investigation of Focal Stellar Images"）：Moffat 轮廓 I(r)∝(1+r²/α²)^{−β} 来源——文章级定位（bibcode 1969A&A.....3..455M，未逐页核验）。
2. β=4 解析通量 `flux=2πA·sxsy/3` 与 `FWHM/σ=1.230310`：**Project-defined derivation**（§5 对 (1+Q)^{−4} 解析积分，各向同性极限 πα²/3·A=2πAσ²/3 自洽），不引用外部公式号。
3. trimmed-mean→σ 换算系数 `0.7316727929211932`：高斯假设下 10–90% trimmed mean 的标准化常数。**闭式**：`2(φ(Φ⁻¹(0.55))−φ(Φ⁻¹(0.95)))/0.8 = 0.7316730952806134`（本仓复算 `run/SCI-FIX-STARPSF-01/results/e1_constants.txt` §B，与实现常量相对差 4.13e-7）。适用域见 §9。

## 14a 参考文献与参考代码库（含许可证）

> 本节只补出处与参考实现，不改动 §5 公式与 §7/§11 容差。

- **Moffat 轮廓**：Moffat, A. F. J. 1969, A&A 3, 455（bibcode 1969A&A.....3..455M；I(r)∝(1+r²/α²)^(−β)）。**核验状态**：文章级，未逐式核验公式号。
- **β=4 解析通量 flux=2πA·sxsy/3 与 FWHM/σ=1.230310**：**Project-defined 解析积分**（对 (1+Q)^(−4) 的整平面积分）；建议用独立符号/数值积分（SciPy quad 或 sympy，BSD-3-Clause）复算，不作文献引用。
- **LM 阻尼最小二乘**：Levenberg 1944, Quart. Appl. Math. 2, 164；Marquardt 1963, SIAM J. Appl. Math. 11, 431；Moré 1978, Lecture Notes in Math. 630, 105。实现对照 GSL gsl_multifit_nlinear（GPL-3.0，https://www.gnu.org/software/gsl/）。
- **10–90% trimmed mean → σ 常数 0.7316727929211932**：**Project-defined 高斯分位积分**（可用 scipy.stats.truncnorm 复算）；**注意**该常数是 trimmed mean 的标准化因子，与 MAD 常数 1.482602218505602 **不可互换**（NOISE_MODEL §9）。
- **空间变异 PSF / PSF 采样基**：Bertin, E. 2011, ASP Conf. Ser. 442, 435（PSFEx；<http://aspbooks.org/custom/publications/paper/442-0435.html>）；photutils（BSD-3-Clause）MoffatPSF/GaussianPSF。**差异**：Astro Celestial Sphere Database（ACSD） 现状为块状共享 7 参数 Moffat4，不做空间变异多项式基（§1 非目标）。
- **拥挤场 PSF 拟合测光**：Stetson, P. B. 1987, PASP 99, 191（DAOPHOT；DOI 10.1086/131977）。
- **q_psf=A/residual_scale**：**Project-defined 质量代理**，非 SNR、非 Fisher information（UNIFIED_SCIENCE_MODEL §3/§11；SCI-PSF §1 非目标）。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

## 15 Acceptance

- §11 Oracle 全过：FWHM 缩放不变量、解析/数值积分一致性、参数拒门；
- §7/§8 全过；
- `eng/tools/science_contract_lint.py` PASS；
- 解析不变量→SYN-002 转换：解析 PSF 星场（已知 A/σ/θ）、q_psf 边界、饱和标志用例登记 SYN-002。

> 本域门与容差的量测域/统计量/SNR 定义/阈值来源见 `docs/algorithms/GATES_AND_TOLERANCES.md`（F-2 冻结门表；门不得引用表外阈值）。
